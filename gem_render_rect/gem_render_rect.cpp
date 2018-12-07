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

#define WIDTH 1920
#define STRIDE (WIDTH * 4)
#define HEIGHT 1080

#define DST_COLOR	0xffff0000

// #define ALIGN(v, a) (((v) + (a)-1) & ~((a)-1)) // alignment unit in bytes

uint8_t * ALIGN(void * pointer, uintptr_t alignment) {
    uint8_t * p = (uint8_t *) pointer + alignment - 1;
    return (uint8_t *)(((uintptr_t) p) & -alignment);
}

void write_ppm(const char * file, uint32_t width, uint32_t height, const uint32_t * argb) {
    printf("write_ppm file: %s width = %d height = %d, argb = %p\n", file, width, height, argb);
    FILE * fp = fopen(file, "wb");
    if(fp == NULL) {
        printf("error while open file '%s': %d:%s\n", file, errno, strerror(errno));
        return;
    }

    fprintf(fp, "P3\n%d %d\n255\n", width, height);
    for(int y = 0 ; y < height; y++) {
        for(int x = 0 ; x < width; x++) {
            uint8_t red = (argb[y * width + x] >> 16) & 0x000000ff;
            uint8_t green = (argb[y * width + x] >> 8) & 0x000000ff;
            uint8_t blue = (argb[y * width + x] >> 0) & 0x000000ff;
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


    drm_intel_bo * dst_bo = drm_intel_bo_alloc(bufmgr, "dst_bo", HEIGHT * STRIDE, 4096);
    printf("drm_intel_bo_alloc(dst_bo, HEIGHT * STRIDE = %d): dst_bo = %p\n", HEIGHT * STRIDE, dst_bo);

    const int linear_size = WIDTH * HEIGHT * sizeof(uint32_t);
    uint32_t * linear = (uint32_t *)ALIGN(malloc(linear_size + 4096), 4096);
    memset(linear, 0, linear_size);

    for (int i = 0; i < WIDTH * HEIGHT; i++)
        linear[i] = DST_COLOR;

    /// gem_write(drm_fd, dst_bo->handle, 0, linear, sizeof(linear));
    struct drm_i915_gem_pwrite gem_pwrite_dst = { 0 };
    gem_pwrite_dst.handle = dst_bo->handle;
    gem_pwrite_dst.size = linear_size;
    gem_pwrite_dst.data_ptr = (uintptr_t)linear;
    ret = ioctl(dri_fd, DRM_IOCTL_I915_GEM_PWRITE, &gem_pwrite_dst);
    if (ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GEM_PWRITE, dst_bo->handle): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("ioctl(DRM_IOCTL_I915_GEM_PWRITE, dst_bo->handle): filled whith 0x%x \n", DST_COLOR);

    __attribute__((aligned(4096))) uint8_t batch_buffer[BATCH_SZ] = { 0 };
    uint32_t * ptr = (uint32_t *)batch_buffer;
    uint8_t * state = batch_buffer + sizeof(batch_buffer) / 2;

    printf("batch_buffer = %p state = %p end = %p\n", batch_buffer, state, batch_buffer + sizeof(batch_buffer));

    #define GEN7_3D(Pipeline,Opcode,Subopcode) ((3 << 29) | \
                                               ((Pipeline) << 27) | \
                                               ((Opcode) << 24) | \
                                               ((Subopcode) << 16))

#define GEN7_PIPE_CONTROL			GEN7_3D(3, 2, 0)
#define GEN7_PIPE_CONTROL_CS_STALL        (1 << 20)
#define GEN7_PIPE_CONTROL_NOWRITE         (0 << 14)
#define GEN7_PIPE_CONTROL_WRITE_QWORD     (1 << 14)
#define GEN7_PIPE_CONTROL_WRITE_DEPTH     (2 << 14)
#define GEN7_PIPE_CONTROL_WRITE_TIME      (3 << 14)
#define GEN7_PIPE_CONTROL_DEPTH_STALL     (1 << 13)
#define GEN7_PIPE_CONTROL_RT_FLUSH        (1 << 12)
#define GEN7_PIPE_CONTROL_ICI_ENABLE      (1 << 11)
#define GEN7_PIPE_CONTROL_TCI_ENABLE      (1 << 10)
#define GEN7_PIPE_CONTROL_NOTIFY_ENABLE   (1 << 8)
#define GEN7_PIPE_CONTROL_DC_FLUSH        (1 << 5)
#define GEN7_PIPE_CONTROL_CCI_ENABLE      (1 << 3)
#define GEN7_PIPE_CONTROL_SCI_ENABLE      (1 << 2)
#define GEN7_PIPE_CONTROL_STALL_AT_SCOREBOARD   (1 << 1)
#define GEN7_PIPE_CONTROL_DEPTH_CACHE_FLUSH	(1 << 0)

    *ptr++ = (GEN7_PIPE_CONTROL | 3);
    *ptr++ = GEN7_PIPE_CONTROL_DEPTH_CACHE_FLUSH | GEN7_PIPE_CONTROL_DC_FLUSH | GEN7_PIPE_CONTROL_DEPTH_STALL | GEN7_PIPE_CONTROL_CS_STALL;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    *ptr++ = (GEN7_PIPE_CONTROL | 3);
    *ptr++ = GEN7_PIPE_CONTROL_SCI_ENABLE | GEN7_PIPE_CONTROL_CCI_ENABLE | GEN7_PIPE_CONTROL_TCI_ENABLE | GEN7_PIPE_CONTROL_ICI_ENABLE;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_PIPELINE_SELECT GEN7_3D(1, 1, 4)
    #define PIPELINE_SELECT_3D 0
    *ptr++ = (GEN7_PIPELINE_SELECT | PIPELINE_SELECT_3D);

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

    #define GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC GEN7_3D(3, 0, 0x23)
    *ptr++ = (GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC | (2 - 2));

    /// *ptr++ = gen7_create_cc_viewport(batch);
    struct gen7_cc_viewport {
        float min_depth;
        float max_depth;
    };

    struct gen7_cc_viewport * vp = (struct gen7_cc_viewport *) ALIGN(state, 32);
    state = (uint8_t *)vp + sizeof(*vp);

    vp->min_depth = 0.0;
    vp->max_depth = 1.0;

    *ptr++ = ((uint8_t *)vp - batch_buffer);

    // 3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP
    #define GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP GEN7_3D(3, 0, 0x21)
    *ptr++ = GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP | (2 - 2);

	// viewport.sf_clip_state = gen7_create_sf_clip_viewport(batch, &aub_annotations);
    struct gen7_sf_clip_viewport {
        struct {
            float m00;
            float m11;
            float m22;
            float m30;
            float m31;
            float m32;
        } viewport;

        uint32_t pad0[2];

        struct {
            float xmin;
            float xmax;
            float ymin;
            float ymax;
        } guardband;

        float pad1[4];
    };

	struct gen7_sf_clip_viewport * scv_state = (struct gen7_sf_clip_viewport *) ALIGN(state, 64);

    state = (uint8_t *) scv_state + sizeof(*scv_state);

    scv_state->viewport.m00 = WIDTH / (1.0 - (-1.0));
    scv_state->viewport.m11 = -1.0 * HEIGHT / (1 - (-1));
    scv_state->viewport.m22 = 0;
    scv_state->viewport.m30 = WIDTH / 2.0;
    scv_state->viewport.m31 = HEIGHT / 2.0;
    scv_state->viewport.m32 = 0;

	scv_state->guardband.xmin = -17.066668;//-1.0;
	scv_state->guardband.xmax = 17.066668;// 1.0;
	scv_state->guardband.ymin = -30.340740;//-1.0;
	scv_state->guardband.ymax =  30.340740;//1.0;

    *ptr++ = ((uint8_t *)scv_state - batch_buffer);

    // gen7_emit_urb(batch);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_VS             GEN7_3D(3, 1, 0x12)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_VS | (2 - 2));
    *ptr++ = 8; // in 1KBs

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET 16

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_HS	GEN7_3D(3, 1, 0x13)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_HS | (2 - 2));
    *ptr++ = (8 << GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_DS	GEN7_3D(3, 1, 0x14)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_DS | (2 - 2));
    *ptr++ = (8 << GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_GS	GEN7_3D(3, 1, 0x15)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_GS | (2 - 2));
    *ptr++ = (8 << GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET);

    #define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS GEN7_3D(3, 1, 0x16)
    *ptr++ = (GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS | (2 - 2));
    *ptr++ = (8 << GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_CONSTANT_BUFFER_OFFSET) | 8; // in 1KBs

    // num of VS entries must be divisible by 8 if size < 9
    #define GEN7_3DSTATE_URB_VS GEN7_3D(3, 0, 0x30)
    *ptr++ = (GEN7_3DSTATE_URB_VS | (2 - 2));

    #define GEN7_URB_ENTRY_NUMBER_SHIFT     0
    #define GEN7_URB_ENTRY_SIZE_SHIFT       16
    #define GEN7_URB_STARTING_ADDRESS_SHIFT 25
    *ptr++ = ((1664    << GEN7_URB_ENTRY_NUMBER_SHIFT) |
              (1 - 1)  << GEN7_URB_ENTRY_SIZE_SHIFT    |
              (2       << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_HS GEN7_3D(3, 0, 0x31)
    *ptr++ = (GEN7_3DSTATE_URB_HS | (2 - 2));
    *ptr++ =((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (15 << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_DS GEN7_3D(3, 0, 0x32)
    *ptr++ = (GEN7_3DSTATE_URB_DS | (2 - 2));
    *ptr++ = ((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (15 << GEN7_URB_STARTING_ADDRESS_SHIFT));

    #define GEN7_3DSTATE_URB_GS GEN7_3D(3, 0, 0x33)
    *ptr++ = (GEN7_3DSTATE_URB_GS | (2 - 2));
    *ptr++ = ((0 << GEN7_URB_ENTRY_SIZE_SHIFT) | (15 << GEN7_URB_STARTING_ADDRESS_SHIFT));

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

    struct gen7_blend_state * blend = (struct gen7_blend_state *) ALIGN(state, 64);
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


    // gen6_create_cc_state(struct intel_batchbuffer *batch, struct annotations_context *aub)
    #define GEN7_3DSTATE_CC_STATE_POINTERS GEN7_3D(3, 0, 0x0e)
    *ptr++ = (GEN7_3DSTATE_CC_STATE_POINTERS | (2 - 2));

    struct gen7_color_calc_state {
        struct {
            uint32_t alpha_test_format:1;
            uint32_t pad0:14;
            uint32_t round_disable:1;
            uint32_t bf_stencil_ref:8;
            uint32_t stencil_ref:8;
        } cc0;

        union {
            float alpha_ref_f;
            struct {
                uint32_t ui:8;
                uint32_t pad0:24;
            } alpha_ref_fi;
        } cc1;

        float constant_r;
        float constant_g;
        float constant_b;
        float constant_a;
    };

  	struct gen7_color_calc_state * cc_state = (struct gen7_color_calc_state *) ALIGN(state, 64);
    state = (uint8_t *)cc_state + sizeof(*cc_state);

    *ptr++ = ((uint8_t *)cc_state - batch_buffer) | 1;

    // ------------------------------------------------------------------------

    #define GEN7_3DSTATE_DEPTH_STENCIL_STATE_POINTERS       GEN7_3D(3, 0, 0x25)
    *ptr++ = GEN7_3DSTATE_DEPTH_STENCIL_STATE_POINTERS;

    struct gen7_depth_stencil_state {
        struct {
            uint32_t pad0:3;
            uint32_t backface_depth_pass_op:3;
            uint32_t backface_pass_depth_fail_op:3;
            uint32_t backface_stencil_fail_op:3;
            uint32_t backface_stencil_test_func:3;
            uint32_t double_sided_stencil_enabled:1;
            uint32_t pad1:2;
            uint32_t stencil_buffer_write_enabled:1;
            uint32_t depth_pass_op:3;
            uint32_t depth_fail_op:3;
            uint32_t stencil_fail_op:3;
            uint32_t stencil_test_func:3;
            uint32_t stencil_test_enabled:1;
        } ds0;

        struct {
            uint32_t backface_stencil_write_mask:8;
            uint32_t backface_stencil_test_mask:8;
            uint32_t stencil_write_mask:8;
            uint32_t stencil_test_mask:8;
        } ds1;

        struct {
            uint32_t pad0:26;
            uint32_t depth_buffer_write_enabled:1;
            uint32_t depth_test_function:3;
            uint32_t pad1:1;
            uint32_t depth_test_enable:1;
        } ds2;
    };

    struct gen7_depth_stencil_state * depth_stencil_state = (struct gen7_depth_stencil_state *) ALIGN(state, 64);
    state = (uint8_t *)depth_stencil_state + sizeof(*depth_stencil_state);

    *ptr++ = ((uint8_t *)depth_stencil_state - batch_buffer);

    // ------------------------------------------------------------------------

    #define GEN7_3DSTATE_CONSTANT_VS		GEN7_3D(3, 0, 0x15)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_VS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_3DSTATE_CONSTANT_HS                GEN7_3D(3, 0, 0x19)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_HS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_3DSTATE_CONSTANT_DS                GEN7_3D(3, 0, 0x1a)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_DS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_3DSTATE_CONSTANT_GS		GEN7_3D(3, 0, 0x16)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_GS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    #define GEN7_3DSTATE_CONSTANT_PS		GEN7_3D(3, 0, 0x17)
    *ptr++ = (GEN7_3DSTATE_CONSTANT_PS | (7 - 2));
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;
    *ptr++ = 0x0;

    // ------------------------------------------------------------------------
    /// gen7_emit_binding_table(batch, src, dst);
    #define GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS GEN7_3D(3, 0, 0x2a)
    *ptr++ = (GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS | (2 - 2));

    /// *ptr++ = (gen7_bind_surfaces(batch, src, dst));
    uint32_t * binding_table = (uint32_t *) ALIGN(state, 32);
    state = (uint8_t *)binding_table + 4;

    #define GEN7_SURFACEFORMAT_B8G8R8A8_UNORM 0x0C0
    /// binding_table[0] = gen7_bind_buf(batch, dst, GEN7_SURFACEFORMAT_B8G8R8A8_UNORM, 1);
    {
        uint32_t * ss = (uint32_t *) ALIGN(state, 32);
        state = (uint8_t *)ss + 8 * sizeof(*ss);

        #define GEN7_SURFACE_2D 1
        #define GEN7_SURFACE_TYPE_SHIFT 29
        #define GEN7_SURFACE_FORMAT_SHIFT 18
        ss[0] = (GEN7_SURFACE_2D << GEN7_SURFACE_TYPE_SHIFT | 0 | GEN7_SURFACEFORMAT_B8G8R8A8_UNORM << GEN7_SURFACE_FORMAT_SHIFT); // I915_TILING_NONE

        //ss[0] = 0x3300443f;

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

    *ptr++ = (uint8_t *) binding_table - batch_buffer;

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

    // ------------------------------------------------------------------------
    /// gen7_emit_vs(batch);
    #define GEN7_3DSTATE_VS GEN7_3D(3, 0, 0x10)
    *ptr++ = (GEN7_3DSTATE_VS | (6 - 2));

    //  mov(8)          g116<1>F        [0F, 1F, 0F, 1F]VF              { align16 1Q };
    //  mov(8)          g114<1>UD       0x00000000UD                    { align16 1Q compacted };
    //  mov(8)          g115<1>F        g1<4>F                          { align16 1Q };
    //  mov(8)          g113<1>UD       g0<4>UD                         { align16 WE_all 1Q };
    //  or(1)           g113.5<1>UD     g0.5<0,1,0>UD   0x0000ff00UD    { align1 WE_all 1N };
    //  send(8)         null<1>F        g113<4>F
    //  urb 0 write HWord interleave complete mlen 5 rlen 0 { align16 1Q EOT };

    static const uint32_t vs_kernel[] = {
        0x00600101, 0x2e8f52fd, 0x00000000, 0x30003000,
        0x200c6c01, 0x00007200, 0x00600101, 0x2e6f03bd,
        0x006e0024, 0x00000000, 0x00600301, 0x2e2f0021,
        0x006e0004, 0x00000000, 0x00000206, 0x2e340c21,
        0x00000014, 0x0000ff00, 0x06600131, 0x200f1fbc,
        0x006e0e24, 0x8a08c000
    };

    uint8_t * vk = ALIGN(state, 64);
    state = vk + sizeof(vs_kernel);
    memcpy(vk, vs_kernel, sizeof(vs_kernel));
    *ptr++ = vk - batch_buffer;

    #define GEN7_VS_SAMPLER_COUNT_SHIFT 27
    #define GEN7_VS_BINDING_TABLE_ENTRY_COUNT_SHIFT 18
    *ptr++ = (0 << GEN7_VS_SAMPLER_COUNT_SHIFT | 0 << GEN7_VS_BINDING_TABLE_ENTRY_COUNT_SHIFT);
    *ptr++ = 0; // scratch address

    #define GEN7_VS_URB_ENTRY_READ_OFFSET_SHIFT 4
    #define GEN7_VS_URB_ENTRY_READ_LENGTH_SHIFT 11
    #define GEN7_VS_DISPATCH_GRF_START_REGISTER_FOR_URB_SHIFT 20
    *ptr++ = (0 << GEN7_VS_URB_ENTRY_READ_OFFSET_SHIFT) | (1 << GEN7_VS_URB_ENTRY_READ_LENGTH_SHIFT) | (1 << GEN7_VS_DISPATCH_GRF_START_REGISTER_FOR_URB_SHIFT);

    #define GEN7_VS_FUNCTION_ENABLE_SHIFT 0
    #define GEN7_VS_VERTEX_CACHE_DISABLE_SHIFT 1
    #define GEN7_VS_STATISTICS_ENABLE_SHIFT 10
    #define HSW_VS_MAX_THREADS_SHIFT 23

    *ptr++ = (1 << GEN7_VS_FUNCTION_ENABLE_SHIFT | 0 << GEN7_VS_VERTEX_CACHE_DISABLE_SHIFT  | 1 << GEN7_VS_STATISTICS_ENABLE_SHIFT | 279 << HSW_VS_MAX_THREADS_SHIFT);

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

    #define GEN7_GS_DISPATCH_GRF_START_REGISTER_FOR_URB_DATA_SHIFT 0
    #define GEN7_GS_VERTEX_URB_ENTRY_READ_OFFSET_SHIFT 4
    #define GEN7_GS_INCLUDE_VERTEXT_HANDLES_SHIFT 10
    #define GEN7_GS_VERTEX_URB_ENTRY_READ_LENGTH_SHIFT 11
    *ptr++ = (1 << GEN7_GS_DISPATCH_GRF_START_REGISTER_FOR_URB_DATA_SHIFT) | (0  << GEN7_GS_VERTEX_URB_ENTRY_READ_OFFSET_SHIFT) | (1 << GEN7_GS_INCLUDE_VERTEXT_HANDLES_SHIFT) | (0 << GEN7_GS_VERTEX_URB_ENTRY_READ_LENGTH_SHIFT);

    #define GEN7_GS_STATISTICS_ENABLED_SHIFT 10
    *ptr++ = (1 << GEN7_GS_STATISTICS_ENABLED_SHIFT);
    *ptr++ = 0; // pass-through

    // ------------------------------------------------------------------------
    #define GEN7_3DSTATE_STREAMOUT			GEN7_3D(3, 0, 0x1e)
    *ptr++ = (GEN7_3DSTATE_STREAMOUT | (3 - 2));
    *ptr++ = 0;
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_clip(batch);
    #define GEN7_3DSTATE_CLIP GEN7_3D(3, 0, 0x12)
    *ptr++ = (GEN7_3DSTATE_CLIP | (4 - 2));
    *ptr++ = 0x00150400;
    *ptr++ = 0x9c000026;
    *ptr++ = 0x0003ffe0;


    // ------------------------------------------------------------------------
    /// gen7_emit_sbe(batch);
    #define GEN7_3DSTATE_SBE GEN7_3D(3, 0, 0x1f)
    *ptr++ = (GEN7_3DSTATE_SBE | (14 - 2));

    #define GEN7_SBE_NUM_OUTPUTS_SHIFT           22
    #define GEN7_SBE_ATTR_SWIZZLE_ENABLE_SHIFT   21
    #define GEN7_SBE_URB_ENTRY_READ_LENGTH_SHIFT 11
    #define GEN7_SBE_URB_ENTRY_READ_OFFSET_SHIFT 4

    *ptr++ = (1 << GEN7_SBE_ATTR_SWIZZLE_ENABLE_SHIFT | 1 << GEN7_SBE_NUM_OUTPUTS_SHIFT | 1 << GEN7_SBE_URB_ENTRY_READ_LENGTH_SHIFT | 1 << GEN7_SBE_URB_ENTRY_READ_OFFSET_SHIFT);
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
    /// gen7_emit_sf(batch);
    #define GEN7_3DSTATE_SF GEN7_3D(3, 0, 0x13)
    *ptr++ = (GEN7_3DSTATE_SF | (7 - 2));
    *ptr++ = 0x00001403;

    #define GEN7_3DSTATE_SF_CULL_NONE (1 << 29)
    *ptr++ = 0x22000800;

    #define GEN7_3DSTATE_SF_TRIFAN_PROVOKE_SHIFT 25
    *ptr++ = 0x4c004808;
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
        /// gen7_emit_ps(batch);
    #define GEN7_3DSTATE_PS GEN7_3D(3, 0, 0x20)
    *ptr++ = (GEN7_3DSTATE_PS | (8 - 2));

    // mesa/src/intel/tools/gen_batch_decoder.c: decode_ps_kernels()

    // Reorder KSPs to be [8, 16, 32] instead of the hardware order
    // if (enabled[0] + enabled[1] + enabled[2] == 1) {
    //     if (enabled[1]) {
    //         ksp[1] = ksp[0];
    //         ksp[0] = 0;
    //     } else if (enabled[2]) {
    //         ksp[2] = ksp[0];
    //         ksp[0] = 0;
    //     }
    // } else {
    //     uint64_t tmp = ksp[1];
    //     ksp[1] = ksp[2];
    //     ksp[2] = tmp;
    // }
    // we have 8 and 16 ps shaders enabled, so their offsets shoud go to kernel[0] and kernel[2]

    // pln(8)          g124<1>F        g4<0,1,0>F      g2<8,8,1>F      { align1 1Q compacted };
    // pln(8)          g125<1>F        g4.4<0,1,0>F    g2<8,8,1>F      { align1 1Q compacted };
    // pln(8)          g126<1>F        g5<0,1,0>F      g2<8,8,1>F      { align1 1Q compacted };
    // pln(8)          g127<1>F        g5.4<0,1,0>F    g2<8,8,1>F      { align1 1Q compacted };
    // sendc(8)        null<1>UW       g124<8,8,1>F
    //                       render RT write SIMD8 LastRT Surface = 0 mlen 4 rlen 0 { align1 1Q EOT };

    static const uint32_t ps_kernel_8[] = {
        0x20024b5a, 0x02047ce0, 0x20224b5a, 0x02047de0,
        0x20024b5a, 0x02057ee0, 0x20224b5a, 0x02057fe0,
        0x05600032, 0x20001fa8, 0x008d0f80, 0x88031400,
    };

    uint8_t * pk8 = ALIGN(state, 64);
    state = pk8 + sizeof(ps_kernel_8);
    memcpy(pk8, ps_kernel_8, sizeof(ps_kernel_8));

    // pln(16)         g120<1>F        g6<0,1,0>F      g2<8,8,1>F      { align1 1H compacted };
    // pln(16)         g122<1>F        g6.4<0,1,0>F    g2<8,8,1>F      { align1 1H compacted };
    // pln(16)         g124<1>F        g7<0,1,0>F      g2<8,8,1>F      { align1 1H compacted };
    // pln(16)         g126<1>F        g7.4<0,1,0>F    g2<8,8,1>F      { align1 1H compacted };
    // sendc(16)       null<1>UW       g120<8,8,1>F
    //     render RT write SIMD16 LastRT Surface = 0 mlen 8 rlen 0 { align1 1H EOT };

    static const uint32_t ps_kernel_16[] = {
        0x2002565a, 0x020678e0, 0x2022565a, 0x02067ae0,
        0x2002565a, 0x02077ce0, 0x2022565a, 0x02077ee0,
        0x05800032, 0x20001fa8, 0x008d0f00, 0x90031000,
    };

    uint8_t * pk16 = ALIGN(state, 64);
    state = pk16 + sizeof(ps_kernel_16);
    memcpy(pk16, ps_kernel_16, sizeof(ps_kernel_16));

    *ptr++ = pk8 - batch_buffer; // kernel[0]

    #define GEN7_PS_SAMPLER_COUNT_SHIFT 27
    #define GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT 18
    *ptr++ = (0 << GEN7_PS_SAMPLER_COUNT_SHIFT | 1 << GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT);
    *ptr++ = 0; // scratch address

    #define HSW_PS_MAX_THREADS_SHIFT 23
    #define HSW_PS_SAMPLE_MASK_SHIFT 12
    int threads = (203 << HSW_PS_MAX_THREADS_SHIFT | 1 << HSW_PS_SAMPLE_MASK_SHIFT);

    #define GEN7_PS_8_DISPATCH_ENABLE (1 << 0)
    #define GEN7_PS_16_DISPATCH_ENABLE (1 << 1)
    #define GEN7_PS_ATTRIBUTE_ENABLE (1 << 10)
    *ptr++ = (threads | GEN7_PS_8_DISPATCH_ENABLE | GEN7_PS_16_DISPATCH_ENABLE | GEN7_PS_ATTRIBUTE_ENABLE);

    #define GEN7_PS_DISPATCH_START_GRF_SHIFT_0 16
    #define GEN7_PS_DISPATCH_START_GRF_SHIFT_1 8
    #define GEN7_PS_DISPATCH_START_GRF_SHIFT_2 0
    *ptr++ = (6 << GEN7_PS_DISPATCH_START_GRF_SHIFT_2) | // first register in kernel[2] (g6 in SIMD16)
             (4 << GEN7_PS_DISPATCH_START_GRF_SHIFT_0);  // first register in kernel[0] (g4 in SIMD8)
    *ptr++ = 0; // kernel[1] ia absent

    *ptr++ = pk16 - batch_buffer; // kernel[2]

    // ------------------------------------------------------------------------
    #define GEN7_3DSTATE_SCISSOR_STATE_POINTERS	GEN7_3D(3, 0, 0xf)
    *ptr++ = GEN7_3DSTATE_SCISSOR_STATE_POINTERS;

    struct gen_scissor_rect {
        uint32_t xmin:16;
        uint32_t ymin:16;
        uint32_t xmax:16;
        uint32_t ymax:16;
    };

    struct gen_scissor_rect * scissor_rect = (struct gen_scissor_rect *) ALIGN(state, 64);
    state = (uint8_t *)scissor_rect + sizeof(*scissor_rect);
    scissor_rect->xmax = WIDTH - 1;
    scissor_rect->ymax = HEIGHT - 1;

    *ptr++ = ((uint8_t *)scissor_rect - batch_buffer);

    // ------------------------------------------------------------------------

    /// gen7_emit_depth_buffer(batch);
    #define GEN7_3DSTATE_DEPTH_BUFFER GEN7_3D(3, 0, 0x05)
    *ptr++ = (GEN7_3DSTATE_DEPTH_BUFFER | (7 - 2));

    #define GEN7_SURFACE_NULL 7
    #define GEN7_DB_SURFACE_2D 1
    #define GEN7_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT 29
    #define GEN7_DEPTHFORMAT_D32_FLOAT 1
    #define GEN7_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT 18
    *ptr++ = (GEN7_DB_SURFACE_2D << GEN7_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT | GEN7_DEPTHFORMAT_D32_FLOAT << GEN7_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT);

    *ptr++ = 0; // disable depth, stencil and hiz
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_HIER_DEPTH_BUFFER		GEN7_3D(3, 0, 0x07)
    *ptr++ = (GEN7_3DSTATE_HIER_DEPTH_BUFFER | 1);
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_STENCIL_BUFFER		GEN7_3D(3, 0, 0x06)
    *ptr++ = (GEN7_3DSTATE_STENCIL_BUFFER | 1);
    *ptr++ = 0;
    *ptr++ = 0;

    #define GEN7_3DSTATE_CLEAR_PARAMS GEN7_3D(3, 0, 0x04)
    *ptr++ = (GEN7_3DSTATE_CLEAR_PARAMS | (3 - 2));
    *ptr++ = 0;
    *ptr++ = 0x00000001;

    // ------------------------------------------------------------------------
    /// gen7_emit_drawing_rectangle(batch, dst);
    #define GEN7_3DSTATE_DRAWING_RECTANGLE GEN7_3D(3, 1, 0)
    *ptr++ = (GEN7_3DSTATE_DRAWING_RECTANGLE | (4 - 2));
    *ptr++ = 0;
    *ptr++ = ((HEIGHT - 1) << 16 | (STRIDE / sizeof(uint32_t) - 1));
    *ptr++ = 0;

    // ------------------------------------------------------------------------
    /// gen7_emit_vertex_buffer(batch, src_x, src_y, dst_x, dst_y, width, height);
    #define GEN7_3DSTATE_VERTEX_BUFFERS GEN7_3D(3, 0, 8)
    *ptr++ = (GEN7_3DSTATE_VERTEX_BUFFERS | (5 - 2));

    #define GEN7_VB0_BUFFER_INDEX_SHIFT 26
    #define GEN7_VB0_VERTEXDATA (0 << 20)
    #define GEN7_VB0_ADDRESS_MODIFY_ENABLE (1 << 14)
    #define GEN7_VB0_BUFFER_PITCH_SHIFT 0
    *ptr++ = (0 << GEN7_VB0_BUFFER_INDEX_SHIFT | GEN7_VB0_VERTEXDATA | GEN7_VB0_ADDRESS_MODIFY_ENABLE | 16 << GEN7_VB0_BUFFER_PITCH_SHIFT);

    /// offset = gen7_create_vertex_buffer(batch, src_x, src_y, dst_x, dst_y, width, height);
    float * v = (float *) ALIGN(state, 8);
    state = (uint8_t *)v + 16 * sizeof(*v);

    v[0] =  0.0; // x
    v[1] = -1.0; // y
    v[2] =  0.0; // z
    v[3] =  1.0; // w

    v[4] = -1.0; // x
    v[5] =  0.0; // y
    v[6] =  0.0; // z
    v[7] =  1.0; // w

    v[8] =  1.0; // x
    v[9] =  0.0; // y
    v[10] = 0.0; // z
    v[11] = 1.0; // w

    v[12] = 0.0; // x
    v[13] = 1.0; // y
    v[14] = 0.0; // z
    v[15] = 1.0; // w

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
    /// gen7_emit_vertex_elements(batch);
    #define GEN7_3DSTATE_VERTEX_ELEMENTS GEN7_3D(3, 0, 9)
    *ptr++ = (GEN7_3DSTATE_VERTEX_ELEMENTS | 1);

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
    *ptr++ = (1 << GEN7_VE1_VFCOMPONENT_0_SHIFT | // x
              1 << GEN7_VE1_VFCOMPONENT_1_SHIFT | // y
              1 << GEN7_VE1_VFCOMPONENT_2_SHIFT | // z
              1 << GEN7_VE1_VFCOMPONENT_3_SHIFT   // w
             );

*ptr++ = 0x780c0000;
*ptr++ = 0;

    // // ------------------------------------------------------------------------
    // #define GEN7_3DSTATE_VF				GEN7_3D(3, 0, 0x0c)
    // *ptr++ = GEN7_3DSTATE_VF;
    // *ptr++ = 0;

    // ------------------------------------------------------------------------
    #define GEN7_3DPRIMITIVE GEN7_3D(3, 3, 0)
    *ptr++ = (GEN7_3DPRIMITIVE | (7- 2));

    #define GEN7_3DPRIMITIVE_VERTEX_SEQUENTIAL (0 << 15)
    #define _3DPRIM_TRISTRIP 0x05
    #define _3DPRIM_RECTLIST 0x0F
    *ptr++ = (GEN7_3DPRIMITIVE_VERTEX_SEQUENTIAL | _3DPRIM_TRISTRIP);

    *ptr++ = 4; // Vertex Count Per Instance
    *ptr++ = 0; // Start Vertex Location
    *ptr++ = 1; // 1 instance
    *ptr++ = 0; // start instance location
    *ptr++ = 0; // index buffer offset, ignored

    // ------------------------------------------------------------------------

    #define MI_BATCH_BUFFER_END (0xA << 23)
    *ptr++ = MI_BATCH_BUFFER_END;

    // ------------------------------------------------------------------------
    /// gen7_render_flush(batch, context, batch_end);

    ret = drm_intel_bo_subdata(batch_bo, 0, sizeof(batch_buffer), batch_buffer);
    if (ret != 0) {
        printf("drm_intel_bo_subdata failed ret = %d:%s\n", ret, strerror(-ret));
        exit(EXIT_FAILURE);
    }

    uint32_t batch_end = (uint32_t)(uintptr_t) ALIGN((void *)((uint8_t *)ptr - batch_buffer), 8);
    ret = drm_intel_bo_mrb_exec(batch_bo, batch_end, NULL, 0, 0, 0);
    printf("drm_intel_bo_mrb_exec batch_end = %p\n", batch_end);
    if (ret != 0) {
        printf("drm_intel_bo_mrb_exec failed ret = %d: %s\n", ret, strerror(-ret));
        exit(EXIT_FAILURE);
    }

  	drm_intel_bo_unreference(batch_bo);

    // ------------------------------------------------------------------------
    /// gem_read(data->drm_fd, buf->bo->handle, 0, data->linear, sizeof(data->linear));
    struct drm_i915_gem_pread gem_pread = { 0 };

    uint32_t * linear_out = (uint32_t *)ALIGN(malloc(linear_size + 4096), 4096);
    memset(linear_out, 0, linear_size);

    gem_pread.handle = dst_bo->handle;
    gem_pread.data_ptr = (uintptr_t)linear_out;
    gem_pread.size = linear_size;
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
