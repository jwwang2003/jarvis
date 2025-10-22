#pragma once

#include "esp_http_server.h"

/**
 * Starts the HTTP server responsible for serving the Svelte front-end and
 * exposing REST-style API endpoints.
 *
 * The server registers the static routes generated in `svelteesp32.h` and
 * example REST handlers for GET/POST requests.
 *
 * @return Handle to the running server on success, nullptr otherwise.
 */
httpd_handle_t start_http_server();

/**
 * Stops a previously started HTTP server. Safe to call if the server is not
 * running.
 */
void stop_http_server();
