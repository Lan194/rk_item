// SPDX‑License‑Identifier: GPL‑2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/delay.h>
#include <linux/slab.h>

static struct kobject *tch_kobj;
static struct spi_device *g_spi;

static u16 touch_read_reg(struct spi_device *spi, u8 cmd)
{
	u8 tx[3] = {cmd, 0x00, 0x00};
	u8 rx[3] = {0};
	struct spi_transfer xfer = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = 3,
	};
	struct spi_message msg;
	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	spi_sync(spi, &msg);
	return ((rx[1] << 8) | rx[2]) >> 3 & 0x0fff;
}

static ssize_t x_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (!g_spi)
		return -ENODEV;
	u16 x = touch_read_reg(g_spi, 0x90);
	return sprintf(buf, "%u\n", x);
}

static ssize_t y_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (!g_spi)
		return -ENODEV;
	u16 y = touch_read_reg(g_spi, 0xD0);
	return sprintf(buf, "%u\n", y);
}

static struct kobj_attribute touch_x_attr = __ATTR_RO(x);
static struct kobj_attribute touch_y_attr = __ATTR_RO(y);

static struct attribute *touch_attrs[] = {
	&touch_x_attr.attr,
	&touch_y_attr.attr,
	NULL,
};
static const struct attribute_group touch_attr_group = {
	.attrs = touch_attrs,
};

static int touch_spi_probe(struct spi_device *spi)
{
	int ret;
	g_spi = spi;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = 1000000;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi setup fail\n");
		return ret;
	}

	tch_kobj = kobject_create_and_add("rk3506_touch", kernel_kobj);
	if (!tch_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(tch_kobj, &touch_attr_group);
	if (ret) {
		kobject_put(tch_kobj);
		return ret;
	}
	dev_info(&spi->dev, "spi touch probe ok\n");
	return 0;
}

static void touch_spi_remove(struct spi_device *spi)
{
	sysfs_remove_group(tch_kobj, &touch_attr_group);
	kobject_put(tch_kobj);
	g_spi = NULL;
	dev_info(&spi->dev, "spi touch remove\n");
}

static const struct of_device_id touch_of_match[] = {
	{ .compatible = "rk,spi-touch" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, touch_of_match);

static struct spi_driver touch_spi_driver = {
	.probe = touch_spi_probe,
	.remove = touch_spi_remove,
	.driver = {
		.name = "rk-spi-touch",
		.of_match_table = touch_of_match,
	},
};
module_spi_driver(touch_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rk3506");
MODULE_DESCRIPTION("RK3506 spi touch driver");
