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
#ifndef _SUNXI_DRM_DRV_H_
#define _SUNXI_DRM_DRV_H_

#define MAX_CRTC	2

#define MAX_FB_BUFFER	3

struct sunxi_drm_private {
	struct drm_fb_helper *fb_helper;

	/*
	 * created crtc object would be contained at this array and
	 * this array is used to be aware of which crtc did it request vblank.
	 */
	struct drm_crtc *crtc[MAX_CRTC];
	struct drm_property *plane_zpos_property;
	struct drm_property *crtc_mode_property;
    void (*handle_vblank)(unsigned int crtc);
    /*if de has mmu will add the func*/
};
struct sunxi_drm_plan_info {
	struct device *dev;
	int pipe;
};

#endif