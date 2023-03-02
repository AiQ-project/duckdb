//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/statistics/list_stats.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/hugeint.hpp"

namespace duckdb {
class BaseStatistics;
class FieldWriter;
class FieldReader;
struct SelectionVector;
class Vector;

struct ListStats {
	DUCKDB_API static void Construct(BaseStatistics &stats);
	DUCKDB_API static unique_ptr<BaseStatistics> CreateUnknown(LogicalType type);
	DUCKDB_API static unique_ptr<BaseStatistics> CreateEmpty(LogicalType type);

	//! Whether or not the statistics are for a list
	DUCKDB_API static bool IsList(const BaseStatistics &stats);

	DUCKDB_API static const BaseStatistics &GetChildStats(const BaseStatistics &stats);
	DUCKDB_API static BaseStatistics &GetChildStats(BaseStatistics &stats);
	DUCKDB_API static void SetChildStats(BaseStatistics &stats, unique_ptr<BaseStatistics> new_stats);

	DUCKDB_API static void Serialize(const BaseStatistics &stats, FieldWriter &writer);
	DUCKDB_API static BaseStatistics Deserialize(FieldReader &reader, LogicalType type);

	DUCKDB_API static string ToString(const BaseStatistics &stats);

	DUCKDB_API static void Merge(BaseStatistics &stats, const BaseStatistics &other);
	DUCKDB_API static void Copy(BaseStatistics &stats, const BaseStatistics &other);
	DUCKDB_API static void Verify(const BaseStatistics &stats, Vector &vector, const SelectionVector &sel, idx_t count);
};

} // namespace duckdb
