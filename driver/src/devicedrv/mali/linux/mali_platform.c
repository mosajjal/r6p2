/**
 * Copyright (C) 2016 Allwinner Technology Limited. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Author: Albert Yu <yuxyun@allwinnertech.com>
 */

#include "mali_kernel_common.h"
#include "mali_osk.h"
#include <linux/mali/mali_utgard.h>
#include <linux/platform_device.h>

#include <linux/err.h>
#include <linux/module.h>  
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <mach/irqs.h>
#ifdef CONFIG_ARCH_SUN7I
#include <mach/clock.h>
#include <mach/includes.h>
#else /* CONFIG_ARCH_SUN7I */
#include <linux/clk/sunxi_name.h>
#endif /* CONFIG_ARCH_SUN7I */
#include <mach/sys_config.h>
#include <mach/platform.h>
#include <linux/dma-mapping.h>

#ifdef CONFIG_CPU_BUDGET_THERMAL
#include <linux/cpu_budget_cooling.h>
#endif /* CONFIG_CPU_BUDGET_THERMAL */

#if defined CONFIG_ARCH_SUN7I
#define GPU_PBASE SW_PA_MALI_IO_BASE
#define IRQ_GPU_GP AW_IRQ_GPU_GP
#define IRQ_GPU_GPMMU AW_IRQ_GPU_GPMMU
#define IRQ_GPU_PP0 AW_IRQ_GPU_PP0
#define IRQ_GPU_PPMMU0 AW_IRQ_GPU_PPMMU0
#define IRQ_GPU_PP1 AW_IRQ_GPU_PP1
#define IRQ_GPU_PPMMU1 AW_IRQ_GPU_PPMMU1

#define CLK_MALI CLK_MOD_MALI
#define CLK_PLL CLK_SYS_PLL8

#define MALI_FREQ 336

static struct clk *clk_ahb  = NULL;

#elif defined CONFIG_ARCH_SUN8IW7P1
#define GPU_PBASE SUNXI_GPU_PBASE
#define IRQ_GPU_GP SUNXI_IRQ_GPU_GP
#define IRQ_GPU_GPMMU SUNXI_IRQ_GPU_GPMMU
#define IRQ_GPU_PP0 SUNXI_IRQ_GPU_PP0
#define IRQ_GPU_PPMMU0 SUNXI_IRQ_GPU_PPMMU0
#define IRQ_GPU_PP1 SUNXI_IRQ_GPU_PP1
#define IRQ_GPU_PPMMU1 SUNXI_IRQ_GPU_PPMMU1

#define CLK_MALI GPU_CLK
#define CLK_PLL PLL_GPU_CLK

#define MALI_FREQ 576
struct __fb_addr_para
{
	unsigned int fb_paddr;
	unsigned int fb_size;
};
extern void sunxi_get_fb_addr_para(struct __fb_addr_para *fb_addr_para);

#ifdef CONFIG_SUNXI_THERMAL
extern int ths_read_data(int value);
#endif /* CONFIG_SUNXI_THERMAL */

static int cur_mode = 0;

#ifdef CONFIG_CPU_BUDGET_THERMAL
static unsigned int thermal_ctrl_freq[] = {576, 432, 312, 120};

/* This data is for sensor, but the data of gpu may be 5 degrees Celsius higher */
static unsigned int temperature_threshold[] = {80, 90, 100};  /* Degrees Celsius  */
#endif /* CONFIG_CPU_BUDGET_THERMAL */
#else /* CONFIG_ARCH_SUN7I */
#error "please select a platform\n"
#endif /* CONFIG_ARCH_SUN7I */

static void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);

extern unsigned long totalram_pages;

static struct clk *clk_mali  = NULL;
static struct clk *clk_pll   = NULL;

static struct mutex mali_lock;

static int clk_enable_status = 0;

static struct resource mali_gpu_resources[] = {
	MALI_GPU_RESOURCES_MALI400_MP2_PMU(GPU_PBASE,
										IRQ_GPU_GP,
										IRQ_GPU_GPMMU,
                                        IRQ_GPU_PP0,
										IRQ_GPU_PPMMU0,
										IRQ_GPU_PP1,
										IRQ_GPU_PPMMU1)
};

