/*
 *  drivers/power/rt9428_battery.c
 *  Driver to Richtek RT9428 Fuelgauge IC
 *
 *  Copyright (C) 2014 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#define CONFIG_ANDROID_ALARM_ACTIVATED 0

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/ktime.h>
#if CONFIG_ANDROID_ALARM_ACTIVATED
#include <linux/android_alarm.h>
#else
#include <linux/alarmtimer.h>
#endif /* #if 1 CONFIG_ANDROID_ALARM_ACTIVATED */
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

#include <linux/power/rt9428_battery.h>

#include <linux/time.h>
#include <linux/delay.h>

#define RT9428_DEVICE_NAME "rt9428"
/*
#define RT9428_REG_VBATH 		0x02
#define RT9428_REG_SOCH		0x04
#define RT9428_REG_CFG0		0x0c
#define RT9428_REG_CFG1		0x0d
#define RT9428_REG_MFAH		0xfe
#define RT9428_REG_DEVID		0x09

#define RT9428_SOCALRT_MASK	0x20//need to be modify

#define RT9428_NORMAL_POLL 	10
*/
struct rt9428_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct rt9428_platform_data *pdata;
	struct power_supply *batt_psy;
	struct alarm polling_alarm;
	struct power_supply fg_psy;
	struct delayed_work dwork;
	struct mutex io_lock;
	struct mutex var_lock;
	struct wake_lock mon_wakelock;
	struct miscdevice rt9428_io_misc;
	ktime_t last_poll_time;
	int alert_irq;
	int last_capacity;
	int capacity;
	int last_vcell;
	int vcell;
	unsigned char suspend:1;
	unsigned char online:1;
	unsigned char reg_addr;
	unsigned int reg_data;
};

struct rt9428_io_desc {
	unsigned char id[2];
	// unsigned short vgcomp0;
	unsigned int vgpara1;
 	unsigned int vgpara2;
	unsigned int vgpara3;
	unsigned int vgpara4;
	unsigned int vgpara5;  
	int r1_gain_tempcold;
	int r1_gain_temphot; 
	int r1_gain_tempcold2;
	int r2_gain_tempcold;
	int r2_gain_temphot; 
	int r2_gain_tempcold2;
	int r3_gain_tempcold;
	int r3_gain_temphot; 
	int r3_gain_tempcold2;
	int r4_gain_tempcold;
	int r4_gain_temphot; 
	int r4_gain_tempcold2;
	unsigned int temp_base;
	unsigned int high_temp_base;
	unsigned int low_temp_base;
	unsigned int low_temp2_base;
	int soc_comp;
};

static unsigned char rt9428_init_regval[] = {
	0x5F, //0x0D
};

static char *rtdef_fg_name = "ext-fg";

static char *rt_fg_supply_list[] = {
	"none",
};

static enum power_supply_property rt_fg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CAPACITY,
};

static inline int rt9428_read_device(struct i2c_client *i2c,
		int reg, int bytes, void *dest)
{
	int ret;
	if (bytes > 1)
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, bytes, dest);
	else {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret < 0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}
	return ret;
}

//default read one byte
#if 0
/*default write one byte*/
static int rt9428_reg_write(struct i2c_client *i2c, int reg, unsigned char data)
{
	struct rt9428_chip* chip = i2c_get_clientdata(i2c);
	int ret;
	pr_debug("I2C Write (client : 0x%x) reg = 0x%x, data = 0x%x\n",
			(unsigned int)i2c,(unsigned int)reg,(unsigned int)data);
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, data);
	mutex_unlock(&chip->io_lock);
	return ret;
}
#endif /* #if 0 */

static int rt9428_reg_read_word(struct i2c_client *i2c, int reg)
{
	struct rt9428_chip* chip = i2c_get_clientdata(i2c);
	int ret;
//	dump_stack();
	
	pr_debug("rt9428 I2C Read (client : 0x%x) reg = 0x%x\n",
			(unsigned int)i2c,(unsigned int)reg);
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&chip->io_lock);
	if (ret<0)
		dev_err(chip->dev, "read reg 0x%x io fail\n", reg);
	return (ret<0)?ret:swab16(ret);
}

static int rt9428_reg_write_word(struct i2c_client *i2c, int reg, unsigned int data)
{
	struct rt9428_chip* chip = i2c_get_clientdata(i2c);
	int ret;
	pr_debug(" rt9428 I2C Write (client : 0x%x) reg = 0x%x, data = 0x%x\n",
			(unsigned int)i2c,(unsigned int)reg,(unsigned int)data);
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, swab16(data));
	mutex_unlock(&chip->io_lock);
	return ret;
}

#if 0
static int rt9428_assign_bits(struct i2c_client *i2c, int reg,
		unsigned char mask, unsigned char data)
{
	struct rt9428_chip *chip = i2c_get_clientdata(i2c);
	unsigned char value;
	int ret;
	mutex_lock(&chip->io_lock);
	ret = rt9428_read_device(i2c, reg, 1, &value);
	if (ret < 0)
		goto out;
	value &= ~mask;
	value |= (data&mask);
	ret = i2c_smbus_write_byte_data(i2c,reg,value);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}

#if 0
static int rt9428_set_bits(struct i2c_client *i2c, int reg,
		unsigned char mask)
{
	return rt9428_assign_bits(i2c,reg,mask,mask);
}
#endif /* #if 0 */

