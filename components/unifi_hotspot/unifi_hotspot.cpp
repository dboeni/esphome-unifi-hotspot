#include "unifi_hotspot.h"
#include "esp_log.h"
#include "cJSON.h"

namespace esphome {
    namespace unifi_hotspot {

#define MAX_HTTP_RECV_BUFFER 1024
        static const char* TAG = "unifi_hotspot";

        void UnifiHotspotComponent::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Unifi Hotspot:");
            ESP_LOGCONFIG(TAG, "  URL: %s", this->url_);
            ESP_LOGCONFIG(TAG, "  Site: %s", this->site_);
            ESP_LOGCONFIG(TAG, "  Username: %s", this->username_);
            ESP_LOGCONFIG(TAG, "  Password: *****");
        }

        esp_err_t UnifiHotspotComponent::_event_handle(esp_http_client_event_t* evt)
        {
            UnifiAuthData* header_data = nullptr;
            if (evt->user_data)
            {
                header_data = static_cast<UnifiAuthData*>(evt->user_data);
            }

            switch (evt->event_id)
            {
            case HTTP_EVENT_ON_HEADER:
                ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER: %s: %s", evt->header_key, evt->header_value);

                if (strcmp(evt->header_key, "Set-Cookie") == 0)
                {
                    std::string complete_cookie(evt->header_value);
                    size_t end = complete_cookie.find(";");
                    std::string cookie = complete_cookie.substr(0, end);

                    ESP_LOGI(TAG, "Cookie: %s", cookie);
                    strncpy(header_data->cookie, cookie.c_str(), sizeof(header_data->cookie) - 1);
                    header_data->cookie[sizeof(header_data->cookie) - 1] = '\0';
                }
                else if (strcmp(evt->header_key, "X-Csrf-Token") == 0)
                {
                    std::string token(evt->header_value);
                    ESP_LOGI(TAG, "X-Csrf-Token: %s", token);

                    strncpy(header_data->csrf_token, token.c_str(), sizeof(header_data->csrf_token) - 1);
                    header_data->csrf_token[sizeof(header_data->csrf_token) - 1] = '\0';
                }
                break;
            }
            return ESP_OK;
        }

