#include "../mdss_panel.h"
#include "../mdss_dsi_cmd.h"
#include "../mdss_dsi.h"
#include "lenovo_lcd_effect.h"

/***********************************
*novatek effect: cabc
************************************/




extern u32 dsi_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,char cmd1, void (*fxn)(int), char *rbuf, int len);

extern int lcd_effect_set_cabc_reg(struct mdss_dsi_ctrl_pdata *ctrl,int regData);
extern int lcd_effect_send_cmd(struct mdss_dsi_ctrl_pdata *ctrl,struct lenovo_lcd_effect_data *pEffectData,int cmd_index);

static char reg_page[6] = {0xF0,0x55,0xAA,0x52,0x08,0x00};
static struct dsi_cmd_desc novatek_reg_page_cmd = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(reg_page)},
	reg_page
};


static int g_cabc_reg_nt35521 =0;

static void lenovo_enable_sre_reg_nt35521(int level) /*level =0,1,2,3*/
{
	int value = level&0x03;
	g_cabc_reg_nt35521  &=0xbf;
	if(value) 
	{
		value -= 1;
		g_cabc_reg_nt35521  &=0x8f;
		g_cabc_reg_nt35521 |=((value+4)<<4);	
	}
	else g_cabc_reg_nt35521  &=0xbf;
	LCD_EFFECT_LOG("%s:level = %d,g_cabc_reg_nt35521=0x%x\n",__func__,value,g_cabc_reg_nt35521);
}

/*
static void lenovo_enable_ie_reg_nt35521(int level)
{
	
	int value = level&0x03;
	g_cabc_reg_nt35521  &=0xbf;

	if(level)
	{
		value -=1;
		g_cabc_reg_nt35521  &=0x4f;
		if(value ==2) value =0xb0;
		else g_cabc_reg_nt35521 |=((value+8)<<4);
	}
	else g_cabc_reg_nt35521  &=0x7f;
		LCD_EFFECT_LOG("%s:level = %d,g_cabc_reg_nt35521=0x%x,\n",__func__,value,g_cabc_reg_nt35521);
}
*/

static void lenovo_enable_cabc_reg_nt35521(int level)
{
	int value = level&0x03;
	g_cabc_reg_nt35521  &=0xf0;
	if(value) g_cabc_reg_nt35521 |=value;
	else g_cabc_reg_nt35521  &=0xf0;
	LCD_EFFECT_LOG("%s:level = %d,g_cabc_reg_nt35521=0x%x\n",__func__,value,g_cabc_reg_nt35521);
}





static int set_novatek_reg_page(struct mdss_dsi_ctrl_pdata *ctrl,int page)
{

	struct dcs_cmd_req cmdreq;

	pr_err("[houdz1]%s:  page=%d\n", __func__, page);
	if(page>3) return -1;
	reg_page[5] = (unsigned char)page;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &novatek_reg_page_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	return 0;
}

static int lenovo_show_lcd_param(struct dsi_cmd_desc *cmds, int cmd_cnt)
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