static int rt9428_clr_bits(struct i2c_client *i2c, int reg,
		unsigned char mask)
{
	return rt9428_assign_bits(i2c,reg,mask,0);
}
#endif /* #if 0 */

/*
static ssize_t rt_fg_show_attrs(struct device *, struct device_attribute *, char *);
static ssize_t rt_fg_store_attrs(struct device *, struct device_attribute *, const char *, \
		size_t count);

#define RT_FG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},		\
	.show = rt_fg_show_attrs,			\
	.store = rt_fg_store_attrs,			\
}

static struct device_attribute rt_fuelgauge_attrs[] = {
	RT_FG_ATTR(reg),
	RT_FG_ATTR(data),
	RT_FG_ATTR(regs),
};

enum {
	FG_REG = 0,
	FG_DATA,
	FG_REGS,
};

static ssize_t rt_fg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rt9428_chip *chip = dev_get_drvdata(dev->parent);
	const ptrdiff_t offset = attr - rt_fuelgauge_attrs;
	int i = 0;
	int j = 0;

	switch (offset) {
	case FG_REG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x\n",
				chip->reg_addr);
		break;
	case FG_DATA:
		chip->reg_data = rt9428_reg_read_word(chip->i2c, chip->reg_addr);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x\n",
				chip->reg_data);
		dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
				__func__, chip->reg_addr, chip->reg_data);
		break;
	case FG_REGS:
		for (j = RT9428_REG_RANGE1_START; j <= RT9428_REG_RANGE1_STOP; j++)
			i += scnprintf(buf + i, PAGE_SIZE - i, "reg%02x 0x%02x\n",
					j, rt9428_reg_read(chip->i2c, j));

		for (j = RT9428_REG_RANGE2_START; j <= RT9428_REG_RANGE2_STOP; j++)
			i += scnprintf(buf + i, PAGE_SIZE - i, "reg%02x 0x%02x\n",
					j, rt9428_reg_read(chip->i2c, j));

		for (j = RT9428_REG_RANGE3_START; j <= RT9428_REG_RANGE3_STOP; j++)
			i += scnprintf(buf + i, PAGE_SIZE - i, "reg%02x 0x%02x\n",
					j, rt9428_reg_read(chip->i2c, j));
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

static ssize_t rt_fg_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct rt9428_chip *chip = dev_get_drvdata(dev->parent);
	const ptrdiff_t offset = attr - rt_fuelgauge_attrs;
	int ret = 0;
	int x = 0;

	switch (offset) {
	case FG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			if (x<RT9428_REG_MAX && \
					((x>=RT9428_REG_RANGE1_START && x<=RT9428_REG_RANGE1_STOP)|| \
					 (x>=RT9428_REG_RANGE2_START && x<=RT9428_REG_RANGE2_STOP)|| \
					 (x>=RT9428_REG_RANGE3_START && x<=RT9428_REG_RANGE3_STOP)))
			{
				chip->reg_addr = x;
				ret = count;
			}
			else
				ret = -EINVAL;
		}
		else
			ret = -EINVAL;
		break;
	case FG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			rt9428_reg_write_word(chip->i2c, chip->reg_addr, x);
			chip->reg_data = x;
			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
					__func__, chip->reg_addr, chip->reg_data);
			ret = count;
		}
		else
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


static int rt_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(rt_fuelgauge_attrs); i++) {
		rc = device_create_file(dev, &rt_fuelgauge_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &rt_fuelgauge_attrs[i]);
create_attrs_succeed:
	return rc;
}
*/

static int rt_fg_get_property(struct power_supply *psy, \
		enum power_supply_property psp, \
		union power_supply_propval *val)
{
	struct rt9428_chip *chip = dev_get_drvdata(psy->dev->parent);
	int rc = 0;
	int regval;
	switch (psp)
	{
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_VBATH);
		if (regval<0)
			rc = -EIO;
		else
		{
			if (!chip->last_vcell)
				chip->last_vcell = chip->vcell = ((regval>>4)*5)>>2;
			val->intval = (regval>>4)*1250/1000;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		//now casually return a constant value
		val->intval = 4350;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_SOCH);
		if (regval<0)
			rc = -EIO;
		else
		{
			if (!chip->last_capacity)
				chip->last_capacity = chip->capacity = (regval*10)>>8;
			if (chip->capacity<100)
			{
				val->intval = (chip->capacity-chip->pdata->soc_comp)*100;
				val->intval /= (100-chip->pdata->soc_comp);
				if (val->intval>0)
					val->intval = DIV_ROUND_UP(val->intval, 10);
				else
					val->intval = 0;
			}
			else
				val->intval = DIV_ROUND_UP(chip->capacity, 10);
			if (val->intval>100)
				val->intval = 100;
		}

