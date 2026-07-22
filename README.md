# CoreKV: Lightweight In-Memory Key-Value Store

CoreKV is a robust, thread-safe, in-memory key-value storage engine implemented in **C++**. It demonstrates the practical implementation of core Computer Science concepts spanning **Operating Systems**, **Database Management Systems (DBMS)**, and **Object-Oriented Programming (OOPs)**.

---

## 🚀 Key Architectural Features

### 1. Database Management Systems (DBMS): Write-Ahead Logging (WAL) & Recovery
* **Durability & Crash Recovery:** To prevent data loss on sudden system failures, all `SET` operations are sequentially logged to a persistent file (`core_kv_wal.log`) *before* modifying the in-memory state.
* **State Restoration:** Upon startup, the engine automatically executes a recovery routine (`recoverFromWAL`) that replays the log file to restore the database to its exact previous state.

### 2. Operating Systems (OS): Concurrency & Thread Safety
* **Mutual Exclusion:** Utilizes `std::mutex` and RAII-based `std::lock_guard` to protect critical sections and prevent data races during concurrent read/write operations.

### 3. Object-Oriented Programming (OOPs): Command Design Pattern
* **Extensibility:** The project implements the **Command Design Pattern** (`Command`, `SetCommand`, `GetCommand`). This decouples the execution logic from the storage engine, making it modular and easy to extend with new commands (like `DELETE` or `UPDATE`) without modifying existing code.

---

## 🛠️ Tech Stack
* **Language:** C++ (C++11/C++14 standard)
* **Compiler:** MinGW-w64 (GCC)
* **Tools:** Visual Studio Code, Git/GitHub

---

## 💻 Code Structure & Usage
The project consists of a single modular entry point (`main.cpp`) demonstrating polymorphically driven command execution and persistent state management:

```cpp
// Example usage snippet from main.cpp
StorageEngine db("core_kv_wal.log");

unique_ptr<Command> cmd1 = make_unique<SetCommand>("roll_number", "24/CS/131");
unique_ptr<Command> cmd2 = make_unique<GetCommand>("roll_number");

cmd1->execute(db);
cmd2->execute(db);
