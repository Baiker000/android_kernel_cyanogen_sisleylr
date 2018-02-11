#include "../../mdss_panel.h"
#include "../../mdss_dsi_cmd.h"
#include "../lenovo_lcd_effect.h"
#include "../../mdss_dsi.h"


extern int lenovo_set_effect_level_nt35521(void *pData,int index,int level);
extern int lenovo_set_effect_level_hx8394d(void *pData,int index,int level);

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






/*************BOE NT35521 start****************************************/


static char nt_page[6] =  {0xF0,0x55,0xAA,0x52,0x08,0x00};
static char vivid[17] = {0xCC,0xFF,0x3F,0x87,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05};
static struct dsi_cmd_desc vivid_cmd[2] = 
{
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(nt_page)},nt_page},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(vivid)},vivid},
};



static char lcd_cabc[2] = {0x55,0x00};
static struct dsi_cmd_desc cabc_cmd[1] =
{
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(lcd_cabc)},lcd_cabc},
};


static char lcd_inverse[2] = {0x20,0x00};
static struct dsi_cmd_desc lcd_inverse_cmd[1] = 
{
	{{DTYPE_DCS_WRITE, 1, 0, 0, 1, sizeof(lcd_inverse)},lcd_inverse},
};

static struct dsi_cmd_desc hue_cmds[16][13];

struct lenovo_lcd_effect_data lcd_effect_data_boe_z2[]=
{
	{"cabc",1,3,cabc_cmd,ARRAY_SIZE(cabc_cmd)},
	{"ce",1,16,vivid_cmd,ARRAY_SIZE(vivid_cmd)},
	{"cta",1,16,(struct dsi_cmd_desc*)(&hue_cmds[0]),ARRAY_SIZE(hue_cmds[0])},
	{"aco",0,16,vivid_cmd,ARRAY_SIZE(vivid_cmd)},
	{"gamma",0,0,NULL,0},
	{"inverse",1,1,lcd_inverse_cmd,ARRAY_SIZE(lcd_inverse_cmd)},
	{"sre",1,3,cabc_cmd,ARRAY_SIZE(cabc_cmd)},		
};

static int lenovo_lcd_effect_gamma_init_z2(struct mdss_dsi_ctrl_pdata *ctrl_pdata,struct dsi_cmd_desc *cmd_desc,int byteCnt)
{
	int cmdCnt = ctrl_pdata->gamma_cmds.cmd_cnt;
	struct dsi_cmd_desc *pHueCmds = cmd_desc ;
	struct dsi_cmd_desc *pGammaCmds = ctrl_pdata->gamma_cmds.cmds ;
	int i,cmdsCnt;

	if((pHueCmds == NULL)||(pGammaCmds ==NULL)) return -1;

	if(byteCnt!= (cmdCnt*sizeof(struct dsi_cmd_desc)))
	{
		pr_err("[houdz1]%s cnt error:sizeof(hue_cmds) = %d,size = %ld\n",__func__,byteCnt,(cmdCnt*sizeof(struct dsi_cmd_desc)));
		return -1;
	}
	
	memset(pHueCmds,0,byteCnt);
	memcpy(pHueCmds,pGammaCmds,byteCnt);

	if(g_lcd_effect_log_on == true)
	{
		printk("[houdz1]%s:sizeof(hue_cmds) = %d,size = %ld\n",__func__,byteCnt,(cmdCnt*sizeof(struct dsi_cmd_desc)));
		pHueCmds = cmd_desc;
		if(ctrl_pdata->panel_id ==0) cmdsCnt = 13;
		else cmdsCnt =6;
		for(i=0; i<16;i++) 
		{
			lenovo_show_lcd_param(pHueCmds,cmdsCnt);
			pHueCmds += cmdsCnt;
		}
	}
	return 0;
}

/*************BOE NT35521 end****************************************/



/**************tianma hx8394d start ****************************************/


