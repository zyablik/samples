#include "gralloc_stuff.h"
#include <gralloc_common_device.h>
#include <sys/ioctl.h>
#include <math.h>
#include <fcntl.h>


// from vendor/hisi/ap/hardware/display/libcopybit/copybit_utils_365x.cpp
int cscmode_mali2dss(int cscmode) {
    switch (cscmode) {
        case MALI_YUV_BT601_NARROW: return DSS_CSC_601_NARROW;
        case MALI_YUV_BT601_WIDE:   return DSS_CSC_601_WIDE;
        case MALI_YUV_BT709_NARROW: return DSS_CSC_709_NARROW;
        case MALI_YUV_BT709_WIDE:   return DSS_CSC_709_WIDE;
        default:                    return DSS_CSC_601_WIDE;
    }
}

// from vendor/hisi/ap/hardware/display/libcopybit/copybit_utils_365x.cpp
int pixel_format_hal2dss(int format) {
     switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:    return HISI_FB_PIXEL_FORMAT_RGBA_8888;
        case HAL_PIXEL_FORMAT_RGBX_8888:    return HISI_FB_PIXEL_FORMAT_RGBX_8888;
        case HAL_PIXEL_FORMAT_RGB_565:      return HISI_FB_PIXEL_FORMAT_RGB_565;
        case HAL_PIXEL_FORMAT_BGRA_8888:    return HISI_FB_PIXEL_FORMAT_BGRA_8888;
        case HAL_PIXEL_FORMAT_RGBA_5551:    return HISI_FB_PIXEL_FORMAT_RGBA_5551;
        case HAL_PIXEL_FORMAT_RGBA_4444:    return HISI_FB_PIXEL_FORMAT_RGBA_4444;
        case HAL_PIXEL_FORMAT_YV12:         return HISI_FB_PIXEL_FORMAT_YCrCb_420_P;
        case HAL_PIXEL_FORMAT_YCbCr_422_SP: return HISI_FB_PIXEL_FORMAT_YCbCr_422_SP;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
                                            return HISI_FB_PIXEL_FORMAT_YCrCb_420_SP;
        case HAL_PIXEL_FORMAT_YCbCr_422_I:  return HISI_FB_PIXEL_FORMAT_YUV_422_I;
        case HAL_PIXEL_FORMAT_YCbCr_420_SP: return HISI_FB_PIXEL_FORMAT_YCbCr_420_SP;
        case HAL_PIXEL_FORMAT_YCbCr_420_P:  return HISI_FB_PIXEL_FORMAT_YCbCr_420_P;
        case HAL_PIXEL_FORMAT_CbYCrY_422_I: return HISI_FB_PIXEL_FORMAT_UYVY_422_Pkg;
        default: 
            printf("pixel_format_hal2dss: unrecognized src format %d", format);
    }
    return -1;
}

// from vendor/hisi/ap/hardware/display/libcopybit/copybit_utils_365x.cpp
int pixel_format_2bpp(int format) {
    switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_5551:
        case HAL_PIXEL_FORMAT_RGB_565:
            return 2;
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            return 4;
        case HAL_PIXEL_FORMAT_RGBA_4444:
        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
        case HAL_PIXEL_FORMAT_CbYCrY_422_I:
            return 2;
        case HAL_PIXEL_FORMAT_YCbCr_420_P:
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YV12:
            return 1;
        default:
            printf("pixel_format_2bpp: unrecognized hwc format %d", format);
    }
    return -1;
}

// from vendor/hisi/ap/hardware/display/libcopybit/copybit_utils_365x.cpp
bool is_YCrCb_P(int format) {
    if ((HISI_FB_PIXEL_FORMAT_YCbCr_420_P == format) ||
        (HISI_FB_PIXEL_FORMAT_YCrCb_422_P == format) ||
        (HISI_FB_PIXEL_FORMAT_YCrCb_420_P == format)) {
        return true;
    } else {
        return false;
    }
}

