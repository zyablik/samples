#pragma once

#include <stdint.h>
//#ifdef GRALLOC_PRIVATE_FB_POST
#include <linux/hisi_dss.h>
//#endif
#include <hardware/gralloc.h>

void dump_info(dss_overlay_t const *pov_req);
int gralloc_specific_fb_post(struct framebuffer_device_t * dev, buffer_handle_t buffer);
