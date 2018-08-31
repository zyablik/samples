#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

int create_and_map_fb(int dri_fd, uint32_t width, uint32_t height, void ** fb_base, uint32_t * fb_id) {
    //If we create the buffer later, we can get the size of the screen first.
    //This must be a valid mode, so it's probably best to do this after we find
    //a valid crtc with modes.
    struct drm_mode_create_dumb create_dumb_fb = {0};
    create_dumb_fb.width = width;
    create_dumb_fb.height = height;
    create_dumb_fb.bpp = 32;
    create_dumb_fb.flags = 0;
    create_dumb_fb.pitch = 0;
    create_dumb_fb.size = 0;
    create_dumb_fb.handle = 0;
    if(ioctl(dri_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb_fb) != 0) {
        printf("error while ioctl(DRM_IOCTL_MODE_CREATE_DUMB): %d:%s\n", errno, strerror(errno));
        return -1;
    }
    printf("DRM_IOCTL_MODE_CREATE_DUMB create_dumb_fb.handle = %u\n", create_dumb_fb.handle);

    struct drm_mode_fb_cmd add_dumb_fb = {0};
    add_dumb_fb.width = create_dumb_fb.width;
    add_dumb_fb.height = create_dumb_fb.height;
    add_dumb_fb.bpp = create_dumb_fb.bpp;
    add_dumb_fb.pitch = create_dumb_fb.pitch;
    add_dumb_fb.depth = 24;
    add_dumb_fb.handle = create_dumb_fb.handle;
    if(ioctl(dri_fd, DRM_IOCTL_MODE_ADDFB, &add_dumb_fb) != 0) {
        printf("error while ioctl(DRM_IOCTL_MODE_ADDFB): %d:%s\n", errno, strerror(errno));
        return -1;
    }
    printf("DRM_IOCTL_MODE_ADDFB: add_dumb_fb.fb_id = %u\n", add_dumb_fb.fb_id);
    *fb_id = add_dumb_fb.fb_id;

    struct drm_mode_map_dumb map_dumb_fb = {0};
    map_dumb_fb.handle = create_dumb_fb.handle;
    if(ioctl(dri_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb_fb) != 0) {
        printf("error while ioctl(DRM_IOCTL_MODE_MAP_DUMB): %d:%s\n", errno, strerror(errno));
        return -1;
    }

    *fb_base = mmap(0, create_dumb_fb.size, PROT_READ | PROT_WRITE, MAP_SHARED, dri_fd, map_dumb_fb.offset);
    printf("FB: base = %p width = %d height = %d\n", *fb_base, create_dumb_fb.width, create_dumb_fb.height);

    return 0;
}

