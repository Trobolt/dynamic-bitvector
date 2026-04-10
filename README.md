# Dynamic BitTree

Built for performance, this C++ dynamic bit vector uses a B+ Tree structure where nodes are constrained to 512 bits to ensure cache-friendly access. Pointer and variable compression via bit-shifting is used to fit the necessary indices into a single cache line, allowing for high-frequency modifications without reallocating large memory blocks.
## Technical Architecture

The core of this project is a modified B+ Tree optimized for bit manipulation:
- Leaves: Each leaf acts as a pointer to bit-data blocks along with local metadata.
- Internal Nodes: These nodes store indices and prefix sums, allowing the tree to act as a searchable index for bit positions and bit counts.
- Efficiency: By using this structure, the project avoids linear shifts during insertion and deletion.

## Core API Reference

The following operations are supported:

- Rank: Returns the number of 1s appearing in the bitstream up to a specific position.
- Select: Finds the exact index of the k-th occurrence of a 1 bit.
- GetBit: Retrieves the state (0 or 1) of the bit at a specific position.
- Insert: Adds a new bit at any given index, expanding the tree.
- Delete: Removes the bit at a specific index and rebalances the tree structure.
