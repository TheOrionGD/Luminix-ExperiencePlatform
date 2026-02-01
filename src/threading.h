#ifndef THREADING_H
#define THREADING_H

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

// Thread type
#ifdef _WIN32
typedef HANDLE thread_t;
#else
typedef pthread_t thread_t;
#endif

// Mutex type
#ifdef _WIN32
typedef CRITICAL_SECTION mutex_t;
#else
typedef pthread_mutex_t mutex_t;
#endif

// Simple thread creation
static inline int thread_create(thread_t* thread, void* (*func)(void*), void* arg) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    return *thread ? 0 : -1;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

// Mutex functions
static inline int mutex_init(mutex_t* m) {
#ifdef _WIN32
    InitializeCriticalSection(m);
    return 0;
#else
    return pthread_mutex_init(m, NULL);
#endif
}

static inline int mutex_lock(mutex_t* m) {
#ifdef _WIN32
    EnterCriticalSection(m);
    return 0;
#else
    return pthread_mutex_lock(m);
#endif
}

static inline int mutex_unlock(mutex_t* m) {
#ifdef _WIN32
    LeaveCriticalSection(m);
    return 0;
#else
    return pthread_mutex_unlock(m);
#endif
}

static inline int mutex_destroy(mutex_t* m) {
#ifdef _WIN32
    DeleteCriticalSection(m);
    return 0;
#else
    return pthread_mutex_destroy(m);
#endif
}

#endif // THREADING_H
