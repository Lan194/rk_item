// SPDX?License?Identifier: GPL?2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

struct buzzer_data {
	struct gpio_desc *gpiod;
	struct kobject *bz_kobj;
};
static struct buzzer_data *g_data;

static ssize_t enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val, ret;

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		return -EINVAL;

	if (val == 1)
		gpiod_set_value(g_data->gpiod, 1);
	else if (val == 0)
		gpiod_set_value(g_data->gpiod, 0);
	else
		return -EINVAL;
	return count;
}

static ssize_t enable_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int v = gpiod_get_value(g_data->gpiod);
	return sprintf(buf, "%d\n", v);
}

static struct kobj_attribute buzzer_attr =
__ATTR(enable, 0644, enable_show, enable_store);

static struct attribute *bz_attrs[] = {
	&buzzer_attr.attr,
	NULL,
};
static const struct attribute_group bz_attr_group = {
	.attrs = bz_attrs,
};

static int __init gpio_buzzer_init(void)
{
	struct device_node *np;
	int ret;

	g_data = kzalloc(sizeof(*g_data), GFP_KERNEL);
	if (!g_data)
		return -ENOMEM;

	np = of_find_compatible_node(NULL, NULL, "vanxoak,rk3506-env");
	if (!np) {
		pr_err("can not find vanxoak,rk3506-env dts node\n");
		ret = -ENODEV;
		kfree(g_data);
		return ret;
	}

	g_data->gpiod = devm_gpiod_get_from_of_node(NULL, np, "buzzer-gpios", 0, GPIOD_OUT_LOW, "buzzer");
	of_node_put(np);

	if (IS_ERR(g_data->gpiod)) {
		pr_err("get buzzer?gpios failed\n");
		ret = PTR_ERR(g_data->gpiod);
		kfree(g_data);
		return ret;
	}

	g_data->bz_kobj = kobject_create_and_add("rk3506_buzzer", kernel_kobj);
	if (!g_data->bz_kobj) {
		kfree(g_data);
		return -ENOMEM;
	}

	ret = sysfs_create_group(g_data->bz_kobj, &bz_attr_group);
	if (ret) {
		kobject_put(g_data->bz_kobj);
		kfree(g_data);
		return ret;
	}

	pr_info("gpio_buzzer module load ok, use dts rk3506?env buzzer?gpios\n");
	return 0;
}

static void __exit gpio_buzzer_exit(void)
{
	if (!g_data)
		return;
	sysfs_remove_group(g_data->bz_kobj, &bz_attr_group);
	kobject_put(g_data->bz_kobj);
	devm_gpiod_put(NULL, g_data->gpiod);
	kfree(g_data);
	pr_info("gpio_buzzer module unload\n");
}

module_init(gpio_buzzer_init);
module_exit(gpio_buzzer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rk3506");
MODULE_DESCRIPTION("RK3506 gpio buzzer driver, reuse existing dts rk3506?env node");
