# CoreKV — Lightweight In-Memory Key-Value Store

CoreKV is a thread-safe, in-memory key-value storage engine built in **C++**. It demonstrates real-world implementation of **OS**, **DBMS**, and **OOP** concepts — the kind of system that powers databases like Redis and RocksDB.

---

## Key Features

- **SET / GET / DELETE / EXISTS** — four core operations
- **Write-Ahead Logging (WAL)** — data survives crashes; engine replays the log on startup
- **Thread-Safe** — multiple threads can read/write simultaneously without data races
- **Command Design Pattern** — adding new commands requires zero changes to existing code
- **Concurrent Write Test** — built-in demo with 5 parallel threads

---

## Tech Stack

- **Language:** C++ (C++14 standard)
- **Compiler:** MinGW-w64 (GCC) / g++
- **Build Tool:** Makefile
- **Tools:** Visual Studio Code, Git/GitHub

---

## Project Structure

```
CoreKV/
├── main.cpp      # Full source code
└── Makefile      # Build configuration
```

---

## How to Build & Run

### Step 1 — Install MinGW (if not already)
Download from: https://www.mingw-w64.org  
Make sure `g++` works by running:
```
g++ --version
```

### Step 2 — Compile using Makefile
```bash
make
```
This runs: `g++ -std=c++14 -Wall -pthread main.cpp -o corekv`

### Step 3 — Run
```bash
./corekv
```

### To clean up build files
```bash
make clean
```

### Manual compile (without Makefile)
```bash
g++ -std=c++14 -Wall -pthread main.cpp -o corekv
./corekv
```

---

## Example Output

```
[Recovery] State restored from WAL.

--- Basic Commands ---
SET name = Aryan Khanna
SET roll = 24/CS/131
SET city = New Delhi
GET name -> Aryan Khanna
GET branch -> (not found)
EXISTS roll -> true
DEL city -> deleted
EXISTS city -> false
GET city -> (not found)

--- Concurrent Write Test ---

[Thread Test] All 5 threads wrote successfully.

--- Reading Concurrent Keys ---
GET thread_key_0 -> value_0
GET thread_key_1 -> value_1
GET thread_key_2 -> value_2
GET thread_key_3 -> value_3
GET thread_key_4 -> value_4
```

---

## CS Concepts Demonstrated

### DBMS — Write-Ahead Logging & Recovery
Every `SET` and `DEL` is written to `core_kv_wal.log` **before** modifying memory. On next startup, the engine reads this log and restores exact state — same principle used in PostgreSQL and SQLite.

### OS — Concurrency & Thread Safety
Uses `std::mutex` with RAII-based `std::lock_guard` to protect critical sections. WAL write and in-memory update happen under the **same lock** — no phantom reads possible between the two operations.

### OOP — Command Design Pattern
`SetCommand`, `GetCommand`, `DeleteCommand`, `ExistsCommand` all inherit from an abstract `Command` class. New commands can be added without touching `StorageEngine` — open for extension, closed for modification (OCP).

---

## Architecture

```
main()
  │
  ├── StorageEngine  ←── holds unordered_map + mutex + WAL file
  │
  └── Command (abstract)
        ├── SetCommand
        ├── GetCommand
        ├── DeleteCommand
        └── ExistsCommand
```

---

## Potential Extensions

- `TTLCommand` — auto-expire keys after N seconds
- WAL compaction — snapshot + truncate log to prevent unbounded growth  
- HTTP interface — expose GET/SET over REST using a simple socket server
- Persistent B-Tree index — replace `unordered_map` for range query support
