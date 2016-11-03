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
#include <video/sunxi_display2.h>

#include <drm/sunxi_drm.h>
#include "sunxi_disp.h"
#include "sunxi_connector.h"
#include "sunxi_encoder.h"
#include "sunxi_crtc.h"
#include "sunxi_fb.h"
#include "sunxi_drm_core.h"
#include "sunxi_gem.h"
#include <asm-generic/dma-mapping-common.h>
static struct sunxi_drm_crtc *sunxi_crt[MAX_CRTC];

static void sunxi_drm_crtc_finish_pageflip(struct drm_device *dev, int crtc);
	
bool sunxi_queue_display(struct drm_crtc *crtc, int x, int y,
		     struct drm_framebuffer *old_fb)
{
    struct sunxi_drm_crtc *sunxi_crtc = to_sunxi_crtc(crtc);
    struct sunxi_drm_fb *sunxi_fb = to_sunxi_fb(crtc->fb);
    struct disp_manager *disp_mgr = sunxi_crtc->disp_mgr;
    disp_layer_config  *commit_layer = sunxi_crtc->commit_layer;
    struct sunxi_drm_gem_buf	*buffer;
    int fb_crop_w = 0, fb_crop_h = 0, i = 3;

    if (!disp_mgr || !disp_mgr->device ||
        !disp_mgr->set_layer_config || !sunxi_crtc->commit_layer) {
        return false;
    }

    if (NULL == sunxi_fb) {
        sunxi_fb = to_sunxi_fb(old_fb);
    }

    if (NULL != sunxi_fb) {

        commit_layer->channel = 0;
        commit_layer->enable = 1;
        commit_layer->layer_id = 0;
        commit_layer->info.zorder = 0;
        commit_layer->info.screen_win.x = 0;
        commit_layer->info.screen_win.y = 0;
        commit_layer->info.screen_win.width = crtc->mode.hdisplay;
        commit_layer->info.screen_win.height = crtc->mode.vdisplay;
        while (i--) {
            if (NULL != sunxi_fb->sunxi_gem_obj[i] &&
                NULL != sunxi_fb->sunxi_gem_obj[i]->buffer) {
                buffer = sunxi_fb->sunxi_gem_obj[i]->buffer;
                commit_layer->info.fb.addr[i] =
                    (unsigned long long)buffer->dma_addr;
                if (buffer->flags & SUNXI_BO_CONTIG == 0) {
                    DRM_ERROR("DE just support contig memory.\n");
                    return false;
                }
printk( KERN_WARNING "fb:%p    buffer:%p %p      i:%d\n\n  ",sunxi_fb,buffer->kvaddr,sg_page(buffer->sgt->sgl),i);
                sunxi_sync_buf(buffer);
            }
        }
        commit_layer->info.fb.format = sunxi_fb->pixel_format;
        commit_layer->info.fb.size[0].width = sunxi_fb->fb.pitches[0]/4;
        commit_layer->info.fb.size[1].width = sunxi_fb->fb.pitches[1]/4;
        commit_layer->info.fb.size[2].width = sunxi_fb->fb.pitches[2]/4;
        commit_layer->info.fb.size[0].height = sunxi_fb->fb.height;
        commit_layer->info.fb.size[1].height = sunxi_fb->fb.height;
        commit_layer->info.fb.size[2].height = sunxi_fb->fb.height;
        fb_crop_w = sunxi_fb->fb.width - x;
        fb_crop_h = sunxi_fb->fb.height - y;
        commit_layer->info.fb.crop.x = ((long long)(x))<<32;
        commit_layer->info.fb.crop.y = ((long long)(y))<<32;
        commit_layer->info.fb.crop.height = ((long long)(fb_crop_h))<<32;
        commit_layer->info.fb.crop.width = ((long long)(fb_crop_w))<<32;
        disp_mgr->set_layer_config(disp_mgr, commit_layer, 1);
    }
    return true;
    
}

static inline struct sunxi_drm_crtc *sunxi_get_crt(unsigned int crt)
{
    if (crt > MAX_CRTC || sunxi_crt[crt] == NULL)
        return NULL;
    return sunxi_crt[crt];
}

void sunxi_drm_blank_handle(unsigned int crt)
{
    struct sunxi_drm_crtc *soc_crt = sunxi_get_crt(crt);
    if (!soc_crt)
	return;
    if (!soc_crt->vsync_enable)
        return;
    drm_handle_vblank(soc_crt->drm_crtc.dev, crt);
    sunxi_drm_crtc_finish_pageflip(soc_crt->drm_crtc.dev, crt);
}