        bool UnifiHotspotComponent::authenticate(const std::string& url, const std::string& username, const std::string& password)
        {
            ESP_LOGI(TAG, "Authenticating with Unifi Hotspot...");

            UnifiAuthData header_data = {};

            std::string full_url = url + "/api/auth/login";
            esp_http_client_config_t config = {};
            config.url = full_url.c_str();
            config.method = HTTP_METHOD_POST;
            config.timeout_ms = 4000;
            config.buffer_size = 1000;
            config.buffer_size_tx = 1000;
            config.auth_type = HTTP_AUTH_TYPE_BASIC;
            //config.transport_type = HTTP_TRANSPORT_OVER_TCP;
            //config.disable_auto_redirect = false;
            //config.max_redirection_count = 3;
            config.user_data = &header_data;
            config.event_handler = &UnifiHotspotComponent::_event_handle;

            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_header(client, "Accept", "application/json");
            ESP_LOGI(TAG, "Header setting");

            std::string body = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
            esp_http_client_set_post_field(client, body.c_str(), body.length());

            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK)
            {
                int status_code = esp_http_client_get_status_code(client);
                if (status_code == 200)
                {
                    ESP_LOGI(TAG, "Login request sent successfully");

                    ESP_LOGD(TAG, "Cookie: %s", header_data.cookie);
                    ESP_LOGD(TAG, "csrf_token: %s", header_data.csrf_token);

                    cookie_ = header_data.cookie;
                    csrf_token_ = header_data.csrf_token;

                    esp_http_client_cleanup(client);
                    return true;
                }
            }
            else
            {
                ESP_LOGE(TAG, "Login failed, error: %s", esp_err_to_name(err));
            }
            esp_http_client_cleanup(client);
            return false;
        }

        bool UnifiHotspotComponent::callApi(esp_http_client_method_t method, std::string url, std::string body, char* response)
        {
            esp_http_client_config_t config = {};
            config.url = url.c_str();
            config.method = method;
            config.buffer_size = 1024;
            config.buffer_size_tx = 1024;

            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_err_t err;

            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_header(client, "Accept-Encoding", "identity");
            esp_http_client_set_header(client, "Cookie", cookie_.c_str());
            esp_http_client_set_header(client, "x-csrf-token", csrf_token_.c_str());

            const int body_len = body.length();
            ESP_LOGI(TAG, "Body len: %d", body_len);
            err = esp_http_client_open(client, body_len);
            if (err != ESP_OK)
            {
                this->status_momentary_error("failed", 1000);
                ESP_LOGE(TAG, "HTTP Request failed: %s", esp_err_to_name(err));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return false;
            }

            if (body_len > 0)
            {
                int write_left = body_len;
                int write_index = 0;
                const char* buf = body.c_str();
                while (write_left > 0)
                {
                    int written = esp_http_client_write(client, buf + write_index, write_left);
                    if (written < 0)
                    {
                        err = ESP_FAIL;
                        break;
                    }
                    write_left -= written;
                    write_index += written;
                }
            }
            if (err != ESP_OK)
            {
                this->status_momentary_error("failed", 1000);
                ESP_LOGE(TAG, "HTTP Request failed: %s", esp_err_to_name(err));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return false;
            }

            int content_length = esp_http_client_fetch_headers(client);
            int total_read_len = 0, read_len;
            if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER)
            {
                read_len = esp_http_client_read(client, response, content_length);
                if (read_len <= 0)
                {
                    ESP_LOGE(TAG, "Error read data");
                }
                response[read_len] = 0;
                ESP_LOGD(TAG, "read_len = %d", read_len);
            }
            ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            ESP_LOGD(TAG, "response content: %s", response);

            return true;
        }

        bool UnifiHotspotComponent::generate_voucher(std::string note, int expire, std::optional<int> data_limit, std::optional<int> upload, std::optional<int> download, char* code)
        {
            if (!authenticate(this->url_, this->username_, this->password_))
            {
                ESP_LOGE(TAG, "Authentication failed, cannot generate voucher");
                return false;
            }
            ESP_LOGI(TAG, "Generating voucher...");

            char* buffer = (char*)malloc(MAX_HTTP_RECV_BUFFER + 1);
            if (buffer == NULL)
            {
                ESP_LOGE(TAG, "Cannot malloc http receive buffer");
                return false;
            }
            std::string full_url = std::string(url_) + "/proxy/network/api/s/" + std::string(site_) + "/cmd/hotspot";
            std::string body = "{\"cmd\":\"create-voucher\", \"n\":1, \"expire\":" + std::to_string(expire) + ", \"note\":\"" + note + "\"";
            if (data_limit.has_value())
            {
                body += ", \"bytes\":" + std::to_string(data_limit.value());
            }
            if (upload.has_value())
            {
                body += ", \"down\":" + std::to_string(upload.value());
            }
            if (download.has_value())
            {
                body += ", \"up\":" + std::to_string(download.value());
            }
            body += "}";

            if (!callApi(HTTP_METHOD_POST, full_url, body, buffer))
            {
                free(buffer);
            }
            ESP_LOGD(TAG, "response content: %s", buffer);

            int create_time = 0;
            cJSON* root = cJSON_Parse(buffer);
            if (root == nullptr)
            {
                ESP_LOGE(TAG, "Failed to parse JSON");
            }
            else
            {
                cJSON* data_array = cJSON_GetObjectItem(root, "data");
                if (cJSON_IsArray(data_array) && cJSON_GetArraySize(data_array) > 0)
                {
                    cJSON* first_item = cJSON_GetArrayItem(data_array, 0);
                    cJSON* create_time_item = cJSON_GetObjectItem(first_item, "create_time");
                    if (cJSON_IsNumber(create_time_item))
                    {
                        create_time = create_time_item->valueint;
                        ESP_LOGD(TAG, "create_time: %d", create_time);
                    }
                }
                cJSON_Delete(root);
            }

            full_url = std::string(url_) + "/proxy/network/api/s/" + std::string(site_) + "/stat/voucher?create_time=" + std::to_string(create_time);
            body = "";
            if (!callApi(HTTP_METHOD_GET, full_url, body, buffer))
            {
                free(buffer);
            }
            ESP_LOGD(TAG, "response content: %s", buffer);

            root = cJSON_Parse(buffer);
            if (root == nullptr)
            {
                ESP_LOGE(TAG, "Failed to parse JSON");
            }
            else
            {
                cJSON* data_array = cJSON_GetObjectItem(root, "data");
                if (cJSON_IsArray(data_array) && cJSON_GetArraySize(data_array) > 0)
                {
                    cJSON* first_item = cJSON_GetArrayItem(data_array, 0);
                    cJSON* code_item = cJSON_GetObjectItem(first_item, "code");
                    if (cJSON_IsString(code_item))
                    {
                        snprintf(code, 12, "%.5s-%.5s", code_item->valuestring, code_item->valuestring + 5);
                    }
                }
                cJSON_Delete(root);
            }
            return true;
        }

    }  // namespace unifi_hotspot
} // namespace esphome