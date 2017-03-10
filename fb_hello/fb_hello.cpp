#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sstream>
#include <unistd.h>

/* fixed screen information */
struct fb_fix_screeninfo fix_info;

/* configurable screen info */
struct fb_var_screeninfo var_info;

void draw_pixel(uint8_t * current_frame, int x, int y, uint32_t pixel);

const std::string fb_type2str(uint32_t type);
const std::string fb_visual2str(uint32_t visual);
const std::string fb_activate2str(uint32_t activate);
const std::string fb_sync2str(uint32_t sync);
const std::string fb_vmode2str(uint32_t vmode);
const std::string fb_rotate2str(uint32_t rotate);

#include "fps.h"

int main(int argc, char *argv[]) {
    char * fbname;
    if(argc > 1) {
        fbname = argv[1];
    } else {
        printf("usage: %s FB_DEV_PATH ( usually /dev/graphics/fb0 )\n", argv[0]);
        exit(1);
    }

    /* Open the framebuffer device in read write */
    int fb_dev_fd = open(fbname, O_RDWR);
    if (fb_dev_fd < 0) {
        printf("unable to open %s error = %d: %s\n", fbname, errno, strerror(errno));
        exit(1);
    }
    printf("framebuffer %s opened fb_dev_fd = %d\n", fbname, fb_dev_fd);

    /* Do Ioctl. Retrieve fixed screen info. */
    if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fix_info) < 0) {
        printf("unable to retrieve fixed screen info: %d: %s\n", errno, strerror(errno));
        exit(1);
    }

    printf("fb_fix_screeninfo:\n");
    printf("  id = %s\n", fix_info.id);
    printf("  smem_start = 0x%lx\n", fix_info.smem_start);
    printf("  smem_len = %u\n", fix_info.smem_len);
    printf("  type = %s\n", fb_type2str(fix_info.type).c_str());
    printf("  type_aux = %u\n", fix_info.type_aux);
    printf("  visual = %s\n", fb_visual2str(fix_info.visual).c_str());
    printf("  xpanstep = %u\n", fix_info.xpanstep);
    printf("  ypanstep = %u\n", fix_info.ypanstep);
    printf("  ywrapstep = %u\n", fix_info.ywrapstep);
    printf("  line_length = %u\n", fix_info.line_length);
    printf("  mmio_start = %lu\n", fix_info.mmio_start);
    printf("  mmio_len = %u\n", fix_info.mmio_len);
    printf("  accel = %u\n", fix_info.accel);
    printf("  capabilities = %u\n", fix_info.capabilities);
    
    /* Do Ioctl. Get the variable screen info. */
    if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &var_info) < 0) {
        printf("unable to retrieve variable screen info: %d: %s\n", errno, strerror(errno));
        exit(1);
    }

    printf("\nfb_var_screeninfo:\n");
    printf("  screen resolution: visible: (%d x %d) virtual: (%d x %d) offset from virtual to visible: (%d, %d)\n",
           var_info.xres, var_info.yres, var_info.xres_virtual, var_info.yres_virtual, var_info.xoffset, var_info.yoffset);
    printf("  bits_per_pixel = %d\n", var_info.bits_per_pixel); // it is possible to change it and write back
    printf("  grayscale = %d\n", var_info.grayscale);
    printf("  red: length %d bits, offset %d, msb_right = %d\n", var_info.red.length, var_info.red.offset, var_info.red.msb_right);
    printf("  green: length %d bits, offset %d, msb_right = %d\n", var_info.green.length, var_info.green.offset, var_info.green.msb_right);
    printf("  blue: length %d bits, offset %d, msb_right = %d\n", var_info.blue.length, var_info.blue.offset, var_info.blue.msb_right);
    printf("  transparency: length %d bits, offset %d, msb_right = %d\n", var_info.transp.length, var_info.transp.offset, var_info.transp.msb_right);
    printf("  nonstd = %d\n", var_info.nonstd);
    printf("  activate = %s\n", fb_activate2str(var_info.activate).c_str());
    printf("  height = %d mm\n", var_info.height);
    printf("  width = %d mm\n", var_info.width);
    printf("  pixclock = %d micro seconds\n", var_info.pixclock / 1000000);
    printf("  left_margin /* time from sync to picture */ = %d pico seconds\n", var_info.left_margin);
    printf("  right_margin /* time from picture to sync */ = %d pico seconds\n", var_info.right_margin);
    printf("  upper_margin /* time from sync to picture */ = %d pico seconds\n", var_info.upper_margin);
    printf("  hsync_len = %d\n", var_info.hsync_len);
    printf("  vsync_len = %d\n", var_info.vsync_len);
    printf("  sync = %s\n", fb_sync2str(var_info.sync).c_str());
    printf("  vmode = %s\n", fb_vmode2str(var_info.vmode).c_str());
    printf("  rotate = %s\n", fb_rotate2str(var_info.rotate).c_str());
    printf("  colorspace = %s\n", fb_vmode2str(var_info.colorspace).c_str());
    printf("\n");

    /* Calculate the size to mmap */
    size_t frame_size = fix_info.line_length * var_info.yres;
    size_t frame_buffer_size = frame_size * 2; // allocate front and back buffers
    /* Now mmap the framebuffer. */
    void * mmapped_fb_mem = mmap(NULL, frame_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_dev_fd, 0);
    if (mmapped_fb_mem == NULL) {
        printf("mmap failed: %d:%s\n", errno, strerror(errno));
        exit(1);
    }
    printf("framebuffer mmap address = %p\n", mmapped_fb_mem);
    printf("framebuffer frame_buffer_size = %lu bytes\n", frame_buffer_size);
    printf("framebuffer end address = %p\n", (uint8_t *)mmapped_fb_mem + frame_buffer_size);

    // FBIOPUT_VSCREENINFO required to make fb work (on android at least)
    var_info.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
    if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &var_info) < 0) {
        printf("unable to write back variable screen info: %d:%s\n", errno, strerror(errno));
        exit(1);
    }

    #define HISIFB_IOCTL_MAGIC 'M'
    #define HISIFB_VSYNC_CTRL _IOW(HISIFB_IOCTL_MAGIC, 0x02, unsigned int)
    #define HISIFB_DSS_VOLTAGE_GET _IOW(HISIFB_IOCTL_MAGIC, 0x10, struct dss_clk_rate)
