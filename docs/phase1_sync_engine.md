# Phase 1 Sync Engine Design

## Goal

Phase 1 narrows the project from a web-first cloud drive into a C++ sync-engine foundation.
The first milestone is a safe desktop-to-desktop synchronization loop with these guarantees:

- fixed-size chunking for large files
- deterministic file and chunk hashes
- durable local metadata
- resumable transfers
- atomic file commit after full verification
- no file corruption if a node exits during transfer

This phase intentionally does not include:

- AI search
- public share square
- mobile support
- embedded ZeroTier SDK
- vector clocks
- Merkle-tree conflict resolution

## Architecture

Phase 1 uses a simple node model:

- each device is a node
- nodes are connected by an existing ZeroTier virtual network
- the sync engine binds to the ZeroTier IP
- synchronization is pull-based after index exchange

Logical modules:

1. `Chunker`
   Splits files into fixed-size chunks and computes hashes.
2. `Scanner`
   Walks watched directories and builds a file manifest.
3. `MetadataStore`
   Persists file, chunk, and sync-task metadata.
4. `Protocol`
   Defines node-to-node message payloads.
5. `SyncWriter`
   Receives chunk data, writes a temporary file, verifies it, and performs atomic rename.

## Safety Rules

Phase 1 treats data safety as the primary product feature.

1. A synced file is never written directly to the final path.
2. Incoming data is stored in a temporary `.part` file.
3. The final file hash must match metadata before commit.
4. Metadata is updated to `ready` only after rename succeeds.
5. Deletes are represented as metadata state changes first, not immediate physical deletion.

## Current Implementation Scope

The repository now introduces a new modular project layout that is independent from the legacy CGI modules.
This allows the sync engine to evolve without breaking the existing upload/storage code path.

Current deliverables in code:

- sync data structures
- fixed-size chunk planner
- local scanner
- metadata store interface with an in-memory implementation
- protocol models
- atomic sync writer
- a small CLI demo target for early validation

## Storage Model

Phase 1 keeps metadata local to each node and focuses on correctness of the local engine.

Core records:

- `NodeInfo`
- `FileRecord`
- `ChunkRecord`
- `ReplicaRecord`
- `SyncTask`

The current implementation uses an in-memory store plus explicit schema notes in code.
For persistent metadata, Phase 2 can add SQLite or RocksDB behind the same interface.

## Hashing Strategy

The target production hash is BLAKE3.
Because the existing repository already ships an MD5 implementation and Phase 1 focuses on engine wiring,
the first code skeleton wraps the existing hash capability behind an abstract hasher interface.
This keeps the chunker and writer independent from the final hash backend.

## Directory Layout

`include/netdisk/`
Public headers for the new engine.

`src/`
Implementation files.

`apps/`
Executable entry points.

## Immediate Next Steps

1. Replace the temporary hash backend with BLAKE3.
2. Add a persistent metadata backend.
3. Add ZeroTier-aware RPC transport.
4. Implement missing-chunk negotiation and transfer.
5. Add conflict/version handling after the single-writer path is stable.
