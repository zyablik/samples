#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>


#include <drm/i915_drm.h>
#include <libdrm/intel_bufmgr.h>

#define BATCH_SZ 4096

#define WIDTH 512
#define STRIDE (WIDTH * 4)
#define HEIGHT 512

#define SRC_COLOR	0xff0000ff
#define DST_COLOR	0xffff0000

// #define ALIGN(v, a) (((v) + (a)-1) & ~((a)-1)) // alignment unit in bytes

uint8_t * ALIGN(void * pointer, uintptr_t alignment) {
    uint8_t * p = (uint8_t *) pointer + alignment - 1;
    return (uint8_t *)(((uintptr_t) p) & -alignment);
}

void write_ppm(const char * file, uint32_t width, uint32_t height, uint32_t * argb) {
    printf("write_ppm file: %s width = %d height = %d, argb = %p\n", file, width, height, argb);
    FILE * fp = fopen(file, "wb");
    if(fp == NULL) {
        printf("error while open file '%s': %d:%s\n", file, errno, strerror(errno));
        return;
    }

    fprintf(fp, "P3\n%d %d\n255\n", width, height);
    for (size_t j = 0; j < height; ++j) {
        for (size_t i = 0; i < width; ++i) {
            uint8_t color[3];
            uint8_t red = (argb[j * height + i] >> 16) & 0x000000ff;
            uint8_t green = (argb[j * height + i] >> 8) & 0x000000ff;
            uint8_t blue = (argb[j * height + i] >> 0) & 0x000000ff;

            fprintf(fp, "%d %d %d  ", red, green, blue);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}

int main(int argc, char ** argv) {
    /// data.drm_fd = drm_open_driver_render(DRIVER_INTEL);

    const char * dri_path = "/dev/dri/card0";

    int dri_fd = open(dri_path, O_RDWR);
    if(dri_fd == -1) {
        printf("error while open %s: %d:%s\n", dri_path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("open(%s) done: dri_fd = %d\n", dri_path, dri_fd);

    /// data.devid = intel_get_drm_devid(data.drm_fd);
    struct drm_i915_getparam gp;
    int devid = 0;

    memset(&gp, 0, sizeof(gp));
    gp.param = I915_PARAM_CHIPSET_ID;
    gp.value = &devid;

    int ret = ioctl(dri_fd, DRM_IOCTL_I915_GETPARAM, &gp, sizeof(gp));
    if (ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GETPARAM, I915_PARAM_CHIPSET_ID): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("ioctl(DRM_IOCTL_I915_GETPARAM, I915_PARAM_CHIPSET_ID): devid = %d\n", devid);

    /// data.bufmgr = drm_intel_bufmgr_gem_init(data.drm_fd, 4096);

    drm_intel_bufmgr * bufmgr = drm_intel_bufmgr_gem_init(dri_fd, 4096);
    printf("drm_intel_bufmgr_gem_init(): bufmgr = %p\n", bufmgr);

    drm_intel_bo * batch_bo = drm_intel_bo_alloc(bufmgr, "batch_bo", BATCH_SZ, 4096);
    printf("drm_intel_bo_alloc(BATCH_SZ = %d): batch_bo = %p\n", BATCH_SZ, batch_bo);

    /// scratch_buf_init(&data, &src, WIDTH, HEIGHT, STRIDE, SRC_COLOR);
    __attribute__((aligned(4096))) uint32_t linear[WIDTH * HEIGHT] = { 0 };

    drm_intel_bo * src_bo = drm_intel_bo_alloc(bufmgr, "src_bo", HEIGHT * STRIDE, 4096);
    printf("drm_intel_bo_alloc(HEIGHT * STRIDE = %d): src_bo = %p\n", HEIGHT * STRIDE, src_bo);

    for (int i = 0; i < WIDTH * HEIGHT; i++)
        linear[i] = SRC_COLOR;

    /// gem_write(data->drm_fd, src_bo->handle, 0, data->linear, sizeof(data->linear));
    struct drm_i915_gem_pwrite gem_pwrite_src = { 0 };
    gem_pwrite_src.handle = src_bo->handle;
    gem_pwrite_src.size = sizeof(linear);
    gem_pwrite_src.data_ptr = (uintptr_t)linear;
    ret = ioctl(dri_fd, DRM_IOCTL_I915_GEM_PWRITE, &gem_pwrite_src);
    if (ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GEM_PWRITE, src_bo->handle): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("ioctl(DRM_IOCTL_I915_GEM_PWRITE, src_bo->handle): filled whith 0x%x \n", SRC_COLOR);

    drm_intel_bo * dst_bo = drm_intel_bo_alloc(bufmgr, "dst_bo", HEIGHT * STRIDE, 4096);
    printf("drm_intel_bo_alloc(dst_bo, HEIGHT * STRIDE = %d): dst_bo = %p\n", HEIGHT * STRIDE, dst_bo);

    for (int i = 0; i < WIDTH * HEIGHT; i++)
        linear[i] = DST_COLOR;

    /// gem_write(drm_fd, dst_bo->handle, 0, linear, sizeof(linear));
    struct drm_i915_gem_pwrite gem_pwrite_dst = { 0 };
    gem_pwrite_dst.handle = dst_bo->handle;
    gem_pwrite_dst.size = sizeof(linear);
    gem_pwrite_dst.data_ptr = (uintptr_t)linear;
    ret = ioctl(dri_fd, DRM_IOCTL_I915_GEM_PWRITE, &gem_pwrite_dst);
    if (ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GEM_PWRITE, dst_bo->handle): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("ioctl(DRM_IOCTL_I915_GEM_PWRITE, dst_bo->handle): filled whith 0x%x \n", DST_COLOR);

    /* This will copy the src to the mid point of the dst buffer. Presumably
        * the out of bounds accesses will get clipped.
        * Resulting buffer should look like:
        *      _______
        *     |dst|dst|
        *     |dst|src|
        *      -------
    */
    /// gen7_render_copyfunc(batch, NULL, &src, 0, 0, WIDTH, HEIGHT, &dst, WIDTH / 2, HEIGHT / 2);

    __attribute__((aligned(4096))) uint8_t batch_buffer[BATCH_SZ] = { 0 };
    uint32_t * ptr = (uint32_t *)batch_buffer;
    uint8_t * state = batch_buffer + sizeof(batch_buffer) / 2;

    printf("batch_buffer = %p state = %p end = %p\n", batch_buffer, state, batch_buffer + sizeof(batch_buffer));

    #define GEN7_3D(Pipeline,Opcode,Subopcode) ((3 << 29) | \
                                               ((Pipeline) << 27) | \
                                               ((Opcode) << 24) | \
                                               ((Subopcode) << 16))

    #define GEN7_PIPELINE_SELECT GEN7_3D(1, 1, 4)
    #define PIPELINE_SELECT_3D 0

    *ptr = (GEN7_PIPELINE_SELECT | PIPELINE_SELECT_3D);
    ptr++;

    // ------------------------------------------------------------------------
    /// gen7_emit_state_base_address(batch);

    #define GEN7_STATE_BASE_ADDRESS GEN7_3D(0, 1, 1)
    *ptr++ = GEN7_STATE_BASE_ADDRESS | (10 - 2);

    // DWORD 1: General State Base Address
    *ptr++ = 0;

    /// OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0, BASE_ADDRESS_MODIFY);
    #define BASE_ADDRESS_MODIFY (1 << 0)
    ret = drm_intel_bo_emit_reloc(batch_bo, (uint8_t *)ptr - batch_buffer, batch_bo, BASE_ADDRESS_MODIFY, I915_GEM_DOMAIN_INSTRUCTION, 0);
    if (ret != 0) {
        printf("error while drm_intel_bo_emit_reloc(I915_GEM_DOMAIN_INSTRUCTION): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // DWORD 2: Surface State Base Address
    *ptr++ = batch_bo->offset64 | BASE_ADDRESS_MODIFY;

    /// OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0, BASE_ADDRESS_MODIFY);
    ret = drm_intel_bo_emit_reloc(batch_bo, (uint8_t *)ptr - batch_buffer, batch_bo, BASE_ADDRESS_MODIFY, I915_GEM_DOMAIN_INSTRUCTION, 0);
    if (ret != 0) {
        printf("error while drm_intel_bo_emit_reloc(I915_GEM_DOMAIN_INSTRUCTION): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    // DWORD 3: Dynamic State Base Address
    *ptr++ = batch_bo->offset64 | BASE_ADDRESS_MODIFY;

    // DWORD 4: Indirect Object Base Address
    *ptr++ = 0;

    /// OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0, BASE_ADDRESS_MODIFY);
    ret = drm_intel_bo_emit_reloc(batch_bo, (uint8_t *)ptr - batch_buffer, batch_bo, BASE_ADDRESS_MODIFY, I915_GEM_DOMAIN_INSTRUCTION, 0);
    if (ret != 0) {
        printf("error while drm_intel_bo_emit_reloc(I915_GEM_DOMAIN_INSTRUCTION): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    // DWORD 5: Instruction Base Address
    *ptr++ = batch_bo->offset64 | BASE_ADDRESS_MODIFY;

    *ptr++ = 0;
    *ptr++ = (0 | BASE_ADDRESS_MODIFY);
    *ptr++ = 0;
    *ptr++ = (0 | BASE_ADDRESS_MODIFY);

    // ------------------------------------------------------------------------
    /// gen7_emit_multisample(batch);

    #define GEN7_3DSTATE_MULTISAMPLE GEN7_3D(3, 1, 0x0d)
    *ptr++ = (GEN7_3DSTATE_MULTISAMPLE | (4 - 2));

    #define GEN7_3DSTATE_MULTISAMPLE_PIXEL_LOCATION_CENTER (0 << 4)
    #define GEN7_3DSTATE_MULTISAMPLE_NUMSAMPLES_1          (0 << 1)
    *ptr++ = (GEN7_3DSTATE_MULTISAMPLE_PIXEL_LOCATION_CENTER | GEN7_3DSTATE_MULTISAMPLE_NUMSAMPLES_1);  // 1 sample/pixel

    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_SAMPLE_MASK GEN7_3D(3, 0, 0x18)
    *ptr++ = (GEN7_3DSTATE_SAMPLE_MASK | (2 - 2));
    *ptr++ = 1;

    ------------------------------------------------------------------------
    // gen7_emit_urb(batch);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS GEN7_3D(3, 1, 0x16)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS | (2 - 2));
    *ptr++ = 8; // in 1KBs

    // num of VS entries must be divisible by 8 if size < 9
    #define GEN7_3DSTATE_URB_VS GEN7_3D(3, 0, 0x30)
    *ptr++ = (GEN7_3DSTATE_URB_VS | (2 - 2));

    #define GEN7_URB_ENTRY_NUMBER_SHIFT     0
    #define GEN7_URB_ENTRY_SIZE_SHIFT       16
    #define GEN7_URB_STARTING_ADDRESS_SHIFT 25
    *ptr++ = ((64     << GEN7_URB_ENTRY_NUMBER_SHIFT) |
              (2 - 1) << GEN7_URB_ENTRY_SIZE_SHIFT    |
              (1      << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_HS GEN7_3D(3, 0, 0x31)
    *ptr++ = (GEN7_3DSTATE_URB_HS | (2 - 2));
    *ptr++ =((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (2 << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_DS GEN7_3D(3, 0, 0x32)
    *ptr++ = (GEN7_3DSTATE_URB_DS | (2 - 2));
    *ptr++ = ((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (2 << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_GS GEN7_3D(3, 0, 0x33)
    *ptr++ = (GEN7_3DSTATE_URB_GS | (2 - 2));
    *ptr++ = ((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (1 << GEN7_URB_STARTING_ADDRESS_SHIFT));

    // ------------------------------------------------------------------------
    /// gen7_emit_vs(batch);
    #define GEN7_3DSTATE_VS GEN7_3D(3, 0, 0x10)
    *ptr++ = (GEN7_3DSTATE_VS | (6 - 2));
    *ptr++ = 0; // no VS kernel
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0; // pass-through

    // ------------------------------------------------------------------------
    /// gen7_emit_hs(batch);
    #define GEN7_3DSTATE_HS GEN7_3D(3, 0, 0x1b)
    *ptr++ = (GEN7_3DSTATE_HS | (7 - 2));
    *ptr++ = 0; // no HS kernel
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0; // pass-through

    // ------------------------------------------------------------------------
    /// gen7_emit_te(batch);
    #define GEN7_3DSTATE_TE GEN7_3D(3, 0, 0x1c)
    *ptr++ = (GEN7_3DSTATE_TE | (4 - 2));
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_ds(batch);
    #define GEN7_3DSTATE_DS GEN7_3D(3, 0, 0x1d)
    *ptr++ = (GEN7_3DSTATE_DS | (6 - 2));
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_gs(batch);
    #define GEN7_3DSTATE_GS GEN7_3D(3, 0, 0x11)
    *ptr++ = (GEN7_3DSTATE_GS | (7 - 2));
    *ptr++ = 0; // no GS kernel
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0; // pass-through

    // ------------------------------------------------------------------------
    /// gen7_emit_clip(batch);
    #define GEN7_3DSTATE_CLIP GEN7_3D(3, 0, 0x12)
    *ptr++ = (GEN7_3DSTATE_CLIP | (4 - 2));
    *ptr++ = 0;
    *ptr++ = 0; // pass-through
    *ptr++ = 0;

    #define GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CL GEN7_3D(3, 0, 0x21)
    *ptr++ = (GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CL | (2 - 2));
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_sf(batch);
    #define GEN7_3DSTATE_SF GEN7_3D(3, 0, 0x13)
    *ptr++ = (GEN7_3DSTATE_SF | (7 - 2));
    *ptr++ = 0;

    #define GEN7_3DSTATE_SF_CULL_NONE (1 << 29)
    *ptr++ = GEN7_3DSTATE_SF_CULL_NONE;

    #define GEN7_3DSTATE_SF_TRIFAN_PROVOKE_SHIFT 25
    *ptr++ = (2 << GEN7_3DSTATE_SF_TRIFAN_PROVOKE_SHIFT);
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_wm(batch);
    #define GEN7_3DSTATE_WM GEN7_3D(3, 0, 0x14)
    *ptr++ = (GEN7_3DSTATE_WM | (3 - 2));

    #define GEN7_WM_DISPATCH_ENABLE (1 << 29)
    #define GEN7_WM_PERSPECTIVE_PIXEL_BARYCENTRIC (1 << 11)
    *ptr++ = (GEN7_WM_DISPATCH_ENABLE | GEN7_WM_PERSPECTIVE_PIXEL_BARYCENTRIC);
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_streamout(batch);
    #define GEN7_3DSTATE_STREAMOUT GEN7_3D(3, 0, 0x1e)
    *ptr++ = (GEN7_3DSTATE_STREAMOUT | (3 - 2));
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_null_depth_buffer(batch);
    #define GEN7_3DSTATE_DEPTH_BUFFER GEN7_3D(3, 0, 0x05)
    *ptr++ = (GEN7_3DSTATE_DEPTH_BUFFER | (7 - 2));

    #define GEN7_SURFACE_NULL 7
    #define GEN7_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT 29
    #define GEN7_DEPTHFORMAT_D32_FLOAT 1
    #define GEN7_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT 18
    *ptr++ = (GEN7_SURFACE_NULL << GEN7_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT |
                GEN7_DEPTHFORMAT_D32_FLOAT << GEN7_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT);

    *ptr++ = 0; // disable depth, stencil and hiz 
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_CLEAR_PARAMS GEN7_3D(3, 0, 0x04)
    *ptr++ = (GEN7_3DSTATE_CLEAR_PARAMS | (3 - 2));
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_cc(batch);
    #define GEN7_3DSTATE_BLEND_STATE_POINTERS GEN7_3D(3, 0, 0x24)
    *ptr++ = (GEN7_3DSTATE_BLEND_STATE_POINTERS | (2 - 2));

    /// *ptr++ = gen7_create_blend_state(batch);

    struct gen7_blend_state {
        struct {
            uint32_t dest_blend_factor:5;
            uint32_t source_blend_factor:5;
            uint32_t pad3:1;
            uint32_t blend_func:3;
            uint32_t pad2:1;
            uint32_t ia_dest_blend_factor:5;
            uint32_t ia_source_blend_factor:5;
            uint32_t pad1:1;
            uint32_t ia_blend_func:3;
            uint32_t pad0:1;
            uint32_t ia_blend_enable:1;
            uint32_t blend_enable:1;
        } blend0;

        struct {
            uint32_t post_blend_clamp_enable:1;
            uint32_t pre_blend_clamp_enable:1;
            uint32_t clamp_range:2;
            uint32_t pad0:4;
            uint32_t x_dither_offset:2;
            uint32_t y_dither_offset:2;
            uint32_t dither_enable:1;
            uint32_t alpha_test_func:3;
            uint32_t alpha_test_enable:1;
            uint32_t pad1:1;
            uint32_t logic_op_func:4;
            uint32_t logic_op_enable:1;
            uint32_t pad2:1;
            uint32_t write_disable_b:1;
            uint32_t write_disable_g:1;
            uint32_t write_disable_r:1;
            uint32_t write_disable_a:1;
            uint32_t pad3:1;
            uint32_t alpha_to_coverage_dither:1;
            uint32_t alpha_to_one:1;
            uint32_t alpha_to_coverage:1;
        } blend1;
    };

    gen7_blend_state * blend = (gen7_blend_state *) ALIGN(state, 64);
    printf("blend = %p state = %p\n", blend, state);

    state = (uint8_t *)blend + sizeof(*blend);


    #define GEN7_BLENDFACTOR_ZERO 0x11
    blend->blend0.dest_blend_factor = GEN7_BLENDFACTOR_ZERO;
    #define GEN7_BLENDFACTOR_ONE 0x1
    blend->blend0.source_blend_factor = GEN7_BLENDFACTOR_ONE;
    #define GEN7_BLENDFUNCTION_ADD 0
    blend->blend0.blend_func = GEN7_BLENDFUNCTION_ADD;
    blend->blend1.post_blend_clamp_enable = 1;
    blend->blend1.pre_blend_clamp_enable = 1;

//    printf("(uint8_t *)blend - batch_buffer = %d sizeof(blend) = %d state = %p\n", (uint8_t *)blend - batch_buffer, sizeof(gen7_blend_state), state);
    *ptr++ = ((uint8_t *)blend - batch_buffer);

    #define GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC GEN7_3D(3, 0, 0x23)
    *ptr++ = (GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC | (2 - 2));

    /// *ptr++ = gen7_create_cc_viewport(batch);
    struct gen7_cc_viewport {
        float min_depth;
        float max_depth;
    };

    gen7_cc_viewport * vp = (gen7_cc_viewport *) ALIGN(state, 32);
    state = (uint8_t *)vp + sizeof(*vp);

    vp->min_depth = -1.e35;
    vp->max_depth = 1.e35;

    *ptr++ = ((uint8_t *)vp - batch_buffer);

    // ------------------------------------------------------------------------
        /// gen7_emit_sampler(batch);
        #define GEN7_3DSTATE_SAMPLER_STATE_POINTERS_PS GEN7_3D(3, 0, 0x2f)
        *ptr++ = (GEN7_3DSTATE_SAMPLER_STATE_POINTERS_PS | (2 - 2));

        /// *ptr++ = (gen7_create_sampler(batch));
        struct gen7_sampler_state {
            struct {
                unsigned int aniso_algorithm:1;
                unsigned int lod_bias:13;
                unsigned int min_filter:3;
                unsigned int mag_filter:3;
                unsigned int mip_filter:2;
                unsigned int base_level:5;
                unsigned int pad1:1;
                unsigned int lod_preclamp:1;
                unsigned int default_color_mode:1;
                unsigned int pad0:1;
                unsigned int disable:1;
            } ss0;

            struct {
                unsigned int cube_control_mode:1;
                unsigned int shadow_function:3;
                unsigned int pad:4;
                unsigned int max_lod:12;
                unsigned int min_lod:12;
            } ss1;

            struct {
                unsigned int pad:5;
                unsigned int default_color_pointer:27;
            } ss2;

            struct {
                unsigned int r_wrap_mode:3;
                unsigned int t_wrap_mode:3;
                unsigned int s_wrap_mode:3;
                unsigned int pad:1;
                unsigned int non_normalized_coord:1;
                unsigned int trilinear_quality:2;
                unsigned int address_round:6;
                unsigned int max_aniso:3;
                unsigned int chroma_key_mode:1;
                unsigned int chroma_key_index:2;
                unsigned int chroma_key_enable:1;
                unsigned int pad0:6;
            } ss3;
        };

        gen7_sampler_state * ss = (gen7_sampler_state *) ALIGN(state, 32);
        state = (uint8_t *)ss + sizeof(*ss);

        #define GEN7_MAPFILTER_NEAREST 0x0
        ss->ss0.min_filter = GEN7_MAPFILTER_NEAREST;
        ss->ss0.mag_filter = GEN7_MAPFILTER_NEAREST;

        #define GEN7_TEXCOORDMODE_CLAMP 2
        ss->ss3.r_wrap_mode = GEN7_TEXCOORDMODE_CLAMP;
        ss->ss3.s_wrap_mode = GEN7_TEXCOORDMODE_CLAMP;
        ss->ss3.t_wrap_mode = GEN7_TEXCOORDMODE_CLAMP;

        ss->ss3.non_normalized_coord = 1;

        *ptr++ = ((uint8_t *)ss - batch_buffer);

    // ------------------------------------------------------------------------
        /// gen7_emit_sbe(batch);
        #define GEN7_3DSTATE_SBE GEN7_3D(3, 0, 0x1f)
        *ptr++ = (GEN7_3DSTATE_SBE | (14 - 2));

        #define GEN7_SBE_NUM_OUTPUTS_SHIFT           22
        #define GEN7_SBE_URB_ENTRY_READ_LENGTH_SHIFT 11
        #define GEN7_SBE_URB_ENTRY_READ_OFFSET_SHIFT 4

        *ptr++ = (1 << GEN7_SBE_NUM_OUTPUTS_SHIFT |
            1 << GEN7_SBE_URB_ENTRY_READ_LENGTH_SHIFT |
            1 << GEN7_SBE_URB_ENTRY_READ_OFFSET_SHIFT);
        *ptr++ = 0;
        *ptr++ = 0; // dw4
        *ptr++ = 0;
        *ptr++ = 0;
        *ptr++ = 0;
        *ptr++ = 0; // dw8
        *ptr++ = 0;
        *ptr++ = 0;
        *ptr++ = 0;
        *ptr++ = 0; // dw12
        *ptr++ = 0;
        *ptr++ = 0;

    // ------------------------------------------------------------------------
        /// gen7_emit_ps(batch);
        #define GEN7_3DSTATE_PS GEN7_3D(3, 0, 0x20)
        *ptr++ = (GEN7_3DSTATE_PS | (8 - 2));

        // pln(16)         g113<1>F        g6<0,1,0>F      g2<8,8,1>F      { align1 WE_normal 1H };
        // pln(16)         g115<1>F        g6.4<0,1,0>F    g2<8,8,1>F      { align1 WE_normal 1H };
        // send(16)        g12<1>UW        g113<8,8,1>F
        //                 sampler (1, 0, 0, 2) mlen 4 rlen 8              { align1 WE_normal 1H };

        // mov(16)         g113<1>F        g12<8,8,1>F                     { align1 WE_normal 1H };
        // mov(16)         g115<1>F        g14<8,8,1>F                     { align1 WE_normal 1H };
        // mov(16)         g117<1>F        g16<8,8,1>F                     { align1 WE_normal 1H };
        // mov(16)         g119<1>F        g18<8,8,1>F                     { align1 WE_normal 1H };
        // send(16)        null            g113<8,8,1>F
        //                 render ( RT write, 0, 16, 12) mlen 8 rlen 0     { align1 WE_normal 1H EOT };

        static const uint32_t ps_kernel[][4] = {
            { 0x0080005a, 0x2e2077bd, 0x000000c0, 0x008d0040 },
            { 0x0080005a, 0x2e6077bd, 0x000000d0, 0x008d0040 },
            { 0x02800031, 0x21801fa9, 0x008d0e20, 0x08840001 },
            { 0x00800001, 0x2e2003bd, 0x008d0180, 0x00000000 },
            { 0x00800001, 0x2e6003bd, 0x008d01c0, 0x00000000 },
            { 0x00800001, 0x2ea003bd, 0x008d0200, 0x00000000 },
            { 0x00800001, 0x2ee003bd, 0x008d0240, 0x00000000 },
            { 0x05800031, 0x20001fa8, 0x008d0e20, 0x90031000 },
        };

        uint8_t * pk = ALIGN(state, 64);
        state = pk + sizeof(ps_kernel);
        memcpy(pk, ps_kernel, sizeof(ps_kernel));
        *ptr++ = pk - batch_buffer;

        #define GEN7_PS_SAMPLER_COUNT_SHIFT 27
        #define GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT 18
        *ptr++ = (1 << GEN7_PS_SAMPLER_COUNT_SHIFT |
                  2 << GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT);
        *ptr++ = 0; // scratch address

        #define HSW_PS_MAX_THREADS_SHIFT 23
        #define HSW_PS_SAMPLE_MASK_SHIFT 12
        int threads = (40 << HSW_PS_MAX_THREADS_SHIFT | 1 << HSW_PS_SAMPLE_MASK_SHIFT);

        #define GEN7_PS_16_DISPATCH_ENABLE (1 << 1)
        #define GEN7_PS_ATTRIBUTE_ENABLE (1 << 10)
        *ptr++ = (threads | GEN7_PS_16_DISPATCH_ENABLE | GEN7_PS_ATTRIBUTE_ENABLE);

        #define GEN7_PS_DISPATCH_START_GRF_SHIFT_0 16
        *ptr++ = (6 << GEN7_PS_DISPATCH_START_GRF_SHIFT_0);
        *ptr++ = 0;
        *ptr++ = 0;

    // ------------------------------------------------------------------------
        /// gen7_emit_vertex_elements(batch);
        #define GEN7_3DSTATE_VERTEX_ELEMENTS GEN7_3D(3, 0, 9)
        *ptr++ = (GEN7_3DSTATE_VERTEX_ELEMENTS |
            ((2 * (1 + 2)) + 1 - 2));

        #define GEN7_VE0_VERTEX_BUFFER_INDEX_SHIFT 26
        #define GEN7_VE0_VALID (1 << 25)
        #define GEN7_SURFACEFORMAT_R32G32B32A32_FLOAT 0x000
        #define GEN7_VE0_FORMAT_SHIFT 16
        #define GEN7_VE0_OFFSET_SHIFT 0
        *ptr++ = (0 << GEN7_VE0_VERTEX_BUFFER_INDEX_SHIFT | GEN7_VE0_VALID |
            GEN7_SURFACEFORMAT_R32G32B32A32_FLOAT << GEN7_VE0_FORMAT_SHIFT |
            0 << GEN7_VE0_OFFSET_SHIFT);

        #define GEN7_VFCOMPONENT_STORE_0 2
        #define GEN7_VE1_VFCOMPONENT_0_SHIFT 28
        #define GEN7_VE1_VFCOMPONENT_1_SHIFT 24
        #define GEN7_VE1_VFCOMPONENT_2_SHIFT 20
        #define GEN7_VE1_VFCOMPONENT_3_SHIFT 16
        *ptr++ = (GEN7_VFCOMPONENT_STORE_0 << GEN7_VE1_VFCOMPONENT_0_SHIFT |
            GEN7_VFCOMPONENT_STORE_0 << GEN7_VE1_VFCOMPONENT_1_SHIFT |
            GEN7_VFCOMPONENT_STORE_0 << GEN7_VE1_VFCOMPONENT_2_SHIFT |
            GEN7_VFCOMPONENT_STORE_0 << GEN7_VE1_VFCOMPONENT_3_SHIFT);

        // x,y
        #define GEN7_VE0_VERTEX_BUFFER_INDEX_SHIFT 26
        #define GEN7_SURFACEFORMAT_R16G16_SSCALED 0x0F6
        *ptr++ = (0 << GEN7_VE0_VERTEX_BUFFER_INDEX_SHIFT | GEN7_VE0_VALID |
            GEN7_SURFACEFORMAT_R16G16_SSCALED << GEN7_VE0_FORMAT_SHIFT |
            0 << GEN7_VE0_OFFSET_SHIFT); // offsets vb in bytes

        #define GEN7_VFCOMPONENT_STORE_SRC    1
        #define GEN7_VFCOMPONENT_STORE_1_FLT  3
        *ptr++ = (GEN7_VFCOMPONENT_STORE_SRC << GEN7_VE1_VFCOMPONENT_0_SHIFT |
            GEN7_VFCOMPONENT_STORE_SRC << GEN7_VE1_VFCOMPONENT_1_SHIFT |
            GEN7_VFCOMPONENT_STORE_0 << GEN7_VE1_VFCOMPONENT_2_SHIFT |
            GEN7_VFCOMPONENT_STORE_1_FLT << GEN7_VE1_VFCOMPONENT_3_SHIFT);

        // s,t
        *ptr++ = (0 << GEN7_VE0_VERTEX_BUFFER_INDEX_SHIFT | GEN7_VE0_VALID |
            GEN7_SURFACEFORMAT_R16G16_SSCALED << GEN7_VE0_FORMAT_SHIFT |
            4 << GEN7_VE0_OFFSET_SHIFT);  // offset vb in bytes

        *ptr++ = (GEN7_VFCOMPONENT_STORE_SRC << GEN7_VE1_VFCOMPONENT_0_SHIFT |
            GEN7_VFCOMPONENT_STORE_SRC << GEN7_VE1_VFCOMPONENT_1_SHIFT |
            GEN7_VFCOMPONENT_STORE_0 << GEN7_VE1_VFCOMPONENT_2_SHIFT |
            GEN7_VFCOMPONENT_STORE_1_FLT << GEN7_VE1_VFCOMPONENT_3_SHIFT);

    // ------------------------------------------------------------------------
        /// gen7_emit_vertex_buffer(batch, src_x, src_y, dst_x, dst_y, width, height);
        #define GEN7_3DSTATE_VERTEX_BUFFERS GEN7_3D(3, 0, 8)
        *ptr++ = (GEN7_3DSTATE_VERTEX_BUFFERS | (5 - 2));

        #define GEN7_VB0_BUFFER_INDEX_SHIFT 26
        #define GEN7_VB0_VERTEXDATA (0 << 20)
        #define GEN7_VB0_ADDRESS_MODIFY_ENABLE (1 << 14)
        #define GEN7_VB0_BUFFER_PITCH_SHIFT 0
        *ptr++ = (0 << GEN7_VB0_BUFFER_INDEX_SHIFT | GEN7_VB0_VERTEXDATA | GEN7_VB0_ADDRESS_MODIFY_ENABLE | 4 * 2 << GEN7_VB0_BUFFER_PITCH_SHIFT);

        /// offset = gen7_create_vertex_buffer(batch, src_x, src_y, dst_x, dst_y, width, height);
        uint16_t * v = (uint16_t *) ALIGN(state, 8);
        state = (uint8_t *)v + 12 * sizeof(*v);

        uint16_t src_x = 0;
        uint16_t src_y = 0;
        uint16_t dst_x = WIDTH / 2;
        uint16_t dst_y = HEIGHT / 2;

        v[0] = dst_x + WIDTH;
        v[1] = dst_y + HEIGHT;
        v[2] = src_x + WIDTH;
        v[3] = src_y + HEIGHT;

        v[4] = dst_x;
        v[5] = dst_y + HEIGHT;
        v[6] = src_x;
        v[7] = src_y + HEIGHT;

        v[8] = dst_x;
        v[9] = dst_y;
        v[10] = src_x;
        v[11] = src_y;

        uint32_t offset = (uint8_t *)v - batch_buffer;

        /// OUT_RELOC(batch->bo, I915_GEM_DOMAIN_VERTEX, 0, offset);
        ret = drm_intel_bo_emit_reloc(batch_bo, (uint8_t *)ptr - (uint8_t *)batch_buffer, batch_bo, offset, I915_GEM_DOMAIN_VERTEX, 0);
        if (ret != 0) {
            printf("error while drm_intel_bo_emit_reloc(I915_GEM_DOMAIN_VERTEX): %d:%s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
        *ptr++ = batch_bo->offset64 + offset;

        *ptr++ = ~0;
        *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_binding_table(batch, src, dst);
    #define GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS GEN7_3D(3, 0, 0x2a)
    *ptr++ = (GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS | (2 - 2));

    /// *ptr++ = (gen7_bind_surfaces(batch, src, dst));
    uint32_t * binding_table = (uint32_t *) ALIGN(state, 32);
    state = (uint8_t *)binding_table + 8;

    #define GEN7_SURFACEFORMAT_B8G8R8A8_UNORM 0x0C0
    /// binding_table[0] = gen7_bind_buf(batch, dst, GEN7_SURFACEFORMAT_B8G8R8A8_UNORM, 1);
    {
        uint32_t * ss = (uint32_t *) ALIGN(state, 32);
        state = (uint8_t *)ss + 8 * sizeof(*ss);

        #define GEN7_SURFACE_2D 1
        #define GEN7_SURFACE_TYPE_SHIFT 29
        #define GEN7_SURFACE_FORMAT_SHIFT 18
        ss[0] = (GEN7_SURFACE_2D << GEN7_SURFACE_TYPE_SHIFT | 0 | GEN7_SURFACEFORMAT_B8G8R8A8_UNORM << GEN7_SURFACE_FORMAT_SHIFT); // I915_TILING_NONE

        ss[1] = dst_bo->offset;

        // Surface state DW2
        #define GEN7_SURFACE_HEIGHT_SHIFT        16
        #define GEN7_SURFACE_WIDTH_SHIFT         0
        ss[2] = ((STRIDE / sizeof(uint32_t) - 1)  << GEN7_SURFACE_WIDTH_SHIFT | (HEIGHT - 1) << GEN7_SURFACE_HEIGHT_SHIFT);

        #define GEN7_SURFACE_PITCH_SHIFT         0
        ss[3] = (STRIDE - 1) << GEN7_SURFACE_PITCH_SHIFT;
        ss[4] = 0;
        ss[5] = 0;
        ss[6] = 0;

        #define HSW_SWIZZLE_ZERO  0
        #define HSW_SWIZZLE_ONE   1
        #define HSW_SWIZZLE_RED   4
        #define HSW_SWIZZLE_GREEN 5
        #define HSW_SWIZZLE_BLUE  6
        #define HSW_SWIZZLE_ALPHA 7
        #define __HSW_SURFACE_SWIZZLE(r,g,b,a) ((a) << 16 | (b) << 19 | (g) << 22 | (r) << 25)
        #define HSW_SURFACE_SWIZZLE(r,g,b,a) __HSW_SURFACE_SWIZZLE(HSW_SWIZZLE_##r, HSW_SWIZZLE_##g, HSW_SWIZZLE_##b, HSW_SWIZZLE_##a)
        ss[7] = HSW_SURFACE_SWIZZLE(RED, GREEN, BLUE, ALPHA);

        ret = drm_intel_bo_emit_reloc(batch_bo, (uint8_t *)ss - batch_buffer + 4, dst_bo, 0, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);
        if (ret != 0) {
            printf("error while drm_intel_bo_emit_reloc(I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER): %d:%s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        binding_table[0] = (uint8_t *)ss - batch_buffer;
    }

    /// binding_table[1] = gen7_bind_buf(batch, src, GEN7_SURFACEFORMAT_B8G8R8A8_UNORM, 0);
    {
        uint32_t * ss = (uint32_t *) ALIGN(state, 32);
        state = (uint8_t *)ss + 8 * sizeof(*ss);

        ss[0] = (GEN7_SURFACE_2D << GEN7_SURFACE_TYPE_SHIFT | 0 | GEN7_SURFACEFORMAT_B8G8R8A8_UNORM << GEN7_SURFACE_FORMAT_SHIFT); // I915_TILING_NONE
        ss[1] = src_bo->offset;
        ss[2] = ((STRIDE / sizeof(uint32_t) - 1)  << GEN7_SURFACE_WIDTH_SHIFT | (HEIGHT - 1) << GEN7_SURFACE_HEIGHT_SHIFT);
        ss[3] = (STRIDE - 1) << GEN7_SURFACE_PITCH_SHIFT;
        ss[4] = 0;
        ss[5] = 0;
        ss[6] = 0;
        ss[7] = HSW_SURFACE_SWIZZLE(RED, GREEN, BLUE, ALPHA);

        ret = drm_intel_bo_emit_reloc(batch_bo, (uint8_t *)ss - batch_buffer + 4, src_bo, 0, I915_GEM_DOMAIN_SAMPLER, 0);
        if (ret != 0) {
            printf("error while drm_intel_bo_emit_reloc(I915_GEM_DOMAIN_SAMPLER, 0): %d:%s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        binding_table[1] = (uint8_t *)ss - batch_buffer;
    }

    *ptr++ = (uint8_t *) binding_table - batch_buffer;

    // ------------------------------------------------------------------------
    /// gen7_emit_drawing_rectangle(batch, dst);
        #define GEN7_3DSTATE_DRAWING_RECTANGLE GEN7_3D(3, 1, 0)
        *ptr++ = (GEN7_3DSTATE_DRAWING_RECTANGLE | (4 - 2));
        *ptr++ = 0;
        *ptr++ = ((HEIGHT - 1) << 16 | (STRIDE / sizeof(uint32_t) - 1));
        *ptr++ = 0;

    // ------------------------------------------------------------------------
        #define GEN7_3DPRIMITIVE GEN7_3D(3, 3, 0)
        *ptr++ = (GEN7_3DPRIMITIVE | (7- 2));

        #define GEN7_3DPRIMITIVE_VERTEX_SEQUENTIAL (0 << 15)
        #define _3DPRIM_RECTLIST 0x0F
        *ptr++ = (GEN7_3DPRIMITIVE_VERTEX_SEQUENTIAL | _3DPRIM_RECTLIST);

        *ptr++ = 3;
        *ptr++ = 0;
        *ptr++ = 1;   // single instance
        *ptr++ = 0;   // start instance location
        *ptr++ = 0;   // index buffer offset, ignored

    #define MI_BATCH_BUFFER_END (0xA << 23)
    *ptr++ = MI_BATCH_BUFFER_END;

    // ------------------------------------------------------------------------
    /// gen7_render_flush(batch, context, batch_end);

    ret = drm_intel_bo_subdata(batch_bo, 0, sizeof(batch_buffer), batch_buffer);
    if (ret != 0) {
        printf("drm_intel_bo_subdata failed ret: %d:%s\n", ret, strerror(-ret));
        exit(EXIT_FAILURE);
    }

    uint32_t batch_end = (uint32_t)(uintptr_t) ALIGN((void *)((uint8_t *)ptr - batch_buffer), 8);
    ret = drm_intel_bo_mrb_exec(batch_bo, batch_end, NULL, 0, 0, 0);

    printf("drm_intel_bo_mrb_exec batch_end = %p\n", batch_end);

    if (ret != 0) {
        printf("drm_intel_bo_mrb_exec failed ret: %d:%s\n", ret, strerror(-ret));
        exit(EXIT_FAILURE);
    }

  	drm_intel_bo_unreference(batch_bo);

    // ------------------------------------------------------------------------
    /// gem_read(data->drm_fd, buf->bo->handle, 0, data->linear, sizeof(data->linear));
    struct drm_i915_gem_pread gem_pread = { 0 };

    uint32_t linear_out[WIDTH * HEIGHT] = { 0 };

    gem_pread.handle = dst_bo->handle;
    gem_pread.data_ptr = (uintptr_t)linear_out;
    gem_pread.size = sizeof(linear_out);
    ret = ioctl(dri_fd, DRM_IOCTL_I915_GEM_PREAD, &gem_pread);
    if (ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GEM_PREAD, dst_bo->handle): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // or:
	// drm_intel_bo_map(dst_bo, 0);

    printf("DST_COLOR = 0x%x linear_out[%d, %d] = 0x%x\n", DST_COLOR, 0, 0, linear_out[0]);
    printf("DST_COLOR = 0x%x linear_out[%d, %d] = 0x%x\n", DST_COLOR, WIDTH / 2, HEIGHT / 2, linear_out[WIDTH * HEIGHT / 2 + WIDTH / 2]);
    printf("DST_COLOR = 0x%x linear_out[%d, %d] = 0x%x\n", DST_COLOR, WIDTH, HEIGHT, linear_out[WIDTH * HEIGHT - 1]);

    printf("write to out.ppm\n");
    write_ppm("out.ppm", WIDTH, HEIGHT, linear_out);

    // write_ppm("out.ppm", WIDTH, HEIGHT, (uint32_t *)dst_bo->virt);
  	drm_intel_bo_unmap(dst_bo);

    return 0;
}
