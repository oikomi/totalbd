

#ifndef TRACE_TIME_H
#define TRACE_TIME_H

#include <sys/resource.h>
#include <sys/time.h>

#define PT_USEC_PER_SEC 1000000l

static inline long pt_time_sec()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (long) tv.tv_sec;
}

static inline long pt_time_usec()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (long) tv.tv_sec * PT_USEC_PER_SEC + (long) tv.tv_usec;
}

static inline long pt_cputime_usec()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return (long) usage.ru_utime.tv_sec * PT_USEC_PER_SEC +
           (long) usage.ru_utime.tv_usec +
           (long) usage.ru_stime.tv_sec * PT_USEC_PER_SEC +
           (long) usage.ru_stime.tv_usec;
}

#endif
