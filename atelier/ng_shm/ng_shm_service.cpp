#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <ngShm.hpp>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <ui/GraphicBuffer.h>
#include <unistd.h>

class ngAbstractServiceObserverImpl: public ngAbstractServiceObserver {
public:
    ngAbstractServiceObserverImpl() {
        printf("[tid = %d] ngAbstractServiceObserverImpl::ngAbstractServiceObserverImpl() this = %p\n", gettid(), this);

        fb_dev_fd = open("/dev/graphics/fb0", O_RDWR);
        if (fb_dev_fd < 0)
            printf("unable to open /dev/graphics/fb0 error = %d: %s\n", errno, strerror(errno));

        if (ioctl(fb_dev_fd, FBIOGET_FSCREENINFO, &fix_info) < 0)
            printf("unable to retrieve fixed screen info: %d: %s\n", errno, strerror(errno));

        if (ioctl(fb_dev_fd, FBIOGET_VSCREENINFO, &var_info) < 0)
            printf("unable to retrieve variable screen info: %d: %s\n", errno, strerror(errno));

        size_t frame_size = fix_info.line_length * var_info.yres;
        mmapped_fb_mem = (uint32_t *) mmap(NULL, frame_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_dev_fd, 0);
        if (mmapped_fb_mem == NULL)
            printf("mmap failed: %d:%s\n", errno, strerror(errno));
 
        printf("framebuffer mmap address = %p\n", mmapped_fb_mem);

        var_info.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
        var_info.bits_per_pixel = 32;
        if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &var_info) < 0) {
            printf("unable to write back variable screen info: %d:%s\n", errno, strerror(errno));
            exit(1);
        }
    }

    int64_t readGraphicBuffer(void * buffer, pid_t pid) override {
        android::GraphicBuffer * graphic_buffer = (android::GraphicBuffer *) buffer;
        printf("[tid = %d] ngAbstractServiceObserverImpl::readGraphicBuffer buffer = %p [%d x %d] stride = %d pid = %d\n", gettid(), buffer,
               graphic_buffer->getWidth(), graphic_buffer->getHeight(), graphic_buffer->getStride(), pid);

        uint32_t * bits = nullptr;
        graphic_buffer->lock(android::GraphicBuffer::USAGE_SW_READ_OFTEN, (void**)&bits);
        memcpy(mmapped_fb_mem, bits, graphic_buffer->getWidth() * graphic_buffer->getHeight() * 4);
        graphic_buffer->unlock();

        if (ioctl(fb_dev_fd, FBIOPUT_VSCREENINFO, &var_info) < 0)
            printf("unable to write back variable screen info: %d:%s\n", errno, strerror(errno));

        printf("[tid = %d] ngAbstractServiceObserverImpl::readGraphicBuffer return 666\n", gettid());
        return 666;
    };

    int removeGraphicBuffer(int64_t eglImageId) override {
        printf("[tid = %d] ngAbstractServiceObserverImpl::removeGraphicBuffer eglImageId = %ld\n", gettid(), eglImageId);
        return 0;
    };

    int64_t createBufferQueue(int width, int height, pid_t pid) override {
        printf("[tid = %d] ngAbstractServiceObserverImpl::createBufferQueue width = %d height = %d, pid = %d\n", gettid(), width, height, pid);
        return 0;
    };

    int removeBufferQueue(int64_t bqId) override {
        printf("[tid = %d] ngAbstractServiceObserverImpl::removeBufferQueue bqId = %ld\n", gettid(), bqId);
        return 0;
    };

    void * getGraphicBufferProducer(int64_t bqId) override {
        printf("[tid = %d] ngAbstractServiceObserverImpl::getGraphicBufferProducer bqId = %ld\n", gettid(), bqId);
        return nullptr;
    };

private:
    int fb_dev_fd;
    uint32_t * mmapped_fb_mem;
    struct fb_fix_screeninfo fix_info;
    struct fb_var_screeninfo var_info;
};

int main(int, char* argv[]) {
    printf("[pid = %d] %s started\n", getpid(), argv[0]);
    ng_shm_start();
    ngAbstractServiceObserverImpl observer;
    ng_graphicbuffer_set_observer(&observer);
    android::IPCThreadState::self()->joinThreadPool(true);
    return 0;
}