typedef struct dss_clk_rate {
    uint64_t dss_pri_clk_rate;
    uint64_t dss_pclk_dss_rate;
    uint64_t dss_pclk_pctrl_rate;
    uint32_t dss_voltage_value; //0:0.7v, 2:0.8v
} dss_clk_rate_t;

    // looks like enable/disable vsync is only affects "/sys/class/graphics/fb0/vsync_event" event generation. fps is always 60
    int enabled = 1;
    if(ioctl(fb_dev_fd, HISIFB_VSYNC_CTRL, &enabled) < 0) {
        printf("vsync ctrl failed! enabled=%d : %s", enabled, strerror(errno));
        exit(1);
    }
    printf("HISIFB_VSYNC_CTRL enabled = %d\n", enabled);
    
    struct dss_clk_rate dcr;
    if(ioctl(fb_dev_fd, HISIFB_DSS_VOLTAGE_GET, &dcr) < 0) {
        printf("HISIFB_DSS_VOLTAGE_GET failed!%s", strerror(errno));
        exit(1);
    }
    printf("HISIFB_DSS_VOLTAGE_GET enabled = %d\n", dcr.dss_voltage_value);
    
    
    /* The program will work only on TRUECOLOR */
    if (fix_info.visual != FB_VISUAL_TRUECOLOR) {
        printf("unsupported mode fix_info.visual = %s\n", fb_visual2str(fix_info.visual).c_str());
        exit(1);
    }
    /* White pixel ? maximum red, green & blue values */
    /* Max 8 bit value = 0xFF */
    uint8_t red = 0x00;
    uint8_t green = 0x00;
    uint8_t blue = 0xff;
    /*
    * Now pack the pixel based on the rgb bit offset.
    * We compute each color value based on bit length
    * and shift it to its corresponding offset in the pixel.
    *
    * For example: Considering a RGB565, the formula will
    * expand as:-
    * Red len=5, off=11 : Green len=6, off=6 : Blue len=5, off=0
    * pixel_value = ((0xFF >> (8 - 5) << 11)|
    * ((0xFF >> (8 - 6) << 6) |
    * ((0xFF >> (8 - 5) << 0) = 0xFFFF // White
    */
    uint32_t pixel = ((red   >> (8 - var_info.red.length))   << var_info.red.offset)   |
                     ((green >> (8 - var_info.green.length)) << var_info.green.offset) |
                     ((blue  >> (8 - var_info.blue.length))  << var_info.blue.offset);

    float t = 0.0;
    bool enable_double_buffering = false;
    while(true) {
        FPS fps;
        uint8_t * current_frame = (uint8_t *) mmapped_fb_mem;
        if(enable_double_buffering)
            current_frame += (var_info.yoffset == 0) ? frame_size : 0; // draw to non-current buffer

        int center_x = var_info.xres / 2 + 300 * cos(t);
        int center_y = var_info.yres / 2 + 300 * sin(t);
        t += 0.01;
        {
//            Duration d("draw pixels");
            // black screen
            memset(current_frame, 0, frame_size);

            // draw rectangle
            for(int x = center_x - 100; x < center_x + 100; x++) {
                for(int y = center_y - 100; y < center_y + 100; y++) {
                    draw_pixel(current_frame, x, y, pixel);
                }
            }
        }

        {
//           Duration d("swap buffer");
            if(enable_double_buffering) {
                //"Pan" to the back buffer
                if (var_info.yoffset == 0) {
                    var_info.yoffset = var_info.yres;
                } else {
                    var_info.yoffset = 0;
                }
                if (ioctl(fb_dev_fd, FBIOPAN_DISPLAY, &var_info)) // FBIOPUT_VSCREENINFO also works here
                    printf("ioctl(FBIOPAN_DISPLAY) error: %d:%s\n", errno, strerror(errno));
            } else {
                // FB_ACTIVATE_FORCE /* force apply even when no change */
                if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &var_info) < 0)
                    printf("unable to write back variable screen info: %d:%s\n", errno, strerror(errno));
            }
        }
    }

    printf("munmap mmapped_fb_mem = %p\n", mmapped_fb_mem);
    if(munmap(mmapped_fb_mem, frame_buffer_size) == -1) {
        printf("munmap %p failed: %d:%s\n", mmapped_fb_mem, errno, strerror(errno));
        exit(1);
    }
    if(close(fb_dev_fd) == -1) {
        printf("close %d failed: %d:%s\n", fb_dev_fd, errno, strerror(errno));
        exit(1);
    }
    return 0;
}

