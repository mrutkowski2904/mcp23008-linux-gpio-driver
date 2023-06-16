#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>

static int device_probe(struct i2c_client *client, const struct i2c_device_id *id);
static void device_remove(struct i2c_client *client);

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
    .probe = device_probe,
    .remove = device_remove,
    .id_table = mcp23008_ids,
    .driver = {
        .name = "custmcp23008",
        .of_match_table = of_match_ptr(mcp23008_dt_match),
    },
};

static int device_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    s32 read_result;
    dev_info(&client->dev, "in probe\n");
    
    /* dummy read to test if properly wired */
    read_result = i2c_smbus_read_byte_data(client, 0x00);
    if(read_result < 0)
        dev_err(&client->dev, "error while reading data\n");
    else
        dev_info(&client->dev, "read ok, value: %d\n", read_result);

    return 0;
}

static void device_remove(struct i2c_client *client)
{

}

module_i2c_driver(mcp23008_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maciej Rutkowski");
MODULE_DESCRIPTION("Driver for MCP23008 GPIO expander");