// from vendor/hisi/ap/hardware/display/libcopybit/copybit_utils_365x.cpp
#define PRIx64 "lx"
void dump_info(dss_overlay_t const *pov_req) {
    unsigned int i;
    unsigned int k = 0;
    dss_layer_t const *layer = NULL;
    dss_wb_layer_t const *wb_layer = NULL;
    dss_overlay_block_t *pov_h_block_infos = NULL;
    dss_overlay_block_t *pov_block_info = NULL;

    if (!pov_req) {
        ALOGE("pov_req is null!\n");
        return ;
    }

    printf("----------------------------<dump begin>----------------------------\n");
    printf("\n");
    printf("\n");
    printf("\t ovl_idx | dirty_rect   | release_fence \n");
    printf("\t---------+-------------------+-----------------+--------------\n");
    printf("\t %7d | [%2d,%2d,%3d,%3d] | %12d\n",
        pov_req->ovl_idx,
        pov_req->dirty_rect.x,
        pov_req->dirty_rect.y,
        pov_req->dirty_rect.w,
        pov_req->dirty_rect.h,
        pov_req->release_fence);
    printf("\n");

    printf("\t crc_enable_status { crc_ov_result | crc_ldi_result | crc_sum_result | crc_ov_frm | crc_ldi_frm | crc_sum_frm | err_status } \n");
    printf("\t-------------------{---------------+----------------+----------------+------------+-------------+-------------+------------}\n");
    printf("\t %17d { %13d | %14d | %14d | %10d | %11d |%12d | %10d }\n",
        pov_req->crc_enable_status,
        pov_req->crc_info.crc_ov_result,
        pov_req->crc_info.crc_ldi_result,
        pov_req->crc_info.crc_sum_result,
        pov_req->crc_info.crc_ov_frm,
        pov_req->crc_info.crc_ldi_frm,
        pov_req->crc_info.crc_sum_frm,
        pov_req->crc_info.err_status);
    printf("\n");
    printf("\n");

    for (i = 0; i < pov_req->ov_block_nums; i++) {
        pov_h_block_infos = (dss_overlay_block_t *)(pov_req->ov_block_infos_ptr); //lint !e511
        pov_block_info = &(pov_h_block_infos[i]);

        printf("layer_nums = %d\n", pov_block_info->layer_nums);

        for (k = 0; k < pov_block_info->layer_nums; k++) {
            layer = &(pov_block_info->layer_infos[k]);

            printf("\n");
            printf("LayerInfo[%d]:\n", k);
            printf("\t layer_idx | chn_idx | transform | blending | alpha |   color  | need_cap \n");
            printf("\t-----------+---------+-----------+----------+-------+------------+----------\n");
            printf("\t %9d | %7d | %9d | %8d | 0x%3x | 0x%8x | %8u \n",
                layer->layer_idx,
                layer->chn_idx,
                layer->transform,
                layer->blending,
                layer->glb_alpha,
                layer->color,
                layer->need_cap);
            printf("\n");

            printf("\t format |  width | height |  bpp   | stride | stride_plane1 | stride_plane2 |  phy_addr  |  vir_addr  | offset_plane1 | offset_plane2 \n");
            printf("\t--------+--------+--------+--------+--------+---------+---------+------------+------------+------------+------------\n");
            printf("\t %6u | %6u | %6u | %6u | %6u | %7u | %7u | %" PRIx64 " | %" PRIx64 " | %7u | %7u \n",
                layer->img.format,
                layer->img.width,
                layer->img.height,
                layer->img.bpp,
                layer->img.stride,
                layer->img.stride_plane1,
                layer->img.stride_plane2,
                layer->img.phy_addr,
                layer->img.vir_addr,
                layer->img.offset_plane1,
                layer->img.offset_plane2);
            printf("\n");


            printf("\t afbc_header_addr |  afbc_payload_addr | afbc_header_stride |  afbc_payload_stride   |  mmu_enable  |  csc_mode  | secure_mode | shared_fd \n");
            printf("\t------------------+--------------------+---------------+----------------+------------+------------+------------+------------\n");
            printf("\t %" PRIx64 " | %" PRIx64 " | %6u | %6u | %u | %u | %u | %d \n",
                layer->img.afbc_header_addr,
                layer->img.afbc_payload_addr,
                layer->img.afbc_header_stride,
                layer->img.afbc_payload_stride,
                layer->img.mmu_enable,
                layer->img.csc_mode,
                layer->img.secure_mode,
                layer->img.shared_fd);
            printf("\n");

            printf("\t         src_rect         |          dst_rect         |       src_rect_mask       \n");
            printf("\t--------------------------+---------------------------+---------------------------\n");
            printf("\t[%5d,%5d,%5d,%5d] | [%5d,%5d,%5d,%5d] | [%5d,%5d,%5d,%5d] \n",
                layer->src_rect.x,
                layer->src_rect.y,
                layer->src_rect.w,
                layer->src_rect.h,
                layer->dst_rect.x,
                layer->dst_rect.y,
                layer->dst_rect.w,
                layer->dst_rect.h,
                layer->src_rect_mask.x,
                layer->src_rect_mask.y,
                layer->src_rect_mask.w,
                layer->src_rect_mask.h);
        }

        printf("\n");

        for (k = 0; k < pov_req->wb_layer_nums; k++) {
            wb_layer = &(pov_req->wb_layer_infos[k]);

            printf("\nWbLayerInfo[%d]:\n", k);
            printf("\t format |  width | height |  bpp   | stride | stride_plane1 | stride_plane2 |"
                "  phy_addr  |  vir_addr  | offset_plane1 | offset_plane2 | afbc_header_addr | afbc_payload_addr "
                " mmu_enable | csc_mode | secure_mode | secure_mode | shared_fd \n");

            printf("\t--------+--------+--------+--------+--------+---------+---------+------------"
                "+------------+------------+------------+-------------------+-------------------"
                "+--------+--------+--------+--------\n");
            printf("\t %6u | %6u | %6u | %6u | %6u | %7u | %7u | %" PRIx64 " | %" PRIx64 " | %7u | %7u | %" PRIx64 " | %" PRIx64 " |"
                " %6u | %6u | %6u | %6d \n",
                wb_layer->dst.format,
                wb_layer->dst.width,
                wb_layer->dst.height,
                wb_layer->dst.bpp,
                wb_layer->dst.stride,
                wb_layer->dst.stride_plane1,
                wb_layer->dst.stride_plane2,
                wb_layer->dst.phy_addr,
                wb_layer->dst.vir_addr,
                wb_layer->dst.offset_plane1,
                wb_layer->dst.offset_plane2,
                wb_layer->dst.afbc_header_addr,
                wb_layer->dst.afbc_payload_addr,
                wb_layer->dst.mmu_enable,
                wb_layer->dst.csc_mode,
                wb_layer->dst.secure_mode,
                wb_layer->dst.shared_fd);
            printf("\n");

            printf("\t         src_rect         |          dst_rect         | chn_idx |transform\n");
            printf("\t--------------------------+---------------------------|---------|---------|\n");
            printf("\t[%5d,%5d,%5d,%5d] | [%5d,%5d,%5d,%5d] |  %7d|%9d\n",
                wb_layer->src_rect.x,
                wb_layer->src_rect.y,
                wb_layer->src_rect.w,
                wb_layer->src_rect.h,
                wb_layer->dst_rect.x,
                wb_layer->dst_rect.y,
                wb_layer->dst_rect.w,
                wb_layer->dst_rect.h,
                wb_layer->chn_idx,
                wb_layer->transform);
        }
    }

    printf("\n");

    printf("----------------------------<dump end>----------------------------\n");
}


