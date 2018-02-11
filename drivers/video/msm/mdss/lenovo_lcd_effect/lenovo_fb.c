

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include <linux/msm_mdp.h>

#include "../mdss.h"
#include "../mdss_panel.h"
#include "../mdss_dsi.h"
#include "../mdss_debug.h"

#include "lenovo_fb.h"
#include "../mdss_fb.h"

extern int lenovo_lcd_effect_handle(struct mdss_dsi_ctrl_pdata *ctrl_data,struct hal_panel_ctrl_data *hal_ctrl_data);

ssize_t lenovo_fb_get_lcd_supported_mode(struct device *dev, struct device_attribute *attr, char *buf)
{

	ssize_t ret = 0,i,count;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata=dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	struct hal_panel_ctrl_data hal_ctrl_data;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,panel_data);
	if (ctrl_pdata == NULL) 
	{
		pr_err("[houdz1]mdss_panel_set_cabc fail:(ctrl_pdata == NUL\n");
		ret = -1;
	}
	
	hal_ctrl_data.id =GET_MODE_NUM;
	count = lenovo_lcd_effect_handle(ctrl_pdata,&hal_ctrl_data);

	for (i = 0; i <count; i++) 
	{
		ret += snprintf(buf + ret, PAGE_SIZE, "%s\n", hal_ctrl_data.panel_data.mode[i].name);
	}

	return ret;
}

ssize_t lenovo_fb_get_lcd_supported_effect(struct device *dev, struct device_attribute *attr, char *buf)
{

	ssize_t ret = 0,i,count;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata=dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	struct hal_panel_ctrl_data hal_ctrl_data;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,panel_data);
	if (ctrl_pdata == NULL) 
	{
		pr_err("[houdz1]mdss_panel_set_cabc fail:(ctrl_pdata == NUL\n");
		ret = -1;
	}
	
	hal_ctrl_data.id =GET_EFFECT_NUM;
	count = lenovo_lcd_effect_handle(ctrl_pdata,&hal_ctrl_data);

	for (i = 0; i <count; i++) 
	{
		ret += snprintf(buf + ret, PAGE_SIZE, "%s\n", hal_ctrl_data.panel_data.effect[i].name);
	}

	return ret;
}

ssize_t lenovo_fb_get_mode(struct device *dev, struct device_attribute *attr,char *buf)
{
	ssize_t ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata=dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	int level;
	struct hal_panel_ctrl_data hal_ctrl_data;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,panel_data);
	if (ctrl_pdata == NULL) 
	{
		pr_err("[houdz1]mdss_panel_set_cabc fail:(ctrl_pdata == NUL\n");
		ret = -1;
	}
	
	hal_ctrl_data.id =GET_MODE;
	level = lenovo_lcd_effect_handle(ctrl_pdata,&hal_ctrl_data);

	pr_err("[houdz1]%s:mode =%d,name = %s\n",__func__,level,hal_ctrl_data.panel_data.mode[level].name);
	
	ret = snprintf(buf, PAGE_SIZE, "%s\n", hal_ctrl_data.panel_data.mode[level].name);
	return ret;
}

ssize_t lenovo_fb_set_mode(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	ssize_t ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata=dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	unsigned long data;
	struct hal_panel_ctrl_data hal_ctrl_data;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,panel_data);
	if (ctrl_pdata == NULL) 
	{
		pr_err("[houdz1]mdss_panel_set_cabc fail:(ctrl_pdata == NUL\n");
		ret = -1;
	}
	

	if (kstrtoul(buf, 0, &data))  return -EINVAL;

	
	hal_ctrl_data.mode = data; 
	hal_ctrl_data.id =SET_MODE;
	ret= lenovo_lcd_effect_handle(ctrl_pdata,&hal_ctrl_data);
	return count;
}


static int g_effect_index =0;

ssize_t lenovo_fb_get_effect(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata=dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	//unsigned long index;
	int level;
	struct hal_panel_ctrl_data hal_ctrl_data;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,panel_data);
	if (ctrl_pdata == NULL) 
	{
		pr_err("[houdz1]mdss_panel_set_cabc fail:(ctrl_pdata == NUL\n");
		ret = -1;
	}
	

	//if (kstrtoul(buf, 0, &index)) 	return -EINVAL;
	

	hal_ctrl_data.id =GET_EFFECT;
	hal_ctrl_data.index =g_effect_index ;
	level = lenovo_lcd_effect_handle(ctrl_pdata,&hal_ctrl_data);
	
	ret = snprintf(buf, PAGE_SIZE, "%d\n",level);
	return ret;
}

ssize_t lenovo_fb_set_effect(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	ssize_t ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata=dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	int index,level;
	unsigned long data;
	struct hal_panel_ctrl_data hal_ctrl_data;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,panel_data);
	if (ctrl_pdata == NULL) 
	{
		pr_err("[houdz1]mdss_panel_set_cabc fail:(ctrl_pdata == NUL\n");
		ret = -1;
	}
	

	if (kstrtoul(buf, 0, &data))  return -EINVAL;
	index = (data >> 4) & 0xf;
	level = data & 0xf;
	

	hal_ctrl_data.id =SET_EFFECT;
	hal_ctrl_data.index =index ;
	hal_ctrl_data.level =level;
	ret = lenovo_lcd_effect_handle(ctrl_pdata,&hal_ctrl_data);
	return count;
} 




ssize_t lenovo_fb_get_effect_index(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n",g_effect_index);
	return ret;
}



ssize_t lenovo_fb_set_effect_index(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	unsigned long index;
	if (kstrtoul(buf, 0, &index)) 	return -EINVAL;
	g_effect_index =index;
	return count;
}







