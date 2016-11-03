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
#ifndef _SUNXI_DRM_CRT_H_
#define _SUNXI_DRM_CRT_H_

struct sunxi_drm_crtc {
	struct drm_crtc			drm_crtc;
	struct drm_plane		*plane;
    struct disp_manager     *disp_mgr;
    disp_layer_config       *commit_layer;
	int			            pipe;
	unsigned int			dpms;
    struct list_head        pageflip_event_list;
	wait_queue_head_t		pending_flip_queue;
	atomic_t			    pending_flip;
    bool                    vsync_enable;
};
    
#define to_sunxi_crtc(x)	container_of(x, struct sunxi_drm_crtc,\
				drm_crtc)
int sunxi_drm_crtc_create(struct drm_device *dev, int nr);

int sunxi_drm_crtc_enable_vblank(struct drm_device *dev, int crt);

void sunxi_drm_crtc_disable_vblank(struct drm_device *dev, int crt);

#endif