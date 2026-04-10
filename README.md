# Dynamic BitTree (B+ Tree Implementation)

A C++ implementation of a dynamic bit vector utilizing a specialized B+ Tree structure. Every node is dynamically adjusted to maximize leaf storage and is strictly constrained to 512 bits for cache-line efficiency.

## Technical Architecture

The architecture is designed for high-performance bitstream modifications by minimizing memory overhead and cache misses:

- Leaves: These store the raw values of the bitvector.
- Internal Nodes: These contain pointers to lower-level nodes or leaves, alongside the variables necessary for indexing.
- Cache Alignment: Every node utilizes exactly 512 bits. Pointers and metadata are compressed using bit-shifting to fit within this fixed size, maximizing the branching factor.
- Efficiency: This structure allows for frequent bitstream changes without the need for reallocating large blocks of memory or performing linear shifts.

## Core API Reference

The following operations are supported:

- Rank: Returns the number of 1s appearing in the bitstream up to a specific position.
- Select: Finds the exact index of the k-th occurrence of a 1 bit.
- GetBit: Retrieves the state (0 or 1) of the bit at a specific position.
- Insert: Adds a new bit at any given index, expanding the tree.
- Delete: Removes the bit at a specific index and rebalances the tree structure.
