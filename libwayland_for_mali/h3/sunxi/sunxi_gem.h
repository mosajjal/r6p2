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
#ifndef _SUNXI_DRM_GEM_H_
#define _SUNXI_DRM_GEM_H_

#define to_sunxi_gem_obj(x)	container_of(x,\
			struct sunxi_drm_gem_obj, base)

#define IS_NONCONTIG_BUFFER(f)		(f & SUNXI_BO_NONCONTIG)

struct sunxi_drm_gem_buf {
	void __iomem		*kvaddr;
	dma_addr_t		dma_addr;
	struct dma_attrs	dma_attrs;
	struct sg_table		*sgt;
	unsigned long		size;
    unsigned int        flags;
	bool			pfnmap;
};

struct sunxi_drm_gem_obj {
	struct drm_gem_object		base;
	struct sunxi_drm_gem_buf	*buffer;
	unsigned long			size;
};

int sunxi_drm_gem_init_object(struct drm_gem_object *obj);

void sunxi_drm_gem_free_object(struct drm_gem_object *obj);

int sunxi_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);

int sunxi_drm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

int sunxi_drm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);

int sunxi_drm_gem_dumb_destroy(struct drm_file *file_priv,
				struct drm_device *dev,
				unsigned int handle);

int sunxi_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				   struct drm_device *dev, uint32_t handle,
				   uint64_t *offset);

int sunxi_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args);


struct dma_buf *sunxi_dmabuf_prime_export(struct drm_device *drm_dev,
				struct drm_gem_object *obj, int flags);

struct drm_gem_object *sunxi_dmabuf_prime_import(struct drm_device *drm_dev,
				struct dma_buf *dma_buf);

struct sunxi_drm_gem_obj *sunxi_drm_gem_init(struct drm_device *dev,
						      unsigned long size);
void sunxi_drm_free_buf(struct drm_device *dev,
                struct sunxi_drm_gem_buf *buffer);

void sunxi_drm_fini_buf(struct drm_device *dev,
				struct sunxi_drm_gem_buf *buffer);

struct sunxi_drm_gem_buf *sunxi_drm_init_buf(struct drm_device *dev,
						unsigned int size);

int sunxi_drm_alloc_buf(struct drm_device *dev,
		struct sunxi_drm_gem_buf *buf);
int sunxi_drm_gem_sync_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
void sunxi_sync_buf(struct sunxi_drm_gem_buf *buf);

#endif