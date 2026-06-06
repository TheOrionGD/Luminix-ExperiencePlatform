#ifndef PTHREAD_H_WRAPPER
#define PTHREAD_H_WRAPPER

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>

// Mutexes (Using SRWLOCK for static initialization support)
typedef SRWLOCK pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
static inline int pthread_mutex_init(pthread_mutex_t *m, void *attr) { InitializeSRWLock(m); return 0; }
static inline int pthread_mutex_lock(pthread_mutex_t *m) { AcquireSRWLockExclusive(m); return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { ReleaseSRWLockExclusive(m); return 0; }
static inline int pthread_mutex_destroy(pthread_mutex_t *m) { return 0; }

// RWLocks (Mapped to SRWLOCK in exclusive mode for simplicity since unlock has no state in pthreads)
typedef SRWLOCK pthread_rwlock_t;
#define PTHREAD_RWLOCK_INITIALIZER SRWLOCK_INIT
static inline int pthread_rwlock_init(pthread_rwlock_t *rw, void *attr) { InitializeSRWLock(rw); return 0; }
static inline int pthread_rwlock_rdlock(pthread_rwlock_t *rw) { AcquireSRWLockExclusive(rw); return 0; }
static inline int pthread_rwlock_wrlock(pthread_rwlock_t *rw) { AcquireSRWLockExclusive(rw); return 0; }
static inline int pthread_rwlock_unlock(pthread_rwlock_t *rw) { ReleaseSRWLockExclusive(rw); return 0; }
static inline int pthread_rwlock_destroy(pthread_rwlock_t *rw) { return 0; }

// Threads
typedef HANDLE pthread_t;
static inline int pthread_create(pthread_t *t, void *attr, void *(*func)(void*), void *arg) {
    *t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(uintptr_t)func, arg, 0, NULL);
    return *t ? 0 : -1;
}

#endif // _WIN32
#endif // PTHREAD_H_WRAPPER
