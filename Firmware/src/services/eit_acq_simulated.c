#include "eit_acq_simulated.h"
#include "dataset_service.h"

#include "lvgl.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Internal simulated-backend struct (embedded base)                   */
/* ------------------------------------------------------------------ */
typedef struct {
    eit_acq_backend_t base;        /**< Must be first for safe casting    */

    /* Loaded data from SD */
    float  **ref_uel_2d;           /**< dataset_service format            */
    float  **target_uel_2d;
    uint16_t n_meas;
    uint16_t n_inj;

    /* Noise injection */
    int      noise_enabled;
    int32_t  noise_level_pct;
    uint32_t rng_state;
    float   *noisy_buf;            /**< Pre-allocated [n_meas × n_inj]   */
    float    signal_rms;           /**< Pre-computed from target data     */

    /* Per-injection timing simulation */
    uint32_t inject_time_ms;       /**< Simulated time per injection step */
    uint32_t measure_time_ms;      /**< Simulated time per measure step   */
    uint32_t cycle_time_ms;        /**< inject + measure (cached)         */
    uint32_t frame_time_ms;        /**< n_inj × cycle_time_ms (cached)    */

    /* State machine */
    eit_acq_state_t state;
    uint32_t frame_start_tick;
    uint16_t current_injection;
    uint32_t frame_counter;

} eit_acq_simulated_t;

/* ------------------------------------------------------------------ */
/*  Fast xorshift32 PRNG                                               */
/* ------------------------------------------------------------------ */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float rand_pm1(uint32_t *state)
{
    uint32_t r = xorshift32(state);
    return ((float)(int32_t)r) / 2147483648.0f;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */
static void recompute_timing(eit_acq_simulated_t *sim)
{
    sim->cycle_time_ms = sim->inject_time_ms + sim->measure_time_ms;
    if (sim->cycle_time_ms > 0) {
        sim->frame_time_ms = (uint32_t)sim->n_inj * sim->cycle_time_ms;
    } else {
        sim->frame_time_ms = 0;  /* instant mode */
    }
}

static void compute_signal_rms(eit_acq_simulated_t *sim)
{
    const uint32_t total = (uint32_t)sim->n_meas * (uint32_t)sim->n_inj;
    const float *data = sim->target_uel_2d[0];
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        sum_sq += data[i] * data[i];
    }
    sim->signal_rms = sqrtf(sum_sq / (float)total);
}

/* ------------------------------------------------------------------ */
/*  Backend ops                                                        */
/* ------------------------------------------------------------------ */
static int sim_init(eit_acq_backend_t *self)
{
    (void)self;
    return 1; /* data already loaded in create */
}

static void sim_deinit(eit_acq_backend_t *self)
{
    (void)self;
    /* real cleanup is in destroy */
}

static int sim_start_frame(eit_acq_backend_t *self)
{
    eit_acq_simulated_t *sim = (eit_acq_simulated_t *)self;

    sim->frame_start_tick = lv_tick_get();
    sim->current_injection = 0;

    if (sim->frame_time_ms == 0) {
        /* Instant mode — frame ready immediately */
        sim->state = EIT_ACQ_FRAME_READY;
        sim->current_injection = sim->n_inj;
    } else {
        sim->state = EIT_ACQ_INJECTING;
    }

    return 1;
}

static eit_acq_status_t sim_poll(eit_acq_backend_t *self)
{
    eit_acq_simulated_t *sim = (eit_acq_simulated_t *)self;
    eit_acq_status_t st;
    st.total_injections = sim->n_inj;

    /* Return immediately for terminal / idle states */
    if (sim->state == EIT_ACQ_IDLE ||
        sim->state == EIT_ACQ_FRAME_READY ||
        sim->state == EIT_ACQ_ERROR) {
        st.state = sim->state;
        st.current_injection = sim->current_injection;
        return st;
    }

    /* Safety: avoid division by zero in instant mode */
    if (sim->cycle_time_ms == 0) {
        sim->state = EIT_ACQ_FRAME_READY;
        sim->current_injection = sim->n_inj;
        st.state = EIT_ACQ_FRAME_READY;
        st.current_injection = sim->n_inj;
        return st;
    }

    uint32_t elapsed = lv_tick_get() - sim->frame_start_tick;

    /* All injections complete? */
    if (elapsed >= sim->frame_time_ms) {
        sim->state = EIT_ACQ_FRAME_READY;
        sim->current_injection = sim->n_inj;
        st.state = EIT_ACQ_FRAME_READY;
        st.current_injection = sim->n_inj;
        return st;
    }

    /* Which injection are we on, and which phase within it? */
    sim->current_injection = (uint16_t)(elapsed / sim->cycle_time_ms);
    uint32_t phase = elapsed % sim->cycle_time_ms;

    if (phase < sim->inject_time_ms) {
        sim->state = EIT_ACQ_INJECTING;
    } else {
        sim->state = EIT_ACQ_MEASURING;
    }

    st.state = sim->state;
    st.current_injection = sim->current_injection;
    return st;
}

