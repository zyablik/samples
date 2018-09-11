#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <linux/fb.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("usage: %s on|off\n", argv[0]);
        return 1;
    }

    if(strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0) {
        printf("usage: %s on|off\n", argv[0]);
        return 1;
    }

    const char * fbname = "/dev/graphics/fb0";
    int fb_fd= open(fbname, O_RDWR);
    if (fb_fd < 0) {
        printf("unable to open %s: %d: %s\n", fbname, errno, strerror(errno));
        return 1;
    }

    if(strcmp(argv[1], "on") == 0) {
        if(ioctl(fb_fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0)
            printf("error while ioctl(FBIOBLANK, FB_BLANK_UNBLANK): %d: %s\n", errno, strerror(errno));
    } else {
        if(ioctl(fb_fd, FBIOBLANK, FB_BLANK_POWERDOWN) < 0)
            printf("error while ioctl(FBIOBLANK, FB_BLANK_POWERDOWN): %d: %s\n", errno, strerror(errno));
    }
    return 0;
}
