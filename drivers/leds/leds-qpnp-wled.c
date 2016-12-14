/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/leds-qpnp-wled.h>

#include <linux/htc_flashlight.h>

#define QPNP_IRQ_FLAGS	(IRQF_TRIGGER_RISING | \
			IRQF_TRIGGER_FALLING | \
			IRQF_ONESHOT)

#define QPNP_WLED_CTRL_BASE		"qpnp-wled-ctrl-base"
#define QPNP_WLED_SINK_BASE		"qpnp-wled-sink-base"

#define QPNP_WLED_INT_EN_SET(b)		(b + 0x15)
#define QPNP_WLED_EN_REG(b)		(b + 0x46)
#define QPNP_WLED_FDBK_OP_REG(b)	(b + 0x48)
#define QPNP_WLED_VREF_REG(b)		(b + 0x49)
#define QPNP_WLED_BOOST_DUTY_REG(b)	(b + 0x4B)
#define QPNP_WLED_SWITCH_FREQ_REG(b)	(b + 0x4C)
#define QPNP_WLED_OVP_REG(b)		(b + 0x4D)
#define QPNP_WLED_ILIM_REG(b)		(b + 0x4E)
#define QPNP_WLED_SOFTSTART_RAMP_DLY(b) (b + 0x53)
#define QPNP_WLED_VLOOP_COMP_RES_REG(b)	(b + 0x55)
#define QPNP_WLED_VLOOP_COMP_GM_REG(b)	(b + 0x56)
#define QPNP_WLED_PSM_CTRL_REG(b)	(b + 0x5B)
#define QPNP_WLED_SC_PRO_REG(b)		(b + 0x5E)
#define QPNP_WLED_TEST1_REG(b)		(b + 0xE2)
#define QPNP_WLED_TEST4_REG(b)		(b + 0xE5)
#define QPNP_WLED_REF_7P7_TRIM_REG(b)	(b + 0xF2)

#define QPNP_WLED_EN_MASK		0x7F
#define QPNP_WLED_EN_SHIFT		7
#define QPNP_WLED_FDBK_OP_MASK		0xF8
#define QPNP_WLED_VREF_MASK		0xF0
#define QPNP_WLED_VREF_STEP_MV		25
#define QPNP_WLED_VREF_MIN_MV		300
#define QPNP_WLED_VREF_MAX_MV		675
#define QPNP_WLED_DFLT_VREF_MV		350

#define QPNP_WLED_VLOOP_COMP_RES_MASK			0xF0
#define QPNP_WLED_VLOOP_COMP_RES_OVERWRITE		0x80
#define QPNP_WLED_LOOP_COMP_RES_DFLT_AMOLED_KOHM	320
#define QPNP_WLED_LOOP_COMP_RES_STEP_KOHM		20
#define QPNP_WLED_LOOP_COMP_RES_MIN_KOHM		20
#define QPNP_WLED_LOOP_COMP_RES_MAX_KOHM		320
#define QPNP_WLED_VLOOP_COMP_GM_MASK			0xF0
#define QPNP_WLED_VLOOP_COMP_GM_OVERWRITE		0x80
#define QPNP_WLED_LOOP_EA_GM_DFLT_AMOLED		0x03
#define QPNP_WLED_LOOP_EA_GM_MIN			0x0
#define QPNP_WLED_LOOP_EA_GM_MAX			0xF
#define QPNP_WLED_VREF_PSM_MASK				0xF8
#define QPNP_WLED_VREF_PSM_STEP_MV			50
#define QPNP_WLED_VREF_PSM_MIN_MV			400
#define QPNP_WLED_VREF_PSM_MAX_MV			750
#define QPNP_WLED_VREF_PSM_DFLT_AMOLED_MV		450
#define QPNP_WLED_PSM_CTRL_OVERWRITE			0x80
#define QPNP_WLED_AVDD_MIN_TRIM_VALUE			-7
#define QPNP_WLED_AVDD_MAX_TRIM_VALUE			8
#define QPNP_WLED_AVDD_TRIM_CENTER_VALUE		7

#define QPNP_WLED_ILIM_MASK		0xF8
#define QPNP_WLED_ILIM_MIN_MA		105
#define QPNP_WLED_ILIM_MAX_MA		1980
#define QPNP_WLED_ILIM_STEP_MA		280
#define QPNP_WLED_DFLT_ILIM_MA		980
#define QPNP_WLED_ILIM_OVERWRITE	0x80
#define QPNP_WLED_BOOST_DUTY_MASK	0xFC
#define QPNP_WLED_BOOST_DUTY_STEP_NS	52
#define QPNP_WLED_BOOST_DUTY_MIN_NS	26
#define QPNP_WLED_BOOST_DUTY_MAX_NS	156
#define QPNP_WLED_DEF_BOOST_DUTY_NS	104
#define QPNP_WLED_SWITCH_FREQ_MASK	0xF0
#define QPNP_WLED_SWITCH_FREQ_800_KHZ	800
#define QPNP_WLED_SWITCH_FREQ_1600_KHZ	1600
#define QPNP_WLED_OVP_MASK		0xFC
#define QPNP_WLED_OVP_17800_MV		17800
#define QPNP_WLED_OVP_19400_MV		19400
#define QPNP_WLED_OVP_29500_MV		29500
#define QPNP_WLED_OVP_31000_MV		31000
#define QPNP_WLED_TEST4_EN_VREF_UP	0x32
#define QPNP_WLED_INT_EN_SET_OVP_DIS	0x00
#define QPNP_WLED_INT_EN_SET_OVP_EN	0x02
#define QPNP_WLED_OVP_FLT_SLEEP_US	10
#define QPNP_WLED_TEST4_EN_IIND_UP	0x1

#define QPNP_WLED_CURR_SINK_REG(b)	(b + 0x46)
#define QPNP_WLED_SYNC_REG(b)		(b + 0x47)
#define QPNP_WLED_MOD_REG(b)		(b + 0x4A)
#define QPNP_WLED_HYB_THRES_REG(b)	(b + 0x4B)
#define QPNP_WLED_MOD_EN_REG(b, n)	(b + 0x50 + (n * 0x10))
#define QPNP_WLED_SYNC_DLY_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x01)
#define QPNP_WLED_FS_CURR_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x02)
#define QPNP_WLED_CABC_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x06)
#define QPNP_WLED_BRIGHT_LSB_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x07)
#define QPNP_WLED_BRIGHT_MSB_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x08)
#define QPNP_WLED_SINK_TEST5_REG(b)	(b + 0xE6)