static struct platform_device mali_gpu_device =
{
    .name = MALI_GPU_NAME_UTGARD,
    .id = 0,
    .dev.coherent_dma_mask = DMA_BIT_MASK(32),
};

static struct mali_gpu_device_data mali_gpu_data = 
{
    .control_interval = 500,
	.utilization_callback = mali_gpu_utilization_callback,
};

static int get_gpu_clk(void)
{
#ifdef CONFIG_ARCH_SUN7I
	/* Get GPU AHB clock */
    clk_ahb = clk_get(NULL, CLK_AHB_MALI); 
	if(!clk_ahb || IS_ERR(clk_ahb)){
		MALI_PRINT_ERROR(("Failed to get AHB clock!\n"));
        return -1;
	}
#endif /* CONFIG_ARCH_SUN7I */

	/* Get Mali clock */
	clk_mali = clk_get(NULL, CLK_MALI);
	if(!clk_mali || IS_ERR(clk_mali)){
		MALI_PRINT_ERROR(("Failed to get Mali clock!\n"));
        return -1;
	}

	/* Get GPU PLL */
	clk_pll = clk_get(NULL, CLK_PLL);
	if(!clk_pll || IS_ERR(clk_pll)){
		MALI_PRINT_ERROR(("Failed to get PLL clock!\n"));
        return -1;
	}

	return 0;
}

static void put_gpu_clk(void)
{
	clk_put(clk_pll);

	clk_put(clk_mali);

#ifdef CONFIG_ARCH_SUN7I
	clk_put(clk_ahb);
#endif /* CONFIG_ARCH_SUN7I */
}

static int set_clk_parent(void)
{
	if(clk_set_parent(clk_mali, clk_pll))
	{
		MALI_PRINT_ERROR(("Failed to set the parent of Mali clock"));
        return -1;
	}

	return 0;
}

void enable_gpu_clk(void)
{
	if (clk_enable_status)
		return;

#ifdef CONFIG_ARCH_SUN7I
	if (clk_prepare_enable(clk_ahb)) {
		MALI_PRINT_ERROR(("Failed to enable AHB clock!\n"));
		BUG();
		return;
	}
#endif /* CONFIG_ARCH_SUN7I */

	if (clk_prepare_enable(clk_pll)) {
		MALI_PRINT_ERROR(("Failed to enable PLL clock!\n"));
		BUG();
		return;
	}

	if (clk_prepare_enable(clk_mali)) {
		MALI_PRINT_ERROR(("Failed to enable Mali clock!\n"));
		BUG();
		return;
	}

	clk_enable_status = 1;
}

void disable_gpu_clk(void)
{
	if (!clk_enable_status)
		return;

	clk_disable_unprepare(clk_mali);
	clk_disable_unprepare(clk_pll);
#ifdef CONFIG_ARCH_SUN7I
	clk_disable_unprepare(clk_ahb);
#endif /* CONFIG_ARCH_SUN7I */

	clk_enable_status = 0;
}

static void set_freq(int freq)
{
	if (freq <= 0 || freq == clk_get_rate(clk_pll)/(1000*1000))
		return;

	if (clk_set_rate(clk_pll, freq*1000*1000)) {
		MALI_PRINT_ERROR(("Failed to set the frequency of PLL clock, \
							Current frequency is %ld MHz, the frequency to be is %d MHz\n",
							clk_get_rate(clk_pll)/(1000*1000), freq));
		return ;
	}

	if (clk_set_rate(clk_mali, freq*1000*1000)) {
		MALI_PRINT_ERROR(("Failed to set the frequency of Mali clock, \
							Current frequency is %ld MHz, the frequency to be is %d MHz\n",
							clk_get_rate(clk_mali)/(1000*1000), freq));
		return;
	}

	MALI_PRINT(("Set gpu frequency to %d MHz\n", freq));
}

#ifdef CONFIG_ARCH_SUN8IW7P1
#ifdef CONFIG_CPU_BUDGET_THERMAL
static void set_freq_wrap(int freq)
{
	if (freq <= 0 || freq == clk_get_rate(clk_pll)/(1000*1000))
		return;

	mutex_lock(&mali_lock);
	mali_dev_pause();
	set_freq(freq);
	mali_dev_resume();
	mutex_unlock(&mali_lock);
}

