#ifndef SRV_HEALTHAPI_HEALTH_API_H
#define SRV_HEALTHAPI_HEALTH_API_H

#include <stdbool.h>
#include <stdint.h>

/** Start the HTTP health API on the requested TCP port. */
bool HealthApi_Start(uint16_t port);

/** Poll for and service one HTTP client without blocking. */
void HealthApi_Poll(void);

#endif /* SRV_HEALTHAPI_HEALTH_API_H */
