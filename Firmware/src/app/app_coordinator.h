#ifndef APP_COORDINATOR_H
#define APP_COORDINATOR_H

#include "app_events.h"
#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_coordinator_init(void);
void app_coordinator_tick(void);
void app_coordinator_post_event(const app_event_t *event);
const app_state_t *app_coordinator_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_COORDINATOR_H */
