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
#ifndef _SUNXI_DRM_CONNECTOR_H_
#define _SUNXI_DRM_CONNECTOR_H_

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include <drm/sunxi_drm.h>
#include "sunxi_drm_drv.h"
#include "sunxi_encoder.h"

#define to_sunxi_connector(x)	container_of(x, struct sunxi_drm_connector,\
				drm_connector)


struct sunxi_drm_connector {
	struct drm_connector	drm_connector;
    struct disp_manager     *disp_mgr;
    /*TODO*/
	uint32_t		encoder_id;
	struct sunxi_drm_plan_info *plane_info;
	int		dpms_now;
};

struct drm_connector *sunxi_drm_connector_create(struct drm_device *dev,
						   struct drm_encoder *encoder);

struct drm_encoder *sunxi_drm_best_encoder(struct drm_connector *connector);

void sunxi_drm_display_power(struct drm_connector *connector, int mode);

#endif
