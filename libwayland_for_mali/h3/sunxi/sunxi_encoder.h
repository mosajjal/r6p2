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

#ifndef _SUNXI_DRM_ENCODER_H_
#define _SUNXI_DRM_ENCODER_H_

struct sunxi_drm_encoder {
	struct drm_encoder		drm_encoder;
	struct disp_manager     *disp_mgr;
	int				dpms;
	bool			updated;
};

#define to_sunxi_encoder(x)	container_of(x, struct sunxi_drm_encoder,\
				drm_encoder)

struct drm_encoder *sunxi_drm_encoder_create(struct drm_device *dev,
			   struct drm_crtc *drm_crtc);

#endif
