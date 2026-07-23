# Qt Chat Server

Qt TCP server for the desktop chat project. The server is designed to stay close to Qt Core, Network, and SQL APIs so it can remain portable to Linux later.

## Requirements

- Qt 6.9.2 MinGW 64-bit on Windows, or Qt 6.9+ with CMake on Linux
- CMake
- SQLite Qt SQL driver

## Build on Windows

```powershell
$env:Path = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.9.2\mingw_64\bin;" + $env:Path
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:\Qt\6.9.2\mingw_64
cmake --build build --parallel 4
```

## Test

```powershell
ctest --test-dir build --output-on-failure
```

Current test targets:

- `protocolcodec`
- `sqliterepository`
- `authservice`

## Run

```powershell
.\build\chat_server.exe
```

The server listens on TCP port `8888` and stores SQLite data next to the executable as `chat_server.sqlite`.

## Current Status

Implemented:

- Length-prefixed UTF-8 JSON protocol
- Register/login with PBKDF2-HMAC-SHA256 password hashing
- SQLite schema and repository layer
- WAL, foreign keys, parameterized SQL, and schema transactions
- Public hall messages and direct messages
- Hall and direct-message history queries
- Online user broadcasts
- Heartbeat `ping`/`pong` and idle timeout foundation
- Small inline file-message transfer foundation

Still in progress:

- Read-state sync and durable unread counts
- Duplicate request ID handling
- Chunked file/image transfer, attachment storage, progress, and download flow
- `ClientSession` extraction from `ChatServer`
- Final Linux compatibility pass
