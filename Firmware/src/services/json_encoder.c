/**
 * Lightweight JSON Encoder - Implementation
 */
#include "json_encoder.h"

#include <string.h>
#include <stdio.h>

/* ---- Internal helpers ---- */

static void json_write_char(json_encoder_t *enc, char c)
{
    if (enc->error) return;
    if (enc->pos >= enc->buf_size - 1) {
        enc->error = 1;
        return;
    }
    enc->buf[enc->pos++] = c;
    enc->buf[enc->pos] = '\0';
}

static void json_write_str(json_encoder_t *enc, const char *s)
{
    if (enc->error || !s) return;
    while (*s) {
        json_write_char(enc, *s++);
    }
}

static void json_write_escaped_str(json_encoder_t *enc, const char *s)
{
    if (enc->error || !s) return;
    json_write_char(enc, '"');
    while (*s) {
        char c = *s++;
        switch (c) {
        case '"':  json_write_str(enc, "\\\""); break;
        case '\\': json_write_str(enc, "\\\\"); break;
        case '\n': json_write_str(enc, "\\n"); break;
        case '\r': json_write_str(enc, "\\r"); break;
        case '\t': json_write_str(enc, "\\t"); break;
        default:
            if ((unsigned char)c < 0x20) {
                /* Control character - skip or escape */
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                json_write_str(enc, buf);
            } else {
                json_write_char(enc, c);
            }
            break;
        }
    }
    json_write_char(enc, '"');
}

static void json_maybe_comma(json_encoder_t *enc)
{
    if (enc->needs_comma) {
        json_write_char(enc, ',');
    }
    enc->needs_comma = 0;
}

/**
 * Convert a float to a decimal ASCII string using ONLY integer printf
 * formatters (%ld, %lu).  This avoids the need for float-aware printf
 * which is not available when linking with -nodefaultlibs.
 *
 * Output uses scientific notation: [-]D.DDDDDDeN
 * 7 significant digits - enough for float32 full precision.
 *
 * Returns the number of characters written (excluding NUL).
 */
static int float_to_str(char *buf, size_t bufsz, float val)
{
    /* NaN: val != val is the portable test */
    if (val != val) {
        if (bufsz >= 4) { buf[0]='N'; buf[1]='a'; buf[2]='N'; buf[3]='\0'; }
        return 3;
    }

    /* Zero */
    if (val == 0.0f) {
        if (bufsz >= 2) { buf[0]='0'; buf[1]='\0'; }
        return 1;
    }

    int neg = 0;
    if (val < 0.0f) { neg = 1; val = -val; }

    /* Find decimal exponent: normalise val into [1.0, 10.0) */
    int exp10 = 0;
    if (val >= 10.0f) {
        while (val >= 1000.0f) { val *= 0.001f; exp10 += 3; }
        while (val >= 10.0f)   { val *= 0.1f;   exp10 += 1; }
    } else if (val < 1.0f) {
        while (val < 0.001f) { val *= 1000.0f; exp10 -= 3; }
        while (val < 1.0f)   { val *= 10.0f;   exp10 -= 1; }
    }

    /* Extract 7 significant digits as integer */
    uint32_t sig = (uint32_t)(val * 1000000.0f + 0.5f);
    if (sig >= 10000000u) { sig /= 10u; exp10++; }

    /* Split into integer digit + 6 fractional digits */
    uint32_t d0   = sig / 1000000u;
    uint32_t frac = sig % 1000000u;

    /* Strip trailing zeros from fraction for cleaner output */
    int frac_digits = 6;
    while (frac_digits > 0 && (frac % 10u) == 0) {
        frac /= 10u;
        frac_digits--;
    }

    int len;
    if (frac_digits == 0 && exp10 == 0) {
        len = snprintf(buf, bufsz, "%s%lu",
                       neg ? "-" : "", (unsigned long)d0);
    } else if (frac_digits == 0) {
        len = snprintf(buf, bufsz, "%s%lue%d",
                       neg ? "-" : "", (unsigned long)d0, exp10);
    } else if (exp10 == 0) {
        len = snprintf(buf, bufsz, "%s%lu.%0*lu",
                       neg ? "-" : "", (unsigned long)d0,
                       frac_digits, (unsigned long)frac);
    } else {
        len = snprintf(buf, bufsz, "%s%lu.%0*lue%d",
                       neg ? "-" : "", (unsigned long)d0,
                       frac_digits, (unsigned long)frac, exp10);
    }
    return (len > 0) ? len : 0;
}

/* ---- Public API ---- */

