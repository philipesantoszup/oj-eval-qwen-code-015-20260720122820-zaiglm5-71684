#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <set>
#include <map>
#include <cstdint>

using namespace std;

// Memory-efficient file-based key-value store
// Uses append-only log approach:
// - insert: append record with deleted=0
// - delete: find original record and mark as deleted (rewrite header to mark file dirty)
// - on startup: replay all records to rebuild index, handling deletes correctly

const string DATA_FILE = "data.bin";

class FileKVStore {
private:
    fstream data_file;
    
    // In-memory index for current session
    map<string, set<int>> index;
    // Track positions of non-deleted entries for efficient deletion
    map<string, map<int, streampos>> positions;
    
    struct Record {
        string key;
        int value;
        bool deleted;
        streampos pos;
    };
    
    void write_record(const string& key, int value, bool deleted) {
        data_file.seekp(0, ios::end);
        
        uint8_t key_len = (uint8_t)key.length();
        data_file.write(reinterpret_cast<const char*>(&key_len), 1);
        data_file.write(key.c_str(), key_len);
        
        int32_t v = value;
        data_file.write(reinterpret_cast<const char*>(&v), sizeof(int32_t));
        
        char del = deleted ? 1 : 0;
        data_file.write(&del, 1);
        
        data_file.flush();
    }
    
    void rebuild_index() {
        index.clear();
        positions.clear();
        
        data_file.seekg(0, ios::beg);
        
        while (data_file.good()) {
            streampos pos = data_file.tellg();
            
            uint8_t key_len;
            data_file.read(reinterpret_cast<char*>(&key_len), 1);
            if (data_file.eof() || !data_file.good()) break;
            
            char key_buf[256];
            data_file.read(key_buf, key_len);
            key_buf[key_len] = '\0';
            string key(key_buf);
            
            int32_t value;
            data_file.read(reinterpret_cast<char*>(&value), sizeof(int32_t));
            
            char deleted;
            data_file.read(&deleted, 1);
            
            if (deleted == 0) {
                // Only add if not deleted
                index[key].insert(value);
                positions[key][value] = pos;
            } else {
                // Remove if previously added (handles delete records)
                if (index[key].count(value)) {
                    index[key].erase(value);
                    positions[key].erase(value);
                }
            }
        }
        
        data_file.clear();
    }
    
    bool mark_deleted(streampos pos, const string& key, int value) {
        data_file.seekp(pos, ios::beg);
        
        uint8_t key_len = (uint8_t)key.length();
        data_file.write(reinterpret_cast<const char*>(&key_len), 1);
        data_file.write(key.c_str(), key_len);
        
        int32_t v = value;
        data_file.write(reinterpret_cast<const char*>(&v), sizeof(int32_t));
        
        char del = 1;  // Mark as deleted
        data_file.write(&del, 1);
        
        data_file.flush();
        return true;
    }
    
public:
    FileKVStore() {
        data_file.open(DATA_FILE, ios::in | ios::out | ios::binary | ios::ate);
        if (!data_file.good()) {
            // Create new file
            data_file.clear();
            data_file.open(DATA_FILE, ios::out | ios::binary);
            data_file.close();
            data_file.open(DATA_FILE, ios::in | ios::out | ios::binary);
        } else {
            rebuild_index();
        }
    }
    
    ~FileKVStore() {
        data_file.close();
    }
    
    void insert(const string& key, int value) {
        // Check if (key, value) already exists
        auto it = index.find(key);
        if (it != index.end() && it->second.count(value)) {
            return;  // Already exists
        }
        
        streampos pos = data_file.tellp();
        pos = data_file.tellp();
        data_file.seekp(0, ios::end);
        pos = data_file.tellp();
        
        write_record(key, value, false);
        index[key].insert(value);
        positions[key][value] = pos;
    }
    
    void remove(const string& key, int value) {
        auto it = index.find(key);
        if (it == index.end() || !it->second.count(value)) {
            return;  // Doesn't exist
        }
        
        auto pos_it = positions.find(key);
        if (pos_it != positions.end()) {
            auto pos_value_it = pos_it->second.find(value);
            if (pos_value_it != pos_it->second.end()) {
                // Mark the original record as deleted
                mark_deleted(pos_value_it->second, key, value);
            }
        }
        
        index[key].erase(value);
        positions[key].erase(value);
        if (index[key].empty()) {
            index.erase(key);
            positions.erase(key);
        }
    }
    
    vector<int> find(const string& key) {
        vector<int> result;
        auto it = index.find(key);
        if (it != index.end()) {
            for (int v : it->second) {
                result.push_back(v);
            }
        }
        return result;  // Already sorted (set maintains order)
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    FileKVStore store;
    
    int n;
    cin >> n;
    
    string cmd, key;
    int value;
    
    for (int i = 0; i < n; i++) {
        cin >> cmd >> key;
        if (cmd == "insert") {
            cin >> value;
            store.insert(key, value);
        } else if (cmd == "delete") {
            cin >> value;
            store.remove(key, value);
        } else if (cmd == "find") {
            vector<int> result = store.find(key);
            if (result.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << result[j];
                }
                cout << "\n";
            }
        }
    }
    
    return 0;
}