static char ce_tianma_on[3] = {0xE4, 0x02, 0x00};
static char ce_tianma_off[3] = {0xE4, 0x00, 0x00};
static char ce_tianma[16][7] =
{
	{0xE5, 0x00, 0x00, 0x08, 0x06, 0x04, 0x00},
	{0xE5, 0x00, 0x01, 0x08, 0x06, 0x04, 0x00},
	{0xE5, 0x00, 0x01, 0x08, 0x06, 0x05, 0x00},
	{0xE5, 0x00, 0x02, 0x09, 0x06, 0x05, 0x00},
	{0xE5, 0x00, 0x02, 0x09, 0x07, 0x05, 0x00},
	{0xE5, 0x00, 0x03, 0x0A, 0x07, 0x05, 0x00},
	{0xE5, 0x00, 0x03, 0x0A, 0x08, 0x06, 0x00},
	{0xE5, 0x00, 0x04, 0x0B, 0x08, 0x06, 0x00},
	{0xE5, 0x00, 0x04, 0x0B, 0x09, 0x06, 0x00},
	{0xE5, 0x00, 0x05, 0x0B, 0x09, 0x07, 0x00},
	{0xE5, 0x00, 0x05, 0x0C, 0x09, 0x07, 0x00},
	{0xE5, 0x00, 0x06, 0x0C, 0x0A, 0x08, 0x00},
	{0xE5, 0x00, 0x06, 0x0D, 0x0A, 0x08, 0x00},
	{0xE5, 0x00, 0x07, 0x0D, 0x0A, 0x09, 0x00},
	{0xE5, 0x00, 0x07, 0x0E, 0x0B, 0x09, 0x00},
	{0xE5, 0x00, 0x08, 0x0E, 0x0B, 0x09, 0x00}
};
static struct dsi_cmd_desc ce_tianma_cmd[16][2] = 
{
	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_off)},ce_tianma_off},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[0][0]}},
	
	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[1][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[2][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[3][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[4][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[5][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[6][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[7][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[8][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[9][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[10][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[11][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[12][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[13][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[14][0]}},

	{{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(ce_tianma_on)},ce_tianma_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ce_tianma[0])},&ce_tianma[15][0]}},
};


static struct dsi_cmd_desc hue_cmds_tianma[16][6];
struct lenovo_lcd_effect_data lcd_effect_data_tianma_z2[]=
{
	{"cabc",1,3,cabc_cmd,ARRAY_SIZE(cabc_cmd)},
	{"ce",1,16,(struct dsi_cmd_desc*)(&ce_tianma_cmd[0]),ARRAY_SIZE(ce_tianma_cmd[0])},
	{"cta",1,16,(struct dsi_cmd_desc*)(&hue_cmds_tianma[0]),ARRAY_SIZE(hue_cmds_tianma[0])},
	{"aco",0,16,NULL,0},
	{"gamma",0,0,NULL,0},
	{"inverse",1,1,lcd_inverse_cmd,ARRAY_SIZE(lcd_inverse_cmd)},
	{"sre",0,3,NULL,0},		
};

/**************tianma hx8394d end****************************************/


int lenovo_lcd_effect_color_data_init(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int ret = 0;
	struct lenovo_lcd_effect *pLcdEffect = &(ctrl_pdata->lenovoLcdEffect);

	if(ctrl_pdata->panel_id == 0)
	{
		lenovo_lcd_effect_gamma_init_z2(ctrl_pdata,(struct dsi_cmd_desc*)&hue_cmds[0],sizeof(hue_cmds));		
		pLcdEffect->effectDataCount = ARRAY_SIZE(lcd_effect_data_boe_z2);
		pLcdEffect->pEffectData = &lcd_effect_data_boe_z2[0];
		pLcdEffect->pFuncSetEffect = lenovo_set_effect_level_nt35521;
	}
	else if(ctrl_pdata->panel_id == 1)
	{
		lenovo_lcd_effect_gamma_init_z2(ctrl_pdata,(struct dsi_cmd_desc*)&hue_cmds_tianma[0],sizeof(hue_cmds_tianma));
		pLcdEffect->effectDataCount = ARRAY_SIZE(lcd_effect_data_tianma_z2);
		pLcdEffect->pEffectData = &lcd_effect_data_tianma_z2[0];
		pLcdEffect->pFuncSetEffect = lenovo_set_effect_level_hx8394d;
	}
	else ret =-1;

	return ret;
}
