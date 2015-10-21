

#ifndef TRACE_MMAP_H
#define TRACE_MMAP_H

#include <sys/mman.h>

/* Some operating systems (like FreeBSD) have a MAP_NOSYNC flag that
 * tells whatever update daemons might be running to not flush dirty
 * vm pages to disk unless absolutely necessary.  My guess is that
 * most systems that don't have this probably default to only synching
 * to disk when absolutely necessary. */
#ifndef MAP_NOSYNC
#define MAP_NOSYNC 0
#endif

/* support for systems where MAP_ANONYMOUS is defined but not MAP_ANON, ie:
 * HP-UX bug #14615 */
#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
# define MAP_ANON MAP_ANONYMOUS
#endif

typedef struct {
    size_t size;
    void *addr;
} pt_segment_t;

/* Return value: on succes returns 0, on failure -1 */
int pt_mmap_open_fd(pt_segment_t *seg, int fd, size_t size);
int pt_mmap_open(pt_segment_t *seg, const char *file, size_t size);
int pt_mmap_create(pt_segment_t *seg, const char *file, size_t size);
int pt_mmap_close(pt_segment_t *seg);

#endif
