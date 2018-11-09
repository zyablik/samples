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

/* Batch */
#define MI_BATCH_BUFFER		((0x30 << 23) | 1)
#define MI_BATCH_BUFFER_START	(0x31 << 23)
#define MI_BATCH_BUFFER_END	(0xA << 23)


#include <drm/i915_drm.h>

// static int is_intel(int fd) {
//     struct drm_i915_getparam gp;
//     int devid;
// 
//     gp.param = I915_PARAM_CHIPSET_ID;
//     gp.value = &devid;
// 
//     if(ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp, sizeof(gp)))
//         return 0;
// 
//     return IS_INTEL(devid);
// }

int main(int argc, char ** argv) {
    /// fd = drm_open_any();
    const char * dri_path = "/dev/dri/card0";

    int dri_fd = open(dri_path, O_RDWR);
    if(dri_fd == -1) {
        printf("error while open %s: %d:%s\n", dri_path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    // is_intel(dri_fd)

    printf("open(%s) done: dri_fd = %d\n", dri_path, dri_fd);

    /// handle = gem_create(dri_fd, 4096);
    struct drm_i915_gem_create create;
    create.handle = 0;
    create.size = 4096;

    int ret = ioctl(dri_fd, DRM_IOCTL_I915_GEM_CREATE, &create);
    if(ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GEM_CREATE): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(create.handle == 0) {
        printf("ioctl(DRM_IOCTL_I915_GEM_CREATE): create.handle = 0. EXIT_FAILURE\n");
        exit(EXIT_FAILURE);
    }

    printf("ioctl(DRM_IOCTL_I915_GEM_CREATE) done: handle = %d\n", create.handle);

    /// gem_write(dri_fd, handle, 0, batch, sizeof(batch));
    uint32_t batch[2] = { MI_BATCH_BUFFER_END };

    struct drm_i915_gem_pwrite gem_pwrite;

    gem_pwrite.handle = create.handle;
    gem_pwrite.offset = 0;
    gem_pwrite.size = sizeof(batch);
    gem_pwrite.data_ptr = (uintptr_t)batch;

    ret = ioctl(dri_fd, DRM_IOCTL_I915_GEM_PWRITE, &gem_pwrite);
    if(ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GEM_PWRITE): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("ioctl(DRM_IOCTL_I915_GEM_PWRITE) done\n");

    /// igt_assert(exec(dri_fd, create.handle, 1, I915_EXEC_RENDER) == 0);
    struct drm_i915_gem_exec_object2 gem_exec[1];
    gem_exec[0].handle = create.handle;
    gem_exec[0].relocation_count = 0;
    gem_exec[0].relocs_ptr = 0;
    gem_exec[0].alignment = 0;
    gem_exec[0].offset = 0;
    gem_exec[0].flags = 0;
    gem_exec[0].rsvd1 = 0;
    gem_exec[0].rsvd2 = 0;

    struct drm_i915_gem_execbuffer2 execbuf;
    execbuf.buffers_ptr = (uintptr_t)gem_exec;
    execbuf.buffer_count = 1;
    execbuf.batch_start_offset = 0;
    execbuf.batch_len = 8;
    execbuf.cliprects_ptr = 0;
    execbuf.num_cliprects = 0;
    execbuf.DR1 = 0;
    execbuf.DR4 = 0;
    execbuf.flags = I915_EXEC_RENDER;
    i915_execbuffer2_set_context_id(execbuf, 0);
    execbuf.rsvd2 = 0;

    ret = ioctl(dri_fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &execbuf);
    if(ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2) done\n");

    /// gem_sync(dri_fd, create.handle) // This wraps the SET_DOMAIN ioctl, which is used to control the coherency of the gem buffer object between the cpu and gtt mappings

    struct drm_i915_gem_set_domain set_domain;
    set_domain.handle = create.handle;
    set_domain.read_domains = I915_GEM_DOMAIN_GTT;
    set_domain.write_domain = I915_GEM_DOMAIN_GTT;

    ret = ioctl(dri_fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
    if(ret == -1) {
        printf("error while ioctl(DRM_IOCTL_I915_GEM_SET_DOMAIN): %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("ioctl(DRM_IOCTL_I915_GEM_SET_DOMAIN) done\n");

    return 0;
}
