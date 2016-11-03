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

enum drm_mode_status
sunxi_hdmi_support(struct disp_manager *disp_mgr, struct drm_display_mode *mode,
                    disp_tv_mode *disp_mode)
{

    if (!disp_mgr->device || disp_mgr->device->type != DISP_OUTPUT_TYPE_HDMI) {
        return MODE_ERROR;
    }
    DRM_DEBUG_KMS("####### %d:\"%s\" %d\n",
		mode->base.id, mode->name, mode->vrefresh);
    if(mode->vrefresh <= 0)
    {
        mode->vrefresh = drm_mode_vrefresh(mode);
    }
    if (mode->vrefresh > 65 || mode->vrefresh < 20) {
        DRM_DEBUG_KMS("%s  bad vfresh[%d]\n", __FILE__, mode->vrefresh);
        return MODE_VSYNC;
    }

    if ((mode->vrefresh == 59) || (mode->vrefresh == 60)) {
		if ((mode->hdisplay == 720) && (mode->vdisplay == 240)) {
			*disp_mode = DISP_TV_MOD_480I;
            //goto mode_ok;
		}
		if ((mode->hdisplay == 720) && (mode->vdisplay == 480)) {
            *disp_mode = DISP_TV_MOD_480P;
            //goto mode_ok;
		}
		if ((mode->hdisplay == 1280) && (mode->vdisplay == 720)) {
            *disp_mode = DISP_TV_MOD_720P_60HZ;
            goto mode_ok;
		}
		if ((mode->hdisplay == 1920) && (mode->vdisplay == 540)) {
            *disp_mode = DISP_TV_MOD_1080I_60HZ;
            //goto mode_ok;
        }
		if ((mode->hdisplay == 1920) && (mode->vdisplay == 1080)) {
            *disp_mode = DISP_TV_MOD_1080P_60HZ;
            goto mode_ok;
		}
	}else if ((mode->vrefresh == 49) || (mode->vrefresh == 50)) {
		if ((mode->hdisplay == 720) && (mode->vdisplay == 288)) {
            *disp_mode = DISP_TV_MOD_576I;
            //goto mode_ok;
		}
		if ((mode->hdisplay == 720) && (mode->vdisplay == 576)) {
            *disp_mode = DISP_TV_MOD_576P;
           // goto mode_ok;
		}
		if ((mode->hdisplay == 1280) && (mode->vdisplay == 720)) {
            *disp_mode = DISP_TV_MOD_720P_50HZ;
           // goto mode_ok;
		}
		if ((mode->hdisplay == 1920) && (mode->vdisplay == 540)) {
            *disp_mode = DISP_TV_MOD_1080I_50HZ;
           // goto mode_ok;
		}
		if ((mode->hdisplay == 1920) && (mode->vdisplay == 1080)) {
            *disp_mode = DISP_TV_MOD_1080P_50HZ;
            //goto mode_ok;
		}
	}

    return MODE_ERROR;

mode_ok:
    return MODE_OK;
}

void convert_to_display_mode(struct drm_display_mode *mode,
			 disp_panel_para *panel_info)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	mode->clock = panel_info->lcd_dclk_freq * 1000;;
	mode->vrefresh = 60;

	mode->hdisplay =  panel_info->lcd_x;
	mode->hsync_start = panel_info->lcd_ht - panel_info->lcd_hbp;
	mode->hsync_end = panel_info->lcd_ht - panel_info->lcd_hbp + panel_info->lcd_hspw;
	mode->htotal = panel_info->lcd_ht;

	mode->vdisplay = panel_info->lcd_y;
	mode->vsync_start = panel_info->lcd_vt - panel_info->lcd_vbp;
	mode->vsync_end = panel_info->lcd_vt - panel_info->lcd_vbp + panel_info->lcd_vspw;
	mode->vtotal = panel_info->lcd_vt;
	mode->width_mm = 260;
	mode->height_mm = 360;
}

int sunxi_drm_device_register(struct drm_device *dev)
{
	struct sunxi_drm_private *private;
    struct sunxi_drm_crtc   *sunxi_crt = NULL;
    struct disp_manager     *disp_mgr = NULL;
    struct drm_encoder *encoder = NULL;
    struct drm_connector *connector = NULL;

	int nr;
    private = (struct sunxi_drm_private *)dev->dev_private;
	DRM_DEBUG_DRIVER("%s\n", __FILE__);

	if (!dev)
		return -EINVAL;

    for (nr = 0; nr < MAX_CRTC; nr++) {
        sunxi_crt = to_sunxi_crtc(private->crtc[nr]);
        disp_mgr = sunxi_crt->disp_mgr;

		if (disp_mgr == NULL || disp_mgr->device == NULL) {
            DRM_ERROR("the linux3.4 drm driver must wait for disp init,check the disp_init info.\n");
			continue;
		}

		encoder = sunxi_drm_encoder_create(dev, &sunxi_crt->drm_crtc);
		if (NULL == encoder) {
			DRM_DEBUG("failed to create encoder.\n");
			continue;
		}

        connector = sunxi_drm_connector_create(dev, encoder);
        if (NULL == connector) {
			DRM_DEBUG("failed to create connector.\n");
			continue;
        }
	}

    if (private->handle_vblank) {
        disp_register_sync_finish_proc(private->handle_vblank);
    }
	return 0;
}

int sunxi_drm_device_unregister(struct drm_device *dev)
{

    struct sunxi_drm_private *private = (struct sunxi_drm_private *)dev->dev_private;
	DRM_DEBUG_DRIVER("%s\n", __FILE__);

	if (!dev) {
		WARN(1, "Unexpected drm device unregister!\n");
		return -EINVAL;
	}

    disp_unregister_sync_finish_proc(private->handle_vblank);
	return 0;
}
