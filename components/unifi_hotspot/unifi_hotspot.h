#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string>
#include <optional>

namespace esphome {
    namespace unifi_hotspot {

        class UnifiHotspotComponent : public Component {
        public:
            void dump_config() override;

            void set_url(const char* value) { this->url_ = value; }
            void set_site(const char* value) { this->site_ = value; }
            void set_username(const char* value) { this->username_ = value; }
            void set_password(const char* value) { this->password_ = value; }

            bool generate_voucher(std::string note, int expire, std::optional<int> data_limit, std::optional<int> upload, std::optional<int> download, char* code);

        private:
            const char* url_{ nullptr };
            const char* site_{ nullptr };
            const char* username_{ nullptr };
            const char* password_{ nullptr };

            std::string cookie_;
            std::string csrf_token_;

            struct UnifiAuthData {
                char cookie[400];
                char csrf_token[50];
            };
            bool authenticate(const std::string& url, const std::string& username, const std::string& password);
            bool callApi(esp_http_client_method_t method, std::string url, std::string body, char* response);
            static esp_err_t _event_handle(esp_http_client_event_t* evt);
        };

        template <typename... Ts>
        class GenerateVoucherAction : public Action<Ts...> {
        public:
            GenerateVoucherAction(UnifiHotspotComponent* parent) : parent_(parent) {}

            void set_note(std::string value) { this->note_ = value; }
            void set_data_limit(int value) { this->data_limit_ = value; }
            void set_download_limit(int value) { this->download_limit_ = value; }
            void set_upload_limit(int value) { this->upload_limit_ = value; }

            Trigger<std::string>* get_set_response_trigger() const { return response_trigger_; }
            void register_error_trigger(Trigger<>* trigger) { this->error_triggers_.push_back(trigger); }

            void play(Ts... x) override
            {
                char* code = (char*)malloc(12);
                if (this->parent_->generate_voucher(note_, this->expire_.value(x...), data_limit_, download_limit_, upload_limit_, code))
                {
                    std::string response_body = code;
                    this->response_trigger_->trigger(response_body);
                }
                else
                {
                    for (auto* trigger : this->error_triggers_)
                        trigger->trigger();
                }
                free(code);
            }
            TEMPLATABLE_VALUE(int, expire)

        private:

            std::string note_;
            std::optional<int> data_limit_;
            std::optional<int> download_limit_;
            std::optional<int> upload_limit_;

            UnifiHotspotComponent* parent_;
            Trigger<std::string>* response_trigger_ = new Trigger<std::string>();
            std::vector<Trigger<>*> error_triggers_{};
        };

    }  // namespace empty_component
} // namespace esphome