int set_crtc(int dri_fd, uint32_t connector_id, struct drm_mode_modeinfo connector_mode, uint32_t encoder_id, uint32_t fb_id) {
    struct drm_mode_get_encoder encoder;

    encoder.encoder_id = encoder_id;
    if(ioctl(dri_fd, DRM_IOCTL_MODE_GETENCODER, &encoder) != 0) { //get encoder
        printf("error while ioctl(DRM_IOCTL_MODE_GETENCODER): %d:%s\n", errno, strerror(errno));
        return -1;
    }

    struct drm_mode_crtc crtc = {0};
    crtc.crtc_id = encoder.crtc_id;
    if(ioctl(dri_fd, DRM_IOCTL_MODE_GETCRTC, &crtc) != 0) {
        printf("error while ioctl(DRM_IOCTL_MODE_GETCRTC): %d:%s\n", errno, strerror(errno));
        return -1;
    }

    crtc.fb_id = fb_id;
    crtc.set_connectors_ptr = (uint64_t)&connector_id;
    crtc.count_connectors = 1;
    crtc.mode = connector_mode;
    crtc.mode_valid = 1;
    if(ioctl(dri_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) != 0) {
        printf("error while ioctl(DRM_IOCTL_MODE_SETCRTC): %d:%s\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

int main() {
//------------------------------------------------------------------------------
//Opening the DRI device
//------------------------------------------------------------------------------
    const char * dri_dev_file = "/dev/dri/card0";
    int dri_fd = open(dri_dev_file, O_RDWR);
    if(dri_fd == -1) {
        printf("error while open %s: %d:%s\n", dri_dev_file, errno, strerror(errno));
        return 1;
    }

//------------------------------------------------------------------------------
//Kernel Mode Setting (KMS)
//------------------------------------------------------------------------------
    struct drm_mode_card_res card_res = {0};

    //Become the "master" of the DRI device
    if(ioctl(dri_fd, DRM_IOCTL_SET_MASTER, 0) != 0) {
        printf("error while ioctl(DRM_IOCTL_SET_MASTER): %d:%s\n", errno, strerror(errno));
        return 1;
    }

    //Get resource counts
    if(ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &card_res) != 0) {
        printf("error while ioctl(DRM_IOCTL_MODE_GETRESOURCES) counts: %d:%s\n", errno, strerror(errno));
        return 1;
    }

    printf("counts: fb: %d, crtc: %d, conn: %d, enc: %d\n", card_res.count_fbs, card_res.count_crtcs, card_res.count_connectors, card_res.count_encoders);

    uint32_t fb_ids[card_res.count_fbs];
    uint32_t crtc_ids[card_res.count_crtcs];
    uint32_t connector_ids[card_res.count_connectors];
    uint32_t encoder_ids[card_res.count_encoders];

    card_res.fb_id_ptr = (uint64_t) fb_ids;
    card_res.crtc_id_ptr = (uint64_t) crtc_ids;
    card_res.connector_id_ptr = (uint64_t) connector_ids;
    card_res.encoder_id_ptr = (uint64_t) encoder_ids;

    //Get resource IDs
    if(ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &card_res) != 0) {
        printf("error while ioctl(DRM_IOCTL_MODE_GETRESOURCES) ids: %d:%s\n", errno, strerror(errno));
        return 1;
    }

    //Loop though all available connectors
    int i;
    struct drm_mode_get_connector connector;
    struct drm_mode_modeinfo connector_mode;
    uint32_t  connector_id;
    for (i = 0; i < card_res.count_connectors; i++) {
        memset(&connector, 0, sizeof(connector));
        connector.connector_id = connector_ids[i];
        if(ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector) != 0) { //get connector resource counts
            printf("error while ioctl(DRM_IOCTL_MODE_GETCONNECTOR) counts: %d:%s\n", errno, strerror(errno));
            return 1;
        }

        struct drm_mode_modeinfo connector_modes[connector.count_modes];
        uint32_t connector_props[connector.count_props];
        uint64_t connector_propvals[connector.count_props];
        uint32_t connector_encoders[connector.count_encoders];

        connector.modes_ptr = (uint64_t) connector_modes;
        connector.props_ptr = (uint64_t) connector_props;
        connector.prop_values_ptr = (uint64_t) connector_propvals;
        connector.encoders_ptr = (uint64_t) connector_encoders;
        if(ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector) != 0) { //get connector resources
            printf("error while ioctl(DRM_IOCTL_MODE_GETCONNECTOR) resources: %d:%s\n", errno, strerror(errno));
            return 1;
        }

        //Check if the connector is OK to use (connected to something)
         printf("i = %d connector: connector_id = %d count_encoders = %d count_modes = %d encoder_id = %d connection = %d\n", i, connector.connector_id, connector.count_encoders, connector.count_modes, connector.encoder_id, connector.connection);

        if (connector.count_encoders<1 || connector.count_modes<1 || !connector.encoder_id || !connector.connection) {
            printf("skip incompatible connector\n");
            continue;
        } else {
            printf("use connector %d\n", connector.connector_id);
            connector_mode = connector_modes[0];
            printf("use connectors mode[0]: %s\n", connector_mode.name);
            break;
        }
    }
//------------------------------------------------------------------------------
//Creating a dumb buffer
//------------------------------------------------------------------------------
    void * fb_base[2];
    uint32_t fb_id[2];
    int ret = create_and_map_fb(dri_fd, connector_mode.hdisplay, connector_mode.vdisplay, &fb_base[0], &fb_id[0]);
    if(ret != 0) {
        printf("create_and_map_fb 1 error\n");
        exit(1);
    }

    ret = create_and_map_fb(dri_fd, connector_mode.hdisplay, connector_mode.vdisplay, &fb_base[1], &fb_id[1]);
    if(ret != 0) {
        printf("create_and_map_fb 2 error\n");
        exit(1);
    }

    uint32_t fb_width = connector_mode.hdisplay;
    uint32_t fb_height = connector_mode.vdisplay;

//------------------------------------------------------------------------------
//Kernel Mode Setting (KMS)
//------------------------------------------------------------------------------

//    Stop being the "master" of the DRI device
//    ioctl(dri_fd, DRM_IOCTL_DROP_MASTER, 0);

//------------------------------------------------------------------------------
//draw
//------------------------------------------------------------------------------

    uint32_t x, y;
    for (i = 0; i < 3; i++) {
        uint32_t color = (rand() % 0x00ffffff) & 0x00ff00ff;
//        printf("i = %d color = 0x%x %d x %d fb_id = %d fb_base = %p\n", i, color, fb_width, fb_height, fb_id[i % 2], fb_base[i % 2]);
        for(y = 0; y < fb_height; y++)
            for(x = 0; x < fb_width; x++) {
                 int location = y * fb_width + x;
                 *(((uint32_t*) fb_base[i % 2]) + location) = color;
            }
//        usleep(100000);
        set_crtc(dri_fd, connector.connector_id, connector_mode, connector.encoder_id, fb_id[i % 2]);
    }

    return 0;
}
