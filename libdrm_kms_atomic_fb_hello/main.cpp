#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static uint32_t get_property_id(int fd, drmModeObjectProperties *props, const char *name) {
    drmModePropertyPtr property;
    uint32_t i, id = 0;

    /* find property according to the name */
    for (i = 0; i < props->count_props; i++) {
        property = drmModeGetProperty(fd, props->props[i]);

        printf("get_property_id(name = %s) property->name = %s property->prop_id = %u\n", name, property->name, property->prop_id);

        if (!strcmp(property->name, name))
            id = property->prop_id;
        drmModeFreeProperty(property);

        if (id)
            break;
    }
    printf("get_property_id(name = %s) return id = %u\n\n", name, id);

    return id;
}

int main(int argc, char *argv[])
{
    int  fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

    drmModeRes * res = drmModeGetResources(fd);

    uint32_t crtc_id = res->crtcs[0];
    uint32_t conn_id = res->connectors[0];

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmModePlaneRes * plane_res = drmModeGetPlaneResources(fd);
    uint32_t plane_id = plane_res->planes[0];

    drmModeConnector * conn = drmModeGetConnector(fd, conn_id);
    uint32_t width = conn->modes[0].hdisplay;
    uint32_t height = conn->modes[0].vdisplay;

    struct drm_mode_create_dumb create = {};
    struct drm_mode_map_dumb map = {};

    create.width = width;
    create.height = height;
    create.bpp = 32;
    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

    printf("modeset_create_fb() create.pitch = %d create.size = %d create.handle = %d\n", create.pitch, create.size, create.handle);
    uint32_t pitch = create.pitch;
    uint32_t size = create.size;
    uint32_t handle = create.handle;
    uint32_t fb_id;
    int ret = drmModeAddFB(fd, width, height, 24, 32, pitch, handle, &fb_id);
    printf("drmModeAddFB ret = %d (%s) width = %d height = %d pitch = %d handle = %d fb_id = %d\n",
           ret, strerror(-ret), width, height, pitch, handle, fb_id);

    map.handle = create.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

    uint8_t * vaddr = (uint8_t *) mmap(0, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);

    memset(vaddr, 0xff, size);

    drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);

    drmModeObjectProperties * props = drmModeObjectGetProperties(fd, conn_id,    DRM_MODE_OBJECT_CONNECTOR);
    uint32_t property_crtc_id = get_property_id(fd, props, "CRTC_ID");
    drmModeFreeObjectProperties(props);

    props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
    uint32_t property_active = get_property_id(fd, props, "ACTIVE");
    uint32_t property_mode_id = get_property_id(fd, props, "MODE_ID");
    drmModeFreeObjectProperties(props);

    // create blob to store current mode, and retun the blob id 
    uint32_t blob_id;
    drmModeCreatePropertyBlob(fd, &conn->modes[0], sizeof(conn->modes[0]), &blob_id);

    drmModeAtomicReq * req = drmModeAtomicAlloc();
    drmModeAtomicAddProperty(req, crtc_id, property_active, 1);
    drmModeAtomicAddProperty(req, crtc_id, property_mode_id, blob_id);
    drmModeAtomicAddProperty(req, conn_id, property_crtc_id, crtc_id);

    ret = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
    printf("drmModeAtomicCommit(): ret = %d errno = %d crtc_id = %u conn_id = %u property_active = %u property_mode_id = %u property_crtc_id = %u blob_id = %u\n",
           ret, errno, crtc_id, conn_id, property_active, property_mode_id, property_crtc_id, blob_id);

    drmModeAtomicFree(req);

    drmModeSetPlane(fd, plane_id, crtc_id, fb_id, 0,
                    50, 50, 320, 320,
                    0, 0, 320 << 16, 320 << 16);

    printf("drmModeSetPlane\n");
    getchar();

    struct drm_mode_destroy_dumb destroy = {};
    drmModeRmFB(fd, fb_id);
    munmap(vaddr, size);
    destroy.handle = handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

    drmModeFreeConnector(conn);
    drmModeFreePlaneResources(plane_res);
    drmModeFreeResources(res);

    close(fd);

    return 0;
}