void draw_pixel(uint8_t * current_frame, int x, int y, u_int32_t pixel) {
    /*
    * Based on bits per pixel we assign the pixel_value to the
    * framebuffer pointer. Recollect the matrix indexing method
    * described for the linear framebuffer.
    * pixel(x,y)=(line_width * y) + x.
    */
//    printf("fix_info.line_length = %d y = %d, x = %d\n", fix_info.line_length, y, x);
    switch (var_info.bits_per_pixel) {
        case 8:
            *(current_frame + fix_info.line_length * y + x) = (uint8_t)pixel;
            break;
        case 16:
            *(uint16_t*)(current_frame + fix_info.line_length * y + x * 2) = (uint16_t)pixel;
            break;
        case 32:
            *(uint32_t*)(current_frame + fix_info.line_length * y + x * 4) = (uint32_t)pixel;
            break;
        default:
            printf("unknown depth var_info.bits_per_pixel = %d\n", var_info.bits_per_pixel);
    }
}

const std::string fb_type2str(uint32_t type) {
    std::string str;
    switch(type) {
        case FB_TYPE_PACKED_PIXELS:      str = "FB_TYPE_PACKED_PIXELS (/* Packed Pixels */)";             break;
        case FB_TYPE_PLANES:             str = "FB_TYPE_PLANES (/* Non interleaved planes */)";           break;
        case FB_TYPE_INTERLEAVED_PLANES: str = "FB_TYPE_INTERLEAVED_PLANES (/* Interleaved planes */)";   break;
        case FB_TYPE_TEXT:               str = "FB_TYPE_TEXT (/* Text/attributes */)";                    break;
        case FB_TYPE_VGA_PLANES:         str = "FB_TYPE_VGA_PLANES (/* EGA/VGA planes */)";               break;
        case FB_TYPE_FOURCC:             str = "FB_TYPE_FOURCC (/* Type identified by a V4L2 FOURCC */)"; break;
        default: str = (std::stringstream() << "unknown FB_TYPE " << type).str();
    }
    return str;
}