#define QPNP_WLED_MOD_FREQ_1200_KHZ	1200
#define QPNP_WLED_MOD_FREQ_2400_KHZ	2400
#define QPNP_WLED_MOD_FREQ_9600_KHZ	9600
#define QPNP_WLED_MOD_FREQ_19200_KHZ	19200
#define QPNP_WLED_MOD_FREQ_MASK		0x3F
#define QPNP_WLED_MOD_FREQ_SHIFT	6
#define QPNP_WLED_ACC_CLK_FREQ_MASK	0xE7
#define QPNP_WLED_ACC_CLK_FREQ_SHIFT	3
#define QPNP_WLED_PHASE_STAG_MASK	0xDF
#define QPNP_WLED_PHASE_STAG_SHIFT	5
#define QPNP_WLED_DIM_RES_MASK		0xFD
#define QPNP_WLED_DIM_RES_SHIFT		1
#define QPNP_WLED_DIM_HYB_MASK		0xFB
#define QPNP_WLED_DIM_HYB_SHIFT		2
#define QPNP_WLED_DIM_ANA_MASK		0xFE
#define QPNP_WLED_HYB_THRES_MASK	0xF8
#define QPNP_WLED_HYB_THRES_MIN		78
#define QPNP_WLED_DEF_HYB_THRES		625
#define QPNP_WLED_HYB_THRES_MAX		10000
#define QPNP_WLED_MOD_EN_MASK		0x7F
#define QPNP_WLED_MOD_EN_SHFT		7
#define QPNP_WLED_MOD_EN		1
#define QPNP_WLED_GATE_DRV_MASK		0xFE
#define QPNP_WLED_SYNC_DLY_MASK		0xF8
#define QPNP_WLED_SYNC_DLY_MIN_US	0
#define QPNP_WLED_SYNC_DLY_MAX_US	1400
#define QPNP_WLED_SYNC_DLY_STEP_US	200
#define QPNP_WLED_DEF_SYNC_DLY_US	400
#define QPNP_WLED_FS_CURR_MASK		0xF0
#define QPNP_WLED_FS_CURR_MIN_UA	0
#define QPNP_WLED_FS_CURR_MAX_UA	30000
#define QPNP_WLED_FS_CURR_STEP_UA	2500
#define QPNP_WLED_CABC_MASK		0x7F
#define QPNP_WLED_CABC_SHIFT		7
#define QPNP_WLED_CURR_SINK_SHIFT	4
#define QPNP_WLED_BRIGHT_LSB_MASK	0xFF
#define QPNP_WLED_BRIGHT_MSB_SHIFT	8
#define QPNP_WLED_BRIGHT_MSB_MASK	0x0F
#define QPNP_WLED_SYNC			0x0F
#define QPNP_WLED_SYNC_RESET		0x00

#define QPNP_WLED_SINK_TEST5_HYB	0x14
#define QPNP_WLED_SINK_TEST5_DIG	0x1E

#define QPNP_WLED_SWITCH_FREQ_800_KHZ_CODE	0x0B
#define QPNP_WLED_SWITCH_FREQ_1600_KHZ_CODE	0x05

#define QPNP_WLED_DISP_SEL_REG(b)	(b + 0x44)
#define QPNP_WLED_MODULE_RDY_REG(b)	(b + 0x45)
#define QPNP_WLED_MODULE_EN_REG(b)	(b + 0x46)
#define QPNP_WLED_MODULE_RDY_MASK	0x7F
#define QPNP_WLED_MODULE_RDY_SHIFT	7
#define QPNP_WLED_MODULE_EN_MASK	0x7F
#define QPNP_WLED_MODULE_EN_SHIFT	7
#define QPNP_WLED_DISP_SEL_MASK		0x7F
#define QPNP_WLED_DISP_SEL_SHIFT	7
#define QPNP_WLED_EN_SC_MASK		0x7F
#define QPNP_WLED_EN_SC_SHIFT		7
#define QPNP_WLED_SC_PRO_EN_DSCHGR	0x8
#define QPNP_WLED_SC_DEB_CYCLES_MIN     2
#define QPNP_WLED_SC_DEB_CYCLES_MAX     16
#define QPNP_WLED_SC_DEB_SUB            2
#define QPNP_WLED_SC_DEB_CYCLES_DFLT_AMOLED 4
#define QPNP_WLED_EXT_FET_DTEST2	0x09

#define QPNP_WLED_SEC_ACCESS_REG(b)    (b + 0xD0)
#define QPNP_WLED_SEC_UNLOCK           0xA5

#define QPNP_WLED_MAX_STRINGS		4
#define WLED_MAX_LEVEL_4095		4095
#define QPNP_WLED_RAMP_DLY_MS		20
#define QPNP_WLED_TRIGGER_NONE		"none"
#define QPNP_WLED_STR_SIZE		20
#define QPNP_WLED_MIN_MSLEEP		20
#define QPNP_WLED_SC_DLY_MS		20

enum qpnp_wled_fdbk_op {
	QPNP_WLED_FDBK_AUTO,
	QPNP_WLED_FDBK_WLED1,
	QPNP_WLED_FDBK_WLED2,
	QPNP_WLED_FDBK_WLED3,
	QPNP_WLED_FDBK_WLED4,
};

enum qpnp_wled_dim_mode {
	QPNP_WLED_DIM_ANALOG,
	QPNP_WLED_DIM_DIGITAL,
	QPNP_WLED_DIM_HYBRID,
};

static u8 qpnp_wled_ctrl_dbg_regs[] = {
	0x44, 0x46, 0x48, 0x49, 0x4b, 0x4c, 0x4d, 0x4e, 0x50, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57, 0x58, 0x5a, 0x5b, 0x5d, 0x5e, 0xe2
};

static u8 qpnp_wled_sink_dbg_regs[] = {
	0x46, 0x47, 0x48, 0x4a, 0x4b,
	0x50, 0x51, 0x52, 0x53,	0x56, 0x57, 0x58,
	0x60, 0x61, 0x62, 0x63,	0x66, 0x67, 0x68,
	0x70, 0x71, 0x72, 0x73,	0x76, 0x77, 0x78,
	0x80, 0x81, 0x82, 0x83,	0x86, 0x87, 0x88,
	0xe6,
};

struct qpnp_wled {
	struct led_classdev	cdev;
	struct spmi_device *spmi;
	struct work_struct work;
	struct mutex lock;
	enum qpnp_wled_fdbk_op fdbk_op;
	enum qpnp_wled_dim_mode dim_mode;
	int ovp_irq;
	int sc_irq;
	u32 sc_cnt;
	u32 avdd_trim_steps_from_center;
	u16 ctrl_base;
	u16 sink_base;
	u16 mod_freq_khz;
	u16 hyb_thres;
	u16 sync_dly_us;
	u16 vref_mv;
	u16 vref_psm_mv;
	u16 loop_comp_res_kohm;
	u16 loop_ea_gm;
	u16 sc_deb_cycles;
	u16 switch_freq_khz;
	u16 ovp_mv;
	u16 ilim_ma;
	u16 boost_duty_ns;
	u16 fs_curr_ua;
	u16 ramp_ms;
	u16 ramp_step;
	u16 cons_sync_write_delay_us;
	u8 strings[QPNP_WLED_MAX_STRINGS];
	u8 num_strings;
	bool en_9b_dim_res;
	bool en_phase_stag;
	bool en_cabc;
	bool disp_type_amoled;
	bool en_ext_pfet_sc_pro;
	bool prev_state;

	
	struct delayed_work flash_work;
	bool flash_enabled;
	u16 fs_curr_ua_flash;
	struct htc_flashlight_dev flash_dev;
	
	u16 hyb_thres_low, hyb_thres_low_wm;
};

static int qpnp_wled_read_reg(struct qpnp_wled *wled, u8 *data, u16 addr)
{
	int rc;

	rc = spmi_ext_register_readl(wled->spmi->ctrl, wled->spmi->sid,
							addr, data, 1);
	if (rc < 0)
		dev_err(&wled->spmi->dev,
			"Error reading address: %x(%d)\n", addr, rc);

	return rc;
}

static int qpnp_wled_write_reg(struct qpnp_wled *wled, u8 *data, u16 addr)
{
	int rc;

	rc = spmi_ext_register_writel(wled->spmi->ctrl, wled->spmi->sid,
							addr, data, 1);
	if (rc < 0)
		dev_err(&wled->spmi->dev,
			"Error writing address: %x(%d)\n", addr, rc);

	dev_dbg(&wled->spmi->dev, "write: WLED_0x%x = 0x%x\n", addr, *data);

	return rc;
}

static int qpnp_wled_sec_access(struct qpnp_wled *wled, u16 base_addr)
{
	int rc;
	u8 reg = QPNP_WLED_SEC_UNLOCK;

	rc = qpnp_wled_write_reg(wled, &reg,
		QPNP_WLED_SEC_ACCESS_REG(base_addr));
	if (rc)
		return rc;

	return 0;
}

