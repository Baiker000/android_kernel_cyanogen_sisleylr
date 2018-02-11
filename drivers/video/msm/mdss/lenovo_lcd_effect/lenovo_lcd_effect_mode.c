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
#include "lenovo_lcd_effect.h"

#define NAME_SIZE 16



extern void dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,struct dsi_panel_cmds *pcmds);
extern int lenovo_show_lcd_param(struct dsi_cmd_desc *cmds, int cmd_cnt);

static struct lenovo_lcd_mode_data lcd_mode_data[]={
	{"custom_mode",0,NULL,0,0},
	{"auto_mode",0,NULL,0,0},
	{"normal_mode",0,NULL,0,0},
	{"comfort_mode",0,NULL,0,0},
	{"outside_mode",0,NULL,0,0},
	{"ultra_mode", 0,NULL,0,0},
	{"camera_mode",0,NULL,0,0},
};


int lenovo_lcd_effect_mode_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_pdata->lenovoLcdEffect);	


	pLcdEffect->pModeData = &lcd_mode_data[0];
	pLcdEffect->modeDataCount = ARRAY_SIZE(lcd_mode_data);

	pLcdEffect->curret_mode_num = MODE_INDEX_NORMAL;
	
	
	if(ctrl_pdata->custom_mode_cmds.cmd_cnt){
		lcd_mode_data[0].is_support = 1;
		lcd_mode_data[0].cmds = ctrl_pdata->custom_mode_cmds.cmds;
		lcd_mode_data[0].cmds_cnt = ctrl_pdata->custom_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->default_mode_cmds.cmd_cnt){
		lcd_mode_data[2].is_support = 1;
		lcd_mode_data[2].cmds = ctrl_pdata->default_mode_cmds.cmds;
		lcd_mode_data[2].cmds_cnt = ctrl_pdata->default_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->comfort_mode_cmds.cmd_cnt){
		lcd_mode_data[3].is_support = 1;
		lcd_mode_data[3].cmds = ctrl_pdata->comfort_mode_cmds.cmds;
		lcd_mode_data[3].cmds_cnt = ctrl_pdata->comfort_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->outside_mode_cmds.cmd_cnt){
		lcd_mode_data[4].is_support = 1;
		lcd_mode_data[4].cmds = ctrl_pdata->outside_mode_cmds.cmds;
		lcd_mode_data[4].cmds_cnt = ctrl_pdata->outside_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->ultra_mode_cmds.cmd_cnt){
		lcd_mode_data[5].is_support = 1;
		lcd_mode_data[5].cmds = ctrl_pdata->ultra_mode_cmds.cmds;
		lcd_mode_data[5].cmds_cnt = ctrl_pdata->ultra_mode_cmds.cmd_cnt;
	}
	if(ctrl_pdata->camera_mode_cmds.cmd_cnt){
		lcd_mode_data[6].is_support = 1;
		lcd_mode_data[6].cmds = ctrl_pdata->camera_mode_cmds.cmds;
		lcd_mode_data[6].cmds_cnt = ctrl_pdata->camera_mode_cmds.cmd_cnt;
	}
	if(g_lcd_effect_log_on == true){
	 	int i;
		for(i=0; i<7;i++) lenovo_show_lcd_param(lcd_mode_data[i].cmds,lcd_mode_data[i].cmds_cnt);
	}

	return 0;
}



int lenovo_lcd_set_mode(struct mdss_dsi_ctrl_pdata *ctrl_data,int mode)
{
	struct dsi_panel_cmds panel_cmds;

	struct lenovo_lcd_effect *lcdEffect=&(ctrl_data->lenovoLcdEffect);
	struct lenovo_lcd_mode_data *pModeData =  lcdEffect->pModeData;

	if(mode >(lcdEffect->modeDataCount)) return -1;
	

	LCD_EFFECT_LOG("%s:mode = %d \n",__func__,mode);
	pModeData += mode;
	if(pModeData->is_support != 1)
	{
		pr_err("[houdz1]%s:the mode(%d) is not support\n",__func__,mode);
		return -1;
	}
	ctrl_data->is_ultra_mode = 0;
	memset(&panel_cmds, 0, sizeof(panel_cmds));
	panel_cmds.cmds =pModeData->cmds;
	panel_cmds.cmd_cnt = pModeData->cmds_cnt;
	if(mode == MODE_INDEX_ULTRA) ctrl_data->is_ultra_mode = 1;
	dsi_cmds_send(ctrl_data,&panel_cmds);
	lcdEffect->curret_mode_num =mode;
	return 0;
}


int lcd_get_mode_support(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data)
{
	int i;
	struct lenovo_lcd_mode_data *pModeData = lcdEffect->pModeData;
if(pModeData==NULL)
	return 0;
	for (i = 0; i < lcdEffect->modeDataCount; i++) 
	{
		if(pModeData->is_support == 1)
		{
			memcpy(panel_data->mode[i].name, pModeData->mode_name, strlen(pModeData->mode_name));
			LCD_EFFECT_LOG("%s:support - %s\n",__func__,panel_data->mode[i].name);
		}
		else 	
		{
			memcpy(panel_data->mode[i].name, "null",5);
			LCD_EFFECT_LOG("%s:support - %s\n",__func__,panel_data->mode[i].name);
		}		
		
		pModeData++;
	}
	panel_data->mode_cnt = lcdEffect->modeDataCount;
	LCD_EFFECT_LOG("%s: mode_cnt=%d\n", __func__,panel_data->mode_cnt);
	return panel_data->mode_cnt;
}

int lcd_get_mode_level(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data)
{
	struct lenovo_lcd_mode_data *pModeData;

	pModeData = lcdEffect->pModeData;	
if(pModeData==NULL)
	return 0;
	pModeData += lcdEffect->curret_mode_num;
	LCD_EFFECT_LOG("%s: name: [%s]  mode: [%d]\n", __func__,pModeData->mode_name, lcdEffect->curret_mode_num);
	memcpy(panel_data->mode[lcdEffect->curret_mode_num].name,pModeData->mode_name,NAME_SIZE);//sizeof(pModeData->mode_name));
	return lcdEffect->curret_mode_num;
}








