#include "http_server.hh"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include "svelteesp32.h"

namespace
{
constexpr const char* kLogTag              = "WebServer";
constexpr std::size_t kMaxPostBodyBytes    = 512;
httpd_handle_t        s_httpd              = nullptr;

esp_err_t send_json(httpd_req_t* req, const char* payload)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
}

/**
 * Example GET handler that returns a minimal JSON payload describing device
 * status. The response body illustrates how REST endpoints can respond with
 * JSON.
 */
esp_err_t status_get_handler(httpd_req_t* req)
{
    static constexpr const char* kPayload = R"({"status":"ok","message":"Jarvis web server ready"})";
    ESP_LOGI(kLogTag, "GET %s", req->uri);
    return send_json(req, kPayload);
}

/**
 * Example POST handler that reads a small JSON payload (up to
 * kMaxPostBodyBytes) and echoes part of the result back to the caller. This
 * shows how to consume request bodies and form responses.
 */
esp_err_t settings_post_handler(httpd_req_t* req)
{
    ESP_LOGI(kLogTag, "POST %s len=%zu", req->uri, req->content_len);

    const size_t toRead = std::min<std::size_t>(req->content_len, kMaxPostBodyBytes);
    std::string  body;
    body.resize(toRead);

    size_t readTotal = 0;
    while (readTotal < toRead)
    {
        const size_t remaining = toRead - readTotal;
        const int    received  = httpd_req_recv(req, &body[readTotal], remaining);
        if (received <= 0)
        {
            ESP_LOGW(kLogTag, "Failed to read POST body, received=%d", received);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read body");
            return ESP_FAIL;
        }
        readTotal += static_cast<std::size_t>(received);
    }

    if (req->content_len > kMaxPostBodyBytes)
    {
        ESP_LOGW(kLogTag, "POST body truncated from %zu to %zu bytes", req->content_len, toRead);
    }

    ESP_LOGI(kLogTag, "Received settings payload: %.*s", static_cast<int>(body.size()), body.data());

    const std::string response = R"({"result":"ok","applied":true})";
    return send_json(req, response.c_str());
}

void register_rest_endpoints(httpd_handle_t server)
{
    const httpd_uri_t statusRoute{
        .uri      = "/api/status",
        .method   = HTTP_GET,
        .handler  = status_get_handler,
        .user_ctx = nullptr,
    };

    const httpd_uri_t settingsRoute{
        .uri      = "/api/settings",
        .method   = HTTP_POST,
        .handler  = settings_post_handler,
        .user_ctx = nullptr,
    };

    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_register_uri_handler(server, &statusRoute));
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_register_uri_handler(server, &settingsRoute));
}
} // namespace

httpd_handle_t start_http_server()
{
    if (s_httpd != nullptr)
    {
        ESP_LOGI(kLogTag, "HTTP server already running");
        return s_httpd;
    }

    httpd_config_t config          = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers        = SVELTEESP32_COUNT + 99;
    config.uri_match_fn            = httpd_uri_match_wildcard;

    ESP_LOGI(kLogTag, "Starting server on port: %d", config.server_port);

    httpd_handle_t server = nullptr;
    const esp_err_t err   = httpd_start(&server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(kLogTag, "Failed to start HTTP server: %d", err);
        return nullptr;
    }

    initSvelteStaticFiles(server);
    register_rest_endpoints(server);

    s_httpd = server;
    return s_httpd;
}

void stop_http_server()
{
    if (s_httpd == nullptr)
    {
        return;
    }

    httpd_stop(s_httpd);
    s_httpd = nullptr;
    ESP_LOGI(kLogTag, "HTTP server stopped");
}
