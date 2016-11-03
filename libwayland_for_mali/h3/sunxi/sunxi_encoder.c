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
#include <drm/drm_crtc_helper.h>

#include <drm/sunxi_drm.h>
#include "sunxi_disp.h"
#include "sunxi_connector.h"
#include "sunxi_encoder.h"
#include "sunxi_crtc.h"

void sunxi_drm_encoder_reset(struct drm_encoder *encoder)
{
    /*TODO*/
}

void sunxi_drm_encoder_destroy(struct drm_encoder *encoder)
{
    struct sunxi_drm_encoder *sunxi_encoder =
		to_sunxi_encoder(encoder);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	sunxi_encoder->disp_mgr = NULL;

	drm_encoder_cleanup(encoder);
	kfree(sunxi_encoder);
}

static struct drm_encoder_funcs sunxi_encoder_funcs = {
	.destroy = sunxi_drm_encoder_destroy,
};

void sunxi_drm_encoder_dpms(struct drm_encoder *encoder, int mode)
{
    struct sunxi_drm_encoder *sunxi_encoder =
		to_sunxi_encoder(encoder);

	DRM_DEBUG_KMS("%s\n", __FILE__);
    sunxi_encoder->dpms = mode;
}

void sunxi_drm_encoder_save(struct drm_encoder *encoder)
{
    /*TODO*/
}

void sunxi_drm_encoder_restore(struct drm_encoder *encoder)
{
    /*TODO*/
}

bool sunxi_check_timing(const struct drm_display_mode *mode,
        disp_video_timings *timing_info)
{
    /*check the lcd timing info */
    if (mode->hdisplay == timing_info->x_res &&
        mode->vdisplay == timing_info->y_res) {
        return true;
    }

    return false;
}

bool sunxi_drm_encoder_mode_fixup(struct drm_encoder *encoder,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode)
{
    struct sunxi_drm_encoder *sunxi_encoder =
		to_sunxi_encoder(encoder);
    struct disp_device *disp_device = sunxi_encoder->disp_mgr->device;

	DRM_DEBUG_KMS("%s\n", __FILE__);

    if (disp_device != NULL) {
        if (disp_device->type == DISP_OUTPUT_TYPE_LCD) {
            /*need check the ht and vt?*/
            disp_video_timings timing_info;

            memset(&timing_info, 0, sizeof(disp_video_timings));
            disp_device->get_timings(disp_device, &timing_info);

            return sunxi_check_timing(mode, &timing_info);
        }

        if (disp_device->type == DISP_OUTPUT_TYPE_HDMI) {
            /*check for support mode and modify the mode to adjusted_mode*/
            return true;
        }
    }

    return false;
}

void sunxi_drm_encoder_prepare(struct drm_encoder *encoder)
{

	DRM_DEBUG_KMS("%s\n", __FILE__);
    return ;
}

void sunxi_drm_encoder_commit(struct drm_encoder *encoder)
{
    struct sunxi_drm_encoder *sunxi_encoder =
		to_sunxi_encoder(encoder);
    struct disp_device *disp_device = sunxi_encoder->disp_mgr->device;
    /*no need to check*/
	DRM_DEBUG_KMS("%s\n", __FILE__);

    if (disp_device != NULL) {
        if (disp_device->type == DISP_OUTPUT_TYPE_LCD) {
            /*need check the ht and vt?*/
            return ;
        }

        if (disp_device->type == DISP_OUTPUT_TYPE_HDMI) {
            /*check for support mode?*/
            return ;
        }
    } else {
        return ;
    }
}

void sunxi_drm_encoder_mode_set(struct drm_encoder *encoder,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode)
{

	DRM_DEBUG_KMS("%s\n", __FILE__);
    return ;
}

struct drm_crtc *sunxi_drm_encoder_get_crtc(struct drm_encoder *encoder)
{
    struct sunxi_drm_encoder *sunxi_encoder =
		to_sunxi_encoder(encoder);
    struct disp_manager    *disp_mgr = sunxi_encoder->disp_mgr;
    struct drm_device *dev = encoder->dev;
    struct drm_crtc *crtc = NULL;
    struct sunxi_drm_crtc *sunxi_crtc = NULL;

    if (disp_mgr == NULL)
        return NULL;
    list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
        sunxi_crtc = to_sunxi_crtc(crtc);

		if (sunxi_crtc->disp_mgr == disp_mgr) {
			return crtc;
		}
	}

    return NULL;
}

/* detect for DAC style encoders */
enum drm_connector_status
sunxi_drm_encoder_detect(struct drm_encoder *encoder,
					    struct drm_connector *connector)
{
    struct sunxi_drm_encoder *sunxi_encoder =
		to_sunxi_encoder(encoder);
    struct disp_manager    *disp_mgr = sunxi_encoder->disp_mgr;
    int status = -1;
	DRM_DEBUG_KMS("%s\n", __FILE__);

    if (disp_mgr == NULL &&
        disp_mgr->device == NULL) {
        return  connector_status_disconnected;  
    }

    if (disp_mgr->device->type == DISP_OUTPUT_TYPE_HDMI &&
        disp_mgr->device->detect != NULL) {
        status = disp_mgr->device->detect(disp_mgr->device);
        return status >= 1 ? connector_status_connected : connector_status_disconnected;
    }

    if (disp_mgr->device->type == DISP_OUTPUT_TYPE_LCD) {
        return connector_status_connected;
    }

    return connector_status_connected;
}

/* disable encoder when not in use - more explicit than dpms off */
void sunxi_drm_encoder_disable(struct drm_encoder *encoder)
{

    return;
}

static struct drm_encoder_helper_funcs sunxi_encoder_helper_funcs = {
	.dpms		= sunxi_drm_encoder_dpms,
	.mode_fixup	= sunxi_drm_encoder_mode_fixup,
	.mode_set	= sunxi_drm_encoder_mode_set,
	.prepare	= sunxi_drm_encoder_prepare,
	.commit		= sunxi_drm_encoder_commit,
	.disable	= sunxi_drm_encoder_disable,
	.detect     = NULL,
    .get_crtc   = NULL,
};

struct drm_encoder *sunxi_drm_encoder_create(struct drm_device *dev,
			   struct drm_crtc *drm_crtc)
{
	struct drm_encoder *encoder;
	struct sunxi_drm_encoder *sunxi_encoder;
    struct sunxi_drm_crtc   *sunxi_crt = NULL;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (!drm_crtc)
		return NULL;
    sunxi_crt = to_sunxi_crtc(drm_crtc);

	sunxi_encoder = kzalloc(sizeof(*sunxi_encoder), GFP_KERNEL);
	if (!sunxi_encoder) {
		DRM_ERROR("failed to allocate encoder\n");
		return NULL;
	}

	sunxi_encoder->dpms = DRM_MODE_DPMS_OFF;
	sunxi_encoder->disp_mgr = sunxi_crt->disp_mgr;
	encoder = &sunxi_encoder->drm_encoder;
	encoder->possible_crtcs = 1<<sunxi_crt->pipe;

	DRM_DEBUG_KMS("possible_crtcs = 0x%x\n", encoder->possible_crtcs);

	drm_encoder_init(dev, encoder, &sunxi_encoder_funcs,
			DRM_MODE_ENCODER_TMDS);

	drm_encoder_helper_add(encoder, &sunxi_encoder_helper_funcs);

	DRM_DEBUG_KMS("encoder has been created\n");

	return encoder;
}

