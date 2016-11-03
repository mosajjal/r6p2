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
#ifndef _SUNXI_DRM_CORE_H_
#define _SUNXI_DRM_CORE_H_
enum drm_mode_status
sunxi_hdmi_support(struct disp_manager *disp_mgr, struct drm_display_mode *mode,
                    disp_tv_mode *disp_mode);

void convert_to_display_mode(struct drm_display_mode *mode,
			 disp_panel_para *panel_info);

int sunxi_drm_device_register(struct drm_device *dev);
int sunxi_drm_device_unregister(struct drm_device *dev);

#endif