void json_init(json_encoder_t *enc, char *buf, size_t buf_size)
{
    if (!enc || !buf || buf_size == 0) return;

    enc->buf = buf;
    enc->buf_size = buf_size;
    enc->pos = 0;
    enc->depth = 0;
    enc->needs_comma = 0;
    enc->error = 0;
    enc->buf[0] = '\0';
}

void json_object_start(json_encoder_t *enc)
{
    if (!enc) return;
    json_maybe_comma(enc);
    json_write_char(enc, '{');
    enc->depth++;
    enc->needs_comma = 0;
}

void json_object_end(json_encoder_t *enc)
{
    if (!enc) return;
    json_write_char(enc, '}');
    if (enc->depth > 0) enc->depth--;
    enc->needs_comma = 1;
}

void json_array_start(json_encoder_t *enc)
{
    if (!enc) return;
    json_maybe_comma(enc);
    json_write_char(enc, '[');
    enc->depth++;
    enc->needs_comma = 0;
}

void json_array_end(json_encoder_t *enc)
{
    if (!enc) return;
    json_write_char(enc, ']');
    if (enc->depth > 0) enc->depth--;
    enc->needs_comma = 1;
}

void json_key(json_encoder_t *enc, const char *key)
{
    if (!enc || !key) return;
    json_maybe_comma(enc);
    json_write_escaped_str(enc, key);
    json_write_char(enc, ':');
    enc->needs_comma = 0;
}

void json_key_string(json_encoder_t *enc, const char *key, const char *value)
{
    if (!enc || !key) return;
    json_key(enc, key);
    json_write_escaped_str(enc, value ? value : "");
    enc->needs_comma = 1;
}

void json_key_int(json_encoder_t *enc, const char *key, int32_t value)
{
    if (!enc || !key) return;
    json_key(enc, key);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    json_write_str(enc, buf);
    enc->needs_comma = 1;
}

void json_key_uint(json_encoder_t *enc, const char *key, uint32_t value)
{
    if (!enc || !key) return;
    json_key(enc, key);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
    json_write_str(enc, buf);
    enc->needs_comma = 1;
}

void json_key_float(json_encoder_t *enc, const char *key, float value)
{
    if (!enc || !key) return;
    json_key(enc, key);
    char buf[32];
    float_to_str(buf, sizeof(buf), value);
    json_write_str(enc, buf);
    enc->needs_comma = 1;
}

void json_key_bool(json_encoder_t *enc, const char *key, int value)
{
    if (!enc || !key) return;
    json_key(enc, key);
    json_write_str(enc, value ? "true" : "false");
    enc->needs_comma = 1;
}

void json_key_null(json_encoder_t *enc, const char *key)
{
    if (!enc || !key) return;
    json_key(enc, key);
    json_write_str(enc, "null");
    enc->needs_comma = 1;
}

void json_string(json_encoder_t *enc, const char *value)
{
    if (!enc) return;
    json_maybe_comma(enc);
    json_write_escaped_str(enc, value ? value : "");
    enc->needs_comma = 1;
}

void json_int(json_encoder_t *enc, int32_t value)
{
    if (!enc) return;
    json_maybe_comma(enc);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    json_write_str(enc, buf);
    enc->needs_comma = 1;
}

void json_uint(json_encoder_t *enc, uint32_t value)
{
    if (!enc) return;
    json_maybe_comma(enc);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
    json_write_str(enc, buf);
    enc->needs_comma = 1;
}

void json_float(json_encoder_t *enc, float value)
{
    if (!enc) return;
    json_maybe_comma(enc);
    char buf[32];
    float_to_str(buf, sizeof(buf), value);
    json_write_str(enc, buf);
    enc->needs_comma = 1;
}

void json_bool(json_encoder_t *enc, int value)
{
    if (!enc) return;
    json_maybe_comma(enc);
    json_write_str(enc, value ? "true" : "false");
    enc->needs_comma = 1;
}

void json_null(json_encoder_t *enc)
{
    if (!enc) return;
    json_maybe_comma(enc);
    json_write_str(enc, "null");
    enc->needs_comma = 1;
}

void json_key_object(json_encoder_t *enc, const char *key)
{
    if (!enc || !key) return;
    json_key(enc, key);
    json_object_start(enc);
}

void json_key_array(json_encoder_t *enc, const char *key)
{
    if (!enc || !key) return;
    json_key(enc, key);
    json_array_start(enc);
}

size_t json_get_length(json_encoder_t *enc)
{
    if (!enc) return 0;
    return enc->pos;
}

int json_has_error(json_encoder_t *enc)
{
    if (!enc) return 1;
    return enc->error;
}

const char *json_get_buffer(json_encoder_t *enc)
{
    if (!enc) return NULL;
    return enc->buf;
}
