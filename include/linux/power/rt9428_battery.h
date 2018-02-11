/*
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _RT9428_BATTERY_H_
#define _RT9428_BATTERY_H_

#define RT9428_DRV_VER	   "1.0.2_G"
enum {
	RT9428_REG_VBATH = 0x02,
	RT9428_REG_RANGE1_START = RT9428_REG_VBATH,
	RT9428_REG_VBATL,
	RT9428_REG_SOCH,
	RT9428_REG_SOCL,
	RT9428_REG_CTRLH,
	RT9428_REG_CTRLL,
	RT9428_REG_DEVID0,
	RT9428_REG_DEVID1,
	RT9428_REG_STATUS,
	RT9428_REG_CRATE,
	RT9428_REG_CFG0,
	RT9428_REG_CFG1,
	RT9428_REG_OCVH,
	RT9428_REG_OCVL,
	RT9428_REG_RANGE1_STOP = RT9428_REG_OCVL,
	RT9428_REG_MFAH = 0xFE,
	RT9428_REG_RANGE2_START = RT9428_REG_MFAH,
	RT9428_REG_MFAL,
	RT9428_REG_RANGE2_STOP = RT9428_REG_MFAL,
	RT9428_REG_MAX,
};

#define RT9428_SMOOTH_POLL	20
#define RT9428_NORMAL_POLL	10
#define RT9428_SOCALRT_MASK	0x20
#define RT9428_SOCL_SHFT	0
#define RT9428_SOCL_MASK	0x1F
#define RT9428_SOCL_MAX		32
#define RT9428_SOCL_MIN		1
struct rt9428_platform_data {
	int r_bat;
	int alert_gpio;
	//ini file
	int low_temp2_base;
	int low_temp_base;
	int temp_base;
	int high_temp_base;
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
	//ini file end
	int soc_comp;
	int full_design;	
	int alert_threshold;
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
};
#endif
