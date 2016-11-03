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
#include "sunxi_fb.h"
#include "sunxi_drm_core.h"
#include "sunxi_gem.h"

#define DRIVER_NAME	"sunxi"
#define DRIVER_DESC	"sunxi DRM"
#define DRIVER_DATE	"20151104"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

#define VBLANK_OFF_DELAY	50000

static struct platform_device *sunxi_drm_pdev;

static int sunxi_drm_load(struct drm_device *dev, unsigned long flags)
{
	struct sunxi_drm_private *private;
	int ret;
	int nr;

	DRM_DEBUG_DRIVER("%s\n", __FILE__);
    dev->vblank_disable_allowed = 1;
    dev->irq_enabled = 1;
	private = kzalloc(sizeof(struct sunxi_drm_private), GFP_KERNEL);
	if (!private) {
		DRM_ERROR("failed to allocate private\n");
		return -ENOMEM;
	}

	dev->dev_private = (void *)private;

	drm_mode_config_init(dev);

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(dev);

	sunxi_drm_mode_config_init(dev);

	for (nr = 0; nr < MAX_CRTC; nr++) {
		ret = sunxi_drm_crtc_create(dev, nr);
		if (ret)
			goto err_crtc;
	}

	ret = drm_vblank_init(dev, MAX_CRTC);
	if (ret)
		goto err_crtc;


	ret = sunxi_drm_device_register(dev);
	if (ret)
		goto err_vblank;

	/* setup possible_clones. */
	//sunxi_drm_encoder_setup(dev);

	/*
	 * create and configure fb helper and also sunxi specific
	 * fbdev object.
	 */
	ret = sunxi_drm_fbdev_init(dev);
	if (ret) {
		DRM_ERROR("failed to initialize drm fbdev\n");
		goto err_drm_device;
	}

	drm_vblank_offdelay = VBLANK_OFF_DELAY;

	return 0;

err_drm_device:
	sunxi_drm_device_unregister(dev);
err_vblank:
	drm_vblank_cleanup(dev);
err_crtc:
	drm_mode_config_cleanup(dev);
	kfree(private);

	return ret;
}

static int sunxi_drm_unload(struct drm_device *dev)
{
	DRM_DEBUG_DRIVER("%s\n", __FILE__);

	drm_vblank_cleanup(dev);
	drm_kms_helper_poll_fini(dev);
	drm_mode_config_cleanup(dev);

	kfree(dev->dev_private);

	dev->dev_private = NULL;

	return 0;
}

static int sunxi_drm_open(struct drm_device *dev, struct drm_file *file)
{

    DRM_DEBUG_DRIVER("%s\n", __FILE__);
	return 0;
}

static void sunxi_drm_preclose(struct drm_device *dev,
					struct drm_file *file)
{
    DRM_DEBUG_DRIVER("%s\n", __FILE__);
    return;
}

static void sunxi_drm_postclose(struct drm_device *dev, struct drm_file *file)
{
	DRM_DEBUG_DRIVER("%s\n", __FILE__);
	file->driver_priv = NULL;
}

static void sunxi_drm_lastclose(struct drm_device *dev)
{
	DRM_DEBUG_DRIVER("%s\n", __FILE__);

}

static const struct file_operations sunxi_drm_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.mmap		= sunxi_drm_gem_mmap,
	.poll		= drm_poll,
	.read		= drm_read,
	.unlocked_ioctl	= drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
	.release	= drm_release,
};
    
static struct vm_operations_struct sunxi_drm_gem_vm_ops = {
	.fault = sunxi_drm_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static struct drm_ioctl_desc sunxi_ioctls[] = {
	DRM_IOCTL_DEF_DRV(SUNXI_GEM_SYNC, sunxi_drm_gem_sync_ioctl,
			DRM_UNLOCKED | DRM_AUTH),
};

static struct drm_driver sunxi_drm_driver = {
	.driver_features	= DRIVER_HAVE_IRQ | DRIVER_MODESET |
					DRIVER_GEM | DRIVER_PRIME,
	.load			= sunxi_drm_load,
	.unload			= sunxi_drm_unload,
	.open			= sunxi_drm_open,
	.preclose		= sunxi_drm_preclose,
	.lastclose		= sunxi_drm_lastclose,
	.postclose		= sunxi_drm_postclose,
	.get_vblank_counter	= drm_vblank_count,
	.enable_vblank		= sunxi_drm_crtc_enable_vblank,
	.disable_vblank		= sunxi_drm_crtc_disable_vblank,
	.gem_init_object	= sunxi_drm_gem_init_object,
	.gem_free_object	= sunxi_drm_gem_free_object,
	.gem_vm_ops		= &sunxi_drm_gem_vm_ops,
	.dumb_create		= sunxi_drm_gem_dumb_create,
	.dumb_map_offset	= sunxi_drm_gem_dumb_map_offset,
	.dumb_destroy		= sunxi_drm_gem_dumb_destroy,
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_export	= sunxi_dmabuf_prime_export,
	.gem_prime_import	= sunxi_dmabuf_prime_import,
	.ioctls			= sunxi_ioctls,
	.fops			= &sunxi_drm_driver_fops,
	.name	= DRIVER_NAME,
	.desc	= DRIVER_DESC,
	.date	= DRIVER_DATE,
	.major	= DRIVER_MAJOR,
	.minor	= DRIVER_MINOR,
};

static int sunxi_drm_platform_probe(struct platform_device *pdev)
{
	DRM_DEBUG_DRIVER("%s\n", __FILE__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	sunxi_drm_driver.num_ioctls = DRM_ARRAY_SIZE(sunxi_ioctls);

	return drm_platform_init(&sunxi_drm_driver, pdev);
}

static int sunxi_drm_platform_remove(struct platform_device *pdev)
{
	DRM_DEBUG_DRIVER("%s\n", __FILE__);

	drm_platform_exit(&sunxi_drm_driver, pdev);

	return 0;
}

static struct platform_driver sunxi_drm_platform_driver = {
	.probe		= sunxi_drm_platform_probe,
	.remove		= sunxi_drm_platform_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "sunxi-drm",
	},
};
static int __init sunxi_drm_init(void)
{
	int ret;

	DRM_DEBUG_DRIVER("%s\n", __FILE__);
    /* must wait for display driver initialization */
	ret = platform_driver_register(&sunxi_drm_platform_driver);
	if (ret < 0)
		goto out;

	sunxi_drm_pdev = platform_device_register_simple("sunxi-drm", -1,
				NULL, 0);
	if (IS_ERR(sunxi_drm_pdev)) {
		ret = PTR_ERR(sunxi_drm_pdev);
		goto out;
	}
    /* a fake irq resource for wait_vblank  h3 for 40 */

    struct resource res = {
    /* SUNXI_IRQ_LCD0 */40, 0,"drm_fake_irq",IORESOURCE_IRQ,
    };
    if(platform_device_add_resources(sunxi_drm_pdev, &res, 1))
        DRM_ERROR("failed to add drm_fake_irq.\n");
	return 0;

out:
	platform_driver_unregister(&sunxi_drm_platform_driver);

	return ret;
}

static void __exit sunxi_drm_exit(void)
{
	DRM_DEBUG_DRIVER("%s\n", __FILE__);

	platform_device_unregister(sunxi_drm_pdev);

	platform_driver_unregister(&sunxi_drm_platform_driver);
}

late_initcall(sunxi_drm_init);
module_exit(sunxi_drm_exit);

MODULE_AUTHOR("Jet Cui");
MODULE_DESCRIPTION("Allwinnertech DRM Driver");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_LICENSE("GPL v2");

