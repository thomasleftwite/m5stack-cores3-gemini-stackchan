#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
struct StorageMock {
    std::map<std::string, std::vector<uint8_t>> files;
    bool failRename = false, shortWrite = false;
};
inline StorageMock storage;
class File {
    std::string path_; bool open_ = false; size_t at_ = 0;
 public:
    File() = default;
    File(const char* path, const char* mode) : path_(path) {
        if (*mode == 'w') { storage.files[path_].clear(); open_ = true; }
        else open_ = storage.files.count(path_) != 0;
    }
    explicit operator bool() const { return open_; }
    size_t size() const { return open_ ? storage.files[path_].size() : 0; }
    size_t read(uint8_t* data, size_t count) {
        if (!open_) return 0; auto& bytes = storage.files[path_]; count = std::min(count, bytes.size() - at_);
        memcpy(data, bytes.data() + at_, count); at_ += count; return count;
    }
    size_t write(const uint8_t* data, size_t count) {
        if (!open_) return 0; if (storage.shortWrite) count /= 2;
        auto& bytes = storage.files[path_]; bytes.insert(bytes.end(), data, data+count); return count;
    }
    void flush() {} void close() { open_ = false; }
};
struct LittleFSMock {
    File open(const char* path, const char* mode) { return File(path, mode); }
    bool exists(const char* path) { return storage.files.count(path) != 0; }
    bool remove(const char* path) { return storage.files.erase(path) != 0; }
    bool rename(const char* a, const char* b) {
        if (storage.failRename || !exists(a)) return false;
        storage.files[b] = storage.files[a]; storage.files.erase(a); return true;
    }
};
inline LittleFSMock LittleFS;
