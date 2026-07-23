# Chat Server Execution Plan

## Current Baseline

- Repository: `chat_server`
- Toolkit verified: Qt 6.9.2 MinGW 64-bit
- Baseline build: passed on 2026-07-23 with `cmake --build build --parallel 4`
- Baseline tests: no tests existed before this work

## Completed

### Stage 1 - Inspect and Build Current Project

- Confirmed this is a small Qt TCP server with most behavior in `ChatServer`.
- Confirmed current protocol is newline-delimited JSON.
- Confirmed current feature set is nickname login, group broadcast, private message routing, and online user broadcast.

### Stage 2 - Preserve Existing Features and Add Foundation Tests

- Added `ProtocolCodec` foundation for the required 4-byte big-endian length-prefixed JSON protocol.
- Added Qt Test coverage for complete frames, partial frames, concatenated frames, invalid JSON, and oversized frames.
- Upgraded the CMake language level to C++20.
- Build and Qt Test passed after the changes.

## Next

1. Integrate `ProtocolCodec` into `ChatServer` and introduce per-client input buffers.
2. Split connection handling into `ClientSession`.
3. Add SQLite repositories with WAL, foreign keys, transactions, users, conversations, messages, read state, and attachments.
4. Add `AuthService` with non-plain-text password hashing and formal register/login.

## Honest Gaps

- The new protocol module is tested but not yet wired into live networking.
- Persistence and password hashing are not implemented yet.
- Server responsibilities are still concentrated in `ChatServer`.
- Clazy was attempted with Qt Creator's bundled `clazy-standalone.exe`, but the local Clang tooling failed before project analysis on MinGW system headers. This remains an environment/toolchain setup item; MinGW compilation itself succeeds.
