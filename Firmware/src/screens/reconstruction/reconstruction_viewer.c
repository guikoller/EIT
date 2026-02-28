#include "reconstruction_viewer.h"

#include "reconstruction_viewer_presenter.h"
#include "reconstruction_viewer_view.h"

#include <string.h>

static reconstruction_viewer_view_t s_view;
static reconstruction_viewer_presenter_t s_presenter;

void reconstruction_viewer_create(const char *filename)
{
    reconstruction_view_bindings_t bindings;
    memset(&bindings, 0, sizeof(bindings));

    reconstruction_viewer_presenter_init(&s_presenter, &s_view);

    bindings.ctx = &s_presenter;
    bindings.on_return = reconstruction_viewer_presenter_on_return;
    bindings.on_save = reconstruction_viewer_presenter_on_save;
    bindings.on_play_pause = reconstruction_viewer_presenter_on_play_pause;

    reconstruction_viewer_view_create(&s_view, lv_screen_active(), &bindings);
    reconstruction_viewer_presenter_on_create(&s_presenter, filename);
}

/**
 * Destroy reconstruction viewer
 */
void reconstruction_viewer_destroy(void)
{
    reconstruction_viewer_presenter_on_return(&s_presenter);
}
