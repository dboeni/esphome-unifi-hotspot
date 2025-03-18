# ESPHome unifi-hotspot Ccomponent

This is an ESPHome custom component to create a voucher code for a Unifi hotspot.

# Installation

To use this component, you need to add `dboeni/esphome-unifi-hotspot` GitHub repository to your ESPHome configuration.

### Example 

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dboeni/esphome-unifi-hotspot

unifi_hotspot:
  username: !secret username
  password: !secret password
  site: "default"
  url: "http://url.dtl:8080"

on_...:
  - unifi_hotspot.generate_voucher:
      note: "ESP Voucher"
      expire: 60
      upload_limit: 100
      download_limit: 100
      data_limit: 100
      on_response:
        then:
          - logger.log:
              format: 'Response code: %s'
              args:
                - code
      on_error:
        then:
          - logger.log: "Error"
```