static int qpnp_wled_set_level(struct qpnp_wled *wled, int level)
{
	int i, rc;
	u8 reg;
	u8 reg0;

	if (wled->flash_enabled)
		return -EBUSY;

	if (wled->hyb_thres_low_wm) {
		
		rc = qpnp_wled_read_reg(wled, &reg0,
				QPNP_WLED_HYB_THRES_REG(wled->sink_base));
		if (rc < 0) {
			pr_err("%s: read HYB_THRES_REG failed, rc=%d\n", __func__, rc);
		} else {
			reg = reg0 & QPNP_WLED_HYB_THRES_MASK;
			if (level >= wled->hyb_thres_low_wm)
				reg |= fls(wled->hyb_thres / QPNP_WLED_HYB_THRES_MIN) - 1;
			else
				reg |= fls(wled->hyb_thres_low / QPNP_WLED_HYB_THRES_MIN) - 1;

			pr_debug("%s: reg=[%u, %u], level=%d\n", __func__, reg0, reg, level);
			rc = qpnp_wled_write_reg(wled, &reg,
					QPNP_WLED_HYB_THRES_REG(wled->sink_base));
			if (rc)
				pr_err("%s: update HYB_THRES_REG failed: value=%u\n", __func__, reg);
		}
	}

	
	for (i = 0; i < wled->num_strings; i++) {
		reg = level & QPNP_WLED_BRIGHT_LSB_MASK;
		rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_BRIGHT_LSB_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;

		reg = level >> QPNP_WLED_BRIGHT_MSB_SHIFT;
		reg = reg & QPNP_WLED_BRIGHT_MSB_MASK;
		rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_BRIGHT_MSB_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
	}

	
	reg = QPNP_WLED_SYNC;
	rc = qpnp_wled_write_reg(wled, &reg,
		QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	if (wled->cons_sync_write_delay_us)
		usleep_range(wled->cons_sync_write_delay_us,
				wled->cons_sync_write_delay_us + 1);

	reg = QPNP_WLED_SYNC_RESET;
	rc = qpnp_wled_write_reg(wled, &reg,
		QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	return 0;
}

static int qpnp_wled_module_en(struct qpnp_wled *wled,
				u16 base_addr, bool state)
{
	int rc;
	u8 reg;

	
	if (state) {
		reg = QPNP_WLED_INT_EN_SET_OVP_DIS;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_INT_EN_SET(base_addr));
		if (rc)
			return rc;
	}

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MODULE_EN_REG(base_addr));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_MODULE_EN_MASK;
	reg |= (state << QPNP_WLED_MODULE_EN_SHIFT);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_MODULE_EN_REG(base_addr));
	if (rc)
		return rc;

	
	if (state) {
		udelay(QPNP_WLED_OVP_FLT_SLEEP_US);
		reg = QPNP_WLED_INT_EN_SET_OVP_EN;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_INT_EN_SET(base_addr));
		if (rc)
			return rc;
	}

	return 0;
}

static int qpnp_wled_flash_en_locked(struct qpnp_wled *wled, int en, int duration)
{
	int i, rc = 0;
	u8 reg;
	int fs, level, cabc;

	if (en) {
		if (wled->flash_enabled) {
			pr_info("%s: already enabled\n", __func__);
			rc = -EBUSY;
			goto exit;
		}

		if (wled->fs_curr_ua_flash > 0 && wled->fs_curr_ua_flash <= QPNP_WLED_FS_CURR_MAX_UA)
			fs = wled->fs_curr_ua_flash;
		else
			fs = QPNP_WLED_FS_CURR_MAX_UA;
		level = WLED_MAX_LEVEL_4095;
		cabc = false;

		if (duration < 10)
			duration = 10;
		schedule_delayed_work(&wled->flash_work, msecs_to_jiffies(duration));
	} else {
		fs = wled->fs_curr_ua;
		level = wled->cdev.brightness;
		cabc = wled->en_cabc;
	}
	pr_info("%s: (%d, %d) [fs=%d, level=%d], %d strings\n",
		__func__, en, duration, fs, level, wled->num_strings);

	
	for (i = 0; i < wled->num_strings; i++) {
#if 0
		reg = (cabc << QPNP_WLED_CABC_SHIFT);
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_CABC_REG(wled->sink_base, i));
		if (rc)
			goto exit;
#endif
		reg = (fs / QPNP_WLED_FS_CURR_STEP_UA) & 0x0f;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base, i));
		if (rc)
			goto exit;

		reg = level & QPNP_WLED_BRIGHT_LSB_MASK;
		rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_BRIGHT_LSB_REG(wled->sink_base, i));
		if (rc < 0)
			goto exit;

		reg = level >> QPNP_WLED_BRIGHT_MSB_SHIFT;
		reg = reg & QPNP_WLED_BRIGHT_MSB_MASK;
		rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_BRIGHT_MSB_REG(wled->sink_base, i));
		if (rc < 0)
			goto exit;
	}

	
	reg = QPNP_WLED_SYNC;
	rc = qpnp_wled_write_reg(wled, &reg, QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		goto exit;

	if (wled->cons_sync_write_delay_us)
		usleep_range(wled->cons_sync_write_delay_us,
				wled->cons_sync_write_delay_us + 1);

	reg = QPNP_WLED_SYNC_RESET;
	rc = qpnp_wled_write_reg(wled, &reg, QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		goto exit;

	wled->flash_enabled = en;
exit:
	pr_info("%s: (%d, %d) done, rc=%d\n", __func__, en, duration, rc);

	return rc;
}

static int qpnp_wled_flash_en(struct qpnp_wled *wled, int en, int duration)
{
	int rc = 0;

	if (!en) {
		cancel_delayed_work_sync(&wled->flash_work);
	}

	mutex_lock(&wled->lock);
	rc = qpnp_wled_flash_en_locked(wled, en, duration);
	mutex_unlock(&wled->lock);

	return rc;
}

static void qpnp_wled_flash_off_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct qpnp_wled *wled = container_of(dwork, struct qpnp_wled, flash_work);

	mutex_lock(&wled->lock);
	qpnp_wled_flash_en_locked(wled, 0, 0);
	mutex_unlock(&wled->lock);
}

static int qpnp_wled_flash_mode(struct htc_flashlight_dev *fl_dev, int mode1, int mode2)
{
	const int max_flash_ms = 500;
	struct qpnp_wled *wled = container_of(fl_dev, struct qpnp_wled, flash_dev);

	wled->fs_curr_ua_flash = QPNP_WLED_FS_CURR_MAX_UA;
	return qpnp_wled_flash_en(wled, mode1, max_flash_ms);
}

static int qpnp_wled_torch_mode(struct htc_flashlight_dev *fl_dev, int mode1, int mode2)
{
	const int max_torch_ms = 10000;
	struct qpnp_wled *wled = container_of(fl_dev, struct qpnp_wled, flash_dev);

	wled->fs_curr_ua_flash = wled->fs_curr_ua;
	return qpnp_wled_flash_en(wled, mode1, max_torch_ms);
}

