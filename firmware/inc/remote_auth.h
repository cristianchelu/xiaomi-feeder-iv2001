/*
 * Remote console password gate — spec/30-processes/uart-console.md § Security
 */
#ifndef REMOTE_AUTH_H
#define REMOTE_AUTH_H

#include <stdbool.h>
#include <stddef.h>

#include "web_admin.h"

typedef enum {
    REMOTE_AUTH_PENDING,
    REMOTE_AUTH_OK,
    REMOTE_AUTH_DENIED,
} remote_auth_result_t;

typedef struct {
    char line[WEB_ADMIN_PASS_MAX + 1u];
    size_t len;
    bool overflow;
} remote_auth_t;

bool remote_auth_required(void);
void remote_auth_begin(remote_auth_t *a);
/* Feed one received byte (already telnet-filtered); CR or LF ends the line. */
remote_auth_result_t remote_auth_feed(remote_auth_t *a, int c);

#endif /* REMOTE_AUTH_H */