int lenovo_set_effect_level_nt35521(void *pData,int index,int level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_data = pData;
	struct lenovo_lcd_effect *lcdEffect=&(ctrl_data->lenovoLcdEffect);
	struct lenovo_lcd_effect_data *pEffectData =  lcdEffect->pEffectData;

	struct dsi_cmd_desc *custom_mode_cmds,*gamma_cmds;


	int maxLevel = pEffectData->max_level;
	int value = level;
	int ret=0;

	char read_cabc[17];
	int i;

	LCD_EFFECT_LOG("%s:index = %d,level=%d\n",__func__,index,level);
	
	if(index > (lcdEffect->effectDataCount)) 
	{
		pr_err("[houdz1]%s:index(%d)>effectDataCount\n",__func__,index);
		return -1;
	}
	pEffectData += index;	
	
	if(pEffectData->is_support != 1)
	{
		pr_err("[houdz1]%s:the effect(%s) is not support\n",__func__,pEffectData->effect_name);
		return -1;
	}	
	maxLevel = pEffectData->max_level;
	if((level <0) ||(level > maxLevel)) 
	{
		pr_err("[houdz1]%s:level(%d) is errort\n",__func__,level);
		return -1;
	}

	custom_mode_cmds = (ctrl_data->custom_mode_cmds).cmds;
	switch(index)	
	{
		case EFFECT_INDEX_CABC:
			lenovo_enable_cabc_reg_nt35521(level);
			lcd_effect_set_cabc_reg(ctrl_data,g_cabc_reg_nt35521);
			break;
		case EFFECT_INDEX_SAT:
			value &=0x0f;
			value =value ==0 ?0xff:(value-1);

	
			lcd_effect_send_cmd(ctrl_data,pEffectData,0);
			if(g_lcd_effect_log_on)
			{
				set_novatek_reg_page(ctrl_data,0);
				dsi_cmd_read(ctrl_data,0xcc,0x00,NULL,read_cabc,17);
				for(i=0;i<17;i++)
				LCD_EFFECT_LOG("- 0xcc[%d] =0x%x\n",i,read_cabc[i]);
			}
			if(custom_mode_cmds != NULL)
			{
				custom_mode_cmds += 2;
				custom_mode_cmds->payload[1] = (value<<4)|value;
				custom_mode_cmds->payload[2]  &=0xf0;
				custom_mode_cmds->payload[2]  |= value;
			}
			break;
		case EFFECT_INDEX_CONTRAST:
			value = ((value&0xf)>>2)|((value&0x03)<<2);

			lcd_effect_send_cmd(ctrl_data,pEffectData,0);
			if(custom_mode_cmds != NULL)
			{
				custom_mode_cmds += 2;
				custom_mode_cmds->payload[13] =(value<<4)|value;
				custom_mode_cmds->payload[14]  &=0xf0;
				custom_mode_cmds->payload[14]  |= value;
			}
			break;
		case EFFECT_INDEX_HUE:
			LCD_EFFECT_LOG("[houdz]%s:EFFECT_INDEX_HUE level = %d\n",__func__,level);
			/*
			if(level == 2)
			{
				unsigned int i;
				char read_cabc[5];
				set_novatek_reg_page(ctrl_data,2);
				dsi_cmd_read(ctrl_data,0xBB,0x00,NULL,read_cabc,17);
				for(i=0;i<4;i++) LCD_EFFECT_LOG("- 0xcc[%d] =0x%x\n",i,read_cabc[i]);
				dsi_cmd_read(ctrl_data,0xEE,0x00,NULL,read_cabc,1);
				LCD_EFFECT_LOG("- 0xee =0x%x\n",read_cabc[0]);
			}
			else */
			lcd_effect_send_cmd(ctrl_data,pEffectData,level);
			if(custom_mode_cmds != NULL)
			{
				custom_mode_cmds += 3;
				gamma_cmds = (pEffectData->cmds)+level*(pEffectData->cmds_cnt);
				for(i=0;i<(pEffectData->cmds_cnt);i++)
				{
					memcpy(custom_mode_cmds,gamma_cmds,sizeof(struct dsi_cmd_desc));
					custom_mode_cmds += 1;
					gamma_cmds += 1;
				}
				if(g_lcd_effect_log_on) lenovo_show_lcd_param((ctrl_data->custom_mode_cmds).cmds,(ctrl_data->custom_mode_cmds).cmd_cnt);
			}
			break;
		case EFFECT_INDEX_SRE:
			lenovo_enable_sre_reg_nt35521(level);
			lcd_effect_set_cabc_reg(ctrl_data,g_cabc_reg_nt35521);
			break;
		case EFFECT_INDEX_INVERSE:
			if(level)  pEffectData->cmds->payload[0] = 0x21;
			else  pEffectData->cmds->payload[0]= 0x20;
			lcd_effect_send_cmd(ctrl_data,pEffectData,0);
			break;
		default:
			pr_err("[houdz1]%s:(index = %d) is not support\n",__func__,index);		
			break;

	}
	return ret;
}






