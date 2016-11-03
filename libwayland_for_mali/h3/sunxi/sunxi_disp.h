/*
 * Copyright (c) 2015 Allwinnertech Co., Ltd.
 * Authors:
 * Jet Cui 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef _SUNXI_DRM_DISP_H_
#define _SUNXI_DRM_DISP_H_

#include <video/sunxi_display2.h>
#include <disp_private.h>

unsigned int bsp_disp_device_switch(int disp, disp_output_type output_type, disp_tv_mode mode);

struct disp_manager *disp_get_layer_manager(u32 disp);

extern unsigned int disp_unregister_sync_finish_proc(void (*proc)(unsigned int));
extern unsigned int  disp_register_sync_finish_proc(void (*proc)(unsigned int));

#endif