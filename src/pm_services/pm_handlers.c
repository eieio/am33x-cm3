/*
 * AM33XX-CM3 firmware
 *
 * Cortex-M3 (CM3) firmware for power management on Texas Instruments' AM33XX series of SoCs
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 *  This software is licensed under the  standard terms and conditions in the Texas Instruments  Incorporated
 *  Technology and Software Publicly Available Software License Agreement , a copy of which is included in the
 *  software download.
*/

#include <stdint.h>

#include <cm3.h>
#include <device_am335x.h>
#include <low_power.h>
#include <system_am335.h>

#define DEBUG	0

extern void wkup_clkdm_sleep();
extern void wkup_clkdm_wake();
extern void pd_state_restore();

/* Enter RTC mode */
/* Initiates a clean shutdown */
void a8_lp_cmd1_handler(struct cmd_data *data, char use_default_val)
{
	struct rtc_data *local_cmd = (struct rtc_data *)data->data;
	int timeout = 0;

	/* If RTC module if not already configured... cannot continue */
	rtc_enable_check();

	if(local_cmd->rtc_timeout_val != 0 &&
			local_cmd->rtc_timeout_val <= RTC_TIMEOUT_MAX)
		timeout = local_cmd->rtc_timeout_val;
	else
		timeout = RTC_TIMEOUT_DEFAULT;

	/* Program the RTC_PMIC register for deasseting pmic_pwr_enable */
	rtc_reg_write(RTC_PMIC_REG, 0x00010000);

	timeout += rtc_reg_read(RTC_ALARM2_SECONDS_REG);

	rtc_reg_write(RTC_ALARM2_SECONDS_REG, timeout);

	/* Disable all the modules - Debug mode only */
	if(DEBUG) {
		misc_modules_disable();
	}

	/* Turn off interconnect */
	interconnect_modules_disable();

	/* Disable the clock domains except MPU */
	clkdm_sleep();

	/* Disable A8 clock now */
	mpu_disable();

	/* Disable MPU clock domain */
	mpu_clkdm_sleep();

	wkup_clkdm_sleep();

	/* TODO: wait for power domain state change interrupt from PRCM */
}

/* Enter RTC_fast mode */
/* Probably this can be a common function for RTC and RTC_fast */
void a8_lp_cmd2_handler(struct cmd_data *data, char use_default_val)
{
	struct rtc_data *rtc_data = (struct rtc_data *)data->data;
	int timeout = 0;

	if(!rtc_data->rtc_timeout_val &&
		(rtc_data->rtc_timeout_val < RTC_TIMEOUT_MAX))
		timeout = rtc_data->rtc_timeout_val;
	else
		timeout = RTC_TIMEOUT_DEFAULT;

	/* If RTC module if not already configured... cannot continue */
	rtc_enable_check();

	/* Program the RTC_PMIC register for deasseting pmic_pwr_enable */
	__raw_writel(0x00010000, RTC_PMIC_REG);

	/* Read the RTC_ALARM2_SECS */
	timeout += __raw_readl(RTC_ALARM2_SECONDS_REG);

	/* Write val + timeout to RTC_ALARM */
	__raw_writel(timeout, RTC_ALARM2_SECONDS_REG);
}

/* Enter DeepSleep0 mode
 * MOSC = OFF
 * PD_PER = RET
 * PD_MPU = RET
 */
