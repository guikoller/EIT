#ifndef HOME_PRESENTER_H
#define HOME_PRESENTER_H

#include "home_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    home_view_t *view;
} home_presenter_t;

void home_presenter_init(home_presenter_t *p, home_view_t *v);
void home_presenter_on_start(void *ctx);
void home_presenter_on_settings(void *ctx);
void home_presenter_on_about(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* HOME_PRESENTER_H */
