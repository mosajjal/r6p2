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
#ifndef _SUNXI_DRM_FB_H_
#define _SUNXI_DRM_FB_H_

#define to_sunxi_fb(x)	container_of(x, struct sunxi_drm_fb, fb)

/*
 * sunxi specific framebuffer structure.
 *
 * @fb: drm framebuffer obejct.
 * @buf_cnt: a buffer count to drm framebuffer.
 * @sunxi_gem_obj: array of sunxi specific gem object containing a gem object.
 */
struct sunxi_drm_fb {
	struct drm_framebuffer		fb;
	unsigned int			buf_cnt;
    unsigned int            pixel_format;
	struct sunxi_drm_gem_obj	*sunxi_gem_obj[MAX_FB_BUFFER];
};

void sunxi_drm_mode_config_init(struct drm_device *dev);

int sunxi_drm_fbdev_init(struct drm_device *dev);
#endif