void a8_lp_cmd3_handler(struct cmd_data *data, char use_default_val)
{
	struct deep_sleep_data *local_cmd = (struct deep_sleep_data *)data->data;

	int per_st = 0;
	int mpu_st = 0;

	/* Disable MOSC if defaults are required or if user asked for it */
	if(use_default_val || !(local_cmd->mosc_state)) {
		disable_master_oscillator();
	}

	configure_wake_sources(local_cmd->wake_sources, use_default_val);

	/* TODO: Check for valid range */
	if(!(use_default_val) && (local_cmd->deepsleep_count))
		configure_deepsleep_count(local_cmd->deepsleep_count);
	else
		configure_deepsleep_count(DS_COUNT_DEFAULT);

	per_st = get_pd_per_stctrl_val(0);
	mpu_st = get_pd_mpu_stctrl_val(0);

	/* VDD MPU lowering does not make sense here so ignore that field */

	if (!use_default_val) {
		mpu_ram_ret_state_change(local_cmd->pd_mpu_ram_ret_state, mpu_st);
		mpu_l1_ret_state_change(local_cmd->pd_mpu_l1_ret_state, mpu_st);
		mpu_l2_ret_state_change(local_cmd->pd_mpu_l2_ret_state, mpu_st);
		icss_mem_ret_state_change(local_cmd->pd_per_icss_mem_ret_state, per_st);
		per_mem_ret_state_change(local_cmd->pd_per_mem_ret_state, per_st);
		ocmc_mem_ret_state_change(local_cmd->pd_per_ocmc_ret_state, per_st);
	}

	/* MPU power domain state change */
	pd_state_change(mpu_st, PD_MPU);

	/* PER power domain state change */
	pd_state_change(per_st, PD_PER);

	/* Disable all the modules - Debug mode only */
	if(DEBUG) {
		misc_modules_disable();
	}

	/* XXX: New addition to resolve any issues that A8 might have */
	essential_modules_disable();

	interconnect_modules_disable();

	mpu_disable();

	mpu_clkdm_sleep();

	clkdm_sleep();

	wkup_clkdm_sleep();

	/* TODO: wait for power domain state change interrupt from PRCM */
}

/* Enter DeepSleep1 mode
 * MOSC = OFF
 * PD_PER = ON
 * PD_MPU = RET
 */
void a8_lp_cmd5_handler(struct cmd_data *data, char use_default_val)
{
	struct deep_sleep_data *local_cmd = (struct deep_sleep_data *)data->data;

	int per_st = 0;
	int mpu_st = 0;

	/* Disable MOSC if possible */
	if(use_default_val || !(local_cmd->mosc_state)) {
		disable_master_oscillator();
	}

	configure_wake_sources(local_cmd->wake_sources, use_default_val);

	/* TODO: Check for valid range */
	if(!(use_default_val) && (local_cmd->deepsleep_count))
		configure_deepsleep_count(local_cmd->deepsleep_count);
	else
		configure_deepsleep_count(DS_COUNT_DEFAULT);

	per_st = get_pd_per_stctrl_val(1);
	mpu_st = get_pd_mpu_stctrl_val(1);

	if (!use_default_val) {
		mpu_ram_ret_state_change(local_cmd->pd_mpu_ram_ret_state, mpu_st);
		mpu_l1_ret_state_change(local_cmd->pd_mpu_l1_ret_state, mpu_st);
		mpu_l2_ret_state_change(local_cmd->pd_mpu_l2_ret_state, mpu_st);
		icss_mem_on_state_change(local_cmd->pd_per_icss_mem_on_state, per_st);
		per_mem_on_state_change(local_cmd->pd_per_mem_on_state, per_st);
		ocmc_mem_on_state_change(local_cmd->pd_per_ocmc_on_state, per_st);
	}

	/* MPU power domain state change */
	pd_state_change(mpu_st, PD_MPU);

	/* PER power domain state change */
	pd_state_change(per_st, PD_PER);

	mpu_disable();

	mpu_clkdm_sleep();

	wkup_clkdm_sleep();

	/* TODO: wait for power domain state change interrupt from PRCM */
}

/* Enter DeepSleep2 mode
 * MOSC = OFF
 * PD_PER = ON
 * PD_MPU = ON
 */
