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
#ifndef _UAPI_SUNXI_DRM_H_
#define _UAPI_SUNXI_DRM_H_

#include <drm/drm.h>
struct drm_sunxi_gem_create {
	uint64_t size;
	unsigned int flags;
	unsigned int handle;
};
enum sunxi_drm_gem_buf_type {
	/* Physically Continuous memory and used as default. */
	SUNXI_BO_CONTIG	= 1 << 0,
	/* Physically Non-Continuous memory. */
	SUNXI_BO_NONCONTIG	= 0 << 0,
	/* non-cachable mapping. */
	SUNXI_BO_NONCACHABLE	= 0 << 1,
	/* cachable mapping. */
	SUNXI_BO_CACHABLE	= 1 << 1,
	/* write-combine mapping. */
	SUNXI_BO_WC		= 1 << 2,
	SUNXI_BO_MASK		= SUNXI_BO_CONTIG | SUNXI_BO_CACHABLE |
					SUNXI_BO_WC
};

#define DRM_SUNXI_GEM_CREATE		0x00
#define DRM_IOCTL_SUNXI_GEM_CREATE		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_SUNXI_GEM_CREATE, struct drm_exynos_gem_create)
#endif