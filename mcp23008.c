#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

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
    struct regmap_config regmap_cfg;
    struct regmap *regmap;
};

static bool mcp23008_is_precious_reg(struct device *dev, unsigned int reg);

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

static bool mcp23008_is_precious_reg(struct device *dev, unsigned int reg)
{
    if ((reg == MCP23008_INTCAP) || (reg == MCP23008_GPIO))
        return true;
    return false;
}

static int mcp23008_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct mcp23008_context *ctx;

    ctx = devm_kzalloc(&client->dev, sizeof(struct mcp23008_context), GFP_KERNEL);
    if (ctx == NULL)
        return -ENOMEM;

    i2c_set_clientdata(client, ctx);

    ctx->regmap_cfg.reg_bits = 8;
    ctx->regmap_cfg.val_bits = 8;
    ctx->regmap_cfg.max_register = MCP23008_OLAT;
    ctx->regmap_cfg.wr_table = &mcp23008_wr_table;
    ctx->regmap_cfg.rd_table = &mcp23008_rd_table;
    ctx->regmap_cfg.precious_reg = mcp23008_is_precious_reg;
    ctx->regmap_cfg.use_single_read = true;
    ctx->regmap_cfg.use_single_write = true;
    ctx->regmap_cfg.can_sleep = true;
    ctx->regmap_cfg.cache_type = REGCACHE_NONE;
    ctx->regmap = devm_regmap_init_i2c(client, &ctx->regmap_cfg);
    if (IS_ERR(ctx->regmap))
        return PTR_ERR(ctx->regmap);

    dev_info(&client->dev, "in probe\n");

    return 0;
}

static void mcp23008_remove(struct i2c_client *client)
{
}

module_i2c_driver(mcp23008_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maciej Rutkowski");
MODULE_DESCRIPTION("Driver for MCP23008 GPIO expander");