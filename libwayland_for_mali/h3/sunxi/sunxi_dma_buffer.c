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
#include <drm/sunxi_drm.h>

#include <linux/dma-buf.h>

#include "sunxi_drm_drv.h"
#include "sunxi_gem.h"

struct page_info {
	struct page *page;
	unsigned int order;
	struct list_head list;
};

static inline int sunxi_order_gfp(unsigned long size, gfp_t *gfp_flags)
{
    int gem_order;

    gem_order = get_order(size);
    if (gem_order >= MAX_ORDER)
        gem_order = MAX_ORDER - 1;
    if (gem_order > 4) {
        *gfp_flags = GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY;
    } else {
        *gfp_flags = GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN;
    }

    if ((1 << gem_order) * PAGE_SIZE - size <= PAGE_SIZE)
        return gem_order;

    return gem_order - 1;
}

static int nocontig_buf_alloc(struct drm_device *dev,
            struct sunxi_drm_gem_buf *buf)
{
    long tmp_size, count;
    int cur_order;
    struct list_head head;
    struct page_info *curs, *tmp_curs;
    gfp_t gfp_flags;
    struct page *page;
    struct scatterlist *sg;

    INIT_LIST_HEAD(&head);
    tmp_size = buf->size;
    if (buf->size / PAGE_SIZE > totalram_pages / 2)
		return -ENOMEM;

    cur_order = sunxi_order_gfp(tmp_size, &gfp_flags);

    if(unlikely(cur_order < 0)) {
        DRM_ERROR("bad order.\n");
        return -ENOMEM;
    }

    count = 0;
    while (tmp_size > 0 && cur_order >= 0) {

        page = alloc_pages(gfp_flags, cur_order);
        if (page == NULL) {
            cur_order--;
            if (cur_order < 0) {
                DRM_ERROR("bad alloc for 0 order.\n");
                goto err_alloc;
            }
            continue;
        }
        curs = kzalloc(sizeof(struct page_info), GFP_KERNEL);
        if (curs == NULL) {
            DRM_ERROR("bad alloc for page info.\n");
            goto err_alloc;
        }
        curs->order = cur_order;
        curs->page = page;
        list_add_tail(&curs->list, &head);
        tmp_size -= (1 << cur_order) * PAGE_SIZE;
        cur_order = sunxi_order_gfp(tmp_size, &gfp_flags);
        count++;

    }

    if (tmp_size > 0) {
        DRM_ERROR("bad alloc err for totle size.\n");
        goto err_alloc;
    }

    buf->sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!buf->sgt) {
        DRM_ERROR("bad alloc for sgt.\n");
		goto err_alloc;
	}

    if (sg_alloc_table(buf->sgt, count, GFP_KERNEL)) {
        DRM_ERROR("bad alloc for sg.\n");
        goto err_alloc;
	}

    sg = buf->sgt->sgl;
    list_for_each_entry_safe(curs, tmp_curs, &head, list) {
        sg_set_page(sg, curs->page, (1 << curs->order) * PAGE_SIZE, 0);
        sg->dma_address = sg_phys(sg);
        list_del(&curs->list);
        kfree(curs);
        sg = sg_next(sg);
    }

    return 0;
err_alloc:
    list_for_each_entry_safe(curs, tmp_curs, &head, list) {
        __free_pages(curs->page, curs->order);
        list_del(&curs->list);
    }

    return -ENOMEM;
}
static int contig_buf_alloc(struct drm_device *dev,
            struct sunxi_drm_gem_buf *buf)
{
    int ret = 0;
	enum dma_attr attr;
	dma_addr_t start_addr;
	DRM_DEBUG_KMS("%s\n", __FILE__);

	init_dma_attrs(&buf->dma_attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &buf->dma_attrs);

	/*
	 * if SUNXI_BO_WC or SUNXI_BO_NONCACHABLE, writecombine mapping
	 * else cachable mapping.
	 */
	if (buf->flags & SUNXI_BO_WC || !(buf->flags & SUNXI_BO_CACHABLE))
		attr = DMA_ATTR_WRITE_COMBINE;
	else
		attr = DMA_ATTR_NON_CONSISTENT;

	dma_set_attr(attr, &buf->dma_attrs);

	buf->kvaddr = dma_alloc_attrs(dev->dev, buf->size,
				&buf->dma_addr, GFP_KERNEL,
				&buf->dma_attrs);
	if (!buf->kvaddr) {
		DRM_ERROR("failed to allocate buffer.\n");
		return -ENOMEM;
	}

