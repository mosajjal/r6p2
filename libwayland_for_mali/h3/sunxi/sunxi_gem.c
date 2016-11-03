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

#include <linux/shmem_fs.h>
#include <drm/sunxi_drm.h>

#include "sunxi_drm_drv.h"
#include "sunxi_gem.h"
#include <linux/scatterlist.h>
struct sunxi_drm_gem_obj *sunxi_drm_gem_create(struct drm_device *dev,
						unsigned int flags,
						unsigned long size);

static int sunxi_drm_gem_handle_create(struct drm_gem_object *obj,
					struct drm_file *file_priv,
					unsigned int *handle);

void sunxi_drm_gem_destroy(struct sunxi_drm_gem_obj *sunxi_gem_obj);

int sunxi_drm_gem_init_object(struct drm_gem_object *obj)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	return 0;
}

static void update_vm_cache_attr(struct sunxi_drm_gem_obj *obj,
					struct vm_area_struct *vma)
{
	DRM_DEBUG_KMS("flags = 0x%x\n", obj->buffer->flags);

	/* non-cachable as default. */
	if (obj->buffer->flags & SUNXI_BO_CACHABLE)
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	else if (obj->buffer->flags & SUNXI_BO_WC)
		vma->vm_page_prot =
			pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
	else
		vma->vm_page_prot =
			pgprot_noncached(vm_get_page_prot(vma->vm_flags));
}

static unsigned int convert_to_vm_err_msg(int msg)
{
	unsigned int out_msg;

	switch (msg) {
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
		out_msg = VM_FAULT_NOPAGE;
		break;

	case -ENOMEM:
		out_msg = VM_FAULT_OOM;
		break;

	default:
		out_msg = VM_FAULT_SIGBUS;
		break;
	}

	return out_msg;
}

static int check_gem_flags(unsigned int flags)
{
	if (flags & ~(SUNXI_BO_MASK)) {
		DRM_ERROR("invalid flags.\n");
		return -EINVAL;
	}

	return 0;
}

int sunxi_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct sunxi_drm_gem_obj *sunxi_gem_obj;
	struct drm_gem_object *obj;
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* set vm_area_struct. */
	ret = drm_gem_mmap(filp, vma);
	if (ret < 0) {
		DRM_ERROR("failed to mmap.\n");
		return ret;
	}

	obj = vma->vm_private_data;
	sunxi_gem_obj = to_sunxi_gem_obj(obj);

	ret = check_gem_flags(sunxi_gem_obj->buffer->flags);
	if (ret) {
		drm_gem_vm_close(vma);
		drm_gem_free_mmap_offset(obj);
		return ret;
	}

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_flags |= VM_MIXEDMAP;

	update_vm_cache_attr(sunxi_gem_obj, vma);

	return ret;
}

int sunxi_drm_gem_dumb_destroy(struct drm_file *file_priv,
				struct drm_device *dev,
				unsigned int handle)
{
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/*
	 * obj->refcount and obj->handle_count are decreased and
	 * if both them are 0 then sunxi_drm_gem_free_object()
	 * would be called by callback to release resources.
	 */
	ret = drm_gem_handle_delete(file_priv, handle);
	if (ret < 0) {
		DRM_ERROR("failed to delete drm_gem_handle.\n");
		return ret;
	}

	return 0;
}

int sunxi_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				   struct drm_device *dev, uint32_t handle,
				   uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	mutex_lock(&dev->struct_mutex);

	/*
	 * get offset of memory allocated for drm framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_MAP_DUMB command.
	 */

	obj = drm_gem_object_lookup(dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		ret = -EINVAL;
		goto unlock;
	}

	if (!obj->map_list.map) {
		ret = drm_gem_create_mmap_offset(obj);
		if (ret)
			goto out;
	}

	*offset = (u64)obj->map_list.hash.key << PAGE_SHIFT;
	DRM_DEBUG_KMS("offset = 0x%lx\n", (unsigned long)*offset);