		pr_err("rt9428 soc %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->pdata->full_design;
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int rt_fg_set_property(struct power_supply *psy, \
		enum power_supply_property psp, \
		const union power_supply_propval *val)
{
	int rc = 0;
	switch (psp)
	{
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

void rt_fg_program_alarm(struct rt9428_chip *chip, int seconds)
{
#if CONFIG_ANDROID_ALARM_ACTIVATED
	ktime_t low_interval = ktime_set(seconds, 0);
	ktime_t slack = ktime_set(10, 0);
	ktime_t next;

	next = ktime_add(chip->last_poll_time, low_interval);
	alarm_start_range(&chip->polling_alarm, next, ktime_add(next, slack));
#else
	alarm_start(&chip->polling_alarm, \
			ktime_add(chip->last_poll_time, ktime_set(seconds, 0)));
#endif /* #if 1 //CONFIG_ANDROID_ALARM_ACTIVATED */

}


static void rt9428_update_info(struct rt9428_chip *chip)
{
	int btemp, original_btemp;
	int regval;
	struct rt9428_platform_data *pdata = chip->pdata;

	dev_err(chip->dev, "rt9428_update_info\n");

	//always clr ALRT
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_CFG0);
	if(regval>=0)
	{
		regval &= (~RT9428_SOCALRT_MASK);
		rt9428_reg_write_word(chip->i2c, RT9428_REG_CFG0, regval);
	}else
		dev_err(chip->dev, "read cfg error %d", regval);
	
	//get battery voltage store to chip->vcell
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_VBATH);
	if (regval<0)
	{
		dev_err(chip->dev, "read vcell fail\n");
		return;
	}
	
	chip->vcell = ((regval>>4)*5)>>2;
	dev_err(chip->dev, "%s, vcell 0x%x %d\n", __func__, regval, chip->vcell);
	
	//get battery temp from battery power supply
	
#if 1
	{
		union power_supply_propval pval;
		if (!chip->batt_psy)
		{
			chip->batt_psy = power_supply_get_by_name("battery");
			if (!chip->batt_psy)
			{
				dev_err(chip->dev, "get battery psy fail. Using default temp value\n");
				btemp = 250;
			}
		}
		if(chip->batt_psy)
		{
			if (chip->batt_psy->get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval)<0)
			{
				dev_err(chip->dev, "get batt temp fail.  Using default temp value\n");
				btemp = 250;
			}
			btemp = pval.intval;
		}
	}
#else
//empty
#endif
	btemp/=10;
	original_btemp = btemp;
	
	// adjust temp for calculation vgcomp
	if (original_btemp < (int)pdata->low_temp2_base)
		btemp = 5;
	else if (original_btemp >= (int)pdata->high_temp_base)
		btemp = 50;
	else if (original_btemp <= (int)pdata->low_temp_base)
		btemp = 5;
	else
		btemp = 25;
	dev_err(chip->dev, "original_temp=%d, btemp=%d\n", original_btemp, btemp);

	mutex_lock(&chip->var_lock);
	//adjust vgcom
	{ 
		s32 temp_para1 = 0;
		s32 temp_para2 = 0;
		s32 temp_para3 = 0;
		s32 temp_para4 = 0;
		u16 data = 0;
		s32 temp_diff = (btemp>pdata->temp_base)?btemp-pdata->temp_base: \
				pdata->temp_base-btemp;
		/* Read RCOMP applied*/
		temp_para1 = (pdata->vgpara1) & 0x00ff;	
		temp_para2 = (pdata->vgpara2) & 0x00ff;	
		temp_para3 = (pdata->vgpara3) & 0x00ff;
		temp_para4 = (pdata->vgpara4) & 0x00ff;

		/* Calculate RCOMP by temperature*/
		if (btemp <= pdata->low_temp2_base)
		{
			temp_para1 = temp_para1 + temp_diff \
				* pdata->r1_gain_tempcold2/100;
			temp_para2 = temp_para2 + temp_diff \
				* pdata->r2_gain_tempcold2/100;
			temp_para3 = temp_para3 + temp_diff \
				* pdata->r3_gain_tempcold2/100;
			temp_para4 = temp_para4 + temp_diff \
				* pdata->r4_gain_tempcold2/100;
		}
		else if (btemp >= pdata->high_temp_base)
		{
			temp_para1 = temp_para1 + temp_diff \
				* pdata->r1_gain_temphot/100;
			temp_para2 = temp_para2 + temp_diff \
				* pdata->r2_gain_temphot/100;
			temp_para3 = temp_para3 + temp_diff \
				* pdata->r3_gain_temphot/100;
			temp_para4 = temp_para4 + temp_diff \
				* pdata->r4_gain_temphot/100;
		}
		else 
		{
			temp_para1 = temp_para1 + temp_diff \
				* pdata->r1_gain_tempcold/100;
			temp_para2 = temp_para2 + temp_diff \
				* pdata->r2_gain_tempcold/100;
			temp_para3 = temp_para3 + temp_diff \
				* pdata->r3_gain_tempcold/100;
			temp_para4 = temp_para4 + temp_diff \
				* pdata->r4_gain_tempcold/100;
		}

		pr_err("0x%x, 0x%x\n", temp_diff, pdata->r2_gain_tempcold);
		/* RCOMP limitation*/
		if (temp_para1 > 0xFF)
			temp_para1 = 0xFF;
		else if (temp_para1 < 0)
			temp_para1 = 0;
		if (temp_para2 > 0xFF)
			temp_para2 = 0xFF;
		else if (temp_para2 < 0)
			temp_para2 = 0;
		if (temp_para3 > 0xFF)
			temp_para3 = 0xFF;
		else if (temp_para3 < 0)
			temp_para3 = 0;
		if (temp_para4 > 0xFF)
			temp_para4 = 0xFF;
		else if (temp_para4 < 0)
			temp_para4 = 0;

		pr_err("%s : temp_para2 = 0x%x, 0x%x\n", __func__, temp_para2, pdata->vgpara2);
		
		/* Write RCOMP */
		data = (pdata->vgpara1 & 0xff00) + temp_para1;
		pr_info("%s : VG1_before_write = 0x%04x\n", __func__, data);
		if (rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, data)<0)
			dev_err(chip->dev, "%s: failed to write VG1\n", __func__);
	/*	
		        /// read vg1
		        msleep(50);
	        rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, 0x8081);
				        msleep(50);

	        regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_MFAH);
	        pr_info("%s : VG1_after_write = 0x%04x\n", __func__, regval);
	*/	
		data = (pdata->vgpara2 & 0xff00) + temp_para2;
		        pr_err("%s : VG2_before_write = 0x%04x\n", __func__, data);
		if (rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, data)<0)
			dev_err(chip->dev, "%s: failed to write VG2\n", __func__);
		        /// read vg2
		        rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, 0x8082);
		        regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_MFAH);
		        pr_err("%s : VG2_after_write = 0x%04x\n", __func__, regval);
		
		data = (pdata->vgpara3 & 0xff00) + temp_para3;
		        /// check vg3
		        pr_info("%s : VG3_before_write = 0x%04x\n", __func__, data);
		if (rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, data)<0)
			dev_err(chip->dev, "%s: failed to write VG3\n", __func__);
	/*	        /// read vg3
		        rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, 0x8083);
		        regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_MFAH);
		        pr_info("%s : VG3_after_write = 0x%04x\n", __func__, regval);
	*/	
		data = (pdata->vgpara4 & 0xff00) + temp_para4;	
		        /// check vg4
		        pr_info("%s : VG4_before_write = 0x%04x\n", __func__, data);
		if (rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, data)<0)
			dev_err(chip->dev, "%s: failed to write VG4\n", __func__);
	/*	        /// read vg4
		        rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, 0x8084);
		        regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_MFAH);
		        pr_info("%s : VG4_after_write = 0x%04x\n", __func__, regval);
	*/	
		data = pdata->vgpara5;
		if ((original_btemp < pdata->low_temp_base) && (original_btemp >= pdata->low_temp2_base))
			data = (pdata->vgpara5 & 0xff01) + 0x02;
		        /// check vg5 
		        pr_info("%s : VG5_before_write = 0x%04x\n", __func__, data);
		if (rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, data)<0)
			dev_err(chip->dev, "%s: failed to write VG5\n", __func__);
	/*	        /// read vg5
		        rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, 0x8085);
		        regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_MFAH);
		        pr_info("%s : VG5_after_write = 0x%04x\n", __func__, regval);
*/	}
	mutex_unlock(&chip->var_lock);
	//get battery soc and store to chip->capacity
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_SOCH);
	if (regval<0)
	{
		dev_err(chip->dev, "fail to read soc value\n");
		goto end;
	}
	else
	{
		chip->capacity = (regval*10)>>8;
		pr_err("rt9428 chip->last_capacity %d, chip->capacity %d\n", chip->last_capacity, chip->capacity);
		if (chip->last_capacity!=chip->capacity ||\
				abs(chip->last_vcell-chip->vcell)>50)
		{
			chip->last_capacity = chip->capacity;
			chip->last_vcell = chip->vcell;
			if (!chip->batt_psy)
			{
				chip->batt_psy = power_supply_get_by_name("battery");
				if (!chip->batt_psy)
				{
					dev_err(chip->dev, "still can't get battery psy\n");
					//power_supply_changed(&chip->fg_psy);					
					goto end;
				}
			}
			power_supply_changed(chip->batt_psy);
		}
	}