	buf->sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!buf->sgt) {
		DRM_ERROR("failed to get sg table.\n");
		ret = -ENOMEM;
		goto err_free_attrs;
	}
	if(sg_alloc_table(buf->sgt, 1, GFP_KERNEL)) {
		DRM_ERROR("failed to init table.\n");
		ret = -ENOMEM;
		kfree(buf->sgt);
		goto err_free_attrs;
	
	}
	sg_set_page(buf->sgt->sgl,phys_to_page(buf->dma_addr),buf->size,0);
	buf->sgt->sgl->dma_address = sg_phys(buf->sgt->sgl);
	DRM_DEBUG_KMS("dma_addr(0x%lx), size(0x%lx)\n",
			(unsigned long)buf->dma_addr,
			buf->size);
	return ret;

err_free_attrs:
	dma_free_attrs(dev->dev, buf->size, buf->kvaddr,
			(dma_addr_t)buf->dma_addr, &buf->dma_attrs);
	buf->dma_addr = (dma_addr_t)NULL;

	return ret;
}
static int lowlevel_buffer_allocate(struct drm_device *dev,
		 struct sunxi_drm_gem_buf *buf)
{

    buf->size = PAGE_ALIGN(buf->size);
    if (buf->flags & SUNXI_BO_CONTIG) {
        return contig_buf_alloc(dev, buf);
    }else{
        return nocontig_buf_alloc(dev, buf);
    }
}

static void lowlevel_buffer_deallocate(struct drm_device *dev,
		 struct sunxi_drm_gem_buf *buf)
{
    int i, order;
    struct page *page;
    struct scatterlist *sg;
	DRM_DEBUG_KMS("%s.\n", __FILE__);

	if ((buf->flags & SUNXI_BO_CONTIG) == 0) {
		DRM_DEBUG_KMS("destroy non-contig mem.\n");
        for_each_sg(buf->sgt->sgl, sg, buf->sgt->nents, i) {
            page = sg_page(sg);
            order = get_order(sg->length);
            __free_pages(page, order);
        }
	}else {
	    DRM_DEBUG_KMS("dma_addr(0x%lx), size(0x%lx)\n",
			(unsigned long)buf->dma_addr,
			buf->size);

	    dma_free_attrs(dev->dev, buf->size, buf->kvaddr,
				(dma_addr_t)buf->dma_addr, &buf->dma_attrs);

	    buf->dma_addr = (dma_addr_t)NULL;
	}

    sg_free_table(buf->sgt);

    kfree(buf->sgt);
    buf->sgt = NULL;
}

struct sunxi_drm_gem_buf *sunxi_drm_init_buf(struct drm_device *dev,
						unsigned int size)
{
	struct sunxi_drm_gem_buf *buffer;

	DRM_DEBUG_KMS("%s.\n", __FILE__);
	DRM_DEBUG_KMS("desired size = 0x%x\n", size);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		DRM_ERROR("failed to allocate sunxi_drm_gem_buf.\n");
		return NULL;
	}

	buffer->size = size;
	return buffer;
}

void sunxi_drm_fini_buf(struct drm_device *dev,
				struct sunxi_drm_gem_buf *buffer)
{
	DRM_DEBUG_KMS("%s.\n", __FILE__);

	if (!buffer) {
		DRM_DEBUG_KMS("buffer is null.\n");
		return;
	}

	kfree(buffer);
	buffer = NULL;
}

int sunxi_drm_alloc_buf(struct drm_device *dev,
		struct sunxi_drm_gem_buf *buf)
{

	/*
	 * allocate memory region and set the memory information
	 * to vaddr and dma_addr of a buffer object.
	 */
	if (lowlevel_buffer_allocate(dev, buf) < 0)
		return -ENOMEM;

	return 0;
}

void sunxi_drm_free_buf(struct drm_device *dev,
                struct sunxi_drm_gem_buf *buffer)
{

	lowlevel_buffer_deallocate(dev, buffer);
}

struct sunxi_drm_dmabuf_attachment {
	struct sg_table sgt;
	enum dma_data_direction dir;
	bool is_mapped;
};

static int sunxi_gem_attach_dma_buf(struct dma_buf *dmabuf,
					struct device *dev,
					struct dma_buf_attachment *attach)
{
	struct sunxi_drm_dmabuf_attachment *sunxi_attach;

	sunxi_attach = kzalloc(sizeof(*sunxi_attach), GFP_KERNEL);
	if (!sunxi_attach)
		return -ENOMEM;

	sunxi_attach->dir = DMA_NONE;
	attach->priv = sunxi_attach;

	return 0;
}

