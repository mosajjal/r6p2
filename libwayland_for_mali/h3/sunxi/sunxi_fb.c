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
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/sunxi_drm.h>

#include "sunxi_drm_drv.h"
#include "sunxi_fb.h"
#include "sunxi_gem.h"
#include "sunxi_encoder.h"
#include "sunxi_disp.h"

unsigned int
sunxi_drm_format_xchang(unsigned int drm_format)
{
   uint32_t format = drm_format & ~DRM_FORMAT_BIG_ENDIAN;

	switch (format) {
	case DRM_FORMAT_ARGB4444:
        return DISP_FORMAT_ARGB_4444;

	case DRM_FORMAT_ABGR4444:
        return DISP_FORMAT_ABGR_4444;

	case DRM_FORMAT_RGBA4444:
        return DISP_FORMAT_RGBA_4444;

	case DRM_FORMAT_BGRA4444:
        return DISP_FORMAT_BGRA_4444;

	case DRM_FORMAT_ARGB1555:
        return DISP_FORMAT_ARGB_1555;

	case DRM_FORMAT_ABGR1555:
        return DISP_FORMAT_ABGR_1555;

	case DRM_FORMAT_RGBA5551:
        return DISP_FORMAT_RGBA_5551;

	case DRM_FORMAT_BGRA5551:
        return DISP_FORMAT_BGRA_5551;

	case DRM_FORMAT_RGB565:
        return DISP_FORMAT_RGB_565;

	case DRM_FORMAT_BGR565:
        return DISP_FORMAT_BGR_565;

	case DRM_FORMAT_RGB888:
        return DISP_FORMAT_RGB_888;

	case DRM_FORMAT_BGR888:
        return DISP_FORMAT_BGR_888;

	case DRM_FORMAT_XRGB8888:
        return DISP_FORMAT_XRGB_8888;

	case DRM_FORMAT_XBGR8888:
        return DISP_FORMAT_XBGR_8888;

	case DRM_FORMAT_RGBX8888:
        return DISP_FORMAT_RGBX_8888;

	case DRM_FORMAT_BGRX8888:
        return DISP_FORMAT_BGRX_8888;

	case DRM_FORMAT_ARGB8888:
        return DISP_FORMAT_ARGB_8888;

    case DRM_FORMAT_ABGR8888:
        return DISP_FORMAT_ABGR_8888;

	case DRM_FORMAT_RGBA8888:
        return DISP_FORMAT_RGBA_8888;

	case DRM_FORMAT_BGRA8888:
        return DISP_FORMAT_BGRA_8888;
	
	case DRM_FORMAT_NV12:
        return DISP_FORMAT_YUV420_SP_UVUV;
	
	case DRM_FORMAT_YUV420:
        return DISP_FORMAT_YUV420_P;
	
	case DRM_FORMAT_YUV422:
        return DISP_FORMAT_YUV422_P;

	case DRM_FORMAT_YUV444:
        return DISP_FORMAT_YUV444_P;

    case DRM_FORMAT_C8:
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_BGR233:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_AYUV:
    case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YVU411:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YVU444:
		return -EINVAL;
	default:
		return -EINVAL;
	}
    return -EINVAL;
}


static void sunxi_drm_fb_destroy(struct drm_framebuffer *fb)
{
	struct sunxi_drm_fb *sunxi_fb = to_sunxi_fb(fb);
    int i;
	DRM_DEBUG_KMS("%s\n", __FILE__);
    /*check wait for de display the buffer? */
	drm_framebuffer_cleanup(fb);
    for (i = 0; i < MAX_FB_BUFFER; i++) {
		struct drm_gem_object *obj;

		if (sunxi_fb->sunxi_gem_obj[i] == NULL)
			continue;

		obj = &sunxi_fb->sunxi_gem_obj[i]->base;
		drm_gem_object_unreference_unlocked(obj);
	}
	kfree(sunxi_fb);
	sunxi_fb = NULL;
}

static int sunxi_drm_fb_create_handle(struct drm_framebuffer *fb,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	struct sunxi_drm_fb *sunxi_fb = to_sunxi_fb(fb);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	return drm_gem_handle_create(file_priv,
			&sunxi_fb->sunxi_gem_obj[0]->base, handle);
}

static int sunxi_drm_fb_dirty(struct drm_framebuffer *fb,
				struct drm_file *file_priv, unsigned flags,
				unsigned color, struct drm_clip_rect *clips,
				unsigned num_clips)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	return 0;
}

