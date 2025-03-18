import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_URL, CONF_ON_ERROR, CONF_TRIGGER_ID
from esphome import automation

DEPENDENCIES = ["network"]

unifi_hotspot_ns = cg.esphome_ns.namespace("unifi_hotspot")
UnifiHotspotComponent = unifi_hotspot_ns.class_("UnifiHotspotComponent", cg.Component)

GenerateVoucherAction = unifi_hotspot_ns.class_(
    "GenerateVoucherAction", automation.Action
)
GenerateVoucherResponseTrigger = unifi_hotspot_ns.class_(
    "GenerateVoucherResponseTrigger",
    automation.Trigger.template(cg.std_string),
)

CONF_USERNAME = "username"
CONF_PASSWORD = "password"
CONF_SITE = "site"
CONF_UPLOAD_LIMIT = "upload_limit"
CONF_DOWNLOAD_LIMIT = "download_limit"
CONF_DATA_LIMIT = "data_limit"
CONF_NOTE = "note"
CONF_EXPIRE = "expire"
CONF_ON_RESPONSE = "on_response"


def validate_url(value):
    value = cv.url(value)
    if value.startswith("http://") or value.startswith("https://"):
        return value
    raise cv.Invalid("URL must start with 'http://' or 'https://'")


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UnifiHotspotComponent),
        cv.Optional(CONF_SITE, default="default"): cv.string,
        cv.Required(CONF_URL): cv.templatable(validate_url),
        cv.Required(CONF_USERNAME): cv.string,
        cv.Required(CONF_PASSWORD): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_url(config[CONF_URL]))
    cg.add(var.set_site(config[CONF_SITE]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    await cg.register_component(var, config)


GENERATE_VOUCHER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(UnifiHotspotComponent),
        cv.Optional(CONF_NOTE, default="ESP Voucher"): cv.string,
        cv.Optional(CONF_EXPIRE, default=60): cv.templatable(
            cv.int_range(min=1, max=1000000)
        ),
        cv.Optional(CONF_DATA_LIMIT): cv.templatable(cv.int_range(min=1, max=1048576)),
        cv.Optional(CONF_UPLOAD_LIMIT): cv.templatable(cv.int_range(min=1, max=100)),
        cv.Optional(CONF_DOWNLOAD_LIMIT): cv.templatable(cv.int_range(min=1, max=100)),
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template()
                )
            }
        ),
    }
)


@automation.register_action(
    "unifi_hotspot.generate_voucher", GenerateVoucherAction, GENERATE_VOUCHER_SCHEMA
)
async def generate_voucher_def_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    cg.add(var.set_note(config[CONF_NOTE]))
    template_ = await cg.templatable(config[CONF_EXPIRE], args, cg.int_)
    cg.add(var.set_expire(template_))
    if CONF_DATA_LIMIT in config:
        template_ = await cg.templatable(config[CONF_DATA_LIMIT], args, cg.int_)
        cg.add(var.set_data_limit(template_))
    if CONF_DOWNLOAD_LIMIT in config:
        template_ = await cg.templatable(config[CONF_DOWNLOAD_LIMIT], args, cg.int_)
        cg.add(var.set_download_limit(template_))
    if CONF_UPLOAD_LIMIT in config:
        template_ = await cg.templatable(config[CONF_UPLOAD_LIMIT], args, cg.int_)
        cg.add(var.set_upload_limit(template_))

    if CONF_ON_RESPONSE in config:
        await automation.build_automation(
            var.get_set_response_trigger(),
            [(cg.std_string, "code")],
            config[CONF_ON_RESPONSE],
        )

    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_error_trigger(trigger))
        await automation.build_automation(trigger, [], conf)

    return var
