/**
 * Lightweight JSON Encoder
 *
 * Minimal JSON generator for embedded systems.
 * Writes to a user-provided buffer without dynamic allocation.
 */
#ifndef JSON_ENCODER_H
#define JSON_ENCODER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * JSON encoder context - writes to user-provided buffer.
 */
typedef struct {
    char *buf;          /**< Output buffer */
    size_t buf_size;    /**< Total buffer size */
    size_t pos;         /**< Current write position */
    int depth;          /**< Nesting depth (for debugging) */
    int needs_comma;    /**< Flag: need comma before next element */
    int error;          /**< Flag: buffer overflow occurred */
} json_encoder_t;

/**
 * Initialize encoder with output buffer.
 * @param enc Encoder context
 * @param buf Output buffer
 * @param buf_size Buffer size in bytes
 */
void json_init(json_encoder_t *enc, char *buf, size_t buf_size);

/* ---- Object/Array delimiters ---- */

void json_object_start(json_encoder_t *enc);
void json_object_end(json_encoder_t *enc);
void json_array_start(json_encoder_t *enc);
void json_array_end(json_encoder_t *enc);

/* ---- Key-value pairs (use inside object) ---- */

/** Write a key (followed by colon). Next call should be a value. */
void json_key(json_encoder_t *enc, const char *key);

/** Write key: "value" */
void json_key_string(json_encoder_t *enc, const char *key, const char *value);

/** Write key: integer */
void json_key_int(json_encoder_t *enc, const char *key, int32_t value);

/** Write key: unsigned integer */
void json_key_uint(json_encoder_t *enc, const char *key, uint32_t value);

/** Write key: float (scientific notation) */
void json_key_float(json_encoder_t *enc, const char *key, float value);

/** Write key: true/false */
void json_key_bool(json_encoder_t *enc, const char *key, int value);

/** Write key: null */
void json_key_null(json_encoder_t *enc, const char *key);

/* ---- Array values (use inside array) ---- */

void json_string(json_encoder_t *enc, const char *value);
void json_int(json_encoder_t *enc, int32_t value);
void json_uint(json_encoder_t *enc, uint32_t value);
void json_float(json_encoder_t *enc, float value);
void json_bool(json_encoder_t *enc, int value);
void json_null(json_encoder_t *enc);

/* ---- Start named object or array ---- */

/** Write key: { ... (caller must call json_object_end) */
void json_key_object(json_encoder_t *enc, const char *key);

/** Write key: [ ... (caller must call json_array_end) */
void json_key_array(json_encoder_t *enc, const char *key);

/* ---- Result accessors ---- */

/** Get current length of JSON string (excluding NUL terminator). */
size_t json_get_length(json_encoder_t *enc);

/** Check if buffer overflow occurred. */
int json_has_error(json_encoder_t *enc);

/** Get pointer to the output buffer. */
const char *json_get_buffer(json_encoder_t *enc);

#ifdef __cplusplus
}
#endif

#endif /* JSON_ENCODER_H */