static int mali_throttle_notifier_call(struct notifier_block *nfb, unsigned long mode, void *cmd)
{
    int retval = NOTIFY_DONE;

	int temperature = 0;
	int i = 0;

	if(BUDGET_GPU_THROTTLE != mode) {
		set_freq_wrap(MALI_FREQ);
		goto out;
	}

#ifdef CONFIG_SUNXI_THERMAL
	temperature = ths_read_data(4);
#endif /* CONFIG_SUNXI_THERMAL */

	if (!temperature)
		goto out;

	for(i = 0; i < sizeof(temperature_threshold)/sizeof(temperature_threshold[0]); i++)
	{
		if(temperature < temperature_threshold[i])
		{
			if(cur_mode != i)
			{
				set_freq_wrap(thermal_ctrl_freq[i]);
				cur_mode = i;
			}
			goto out;
		}
	}

	set_freq_wrap(thermal_ctrl_freq[i]);
	cur_mode = i;

out:		
	return retval;
}

static struct notifier_block mali_throttle_notifier = {
.notifier_call = mali_throttle_notifier_call,
};
#endif /* CONFIG_CPU_BUDGET_THERMAL */
#endif /* CONFIG_ARCH_SUN8IW7P1 */

static int mali_platform_init(void)
{
#ifdef CONFIG_ARCH_SUN8IW7P1
	struct __fb_addr_para fb_addr_para = {0};
	sunxi_get_fb_addr_para(&fb_addr_para);
#endif /* CONFIG_ARCH_SUN7I */

#ifdef CONFIG_ARCH_SUN7I
	mali_gpu_data.fb_start = ION_CARVEOUT_MEM_BASE;
	mali_gpu_data.fb_size = sun7i_ion_carveout_size();
#elif defined CONFIG_ARCH_SUN8IW7P1
	mali_gpu_data.fb_start = fb_addr_para.fb_paddr;
	mali_gpu_data.fb_size = fb_addr_para.fb_size;
#else
#error "please select a platform\n"
#endif /* CONFIG_ARCH_SUN7I */
	mali_gpu_data.shared_mem_size = totalram_pages * PAGE_SIZE; /* B */

	if (get_gpu_clk())
		return -1;

	if (set_clk_parent())
		return -1;

	set_freq(MALI_FREQ);

#ifdef CONFIG_ARCH_SUN7I
	if(clk_reset(clk_mali, AW_CCU_CLK_NRESET)){
		MALI_PRINT(("try to reset release failed!\n"));
        return -1;
	}
#endif /* CONFIG_ARCH_SUN7I */

	enable_gpu_clk();

#ifdef CONFIG_CPU_BUDGET_THERMAL
	register_budget_cooling_notifier(&mali_throttle_notifier);
#endif /* CONFIG_CPU_BUDGET_THERMAL */

	mutex_init(&mali_lock);

    MALI_SUCCESS;
}

int mali_platform_device_register(void)
{
    int err;

	if(mali_platform_init())
		return -1;

	err = platform_device_add_resources(&mali_gpu_device,
										mali_gpu_resources,
										sizeof(mali_gpu_resources) / sizeof(mali_gpu_resources[0]));

	if (err) {
		MALI_PRINT_ERROR(("platform_device_add_resources failed!\n"));
		goto out;
	}

	err = platform_device_register(&mali_gpu_device);
	if (err) {
		MALI_PRINT_ERROR(("platform_device_register failed!\n"));
		platform_device_unregister(&mali_gpu_device);
		goto out;
	}

	err = platform_device_add_data(&mali_gpu_device, &mali_gpu_data, sizeof(mali_gpu_data));
	if (err) {
		MALI_PRINT_ERROR(("platform_device_add_data failed!\n"));
		goto out;
	}

#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), 1000);
	pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
#endif
	pm_runtime_enable(&(mali_gpu_device.dev));
#endif

out:
    return err;
}

int mali_platform_device_unregister(void)
{
	platform_device_unregister(&mali_gpu_device);

	disable_gpu_clk();

	put_gpu_clk();

	return 0;
}

static void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
}