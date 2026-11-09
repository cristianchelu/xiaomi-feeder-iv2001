/*
 * Web admin: network settings, password, provisioning, backup —
 * spec/30-processes/web-ui.md § Administration
 */

#ifndef WEB_ADMIN_H
#define WEB_ADMIN_H

#include <stdbool.h>
#include <stddef.h>

#include "port_err.h"

#define WEB_ADMIN_PASS_MAX 32u

typedef enum {
    WEB_ADMIN_NETWORK = 0,     /* POST /api/network */
    WEB_ADMIN_PASSWORD,        /* POST /api/password */
    WEB_ADMIN_REPROVISION,     /* POST /api/reprovision */
    WEB_ADMIN_FACTORY_RESET,   /* POST /api/factory_reset */
    WEB_ADMIN_IMPORT,          /* POST /api/import */
} web_admin_action_t;

bool web_admin_password_set(void);
/* True when s[0..n) is the web password (or none is set). */
bool web_admin_password_matches(const char *s, size_t n);
/* POST bodies start with "key=<password>\n" when a password is set. Strips
 * the line (always, when present) and returns false on a wrong/missing key. */
bool web_admin_unlock(const char **body, size_t *len);

/* Runs an admin action; returns PORT_OK, PORT_ERR_INVALID_ARG or PORT_ERR_IO.
 * Actions that need a reboot arm it (response is sent first). */
port_err_t web_admin_run(web_admin_action_t action, const char *body, size_t len);

/* ,"network":{...} member for /api/status (never any password). */
int web_admin_format_network(char *buf, size_t len);
/* GET /api/export: JSON object of settings ("group/key": "value"). */
int web_admin_export(char *buf, size_t len);

#endif /* WEB_ADMIN_H */
