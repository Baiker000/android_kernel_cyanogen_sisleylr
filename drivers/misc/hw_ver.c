//
// hw_ver.c
//
// Drivers for hw version detected.
//

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/err.h>

#include <linux/qpnp/qpnp-adc.h>
//#include <mach/gpiomux.h>

static struct hw_ver_pdata *hw_pdata = NULL;

struct hw_ver_pdata {
	unsigned int	ver_gpio0;
	unsigned int	ver_gpio1;
};

int get_hw_id(void)
{
    int ver_gpio0 = -1, ver_gpio1 = -1;
    if((hw_pdata != NULL) && (hw_pdata->ver_gpio0 != 0) && (hw_pdata->ver_gpio1 != 0)) {
        ver_gpio0 = gpio_get_value(hw_pdata->ver_gpio0);
        ver_gpio1 = gpio_get_value(hw_pdata->ver_gpio1);
    } else {
        return -1;
    }
    pr_info("get_hw_id ver_gpio0 = %d, ver_gpio1 = %d\n", ver_gpio0, ver_gpio1);

    //dvt2
    if((ver_gpio0 == 1) && (ver_gpio1 == 0)) {
        return 1;
    //pvt
    } else if((ver_gpio0 == 0) && (ver_gpio1 == 1)) {
        return 2;
    //dvt1
    } else {
        return 0;
    }
}

EXPORT_SYMBOL(get_hw_id);
    
static ssize_t show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hw_ver_pdata *pdata;
	unsigned int ver_gpio0, ver_gpio1;
	unsigned int hw_ver = 0;
	pdata = dev_get_drvdata(dev);

	ver_gpio0 = gpio_get_value(pdata->ver_gpio0);
	ver_gpio1 = gpio_get_value(pdata->ver_gpio1);
	pr_info("ver_gpio0 = %d, ver_gpio1 = %d\n", ver_gpio0, ver_gpio1);

	hw_ver = (ver_gpio0 & 0x1);
	hw_ver |= (ver_gpio1 & 0x1) << 4;

	return sprintf(buf, "%02x", hw_ver);
}


static DEVICE_ATTR(version, S_IRUGO, show_version, NULL);

static struct attribute *hw_ver_attrs[] = {
	&dev_attr_version.attr,
	NULL,
};

static struct attribute_group hw_ver_attr_group = {
	.attrs = hw_ver_attrs,
};
/*
static struct gpiomux_setting gpio_hw_ver_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	//.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config hw_ver_det_config[2];
*/
static int hw_ver_probe(struct platform_device *pdev)
//static int __devinit hw_ver_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct hw_ver_pdata *pdata;
	int rc = 0;

	node = pdev->dev.of_node;
pr_err("hw_ver_probe\n");
	pdata = kzalloc(sizeof(struct hw_ver_pdata), GFP_KERNEL);
	hw_pdata = pdata;
	if (!pdata) {
		pr_err( "%s: Can't allocate qpnp_tm_chip\n", __func__);
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, pdata);

	if (of_gpio_count(node) < 2)
		goto free_data;
 
	pdata->ver_gpio0 = of_get_gpio(node, 0);
	pdata->ver_gpio1 = of_get_gpio(node, 1);

	if (!gpio_is_valid(pdata->ver_gpio0) || !gpio_is_valid(pdata->ver_gpio1)) {
		pr_err("%s: invalid GPIO pins, ver_gpio0=%d/ver_gpio1=%d\n",
		       node->full_name, pdata->ver_gpio0, pdata->ver_gpio1);
		goto free_data;
	}
	pr_info("hw_ver: ver_gpio0 = %d, ver_gpio1 = %d\n", pdata->ver_gpio0, pdata->ver_gpio1);

	//hw_ver_det_config[0].gpio = pdata->ver_gpio0;
	//hw_ver_det_config[1].gpio = pdata->ver_gpio1;
	//hw_ver_det_config[0].settings[GPIOMUX_ACTIVE] = &gpio_hw_ver_config;
  	//hw_ver_det_config[1].settings[GPIOMUX_ACTIVE] = &gpio_hw_ver_config;  
	//msm_gpiomux_install(hw_ver_det_config, ARRAY_SIZE(hw_ver_det_config));

	rc = gpio_request(pdata->ver_gpio0, "ver_gpio0");
	if (rc)
		goto err_request_gpio0;
	rc = gpio_request(pdata->ver_gpio1, "ver_gpio1");
	if (rc)
		goto err_request_gpio1;

	gpio_direction_input(pdata->ver_gpio0);
	gpio_direction_input(pdata->ver_gpio1);

	rc = sysfs_create_group(&pdev->dev.kobj, &hw_ver_attr_group);
	if (rc) {
		pr_err("Unable to create sysfs for hw_ver, errors: %d\n", rc);
		goto err_sysfs_create;
	}

	return 0;

err_sysfs_create:
	gpio_free(pdata->ver_gpio1);
err_request_gpio1:
	gpio_free(pdata->ver_gpio0);
err_request_gpio0:
free_data:
	kfree(pdata);
	return rc;
}

static int hw_ver_remove(struct platform_device *pdev)
{
	struct hw_ver_pdata *pdata;

	pdata = dev_get_drvdata(&pdev->dev);

	gpio_free(pdata->ver_gpio0);
	gpio_free(pdata->ver_gpio1);

	sysfs_remove_group(&pdev->dev.kobj, &hw_ver_attr_group);

	if(pdata != NULL)
		kfree(pdata);

	return 0;
}

static struct of_device_id hw_ver_of_match[] = {
	{ .compatible = "hw-version", },
	{ },
};

static struct platform_driver hw_ver_device_driver = {
	.probe		= hw_ver_probe,
	.remove		= hw_ver_remove,
	.driver		= {
		.name	= "hw-version",
		.owner	= THIS_MODULE,
		.of_match_table = hw_ver_of_match,
	}
};

static int __init hw_ver_init(void)
{
pr_err("hw_ver_init\n");
	return platform_driver_register(&hw_ver_device_driver);
}

static void __exit hw_ver_exit(void)
{
	platform_driver_unregister(&hw_ver_device_driver);
}

arch_initcall(hw_ver_init);
module_exit(hw_ver_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Cheng Xuetao <chengxta@lenovo.com>");
MODULE_DESCRIPTION("Drivers for HW Version detected");
MODULE_ALIAS("platform:hw-version");

// end of file
