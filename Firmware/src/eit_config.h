/**
 * EIT System Configuration
 *
 * Central place for every tuneable parameter so that changing a value
 * here propagates automatically to every module that depends on it.
 */
#ifndef EIT_CONFIG_H
#define EIT_CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Reconstruction grid                                                */
/* ------------------------------------------------------------------ */
/** Side-length of the square reconstruction image (pixels). */
#define EIT_IMAGE_SIZE          32u

/** Total number of pixels in the reconstruction image. */
#define EIT_MAX_PIXELS          (EIT_IMAGE_SIZE * EIT_IMAGE_SIZE)

/* ------------------------------------------------------------------ */
/*  Electrode / measurement geometry                                   */
/* ------------------------------------------------------------------ */
/** Number of electrodes placed around the object boundary. */
#define EIT_NUM_ELECTRODES      16u

/** Maximum number of measurements the system can handle.
 *  For a 16-electrode adjacent-drive system: 79 injections × 16 = 1264. */
#define EIT_MAX_MEASUREMENTS    1264u

/* ------------------------------------------------------------------ */
/*  Display                                                            */
/* ------------------------------------------------------------------ */
/** Side-length of the upscaled square image shown on screen (pixels). */
#define EIT_DISPLAY_SIZE        288u

/* ------------------------------------------------------------------ */
/*  Noise injection defaults                                           */
/* ------------------------------------------------------------------ */
/** Maximum noise-level slider value (percentage). */
#define EIT_NOISE_LEVEL_MAX     17

/** Default noise level at start-up (percentage). */
#define EIT_NOISE_LEVEL_DEFAULT 10

/* ------------------------------------------------------------------ */
/*  SDRAM memory map                                                   */
/*  16 MB total  (0xC000_0000 – 0xC0FF_FFFF)                          */
/*  First ~4 MB are reserved for the LTDC framebuffers (managed by      */
/*  the BSP / tft driver).                                              */
/* ------------------------------------------------------------------ */
/** Sensitivity-matrix storage (up to 5 MB). */
#define EIT_SDRAM_SENSITIVITY_ADDR  ((uint32_t)0xC0400000u)

/** Pre-upscaled RGB565 color buffer for the reconstruction view. */
#define EIT_SDRAM_COLOR_BUF_ADDR    ((uint32_t)0xC0900000u)

/** Electrode-field buffer used during calibration / sensitivity
 *  matrix generation. */
#define EIT_SDRAM_ELEC_FIELD_ADDR   ((uint32_t)0xC0A00000u)

/* ------------------------------------------------------------------ */
/*  Algorithm selection                                                */
/* ------------------------------------------------------------------ */
typedef enum {
    EIT_ALGO_LBP = 0,
    /* future: EIT_ALGO_GREIT, EIT_ALGO_DBAR, ... */
    EIT_ALGO_COUNT
} eit_algorithm_t;

/** Default reconstruction algorithm. */
#define EIT_ALGO_DEFAULT        EIT_ALGO_LBP

/** Default "show data table" flag (1 = show, 0 = hide). */
#define EIT_SHOW_DATA_TABLE_DEFAULT  1

/* ------------------------------------------------------------------ */
/*  File browser                                                       */
/* ------------------------------------------------------------------ */
/** Maximum number of files the SD browser can display. */
#define EIT_MAX_BROWSER_FILES   50

#endif /* EIT_CONFIG_H */