static ssize_t qpnp_wled_ramp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int i, rc;

	mutex_lock(&wled->lock);

	if (!wled->cdev.brightness) {
		rc = qpnp_wled_module_en(wled, wled->ctrl_base, true);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled enable failed\n");
			goto unlock_mutex;
		}
	}

	
	for (i = 0; i <= wled->cdev.max_brightness;) {
		rc = qpnp_wled_set_level(wled, i);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled set level failed\n");
			goto restore_brightness;
		}

		if (wled->ramp_ms < QPNP_WLED_MIN_MSLEEP)
			usleep_range(wled->ramp_ms * USEC_PER_MSEC,
					wled->ramp_ms * USEC_PER_MSEC);
		else
			msleep(wled->ramp_ms);

		if (i == wled->cdev.max_brightness)
			break;

		i += wled->ramp_step;
		if (i > wled->cdev.max_brightness)
			i = wled->cdev.max_brightness;
	}

	
	for (i = wled->cdev.max_brightness; i >= 0;) {
		rc = qpnp_wled_set_level(wled, i);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled set level failed\n");
			goto restore_brightness;
		}

		if (wled->ramp_ms < QPNP_WLED_MIN_MSLEEP)
			usleep_range(wled->ramp_ms * USEC_PER_MSEC,
					wled->ramp_ms * USEC_PER_MSEC);
		else
			msleep(wled->ramp_ms);

		if (i == 0)
			break;

		i -= wled->ramp_step;
		if (i < 0)
			i = 0;
	}

	dev_info(&wled->spmi->dev, "wled ramp complete\n");

restore_brightness:
	
	qpnp_wled_set_level(wled, wled->cdev.brightness);
	if (!wled->cdev.brightness) {
		rc = qpnp_wled_module_en(wled, wled->ctrl_base, false);
		if (rc)
			dev_err(&wled->spmi->dev, "wled enable failed\n");
	}
unlock_mutex:
	mutex_unlock(&wled->lock);

	return count;
}

static int qpnp_wled_dump_regs(struct qpnp_wled *wled, u16 base_addr,
				u8 dbg_regs[], u8 size, char *label,
				int count, char *buf)
{
	int i, rc;
	u8 reg;

	for (i = 0; i < size; i++) {
		rc = qpnp_wled_read_reg(wled, &reg,
				base_addr + dbg_regs[i]);
		if (rc < 0)
			return rc;

		count += snprintf(buf + count, PAGE_SIZE - count,
				"%s: REG_0x%x = 0x%x\n", label,
				base_addr + dbg_regs[i], reg);

		if (count >= PAGE_SIZE)
			return PAGE_SIZE - 1;
	}

	return count;
}

static ssize_t qpnp_wled_dump_regs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int count = 0;

	count = qpnp_wled_dump_regs(wled, wled->ctrl_base,
			qpnp_wled_ctrl_dbg_regs,
			ARRAY_SIZE(qpnp_wled_ctrl_dbg_regs),
			"wled_ctrl", count, buf);

	if (count < 0 || count == PAGE_SIZE - 1)
		return count;

	count = qpnp_wled_dump_regs(wled, wled->sink_base,
			qpnp_wled_sink_dbg_regs,
			ARRAY_SIZE(qpnp_wled_sink_dbg_regs),
			"wled_sink", count, buf);

	if (count < 0 || count == PAGE_SIZE - 1)
		return count;

	return count;
}

static ssize_t qpnp_wled_ramp_ms_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wled->ramp_ms);
}

static ssize_t qpnp_wled_ramp_ms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int data;

	if (sscanf(buf, "%d", &data) != 1)
		return -EINVAL;

	wled->ramp_ms = data;
	return count;
}

static ssize_t qpnp_wled_ramp_step_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wled->ramp_step);
}

static ssize_t qpnp_wled_ramp_step_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int data;

	if (sscanf(buf, "%d", &data) != 1)
		return -EINVAL;

	wled->ramp_step = data;
	return count;
}

static ssize_t qpnp_wled_dim_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	char *str;

	if (wled->dim_mode == QPNP_WLED_DIM_ANALOG)
		str = "analog";
	else if (wled->dim_mode == QPNP_WLED_DIM_DIGITAL)
		str = "digital";
	else
		str = "hybrid";

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t qpnp_wled_dim_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	char str[QPNP_WLED_STR_SIZE + 1];
	int rc, temp;
	u8 reg;

	if (snprintf(str, QPNP_WLED_STR_SIZE, "%s", buf) > QPNP_WLED_STR_SIZE)
		return -EINVAL;

	if (strcmp(str, "analog") == 0)
		temp = QPNP_WLED_DIM_ANALOG;
	else if (strcmp(str, "digital") == 0)
		temp = QPNP_WLED_DIM_DIGITAL;
	else
		temp = QPNP_WLED_DIM_HYBRID;

	if (temp == wled->dim_mode)
		return count;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	if (temp == QPNP_WLED_DIM_HYBRID) {
		reg &= QPNP_WLED_DIM_HYB_MASK;
		reg |= (1 << QPNP_WLED_DIM_HYB_SHIFT);
	} else {
		reg &= QPNP_WLED_DIM_HYB_MASK;
		reg |= (0 << QPNP_WLED_DIM_HYB_SHIFT);
		reg &= QPNP_WLED_DIM_ANA_MASK;
		reg |= temp;
	}

	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc)
		return rc;

	wled->dim_mode = temp;

	return count;
}

static ssize_t qpnp_wled_fs_curr_ua_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wled->fs_curr_ua);
}

static ssize_t qpnp_wled_fs_curr_ua_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int data, i, rc, temp;
	u8 reg;

	if (sscanf(buf, "%d", &data) != 1)
		return -EINVAL;

	for (i = 0; i < wled->num_strings; i++) {
		if (data < QPNP_WLED_FS_CURR_MIN_UA)
			data = QPNP_WLED_FS_CURR_MIN_UA;
		else if (data > QPNP_WLED_FS_CURR_MAX_UA)
			data = QPNP_WLED_FS_CURR_MAX_UA;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
							wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_FS_CURR_MASK;
		temp = data / QPNP_WLED_FS_CURR_STEP_UA;
		reg |= temp;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
							wled->strings[i]));
		if (rc)
			return rc;
	}

	wled->fs_curr_ua = data;

	
	reg = QPNP_WLED_SYNC;
	rc = qpnp_wled_write_reg(wled, &reg,
		QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	if (wled->cons_sync_write_delay_us)
		usleep_range(wled->cons_sync_write_delay_us,
				wled->cons_sync_write_delay_us + 1);

	reg = QPNP_WLED_SYNC_RESET;
	rc = qpnp_wled_write_reg(wled, &reg,
		QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t qpnp_wled_flash_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wled->flash_enabled);
}

static ssize_t qpnp_wled_flash_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int data = 0, curr = 0, rc = 0;

	if (sscanf(buf, "%d %d", &data, &curr) < 1)
		return -EINVAL;
	wled->fs_curr_ua_flash = curr;

	rc = qpnp_wled_flash_en(wled, !!data, data);
	if (rc)
		return rc;

	return count;
}

static struct device_attribute qpnp_wled_attrs[] = {
	__ATTR(dump_regs, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_dump_regs_show,
			NULL),
	__ATTR(dim_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_dim_mode_show,
			qpnp_wled_dim_mode_store),
	__ATTR(fs_curr_ua, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_fs_curr_ua_show,
			qpnp_wled_fs_curr_ua_store),
	__ATTR(start_ramp, (S_IRUGO | S_IWUSR | S_IWGRP),
			NULL,
			qpnp_wled_ramp_store),
	__ATTR(ramp_ms, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_ramp_ms_show,
			qpnp_wled_ramp_ms_store),
	__ATTR(ramp_step, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_ramp_step_show,
			qpnp_wled_ramp_step_store),

	
	__ATTR(flash_en, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_flash_en_show,
			qpnp_wled_flash_en_store),
};