end:	
	dev_err(chip->dev, "vcell=%d, soc=%d, temp=%d\n", chip->vcell, chip->capacity, original_btemp);
	
#if CONFIG_ANDROID_ALARM_ACTIVATED
	chip->last_poll_time = alarm_get_elapsed_realtime();
	alarm_cancel(&chip->polling_alarm);
#else
	chip->last_poll_time = ktime_get_boottime();
	alarm_cancel(&chip->polling_alarm);
#endif /* #if 1 CONFIG_ANDROID_ALARM_ACTIVATED */
	rt_fg_program_alarm(chip, RT9428_NORMAL_POLL);
	wake_unlock(&chip->mon_wakelock);
}

static irqreturn_t rt9428_irq_handler(int irqno, void *param)
{
	struct rt9428_chip *chip = (struct rt9428_chip *)param;
	pr_err("%s \n", __func__);	
	if (chip->suspend)
	{
		schedule_delayed_work(&chip->dwork, msecs_to_jiffies(500));
		goto intr_end;
	}
	rt9428_update_info(chip);
intr_end:
	return IRQ_HANDLED;
}

static void rt9428_dwork_func(struct work_struct *work)
{
	struct rt9428_chip *chip = (struct rt9428_chip *)container_of(work, struct rt9428_chip, dwork.work);
	
	rt9428_update_info(chip);
}

#if CONFIG_ANDROID_ALARM_ACTIVATED
static void rt_fg_expired_timer_func(struct alarm *alarm)
#else
static enum alarmtimer_restart rt_fg_expired_timer_func(
		struct alarm *alarm, ktime_t now)

#endif /* #if 1 CONFIG_ANDROID_ALARM_ACTIVATED */
{
	struct rt9428_chip *chip = container_of(alarm, struct rt9428_chip, polling_alarm);
	wake_lock(&chip->mon_wakelock);
	schedule_delayed_work(&chip->dwork, 0);
	pr_debug("Alarm activated\n");
#if CONFIG_ANDROID_ALARM_ACTIVATED
	//empty
#else
	return ALARMTIMER_NORESTART;
#endif /* if 0 CONFIG_ANDROID_ALARM_ACTIVATED */
}

