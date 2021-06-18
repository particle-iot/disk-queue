//============================================================================
// Name        : diskqueue.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <ctime>
#include <atomic>
#include <pthread.h>

// Need these
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <vector>

#include "DiskQueue.h"

using namespace std;

constexpr char chroniclePath[] = "/tmp/chron";

void listdir() {
    auto dir = opendir(chroniclePath);
    if (!dir) {
        fprintf(stderr, "opendir on '%s' failed\n", chroniclePath);
        return;
    }

    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type != DT_REG) {
            continue;
        }

        char* stop = nullptr;
        unsigned long n = strtoul(ent->d_name, &stop, 10);
        if (strlen(ent->d_name) == (size_t)(stop - ent->d_name)) {
            printf("File %lu '%s' is good\n", n, ent->d_name);
        } else {
            printf("File %lu '%s' is not good\n", n, ent->d_name);
        }
    }
    //sort(files.begin(), files.end(), )

    closedir(dir);
}

struct LogData {
    uint8_t flags;
    uint8_t data[];
};

DiskQueue theQueue(500, 200);

int main() {
    cout << "Size of struct " << sizeof(LogData) << endl;
    listdir();

    auto ret = theQueue.start(chroniclePath);
    if (ret) {
        cerr << "Can't start DiskQueue" << endl;
        exit(-1);
    }
#if 1
    theQueue.push_back((uint8_t*)"Hello there 1", 0);
    theQueue.push_back((uint8_t*)"Hello here 2", 12);
    theQueue.push_back((uint8_t*)"Hello where 3", 13);
    theQueue.push_back((uint8_t*)"Hello where 4", 13);
    theQueue.push_back((uint8_t*)"Hello where 5", 13);
    theQueue.push_back((uint8_t*)"Hello where 6", 13);
    theQueue.push_back((uint8_t*)"Hello where 7", 13);
    theQueue.push_back((uint8_t*)"Hello where 8", 13);
    theQueue.push_back((uint8_t*)"Hello where 9", 13);
    theQueue.push_back((uint8_t*)"Hello where 10", 14);
    theQueue.push_back((uint8_t*)"Hello where 11", 14);
    theQueue.push_back((uint8_t*)"Hello where 12", 14);
    theQueue.push_back((uint8_t*)"Hello where 13", 14);
    theQueue.push_back((uint8_t*)"Hello where 14", 14);
    theQueue.push_back((uint8_t*)"Hello where 15", 14);
    theQueue.push_back((uint8_t*)"Hello where 16", 14);
    theQueue.push_back((uint8_t*)"Hello where 17", 14);
    theQueue.push_back((uint8_t*)"Hello where 18", 14);
    theQueue.push_back((uint8_t*)"Hello where 19", 14);
#else
    char buf[1024] = {};
    size_t s = 0;
    bool peekret;
    do {
        auto size = theQueue.peek_front_size();
        peekret = theQueue.peek_front((uint8_t*)buf, s);
        if (peekret) theQueue.pop_front();
        else break;
        printf("Peeked: %d/%lu/%lu: %s\n", (int)peekret, size, s, buf);
        memset(buf, 0, sizeof(buf));
    } while (peekret);
#endif

    auto l = theQueue.list();
    for (auto i : l) {
        printf("List file %lu\n", i);
    }

    theQueue.stop();

    exit(0);
}
