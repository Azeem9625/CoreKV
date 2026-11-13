// CoreKV - Lightweight, thread-safe, in-memory key-value store with a Write-Ahead Log.
// Build: g++ -std=c++14 -Wall -pthread main.cpp -o corekv

#include <atomic>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// StorageEngine: unordered_map + mutex + WAL file
//
// WAL record format (binary-safe, so keys/values may contain spaces/newlines):
//   SET <keylen> <vallen>\n<key bytes><value bytes>\n
//   DEL <keylen>\n<key bytes>\n
// A half-written record at the end of the log (crash mid-write) is detected
// during recovery and ignored.
// ---------------------------------------------------------------------------
class StorageEngine {
private:
    std::unordered_map<std::string, std::string> store;
    mutable std::mutex mtx;   // guards `store` AND the WAL file
    std::FILE* wal;
    std::string walPath;

    // Push the record out of user-space buffers and ask the OS to write it to disk.
    static void flushToDisk(std::FILE* f) {
        std::fflush(f);
#ifdef _WIN32
        _commit(_fileno(f));
#else
        fsync(fileno(f));
#endif
    }

    static void writeSet(std::FILE* f, const std::string& key, const std::string& value) {
        std::fprintf(f, "SET %zu %zu\n", key.size(), value.size());
        std::fwrite(key.data(), 1, key.size(), f);
        std::fwrite(value.data(), 1, value.size(), f);
        std::fputc('\n', f);
    }

    static void writeDel(std::FILE* f, const std::string& key) {
        std::fprintf(f, "DEL %zu\n", key.size());
        std::fwrite(key.data(), 1, key.size(), f);
        std::fputc('\n', f);
    }

    // Replays the log. Returns true if a WAL file existed.
    bool recoverFromWAL() {
        std::ifstream in(walPath.c_str(), std::ios::binary);
        if (!in.is_open()) return false;

        std::string op;
        while (in >> op) {
            if (op == "SET") {
                size_t klen = 0, vlen = 0;
                if (!(in >> klen >> vlen)) break;
                in.get();  // newline after header
                std::string key(klen, '\0'), value(vlen, '\0');
                if (klen && !in.read(&key[0], klen)) break;
                if (vlen && !in.read(&value[0], vlen)) break;
                if (in.get() != '\n') break;       // torn / corrupt record
                store[key] = value;
            } else if (op == "DEL") {
                size_t klen = 0;
                if (!(in >> klen)) break;
                in.get();
                std::string key(klen, '\0');
                if (klen && !in.read(&key[0], klen)) break;
                if (in.get() != '\n') break;
                store.erase(key);
            } else {
                break;  // unknown data: stop replaying
            }
        }
        return true;
    }

    // Snapshot + truncate: rewrite the log so it holds exactly one SET per live key.
    // Runs at startup, which keeps the log from growing without bound and also
    // discards any torn record left by a crash.
    void compact() {
        std::string tmp = walPath + ".tmp";
        std::FILE* f = std::fopen(tmp.c_str(), "wb");
        if (!f) return;
        for (const auto& kv : store) writeSet(f, kv.first, kv.second);
        flushToDisk(f);
        std::fclose(f);

#ifdef _WIN32
        std::remove(walPath.c_str());  // Windows rename() fails if target exists
#endif
        std::rename(tmp.c_str(), walPath.c_str());
    }

public:
    explicit StorageEngine(const std::string& filename) : wal(nullptr), walPath(filename) {
        if (recoverFromWAL()) {
            std::cout << "[Recovery] State restored from WAL.\n";
            compact();
        } else {
            std::cout << "[Recovery] No WAL found, starting fresh.\n";
        }
        wal = std::fopen(walPath.c_str(), "ab");
        if (!wal) throw std::runtime_error("cannot open WAL file: " + walPath);
    }