static ssize_t rt9428_io_read(struct file *filp, char __user *buf,
			      size_t buf_size, loff_t *ppos)
{
	struct rt9428_chip *chip = container_of(filp->private_data, \
		struct rt9428_chip, rt9428_io_misc);
	struct rt9428_io_desc *rid;
	int ret = 0;
	dev_info(chip->dev, "%s bufsize = %d\n", __func__, buf_size);
	rid = devm_kzalloc(chip->dev, sizeof(*rid), GFP_KERNEL);
	if (!rid)
	{
		dev_err(chip->dev, "err allocation for buf\n");
		return -ENOMEM;
	}
	if (buf_size<sizeof(*rid))
	{
		dev_err(chip->dev, "buf size is smaller than io_desc\n");
		ret = -EINVAL;
		goto err_read;
	}
	if (*ppos != 0)
		goto err_read;

	rid->id[0] = 'r';
	rid->id[1] = 't';
	rid->vgpara1 = chip->pdata->vgpara1;
	rid->vgpara2 = chip->pdata->vgpara2;
	rid->vgpara3 = chip->pdata->vgpara3;
	rid->vgpara4 = chip->pdata->vgpara4;
	rid->vgpara5 = chip->pdata->vgpara5;
	rid->r1_gain_tempcold = chip->pdata->r1_gain_tempcold;
	rid->r1_gain_temphot = chip->pdata->r1_gain_temphot;
	rid->r1_gain_tempcold2 = chip->pdata->r1_gain_tempcold2;
	rid->r2_gain_tempcold = chip->pdata->r2_gain_tempcold;
	rid->r2_gain_temphot = chip->pdata->r2_gain_temphot;
	rid->r2_gain_tempcold2 = chip->pdata->r2_gain_tempcold2;
	rid->r3_gain_tempcold = chip->pdata->r3_gain_tempcold;
	rid->r3_gain_temphot = chip->pdata->r3_gain_temphot;
	rid->r3_gain_tempcold2 = chip->pdata->r3_gain_tempcold2;
	rid->r4_gain_tempcold = chip->pdata->r4_gain_tempcold;
	rid->r4_gain_temphot = chip->pdata->r4_gain_temphot;
	rid->r4_gain_tempcold2 = chip->pdata->r4_gain_tempcold2;
	rid->temp_base = chip->pdata->temp_base;
	rid->high_temp_base = chip->pdata->high_temp_base;
	rid->low_temp_base = chip->pdata->low_temp_base;
	rid->low_temp2_base = chip->pdata->low_temp2_base;
	rid->soc_comp = chip->pdata->soc_comp;
	if (copy_to_user(buf, rid, sizeof(*rid))>0)
	{
		dev_err(chip->dev, "copy to user buf fail\n");
		ret = -EINVAL;
		goto err_read;
	}
	*ppos = sizeof(*rid);
	ret = *ppos;
err_read:
	devm_kfree(chip->dev, rid);
	return ret;
}

static ssize_t rt9428_io_write(struct file *filp, const char __user *buf, size_t buf_size, \
	loff_t *offset)
{
	struct rt9428_chip *chip = container_of(filp->private_data, \
		struct rt9428_chip, rt9428_io_misc);
	struct rt9428_io_desc *rid;
	int ret = 0;
	dev_info(chip->dev, "%s bufsize = %d\n", __func__, buf_size);
	rid = devm_kzalloc(chip->dev, sizeof(*rid), GFP_KERNEL);
	if (!rid)
	{
		dev_err(chip->dev, "no memory can be allocated\n");
		return -ENOMEM;
	}
	if (buf_size<sizeof(*rid))
	{
		dev_err(chip->dev, "buf size is smaller than io_desc\n");
		ret = -EINVAL;
		goto err_write;
	}
	if (copy_from_user(rid, buf, sizeof(*rid))>0)
	{
		dev_err(chip->dev, "copy from user buf fail\n");
		ret = -EINVAL;
		goto err_write;
	}
	if (rid->id[0]!='r' || rid->id[1]!='t')
	{
		ret = -EINVAL;
		goto err_write;
	}
	mutex_lock(&chip->var_lock);
	chip->pdata->vgpara1 = rid->vgpara1;
	chip->pdata->vgpara2 = rid->vgpara2;
	chip->pdata->vgpara3 = rid->vgpara3;
	chip->pdata->vgpara4 = rid->vgpara4;
	chip->pdata->vgpara5 = rid->vgpara5;
	chip->pdata->r1_gain_tempcold = rid->r1_gain_tempcold;
	chip->pdata->r1_gain_temphot = rid->r1_gain_temphot;
	chip->pdata->r1_gain_tempcold2 = rid->r1_gain_tempcold2;
	chip->pdata->r2_gain_tempcold = rid->r2_gain_tempcold;
	chip->pdata->r2_gain_temphot = rid->r2_gain_temphot;
	chip->pdata->r2_gain_tempcold2 = rid->r2_gain_tempcold2;
	chip->pdata->r3_gain_tempcold = rid->r3_gain_tempcold;
	chip->pdata->r3_gain_temphot = rid->r3_gain_temphot;
	chip->pdata->r3_gain_tempcold2 = rid->r3_gain_tempcold2;
	chip->pdata->r4_gain_tempcold = rid->r4_gain_tempcold;
	chip->pdata->r4_gain_temphot = rid->r4_gain_temphot;
	chip->pdata->r4_gain_tempcold2 = rid->r4_gain_tempcold2;
	chip->pdata->temp_base = rid->temp_base;
	chip->pdata->high_temp_base = rid->high_temp_base;
	chip->pdata->low_temp_base = rid->low_temp_base;
	chip->pdata->low_temp2_base = rid->low_temp2_base;
	chip->pdata->soc_comp = rid->soc_comp;
	mutex_unlock(&chip->var_lock);
	schedule_delayed_work(&chip->dwork, 0);
	ret = buf_size;
err_write:
	devm_kfree(chip->dev, rid);
	return ret;
}