void a8_lp_cmd7_handler(struct cmd_data *data, char use_default_val)
{
	struct deep_sleep_data *local_cmd = (struct deep_sleep_data *)data->data;

	int per_st = 0;
	int mpu_st = 0;

	/* Disable MOSC if possible */
	if(use_default_val || !(local_cmd->mosc_state)) {
		disable_master_oscillator();
	}

	configure_wake_sources(local_cmd->wake_sources, use_default_val);

	/* TODO: Check for valid range */
	if(!(use_default_val) && (local_cmd->deepsleep_count))
		configure_deepsleep_count(local_cmd->deepsleep_count);
	else
		configure_deepsleep_count(DS_COUNT_DEFAULT);

	per_st = get_pd_per_stctrl_val(2);
	mpu_st = get_pd_mpu_stctrl_val(2);

	if (!use_default_val) {
		mpu_ram_on_state_change(local_cmd->pd_mpu_ram_on_state, mpu_st);
		icss_mem_on_state_change(local_cmd->pd_per_icss_mem_on_state, per_st);
		per_mem_on_state_change(local_cmd->pd_per_mem_on_state, per_st);
		ocmc_mem_on_state_change(local_cmd->pd_per_ocmc_on_state, per_st);
	}

	/* MPU power domain state change */
	pd_state_change(mpu_st, PD_MPU);

	/* PER power domain state change */
	pd_state_change(per_st, PD_PER);

	mpu_disable();

	wkup_clkdm_sleep();

	/*TODO: wait for power domain state change interrupt from PRCM */
}

/* Standalone application handler */
void a8_standalone_handler(struct cmd_data *data)
{
	/* TBD */
#if 0
	return app_handler((void *)load_addr, );
#endif
}

/* All wake interrupts invoke this function */
void generic_wake_handler()
{
	int i = 0;

	/* Right now assuming that the cmd_id is a valid reflection of what we did
	 * But this should read the trace vector to find out where we are
	 */
	switch(cmd_global_data.cmd_id) {
	case 0x1:
		a8_wake_cmd1_handler();	/* RTC */
		break;
	case 0x2:
		a8_wake_cmd2_handler();	/* RTC_fast */
		break;
	case 0x3:
		a8_wake_cmd3_handler();	/* DS0 */
		break;
	case 0x5:
		a8_wake_cmd5_handler();	/* DS1 */
		break;
	case 0x7:
		a8_wake_cmd7_handler();	/* DS2 */
		break;
	case 0xff:
	default:
		while(1)
		;
	}

	enable_master_oscillator();

	/* If everything is done, we init things again */
	/* Flush out NVIC interrupts */
	for(i=0; i<AM335X_NUM_EXT_INTERRUPTS; i++)
	{
		nvic_disable_irq(i);
		nvic_clear_irq(i);
	}

	msg_init();

	trace_init();

	pm_init();

	/* Enable only the MBX IRQ */
	nvic_enable_irq(AM335X_IRQ_MBINT0);

	/* Enable MPU only after we are sure that we are done with the wakeup */
	mpu_enable();
}

/* Exit RTC mode */
void a8_wake_cmd1_handler()
{
	/* RTC wake is a cold boot... so this doesn't make sense */
}

/* Exit RTC_fast mode */
void a8_wake_cmd2_handler()
{
	/* RTC fast wake is also similar to cold boot... */
}

/* Exit DeepSleep0 mode
 * MOSC = OFF
 * PD_PER = RET
 * PD_MPU = RET
 */
void a8_wake_cmd3_handler()
{
	int result = 0;

	result = verify_pd_transitions();

	pd_state_restore();

	wkup_clkdm_wake();

	clkdm_wake();

	interconnect_modules_enable();

	essential_modules_enable();

	msg_cmd_stat_update(result);

	clear_wake_sources();

	mpu_clkdm_wake();
}

/* Exit DeepSleep1 mode
 * MOSC = OFF
 * PD_PER = ON
 * PD_MPU = RET
 */
void a8_wake_cmd5_handler()
{
	int result = 0;

	result = verify_pd_transitions();

	pd_state_restore();

	wkup_clkdm_wake();

	essential_modules_enable();

	msg_cmd_stat_update(result);

	clear_wake_sources();

	mpu_clkdm_wake();
}

/* Exit DeepSleep2 mode
 * MOSC = OFF
 * PD_PER = ON
 * PD_MPU = ON
 */
void a8_wake_cmd7_handler()
{
	int result = 0;

	result = verify_pd_transitions();

	pd_state_restore();

	wkup_clkdm_wake();

	msg_cmd_stat_update(result);

	/* For A8 to get the module interrupt, the clock needs to be ungated */
	enable_wake_sources_for_ds2();

	clear_wake_sources();
}