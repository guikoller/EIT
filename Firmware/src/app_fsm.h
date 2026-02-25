#ifndef APP_FSM_H
#define APP_FSM_H

#include "app_events.h"
#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_fsm_init(app_state_t *state);
void app_fsm_dispatch(app_state_t *state, const app_event_t *event);
void app_fsm_tick(app_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* APP_FSM_H */
