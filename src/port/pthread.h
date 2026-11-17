/*
 * pthread.h for libdvdnav in dvdnav.library: its mutexes (one per
 * handle, one in the read cache) are Exec signal semaphores. They nest
 * within a task, as libdvdnav needs nothing more.
 *
 * On the host (tests/dvd_host.c) the same calls do nothing: the tests use
 * one thread.
 * Part of dvdnav.library: GPL-2.0-or-later (see src/dvdlib/README).
 */
#ifndef DVDNAV_PTHREAD_H
#define DVDNAV_PTHREAD_H
#ifdef __amigaos__
#include <exec/semaphores.h>
#include <proto/exec.h>
typedef struct SignalSemaphore dvdp_mutex_t;
#define pthread_mutex_init(m, a)  InitSemaphore(m)
#define pthread_mutex_destroy(m)  ((void)(m))
#define pthread_mutex_lock(m)     ObtainSemaphore(m)
#define pthread_mutex_unlock(m)   ReleaseSemaphore(m)
#else
typedef struct { int depth; } dvdp_mutex_t;
#define pthread_mutex_init(m, a)  ((void)((m)->depth = 0))
#define pthread_mutex_destroy(m)  ((void)(m))
#define pthread_mutex_lock(m)     ((void)++(m)->depth)
#define pthread_mutex_unlock(m)   ((void)--(m)->depth)
#endif
/* the system's sys/types.h may have its own pthread_mutex_t */
#define pthread_mutex_t dvdp_mutex_t
#endif
