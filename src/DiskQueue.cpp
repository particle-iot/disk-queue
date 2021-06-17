/*
 * Copyright (c) 2021 Particle Industries, Inc.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DiskQueue.h"

// TODO
// * Implement disk quota
// * Properly unlink file given corrupt items/headers

int DiskQueue::start(const char* path) {
    // Check if already running
    CHECK_FALSE(_running, SYSTEM_ERROR_NONE);

    const std::lock_guard<RecursiveMutex> lock(_lock);

    int ret = SYSTEM_ERROR_NONE;
    do {
        if (!dirExists(path) && mkdir(path, 0775)) {
            ret = SYSTEM_ERROR_FILE;
            break;
        }

        _path = String(path) + "/";

        // Create a list of all filenames that may contain previously saved data
        getFilenames(path);

        // Point us to the file that will form the front of the queue
        getNextWriteFile();

        // Point us to the file that will form the back of the queue
        getNextReadFile();

        _running = true;

        return SYSTEM_ERROR_NONE;
    } while (false);

    // Cleanup if errors
    cleanup();
    return ret;
}

int DiskQueue::stop() {
    const std::lock_guard<RecursiveMutex> lock(_lock);

    _running = false;

    // Close all files and
    cleanupFiles();

    return SYSTEM_ERROR_NONE;
}


void DiskQueue::pop_front() {
    if (!_running) {
        return;
    }

    const std::lock_guard<RecursiveMutex> lock(_lock);

    volatile auto offset = lseek(_fdRead, 0, SEEK_CUR);  // TODO: remove me
    QueueItemHeader item = {};
    auto rret = read(_fdRead, &item, sizeof(item));
    // Check for end of file
    if (rret < (int)sizeof(item)) {
        closeFile(_fdRead);  // TODO do this somewhere else
        // TODO goto next file
        return;
    }
    // Check for correct magic
    if (QueueItemMagic != item.magic) {
        closeFile(_fdRead);  // TODO do this somewhere else
        // TODO goto next file
        return;
    }
    // Check for active entry
    if (item.flags & ItemFlagActive) {
    }

    // TODO check if seek will go past end of file
    // Advance to next entry
    lseek(_fdRead, item.length, SEEK_CUR);
}

bool DiskQueue::front(void* data, size_t& size) {
    CHECK_TRUE(_running, false);

    const std::lock_guard<RecursiveMutex> lock(_lock);
    if (-1 == _fdRead) {
        return false;
    }
    volatile auto offset = lseek(_fdRead, 0, SEEK_CUR);  // TODO: remove me
    QueueItemHeader item = {};
    auto rret = read(_fdRead, &item, sizeof(item));
    // Check for end of file
    if (rret < (int)sizeof(item)) {
        closeFile(_fdRead);  // TODO do this somewhere else
        return false;
    }

    // Check for correct magic
    if (QueueItemMagic != item.magic) {
        closeFile(_fdRead);  // TODO do this somewhere else
        return false;
    }
    // Check for active entry
    if (item.flags & ItemFlagActive) {
    }

    rret = read(_fdRead, data, item.length);
    // Check for end of file
    if (rret < (int)item.length) {
        closeFile(_fdRead);  // TODO do this somewhere else
        return false;
    }

    // Rewind to header
    lseek(_fdRead, -(sizeof(item) + item.length), SEEK_CUR);
    size = (size_t)item.length;

    return true;
}

int DiskQueue::getNextWriteFile() {
    // Close the current file if open
    closeFile(_fdWrite);

    unsigned long fileN = 0;
    bool newWriteFile = false;

    if (_fileList.isEmpty()) {
        newWriteFile = true;
    } else {
        fileN = _fileList.last()->n;
    }

    String filename = _path + String(fileN);
    struct stat st = {};
    if (!stat(filename, &st)) {
        if ((size_t)st.st_size >= _capacity) {
            filename = _path + String(++fileN);
            newWriteFile = true;
        }
    }

    _fdWrite = open(filename.c_str(), O_CREAT | O_RDWR | O_APPEND, 0664);
    // TODO: Check _fdWrite
    if (_fdWrite < 0) {
        return -1; // TODO: choose another
    }
    if (newWriteFile) {
        _feWrite = addFileNode(fileN, 0, String(fileN));
        QueueFileHeader fHeader = {QueueFileMagic, 0x01, 0x00};
        write(_fdWrite, &fHeader, sizeof(fHeader));
    } else {
        _feWrite = _fileList.last();
    }

    return SYSTEM_ERROR_NONE;
}

int DiskQueue::getNextReadFile() {
    int ret = SYSTEM_ERROR_NONE;

    // Close anything that may be open
    closeFile(_fdRead);

    do {
        // Figure out which file is the first one to start reading from.  It is normally the first numerical filename.
        unsigned long fileN = _fileList.first()->n;
        String filename = _path + String(fileN);

        _fdRead = open(filename.c_str(), O_RDWR, 0664);
        if (_fdRead < 0) {
            if (_feWrite != _feRead) {
                unlink(filename.c_str());
                _fileList.takeFirst();
            }
            continue;
        }

        // Get the file header
        QueueFileHeader fHeader = {};
        auto rret = read(_fdRead, &fHeader, sizeof(fHeader));
        if (rret < (int)sizeof(fHeader)) {
            ret = SYSTEM_ERROR_IO;
            break;
        }

        while (true) {
            volatile auto offset = lseek(_fdRead, 0, SEEK_CUR);  // TODO: remove me
    //            if (offset <= st.st_size) {
    //                break;
    //            }
            QueueItemHeader itemHeader = {};
            rret = read(_fdRead, &itemHeader, sizeof(itemHeader));
            // Check for end of file
            if (rret < (int)sizeof(itemHeader)) {
                break;
            }
            // Check for correct magic
            if (QueueItemMagic != itemHeader.magic) {
                break;
            }
            // Check for active entry
            if (itemHeader.flags & ItemFlagActive) {
                // Rewind to header
                lseek(_fdRead, -sizeof(itemHeader), SEEK_CUR);
                break;
            }
            // Advance to next entry
            lseek(_fdRead, itemHeader.length, SEEK_CUR);
        }
    } while (true);

    return ret;
}

void DiskQueue::closeFile(int& fd) {
    // Simply close the file.  Sync the file at the caller and not here.
    if (-1 != fd) {
        close(fd);
        fd = -1;
    }
}

bool DiskQueue::push_back(const void* data, size_t size) {
    CHECK_TRUE(_running, false);

    const std::lock_guard<RecursiveMutex> lock(_lock);

    if ((_fdWrite <= 0) || !_feWrite) {
        return false;
    }

    auto fileSize = lseek(_fdWrite, 0, SEEK_CUR);

    if ((size_t)fileSize >= _capacity) {
        if (getNextWriteFile()) {
            return false; // TODO: choose another
        }
    }

    if (size == 0) {
        size = strlen((char*)data);
    }
    QueueItemHeader itemHeader = { QueueItemMagic, ItemFlagActive, (uint16_t)size }; // TODO: fix length
    auto written = write(_fdWrite, &itemHeader, sizeof(itemHeader));
    written += write(_fdWrite, data, size);
    (void)written;

    fsync(_fdWrite);

    return true;
}

/**
 * @brief Get list of file numbers that represent disk queue data filenames.
 *
 * @return Vector<unsigned long> An array of the file numbers.
 */
