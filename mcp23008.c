#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio.h>

#define NUM_OF_GPIOS 8

#define MCP23008_IODIR 0x00
#define MCP23008_IPOL 0x01
#define MCP23008_GPINTEN 0x02
#define MCP23008_DEFVAL 0x03
#define MCP23008_INTCON 0x04
#define MCP23008_IOCON 0x05
#define MCP23008_GPPU 0x06
#define MCP23008_INTF 0x07
#define MCP23008_INTCAP 0x08
#define MCP23008_GPIO 0x09
#define MCP23008_OLAT 0x0A

struct mcp23008_context
{
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_config regmap_cfg;
    struct gpio_chip gpiochip;
};

/* chip operations */
static int mcp23008_get_value(struct gpio_chip *chip, unsigned int offset);
static void mcp23008_set_value(struct gpio_chip *chip, unsigned int offset, int val);
static int mcp23008_direction_output(struct gpio_chip *chip, unsigned int offset, int val);
static int mcp23008_direction_input(struct gpio_chip *chip, unsigned int offset);

static int mcp23008_direction(struct gpio_chip *chip, unsigned int offset, bool is_input, int val);

static bool mcp23008_regmap_is_precious_reg(struct device *dev, unsigned int reg);

static int mcp23008_probe(struct i2c_client *client, const struct i2c_device_id *id);
static void mcp23008_remove(struct i2c_client *client);

/* dt match */
struct of_device_id mcp23008_dt_match[] = {
    {.compatible = "mr,custmcp23008"},
    {},
};
MODULE_DEVICE_TABLE(of, mcp23008_dt_match);

struct i2c_device_id mcp23008_ids[] = {
    {"custmcp23008", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, mcp23008_ids);

struct i2c_driver mcp23008_i2c_driver = {
    .probe = mcp23008_probe,
    .remove = mcp23008_remove,
    .id_table = mcp23008_ids,
    .driver = {
        .name = "custmcp23008",
        .of_match_table = of_match_ptr(mcp23008_dt_match),
    },
};

/* writeable ranges */
static const struct regmap_range mcp23008_wr_range[] = {
    {
        .range_min = MCP23008_IODIR,
        .range_max = MCP23008_GPPU,
    },
    {
        .range_min = MCP23008_GPIO,
        .range_max = MCP23008_OLAT,
    },
};

struct regmap_access_table mcp23008_wr_table = {
    .yes_ranges = mcp23008_wr_range,
    .n_yes_ranges = ARRAY_SIZE(mcp23008_wr_range),
};

/* readable range */
static const struct regmap_range mcp23008_rd_range[] = {
    {
        .range_min = MCP23008_IODIR,
        .range_max = MCP23008_OLAT,
    },
};

struct regmap_access_table mcp23008_rd_table = {
    .yes_ranges = mcp23008_rd_range,
    .n_yes_ranges = ARRAY_SIZE(mcp23008_rd_range),
};

static int mcp23008_get_value(struct gpio_chip *chip, unsigned int offset)
{
    int status;
    unsigned int val;
    struct mcp23008_context *ctx;

    ctx = gpiochip_get_data(chip);
    status = regmap_read(ctx->regmap, MCP23008_GPIO, &val);
    if (status)
        return status;

    return (val >> offset) & 0x01;
}

static void mcp23008_set_value(struct gpio_chip *chip, unsigned int offset, int val)
{
    int status;
    struct mcp23008_context *ctx;

    ctx = gpiochip_get_data(chip);
    status = regmap_update_bits(ctx->regmap, MCP23008_GPIO, (1 << offset), (val << offset));
    if (status)
        dev_err(&ctx->client->dev, "error while setting gpio value\n");
}

static int mcp23008_direction_output(struct gpio_chip *chip, unsigned int offset, int val)
{
    return mcp23008_direction(chip, offset, false, val);
}

static int mcp23008_direction_input(struct gpio_chip *chip, unsigned int offset)
{
    return mcp23008_direction(chip, offset, true, 0);
}

static int mcp23008_direction(struct gpio_chip *chip, unsigned int offset, bool is_input, int val)
{
    int status;
    u8 direction_bit;
    struct mcp23008_context *ctx;

    ctx = gpiochip_get_data(chip);
    direction_bit = (u8)is_input;
    status = regmap_update_bits(ctx->regmap, MCP23008_IODIR, (1 << offset), (direction_bit << offset));
    if (status)
        return status;

    if (!is_input)
        mcp23008_set_value(chip, offset, val);

    return 0;
}

static bool mcp23008_regmap_is_precious_reg(struct device *dev, unsigned int reg)
{
    if ((reg == MCP23008_INTCAP) || (reg == MCP23008_GPIO))
        return true;
    return false;
}

static int mcp23008_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int status;
    struct mcp23008_context *ctx;

    ctx = devm_kzalloc(&client->dev, sizeof(struct mcp23008_context), GFP_KERNEL);
    if (ctx == NULL)
        return -ENOMEM;

    i2c_set_clientdata(client, ctx);
    ctx->client = client;

    ctx->regmap_cfg.reg_bits = 8;
    ctx->regmap_cfg.val_bits = 8;
    ctx->regmap_cfg.max_register = MCP23008_OLAT;
    ctx->regmap_cfg.wr_table = &mcp23008_wr_table;
    ctx->regmap_cfg.rd_table = &mcp23008_rd_table;
    ctx->regmap_cfg.precious_reg = mcp23008_regmap_is_precious_reg;
    ctx->regmap_cfg.use_single_read = true;
    ctx->regmap_cfg.use_single_write = true;
    ctx->regmap_cfg.can_sleep = true;
    ctx->regmap_cfg.cache_type = REGCACHE_NONE;
    ctx->regmap = devm_regmap_init_i2c(client, &ctx->regmap_cfg);
    if (IS_ERR(ctx->regmap))
        return PTR_ERR(ctx->regmap);

    ctx->gpiochip.label = client->name;
    ctx->gpiochip.base = -1;
    ctx->gpiochip.owner = THIS_MODULE;
    ctx->gpiochip.can_sleep = true;
    ctx->gpiochip.ngpio = NUM_OF_GPIOS;
    ctx->gpiochip.get = mcp23008_get_value;
    ctx->gpiochip.set = mcp23008_set_value;
    ctx->gpiochip.direction_input = mcp23008_direction_input;
    ctx->gpiochip.direction_output = mcp23008_direction_output;
    status = devm_gpiochip_add_data(&client->dev, &ctx->gpiochip, ctx);
    if (status)
        return status;

    dev_info(&client->dev, "mcp23008 gpio expander inserted\n");

    return 0;
}

static void mcp23008_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "mcp23008 gpio expander removed\n");
}

module_i2c_driver(mcp23008_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maciej Rutkowski");
MODULE_DESCRIPTION("Driver for MCP23008 GPIO expander");