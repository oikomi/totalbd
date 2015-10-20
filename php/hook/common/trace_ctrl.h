

#ifndef TRACE_CTRL_H
#define TRACE_CTRL_H

#include <stdint.h>
#include "trace_mmap.h"

/* We use PT_PID_MAX + 1 as the size of file ctrl file. 4 million is the hard
 * limit of linux kernel so far, and it is 99999 on Mac OS X which is coverd by
 * this value. So 4*1024*1024 can serve both linux and unix(include darwin). */
#define PT_PID_MAX          4194304
#define PT_CTRL_SIZE        PT_PID_MAX + 1
#define PT_CTRL_FILENAME    "phptrace.ctrl"
#define PT_CTRL_ACTIVE      0x01

typedef struct {
    pt_segment_t seg;
} pt_ctrl_t;

int pt_ctrl_open(pt_ctrl_t *ctrl, const char *file);
int pt_ctrl_create(pt_ctrl_t *ctrl, const char *file);
int pt_ctrl_close(pt_ctrl_t *ctrl);
void pt_ctrl_clean_all(pt_ctrl_t *ctrl);

#define pt_ctrl_pid(ctrl, pid)              (*(uint8_t *) (((ctrl)->seg.addr) + (pid)))
#define pt_ctrl_is_active(ctrl, pid)        (pt_ctrl_pid(ctrl, pid) &   PT_CTRL_ACTIVE)
#define pt_ctrl_set_active(ctrl, pid)       (pt_ctrl_pid(ctrl, pid) |=  PT_CTRL_ACTIVE)
#define pt_ctrl_set_inactive(ctrl, pid)     (pt_ctrl_pid(ctrl, pid) &= ~PT_CTRL_ACTIVE)

#endif