static void qpnp_wled_work(struct work_struct *work)
{
	struct qpnp_wled *wled;
	int level, rc;

	wled = container_of(work, struct qpnp_wled, work);

	level = wled->cdev.brightness;

	mutex_lock(&wled->lock);

	if (level) {
		rc = qpnp_wled_set_level(wled, level);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled set level failed\n");
			goto unlock_mutex;
		}
	}

	if (!!level != wled->prev_state) {
		rc = qpnp_wled_module_en(wled, wled->ctrl_base, !!level);

		if (rc) {
			dev_err(&wled->spmi->dev, "wled %sable failed\n",
						level ? "en" : "dis");
			goto unlock_mutex;
		}
	}

	wled->prev_state = !!level;
unlock_mutex:
	mutex_unlock(&wled->lock);
}

static enum led_brightness qpnp_wled_get(struct led_classdev *led_cdev)
{
	struct qpnp_wled *wled;

	wled = container_of(led_cdev, struct qpnp_wled, cdev);

	return wled->cdev.brightness;
}

static void qpnp_wled_set(struct led_classdev *led_cdev,
				enum led_brightness level)
{
	struct qpnp_wled *wled;

	wled = container_of(led_cdev, struct qpnp_wled, cdev);

	if (level < LED_OFF)
		level = LED_OFF;
	else if (level > wled->cdev.max_brightness)
		level = wled->cdev.max_brightness;

	wled->cdev.brightness = level;
	schedule_work(&wled->work);
}

static int qpnp_wled_set_disp(struct qpnp_wled *wled, u16 base_addr)
{
	int rc;
	u8 reg;

	
	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_DISP_SEL_REG(base_addr));
	if (rc < 0)
		return rc;

	reg &= QPNP_WLED_DISP_SEL_MASK;
	reg |= (wled->disp_type_amoled << QPNP_WLED_DISP_SEL_SHIFT);

	rc = qpnp_wled_sec_access(wled, base_addr);
	if (rc)
		return rc;

	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_DISP_SEL_REG(base_addr));
	if (rc)
		return rc;

	if (wled->disp_type_amoled) {
		
		if (wled->vref_psm_mv < QPNP_WLED_VREF_PSM_MIN_MV)
			wled->vref_psm_mv = QPNP_WLED_VREF_PSM_MIN_MV;
		else if (wled->vref_psm_mv > QPNP_WLED_VREF_PSM_MAX_MV)
			wled->vref_psm_mv = QPNP_WLED_VREF_PSM_MAX_MV;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_PSM_CTRL_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;

		reg &= QPNP_WLED_VREF_PSM_MASK;
		reg |= ((wled->vref_psm_mv - QPNP_WLED_VREF_PSM_MIN_MV)/
			QPNP_WLED_VREF_PSM_STEP_MV);
		reg |= QPNP_WLED_PSM_CTRL_OVERWRITE;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_PSM_CTRL_REG(wled->ctrl_base));
		if (rc)
			return rc;

		
		if (wled->loop_comp_res_kohm < QPNP_WLED_LOOP_COMP_RES_MIN_KOHM)
			wled->loop_comp_res_kohm =
					QPNP_WLED_LOOP_COMP_RES_MIN_KOHM;
		else if (wled->loop_comp_res_kohm >
					QPNP_WLED_LOOP_COMP_RES_MAX_KOHM)
			wled->loop_comp_res_kohm =
					QPNP_WLED_LOOP_COMP_RES_MAX_KOHM;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_VLOOP_COMP_RES_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;

		reg &= QPNP_WLED_VLOOP_COMP_RES_MASK;
		reg |= ((wled->loop_comp_res_kohm -
				 QPNP_WLED_LOOP_COMP_RES_MIN_KOHM)/
				 QPNP_WLED_LOOP_COMP_RES_STEP_KOHM);
		reg |= QPNP_WLED_VLOOP_COMP_RES_OVERWRITE;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_VLOOP_COMP_RES_REG(wled->ctrl_base));
		if (rc)
			return rc;

		
		if (wled->loop_ea_gm < QPNP_WLED_LOOP_EA_GM_MIN)
			wled->loop_ea_gm = QPNP_WLED_LOOP_EA_GM_MIN;
		else if (wled->loop_ea_gm > QPNP_WLED_LOOP_EA_GM_MAX)
			wled->loop_ea_gm = QPNP_WLED_LOOP_EA_GM_MAX;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_VLOOP_COMP_GM_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;

		reg &= QPNP_WLED_VLOOP_COMP_GM_MASK;
		reg |= (wled->loop_ea_gm | QPNP_WLED_VLOOP_COMP_GM_OVERWRITE);
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_VLOOP_COMP_GM_REG(wled->ctrl_base));
		if (rc)
			return rc;

		
		reg = 0;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_SOFTSTART_RAMP_DLY(base_addr));
		if (rc)
			return rc;

		
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_TEST4_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;

		rc = qpnp_wled_sec_access(wled, base_addr);
		if (rc)
			return rc;

		reg |= QPNP_WLED_TEST4_EN_IIND_UP;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_TEST4_REG(base_addr));
		if (rc)
			return rc;
	} else {
		rc = qpnp_wled_sec_access(wled, base_addr);
		if (rc)
			return rc;

		reg = QPNP_WLED_TEST4_EN_VREF_UP;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_TEST4_REG(base_addr));
		if (rc)
			return rc;
	}

	return 0;
}