static int sim_get_frame(eit_acq_backend_t *self, eit_frame_t *out)
{
    eit_acq_simulated_t *sim = (eit_acq_simulated_t *)self;

    /* Allow first get_frame without start_frame (for initial display) */
    if (sim->state != EIT_ACQ_FRAME_READY && sim->frame_counter != 0) {
        return 0;
    }

    const uint32_t total = (uint32_t)sim->n_meas * (uint32_t)sim->n_inj;

    /* Apply noise if enabled */
    if (sim->noise_enabled && sim->noise_level_pct > 0 && sim->noisy_buf) {
        float amplitude = sim->signal_rms * (float)sim->noise_level_pct / 100.0f;
        const float *src = sim->target_uel_2d[0];
        for (uint32_t i = 0; i < total; i++) {
            sim->noisy_buf[i] = src[i] + amplitude * rand_pm1(&sim->rng_state);
        }
        out->uel = sim->noisy_buf;
    } else {
        out->uel = sim->target_uel_2d[0];
    }

    out->n_meas       = sim->n_meas;
    out->n_inj        = sim->n_inj;
    out->frame_number = sim->frame_counter;
    out->timestamp_ms = lv_tick_get();

    sim->frame_counter++;
    sim->state = EIT_ACQ_IDLE;

    return 1;
}

static int sim_get_ref_frame(eit_acq_backend_t *self, eit_frame_t *out)
{
    eit_acq_simulated_t *sim = (eit_acq_simulated_t *)self;

    out->uel          = sim->ref_uel_2d[0];
    out->n_meas       = sim->n_meas;
    out->n_inj        = sim->n_inj;
    out->frame_number = 0;
    out->timestamp_ms = lv_tick_get();

    return 1;
}

static const eit_acq_ops_t s_sim_ops = {
    .init          = sim_init,
    .deinit        = sim_deinit,
    .start_frame   = sim_start_frame,
    .poll          = sim_poll,
    .get_frame     = sim_get_frame,
    .get_ref_frame = sim_get_ref_frame,
};

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */
eit_acq_backend_t *eit_acq_simulated_create(const char *ref_filename,
                                             const char *target_filename)
{
    if (!ref_filename || !target_filename) return NULL;

    eit_acq_simulated_t *sim = (eit_acq_simulated_t *)calloc(1, sizeof(*sim));
    if (!sim) return NULL;

    sim->base.ops = &s_sim_ops;

    /* Load reference data from SD */
    uint16_t ref_n_meas = 0, ref_n_inj = 0;
    sim->ref_uel_2d = dataset_service_load_uel_2d(ref_filename, &ref_n_meas, &ref_n_inj);
    if (!sim->ref_uel_2d) {
        free(sim);
        return NULL;
    }

    /* Load target data from SD */
    uint16_t tgt_n_meas = 0, tgt_n_inj = 0;
    sim->target_uel_2d = dataset_service_load_uel_2d(target_filename, &tgt_n_meas, &tgt_n_inj);
    if (!sim->target_uel_2d) {
        dataset_service_free_uel_2d(sim->ref_uel_2d);
        free(sim);
        return NULL;
    }

    /* Dimension check */
    if (ref_n_meas != tgt_n_meas || ref_n_inj != tgt_n_inj) {
        dataset_service_free_uel_2d(sim->ref_uel_2d);
        dataset_service_free_uel_2d(sim->target_uel_2d);
        free(sim);
        return NULL;
    }

    sim->n_meas = ref_n_meas;
    sim->n_inj  = ref_n_inj;

    /* Pre-allocate noise scratch buffer */
    const uint32_t total = (uint32_t)sim->n_meas * (uint32_t)sim->n_inj;
    sim->noisy_buf = (float *)malloc(total * sizeof(float));

    /* Pre-compute signal RMS for noise scaling */
    compute_signal_rms(sim);

    /* Defaults */
    sim->noise_enabled   = 0;
    sim->noise_level_pct = 10;
    sim->rng_state       = 0xDEADBEEFu;

    sim->inject_time_ms  = 1;   /* 1 ms per injection step  */
    sim->measure_time_ms = 1;   /* 1 ms per measurement step */
    recompute_timing(sim);       /* → ~160 ms / frame at 79 injections */

    sim->state         = EIT_ACQ_IDLE;
    sim->frame_counter = 0;

    return &sim->base;
}

void eit_acq_simulated_destroy(eit_acq_backend_t *backend)
{
    if (!backend) return;
    eit_acq_simulated_t *sim = (eit_acq_simulated_t *)backend;

    if (sim->ref_uel_2d)    dataset_service_free_uel_2d(sim->ref_uel_2d);
    if (sim->target_uel_2d) dataset_service_free_uel_2d(sim->target_uel_2d);
    if (sim->noisy_buf)     free(sim->noisy_buf);
    free(sim);
}

void eit_acq_simulated_set_timing(eit_acq_backend_t *backend,
                                   uint32_t inject_ms,
                                   uint32_t measure_ms)
{
    if (!backend) return;
    eit_acq_simulated_t *sim = (eit_acq_simulated_t *)backend;
    sim->inject_time_ms  = inject_ms;
    sim->measure_time_ms = measure_ms;
    recompute_timing(sim);
}

void eit_acq_simulated_set_noise(eit_acq_backend_t *backend,
                                  int enabled, int32_t level_pct)
{
    if (!backend) return;
    eit_acq_simulated_t *sim = (eit_acq_simulated_t *)backend;
    sim->noise_enabled = enabled;
    if (level_pct < 0)   level_pct = 0;
    if (level_pct > 100) level_pct = 100;
    sim->noise_level_pct = level_pct;
}