void sunxi_drm_crtc_dpms(struct drm_crtc *crtc, int mode)
{
    struct sunxi_drm_crtc *sunxi_crtc = to_sunxi_crtc(crtc);

	DRM_DEBUG_KMS("crtc[%d] mode[%d]\n", crtc->base.id, mode);

	if (sunxi_crtc->dpms == mode) {
		DRM_DEBUG_KMS("desired dpms mode is same as previous one.\n");
		return;
	}

	if (mode > DRM_MODE_DPMS_ON) {
		/* wait for the completion of page flip. */
		wait_event(sunxi_crtc->pending_flip_queue,
				atomic_read(&sunxi_crtc->pending_flip) == 0);
		drm_vblank_off(crtc->dev, sunxi_crtc->pipe);
	}
    /*will check display driver*/
	sunxi_crtc->dpms = mode;
}

void sunxi_drm_crtc_prepare(struct drm_crtc *crtc)
{
    DRM_DEBUG_KMS("%s\n", __FILE__);
    return ;
}

void sunxi_drm_crtc_commit(struct drm_crtc *crtc)
{
    DRM_DEBUG_KMS("%s\n", __FILE__);
    sunxi_drm_crtc_dpms(crtc, DRM_MODE_DPMS_ON);
    return ;
}

/* Provider can fixup or change mode timings before modeset occurs */
bool sunxi_drm_crtc_mode_fixup(struct drm_crtc *crtc,
		   struct drm_display_mode *mode,
		   struct drm_display_mode *adjusted_mode)
{
    DRM_DEBUG_KMS("%s\n", __FILE__);

	/* check display driver ToDO, leave it to encoder*/
	return true;
}

/* Actually set the mode */
int sunxi_drm_crtc_mode_set(struct drm_crtc *crtc, struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode, int x, int y,
		struct drm_framebuffer *old_fb)
{
    disp_tv_mode disp_mode = DISP_TV_MODE_NUM;
    int disp_mode_cmp = -1;
    enum drm_mode_status mode_ret;
    bool    update = 0;
    struct sunxi_drm_crtc *sunxi_crtc = to_sunxi_crtc(crtc);

    DRM_DEBUG_KMS("%s\n", __FILE__);

    if (!sunxi_crtc->disp_mgr || !sunxi_crtc->disp_mgr->device) {
        return -ENXIO;
    }

    if (sunxi_crtc->disp_mgr->device->type == DISP_OUTPUT_TYPE_HDMI) {
        mode_ret = sunxi_hdmi_support(sunxi_crtc->disp_mgr,
                    adjusted_mode, &disp_mode);
        if (mode_ret == MODE_OK) {
            if (NULL != sunxi_crtc->disp_mgr->device->get_mode) {
                disp_mode_cmp =
                 sunxi_crtc->disp_mgr->device->get_mode(sunxi_crtc->disp_mgr->device);
                if (((int)disp_mode) != disp_mode_cmp) {
                    update = 1;
                }
            }

            if (update && 0 != bsp_disp_device_switch(sunxi_crtc->pipe,
                            DISP_OUTPUT_TYPE_HDMI, disp_mode)) {
                return -ENXIO;
            }

        }
    }

    if (update) {
        sunxi_queue_display(crtc, x, y, NULL);
    } else {
        sunxi_queue_display(crtc, x, y, old_fb);
    }
    return 0;
}

/* Move the crtc on the current fb to the given position *optional* */
int sunxi_drm_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
		     struct drm_framebuffer *old_fb)
{
    struct sunxi_drm_crtc *sunxi_crtc = to_sunxi_crtc(crtc);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* when framebuffer changing is requested,
	 *   crtc's dpms should be on */
	if (sunxi_crtc->dpms > DRM_MODE_DPMS_ON) {
		DRM_ERROR("failed framebuffer changing request.\n");
		return -EPERM;
	}
    if(!sunxi_queue_display(crtc, x, y, old_fb)){
        DRM_ERROR("failed queue dispaly  request.\n");
		return -EPERM;
    }
    /* sunxi for de set the glb */
    return 0; 
}

int sunxi_drm_crtc_mode_set_base_atomic(struct drm_crtc *crtc,
			    struct drm_framebuffer *fb, int x, int y,
			    enum mode_set_atomic set)
{
    return 0;
    /*TODO*/
}

/* reload the current crtc LUT */
void sunxi_drm_crtc_load_lut(struct drm_crtc *crtc)
{
    return;
    /*TODO*/
}

/* disable crtc when not in use - more explicit than dpms off */
void sunxi_drm_crtc_disable(struct drm_crtc *crtc)
{
    return ;
    /*TODO*/
}

