#ifndef SRV_HOSTCHECK_HOST_CHECK_H
#define SRV_HOSTCHECK_HOST_CHECK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     completed;
    bool     success;
    uint16_t httpStatus;
    uint32_t checkedAtEpoch;
} HostCheck_Result;

/** Initialize scheduling and run the first check on the next poll. */
void HostCheck_Init(void);

/** Run the check when its interval expires. */
void HostCheck_Poll(void);

/** Copy the latest completed (or initial pending) result. */
void HostCheck_GetLatest(HostCheck_Result * resultOut);

#endif /* SRV_HOSTCHECK_HOST_CHECK_H */
