#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm -Idrivers/gpu/drm/sunxi -Idrivers/video/sunxi/disp2/disp/de
sunxidrm-y := sunxi_drm_drv.o sunxi_encoder.o sunxi_connector.o \
		sunxi_crtc.o  sunxi_fb.o \
		sunxi_dma_buffer.o sunxi_gem.o sunxi_drm_core.o 

obj-y		+= sunxidrm.o