out:
	drm_gem_object_unreference(obj);
unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int sunxi_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args)
{
	struct sunxi_drm_gem_obj *sunxi_gem_obj;
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/*
	 * alocate memory to be used for framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_CREATE_DUMB command.
	 */

	args->pitch = args->width * ((args->bpp + 7) / 8);
	args->size = args->pitch * args->height;

	sunxi_gem_obj = sunxi_drm_gem_create(dev, args->flags, args->size);
	if (IS_ERR(sunxi_gem_obj))
		return PTR_ERR(sunxi_gem_obj);

	ret = sunxi_drm_gem_handle_create(&sunxi_gem_obj->base, file_priv,
			&args->handle);
	if (ret) {
		sunxi_drm_gem_destroy(sunxi_gem_obj);
		return ret;
	}

	return 0;
}

static int sunxi_drm_gem_map_buf(struct drm_gem_object *obj,
					struct vm_area_struct *vma,
					unsigned long f_vaddr,
					pgoff_t page_offset)
{
	struct sunxi_drm_gem_obj *sunxi_gem_obj = to_sunxi_gem_obj(obj);
	struct sunxi_drm_gem_buf *buf = sunxi_gem_obj->buffer;
	struct scatterlist *sgl;
	unsigned long pfn;
	int i;

	if (!buf->sgt)
		return -EINTR;

	if (page_offset >= (buf->size >> PAGE_SHIFT)) {
		DRM_ERROR("invalid page offset\n");
		return -EINVAL;
	}

	sgl = buf->sgt->sgl;
	for_each_sg(buf->sgt->sgl, sgl, buf->sgt->nents, i) {
		if (page_offset < (sgl->length >> PAGE_SHIFT))
			break;
		page_offset -=	(sgl->length >> PAGE_SHIFT);
	}

	pfn = __phys_to_pfn(sg_phys(sgl)) + page_offset;

	return vm_insert_mixed(vma, f_vaddr, pfn);
}

void sunxi_drm_gem_free_object(struct drm_gem_object *obj)
{
	struct sunxi_drm_gem_obj *sunxi_gem_obj;
	struct sunxi_drm_gem_buf *buf;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	sunxi_gem_obj = to_sunxi_gem_obj(obj);
	buf = sunxi_gem_obj->buffer;

	sunxi_drm_gem_destroy(to_sunxi_gem_obj(obj));
}

struct sunxi_drm_gem_obj *sunxi_drm_gem_init(struct drm_device *dev,
						      unsigned long size)
{
	struct sunxi_drm_gem_obj *sunxi_gem_obj;
	struct drm_gem_object *obj;
	int ret;

	sunxi_gem_obj = kzalloc(sizeof(*sunxi_gem_obj), GFP_KERNEL);
	if (!sunxi_gem_obj) {
		DRM_ERROR("failed to allocate sunxi gem object\n");
		return NULL;
	}

	sunxi_gem_obj->size = size;
	obj = &sunxi_gem_obj->base;

	ret = drm_gem_object_init(dev, obj, size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		kfree(sunxi_gem_obj);
		return NULL;
	}

	DRM_DEBUG_KMS("created file object = 0x%x\n", (unsigned int)obj->filp);

	return sunxi_gem_obj;
}

static int sunxi_drm_gem_handle_create(struct drm_gem_object *obj,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	int ret;

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, obj, handle);
	if (ret)
		return ret;

	DRM_DEBUG_KMS("gem handle = 0x%x\n", *handle);

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