static struct drm_crtc_helper_funcs sunxi_crtc_helper_funcs = {
	.dpms		= sunxi_drm_crtc_dpms,
	.prepare	= sunxi_drm_crtc_prepare,
	.commit		= sunxi_drm_crtc_commit,
	.mode_fixup	= sunxi_drm_crtc_mode_fixup,
	.mode_set	= sunxi_drm_crtc_mode_set,
	.mode_set_base	= sunxi_drm_crtc_mode_set_base,
	.load_lut	= sunxi_drm_crtc_load_lut,
	.disable	= sunxi_drm_crtc_disable,
};

/* Save CRTC state */
void sunxi_drm_crtc_save(struct drm_crtc *crtc)
{
     /*TODO*/
}

/* Restore CRTC state */
void sunxi_drm_crtc_restore(struct drm_crtc *crtc)
{
     /*TODO*/
}

/* Reset CRTC state */
void sunxi_drm_crtc_reset(struct drm_crtc *crtc)
{
    /*TODO*/
}

/* cursor controls */
int sunxi_drm_crtc_cursor_set(struct drm_crtc *crtc, struct drm_file *file_priv,
			  uint32_t handle, uint32_t width, uint32_t height)
{
    return 0;
    /*TODO*/
}

int sunxi_drm_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
    return 0;
    /*TODO*/
}

/* Set gamma on the CRTC */
void sunxi_drm_crtc_gamma_set(struct drm_crtc *crtc, u16 *r,
                    u16 *g, u16 *b, uint32_t start, uint32_t size)
{
    /*TODO*/
}

/* Object destroy routine */
void sunxi_drm_crtc_destroy(struct drm_crtc *crtc)
{
    struct sunxi_drm_crtc *sunxi_crtc = to_sunxi_crtc(crtc);
	struct sunxi_drm_private *private = crtc->dev->dev_private;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	private->crtc[sunxi_crtc->pipe] = NULL;
    sunxi_crt[sunxi_crtc->pipe] = NULL;
	drm_crtc_cleanup(crtc);
	kfree(sunxi_crtc);
}

int sunxi_drm_crtc_set_config(struct drm_mode_set *set)
{
    DRM_DEBUG_KMS("%s\n", __FILE__);
    return drm_crtc_helper_set_config(set);
}

/*
	 * Flip to the given framebuffer.  This implements the page
	 * flip ioctl described in drm_mode.h, specifically, the
	 * implementation must return immediately and block all
	 * rendering to the current fb until the flip has completed.
	 * If userspace set the event flag in the ioctl, the event
	 * argument will point to an event to send back when the flip
	 * completes, otherwise it will be NULL.
*/
int sunxi_drm_crtc_page_flip(struct drm_crtc *crtc,
			 struct drm_framebuffer *fb,
			 struct drm_pending_vblank_event *event)
{

    struct drm_device *dev = crtc->dev;
	struct sunxi_drm_crtc *sunxi_crtc = to_sunxi_crtc(crtc);
	struct drm_framebuffer *old_fb = crtc->fb;
	int ret = -EINVAL;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* when the page flip is requested, crtc's dpms should be on */
	if (sunxi_crtc->dpms > DRM_MODE_DPMS_ON) {
		DRM_ERROR("failed page flip request.\n");
		return -EINVAL;
	}

	mutex_lock(&dev->struct_mutex);