static irqreturn_t qpnp_wled_ovp_irq(int irq, void *_wled)
{
	struct qpnp_wled *wled = _wled;

	dev_dbg(&wled->spmi->dev, "ovp detected\n");

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_wled_sc_irq(int irq, void *_wled)
{
	struct qpnp_wled *wled = _wled;

	dev_err(&wled->spmi->dev,
			"Short circuit detected %d times\n", ++wled->sc_cnt);

	qpnp_wled_module_en(wled, wled->ctrl_base, false);
	msleep(QPNP_WLED_SC_DLY_MS);
	qpnp_wled_module_en(wled, wled->ctrl_base, true);

	return IRQ_HANDLED;
}

static int qpnp_wled_config(struct qpnp_wled *wled)
{
	int rc, i, temp;
	u8 reg = 0;

	
	rc = qpnp_wled_set_disp(wled, wled->ctrl_base);
	if (rc < 0)
		return rc;

	
	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_FDBK_OP_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_FDBK_OP_MASK;
	reg |= wled->fdbk_op;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_FDBK_OP_REG(wled->ctrl_base));
	if (rc)
		return rc;

	
	if (wled->vref_mv < QPNP_WLED_VREF_MIN_MV)
		wled->vref_mv = QPNP_WLED_VREF_MIN_MV;
	else if (wled->vref_mv > QPNP_WLED_VREF_MAX_MV)
		wled->vref_mv = QPNP_WLED_VREF_MAX_MV;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_VREF_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_VREF_MASK;
	temp = wled->vref_mv - QPNP_WLED_VREF_MIN_MV;
	reg |= (temp / QPNP_WLED_VREF_STEP_MV);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_VREF_REG(wled->ctrl_base));
	if (rc)
		return rc;

	
	if (wled->ilim_ma < QPNP_WLED_ILIM_MIN_MA)
		wled->ilim_ma = QPNP_WLED_ILIM_MIN_MA;
	else if (wled->ilim_ma > QPNP_WLED_ILIM_MAX_MA)
		wled->ilim_ma = QPNP_WLED_ILIM_MAX_MA;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_ILIM_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	temp = (wled->ilim_ma / QPNP_WLED_ILIM_STEP_MA);
	if (temp != (reg & ~QPNP_WLED_ILIM_MASK)) {
		reg &= QPNP_WLED_ILIM_MASK;
		reg |= temp;
		reg |= QPNP_WLED_ILIM_OVERWRITE;
		rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_ILIM_REG(wled->ctrl_base));
		if (rc)
			return rc;
	}

	
	if (wled->boost_duty_ns < QPNP_WLED_BOOST_DUTY_MIN_NS)
		wled->boost_duty_ns = QPNP_WLED_BOOST_DUTY_MIN_NS;
	else if (wled->boost_duty_ns > QPNP_WLED_BOOST_DUTY_MAX_NS)
		wled->boost_duty_ns = QPNP_WLED_BOOST_DUTY_MAX_NS;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_BOOST_DUTY_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_BOOST_DUTY_MASK;
	reg |= (wled->boost_duty_ns / QPNP_WLED_BOOST_DUTY_STEP_NS);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_BOOST_DUTY_REG(wled->ctrl_base));
	if (rc)
		return rc;

	
	if (wled->switch_freq_khz == QPNP_WLED_SWITCH_FREQ_1600_KHZ)
		temp = QPNP_WLED_SWITCH_FREQ_1600_KHZ_CODE;
	else
		temp = QPNP_WLED_SWITCH_FREQ_800_KHZ_CODE;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_SWITCH_FREQ_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_SWITCH_FREQ_MASK;
	reg |= temp;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_SWITCH_FREQ_REG(wled->ctrl_base));
	if (rc)
		return rc;

	
	if (wled->ovp_mv <= QPNP_WLED_OVP_17800_MV) {
		wled->ovp_mv = QPNP_WLED_OVP_17800_MV;
		temp = 3;
	} else if (wled->ovp_mv <= QPNP_WLED_OVP_19400_MV) {
		wled->ovp_mv = QPNP_WLED_OVP_19400_MV;
		temp = 2;
	} else if (wled->ovp_mv <= QPNP_WLED_OVP_29500_MV) {
		wled->ovp_mv = QPNP_WLED_OVP_29500_MV;
		temp = 1;
	} else {
		wled->ovp_mv = QPNP_WLED_OVP_31000_MV;
		temp = 0;
	}

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_OVP_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_OVP_MASK;
	reg |= temp;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_OVP_REG(wled->ctrl_base));
	if (rc)
		return rc;

	if (wled->disp_type_amoled) {
		
		rc = qpnp_wled_sec_access(wled, wled->ctrl_base);
		if (rc)
			return rc;

		
		if ((s32)wled->avdd_trim_steps_from_center <
					QPNP_WLED_AVDD_MIN_TRIM_VALUE) {
			wled->avdd_trim_steps_from_center =
					QPNP_WLED_AVDD_MIN_TRIM_VALUE;
		} else if ((s32)wled->avdd_trim_steps_from_center >
						QPNP_WLED_AVDD_MAX_TRIM_VALUE) {
			wled->avdd_trim_steps_from_center =
					QPNP_WLED_AVDD_MAX_TRIM_VALUE;
		}
		reg = wled->avdd_trim_steps_from_center +
					QPNP_WLED_AVDD_TRIM_CENTER_VALUE;

		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_REF_7P7_TRIM_REG(wled->ctrl_base));
		if (rc)
			return rc;
	}

	
	if (wled->mod_freq_khz <= QPNP_WLED_MOD_FREQ_1200_KHZ) {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_1200_KHZ;
		temp = 3;
	} else if (wled->mod_freq_khz <= QPNP_WLED_MOD_FREQ_2400_KHZ) {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_2400_KHZ;
		temp = 2;
	} else if (wled->mod_freq_khz <= QPNP_WLED_MOD_FREQ_9600_KHZ) {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_9600_KHZ;
		temp = 1;
	} else if (wled->mod_freq_khz <= QPNP_WLED_MOD_FREQ_19200_KHZ) {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_19200_KHZ;
		temp = 0;
	} else {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_9600_KHZ;
		temp = 1;
	}

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_MOD_FREQ_MASK;
	reg |= (temp << QPNP_WLED_MOD_FREQ_SHIFT);

	reg &= QPNP_WLED_PHASE_STAG_MASK;
	reg |= (wled->en_phase_stag << QPNP_WLED_PHASE_STAG_SHIFT);

	reg &= QPNP_WLED_ACC_CLK_FREQ_MASK;
	reg |= (temp << QPNP_WLED_ACC_CLK_FREQ_SHIFT);

	reg &= QPNP_WLED_DIM_RES_MASK;
	reg |= (wled->en_9b_dim_res << QPNP_WLED_DIM_RES_SHIFT);

	if (wled->dim_mode == QPNP_WLED_DIM_HYBRID) {
		reg &= QPNP_WLED_DIM_HYB_MASK;
		reg |= (1 << QPNP_WLED_DIM_HYB_SHIFT);
	} else {
		reg &= QPNP_WLED_DIM_HYB_MASK;
		reg |= (0 << QPNP_WLED_DIM_HYB_SHIFT);
		reg &= QPNP_WLED_DIM_ANA_MASK;
		reg |= wled->dim_mode;
	}

	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc)
		return rc;

	
	if (wled->hyb_thres < QPNP_WLED_HYB_THRES_MIN)
		wled->hyb_thres = QPNP_WLED_HYB_THRES_MIN;
	else if (wled->hyb_thres > QPNP_WLED_HYB_THRES_MAX)
		wled->hyb_thres = QPNP_WLED_HYB_THRES_MAX;

	if (wled->hyb_thres_low < QPNP_WLED_HYB_THRES_MIN)
		wled->hyb_thres_low = QPNP_WLED_HYB_THRES_MIN;
	else if (wled->hyb_thres_low > QPNP_WLED_HYB_THRES_MAX)
		wled->hyb_thres_low = QPNP_WLED_HYB_THRES_MAX;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_HYB_THRES_REG(wled->sink_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_HYB_THRES_MASK;
	temp = fls(wled->hyb_thres / QPNP_WLED_HYB_THRES_MIN) - 1;
	reg |= temp;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_HYB_THRES_REG(wled->sink_base));
	if (rc)
		return rc;

	
	if (wled->dim_mode == QPNP_WLED_DIM_DIGITAL)
		reg = QPNP_WLED_SINK_TEST5_DIG;
	else
		reg = QPNP_WLED_SINK_TEST5_HYB;

	rc = qpnp_wled_sec_access(wled, wled->sink_base);
	if (rc)
		return rc;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_SINK_TEST5_REG(wled->sink_base));
	if (rc)
		return rc;

	
	reg = 0x00;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_CURR_SINK_REG(wled->sink_base));

	for (i = 0; i < wled->num_strings; i++) {
		if (wled->strings[i] >= QPNP_WLED_MAX_STRINGS) {
			dev_err(&wled->spmi->dev, "Invalid string number\n");
			return -EINVAL;
		}

		
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_MOD_EN_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_MOD_EN_MASK;
		reg |= (QPNP_WLED_MOD_EN << QPNP_WLED_MOD_EN_SHFT);

		if (wled->dim_mode == QPNP_WLED_DIM_HYBRID)
			reg &= QPNP_WLED_GATE_DRV_MASK;
		else
			reg |= ~QPNP_WLED_GATE_DRV_MASK;

		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_MOD_EN_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		
		if (wled->sync_dly_us > QPNP_WLED_SYNC_DLY_MAX_US)
			wled->sync_dly_us = QPNP_WLED_SYNC_DLY_MAX_US;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_SYNC_DLY_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_SYNC_DLY_MASK;
		temp = wled->sync_dly_us / QPNP_WLED_SYNC_DLY_STEP_US;
		reg |= temp;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_SYNC_DLY_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		
		if (wled->fs_curr_ua > QPNP_WLED_FS_CURR_MAX_UA)
			wled->fs_curr_ua = QPNP_WLED_FS_CURR_MAX_UA;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_FS_CURR_MASK;
		temp = wled->fs_curr_ua / QPNP_WLED_FS_CURR_STEP_UA;
		reg |= temp;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_CABC_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_CABC_MASK;
		reg |= (wled->en_cabc << QPNP_WLED_CABC_SHIFT);
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_CABC_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_CURR_SINK_REG(wled->sink_base));
		if (rc < 0)
			return rc;
		temp = wled->strings[i] + QPNP_WLED_CURR_SINK_SHIFT;
		reg |= (1 << temp);
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_CURR_SINK_REG(wled->sink_base));
		if (rc)
			return rc;
	}

	
	if (wled->ovp_irq >= 0) {
		rc = devm_request_threaded_irq(&wled->spmi->dev, wled->ovp_irq,
			NULL, qpnp_wled_ovp_irq,
			QPNP_IRQ_FLAGS,
			"qpnp_wled_ovp_irq", wled);
		if (rc < 0) {
			dev_err(&wled->spmi->dev,
				"Unable to request ovp(%d) IRQ(err:%d)\n",
				wled->ovp_irq, rc);
			return rc;
		}
	}

	if (wled->sc_irq >= 0) {
		wled->sc_cnt = 0;
		rc = devm_request_threaded_irq(&wled->spmi->dev, wled->sc_irq,
			NULL, qpnp_wled_sc_irq,
			QPNP_IRQ_FLAGS,
			"qpnp_wled_sc_irq", wled);
		if (rc < 0) {
			dev_err(&wled->spmi->dev,
				"Unable to request sc(%d) IRQ(err:%d)\n",
				wled->sc_irq, rc);
			return rc;
		}

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_SC_PRO_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_EN_SC_MASK;
		reg |= 1 << QPNP_WLED_EN_SC_SHIFT;

		if (wled->disp_type_amoled) {
			if (wled->sc_deb_cycles < QPNP_WLED_SC_DEB_CYCLES_MIN)
				wled->sc_deb_cycles =
						 QPNP_WLED_SC_DEB_CYCLES_MIN;
			else if (wled->sc_deb_cycles >
						 QPNP_WLED_SC_DEB_CYCLES_MAX)
				wled->sc_deb_cycles =
						 QPNP_WLED_SC_DEB_CYCLES_MAX;

			temp = fls(wled->sc_deb_cycles) - QPNP_WLED_SC_DEB_SUB;
			reg |= ((temp << 1) | QPNP_WLED_SC_PRO_EN_DSCHGR);
		}

		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_SC_PRO_REG(wled->ctrl_base));
		if (rc)
			return rc;

		if (wled->en_ext_pfet_sc_pro) {
			rc = qpnp_wled_sec_access(wled, wled->ctrl_base);
			if (rc)
				return rc;

			reg = QPNP_WLED_EXT_FET_DTEST2;
			rc = qpnp_wled_write_reg(wled, &reg,
					QPNP_WLED_TEST1_REG(wled->ctrl_base));
			if (rc)
				return rc;
		}
	}

	return 0;
}

