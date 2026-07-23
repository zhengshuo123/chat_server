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

### Stage 18 - Client SQLite Cache Checkpoint

- Server code was not changed in this client-focused local persistence stage.
- Server build and all server Qt Tests were rerun successfully.
- Client build and Qt Tests passed, including the new `clientrepository` SQLite cache tests.

### Stage 19 - Known Conversation Resync Checkpoint

- Server code was not changed in this client-focused reconnect sync stage.
- Server build and all server Qt Tests were rerun successfully.
- Client now requests history for all known local conversations after login/reconnect and for newly discovered online direct conversations.

### Stage 20 - Conversation List Sync API

- Added repository support for conversation membership and per-user conversation listing.
- Login now ensures the user is a member of the hall conversation.
- Direct text/file messages now ensure both participants are members of the direct conversation.
- Added live `conversation_list` handling with unread counts.
- Added Qt Test coverage for member conversation listing and unread count calculation.
- Server build and all server Qt Tests passed.
- End-to-end TCP smoke test passed for registering two users, creating a direct conversation, and retrieving it through `conversation_list`.

### Stage 21 - Client Background Local Store Checkpoint

- Server code was not changed in this client-focused local store threading stage.
- Server build and all server Qt Tests were rerun successfully.
- Client now queues runtime local SQLite writes to a dedicated worker thread.

### Stage 22 - Clazy Verification

- Located Qt Creator's bundled `clazy-standalone.exe`.
- Reconfigured the server build with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`.
- Ran Clazy with explicit MinGW target/include arguments over `ChatServer`, repository, auth service, and protocol codec sources.
- Server build and all server Qt Tests passed.
- Server Clazy completed with no project warnings.
- Client build, client Qt Tests, and client Clazy completed with no project warnings.

### Stage 23 - Client File Message UI Checkpoint

- Server code was not changed in this client-focused file UI stage.
- Server build and all server Qt Tests were rerun successfully.
- Client now renders file cards, caches current inline file payloads locally, and supports saving cached file messages.

### Stage 24 - Client Windows Packaging Checkpoint

- Server code was not changed in this client packaging recheck.
- Client `windeployqt` package was regenerated after Qt Sql support was added.
- Client package includes the SQLite driver at `sqldrivers/qsqlite.dll`.

### Stage 25 - Server Attachment Storage And Download

- File messages are now saved as durable server attachments under the server application `attachments` directory.
- `SQLiteRepository` can return inserted message IDs, persist attachment metadata, and load attachment metadata with history messages.
- Added `file_download` handling and `file_download_result` responses so clients can retrieve persisted attachments after restart.
- History responses now include `attachment_id`, file name, and size for file messages.
- Private file sends clean up the saved file when the target is offline or database persistence fails, avoiding orphaned attachment files.
- Added Qt Test coverage for attachment metadata persistence and lookup.
- Server build, all server Qt Tests, and server Clazy passed.
- Client build, all client Qt Tests, and client Clazy passed as the download-consumer checkpoint.

### Stage 26 - Chunked File Upload

- Added `file_upload_start`, `file_upload_chunk`, `file_upload_ready`, and `file_upload_progress` handling.
- Server stores upload chunks directly on disk, validates offsets, enforces a 64 KB chunk size, and caps attachments at 50 MB.
- Completed chunk uploads are persisted as file messages plus attachment metadata, then broadcast with `attachment_id`.
- Incomplete upload files are deleted if the client disconnects or sends an invalid chunk.
- Existing inline `file_message` handling remains for compatibility with older clients.
- Server build, all server Qt Tests, and server Clazy passed.
- Real TCP smoke test passed for register, chunked hall upload, `file_chat` attachment ID, and history lookup.
- Client build, all client Qt Tests, client Clazy, and Windows packaging recheck passed as the upload producer checkpoint.

### Stage 27 - Chunked File Download

- `file_download` now responds with `file_download_ready` followed by sequential `file_download_chunk` frames instead of one inline payload.
- Download responses validate the persisted attachment file size before streaming.
- Existing attachment lookup and permission/login checks remain in the server download path.
- Server build, all server Qt Tests, and server Clazy passed.
- Real TCP smoke test passed for chunked upload, chunked download, and byte-for-byte attachment round trip.
- Client build, all client Qt Tests, client Clazy, and Windows packaging recheck passed as the download consumer checkpoint.

### Stage 28 - Client Image Thumbnail Checkpoint

- Server code was not changed in this client-focused rendering stage.
- Server build, all server Qt Tests, and server Clazy were rerun successfully.
- Client now renders thumbnails for locally cached image attachments while preserving generic cards for other files.

## Next

1. Split connection handling into `ClientSession`.
2. Add image thumbnail metadata if the server needs to advertise dimensions before download.
3. Remove nickname-only login compatibility once migration is no longer needed.

## Honest Gaps

- Live register/login is wired for password-based accounts; nickname-only login remains temporarily for migration and should be removed once the client no longer needs compatibility.
- Server responsibilities are still concentrated in `ChatServer`.
- Clazy is not in `PATH`, but Qt Creator's bundled `clazy-standalone.exe` works when invoked with explicit MinGW target/include arguments.