static struct drm_framebuffer_funcs sunxi_drm_fb_funcs = {
	.destroy	= sunxi_drm_fb_destroy,
	.create_handle	= sunxi_drm_fb_create_handle,
	.dirty		= sunxi_drm_fb_dirty,
};

static int check_fb_gem_memory_type(struct drm_device *drm_dev,
				struct sunxi_drm_gem_obj *sunxi_gem_obj)
{
	unsigned int flags;

    if (!sunxi_gem_obj->buffer)
        return -1;
	flags = sunxi_gem_obj->buffer->flags;

	/*
	 * without iommu support, not support physically non-continuous memory
	 * for framebuffer.
	 */
	if (IS_NONCONTIG_BUFFER(flags)) {
		DRM_ERROR("cannot use this gem memory type for fb.\n");
		return -EINVAL;
	}

	return 0;
}

static struct drm_framebuffer *
sunxi_gem_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		      struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_gem_object *obj;
	struct sunxi_drm_gem_obj *sunxi_gem_obj;
	struct sunxi_drm_fb *sunxi_fb;
	int i, ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	sunxi_fb = kzalloc(sizeof(*sunxi_fb), GFP_KERNEL);
	if (!sunxi_fb) {
		DRM_ERROR("failed to allocate sunxi drm framebuffer\n");
		return ERR_PTR(-ENOMEM);
	}

	obj = drm_gem_object_lookup(dev, file_priv, mode_cmd->handles[0]);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object\n");
		ret = -ENOENT;
		goto err_free;
	}

	drm_helper_mode_fill_fb_struct(&sunxi_fb->fb, mode_cmd);
	sunxi_fb->sunxi_gem_obj[0] = to_sunxi_gem_obj(obj);
	sunxi_fb->buf_cnt = drm_format_num_planes(mode_cmd->pixel_format);
    sunxi_fb->pixel_format = (unsigned int)sunxi_drm_format_xchang(mode_cmd->pixel_format);
    if ( sunxi_fb->pixel_format == -EINVAL) {
        DRM_ERROR("failed to change the fornat 0x%08x\n", mode_cmd->pixel_format);
    }

	DRM_DEBUG_KMS("buf_cnt = %d  format = 0x%08x\n", sunxi_fb->buf_cnt, mode_cmd->pixel_format);
printk("fb%p count:%d   \n",sunxi_fb,sunxi_fb->buf_cnt);
	for (i = 1; i < sunxi_fb->buf_cnt; i++) {
        if (mode_cmd->handles[i] == 0) {
            break;
        }
		obj = drm_gem_object_lookup(dev, file_priv,
				mode_cmd->handles[i]);
		if (!obj) {
			DRM_ERROR("failed to lookup gem object\n");
			ret = -ENOENT;
			sunxi_fb->buf_cnt = i;
			goto err_unreference;
		}

		sunxi_gem_obj = to_sunxi_gem_obj(obj);
		sunxi_fb->sunxi_gem_obj[i] = sunxi_gem_obj;

		ret = check_fb_gem_memory_type(dev, sunxi_gem_obj);
		if (ret < 0) {
			DRM_ERROR("cannot use this gem memory type for fb.\n");
			goto err_unreference;
		}
	}

	ret = drm_framebuffer_init(dev, &sunxi_fb->fb, &sunxi_drm_fb_funcs);
	if (ret) {
		DRM_ERROR("failed to init framebuffer.\n");
		goto err_unreference;
	}

	return &sunxi_fb->fb;

err_unreference:
	for (i = 0; i < sunxi_fb->buf_cnt; i++) {
		struct drm_gem_object *obj;

		obj = &sunxi_fb->sunxi_gem_obj[i]->base;
		if (obj)
			drm_gem_object_unreference_unlocked(obj);
	}

err_free:
	kfree(sunxi_fb);
	return ERR_PTR(ret);
}
  
static struct drm_mode_config_funcs sunxi_drm_mode_config_funcs = {
	.fb_create = sunxi_gem_fb_create,
	//.output_poll_changed = sunxi_drm_output_poll_changed,  ToDo
};

void sunxi_drm_mode_config_init(struct drm_device *dev)
{
	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	dev->mode_config.max_width = 2000;
	dev->mode_config.max_height = 1500;

	dev->mode_config.funcs = &sunxi_drm_mode_config_funcs;
}

int sunxi_drm_fbdev_init(struct drm_device *dev)
{
    return 0;
}
