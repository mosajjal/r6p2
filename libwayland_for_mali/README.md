Original notes on how to use these drivers. 

1.we use the drm-backend.so backend, and must use drm driver.

2.libMali.so has itself libgbm.so,so you must modify the weston compositor to load libmali.so:

`/weston/libweston/compositor-drm.c:`

......
```c
static struct gbm_device *
create_gbm_device(int fd)
{
struct gbm_device *gbm;

gl_renderer = weston_load_module("gl-renderer.so",
 "gl_renderer_interface");
if (!gl_renderer)
return NULL;

/* GBM will load a dri driver, but even though they need symbols from
 * libglapi, in some version of Mesa they are not linked to it. Since
 * only the gl-renderer module links to it, the call above won't make
 * these symbols globally available, and loading the DRI driver fails.
 * Workaround this by dlopen()'ing libglapi with RTLD_GLOBAL. */
dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL);//here load libMali

gbm = gbm_create_device(fd);

return gbm;
}
```
......
when gbm_bo_create with GBM_BO_USE_SCANOUT flags, we(drm driver) use dma_alloc_attrs to alloc continuous memory,  other use nocontig_buf_alloc to alloc non- continuous memory(egl-wayland-client app).

3. we use r6p2-01rel0.