static int qpnp_wled_parse_dt(struct qpnp_wled *wled)
{
	struct spmi_device *spmi = wled->spmi;
	struct property *prop;
	const char *temp_str;
	u32 temp_val;
	int rc, i;
	u8 *strings;

	wled->cdev.name = "wled";
	rc = of_property_read_string(spmi->dev.of_node,
			"linux,name", &wled->cdev.name);
	if (rc && (rc != -EINVAL)) {
		dev_err(&spmi->dev, "Unable to read led name\n");
		return rc;
	}

	wled->cdev.default_trigger = QPNP_WLED_TRIGGER_NONE;
	rc = of_property_read_string(spmi->dev.of_node, "linux,default-trigger",
					&wled->cdev.default_trigger);
	if (rc && (rc != -EINVAL)) {
		dev_err(&spmi->dev, "Unable to read led trigger\n");
		return rc;
	}

	wled->disp_type_amoled = of_property_read_bool(spmi->dev.of_node,
				"qcom,disp-type-amoled");
	if (wled->disp_type_amoled) {
		wled->vref_psm_mv = QPNP_WLED_VREF_PSM_DFLT_AMOLED_MV;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,vref-psm-mv", &temp_val);
		if (!rc) {
			wled->vref_psm_mv = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read vref-psm\n");
			return rc;
		}

		wled->loop_comp_res_kohm =
			QPNP_WLED_LOOP_COMP_RES_DFLT_AMOLED_KOHM;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,loop-comp-res-kohm", &temp_val);
		if (!rc) {
			wled->loop_comp_res_kohm = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read loop-comp-res-kohm\n");
			return rc;
		}

		wled->loop_ea_gm = QPNP_WLED_LOOP_EA_GM_DFLT_AMOLED;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,loop-ea-gm", &temp_val);
		if (!rc) {
			wled->loop_ea_gm = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read loop-ea-gm\n");
			return rc;
		}

		wled->sc_deb_cycles = QPNP_WLED_SC_DEB_CYCLES_DFLT_AMOLED;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,sc-deb-cycles", &temp_val);
		if (!rc) {
			wled->sc_deb_cycles = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read sc debounce cycles\n");
			return rc;
		}

		wled->avdd_trim_steps_from_center = 0;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,avdd-trim-steps-from-center", &temp_val);
		if (!rc) {
			wled->avdd_trim_steps_from_center = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read avdd trim steps from center value\n");
			return rc;
		}
	}

	wled->fdbk_op = QPNP_WLED_FDBK_AUTO;
	rc = of_property_read_string(spmi->dev.of_node,
			"qcom,fdbk-output", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "wled1") == 0)
			wled->fdbk_op = QPNP_WLED_FDBK_WLED1;
		else if (strcmp(temp_str, "wled2") == 0)
			wled->fdbk_op = QPNP_WLED_FDBK_WLED2;
		else if (strcmp(temp_str, "wled3") == 0)
			wled->fdbk_op = QPNP_WLED_FDBK_WLED3;
		else if (strcmp(temp_str, "wled4") == 0)
			wled->fdbk_op = QPNP_WLED_FDBK_WLED4;
		else
			wled->fdbk_op = QPNP_WLED_FDBK_AUTO;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read feedback output\n");
		return rc;
	}

	wled->vref_mv = QPNP_WLED_DFLT_VREF_MV;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,vref-mv", &temp_val);
	if (!rc) {
		wled->vref_mv = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read vref\n");
		return rc;
	}

	wled->switch_freq_khz = QPNP_WLED_SWITCH_FREQ_800_KHZ;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,switch-freq-khz", &temp_val);
	if (!rc) {
		wled->switch_freq_khz = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read switch freq\n");
		return rc;
	}

	wled->ovp_mv = QPNP_WLED_OVP_29500_MV;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,ovp-mv", &temp_val);
	if (!rc) {
		wled->ovp_mv = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read vref\n");
		return rc;
	}

	wled->ilim_ma = QPNP_WLED_DFLT_ILIM_MA;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,ilim-ma", &temp_val);
	if (!rc) {
		wled->ilim_ma = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read ilim\n");
		return rc;
	}

	wled->boost_duty_ns = QPNP_WLED_DEF_BOOST_DUTY_NS;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,boost-duty-ns", &temp_val);
	if (!rc) {
		wled->boost_duty_ns = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read boost duty\n");
		return rc;
	}

	wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_9600_KHZ;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,mod-freq-khz", &temp_val);
	if (!rc) {
		wled->mod_freq_khz = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read modulation freq\n");
		return rc;
	}

	wled->dim_mode = QPNP_WLED_DIM_HYBRID;
	rc = of_property_read_string(spmi->dev.of_node,
			"qcom,dim-mode", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "analog") == 0)
			wled->dim_mode = QPNP_WLED_DIM_ANALOG;
		else if (strcmp(temp_str, "digital") == 0)
			wled->dim_mode = QPNP_WLED_DIM_DIGITAL;
		else
			wled->dim_mode = QPNP_WLED_DIM_HYBRID;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read dim mode\n");
		return rc;
	}

	if (wled->dim_mode == QPNP_WLED_DIM_HYBRID) {
		wled->hyb_thres = QPNP_WLED_DEF_HYB_THRES;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,hyb-thres", &temp_val);
		if (!rc) {
			wled->hyb_thres = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read hyb threshold\n");
			return rc;
		}

		rc = of_property_read_u32(spmi->dev.of_node,
				"htc,hyb-thres-low", &temp_val);
		if (!rc)
			wled->hyb_thres_low = temp_val;
		rc = of_property_read_u32(spmi->dev.of_node,
				"htc,hyb-thres-low-wm", &temp_val);
		if (!rc)
			wled->hyb_thres_low_wm = temp_val;
		pr_info("%s: low setting=%u, watermark=%u\n",
			__func__, wled->hyb_thres_low, wled->hyb_thres_low_wm);
	}

	wled->sync_dly_us = QPNP_WLED_DEF_SYNC_DLY_US;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,sync-dly-us", &temp_val);
	if (!rc) {
		wled->sync_dly_us = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read sync delay\n");
		return rc;
	}

	wled->fs_curr_ua = QPNP_WLED_FS_CURR_MAX_UA;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,fs-curr-ua", &temp_val);
	if (!rc) {
		wled->fs_curr_ua = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read full scale current\n");
		return rc;
	}

	wled->cons_sync_write_delay_us = 0;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,cons-sync-write-delay-us", &temp_val);
	if (!rc)
		wled->cons_sync_write_delay_us = temp_val;

	wled->en_9b_dim_res = of_property_read_bool(spmi->dev.of_node,
			"qcom,en-9b-dim-res");
	wled->en_phase_stag = of_property_read_bool(spmi->dev.of_node,
			"qcom,en-phase-stag");
	wled->en_cabc = of_property_read_bool(spmi->dev.of_node,
			"qcom,en-cabc");

	prop = of_find_property(spmi->dev.of_node,
			"qcom,led-strings-list", &temp_val);
	if (!prop || !temp_val || temp_val > QPNP_WLED_MAX_STRINGS) {
		dev_err(&spmi->dev, "Invalid strings info, use default");
		wled->num_strings = QPNP_WLED_MAX_STRINGS;
		for (i = 0; i < wled->num_strings; i++)
			wled->strings[i] = i;
	} else {
		wled->num_strings = temp_val;
		strings = prop->value;
		for (i = 0; i < wled->num_strings; ++i)
			wled->strings[i] = strings[i];
	}

	wled->ovp_irq = spmi_get_irq_byname(spmi, NULL, "ovp-irq");
	if (wled->ovp_irq < 0)
		dev_dbg(&spmi->dev, "ovp irq is not used\n");

	wled->sc_irq = spmi_get_irq_byname(spmi, NULL, "sc-irq");
	if (wled->sc_irq < 0)
		dev_dbg(&spmi->dev, "sc irq is not used\n");

	wled->en_ext_pfet_sc_pro = of_property_read_bool(spmi->dev.of_node,
					"qcom,en-ext-pfet-sc-pro");

	return 0;
}

