#include "eit_acquisition.h"

#include <stddef.h>

static eit_acq_backend_t *s_backend = NULL;

void eit_acquisition_init(eit_acq_backend_t *backend)
{
    s_backend = backend;
    if (s_backend && s_backend->ops && s_backend->ops->init) {
        s_backend->ops->init(s_backend);
    }
}

void eit_acquisition_deinit(void)
{
    if (s_backend && s_backend->ops && s_backend->ops->deinit) {
        s_backend->ops->deinit(s_backend);
    }
    s_backend = NULL;
}

int eit_acquisition_start_frame(void)
{
    if (!s_backend || !s_backend->ops || !s_backend->ops->start_frame) return 0;
    return s_backend->ops->start_frame(s_backend);
}

eit_acq_status_t eit_acquisition_poll(void)
{
    eit_acq_status_t st = { EIT_ACQ_ERROR, 0, 0 };
    if (!s_backend || !s_backend->ops || !s_backend->ops->poll) return st;
    return s_backend->ops->poll(s_backend);
}

int eit_acquisition_get_frame(eit_frame_t *out)
{
    if (!s_backend || !s_backend->ops || !s_backend->ops->get_frame || !out) return 0;
    return s_backend->ops->get_frame(s_backend, out);
}

int eit_acquisition_get_ref_frame(eit_frame_t *out)
{
    if (!s_backend || !s_backend->ops || !s_backend->ops->get_ref_frame || !out) return 0;
    return s_backend->ops->get_ref_frame(s_backend, out);
}

eit_acq_backend_t *eit_acquisition_get_backend(void)
{
    return s_backend;
}