void sunxi_drm_gem_destroy(struct sunxi_drm_gem_obj *sunxi_gem_obj)
{
	struct drm_gem_object *obj;
	struct sunxi_drm_gem_buf *buf;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	obj = &sunxi_gem_obj->base;
	buf = sunxi_gem_obj->buffer;

	DRM_DEBUG_KMS("handle count = %d\n", atomic_read(&obj->handle_count));

	if (obj->import_attach) {
        drm_prime_gem_destroy(&sunxi_gem_obj->base, buf->sgt);
	} else {
	    sunxi_drm_free_buf(obj->dev, buf);
	}

out:
	sunxi_drm_fini_buf(obj->dev, buf);
	sunxi_gem_obj->buffer = NULL;

	if (obj->map_list.map)
		drm_gem_free_mmap_offset(obj);

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(sunxi_gem_obj);
	sunxi_gem_obj = NULL;
}

struct sunxi_drm_gem_obj *sunxi_drm_gem_create(struct drm_device *dev,
						unsigned int flags,
						unsigned long size)
{
	struct sunxi_drm_gem_obj *sunxi_gem_obj;
	struct sunxi_drm_gem_buf *buf;
	int ret;

	if (!size) {
		DRM_ERROR("invalid size.\n");
		return ERR_PTR(-EINVAL);
	}

	size = roundup(size, PAGE_SIZE);
	DRM_DEBUG_KMS("%s\n", __FILE__);

	ret = check_gem_flags(flags);
	if (ret)
		return ERR_PTR(ret);

	buf = sunxi_drm_init_buf(dev, size);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	sunxi_gem_obj = sunxi_drm_gem_init(dev, size);
	if (!sunxi_gem_obj) {
		ret = -ENOMEM;
		goto err_fini_buf;
	}

	sunxi_gem_obj->buffer = buf;

	/* set memory type and cache attribute from user side. */
    buf->flags = flags;
	ret = sunxi_drm_alloc_buf(dev, buf);
	if (ret < 0) {
		drm_gem_object_release(&sunxi_gem_obj->base);
		goto err_fini_buf;
	}

	return sunxi_gem_obj;

err_fini_buf:
	sunxi_drm_fini_buf(dev, buf);
	return ERR_PTR(ret);
}
 int ctk = 0;
EXPORT_SYMBOL_GPL(ctk);
void sunxi_sync_buf(struct sunxi_drm_gem_buf *buf)
{
printk(KERN_WARNING "%s page:%x  addr:%p  len:%d######\n",current->comm,sg_page(buf->sgt->sgl),buf->kvaddr,buf->sgt->nents);

ctk = 1;
    dma_sync_sg_for_device(NULL, buf->sgt->sgl,
			        buf->sgt->nents, DMA_BIDIRECTIONAL);
ctk = 0;
}

int sunxi_drm_gem_sync_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
    int ret = 0;
    struct sunxi_sync_gem_cmd *gem_cmd;
	struct drm_gem_object *obj;
    struct sunxi_drm_gem_buf *buf;
    struct sunxi_drm_gem_obj *sunxi_gem;

    gem_cmd = (struct sunxi_sync_gem_cmd *)data;
   	obj = drm_gem_object_lookup(dev, file_priv, gem_cmd->gem_handle);
    if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return -EINVAL;
	}
    sunxi_gem = to_sunxi_gem_obj(obj);
    buf = sunxi_gem->buffer;
    if (!buf) {
        DRM_ERROR("failed to lookup sunxi_drm_gem_buf.\n");
		ret = -EINVAL;
		goto out;
	}

    sunxi_sync_buf(buf);
out:
    drm_gem_object_unreference(obj);

    return ret;
}


int sunxi_drm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_device *dev = obj->dev;
	unsigned long f_vaddr;
	pgoff_t page_offset;
	int ret;

	page_offset = ((unsigned long)vmf->virtual_address -
			vma->vm_start) >> PAGE_SHIFT;
	f_vaddr = (unsigned long)vmf->virtual_address;

	mutex_lock(&dev->struct_mutex);

	ret = sunxi_drm_gem_map_buf(obj, vma, f_vaddr, page_offset);
	if (ret < 0)
		DRM_ERROR("failed to map a buffer with user.\n");

	mutex_unlock(&dev->struct_mutex);

	return convert_to_vm_err_msg(ret);
}
