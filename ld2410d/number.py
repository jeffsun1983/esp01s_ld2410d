import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID, CONF_NAME, CONF_ICON, CONF_UNIT_OF_MEASUREMENT,
    CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP, CONF_DISABLED_BY_DEFAULT,
    CONF_MODE, CONF_DEVICE_CLASS
)
from esphome.components.number import NumberMode

DEPENDENCIES = ["ld2410d"]

ld2410d_ns = cg.esphome_ns.namespace("ld2410d")
LD2410DComponent = ld2410d_ns.class_("LD2410DComponent")
LD2410DNumber = ld2410d_ns.class_("LD2410DNumber", number.Number, cg.Component)

CONF_LD2410D_ID = "ld2410d_id"
CONF_GATE_INDEX = "gate"
CONF_TYPE = "type"

MODE_OPTIONS = {
    "auto": NumberMode.NUMBER_MODE_AUTO,
    "slider": NumberMode.NUMBER_MODE_SLIDER,
    "box": NumberMode.NUMBER_MODE_BOX,
}

BASE_SCHEMA = {
    cv.GenerateID(): cv.declare_id(LD2410DNumber),
    cv.GenerateID(CONF_LD2410D_ID): cv.use_id(LD2410DComponent),
    cv.Required(CONF_TYPE): cv.one_of("motion", "micro", "distance", "timeout"),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_ICON): cv.icon,
    cv.Optional(CONF_UNIT_OF_MEASUREMENT): cv.string,
    cv.Optional(CONF_MIN_VALUE): cv.float_,
    cv.Optional(CONF_MAX_VALUE): cv.float_,
    cv.Optional(CONF_STEP): cv.positive_float,
    cv.Optional(CONF_DISABLED_BY_DEFAULT, default=False): cv.boolean,
    cv.Optional(CONF_MODE, default="auto"): cv.enum(MODE_OPTIONS, upper=False),
    cv.Optional(CONF_DEVICE_CLASS): cv.string,
}

def validate_gate(config):
    t = config[CONF_TYPE]
    if t in ["motion", "micro"] and CONF_GATE_INDEX not in config:
        raise cv.Invalid(f"Type '{t}' requires 'gate' field")
    if t in ["distance", "timeout"] and CONF_GATE_INDEX in config:
        raise cv.Invalid(f"Type '{t}' does not allow 'gate' field")
    return config

CONFIG_SCHEMA = cv.All(cv.Schema(BASE_SCHEMA).extend({
    cv.Optional(CONF_GATE_INDEX): cv.int_range(min=0, max=15),
}), validate_gate)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    t = config[CONF_TYPE]
    if t == "distance":
        min_val = config.get(CONF_MIN_VALUE, 0.7)
        max_val = config.get(CONF_MAX_VALUE, 10.0)
        step = config.get(CONF_STEP, 0.1)
    elif t == "timeout":
        min_val = config.get(CONF_MIN_VALUE, 0)
        max_val = config.get(CONF_MAX_VALUE, 300)
        step = config.get(CONF_STEP, 1)
    else:
        min_val = config.get(CONF_MIN_VALUE, 0)
        max_val = config.get(CONF_MAX_VALUE, 95)
        step = config.get(CONF_STEP, 0.5)

    await number.register_number(
        var,
        config,
        min_value=min_val,
        max_value=max_val,
        step=step,
    )

    parent = await cg.get_variable(config[CONF_LD2410D_ID])
    cg.add(var.set_parent(parent))
    if CONF_GATE_INDEX in config:
        cg.add(var.set_gate(config[CONF_GATE_INDEX]))
    cg.add(var.set_type(config[CONF_TYPE]))

    if t == "distance":
        cg.add(parent.register_distance_number(var))
    elif t == "timeout":
        cg.add(parent.register_timeout_number(var))