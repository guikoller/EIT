#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SSID_MAX      33
#define WIFI_PASSWORD_MAX  65
#define WIFI_SERVER_MAX    128

typedef struct {
    char ssid[WIFI_SSID_MAX];
    char password[WIFI_PASSWORD_MAX];
    char server[WIFI_SERVER_MAX];
} wifi_service_config_t;

typedef enum {
    WIFI_STATE_OFFLINE = 0,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_ERROR,
} wifi_service_state_t;

int wifi_service_init(void);

void wifi_service_get_config(wifi_service_config_t *cfg);
void wifi_service_set_config(const wifi_service_config_t *cfg);

int wifi_service_load_config(void);
int wifi_service_save_config(void);

wifi_service_state_t wifi_service_get_state(void);
const char *wifi_service_last_error(void);

int wifi_service_connect(void);
int wifi_service_send_json_file(const char *json_path,
                                char *server_reply,
                                uint32_t server_reply_size);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SERVICE_H */
