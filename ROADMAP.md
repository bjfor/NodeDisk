# Roadmap

## 1.0

- independent C++ repository skeleton
- sync core available
- backup flow available
- shared library flow available
- SQLite metadata available
- mirror and FastDFS storage backends available
- CLI / demo / regression test entry points available

## 2.0

- control-plane persistence
  - device registration/state persistence
  - backup/transfer policy persistence
  - scheduled task persistence
  - backup job persistence
  - restore request persistence
  - audit event persistence
- backup center enhancement
  - backup history query
  - latest-backup query by node/path
  - execute restore from stored backup
  - persisted restore state
  - richer restore workflow
  - device-file mapping for restored files
- shared-library persistence and lifecycle management
  - shared entry persistence
  - recipient-node persistence
  - per-recipient delivery state
  - inspect/query support
  - expiry cleanup
  - pending/active list query
  - device-file mapping for received files
- stronger multi-device coordination
  - online device / peer refresh
  - retryable task query
  - manual task retry
- unified control-plane entry
  - control summary
  - unified device/task/shared/restore queries
  - structured JSON output
  - manual retry actions for task / restore / shared receive
- failure-path operations
  - split shared publish / receive flow
  - failed restore retry verified
  - failed shared receive retry verified
  - unified device-file view across backup / share / restore

## 2.x

- document preview / editing integration
- stronger distributed scheduling
- replica management enhancement
- richer operator tooling
