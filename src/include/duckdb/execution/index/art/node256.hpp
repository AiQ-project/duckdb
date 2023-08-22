//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/index/art/node256.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/execution/index/fixed_size_allocator.hpp"
#include "duckdb/execution/index/art/art.hpp"
#include "duckdb/execution/index/art/node.hpp"

namespace duckdb {

//! Node256 holds up to 256 Node children which can be directly indexed by the key byte
class Node256 {
public:
	//! Delete copy constructors, as any Node256 can never own its memory
	Node256(const Node256 &) = delete;
	Node256 &operator=(const Node256 &) = delete;

	//! Number of non-null children
	uint16_t count;
	//! Node pointers to the child nodes
	Node children[Node::NODE_256_CAPACITY];

public:
	//! Get a new Node256, might cause a new buffer allocation, and initialize it
	static Node256 &New(ART &art, Node &node);
	//! Free the node (and its subtree)
	static void Free(ART &art, Node &node);

	//! Initializes all the fields of the node while growing a Node48 to a Node256
	static Node256 &GrowNode48(ART &art, Node &node256, Node &node48);

	//! Initializes a merge by incrementing the buffer IDs of the node
	void InitializeMerge(ART &art, const ARTFlags &flags);

	//! Insert a child node at byte
	static void InsertChild(ART &art, Node &node, const uint8_t byte, const Node child);
	//! Delete the child node at byte
	static void DeleteChild(ART &art, Node &node, const uint8_t byte);

	//! Replace the child node at byte
	inline void ReplaceChild(const uint8_t byte, const Node child) {
		children[byte] = child;
	}

	//! Get the child for the respective byte in the node
	template <class NODE>
	inline optional_ptr<NODE> GetChild(const uint8_t byte) {
		if (children[byte].HasMetadata()) {
			return &children[byte];
		}
		return nullptr;
	}
	//! Get the first child that is greater or equal to the specific byte
	template <class NODE>
	inline optional_ptr<NODE> GetNextChild(uint8_t &byte) {
		for (idx_t i = byte; i < Node::NODE_256_CAPACITY; i++) {
			if (children[i].HasMetadata()) {
				byte = i;
				return &children[i];
			}
		}
		return nullptr;
	}

	//! Vacuum the children of the node
	void Vacuum(ART &art, const ARTFlags &flags);
};
} // namespace duckdb
