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

        class GenerateVoucherResponseTrigger : public Trigger<std::string&> {
        public:
            void process(std::string& code)
            {
                this->trigger(code);
            }
        };

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
            void set_expire(int value) { this->expire_ = value; }
            void set_data_limit(int value) { this->data_limit_ = value; }
            void set_download_limit(int value) { this->download_limit_ = value; }
            void set_upload_limit(int value) { this->upload_limit_ = value; }

            void register_response_trigger(GenerateVoucherResponseTrigger* trigger) { this->response_triggers_.push_back(trigger); }
            void register_error_trigger(Trigger<>* trigger) { this->error_triggers_.push_back(trigger); }

            void play(Ts... x) override
            {
                char* code = (char*)malloc(12);
                if (this->parent_->generate_voucher(note_, expire_, data_limit_, download_limit_, upload_limit_, code))
                {
                    std::string response_body = code;
                    if (this->response_triggers_.size() == 1)
                    {
                        this->response_triggers_[0]->process(response_body);
                    }
                    else
                    {
                        for (auto* trigger : this->response_triggers_)
                        {
                            auto response_body_copy = std::string(response_body);
                            trigger->process(response_body_copy);
                        }
                    }
                }
                else
                {
                    for (auto* trigger : this->error_triggers_)
                        trigger->trigger();
                }
                free(code);
            }

        private:
            std::string note_;
            int expire_;
            std::optional<int> data_limit_;
            std::optional<int> download_limit_;
            std::optional<int> upload_limit_;

            UnifiHotspotComponent* parent_;
            std::vector<GenerateVoucherResponseTrigger*> response_triggers_{};
            std::vector<Trigger<>*> error_triggers_{};
        };

    }  // namespace empty_component
} // namespace esphome