static int qpnp_wled_probe(struct spmi_device *spmi)
{
	struct qpnp_wled *wled;
	struct resource *wled_resource;
	int rc, i;

	wled = devm_kzalloc(&spmi->dev, sizeof(*wled), GFP_KERNEL);
	if (!wled)
		return -ENOMEM;

	wled->spmi = spmi;

	wled_resource = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM,
					QPNP_WLED_SINK_BASE);
	if (!wled_resource) {
		dev_err(&spmi->dev, "Unable to get wled sink base address\n");
		return -EINVAL;
	}

	wled->sink_base = wled_resource->start;

	wled_resource = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM,
					QPNP_WLED_CTRL_BASE);
	if (!wled_resource) {
		dev_err(&spmi->dev, "Unable to get wled ctrl base address\n");
		return -EINVAL;
	}

	wled->ctrl_base = wled_resource->start;

	dev_set_drvdata(&spmi->dev, wled);

	rc = qpnp_wled_parse_dt(wled);
	if (rc) {
		dev_err(&spmi->dev, "DT parsing failed\n");
		return rc;
	}

	rc = qpnp_wled_config(wled);
	if (rc) {
		dev_err(&spmi->dev, "wled config failed\n");
		return rc;
	}

	mutex_init(&wled->lock);
	INIT_WORK(&wled->work, qpnp_wled_work);
	wled->ramp_ms = QPNP_WLED_RAMP_DLY_MS;
	wled->ramp_step = 1;

	wled->cdev.brightness_set = qpnp_wled_set;
	wled->cdev.brightness_get = qpnp_wled_get;

	wled->cdev.max_brightness = WLED_MAX_LEVEL_4095;

	INIT_DELAYED_WORK(&wled->flash_work, qpnp_wled_flash_off_work);

	rc = led_classdev_register(&spmi->dev, &wled->cdev);
	if (rc) {
		dev_err(&spmi->dev, "wled registration failed(%d)\n", rc);
		goto wled_register_fail;
	}

	for (i = 0; i < ARRAY_SIZE(qpnp_wled_attrs); i++) {
		rc = sysfs_create_file(&wled->cdev.dev->kobj,
				&qpnp_wled_attrs[i].attr);
		if (rc < 0) {
			dev_err(&spmi->dev, "sysfs creation failed\n");
			goto sysfs_fail;
		}
	}

	wled->flash_dev.id = 1;
	wled->flash_dev.flash_func = qpnp_wled_flash_mode;
	wled->flash_dev.torch_func = qpnp_wled_torch_mode;

	if (register_htc_flashlight(&wled->flash_dev)) {
		pr_err("%s: register htc_flashlight failed!\n", __func__);
		wled->flash_dev.id = -1;
	}

	return 0;

sysfs_fail:
	for (i--; i >= 0; i--)
		sysfs_remove_file(&wled->cdev.dev->kobj,
				&qpnp_wled_attrs[i].attr);
	led_classdev_unregister(&wled->cdev);
wled_register_fail:
	cancel_work_sync(&wled->work);
	mutex_destroy(&wled->lock);
	return rc;
}

static int qpnp_wled_remove(struct spmi_device *spmi)
{
	struct qpnp_wled *wled = dev_get_drvdata(&spmi->dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(qpnp_wled_attrs); i++)
		sysfs_remove_file(&wled->cdev.dev->kobj,
				&qpnp_wled_attrs[i].attr);

	if (wled->flash_dev.id >= 0) {
		unregister_htc_flashlight(&wled->flash_dev);
		wled->flash_dev.id = -1;
	}

	led_classdev_unregister(&wled->cdev);
	cancel_work_sync(&wled->work);
	mutex_destroy(&wled->lock);

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,qpnp-wled",},
	{ },
};

static struct spmi_driver qpnp_wled_driver = {
	.driver		= {
		.name	= "qcom,qpnp-wled",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_wled_probe,
	.remove		= qpnp_wled_remove,
};

static int __init qpnp_wled_init(void)
{
	return spmi_driver_register(&qpnp_wled_driver);
}
module_init(qpnp_wled_init);

static void __exit qpnp_wled_exit(void)
{
	spmi_driver_unregister(&qpnp_wled_driver);
}
module_exit(qpnp_wled_exit);

MODULE_DESCRIPTION("QPNP WLED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp-wled");