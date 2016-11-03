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
#include "sunxi_drm_core.h"

enum drm_mode_status sunxi_lcd_support(struct drm_connector *connector,
                    struct drm_display_mode *mode)
{
     struct sunxi_drm_connector *sunxi_connector =
		to_sunxi_connector(connector);
    struct disp_manager *disp_mgr = sunxi_connector->disp_mgr;
    disp_panel_para panel_info;
    if (!disp_mgr || !disp_mgr->device ||
        disp_mgr->device->type != DISP_OUTPUT_TYPE_LCD) {
        return MODE_ERROR;
    }

    if (disp_mgr->device->get_panel_info != NULL) {
        memset(&panel_info, 0, sizeof(disp_panel_para));
        disp_mgr->device->get_panel_info(disp_mgr->device, &panel_info);
        if(panel_info.lcd_x == mode->hdisplay &&
            panel_info.lcd_y == mode->vdisplay) {
            goto mode_ok; 
        }
    }

    if (disp_mgr->device->get_timings) {
        /*backup*/
    }

    return MODE_VIRTUAL_X;
mode_ok:
    return MODE_OK;
}

struct drm_encoder *sunxi_drm_best_encoder(struct drm_connector *connector)
{
    struct drm_device *dev = connector->dev;
    struct sunxi_drm_connector *sunxi_connector =
		to_sunxi_connector(connector);
    struct disp_manager *disp_mgr = sunxi_connector->disp_mgr;
    struct sunxi_drm_encoder *sunxi_encoder = NULL;
    struct drm_encoder *encoder = NULL;
    if (disp_mgr == NULL) {
        return NULL;
    }

    list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		    sunxi_encoder = to_sunxi_encoder(encoder);
            if(sunxi_encoder->disp_mgr == disp_mgr)
                return encoder;
		}

    return NULL;

}

static int sunxi_hdmi_get_edid2mode(struct drm_connector *connector)
{

    int count;
    struct sunxi_drm_connector *sunxi_connector =
		to_sunxi_connector(connector);
    struct disp_manager *disp_mgr = sunxi_connector->disp_mgr;
    void *edid = NULL;

    if (disp_mgr->device->get_edid != NULL) {
        edid = (void *)disp_mgr->device->get_edid(disp_mgr->device);
    }

    if (edid == NULL) {
		DRM_ERROR("failed to get edid data.\n");
		kfree(edid);
		edid = NULL;
		return 0;
	}

	drm_mode_connector_update_edid_property(connector, edid);
	count = drm_add_edid_modes(connector, edid);

	connector->display_info.raw_edid = edid;
    return count;
}

static int sunxi_lcd_panel2mode(struct drm_connector *connector)
{

    struct drm_display_mode *mode = drm_mode_create(connector->dev);
	struct sunxi_drm_connector *sunxi_connector =
		to_sunxi_connector(connector);
    struct disp_manager *disp_mgr = sunxi_connector->disp_mgr;
    disp_video_timings disp_timings;
    disp_panel_para panel_info;

	if (disp_mgr->device != NULL) {
        if( disp_mgr->device->get_timings) {
            memset(&disp_timings, 0, sizeof(disp_video_timings));
		    disp_mgr->device->get_timings(disp_mgr->device, &disp_timings);
        }

        if (disp_mgr->device->get_panel_info) {
            memset(&panel_info, 0 , sizeof(disp_panel_para));
            disp_mgr->device->get_panel_info(disp_mgr->device, &panel_info);
        }

	} else {
		drm_mode_destroy(connector->dev, mode);
		return 0;
	}

	convert_to_display_mode(mode, &panel_info);
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);
    return 1;
}

static int sunxi_drm_connector_get_modes(struct drm_connector *connector)
{
    int count = 0;
    switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_HDMIA:
        count = sunxi_hdmi_get_edid2mode(connector);
        break;
    case DRM_MODE_CONNECTOR_VIRTUAL:
        break;
    default:
        count = sunxi_lcd_panel2mode(connector);
       /*is LCD */
    }
    return count;
}

static int sunxi_drm_connector_mode_valid(struct drm_connector *connector,
					    struct drm_display_mode *mode)
{
    disp_tv_mode disp_mode;
	struct sunxi_drm_connector *sunxi_connector =
		to_sunxi_connector(connector);
    struct disp_manager *disp_mgr = sunxi_connector->disp_mgr;
    switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_HDMIA:
        return sunxi_hdmi_support(disp_mgr, mode, &disp_mode);
    case DRM_MODE_CONNECTOR_LVDS:
        break;
    case DRM_MODE_CONNECTOR_eDP:
        break;
    case DRM_MODE_CONNECTOR_VIRTUAL:
        break;
    default:
        return sunxi_lcd_support(connector, mode);
           /*is LCD */
    }
   return MODE_ERROR;
}

static struct drm_connector_helper_funcs sunxi_connector_helper_funcs = {
	.get_modes	= sunxi_drm_connector_get_modes,
	.mode_valid	= sunxi_drm_connector_mode_valid,
	.best_encoder	= sunxi_drm_best_encoder,
};