static int rt9428_io_open(struct inode *inode, struct file *filp)
{
	struct rt9428_chip *chip = container_of(filp->private_data, \
		struct rt9428_chip, rt9428_io_misc);
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9428_io_release(struct inode *inode, struct file *filp)
{
	struct rt9428_chip *chip = container_of(filp->private_data, \
		struct rt9428_chip, rt9428_io_misc);
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static struct file_operations rt9428_misc_fops =
{
        .owner = THIS_MODULE,
        .read = rt9428_io_read,
        .write = rt9428_io_write,
        .open = rt9428_io_open,
        .release = rt9428_io_release,
};


static int rt9428_parse_dt(struct device *dev, struct rt9428_platform_data *pdata)
{
#ifdef CONFIG_OF
	struct device_node *np = dev->of_node;
	u32 prop_array[16];
#if 0
	if (of_property_read_u32_array(np, "rt,vgcomp0", prop_array, 1)<0)
	{
		dev_warn(dev, "no vgcom0 value, using default value\n");
		pdata->vgcomp0 = 0xAB;
	}
	else
		pdata->vgcomp0 = prop_array[0];
#endif /* #if 0 */
	if (of_property_read_u32_array(np, "rt,vgpara", prop_array, 5)<0)
	{
		dev_warn(dev, "no vgpara value, using default value\n");
		pdata->vgpara1 = 0x810F;
		pdata->vgpara2 = 0x8239;
		pdata->vgpara3 = 0x8328;
		pdata->vgpara4 = 0x84A5;
		pdata->vgpara5 = 0x8501;
	}
	else
	{
		pdata->vgpara1 = prop_array[0];
		pdata->vgpara2 = prop_array[1];
		pdata->vgpara3 = prop_array[2];
		pdata->vgpara4 = prop_array[3];
		pdata->vgpara5 = prop_array[4];
	}
	if (of_property_read_u32_array(np, "rt,r1_gain", prop_array, 6)<0)
	{
		dev_warn(dev, "no r1_gain value, using default value\n");
		pdata->r1_gain_tempcold = -50;
		pdata->r1_gain_temphot = 120;
		pdata->r1_gain_tempcold2 = -50; 
	}
	else
	{
		pdata->r1_gain_tempcold = (prop_array[0]?-prop_array[1]:prop_array[1]);
		pdata->r1_gain_temphot = (prop_array[2]?-prop_array[3]:prop_array[3]);
		pdata->r1_gain_tempcold2 = (prop_array[4]?-prop_array[5]:prop_array[5]);
	}
	if (of_property_read_u32_array(np, "rt,r2_gain", prop_array, 6)<0)
	{
		dev_warn(dev, "no r2_gain value, using default value\n");
		pdata->r2_gain_tempcold = 590;
		pdata->r2_gain_temphot = 0;
		pdata->r2_gain_tempcold2 = 590;
	}
	else
	{
		pdata->r2_gain_tempcold = (prop_array[0]?-prop_array[1]:prop_array[1]);
		pdata->r2_gain_temphot = (prop_array[2]?-prop_array[3]:prop_array[3]);
		pdata->r2_gain_tempcold2 = (prop_array[4]?-prop_array[5]:prop_array[5]);
	}
	if (of_property_read_u32_array(np, "rt,r3_gain", prop_array, 6)<0)
	{
		dev_warn(dev, "no r3_gain value, using default value\n");
		pdata->r3_gain_tempcold = -120;
		pdata->r3_gain_temphot = 20;
		pdata->r3_gain_tempcold2 = -120;		
	}
	else
	{
		pdata->r3_gain_tempcold = (prop_array[0]?-prop_array[1]:prop_array[1]);
		pdata->r3_gain_temphot = (prop_array[2]?-prop_array[3]:prop_array[3]);
		pdata->r3_gain_tempcold2 = (prop_array[4]?-prop_array[5]:prop_array[5]);
	}
	if (of_property_read_u32_array(np, "rt,r4_gain", prop_array, 6)<0)
	{
		dev_warn(dev, "no r4_gain value, using default value\n");
		pdata->r4_gain_tempcold = -700;
		pdata->r4_gain_temphot = 320;
		pdata->r4_gain_tempcold2 = -700;
	}
	else
	{
		pdata->r4_gain_tempcold = (prop_array[0]?-prop_array[1]:prop_array[1]);
		pdata->r4_gain_temphot = (prop_array[2]?-prop_array[3]:prop_array[3]);
		pdata->r4_gain_tempcold2 = (prop_array[4]?-prop_array[5]:prop_array[5]);
	}
	if (of_property_read_u32(np, "rt,alert_threshold", &prop_array[0])<0)
	{
		dev_warn(dev, "no alert_threshold value, using default value\n");
		pdata->alert_threshold = 1;
	}
	else
	{
/*		if (prop_array[0] >= RT9428_SOCL_MIN && prop_array[0] <= RT9428_SOCL_MAX)
		{
			pdata->alert_threshold = prop_array[0];
			rt9428_init_regval[0] &= (~RT9428_SOCL_MASK);
			rt9428_init_regval[0] |= ((~prop_array[0]+1)&RT9428_SOCL_MASK);
		}
		else
			dev_err(dev, "alert threshold value is 1, due to out of range (1~32)\n");
*/			
	}
	if (of_property_read_u32(np, "rt,soc_comp", &prop_array[0])<0)
	{
		dev_warn(dev, "no soc_comp value, using default value\n");
		pdata->soc_comp = 0;
	}
	else
		pdata->soc_comp = prop_array[0];
	if (of_property_read_u32_array(np, "rt,temp_base", prop_array, 8)<0)
	{
		dev_warn(dev, "no temp_base value, using default value\n");
		pdata->low_temp2_base = 5;
		pdata->low_temp_base = 5;
		pdata->temp_base = 25;
		pdata->high_temp_base = 50;
		
	}
	else
	{
		pr_err("ww_debug %d, %d, %d, %d, %d, %d, %d, %d\n", prop_array[0], prop_array[1], prop_array[2], prop_array[3], prop_array[4], prop_array[5], prop_array[6], prop_array[7]);
		pdata->low_temp2_base = (prop_array[0]?-prop_array[1]:prop_array[1]);
		pdata->low_temp_base = (prop_array[2]?-prop_array[3]:prop_array[3]);
		pdata->temp_base = (prop_array[4]?-prop_array[5]:prop_array[5]);
		pdata->high_temp_base = (prop_array[6]?-prop_array[7]:prop_array[7]);
	}
	pdata->alert_gpio = of_get_named_gpio(np, "rt,alert_gpio", 0);
	if (of_property_read_u32(np, "rt,full_design", &prop_array[0])<0)
	{
		dev_warn(dev, "no full design value, using default value\n");
		pdata->full_design = 2150;
	}
	else
		pdata->full_design = prop_array[0];
#endif /* #ifdef CONFIG_OF */

	pr_err("'%s: %d, %d, %d, %d, %d, / %d, %d, %d, / %d, %d, %d, / %d, %d, %d, / %d, %d, %d, / %d, %d, / %d, %d, %d, %d, / %d, %d\n", __func__, pdata->vgpara1, pdata->vgpara2, pdata->vgpara3, pdata->vgpara4, pdata->vgpara5, 
		pdata->r1_gain_tempcold, pdata->r1_gain_temphot, pdata->r1_gain_tempcold2, 
		pdata->r2_gain_tempcold, pdata->r2_gain_temphot, pdata->r2_gain_tempcold2,
		pdata->r3_gain_tempcold, pdata->r3_gain_temphot, pdata->r3_gain_tempcold2,	
		pdata->r4_gain_tempcold, pdata->r4_gain_temphot, pdata->r4_gain_tempcold2,	
		pdata->alert_threshold, pdata->soc_comp, 
		pdata->low_temp2_base, pdata->low_temp_base, 	pdata->temp_base, pdata->high_temp_base,
		pdata->alert_gpio, pdata->full_design
		);
	return 0;
}

static int  rt9428_intr_init(struct rt9428_chip *chip)
{
	int rc = 0;
	if (gpio_is_valid(chip->pdata->alert_gpio))
	{
		rc = gpio_request(chip->pdata->alert_gpio, "rt9428_fg_intr");
		if (rc<0)
		{
			dev_err(chip->dev, "gpio request error\n");
			goto err_intr;
		}
		chip->alert_irq = gpio_to_irq(chip->pdata->alert_gpio);
		if (chip->alert_irq<0)
		{
			dev_err(chip->dev, "irq value is not valid\n");
			gpio_free(chip->pdata->alert_gpio);
			rc = -EINVAL;
			goto err_intr;
		}
		rc = devm_request_threaded_irq(chip->dev, chip->alert_irq, NULL, rt9428_irq_handler, \
				IRQF_TRIGGER_FALLING|IRQF_DISABLED|IRQF_ONESHOT, "rt9428_fg_irq", chip);
		if (rc<0)
		{
			dev_err(chip->dev, "irq register failed (%d)\n", rc);
			gpio_free(chip->pdata->alert_gpio);
			chip->alert_irq = -1;
			rc = -EINVAL;
			goto err_intr;
		}
		enable_irq_wake(chip->alert_irq);
	}
	else
	{
		dev_err(chip->dev, "irq io(%d)  is not valid\n", chip->pdata->alert_gpio);
		rc = -EINVAL;
	}
err_intr:
	return rc;
}

static int  rt9428_intr_deinit(struct rt9428_chip *chip)
{
	if (chip->alert_irq >= 0)
	{
		devm_free_irq(chip->dev, chip->alert_irq, chip);
		gpio_free(chip->pdata->alert_gpio);
	}
	return 0;
}

static int rt9428_chip_init(struct rt9428_chip *chip)
{
	int regval = 0;
	//default: sleep 0 alrt 0
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_CFG0);
	regval &= 0xff00;
	regval |= rt9428_init_regval[0];
	
	dev_err(chip->dev, "%s,regval 0x%x \n", __func__, regval);	
	
	rt9428_reg_write_word(chip->i2c, RT9428_REG_CFG0, regval);
	chip->online = 1;

		regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_CFG0);
	dev_err(chip->dev, "%s,regval 0x%x \n", __func__, regval);	
	return 0;
}