const std::string fb_visual2str(uint32_t visual) {
    std::string str;
    switch(visual) {
        case FB_VISUAL_MONO01:             str = "FB_VISUAL_MONO01 (/* Monochr. 1=Black 0=White */)";           break;
        case FB_VISUAL_MONO10:             str = "FB_VISUAL_MONO10 (/* Monochr. 1=White 0=Black */)";           break;
        case FB_VISUAL_TRUECOLOR:          str = "FB_VISUAL_TRUECOLOR (/* True color */)";                      break;
        case FB_VISUAL_PSEUDOCOLOR:        str = "FB_VISUAL_PSEUDOCOLOR (/* Pseudo color (like atari) */)";     break;
        case FB_VISUAL_DIRECTCOLOR:        str = "FB_VISUAL_DIRECTCOLOR (/* Direct color */)";                  break;
        case FB_VISUAL_STATIC_PSEUDOCOLOR: str = "FB_VISUAL_STATIC_PSEUDOCOLOR (/* Pseudo color readonly */)";  break;
        case FB_VISUAL_FOURCC:             str = "FB_VISUAL_FOURCC (/* Visual identified by a V4L2 FOURCC */)"; break;
        default: str = (std::stringstream() << "unknown FB_VISUAL_ " << visual).str();
    }
    return str;
}

const std::string fb_activate2str(uint32_t activate) {
    std::stringstream str;
    if(activate == FB_ACTIVATE_NOW)     str << "FB_ACTIVATE_NOW";
    if(activate & FB_ACTIVATE_MASK)     str << " | FB_ACTIVATE_MASK";
    if(activate & FB_ACTIVATE_NXTOPEN)  str << " | FB_ACTIVATE_NXTOPEN";
    if(activate & FB_ACTIVATE_TEST)     str << " | FB_ACTIVATE_TEST";
    if(activate & FB_ACTIVATE_VBL)      str << " | FB_ACTIVATE_VBL ";
    if(activate & FB_CHANGE_CMAP_VBL)   str << " | FB_CHANGE_CMAP_VBL ";
    if(activate & FB_ACTIVATE_ALL)      str << " | FB_ACTIVATE_ALL ";
    if(activate & FB_ACTIVATE_FORCE)    str << " | FB_ACTIVATE_FORCE ";
    if(activate & FB_ACTIVATE_INV_MODE) str << " | FB_ACTIVATE_INV_MODE ";
    return str.str();
}

const std::string fb_sync2str(uint32_t sync) {
    std::string str;
    switch(sync) {
        case FB_SYNC_HOR_HIGH_ACT:  str = "FB_SYNC_HOR_HIGH_ACT (/* horizontal sync high active */)";  break;
        case FB_SYNC_VERT_HIGH_ACT: str = "FB_SYNC_VERT_HIGH_ACT  (/* vertical sync high active */)";  break;
        case FB_SYNC_EXT:           str = "FB_SYNC_EXT (/* external sync */)";                         break;
        case FB_SYNC_COMP_HIGH_ACT: str = "FB_SYNC_COMP_HIGH_ACT  (/* composite sync high active */)"; break;
        case FB_SYNC_BROADCAST:     str = "FB_SYNC_BROADCAST  (/* broadcast video timings */)";        break;
        case FB_SYNC_ON_GREEN:      str = "FB_SYNC_ON_GREEN  (/* sync on green */)";                   break;
        default: str = (std::stringstream() << "unknown FB_SYNC_ " << sync).str();
    }
    return str;
}

const std::string fb_vmode2str(uint32_t vmode) {
    std::string str;
    switch(vmode) {
        case FB_VMODE_NONINTERLACED: str = "FB_VMODE_NONINTERLACED (/* non interlaced */)";              break;
        case FB_VMODE_INTERLACED:    str = "FB_VMODE_INTERLACED  (/* interlaced */)";                    break;
        case FB_VMODE_DOUBLE:        str = "FB_VMODE_DOUBLE (/* double scan */)";                        break;
        case FB_VMODE_ODD_FLD_FIRST: str = "FB_VMODE_ODD_FLD_FIRST  (/* interlaced: top line first */)"; break;
        case FB_VMODE_MASK:          str = "FB_VMODE_MASK  ";                                            break;
        default: str = (std::stringstream() << "unknown FB_VMODE_ " << vmode).str();
    }
    return str;
}

const std::string fb_rotate2str(uint32_t rotate) {
    std::string str;
    switch(rotate) {
        case FB_ROTATE_UR:  str = "FB_ROTATE_UR"; break;
        case FB_ROTATE_CW:  str = "FB_ROTATE_CW"; break;
        case FB_ROTATE_UD:  str = "FB_ROTATE_UD"; break;
        case FB_ROTATE_CCW: str = "FB_ROTATE_CCW"; break;
        default: str = (std::stringstream() << "unknown FB_ROTATE_ " << rotate).str();
    }
    return str;
}
