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


extern u32 dsi_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,char cmd1, void (*fxn)(int), char *rbuf, int len);
extern void dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,struct dsi_panel_cmds *pcmds);
extern int lenovo_lcd_effect_color_data_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata);



		   
int lenovo_lcd_effect_color_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_pdata->lenovoLcdEffect);	
	
	memset(pLcdEffect->curret_effect_num,0x0,sizeof(pLcdEffect->curret_effect_num));
	lenovo_lcd_effect_color_data_init(ctrl_pdata);
	return 0;
	
  }
	


int lcd_get_effect_support(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data)
{
	int i;
	struct lenovo_lcd_effect_data *pEffectData = lcdEffect->pEffectData;
if(pEffectData==NULL)
	return 0;
	panel_data->effect_cnt = lcdEffect->effectDataCount;
	for (i = 0; i < panel_data->effect_cnt; i++) 
	{
		
		if(pEffectData->is_support == 1)
		{
			memcpy(panel_data->effect[i].name, pEffectData->effect_name, strlen(pEffectData->effect_name));
			LCD_EFFECT_LOG("%s:support - %s\n",__func__,panel_data->effect[i].name);
		}
		else
		{
			memcpy(panel_data->effect[i].name, "null",5);
			LCD_EFFECT_LOG("%s:support - %s\n",__func__,panel_data->effect[i].name);
		}
		pEffectData ++ ;
	}
	LCD_EFFECT_LOG("%s:GET_EFFECT_NUM = 0x%x\n",__func__,panel_data->effect_cnt);
	return panel_data->effect_cnt;
}


int lcd_get_effect_max_level(struct lenovo_lcd_effect *lcdEffect, int index)
{
	struct lenovo_lcd_effect_data *pEffectData =  lcdEffect->pEffectData;
	pEffectData += index;
	LCD_EFFECT_LOG("%s: name: [%s] index: [%d] max_level: [%d]\n", __func__,pEffectData->effect_name, index, pEffectData->max_level);
	return pEffectData->max_level;
}


 int lcd_get_effect_level(struct lenovo_lcd_effect *lcdEffect, int index)
{
	struct lenovo_lcd_effect_data *pEffectData =  lcdEffect->pEffectData;
	
	if((index <0)||(pEffectData ==NULL)) return -1;
	
	pEffectData +=index;
	LCD_EFFECT_LOG("%s: name: [%s] index: [%d] level: [%d]\n", __func__,pEffectData->effect_name, index, lcdEffect->curret_effect_num[index]);
	return lcdEffect->curret_effect_num[index];
}


int lcd_effect_set_cabc_reg(struct mdss_dsi_ctrl_pdata *ctrl,int regData)
{
	unsigned char read_cabc=0;
	unsigned char timeout=4;
	struct dsi_panel_cmds dsi_cmd;	

	char cabc_mode[2] = {0x55,0x00};
	struct dsi_cmd_desc cabc_mode_cmd = {{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(cabc_mode)},cabc_mode};

		
	cabc_mode[1] = regData;
	LCD_EFFECT_LOG("%s:regData = 0x%x\n",__func__,cabc_mode[1]);
	memset(&dsi_cmd,0,sizeof(struct dsi_panel_cmds));
	dsi_cmd.cmds = &cabc_mode_cmd;
	dsi_cmd.cmd_cnt = 1;
	dsi_cmds_send(ctrl,&dsi_cmd);
	do
	{
		dsi_cmd_read(ctrl,0x56,0x00,NULL,&read_cabc,1); 
		LCD_EFFECT_LOG("read_cabc=0x%x,timeout=%d\n",read_cabc,timeout);
		if(read_cabc == regData) break;
	}while(timeout--);
	if(timeout == 0) return -1;
	return 0;
}




int lcd_effect_send_cmd(struct mdss_dsi_ctrl_pdata *ctrl,struct lenovo_lcd_effect_data *pEffectData,int cmd_index)
{
	struct dsi_panel_cmds dsi_cmd;
	int i;
	struct dsi_ctrl_hdr *pdchdr;
	char *payload;
	struct dsi_cmd_desc  *pdesc;
	
	memset(&dsi_cmd,0,sizeof(struct dsi_panel_cmds));

	dsi_cmd.cmds =  (pEffectData->cmds)+cmd_index*(pEffectData->cmds_cnt);
	dsi_cmd.cmd_cnt =  pEffectData->cmds_cnt;
	dsi_cmd.link_state = DSI_LP_MODE;
	if(g_lcd_effect_log_on)
	{
		pdesc = dsi_cmd.cmds;
		pdchdr = &(pdesc->dchdr);
		payload = pdesc->payload;
		for(i =0; i<(pdchdr->dlen);i++) LCD_EFFECT_LOG("[houdz]%s:0x%x\n",__func__,*(payload+i));
	}
	dsi_cmds_send(ctrl,&dsi_cmd);
	return 0;
}
