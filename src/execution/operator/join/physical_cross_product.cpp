#include "duckdb/execution/operator/join/physical_cross_product.hpp"
#include "duckdb/common/types/column_data_collection.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/execution/operator/join/physical_join.hpp"

namespace duckdb {

PhysicalCrossProduct::PhysicalCrossProduct(vector<LogicalType> types, unique_ptr<PhysicalOperator> left,
                                           unique_ptr<PhysicalOperator> right, idx_t estimated_cardinality)
    : PhysicalOperator(PhysicalOperatorType::CROSS_PRODUCT, move(types), estimated_cardinality) {
	children.push_back(move(left));
	children.push_back(move(right));
}

//===--------------------------------------------------------------------===//
// Sink
//===--------------------------------------------------------------------===//
class CrossProductGlobalState : public GlobalSinkState {
public:
	explicit CrossProductGlobalState(ClientContext &context, const PhysicalCrossProduct &op)
	    : rhs_materialized(context, op.children[1]->GetTypes()) {
		rhs_materialized.InitializeAppend(append_state);
	}

	ColumnDataCollection rhs_materialized;
	ColumnDataAppendState append_state;
	mutex rhs_lock;
};

unique_ptr<GlobalSinkState> PhysicalCrossProduct::GetGlobalSinkState(ClientContext &context) const {
	return make_unique<CrossProductGlobalState>(context, *this);
}

SinkResultType PhysicalCrossProduct::Sink(ExecutionContext &context, GlobalSinkState &state, LocalSinkState &lstate_p,
                                          DataChunk &input) const {
	auto &sink = (CrossProductGlobalState &)state;
	lock_guard<mutex> client_guard(sink.rhs_lock);
	sink.rhs_materialized.Append(sink.append_state, input);
	return SinkResultType::NEED_MORE_INPUT;
}

//===--------------------------------------------------------------------===//
// Operator
//===--------------------------------------------------------------------===//
class CrossProductOperatorState : public OperatorState {
public:
	CrossProductOperatorState(ExecutionContext &context, const PhysicalCrossProduct &op)
	    : position_in_chunk(0), initialized(false), finished(false) {
		scan_chunk.Initialize(context.client, op.children[1]->GetTypes());
	}

	void Reset(ColumnDataCollection &collection, DataChunk &input, DataChunk &output) {
		initialized = true;
		finished = false;
		scan_input_chunk = false;
		collection.InitializeScan(scan_state);
		position_in_chunk = 0;
		scan_chunk.Reset();
	}

	bool NextValue(ColumnDataCollection &collection, DataChunk &input, DataChunk &output) {
		if (!initialized) {
			// not initialized yet: initialize the scan
			Reset(collection, input, output);
		}
		position_in_chunk++;
		idx_t chunk_size = scan_input_chunk ? input.size() : scan_chunk.size();
		if (position_in_chunk < chunk_size) {
			return true;
		}
		// fetch the next chunk
		collection.Scan(scan_state, scan_chunk);
		position_in_chunk = 0;
		if (scan_chunk.size() == 0) {
			return false;
		}
		// the way the cross product works is that we keep one chunk constantly referenced
		// while iterating over the other chunk one value at a time
		// the second one is the chunk we are "scanning"

		// for the engine, it is better if we emit larger chunks
		// hence the chunk that we keep constantly referenced should be the larger of the two
		scan_input_chunk = input.size() < scan_chunk.size();
		return true;
	}

	ColumnDataScanState scan_state;
	DataChunk scan_chunk;
	idx_t position_in_chunk;
	bool initialized;
	bool finished;
	bool scan_input_chunk;
};

unique_ptr<OperatorState> PhysicalCrossProduct::GetOperatorState(ExecutionContext &context) const {
	return make_unique<CrossProductOperatorState>(context, *this);
}

OperatorResultType PhysicalCrossProduct::Execute(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
                                                 GlobalOperatorState &gstate, OperatorState &state_p) const {
	auto &state = (CrossProductOperatorState &)state_p;
	auto &sink = (CrossProductGlobalState &)*sink_state;

	if (sink.rhs_materialized.Count() == 0) {
		// no RHS: empty result
		return OperatorResultType::FINISHED;
	}
	if (!state.NextValue(sink.rhs_materialized, input, chunk)) {
		// ran out of entries on the RHS
		// reset the RHS and move to the next chunk on the LHS
		state.initialized = false;
		return OperatorResultType::NEED_MORE_INPUT;
	}

	// set up the constant chunk
	auto &constant_chunk = state.scan_input_chunk ? state.scan_chunk : input;
	auto col_count = constant_chunk.ColumnCount();
	auto col_offset = state.scan_input_chunk ? input.ColumnCount() : 0;
	chunk.SetCardinality(constant_chunk.size());
	for (idx_t i = 0; i < col_count; i++) {
		chunk.data[col_offset + i].Reference(constant_chunk.data[i]);
	}

	// for the chunk that we are scanning, scan a single value from that chunk
	auto &scan_chunk = state.scan_input_chunk ? input : state.scan_chunk;
	col_count = scan_chunk.ColumnCount();
	col_offset = state.scan_input_chunk ? 0 : input.ColumnCount();
	for (idx_t i = 0; i < col_count; i++) {
		ConstantVector::Reference(chunk.data[col_offset + i], scan_chunk.data[i], state.position_in_chunk,
		                          scan_chunk.size());
	}

	return OperatorResultType::HAVE_MORE_OUTPUT;
}

//===--------------------------------------------------------------------===//
// Pipeline Construction
//===--------------------------------------------------------------------===//
void PhysicalCrossProduct::BuildPipelines(Executor &executor, Pipeline &current, PipelineBuildState &state) {
	PhysicalJoin::BuildJoinPipelines(executor, current, state, *this);
}

vector<const PhysicalOperator *> PhysicalCrossProduct::GetSources() const {
	return children[0]->GetSources();
}

} // namespace duckdb