void sunxi_drm_display_power(struct drm_connector *connector, int mode)
{
    struct sunxi_drm_connector *sunxi_connector =
		to_sunxi_connector(connector);
    struct disp_manager *disp_mgr = sunxi_connector->disp_mgr;

    if (mode == sunxi_connector->dpms_now)
        return;
    if (mode == DRM_MODE_DPMS_ON) {
        disp_mgr->enable(disp_mgr);
    } else {
        disp_mgr->disable(disp_mgr);
    }
    sunxi_connector->dpms_now = mode;
}

void sunxi_drm_connector_dpms(struct drm_connector *connector, int mode)
{
    if (mode < connector->dpms) {
        drm_helper_connector_dpms(connector, mode);
    }

    sunxi_drm_display_power(connector, mode);

    if (mode > connector->dpms) {
        drm_helper_connector_dpms(connector, mode);
    }
}

void sunxi_drm_connector_restore(struct drm_connector *connector)
{
    /*TODO*/
}

void sunxi_drm_connector_reset(struct drm_connector *connector)
{
    /*TODO*/
}

enum drm_connector_status
sunxi_drm_connector_detect(struct drm_connector *connector,
					    bool force)
{
    struct sunxi_drm_connector *sunxi_connector =
		to_sunxi_connector(connector);
    struct disp_manager *disp_mgr = sunxi_connector->disp_mgr;

    int status = -1;
    if (disp_mgr == NULL &&
        disp_mgr->device == NULL) {
        return connector_status_disconnected;  
    }

    if ( disp_mgr->device->type == DISP_OUTPUT_TYPE_HDMI &&
            disp_mgr->device->detect != NULL) {
        status = disp_mgr->device->detect(disp_mgr->device);
        return status >= 1 ?
            connector_status_connected : connector_status_disconnected;
    }

    if (disp_mgr->device->type == DISP_OUTPUT_TYPE_LCD) {
        return connector_status_connected;
    }

    return connector_status_disconnected;
}

int sunxi_drm_connector_fill_modes(struct drm_connector *connector,
        uint32_t max_width, uint32_t max_height)
{

    return drm_helper_probe_single_connector_modes(connector, max_width,
							max_height);
}

int sunxi_drm_connector_set_property(struct drm_connector *connector,
            struct drm_property *property,uint64_t val)
{
    return 0;
    /*TODO*/
}

void sunxi_drm_connector_destroy(struct drm_connector *connector)
{
    struct sunxi_drm_connector *sunxi_connector =
		to_sunxi_connector(connector);

    drm_sysfs_connector_remove(connector);
    drm_connector_cleanup(connector);
    kfree(sunxi_connector);
}

void sunxi_drm_connector_force(struct drm_connector *connector)
{
    /*TODO*/
}

void sunxi_drm_connector_save(struct drm_connector *connector)
{
    /*TODO*/
}

static struct drm_connector_funcs sunxi_connector_funcs = {
	.dpms		= sunxi_drm_connector_dpms,
	.fill_modes	= sunxi_drm_connector_fill_modes,
	.detect		= sunxi_drm_connector_detect,
	.destroy	= sunxi_drm_connector_destroy,
};

struct disp_manager *sunxi_drm_get_manager(struct drm_encoder *encoder)
{
    struct sunxi_drm_encoder *sunxi_encoder = to_sunxi_encoder(encoder);
    return sunxi_encoder->disp_mgr;
}

struct drm_connector *sunxi_drm_connector_create(struct drm_device *dev,
						   struct drm_encoder *encoder)
{
	struct sunxi_drm_connector *sunxi_connector;
	struct disp_manager *manager = sunxi_drm_get_manager(encoder);
	struct drm_connector *connector;
	int type;
	int err;

	DRM_DEBUG_KMS("%s\n", __FILE__);
    if (!manager || !manager->device) {
        DRM_ERROR("there is nothing device for connetcor, check the disp_init.\n");
        return NULL;
    }
	sunxi_connector = kzalloc(sizeof(*sunxi_connector), GFP_KERNEL);
	if (!sunxi_connector) {
		DRM_ERROR("failed to allocate connector\n");
		return NULL;
	}

	connector = &sunxi_connector->drm_connector;

	switch (manager->device->type) {
	case DISP_OUTPUT_TYPE_HDMI:
		type = DRM_MODE_CONNECTOR_HDMIA;
        /*tmp not support hotplug*/
		//connector->interlace_allowed = true;
		connector->polled = DRM_CONNECTOR_POLL_HPD | DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT;
		break;
	default:
		type = DRM_MODE_CONNECTOR_Unknown;
		break;
	}

	drm_connector_init(dev, connector, &sunxi_connector_funcs, type);
	drm_connector_helper_add(connector, &sunxi_connector_helper_funcs);

	err = drm_sysfs_connector_add(connector);
	if (err)
		goto err_connector;

	sunxi_connector->encoder_id = encoder->base.id;
	sunxi_connector->disp_mgr = manager;
	connector->encoder = encoder;
    sunxi_connector->dpms_now = DRM_MODE_DPMS_ON;
	err = drm_mode_connector_attach_encoder(connector, encoder);
	if (err) {
		DRM_ERROR("failed to attach a connector to a encoder\n");
		goto err_sysfs;
	}

	DRM_DEBUG_KMS("connector has been created\n");

	return connector;

err_sysfs:
	drm_sysfs_connector_remove(connector);
err_connector:
	drm_connector_cleanup(connector);
	kfree(sunxi_connector);
	return NULL;
}

