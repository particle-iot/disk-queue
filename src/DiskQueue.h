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

#pragma once

#include "Particle.h"

/**
 * @brief The <code>DiskQueue</code> class represents a disk-based queue that
 *
 */

class DiskQueue {

public:
    /**
     * @brief Construct a new DiskQueue object with the given file capacity and disk limit
     *
     * @param[in]   capacity        Size of read and write cache queues
     * @param[in]   diskLimit       Count of elements in cache queue before writing to disk
     */
    DiskQueue(size_t capacity, size_t diskLimit)
    : _offset(0),
      _fdWrite(-1),
      _fdRead(-1),
      _feWrite(nullptr),
      _feRead(nullptr),
      _capacity(capacity),
      _diskLimit(diskLimit),
      _running(false) {

    }

    /**
     * @brief Destroy the DiskQueue object
     *
     */
    ~DiskQueue() {
        cleanupFiles();
        cleanup();
    }

    /**
     * @brief Start the disk queue.  This creates the cache queues if they haven't already been
     * created.  The path is checked to exist and created if nonexistant.  This will not create
     * intermediate directories above the path if nested.
     *
     * @param[in]   path            Full directory path for storing the queue on the file system, eg `/usr/my_queue`
     * @retval SYSTEM_ERROR_NONE
     * @retval SYSTEM_ERROR_NO_MEMORY
     * @retval SYSTEM_ERROR_FILE
     */
    int start(const char* path);

    /**
     * @brief Stop the disk queue.  Flush both read and write queues to disk in anticipation
     * of service disruption.
     *
     * @retval SYSTEM_ERROR_NONE
     */
    int stop();

    /**
     * @brief Take item from read queue if available.
     */
    void pop_front();

    /**
     * @brief Inspect item from read queue if available.
     *
     * @param[out]     data     Where to store the data
     * @param[out]     size     [in] maximum size available for copying, [out] size written
     * @return true Item has been taken and is in output object
     * @return false No item is available
     */
    bool front(void* data, size_t& size);

    /**
     * @brief Push item to write queue if space available
     *
     * @param[in]      data     Where to copy data from
     * @param[in]      size     size of the input data
     * @return true Item has been pushed
     * @return false Item has not been pushed
     */

    bool push_back(const void* data, size_t size);

    /**
     * @brief Get list of file numbers that represent disk queue data filenames.
     *
     * @return Vector<unsigned long> An array of the file numbers.
     */
    Vector<unsigned long> list();

private:
    static constexpr uint8_t QueueFileMagic = 'P';          //< Magic number that must be present at the beginning of each queue file
    static constexpr uint8_t FileFlagReverse = (1 << 0);    //< Flag to indicate that the queue is to be popped in reverse order

    static constexpr uint8_t QueueItemMagic = 0xf0;         //< Magic number that must be present at the beginning of each queue item
    static constexpr uint8_t ItemFlagActive = (1 << 0);     //< Flag to indicate that the queue item is still active

#pragma pack(push,1)
    struct QueueFileHeader {
        uint8_t magic;          //< Magic number must be 'P'
        uint8_t version;        //< Version of the header structures in this file
        uint8_t flags;          //< Various file-wide flags
    };
    struct QueueItemHeader {
        uint8_t magic;          //< Magic number must be 0xf0
        uint8_t flags;          //< Various item specific flags
        uint16_t length;        //< Length of data immediately following this structure
    };
#pragma pack(pop)

    /**
     * @brief A structure containing the disk based file numbers and filenames.
     *
     */
    struct FileEntry {
        unsigned long n;
        int fd;
        size_t size;
        String entry;
    };

    /**
     * @brief Detect if path exists.
     *
     * @param[in]   path            Full path of file or directory
     * @return true File or directory exists
     * @return false File or directory doesn't exist
     */
    bool dirExists(const char* path);

    /**
     * @brief Quicksort partition for file numbers.
     *
     * @param[in,out]   array       Array of file numbers to partition
     * @param[in]       begin       First index to consider (inclusive)
     * @param[in]       end         Last index to consider (inclusive)
     * @return int The pivot of the partition.
     */
    int sortPartition(Vector<FileEntry*>& array, int begin, int end);

    /**
     * @brief Quicksort algorithm for file numbers.
     *
     * @param[in,out]   array       Array of file numbers to sort
     * @param[in]       begin       First index to consider (inclusive)
     * @param[in]       end         Last index to consider (inclusive)
     */
    void quickSortFiles(Vector<FileEntry*>& array, int begin, int end);

    /**
     * @brief Destroy file number array.
     *
     */
    void cleanupFiles();

    /**
     * @brief Destroy read and write cache queues.
     *
     */
    void cleanup();

    /**
     * @brief Create (allocate) FileEntry object and initialize it with given
     * file number and file name.  Optionally append it to file list.
     *
     * @param[in]   n               File number.
     * @param[in]   name            File name associated with file number.
     * @param[in]   append          Append to end of file list.  True to append.  False to no append.
     * @return FileEntry* Allocated FileEntry object pointer.  nullptr if unsuccessful.
     */
    FileEntry* addFileNode(unsigned long n, size_t size, const char* name, bool append = true);

    /**
     * @brief Destroy FileEntry object.
     *
     * @param[in]   entry           FileEntry object to delete.
     */
    void removeFileNode(FileEntry* entry);

    /**
     * @brief Remove and destroy FileEntry object from file list.
     *
     * @param[in]   index           Index into the file list.
     */
    void removeFileNode(int index);

    /**
     * @brief Get the next write file for the queue
     */
    int getNextWriteFile();

    /**
     * @brief Get the next read file for the queue
     */
    int getNextReadFile();

    /**
     * @brief Close the given file and mark as closed
     */
    void closeFile(int& fd);

    /**
     * @brief Get the file numbers associated under the given path.
     *
     * @param[in]   path            Full path for search
     * @retval SYSTEM_ERROR_NONE
     * @retval SYSTEM_ERROR_NOT_FOUND
     * @retval SYSTEM_ERROR_NO_MEMORY
     */
    int getFilenames(const char* path);

    RecursiveMutex _lock;
    off_t _offset;
    int _fdWrite;
    QueueFileHeader _writeHeader;
    FileEntry* _feWrite;

    int _fdRead;
    QueueFileHeader _readHeader;
    FileEntry* _feRead;

    Vector<FileEntry*> _fileList;
    size_t _capacity;
    size_t _diskLimit;
    String _path;
    bool _running;
};