	if (event) {
		/*
		 * the pipe from user always is 0 so we can set pipe number
		 * of current owner to event.
		 */
		event->pipe = sunxi_crtc->pipe;

		ret = drm_vblank_get(dev, sunxi_crtc->pipe);
		if (ret) {
			DRM_DEBUG("failed to acquire vblank counter\n");
			goto out;
		}

		spin_lock_irq(&dev->event_lock);
		list_add_tail(&event->base.link,
				&sunxi_crtc->pageflip_event_list);
		atomic_add_return(1, &sunxi_crtc->pending_flip);
		spin_unlock_irq(&dev->event_lock);

		crtc->fb = fb;
		ret = sunxi_drm_crtc_mode_set_base(crtc, crtc->x, crtc->y,
						    NULL);
		if (ret) {
			crtc->fb = old_fb;

			spin_lock_irq(&dev->event_lock);
			drm_vblank_put(dev, sunxi_crtc->pipe);
			list_del(&event->base.link);
            atomic_sub_return(1, &sunxi_crtc->pending_flip);
			spin_unlock_irq(&dev->event_lock);

			goto out;
		}
	} else {
	    crtc->fb = fb;
		ret = sunxi_drm_crtc_mode_set_base(crtc, crtc->x, crtc->y,
						    NULL);
        if (ret)
            crtc->fb = old_fb;
	}
out:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

void sunxi_drm_crtc_finish_pageflip(struct drm_device *dev, int crtc)
{
	struct sunxi_drm_private *dev_priv = dev->dev_private;
	struct drm_pending_vblank_event *e, *t;
	struct drm_crtc *drm_crtc = dev_priv->crtc[crtc];
	struct sunxi_drm_crtc *sunxi_crtc = to_sunxi_crtc(drm_crtc);
	unsigned long flags;
    struct timeval now;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	spin_lock_irqsave(&dev->event_lock, flags);

	list_for_each_entry_safe(e, t, &sunxi_crtc->pageflip_event_list,
			base.link) {

		list_del(&e->base.link);
		do_gettimeofday(&now);
		e->event.sequence = 0;
		e->event.tv_sec = now.tv_sec;
		e->event.tv_usec = now.tv_usec;

		list_move_tail(&e->base.link, &e->base.file_priv->event_list);
		wake_up_interruptible(&e->base.file_priv->event_wait);
		drm_vblank_put(dev, crtc);
		atomic_sub_return(1, &sunxi_crtc->pending_flip);
		wake_up(&sunxi_crtc->pending_flip_queue);
	}

	spin_unlock_irqrestore(&dev->event_lock, flags);
}

int sunxi_drm_crtc_set_property(struct drm_crtc *crtc,
			    struct drm_property *property, uint64_t val)
{
    /*ToDo*/
    return 0;
}

static struct drm_crtc_funcs sunxi_crtc_funcs = {
	.set_config	= sunxi_drm_crtc_set_config,
	.page_flip	= sunxi_drm_crtc_page_flip,
	.destroy	= sunxi_drm_crtc_destroy,
};

int sunxi_drm_crtc_enable_vblank(struct drm_device *dev, int crt)
{
    struct sunxi_drm_crtc *soc_crt = sunxi_get_crt(crt);
    soc_crt->vsync_enable = 1;
    return 0; 
}

void sunxi_drm_crtc_disable_vblank(struct drm_device *dev, int crt)
{
    struct sunxi_drm_crtc *soc_crt = sunxi_get_crt(crt);
    soc_crt->vsync_enable = 0;
    return ; 
}

int sunxi_drm_crtc_create(struct drm_device *dev, int nr)
{
	struct sunxi_drm_crtc *sunxi_crtc;
	struct sunxi_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc;
    int lyr = 0, ch = 0;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	sunxi_crtc = kzalloc(sizeof(*sunxi_crtc), GFP_KERNEL);
	if (!sunxi_crtc) {
		DRM_ERROR("failed to allocate sunxi crtc\n");
		return -ENOMEM;
	}

    crtc = &sunxi_crtc->drm_crtc;
	sunxi_crtc->pipe = nr;
	sunxi_crtc->dpms = DRM_MODE_DPMS_OFF;
    sunxi_crtc->vsync_enable = 1;
    sunxi_crtc->commit_layer = kzalloc(sizeof(disp_layer_config),GFP_KERNEL);

    if (sunxi_crtc->commit_layer == NULL) {
        DRM_ERROR("failed to allocate sunxi commit_layer\n");
		return -ENOMEM;
    }

	INIT_LIST_HEAD(&sunxi_crtc->pageflip_event_list);
	init_waitqueue_head(&sunxi_crtc->pending_flip_queue);
	atomic_set(&sunxi_crtc->pending_flip, 0);

    sunxi_crtc->disp_mgr = disp_get_layer_manager(nr);
    if (NULL == sunxi_crtc->disp_mgr) {
        DRM_ERROR("failed to get sunxi display manager.\n");
		return -ENODEV;
    }
    sunxi_crtc->commit_layer->enable = 0;
    for (ch = 0; ch < sunxi_crtc->disp_mgr->num_chns ; ch++) {
        sunxi_crtc->commit_layer->channel = ch;
        for (lyr = 0; lyr < 4; lyr++) {
            sunxi_crtc->commit_layer->layer_id = lyr;
            sunxi_crtc->disp_mgr->set_layer_config(sunxi_crtc->disp_mgr,
                                sunxi_crtc->commit_layer, 1);
        }
    }

    if (NULL == private->handle_vblank) {
        private->handle_vblank = sunxi_drm_blank_handle;
    }

	private->crtc[nr] = crtc;
    sunxi_crt[nr] = sunxi_crtc;
	drm_crtc_init(dev, crtc, &sunxi_crtc_funcs);
	drm_crtc_helper_add(crtc, &sunxi_crtc_helper_funcs);

	return 0;
}

