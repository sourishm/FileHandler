#include <iostream>
#include <list>
#include <mutex>
#include <condition_variable>
#include <memory>

using namespace std;

auto range_comparison = [] (int64_t start, int64_t end, int64_t value) -> bool {
    return ((start <= value) && (value <= end));
};

auto print_lock_attempt = [] (int64_t& offset, int64_t& length, bool type) {
    const char* lock = type ? "WRITE" : "READ";
    cout << "Attempting to " << lock << " lock from offset " << offset << " and range " << length << endl;
};

auto print_lock_success = [] () {cout << "Locking success" << endl;};

class FileRange {
private:
    int64_t offset, length;
    bool write_exclusive;
public:
    FileRange(int64_t& _offset, int64_t& _length, bool type) : offset(_offset), length (_length), write_exclusive(type) {}
    ~FileRange() {}
    inline bool withinRange(int64_t& _offset, int64_t& _length)
    {
        return (range_comparison(offset, (offset+length), _offset) ||
                range_comparison(_offset, (_offset+_length), offset));
    }
    inline bool matchFileRange(int64_t& _offset, int64_t& _length, bool type)
    {
        return ((offset == _offset) && (length == _length) && (write_exclusive == type));
    }
    inline bool isWrite() { return write_exclusive; }
};

class FileHandle {
private:
    list<unique_ptr<FileRange>> frList;
    mutex mtx;
    condition_variable cv;
    bool rangeCheck(int64_t& _offset, int64_t& _length, bool write)
    {
        for (auto& x : frList) {
            if (x->withinRange(_offset, _length) && (!write || x->isWrite()))
                return true;
        }
        return false;
    }
public:
    void readLock(int64_t _offset, int64_t _length)
    {
        unique_lock<mutex> uniqueLock(mtx);
        print_lock_attempt(_offset, _length, false);
        while (rangeCheck(_offset, _length, true)) {
            cv.wait(uniqueLock);
        }
        unique_ptr<FileRange> fileRange(new FileRange(_offset, _length, false));
        frList.push_back(move(fileRange));
        print_lock_success();
    }
    void readUnlock(int64_t _offset, int64_t _length)
    {
        unique_lock<mutex> uniqueLock(mtx);
        for (auto& x : frList) {
            if (x->matchFileRange(_offset, _length, false)) {
                frList.remove(x);
                break;
            }
        }
        uniqueLock.unlock();
        cv.notify_all();
    }
    void writeLock(int64_t _offset, int64_t _length)
    {
        unique_lock<mutex> uniqueLock(mtx);
        print_lock_attempt(_offset, _length, true);
        while (rangeCheck(_offset, _length, false)) {
            cv.wait(uniqueLock);
        }
        unique_ptr<FileRange> fileRange(new FileRange(_offset, _length, true));
        frList.push_back(move(fileRange));
        print_lock_success();
    }
    void writeUnlock(int64_t _offset, int64_t _length)
    {
        unique_lock<mutex> uniqueLock(mtx);
        for (auto& x : frList) {
            if (x->matchFileRange(_offset, _length, true)) {
                frList.remove(x);
                break;
            }
        }
        uniqueLock.unlock();
        cv.notify_all();
    }
};

int main() {
    FileHandle fileHandle;
    fileHandle.readLock(100, 50);
    fileHandle.writeLock(120, 50);
    return 0;
}