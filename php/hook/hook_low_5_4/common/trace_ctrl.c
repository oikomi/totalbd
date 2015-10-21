

#include <string.h>
#include "trace_ctrl.h"

int pt_ctrl_open(pt_ctrl_t *ctrl, const char *file)
{
    return pt_mmap_open(&ctrl->seg, file, PT_CTRL_SIZE);
}

int pt_ctrl_create(pt_ctrl_t *ctrl, const char *file)
{
    return pt_mmap_create(&ctrl->seg, file, PT_CTRL_SIZE);
}

int pt_ctrl_close(pt_ctrl_t *ctrl)
{
    return pt_mmap_close(&ctrl->seg);
}

void pt_ctrl_clean_all(pt_ctrl_t *ctrl)
{
    memset(ctrl->seg.addr, 0x00, ctrl->seg.size);
}