static int rt9428_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct rt9428_chip *chip;
	struct rt9428_platform_data *pdata = client->dev.platform_data;
	int regval, ret = 0;
	bool use_dt = client->dev.of_node;

	pr_err("rt9428 prob start\n");

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, " %s : i2c_check_functionality fail\n",
				__func__);
		ret = -EIO;
		goto err_init;
	}

	if (use_dt)
	{
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
		{
			ret = -ENOMEM;
			goto err_init;
		}
		rt9428_parse_dt(&client->dev, pdata);
		chip->pdata = pdata;
	}
	else
	{
		if (!pdata)
		{
			ret = -EINVAL;
			goto err_init;
		}
		chip->pdata = pdata;
	}

	chip->i2c = client;
	chip->dev = &client->dev;
	chip->alert_irq = -1; // set default irq number = -1;
	mutex_init(&chip->var_lock);
	mutex_init(&chip->io_lock);
	INIT_DELAYED_WORK(&chip->dwork, rt9428_dwork_func);
	wake_lock_init(&chip->mon_wakelock, WAKE_LOCK_SUSPEND, "rt_fg_wake");
	i2c_set_clientdata(client, chip);

	regval = rt9428_reg_read_word(client, RT9428_REG_DEVID0);
	if (regval < 0 || regval != 0x0012) {
		dev_err(&client->dev,
			"read device id fail or id is not correct\n");
		ret = -ENODEV;
		goto err_init1;
	}
	/*register power_supply for rt9428 fg */
	chip->fg_psy.name = rtdef_fg_name;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	chip->fg_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