static void sunxi_gem_detach_dma_buf(struct dma_buf *dmabuf,
					struct dma_buf_attachment *attach)
{
	struct sunxi_drm_dmabuf_attachment *sunxi_attach = attach->priv;
	struct sg_table *sgt;

	if (!sunxi_attach)
		return;

	sgt = &sunxi_attach->sgt;

	if (sunxi_attach->dir != DMA_NONE)
		dma_unmap_sg(attach->dev, sgt->sgl, sgt->nents,
				sunxi_attach->dir);

	sg_free_table(sgt);
	kfree(sunxi_attach);
	attach->priv = NULL;
}

static struct sg_table *
		sunxi_gem_map_dma_buf(struct dma_buf_attachment *attach,
					enum dma_data_direction dir)
{
	struct sunxi_drm_dmabuf_attachment *sunxi_attach = attach->priv;
	struct sunxi_drm_gem_obj *gem_obj = attach->dmabuf->priv;
	struct drm_device *dev = gem_obj->base.dev;
	struct sunxi_drm_gem_buf *buf;
	struct scatterlist *rd, *wr;
	struct sg_table *sgt = NULL;
	unsigned int i;
	int nents, ret;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	/* just return current sgt if already requested. */
	if (sunxi_attach->dir == dir && sunxi_attach->is_mapped)
		return &sunxi_attach->sgt;

	buf = gem_obj->buffer;
	if (!buf) {
		DRM_ERROR("buffer is null.\n");
		return ERR_PTR(-ENOMEM);
	}

	sgt = &sunxi_attach->sgt;

