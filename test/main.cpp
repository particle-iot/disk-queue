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
#include <thread>
#include <signal.h>

// Need these
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <vector>

#include "DiskQueue.h"
#include "readerwriterqueue.h"

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

constexpr size_t MaxTestElements = 10000;

enum ProducerCommands {
    ProducerStart,
    ProducerNone,
    ProducerPause,
    ProducerKill,
};

atomic<ProducerCommands> producerCommand(ProducerNone);
size_t producerCount = 0;

enum ConsumerCommands {
    ConsumerStart,
    ConsumerNone,
    ConsumerPause,
    ConsumerKill,
};

atomic<ConsumerCommands> consumerCommand(ConsumerNone);
size_t consumerCount = 0;

DiskQueue theQueue(500);
moodycamel::ReaderWriterQueue<unsigned int> q(MaxTestElements);

void producerThread() {
    bool run = false;

    while (true) {
        auto c = producerCommand.exchange(ProducerNone, memory_order_relaxed);
        switch (c) {
            case ProducerNone:
                break;
            case ProducerStart:
                run = true;
                break;
            case ProducerPause:
                run = false;
                break;
            case ProducerKill:
                return;
        }

        if (run) {
            producerCount++;
        }
    }
}

void consumerThread() {
    bool run = false;

    while (true) {
        auto c = consumerCommand.exchange(ConsumerNone, memory_order_relaxed);
        switch (c) {
            case ConsumerNone:
                break;
            case ConsumerStart:
                run = true;
                break;
            case ConsumerPause:
                run = false;
                break;
            case ConsumerKill:
                return;
        }

        if (run) {
            consumerCount++;
        }
    }
}

void sigintHandler(int number) {
    producerCommand.store(ProducerKill, memory_order_relaxed);
    consumerCommand.store(ConsumerKill, memory_order_relaxed);
    theQueue.stop();
}

int main() {
    signal(SIGINT, sigintHandler);

    //listdir();

    auto ret = theQueue.start(chroniclePath, DiskQueuePolicy::FifoDeleteOld);
    if (ret) {
        cerr << "Can't start DiskQueue" << endl;
        exit(-1);
    }

    thread producer(producerThread);
    thread consumer(consumerThread);

#if 1
//    theQueue.pushBack((uint8_t*)"Hello there 1", 0);
//    theQueue.pushBack((uint8_t*)"Hello here 2", 12);
//    theQueue.pushBack((uint8_t*)"Hello where 3", 13);
//    theQueue.pushBack((uint8_t*)"Hello where 4", 13);
//    theQueue.pushBack((uint8_t*)"Hello where 5", 13);
//    theQueue.pushBack((uint8_t*)"Hello where 6", 13);
//    theQueue.pushBack((uint8_t*)"Hello where 7", 13);
//    theQueue.pushBack((uint8_t*)"Hello where 8", 13);
//    theQueue.pushBack((uint8_t*)"Hello where 9", 13);
//    theQueue.pushBack((uint8_t*)"Hello where 10", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 11", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 12", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 13", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 14", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 15", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 16", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 17", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 18", 14);
//    theQueue.pushBack((uint8_t*)"Hello where 19", 14);

    int method = 0;
    for (int i = 0;i < 100;i++) {
        char b[64] = {};
        auto s = snprintf(b, sizeof(b), "Hello where %d", i);
        switch (method) {
        case 0: theQueue.pushBack((uint8_t*)b, (size_t)s); method++; break;

        case 1: theQueue.pushBack(b); method++; break;

        case 2: theQueue.pushBack(String(b));
        // Fall through
        default:
            method = 0;
        }
    }
#else
    char buf[1024] = {};
    bool peekret;
    do {
        auto disk1 = theQueue.getCurrentDiskUsage();
        auto size = theQueue.peekFrontSize();
        size_t s = sizeof(buf);
        peekret = theQueue.peekFront((uint8_t*)buf, s);
        if (peekret) theQueue.popFront();
        else break;
        auto disk2 = theQueue.getCurrentDiskUsage();
        printf("Peeked: %lu/%lu %d/%lu/%lu: %s\n", disk1, disk2, (int)peekret, size, s, buf);
        memset(buf, 0, sizeof(buf));
    } while (peekret);
#endif

    auto l = theQueue.list();
    for (auto i : l) {
        printf("List file %lu\n", i);
    }

    producerCommand.store(ProducerKill, memory_order_relaxed);
    consumerCommand.store(ConsumerKill, memory_order_relaxed);

    producer.join();
    consumer.join();
    printf("Producer: %lu\nConsumer: %lu\n", producerCount, consumerCount);

    theQueue.stop();

    exit(0);
}
