// SPDX‑License‑Identifier: GPL‑2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pwm.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/slab.h>

struct pwm_backlight_data {
	struct pwm_device *pwm;
	unsigned int period_ns;
	int cur_level;
	struct kobject *bk_kobj;
};

static struct pwm_backlight_data *g_data;

static const unsigned int duty_table[] = {0,5000,10000,15000,20000,25000};

static ssize_t brightness_level_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int level, ret;
	unsigned int duty_ns;

	ret = kstrtoint(buf, 10, &level);
	if (ret)
		return -EINVAL;
	if (level < 0 || level > 5)
		return -EINVAL;

	g_data->cur_level = level;
	if (level == 0) {
		pwm_disable(g_data->pwm);
	} else {
		duty_ns = duty_table[level];
		pwm_config(g_data->pwm, duty_ns, g_data->period_ns);
		pwm_enable(g_data->pwm);
	}
	return count;
}

static ssize_t brightness_level_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_data->cur_level);
}

static struct kobj_attribute backlight_attr =
__ATTR(brightness_level, 0644, brightness_level_show, brightness_level_store);

static struct attribute *bk_attrs[] = {
	&backlight_attr.attr,
	NULL,
};
static const struct attribute_group bk_attr_group = {
	.attrs = bk_attrs,
};

static int __init pwm_backlight_init(void)
{
	int ret;
	g_data = kzalloc(sizeof(*g_data), GFP_KERNEL);
	if (!g_data)
		return -ENOMEM;

	g_data->pwm = pwm_get(NULL, "pwm2");
	if (IS_ERR(g_data->pwm)) {
		pr_err("pwm2 get failed\n");
		ret = PTR_ERR(g_data->pwm);
		kfree(g_data);
		return ret;
	}

	g_data->period_ns = 25000;
	g_data->cur_level = 0;
	pwm_disable(g_data->pwm);

	g_data->bk_kobj = kobject_create_and_add("rk3506_backlight", kernel_kobj);
	if (!g_data->bk_kobj) {
		pwm_put(g_data->pwm);
		kfree(g_data);
		return -ENOMEM;
	}

	ret = sysfs_create_group(g_data->bk_kobj, &bk_attr_group);
	if (ret) {
		kobject_put(g_data->bk_kobj);
		pwm_put(g_data->pwm);
		kfree(g_data);
		return ret;
	}

	pr_info("pwm_backlight module load ok\n");
	return 0;
}

static void __exit pwm_backlight_exit(void)
{
	if (!g_data)
		return;
	sysfs_remove_group(g_data->bk_kobj, &bk_attr_group);
	kobject_put(g_data->bk_kobj);
	pwm_disable(g_data->pwm);
	pwm_put(g_data->pwm);
	kfree(g_data);
	pr_info("pwm_backlight module unload\n");
}

module_init(pwm_backlight_init);
module_exit(pwm_backlight_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rk3506");
MODULE_DESCRIPTION("RK3506 pwm backlight driver, no dts node required");
