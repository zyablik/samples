#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sstream>
#include <unistd.h>

// outputs only when drawing:
// VSYNC=150886377143122, xxxxxxEvent=x 
// VSYNC=150886393826976, xxxxxxEvent=x 
// VSYNC=150886410257705, xxxxxxEvent=x 
// VSYNC=150886426638955, xxxxxxEvent=x 
// VSYNC=150886443078018, xxxxxxEvent=x 
// VSYNC=150886459497289, xxxxxxEvent=x 
// VSYNC=150886475613434, xxxxxxEvent=x 
// VSYNC=150886492024372, xxxxxxEvent=x 
// VSYNC=150886508433747, xxxxxxEvent=x 
// VSYNC=150886525125934, xxxxxxEvent=x 
// VSYNC=150946353450404, xxxxxxEvent=x 
// VSYNC=150946369592071, xxxxxxEvent=x 

int main(int, char **) {
        // looks like enable/disable vsync is only affects "/sys/class/graphics/fb0/vsync_event" event generation. fps is always 60
    const char fb_dev_file_name[] = "/dev/graphics/fb0";
    int fb_dev_fd = open(fb_dev_file_name, O_RDWR);
    if (fb_dev_fd < 0) {
        printf("unable to open %s error = %d: %s\n", fb_dev_file_name, errno, strerror(errno));
        exit(1);
    }
    printf("framebuffer %s opened fb_dev_fd = %d\n", fb_dev_file_name, fb_dev_fd);

    // from mate9-55/vendor/hisi/ap/kernel/drivers/video/hisi/dss/hisi_fb.c
    #define HISIFB_IOCTL_MAGIC 'M'
    #define HISIFB_VSYNC_CTRL _IOW(HISIFB_IOCTL_MAGIC, 0x02, unsigned int)

    int enabled = 1;
    if(ioctl(fb_dev_fd, HISIFB_VSYNC_CTRL, &enabled) < 0) {
        printf("vsync ctrl failed! enabled=%d : %s", enabled, strerror(errno));
        exit(1);
    }
    printf("HISIFB_VSYNC_CTRL enabled = %d\n", enabled);

//  gles uses /sys/devices/virtual/graphics/fb0/vsync_event
    const char vsync_file_name[] = "/sys/class/graphics/fb0/vsync_event";
//    const char vsync_file_name[] = "/sys/class/graphics/fb0/vsync_timestamp";
    int vsync_file_fd = open(vsync_file_name, O_RDONLY);
    if (vsync_file_fd < 0) {
        printf("error while open %s: %d: %s\n", vsync_file_name, errno, strerror(errno));
        exit(1);
    }

    printf("vsync_file_fd = %d\n", vsync_file_fd);
    while(true) {
        const int MAX_DATA = 64;
        static char timestamp[MAX_DATA];
        ssize_t len = pread(vsync_file_fd, timestamp, MAX_DATA -1, 0);
        if (len < 0)
            printf("error reading vsync timestamp: %d: %s\n", errno, strerror(errno));

        printf("%s", timestamp);
    }

    return 0;
}
