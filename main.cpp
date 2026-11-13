#include <iostream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <memory>

using namespace std;

class StorageEngine {
private:
    unordered_map<string, string> store;
    ofstream wal;

    void recoverFromWAL(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) return;

        string line;
        while (getline(file, line)) {
            istringstream ss(line);
            string op, key, value;
            ss >> op >> key;
            getline(ss, value);
            if (!value.empty() && value[0] == ' ') value = value.substr(1);

            if (op == "SET") store[key] = value;
            else if (op == "DEL") store.erase(key);
        }
        cout << "[Recovery] State restored from WAL.\n";
    }

public:
    StorageEngine(const string& wal_filename) {
        recoverFromWAL(wal_filename);
        wal.open(wal_filename.c_str(), ios::app);
    }

    ~StorageEngine() {
        if (wal.is_open()) wal.close();
    }

    void set(const string& key, const string& value) {
        wal << "SET " << key << " " << value << "\n";
        wal.flush();
        store[key] = value;
    }

    bool del(const string& key) {
        if (store.find(key) == store.end()) return false;
        wal << "DEL " << key << "\n";
        wal.flush();
        store.erase(key);
        return true;
    }

    string get(const string& key) {
        unordered_map<string, string>::iterator it = store.find(key);
        if (it != store.end()) return it->second;
        return "";
    }

    bool exists(const string& key) {
        return store.count(key) > 0;
    }
};


class Command {
public:
    virtual ~Command() {}
    virtual void execute(StorageEngine& db) = 0;
};

class SetCommand : public Command {
    string key, value;
public:
    SetCommand(string k, string v) : key(k), value(v) {}
    void execute(StorageEngine& db) {
        db.set(key, value);
        cout << "SET " << key << " = " << value << "\n";
    }
};

class GetCommand : public Command {
    string key;
public:
    GetCommand(string k) : key(k) {}
    void execute(StorageEngine& db) {
        string result = db.get(key);
        if (!result.empty())
            cout << "GET " << key << " -> " << result << "\n";
        else
            cout << "GET " << key << " -> (not found)\n";
    }
};

class DeleteCommand : public Command {
    string key;
public:
    DeleteCommand(string k) : key(k) {}
    void execute(StorageEngine& db) {
        bool deleted = db.del(key);
        if (deleted)
            cout << "DEL " << key << " -> deleted\n";
        else
            cout << "DEL " << key << " -> (not found)\n";
    }
};

class ExistsCommand : public Command {
    string key;
public:
    ExistsCommand(string k) : key(k) {}
    void execute(StorageEngine& db) {
        if (db.exists(key))
            cout << "EXISTS " << key << " -> true\n";
        else
            cout << "EXISTS " << key << " -> false\n";
    }
};


int main() {
    StorageEngine db("core_kv_wal.log");

    cout << "\n--- Basic Commands ---\n";

    Command* c1 = new SetCommand("name", "Aryan Khanna");
    Command* c2 = new SetCommand("roll", "24/CS/131");
    Command* c3 = new SetCommand("city", "New Delhi");
    Command* c4 = new GetCommand("name");
    Command* c5 = new GetCommand("branch");
    Command* c6 = new ExistsCommand("roll");
    Command* c7 = new DeleteCommand("city");
    Command* c8 = new ExistsCommand("city");
    Command* c9 = new GetCommand("city");

    c1->execute(db);
    c2->execute(db);
    c3->execute(db);
    c4->execute(db);
    c5->execute(db);
    c6->execute(db);
    c7->execute(db);
    c8->execute(db);
    c9->execute(db);

    delete c1; delete c2; delete c3;
    delete c4; delete c5; delete c6;
    delete c7; delete c8; delete c9;

    cout << "\n--- WAL Recovery Test ---\n";
    cout << "Restart the program to see WAL recovery in action.\n";

    return 0;
}