	ret = sg_alloc_table(sgt, buf->sgt->orig_nents, GFP_KERNEL);
	if (ret) {
		DRM_ERROR("failed to alloc sgt.\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_lock(&dev->struct_mutex);

	rd = buf->sgt->sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	if (dir != DMA_NONE) {
		nents = dma_map_sg(attach->dev, sgt->sgl, sgt->orig_nents, dir);
		if (!nents) {
			DRM_ERROR("failed to map sgl with iommu.\n");
			sg_free_table(sgt);
			sgt = ERR_PTR(-EIO);
			goto err_unlock;
		}
	}

	sunxi_attach->is_mapped = true;
	sunxi_attach->dir = dir;
	attach->priv = sunxi_attach;

	DRM_DEBUG_PRIME("buffer size = 0x%lx\n", buf->size);

err_unlock:
	mutex_unlock(&dev->struct_mutex);
	return sgt;
}

static void sunxi_gem_unmap_dma_buf(struct dma_buf_attachment *attach,
						struct sg_table *sgt,
						enum dma_data_direction dir)
{
	/* Nothing to do. */
}

static void sunxi_dmabuf_release(struct dma_buf *dmabuf)
{
	struct sunxi_drm_gem_obj *sunxi_gem_obj = dmabuf->priv;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	/*
	 * sunxi_dmabuf_release() call means that file object's
	 * f_count is 0 and it calls drm_gem_object_handle_unreference()
	 * to drop the references that these values had been increased
	 * at drm_prime_handle_to_fd()
	 */
	if (sunxi_gem_obj->base.export_dma_buf == dmabuf) {
		sunxi_gem_obj->base.export_dma_buf = NULL;

		/*
		 * drop this gem object refcount to release allocated buffer
		 * and resources.
		 */
		drm_gem_object_unreference_unlocked(&sunxi_gem_obj->base);
	}
}

static void *sunxi_gem_dmabuf_kmap_atomic(struct dma_buf *dma_buf,
						unsigned long page_num)
{
	/* TODO */

	return NULL;
}

static void sunxi_gem_dmabuf_kunmap_atomic(struct dma_buf *dma_buf,
						unsigned long page_num,
						void *addr)
{
	/* TODO */
}

static void *sunxi_gem_dmabuf_kmap(struct dma_buf *dma_buf,
					unsigned long page_num)
{
	/* TODO */

	return NULL;
}

static void sunxi_gem_dmabuf_kunmap(struct dma_buf *dma_buf,
					unsigned long page_num, void *addr)
{
	/* TODO */
}

static int sunxi_gem_dmabuf_mmap(struct dma_buf *dma_buf,
	struct vm_area_struct *vma)
{

    /* drm don't allow dma_fd to do other but DRM_CLOEXEC,
     * if you want use dma_fd for the mmap(). modify the  
     * drm_prime_handle_to_fd_ioctl add O_RDWR.
     */
    struct sunxi_drm_gem_obj *sunxi_gem;
    struct drm_gem_object *obj;
    struct sunxi_drm_gem_buf *sunxi_buf;
    struct sg_table *table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int i, ret;
    unsigned long remainder, len; 
    struct page *page;

    obj = (struct drm_gem_object *)dma_buf->priv;
    sunxi_gem = to_sunxi_gem_obj(obj);
    sunxi_buf = sunxi_gem->buffer;
    if (sunxi_buf == NULL) {
		DRM_ERROR("not a sunxi_drm_gem_buf, something wronge.\n");
        return -ENOMEM;
    }
    table = sunxi_buf->sgt;

    for_each_sg(table->sgl, sg, table->nents, i) {
		page = sg_page(sg);
		remainder = vma->vm_end - addr;
		len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
				vma->vm_page_prot);
		if (ret)
			return ret;
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

static struct dma_buf_ops sunxi_dmabuf_ops = {
	.attach			= sunxi_gem_attach_dma_buf,
	.detach			= sunxi_gem_detach_dma_buf,
	.map_dma_buf		= sunxi_gem_map_dma_buf,
	.unmap_dma_buf		= sunxi_gem_unmap_dma_buf,
	.kmap			= sunxi_gem_dmabuf_kmap,
	.kmap_atomic		= sunxi_gem_dmabuf_kmap_atomic,
	.kunmap			= sunxi_gem_dmabuf_kunmap,
	.kunmap_atomic		= sunxi_gem_dmabuf_kunmap_atomic,
	.mmap			= sunxi_gem_dmabuf_mmap,
	.release		= sunxi_dmabuf_release,
};

struct dma_buf *sunxi_dmabuf_prime_export(struct drm_device *drm_dev,
				struct drm_gem_object *obj, int flags)
{
	struct sunxi_drm_gem_obj *sunxi_gem_obj = to_sunxi_gem_obj(obj);

	return dma_buf_export(sunxi_gem_obj, &sunxi_dmabuf_ops,
				sunxi_gem_obj->base.size, flags);
}

struct drm_gem_object *sunxi_dmabuf_prime_import(struct drm_device *drm_dev,
				struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct scatterlist *sgl;
	struct sunxi_drm_gem_obj *sunxi_gem_obj;
	struct sunxi_drm_gem_buf *buffer;
	int ret;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	/* is this one of own objects? */
	if (dma_buf->ops == &sunxi_dmabuf_ops) {
		struct drm_gem_object *obj;

		sunxi_gem_obj = dma_buf->priv;
		obj = &sunxi_gem_obj->base;

		/* is it from our device? */
		if (obj->dev == drm_dev) {
			/*
			 * Importing dmabuf exported from out own gem increases
			 * refcount on gem itself instead of f_count of dmabuf.
			 */
			drm_gem_object_reference(obj);
			return obj;
		}
	}

	attach = dma_buf_attach(dma_buf, drm_dev->dev);
	if (IS_ERR(attach))
		return ERR_PTR(-EINVAL);

	get_dma_buf(dma_buf);

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sgt)) {
		ret = PTR_ERR(sgt);
		goto err_buf_detach;
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		DRM_ERROR("failed to allocate sunxi_drm_gem_buf.\n");
		ret = -ENOMEM;
		goto err_unmap_attach;
	}

	sunxi_gem_obj = sunxi_drm_gem_init(drm_dev, dma_buf->size);
	if (!sunxi_gem_obj) {
		ret = -ENOMEM;
		goto err_free_buffer;
	}

	sgl = sgt->sgl;

	buffer->size = dma_buf->size;

	if (sgt->nents == 1) {
		/* always physically continuous memory if sgt->nents is 1. */
		buffer->flags |= SUNXI_BO_CONTIG;
	    buffer->dma_addr = sg_dma_address(sgl);

	} else {
		/*
		 * this case could be CONTIG or NONCONTIG type but for now
		 * sets NONCONTIG.
		 * TODO. we have to find a way that exporter can notify
		 * the type of its own buffer to importer.
		 */
		buffer->flags = SUNXI_BO_NONCONTIG;
	}

	sunxi_gem_obj->buffer = buffer;
	buffer->sgt = sgt;
	sunxi_gem_obj->base.import_attach = attach;

	DRM_DEBUG_PRIME("dma_addr = 0x%x, size = 0x%lx\n", buffer->dma_addr,
								buffer->size);

	return &sunxi_gem_obj->base;

err_free_buffer:
	kfree(buffer);
	buffer = NULL;
err_unmap_attach:
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
err_buf_detach:
	dma_buf_detach(dma_buf, attach);
	dma_buf_put(dma_buf);

	return ERR_PTR(ret);
}