Vector<unsigned long> DiskQueue::list() {
    Vector<unsigned long> fileList;

    for (auto item = _fileList.begin();item != _fileList.end();++item) {
        fileList.append((*item)->n);
    }

    return fileList;
}

bool DiskQueue::dirExists(const char* path) {
    struct stat st = {};
    if (!stat(path, &st)) {
        if (S_ISDIR(st.st_mode)) {
            return true;
        }
    }

    return false;
}

int DiskQueue::sortPartition(Vector<FileEntry*>& array, int begin, int end) {
    FileEntry* last = array[end];
    int pivot = (begin - 1);

    for (int i = begin; i <= end - 1; ++i) {
        if (array[i]->n <= last->n) {
            std::swap<FileEntry*>(array[++pivot], array[i]);
        }
    }
    std::swap<FileEntry*>(array[pivot + 1], array[end]);

    return (pivot + 1);
}

void DiskQueue::quickSortFiles(Vector<FileEntry*>& array, int begin, int end)
{
    if (array.isEmpty()) {
        // Done
        return;
    }

    Vector<int> stack(end - begin + 1, 0);

    int top = -1;

    stack[++top] = begin;
    stack[++top] = end;

    while (top >= 0) {
        end = stack[top--];
        begin = stack[top--];

        int pivot = sortPartition(array, begin, end);

        if (pivot - 1 > begin) {
            stack[++top] = begin;
            stack[++top] = pivot - 1;
        }

        if (pivot + 1 < end) {
            stack[++top] = pivot + 1;
            stack[++top] = end;
        }
    }
}

void DiskQueue::cleanupFiles() {
    if (!_fileList.isEmpty()) {
        for (auto item = _fileList.begin();item != _fileList.end();++item) {
            removeFileNode(*item);
        }
    }

    _fileList.clear();

    closeFile(_fdWrite);
    closeFile(_fdRead);
}

void DiskQueue::cleanup() {
}

DiskQueue::FileEntry* DiskQueue::addFileNode(unsigned long n, size_t size, const char* name, bool append) {
    FileEntry* entry = new FileEntry;
    if (!entry) {
        return nullptr;
    }
    entry->n = n;
    entry->size = size;
    entry->entry = String(name);

    if (append) {
        _fileList.append(entry);
    }

    return entry;
}

void DiskQueue::removeFileNode(FileEntry* entry) {
    delete entry;
}

void DiskQueue::removeFileNode(int index) {
    if ((index >= 0) && (index < _fileList.size())) {
        auto entry = _fileList.at(index);
        removeFileNode(entry);
        _fileList.removeAt(index);
    }
}

int DiskQueue::getFilenames(const char* path) {
    auto dir = opendir(path);
    if (!dir) {
        return SYSTEM_ERROR_NOT_FOUND;
    }

    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type != DT_REG) {
            continue;
        }

        struct stat st = {};
        stat(ent->d_name, &st);
        char* stop = nullptr;
        unsigned long n = strtoul(ent->d_name, &stop, 10);
        if (strlen(ent->d_name) == (size_t)(stop - ent->d_name)) {
            FileEntry* entry = addFileNode(n, st.st_size, ent->d_name);
            CHECK_TRUE(entry, SYSTEM_ERROR_NO_MEMORY);
        }
    }

    quickSortFiles(_fileList, 0, _fileList.size() - 1);

    closedir(dir);

    return SYSTEM_ERROR_NONE;
}

