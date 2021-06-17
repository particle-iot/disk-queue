/*
 * Particle.h
 *
 *  Created on: May 13, 2020
 *      Author: eric
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <mutex>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

// List of all defined system errors
#define SYSTEM_ERROR_NONE                   (0)
#define SYSTEM_ERROR_UNKNOWN                (-100)
#define SYSTEM_ERROR_BUSY                   (-110)
#define SYSTEM_ERROR_NOT_SUPPORTED          (-120)
#define SYSTEM_ERROR_NOT_ALLOWED            (-130)
#define SYSTEM_ERROR_CANCELLED              (-140)
#define SYSTEM_ERROR_ABORTED                (-150)
#define SYSTEM_ERROR_TIMEOUT                (-160)
#define SYSTEM_ERROR_NOT_FOUND              (-170)
#define SYSTEM_ERROR_ALREADY_EXISTS         (-180)
#define SYSTEM_ERROR_TOO_LARGE              (-190)
#define SYSTEM_ERROR_NOT_ENOUGH_DATA        (-191)
#define SYSTEM_ERROR_LIMIT_EXCEEDED         (-200)
#define SYSTEM_ERROR_END_OF_STREAM          (-201)
#define SYSTEM_ERROR_INVALID_STATE          (-210)
#define SYSTEM_ERROR_IO                     (-220)
#define SYSTEM_ERROR_WOULD_BLOCK            (-221)
#define SYSTEM_ERROR_FILE                   (-225)
#define SYSTEM_ERROR_NETWORK                (-230)
#define SYSTEM_ERROR_PROTOCOL               (-240)
#define SYSTEM_ERROR_INTERNAL               (-250)
#define SYSTEM_ERROR_NO_MEMORY              (-260)
#define SYSTEM_ERROR_INVALID_ARGUMENT       (-270)
#define SYSTEM_ERROR_BAD_DATA               (-280)
#define SYSTEM_ERROR_OUT_OF_RANGE           (-290)
#define SYSTEM_ERROR_DEPRECATED             (-300)
#define SYSTEM_ERROR_COAP                   (-1000)
#define SYSTEM_ERROR_COAP_4XX               (-1100)
#define SYSTEM_ERROR_COAP_5XX               (-1132)
#define SYSTEM_ERROR_AT_NOT_OK              (-1200)
#define SYSTEM_ERROR_AT_RESPONSE_UNEXPECTED (-1210)

/**
 * Check the result of an expression.
 *
 * @see @ref check_macros
 */
#define CHECK(_expr) \
        ({ \
            const auto _ret = _expr; \
            if (_ret < 0) { \
                return _ret; \
            } \
            _ret; \
        })

/**
 * Check the result of a predicate expression.
 *
 * @see @ref check_macros
 */
#define CHECK_TRUE(_expr, _ret) \
        do { \
            const bool _ok = (bool)(_expr); \
            if (!_ok) { \
                return _ret; \
            } \
        } while (false)

/**
 * Check the result of a predicate expression.
 *
 * @see @ref check_macros
 */
#define CHECK_FALSE(_expr, _ret) \
        CHECK_TRUE(!(_expr), _ret)

typedef uint32_t system_tick_t;

struct os_queue_s {
    void* thing;
};
typedef os_queue_s* os_queue_t;

static inline int os_queue_create(os_queue_t* q, size_t size, size_t nelems, void* context) {
    return 0;
}

static inline int os_queue_destroy(os_queue_t q, void* context) {
    return 0;
}

static inline int os_queue_take(os_queue_t q, void* item, system_tick_t delay, void* context) {
    return 1;
}

static inline int os_queue_put(os_queue_t q, void* item, system_tick_t delay, void* context) {
    return 1;
}


#define WITH_LOCK(lock) for (bool __todo = true; __todo;) for (std::lock_guard<decltype(lock)> __lock((lock)); __todo; __todo=0)
#define TRY_LOCK(lock) for (bool __todo = true; __todo; ) for (std::unique_lock<typename std::remove_reference<decltype(lock)>::type> __lock##lock((lock), std::try_to_lock); __todo &= bool(__lock##lock); __todo=0)


#include "spark_wiring_vector.h"
#include "spark_wiring_string.h"

using namespace spark;
using RecursiveMutex = std::recursive_mutex;
