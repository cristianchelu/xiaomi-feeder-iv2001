/*
 * Remote console password gate — spec/30-processes/uart-console.md § Security
 */

#include "remote_auth.h"

#include <string.h>

bool remote_auth_required(void)
{
    return web_admin_password_set();
}

void remote_auth_begin(remote_auth_t *a)
{
    if (a != NULL) {
        memset(a, 0, sizeof(*a));
    }
}

remote_auth_result_t remote_auth_feed(remote_auth_t *a, int c)
{
    if (a == NULL || c < 0) {
        return REMOTE_AUTH_DENIED;
    }
    if (c == '\r' || c == '\n') {
        if (a->len == 0u && !a->overflow) {
            return REMOTE_AUTH_PENDING; /* stray CR/LF before the password */
        }
        return (!a->overflow && web_admin_password_matches(a->line, a->len))
                   ? REMOTE_AUTH_OK
                   : REMOTE_AUTH_DENIED;
    }
    if (a->len >= WEB_ADMIN_PASS_MAX) {
        a->overflow = true;
        return REMOTE_AUTH_DENIED;
    }
    a->line[a->len++] = (char)c;
    return REMOTE_AUTH_PENDING;
}