#else
	chip->fg_psy.type = -1;
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) */
	chip->fg_psy.supplied_to = rt_fg_supply_list;
	chip->fg_psy.properties = rt_fg_props;
	chip->fg_psy.num_properties = ARRAY_SIZE(rt_fg_props);
	chip->fg_psy.get_property = rt_fg_get_property;
	chip->fg_psy.set_property = rt_fg_set_property;
	ret = power_supply_register(&client->dev, &chip->fg_psy);
	if (ret<0)
	{
		dev_err(&client->dev, "power supply register fail\n");
		goto err_init1;
	}

	rt9428_chip_init(chip);
	rt9428_intr_init(chip);

#if CONFIG_ANDROID_ALARM_ACTIVATED
	alarm_init(&chip->polling_alarm,
			ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
			rt_fg_expired_timer_func);
#else
	alarm_init(&chip->polling_alarm,
			ALARM_BOOTTIME,
			rt_fg_expired_timer_func);
#endif /* #if 1 CONFIG_ANDROID_ALARM_ACTIVATED */

	chip->rt9428_io_misc.minor = MISC_DYNAMIC_MINOR;
        chip->rt9428_io_misc.name = "rt9428_io";
        chip->rt9428_io_misc.parent = &client->dev;
        chip->rt9428_io_misc.fops = &rt9428_misc_fops;
        ret = misc_register(&chip->rt9428_io_misc);
        if (ret<0)
        {   
                dev_err(&client->dev, "misc register fail\n");
                goto err_psy;
        }   


	//queue update work immediately
	schedule_delayed_work(&chip->dwork, msecs_to_jiffies(5000));

	//rt_fg_create_attrs(chip->fg_psy.dev);
	dev_err(&client->dev, "rt9438 driver successfully loaded\n");
	return 0;
err_psy:
	power_supply_unregister(&chip->fg_psy);
err_init1:
	wake_lock_destroy(&chip->mon_wakelock);
	mutex_destroy(&chip->io_lock);
err_init:
	devm_kfree(&client->dev, chip);
	dev_err(&client->dev, "rt9438 driver loaded failed\n");
	return ret;
}

static int  rt9428_i2c_remove(struct i2c_client *client)
{
	struct rt9428_chip *chip = i2c_get_clientdata(client);
	misc_deregister(&chip->rt9428_io_misc);
	rt9428_intr_deinit(chip);
	power_supply_unregister(&chip->fg_psy);
	wake_lock_destroy(&chip->mon_wakelock);
	return 0;
}

static int rt9428_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct rt9428_chip *chip = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&chip->dwork);
	
	alarm_cancel(&chip->polling_alarm);

	chip->suspend = 1;
	return 0;
}

static int rt9428_i2c_resume(struct i2c_client *client)
{
	struct rt9428_chip *chip = i2c_get_clientdata(client);
	//schedule_delayed_work(&chip->dwork, msecs_to_jiffies(500));	

//	rt_fg_program_alarm(chip, 1);	
	
	chip->suspend = 0;
	return 0;
}

static const struct i2c_device_id rt_i2c_id[] =
{
	{RT9428_DEVICE_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt_i2c_id);

static const struct of_device_id rt_match_table[] = {
	{ .compatible = "rt," RT9428_DEVICE_NAME,},
	{},
};

static struct i2c_driver rt9428_i2c_driver = {
	.driver = {
		.name = RT9428_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = rt_match_table,
	},
	.probe = rt9428_i2c_probe,
	.remove = rt9428_i2c_remove,
	.suspend = rt9428_i2c_suspend,
	.resume = rt9428_i2c_resume,
	.id_table = rt_i2c_id,
};

static int __init rt9428_init(void)
{
	return i2c_add_driver(&rt9428_i2c_driver);
}

static void __exit rt9428_exit(void)
{
	i2c_del_driver(&rt9428_i2c_driver);
}

module_init(rt9428_init);
module_exit(rt9428_exit);

MODULE_AUTHOR("cy_huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("RT9428 Fuelgauge Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RT9428_DRV_VER);
