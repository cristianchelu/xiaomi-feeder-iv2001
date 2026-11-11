/*
 * NTP server setting — spec/30-processes/time-sync.md § Server
 */

#include "ntp_config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "config_keys.h"
#include "config_port.h"

static bool ntp_config_valid(const char *s)
{
    size_t i;
    size_t n = strlen(s);

    if (n == 0u || n > NTP_CONFIG_MAX_LEN || s[0] == '.' || s[0] == '-') {
        return false;
    }
    for (i = 0u; i < n; i++) {
        unsigned char c = (unsigned char)s[i];

        if (!isalnum(c) && c != '.' && c != '-') {
            return false;
        }
    }
    return true;
}

void ntp_config_get(char *buf, size_t len)
{
    const config_port_t *cfg = config_port_get();
    char tmp[NTP_CONFIG_MAX_LEN + 1u];

    if (buf == NULL || len == 0u) {
        return;
    }
    if (cfg != NULL &&
        cfg->read(CONFIG_GROUP_TIME, CONFIG_KEY_NTP_SERVER, tmp, sizeof(tmp)) == PORT_OK &&
        ntp_config_valid(tmp)) {
        (void)snprintf(buf, len, "%s", tmp);
        return;
    }
    (void)snprintf(buf, len, "%s", NTP_CONFIG_DEFAULT);
}

port_err_t ntp_config_set(const char *server)
{
    const config_port_t *cfg = config_port_get();

    if (cfg == NULL || server == NULL) {
        return PORT_ERR_INVALID_ARG;
    }
    if (server[0] == '\0') {
        (void)cfg->erase(CONFIG_GROUP_TIME, CONFIG_KEY_NTP_SERVER);
        return PORT_OK;
    }
    if (!ntp_config_valid(server)) {
        return PORT_ERR_INVALID_ARG;
    }
    return cfg->write(CONFIG_GROUP_TIME, CONFIG_KEY_NTP_SERVER, server);
}
