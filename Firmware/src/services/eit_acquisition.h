/**
 * EIT Data Acquisition – Hardware Abstraction Layer
 *
 * Defines a generic interface for acquiring EIT measurement frames.
 * Backends implement the ops vtable:
 *   - Simulated backend : replays data from SD card files with configurable timing
 *   - Real HW backend   : controls DAQ unit (current injection + ADC measurement)
 *
 * The acquisition pipeline models three hardware states:
 *   INJECTING  → current is being driven into electrode pair
 *   MEASURING  → ADC is sampling the voltage electrodes
 *   FRAME_READY → all injections complete, full Uel matrix available
 */
#ifndef EIT_ACQUISITION_H
#define EIT_ACQUISITION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Acquisition state machine                                          */
/* ------------------------------------------------------------------ */
typedef enum {
    EIT_ACQ_IDLE,           /**< Not started / between frames            */
    EIT_ACQ_INJECTING,      /**< Current injection in progress           */
    EIT_ACQ_MEASURING,      /**< Voltage measurement in progress         */
    EIT_ACQ_FRAME_READY,    /**< Full frame acquired, call get_frame()   */
    EIT_ACQ_ERROR           /**< Unrecoverable error                     */
} eit_acq_state_t;

/* Returned by poll() — includes progress within the frame */
typedef struct {
    eit_acq_state_t state;
    uint16_t current_injection;   /**< 0-based index of current injection   */
    uint16_t total_injections;    /**< Total injections per frame (n_inj)   */
} eit_acq_status_t;

/* ------------------------------------------------------------------ */
/*  Measurement frame                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    float   *uel;            /**< Voltage data, flat [n_meas × n_inj].
                                  Owned by backend – valid until next
                                  get_frame() or backend destruction.      */
    uint16_t n_meas;         /**< Measurements per injection               */
    uint16_t n_inj;          /**< Number of injection patterns             */
    uint32_t frame_number;   /**< Sequential frame counter (0-based)       */
    uint32_t timestamp_ms;   /**< Tick when frame completed                */
} eit_frame_t;

/* ------------------------------------------------------------------ */
/*  Backend interface (vtable)                                         */
/* ------------------------------------------------------------------ */
typedef struct eit_acq_backend eit_acq_backend_t;

typedef struct {
    int               (*init)(eit_acq_backend_t *self);
    void              (*deinit)(eit_acq_backend_t *self);
    int               (*start_frame)(eit_acq_backend_t *self);
    eit_acq_status_t  (*poll)(eit_acq_backend_t *self);
    int               (*get_frame)(eit_acq_backend_t *self, eit_frame_t *out);
    int               (*get_ref_frame)(eit_acq_backend_t *self, eit_frame_t *out);
} eit_acq_ops_t;

struct eit_acq_backend {
    const eit_acq_ops_t *ops;
    /* Backend-specific data follows via struct embedding */
};

/* ------------------------------------------------------------------ */
/*  High-level acquisition service (thin wrapper around active backend) */
/* ------------------------------------------------------------------ */
void              eit_acquisition_init(eit_acq_backend_t *backend);
void              eit_acquisition_deinit(void);
int               eit_acquisition_start_frame(void);
eit_acq_status_t  eit_acquisition_poll(void);
int               eit_acquisition_get_frame(eit_frame_t *out);
int               eit_acquisition_get_ref_frame(eit_frame_t *out);
eit_acq_backend_t *eit_acquisition_get_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* EIT_ACQUISITION_H */
