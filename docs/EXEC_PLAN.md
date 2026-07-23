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

### Stage 3 - Client UI Prototype Checkpoint

- Server code was not changed in this UI-only stage.
- Server build and `protocolcodec` Qt Test were rerun successfully to keep both repositories verified.

### Stage 4 - Client Model Extraction Checkpoint

- Server code was not changed in this client-only model extraction stage.
- Server build and `protocolcodec` Qt Test were rerun successfully.

### Stage 5 - Wire Length-Prefixed JSON Protocol

- Updated `ChatServer` to use `ProtocolCodec` for outbound frames.
- Added per-client input buffers and decode loops for partial frames, concatenated frames, invalid frames, and oversized buffered input.
- Normalized envelope payloads before existing command dispatch.
- Build passed and server `protocolcodec` Qt Test passed.
- Real TCP smoke test passed with envelope login and server responses: `login_result`, `system`, `user_list`.

### Stage 6 - SQLite Repository Foundation

- Added `SQLiteRepository` with isolated SQL access.
- Opens SQLite with foreign keys, WAL, and busy timeout enabled.
- Added schema for users, conversations, conversation members, messages, and attachments.
- Uses transactions for schema migration and parameterized SQL for repository writes/reads.
- Added Qt Test coverage proving users, conversations, and messages persist after closing and reopening the database.
- Build passed and server tests passed: `protocolcodec`, `sqliterepository`.

### Stage 7 - AuthService Password Hashing Foundation

- Added `AuthService` with register and login verification paths.
- Implemented PBKDF2-HMAC-SHA256 password hashing with per-user random salts and encoded iteration metadata.
- Repository now exposes credential lookup without moving SQL into service logic.
- Added Qt Test coverage for successful registration/login, wrong-password rejection, duplicate user rejection, short password rejection, and no plaintext password storage.
- Build passed and server tests passed: `protocolcodec`, `sqliterepository`, `authservice`.

### Stage 8 - Live Register/Login Integration

- `ChatServer` now opens `chat_server.sqlite` on startup, initializes schema, and creates the hall conversation.
- Added live `register` command using `AuthService`; successful registration logs the connection in.
- Added password-aware `login` command using `AuthService` while preserving existing nickname-only compatibility during migration.
- Group messages are now persisted into the hall conversation.
- End-to-end TCP smoke test passed for register, server restart, and login using the persisted account.

### Stage 9 - Hall History Loading

- Added live `history` request handling for the hall conversation.
- Server returns `history_result` with persisted message metadata and content.
- End-to-end TCP smoke test passed for register, send hall message, request history, and verify the persisted message is returned.

### Stage 10 - Direct Message Persistence and History

- Added stable direct conversation IDs with canonical username ordering.
- Private messages now carry `conversation_id` and are persisted through `SQLiteRepository`.
- `history` now works for direct conversations as well as the hall.
- End-to-end TCP smoke test passed for two registered users, private message delivery, and direct history lookup.

### Stage 11 - Heartbeat and Connection Timeout Foundation

- Added `ping` handling and `pong` responses.
- Added per-client last-activity timestamps and a periodic timeout check.
- Server disconnects connections idle for more than the configured timeout.
- Build passed and server tests passed.
- End-to-end TCP heartbeat smoke test passed.

### Stage 12 - Client Auto Reconnect Checkpoint

- Server code was not changed in this client-only reconnect stage.
- Server build and all server Qt Tests were rerun successfully.

### Stage 13 - README and Client Packaging Checkpoint

- Added `README.md` with build, test, run, current status, and known remaining work.
- Server build and all server Qt Tests were rerun successfully.
- Client packaging script was verified in the client repository.

### Stage 14 - Small File Transfer Foundation

- Added `file_message` handling with 512 KB inline base64 validation.
- Server forwards file messages to the hall or a direct conversation.
- File message metadata is persisted as `kind = 'file'`.
- End-to-end TCP smoke test passed for sending a hall file message and reading it from history.

### Stage 15 - Request ID Deduplication

- Added per-connection `request_id` tracking for mutating requests.
- Duplicate `register`, `message`, `private_message`, and `file_message` requests are ignored after the first successful dispatch path starts.
- End-to-end TCP smoke test passed: two messages with the same `request_id` produce one persisted hall message.

### Stage 16 - Client UI Rendering Polish Checkpoint

- Server code was not changed in this client-focused UI rendering stage.
- Server build and all server Qt Tests were rerun successfully.
- Client main-window screenshots and right-panel collapse tests passed.

### Stage 17 - Read-State Synchronization

- Added `SQLiteRepository::markConversationRead` and `unreadCount` using `conversation_members.last_read_message_id`.
- Added Qt Test coverage for unread count calculation, mark-read persistence, and ignoring the reader's own messages.
- Added live `mark_read` protocol handling and `read_state` responses.
- Included `mark_read` in request-id deduplication because it mutates durable read state.
- Server build and all server Qt Tests passed.
- End-to-end TCP smoke test passed for register, hall message, history, `mark_read`, and `read_state unread_count = 0`.

## Next

1. Split connection handling into `ClientSession`.
2. Add reconnect/session sync.
3. Replace inline file transfer with chunked attachment storage.

## Honest Gaps

- The new protocol module is tested but not yet wired into live networking.
- Live register/login is wired for password-based accounts; nickname-only login remains temporarily for migration and should be removed once the client no longer needs compatibility.
- Server responsibilities are still concentrated in `ChatServer`.
- Clazy is not currently available in `PATH`; an earlier Qt Creator bundled `clazy-standalone.exe` attempt failed before project analysis on MinGW system headers. This remains an environment/toolchain setup item; MinGW compilation itself succeeds.