// from vendor/hisi/ap/hardware/display/libgralloc/gralloc_specific_device_hi366x.cpp
int gralloc_specific_fb_post(struct framebuffer_device_t * dev, buffer_handle_t buffer) {
#define MM_BUF_BASE_ADDR    (0x40)
#define MM_BUF_LINE_NUM     (8)

    bool isAfbc = false;
    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(buffer);
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

// disable VSYNC, doesn't work
// #define S3CFB_SET_VSYNC_INT _IOW('F', 206, unsigned int)
//     int val = 0;
//     if(ioctl(m->framebuffer->fd, S3CFB_SET_VSYNC_INT, &val) < 0)
//         printf("S3CFB_SET_VSYNC_INT disable failed for fd: %d %d:%s\n", m->framebuffer->fd, errno, strerror(errno));

    dss_overlay_t ov_dss;
    dss_overlay_block_t ov_block;
    
    memset(&ov_dss,  0, sizeof(dss_overlay_t));
    memset(&ov_block, 0, sizeof(dss_overlay_block_t));

    ov_dss.ovl_idx = DSS_OVL0;
    ov_dss.ov_block_infos_ptr = (uint64_t)(&ov_block);
    ov_dss.ov_block_nums = 1;
    ov_dss.release_fence = -1;

    ov_block.layer_nums = 1;
    ov_block.layer_infos[0].img.format = pixel_format_hal2dss(hnd->format);
    ov_block.layer_infos[0].img.width = hnd->w;
    ov_block.layer_infos[0].img.height = hnd->h;
    ov_block.layer_infos[0].img.bpp = pixel_format_2bpp(hnd->format);
    ov_block.layer_infos[0].img.stride = hnd->byte_stride;
    ov_block.layer_infos[0].img.phy_addr = hnd->phyAddr;
    ov_block.layer_infos[0].img.vir_addr = hnd->iova;

    // frame buffer is AFBCD
    isAfbc = !!(hnd->internal_format & GRALLOC_ARM_INTFMT_AFBC_SPLITBLK);
    if (isAfbc) {
        ov_block.layer_infos[0].need_cap  = CAP_AFBCD;
        ov_block.layer_infos[0].img.width = GRALLOC_ALIGN(hnd->w, AFBC_RECT_ALIGN_PIXELS);
        ov_block.layer_infos[0].img.height = GRALLOC_ALIGN(hnd->h, AFBC_PIXELS_PER_BLOCK);
        ov_block.layer_infos[0].img.mmbuf_base =  MM_BUF_BASE_ADDR;
        ov_block.layer_infos[0].img.mmbuf_size =  GRALLOC_ALIGN(hnd->w, AFBC_RECT_ALIGN_PIXELS) * pixel_format_2bpp(hnd->format) * MM_BUF_LINE_NUM;
    }

    ov_block.layer_infos[0].img.afbc_header_addr = hnd->afbc_header_addr;
    ov_block.layer_infos[0].img.afbc_header_stride = hnd->afbc_header_stride;
    ov_block.layer_infos[0].img.afbc_payload_addr = hnd->afbc_payload_addr;
    ov_block.layer_infos[0].img.afbc_payload_stride = hnd->afbc_payload_stride;
    ov_block.layer_infos[0].img.afbc_scramble_mode = hnd->afbc_scramble;

    if (is_YCrCb_P(hnd->format)) {
        ov_block.layer_infos[0].img.stride_plane1= hnd->vStride;
        ov_block.layer_infos[0].img.offset_plane1= hnd->vOffset;
        ov_block.layer_infos[0].img.stride_plane2= hnd->uStride;
        ov_block.layer_infos[0].img.offset_plane2= hnd->uOffset;
    } else {
        ov_block.layer_infos[0].img.stride_plane1= hnd->uStride;
        ov_block.layer_infos[0].img.offset_plane1= hnd->uOffset;
        ov_block.layer_infos[0].img.stride_plane2= hnd->vStride;
        ov_block.layer_infos[0].img.offset_plane2= hnd->vOffset;
    }

    ov_block.layer_infos[0].img.buf_size = hnd->iova_size;
    ov_block.layer_infos[0].img.mmu_enable = hnd->phyAddr ? 0 : 1;
    ov_block.layer_infos[0].img.csc_mode = cscmode_mali2dss(hnd->yuv_info);
    ov_block.layer_infos[0].transform = HISI_FB_TRANSFORM_NOP;
    ov_block.layer_infos[0].blending = HISI_FB_BLENDING_NONE;
    ov_block.layer_infos[0].img.shared_fd = -1;

    ov_block.layer_infos[0].src_rect.x = 0;
    ov_block.layer_infos[0].src_rect.y = 0;
    ov_block.layer_infos[0].src_rect.w = hnd->w;
    ov_block.layer_infos[0].src_rect.h = hnd->h;
    ov_block.layer_infos[0].dst_rect.x = 0;
    ov_block.layer_infos[0].dst_rect.y = 0;
    ov_block.layer_infos[0].dst_rect.w = hnd->w;
    ov_block.layer_infos[0].dst_rect.h = hnd->h;
    ov_block.layer_infos[0].src_rect_mask.x = 0;
    ov_block.layer_infos[0].src_rect_mask.y = 0;
    ov_block.layer_infos[0].src_rect_mask.w = 0;
    ov_block.layer_infos[0].src_rect_mask.h = 0;
    ov_block.layer_infos[0].glb_alpha = 0xff;

    ov_block.layer_infos[0].layer_idx = 0;
    ov_block.layer_infos[0].chn_idx = DSS_RCHN_D2;
    ov_block.layer_infos[0].acquire_fence = -1;

    if (ioctl(m->framebuffer->fd, HISIFB_OV_ONLINE_PLAY, &ov_dss) < 0) {
        printf("gralloc_specific_fb_post: ioctl(HISIFB_OV_ONLINE_PLAY) error: %d: %s\n", errno, strerror(errno));
        return -1;
    }

    if (ov_dss.release_fence >= 0) {
        close(ov_dss.release_fence);
    }
    return 0;
}
