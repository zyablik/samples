#include "fps.h"
#include <ngShm.hpp>
#include <stdio.h>
#include <ui/GraphicBuffer.h>
#include <unistd.h>

int main(int, char* argv[]) {
    printf("[pid = %d] %s started\n", getpid(), argv[0]);
    if(!ng_shm_attach()) {
        printf("error in ng_shm_attach()\n");
        exit(1);
    }
    printf("attached to ng-shm-service\n");
    const int width = 800;
    const int height = 600;
    android::GraphicBuffer * buffer = new android::GraphicBuffer(width, height, android::PIXEL_FORMAT_RGBA_8888,
                                         android::GraphicBuffer::USAGE_SW_READ_OFTEN | android::GraphicBuffer::USAGE_SW_WRITE_OFTEN | android::GraphicBuffer::USAGE_HW_TEXTURE);
    printf("create buffer = %p [%d x %d] stride = %d\n", buffer, buffer->getWidth(), buffer->getHeight(), buffer->getStride());
    uint32_t * bits = NULL;
    buffer->lock(android::GraphicBuffer::USAGE_SW_WRITE_OFTEN, (void**)&bits);

    for(unsigned y = 0; y < buffer->getHeight(); ++y)
        for(unsigned x = 0; x < buffer->getWidth(); ++x)
            *(bits + y * buffer->getWidth() + x) = 0x00FFFF00;

    buffer->unlock();

    int64_t eglImageId = ng_send_graphicbuffer((void*) buffer, 0, 0);
    printf("ng_send_graphicbuffer eglImageId = %ld\n", eglImageId);
    
    printf("ng_wait_vsync loop:\n");
    while(true) {
        FPS fps;
        ng_request_vsync();
        int64_t vsync_timestamp, vsync_period;
        ng_wait_vsync(&vsync_timestamp, &vsync_period);
        printf("vsync_timestamp = %ld vsync_period = %ld\n", vsync_timestamp, vsync_period);
    }
    return 0;
}
