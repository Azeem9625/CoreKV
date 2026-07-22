#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <memory>

using namespace std;

class StorageEngine {
private:
    unordered_map<string, string> store;
    mutex rw_lock; 
    ofstream wal_file;
    mutex wal_mutex;

    void recoverFromWAL(const string& filename) {
        ifstream infile(filename);
        if (!infile.is_open()) return;
        string command, key, value;
        while (infile >> command >> key >> value) {
            if (command == "SET") {
                store[key] = value;
            }
        }
        cout << "[System] WAL recovery completed. State restored.\n";
    }

public:
    StorageEngine(const string& wal_filename) {
        wal_file.open(wal_filename, ios::out | ios::app);
        recoverFromWAL(wal_filename);
    }

    ~StorageEngine() {
        if (wal_file.is_open()) {
            wal_file.close();
        }
    }

    void set(const string& key, const string& value) {
        {
            lock_guard<mutex> log_guard(wal_mutex);
            if (wal_file.is_open()) {
                wal_file << "SET " << key << " " << value << "\n";
                wal_file.flush();
            }
        }
        lock_guard<mutex> lock(rw_lock);
        store[key] = value;
    }

    string get(const string& key) {
        lock_guard<mutex> lock(rw_lock);
        auto it = store.find(key);
        if (it != store.end()) {
            return it->second;
        }
        return "[Error: Key not found]";
    }
};

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(StorageEngine& engine) = 0;
};

class SetCommand : public Command {
private:
    string key;
    string value;
public:
    SetCommand(string k, string v) : key(move(k)), value(move(v)) {}
    void execute(StorageEngine& engine) override {
        engine.set(key, value);
        cout << "[Executed] SET " << key << " = " << value << "\n";
    }
};

class GetCommand : public Command {
private:
    string key;
public:
    explicit GetCommand(string k) : key(move(k)) {}
    void execute(StorageEngine& engine) override {
        string result = engine.get(key);
        cout << "[Executed] GET " << key << " -> " << result << "\n";
    }
};

int main() {
    StorageEngine db("core_kv_wal.log");

    unique_ptr<Command> cmd1 = make_unique<SetCommand>("roll_number", "24/CS/131");
    unique_ptr<Command> cmd2 = make_unique<GetCommand>("roll_number");

    cmd1->execute(db);
    cmd2->execute(db);

    return 0;
}