    ~StorageEngine() {
        if (wal) std::fclose(wal);
    }

    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    // WAL write and in-memory update happen under the SAME lock.
    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mtx);
        writeSet(wal, key, value);
        flushToDisk(wal);         // log first...
        store[key] = value;       // ...then memory
    }

    bool del(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        if (store.find(key) == store.end()) return false;
        writeDel(wal, key);
        flushToDisk(wal);
        store.erase(key);
        return true;
    }

    // Returns false if the key is missing, so an empty value is distinguishable.
    bool get(const std::string& key, std::string& out) const {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = store.find(key);
        if (it == store.end()) return false;
        out = it->second;
        return true;
    }

    bool exists(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mtx);
        return store.count(key) > 0;
    }
};

// ---------------------------------------------------------------------------
// Command design pattern: new commands need no change to StorageEngine.
// ---------------------------------------------------------------------------
class Command {
public:
    virtual ~Command() {}
    virtual void execute(StorageEngine& db) = 0;
};

class SetCommand : public Command {
    std::string key, value;
public:
    SetCommand(std::string k, std::string v) : key(std::move(k)), value(std::move(v)) {}
    void execute(StorageEngine& db) override {
        db.set(key, value);
        std::cout << "SET " << key << " = " << value << "\n";
    }
};

class GetCommand : public Command {
    std::string key;
public:
    explicit GetCommand(std::string k) : key(std::move(k)) {}
    void execute(StorageEngine& db) override {
        std::string result;
        if (db.get(key, result))
            std::cout << "GET " << key << " -> " << result << "\n";
        else
            std::cout << "GET " << key << " -> (not found)\n";
    }
};

class DeleteCommand : public Command {
    std::string key;
public:
    explicit DeleteCommand(std::string k) : key(std::move(k)) {}
    void execute(StorageEngine& db) override {
        if (db.del(key))
            std::cout << "DEL " << key << " -> deleted\n";
        else
            std::cout << "DEL " << key << " -> (not found)\n";
    }
};

class ExistsCommand : public Command {
    std::string key;
public:
    explicit ExistsCommand(std::string k) : key(std::move(k)) {}
    void execute(StorageEngine& db) override {
        std::cout << "EXISTS " << key << " -> " << (db.exists(key) ? "true" : "false") << "\n";
    }
};

// ---------------------------------------------------------------------------
// Concurrent write test: 5 threads writing in parallel, then verified.
// ---------------------------------------------------------------------------
static void concurrentWriteTest(StorageEngine& db) {
    const int N = 5;
    std::atomic<int> ok(0);
    std::vector<std::thread> threads;

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&db, &ok, i]() {
            db.set("thread_key_" + std::to_string(i), "value_" + std::to_string(i));
            ++ok;
        });
    }
    for (auto& t : threads) t.join();

    // Verify every write is actually present with the right value.
    bool verified = true;
    for (int i = 0; i < N; ++i) {
        std::string v;
        if (!db.get("thread_key_" + std::to_string(i), v) || v != "value_" + std::to_string(i))
            verified = false;
    }

    if (ok == N && verified)
        std::cout << "\n[Thread Test] All " << N << " threads wrote successfully.\n";
    else
        std::cout << "\n[Thread Test] FAILED.\n";
}

int main() {
    StorageEngine db("core_kv_wal.log");

    std::cout << "\n--- Basic Commands ---\n";

    std::vector<std::unique_ptr<Command>> cmds;
    cmds.emplace_back(new SetCommand("name", "Aryan Khanna"));
    cmds.emplace_back(new SetCommand("roll", "24/CS/131"));
    cmds.emplace_back(new SetCommand("city", "New Delhi"));
    cmds.emplace_back(new GetCommand("name"));
    cmds.emplace_back(new GetCommand("branch"));
    cmds.emplace_back(new ExistsCommand("roll"));
    cmds.emplace_back(new DeleteCommand("city"));
    cmds.emplace_back(new ExistsCommand("city"));
    cmds.emplace_back(new GetCommand("city"));

    for (auto& c : cmds) c->execute(db);

    std::cout << "\n--- Concurrent Write Test ---\n";
    concurrentWriteTest(db);

    std::cout << "\n--- Reading Concurrent Keys ---\n";
    for (int i = 0; i < 5; ++i)
        GetCommand("thread_key_" + std::to_string(i)).execute(db);

    return 0;
}
