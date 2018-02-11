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


int g_lcd_effect_log_on = true;



extern int lcd_get_mode_support(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data);
extern int lcd_get_mode_level(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data);
extern int lenovo_lcd_set_mode(struct mdss_dsi_ctrl_pdata *ctrl_data,int mode);
extern int lenovo_lcd_effect_mode_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata);

extern int lcd_get_effect_support(struct lenovo_lcd_effect *lcdEffect,struct hal_panel_data *panel_data);
extern int lcd_get_effect_max_level(struct lenovo_lcd_effect *lcdEffect, int index);
extern  int lcd_get_effect_level(struct lenovo_lcd_effect *lcdEffect, int index);
extern int lenovo_lcd_effect_color_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata);



int lenovo_show_lcd_param(struct dsi_cmd_desc *cmds, int cmd_cnt)
{
	int i, j;

	printk("======================================= cmds_cnt %d =========================================\n", cmd_cnt);
	for (i = 0; i < cmd_cnt; i++) {
		printk("%2x %2x %2x %2x %2x %2x ", cmds[i].dchdr.dtype,
				cmds[i].dchdr.last,
				cmds[i].dchdr.vc,
				cmds[i].dchdr.ack,
				cmds[i].dchdr.wait,
				cmds[i].dchdr.dlen);
		for (j = 0; j < cmds[i].dchdr.dlen; j++) {
			printk("%2x ", cmds[i].payload[j]);
		}
		printk("\n");
	}
	pr_debug("===========================================================================================\n");
	return 0;
}

int lenovo_lcd_effect_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if(g_lcd_effect_log_on == true)  printk("[houdz1]%s\n",__func__);
	LCD_EFFECT_LOG("%s start\n",__func__);
	lenovo_lcd_effect_mode_init(ctrl_pdata);
	lenovo_lcd_effect_color_init(ctrl_pdata);
	LCD_EFFECT_LOG("%s done\n",__func__);
	return 0;
}


static int is_custom_mode(struct mdss_dsi_ctrl_pdata *ctrl_data)
{
	struct lenovo_lcd_effect *lcdEffect=&(ctrl_data->lenovoLcdEffect);
	return ((lcdEffect->curret_mode_num)==MODE_INDEX_CUSTOM);
}


int lenovo_lcd_effect_handle(struct mdss_dsi_ctrl_pdata *ctrl_data,struct hal_panel_ctrl_data *hal_ctrl_data)
{
	int ret = 0;
	struct lenovo_lcd_effect *lcdEffect=&(ctrl_data->lenovoLcdEffect);
	int *pEffectCurrentLevel ;
	struct lenovo_lcd_mode_data *pModeData =  lcdEffect->pModeData;

	pEffectCurrentLevel = lcdEffect->curret_effect_num;
	
	if(lcdEffect == NULL)  return -1;
	LCD_EFFECT_LOG("%s:hal_ctrl_data->id = %d\n",__func__,hal_ctrl_data->id);

	switch(hal_ctrl_data->id){
		case GET_MODE_NUM:
			ret = lcd_get_mode_support(lcdEffect,&(hal_ctrl_data->panel_data));
			break;
		case GET_MODE:
			ret = lcd_get_mode_level(lcdEffect,&(hal_ctrl_data->panel_data));
			break;
		case SET_MODE:
			ret = lenovo_lcd_set_mode(ctrl_data,hal_ctrl_data->mode);
			break;
		case GET_EFFECT_NUM:
			ret = lcd_get_effect_support(lcdEffect,&(hal_ctrl_data->panel_data));
			break;
		case GET_EFFECT_LEVEL:
			ret = lcd_get_effect_max_level(lcdEffect,hal_ctrl_data->index);
			break;
		case GET_EFFECT:
			ret = lcd_get_effect_level(lcdEffect,hal_ctrl_data->index);
			break;
		case SET_EFFECT:
			if (is_custom_mode(ctrl_data)||(hal_ctrl_data->index ==EFFECT_INDEX_INVERSE)){
				if(lcdEffect->pFuncSetEffect == NULL) ret =-1;
				else{
					ret = (*(lcdEffect->pFuncSetEffect))(ctrl_data,hal_ctrl_data->index, hal_ctrl_data->level);
					if(ret == 0) {
						pEffectCurrentLevel += (hal_ctrl_data->index);
						*pEffectCurrentLevel = hal_ctrl_data->level;	
					}
				}
			}
			else{
				pModeData +=( lcdEffect->curret_mode_num);
				pr_err("%s:(%s) can't support change effect\n",__func__,pModeData->mode_name);
				ret = -EINVAL;
			}
			break;
		default :
			break;

	}
	return ret;
}


int lenovo_lcd_effect_reset(struct mdss_dsi_ctrl_pdata *ctrl_data)
{
	int ret=0;

	struct lenovo_lcd_effect *lcdEffect=&(ctrl_data->lenovoLcdEffect);
	struct lenovo_lcd_effect_data *pEffectData =  lcdEffect->pEffectData;
	
	int i;

	LCD_EFFECT_LOG("%s start\n",__func__);

	if(lcdEffect->pFuncSetEffect == NULL) ret =-1;
	else if(is_custom_mode(ctrl_data))
	{
             for(i=0;i<(lcdEffect->effectDataCount);i++,pEffectData++)
              {
			 if(pEffectData->is_support ==1) 
				ret = (*(lcdEffect->pFuncSetEffect))(ctrl_data,i,lcdEffect->curret_effect_num[i] );
              }
	}
	else 
	{
		ret = (*(lcdEffect->pFuncSetEffect))(ctrl_data,EFFECT_INDEX_INVERSE,lcdEffect->curret_effect_num[EFFECT_INDEX_INVERSE]);
		ret = lenovo_lcd_set_mode(ctrl_data,lcdEffect->curret_mode_num);
	}
	LCD_EFFECT_LOG("%s done\n",__func__);
	return ret;
}


void dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;

	/*Panel ON/Off commands should be sent in DSI Low Power Mode*/
	if (pcmds->link_state == DSI_LP_MODE)  cmdreq.flags  |= CMD_REQ_LP_MODE;

	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};

u32 dsi_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,
		char cmd1, void (*fxn)(int), char *rbuf, int len)
{
	struct dcs_cmd_req cmdreq;
	int ret =0;

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = len;
	cmdreq.rbuf = rbuf;
	cmdreq.cb = fxn; /* call back */
	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, until call back called
	 */

	return ret;
}

