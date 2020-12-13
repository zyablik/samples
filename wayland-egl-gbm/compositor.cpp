#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>

#include <poll.h>

#include <gbm.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <wayland-server.h>

#define WIDTH 1920
#define HEIGHT 1080

static char redraw_needed = 0;
EGLDisplay egl_display = EGL_NO_DISPLAY;

PFNEGLQUERYWAYLANDBUFFERWL eglQueryWaylandBufferWL;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

struct texture {
    GLuint identifier;
    int width, height;
};

struct client {
    struct wl_client *client;
    struct wl_resource *pointer;
    struct wl_resource *keyboard;
    struct wl_list link;
};
struct wl_list clients;

struct client * get_client(struct wl_client *_client) {
    struct client *client;
    wl_list_for_each (client, &clients, link) {
        if (client->client == _client) return client;
    }
    client = (struct client *)calloc(1, sizeof(struct client));
    client->client = _client;
    wl_list_insert (&clients, &client->link);
    return client;
}

struct surface {
    struct wl_resource *surface;
    struct wl_resource *buffer;
    struct wl_resource *frame_callback;
    int x, y;
    struct texture texture;
    struct client *client;
    struct wl_list link;
};

struct wl_list surfaces;
struct surface *moving_surface = NULL;
struct surface *active_surface = NULL;
struct surface *pointer_surface = NULL; // surface under the pointer

void deactivate_surface(struct surface * surface) {
    printf("deactivate_surface(): surface = %p\n", surface);
}

void activate_surface(struct surface * surface) {
    printf("activate_surface(): surface = %p\n", surface);
}

void delete_surface (struct wl_resource *resource) {
    struct surface *surface = (struct surface *)wl_resource_get_user_data (resource);
    wl_list_remove (&surface->link);
    if (surface == active_surface) active_surface = NULL;
    if (surface == pointer_surface) pointer_surface = NULL;
    free(surface);
}

// surface
void surface_destroy (struct wl_client *client, struct wl_resource *resource) {
    printf("%s(): client = %p resource = %p", __FUNCTION__, client, resource);
}

void surface_attach (struct wl_client *client, struct wl_resource *resource, struct wl_resource *buffer, int32_t x, int32_t y) {
    struct surface * surface = (struct surface *) wl_resource_get_user_data (resource);
    surface->buffer = buffer;
}

void surface_damage (struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {}

void surface_frame (struct wl_client *client, struct wl_resource *resource, uint32_t callback) {
    struct surface *surface = (struct surface *) wl_resource_get_user_data (resource);
    surface->frame_callback = wl_resource_create (client, &wl_callback_interface, 1, callback);
}

void surface_set_opaque_region (struct wl_client *client, struct wl_resource *resource, struct wl_resource *region) {}

void surface_set_input_region (struct wl_client *client, struct wl_resource *resource, struct wl_resource *region) {}

void texture_delete(struct texture* texture);
void texture_create_from_egl_image (struct texture *texture, int width, int height, EGLImage image);

void asciiScreenshot(int width, int height) {
    GLint readType, readFormat;
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &readType);
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &readFormat);
    printf("readType = 0x%x, readFormat = 0x%x\n", readType, readFormat);

    unsigned int bytesPerPixel = 0;
    switch(readType) {
        case GL_UNSIGNED_BYTE:
            switch(readFormat) {
                case GL_RGBA: bytesPerPixel = 4; break;
                case GL_RGB: bytesPerPixel = 3; break;
                case GL_LUMINANCE_ALPHA: bytesPerPixel = 2; break;
                case GL_ALPHA:
                case GL_LUMINANCE: bytesPerPixel = 1; break;
            }
        break;

        case GL_UNSIGNED_SHORT_4_4_4_4: // GL_RGBA format
        case GL_UNSIGNED_SHORT_5_5_5_1: // GL_RGBA format
        case GL_UNSIGNED_SHORT_5_6_5: // GL_RGB format
            bytesPerPixel = 2;
        break;
    }
    printf("bytesPerPixel = %d\n", bytesPerPixel);

    GLubyte *pixels = (GLubyte*) malloc(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    int i, j;
    unsigned k;
    for(i = height - 1; i >= 0; i-=20) {
        for(j = 0; j < width; j+= 10) {
            uint32_t color = 0;
            for(k = 0; k < bytesPerPixel; k++){
                color += pixels[(i * width + j) * bytesPerPixel + k];
            }
//             printf("color[%d, %d] = %d\n", i, j, color);
            if(color == 0)
                printf(" ");
            else if(color == 408)
                printf("=");
            else if(color > 0 && color < 128)
                printf(".");
            else if(color >= 128 && color < 256)
                printf("*");
            else if(color >= 256 && color < 256 + 128)
                printf("#");
            else if(color >= 256 + 128 && color < 512)
                printf("@");
            else if(color >= 512 && color < 512 + 128)
                printf("k");
            else if(color >= 512 + 128)
                printf("d");
        }
        printf("\n");
    }
    free(pixels);
}

void surface_commit(struct wl_client * client, struct wl_resource * resource) {
    struct surface * surface = (struct surface *) wl_resource_get_user_data(resource);
    EGLint texture_format;
    if (eglQueryWaylandBufferWL(egl_display, surface->buffer, EGL_TEXTURE_FORMAT, &texture_format)) {
        EGLint width, height;
        eglQueryWaylandBufferWL(egl_display, surface->buffer, EGL_WIDTH, &width);
        eglQueryWaylandBufferWL(egl_display, surface->buffer, EGL_HEIGHT, &height);
        printf("surface_commit width = %d height = %d\n", width, height);
        EGLAttrib attribs = EGL_NONE;
        EGLImage image = eglCreateImage(egl_display, EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL, surface->buffer, &attribs);
        texture_delete(&surface->texture);
        texture_create_from_egl_image(&surface->texture, width, height, image);
    }
    wl_buffer_send_release(surface->buffer);
    redraw_needed = 1;
}

void surface_set_buffer_transform (struct wl_client *client, struct wl_resource *resource, int32_t transform) {}

void surface_set_buffer_scale (struct wl_client *client, struct wl_resource *resource, int32_t scale) {}

static struct wl_surface_interface surface_interface = {
    &surface_destroy,
    &surface_attach,
    &surface_damage,
    &surface_frame,
    &surface_set_opaque_region,
    &surface_set_input_region,
    &surface_commit,
    &surface_set_buffer_transform,
    &surface_set_buffer_scale
};

// region
void region_destroy(struct wl_client *client, struct wl_resource *resource) {}

void region_add(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {}

void region_subtract(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {}

struct wl_region_interface region_interface = {
    region_destroy,
    region_add,
    region_subtract
};

void compositor_create_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    struct surface * surface = (struct surface *) calloc(1, sizeof(struct surface));
    surface->surface = (struct wl_resource *) wl_resource_create(client, &wl_surface_interface, 3, id);
    wl_resource_set_implementation(surface->surface, &surface_interface, surface, &delete_surface);
    surface->client = get_client(client);
    wl_list_insert (&surfaces, &surface->link);
}

void compositor_create_region(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    struct wl_resource *region = wl_resource_create (client, &wl_region_interface, 1, id);
    wl_resource_set_implementation (region, &region_interface, NULL, NULL);
}

struct wl_compositor_interface compositor_interface = {
    compositor_create_surface,
    compositor_create_region
};

void compositor_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    printf ("bind: compositor\n");
    struct wl_resource *resource = wl_resource_create (client, &wl_compositor_interface, 1, id);
    wl_resource_set_implementation (resource, &compositor_interface, NULL, NULL);
}

void texture_create_from_egl_image(struct texture *texture, int width, int height, EGLImage image) {
//     GLuint offscreen_framebuffer;
//     glGenFramebuffers(1, &offscreen_framebuffer);
//     glBindFramebuffer(GL_FRAMEBUFFER, offscreen_framebuffer);

    texture->width = width;
    texture->height = height;

    glActiveTexture(GL_TEXTURE1);

    glGenTextures(1, &texture->identifier);
    glBindTexture(GL_TEXTURE_2D, texture->identifier);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

//     glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->identifier, 0);
// 
//     GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//     printf("glCheckFramebufferStatusOES status = %d\n", status);
//     if(status != GL_FRAMEBUFFER_COMPLETE) {
//         printf("failed to make complete framebuffer object %x", status);
//     }
// 
//     asciiScreenshot(WIDTH, HEIGHT);

    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint createProgram(const char * vertexShaderStr, const char * fragmentShaderStr);

void texture_draw(struct texture * texture, GLuint rectProgram) {
    printf("texture_draw texture->identifier = %d\n", texture->identifier);

    if (!texture->identifier)
        return;

    const GLfloat rect_positions[] = {
        // X, Y, Z, W
        -1.0, -1.0, 0.0, 1.0,
        -1.0,  1.0, 0.0, 1.0,
         1.0, -1.0, 0.0, 1.0,
         1.0,  1.0, 0.0, 1.0,
    };

    const GLfloat text_coords[] = {
        // U, V
        0.0, 0.0,
        0.0, 1.0,
        1.0, 0.0,
        1.0, 1.0,
    };

    GLuint positionHandle = glGetAttribLocation(rectProgram, "aPosition");
    GLuint textCoordHandle = glGetAttribLocation(rectProgram, "aTexCoord");
    GLuint samplerHandle = glGetUniformLocation(rectProgram, "uSampler");

    printf("positionHandle = %d aTexCoord = %d samplerHandle = %d\n", positionHandle, textCoordHandle, samplerHandle);

    glClearColor(0, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glVertexAttribPointer(positionHandle, 4 /* X, Y, Z, W = vec4*/, GL_FLOAT, GL_FALSE, 0, rect_positions);
    glEnableVertexAttribArray(positionHandle);

    glVertexAttribPointer(textCoordHandle, 2 /*U,V = vec2 */, GL_FLOAT, GL_FALSE, 0, text_coords);
    glEnableVertexAttribArray(textCoordHandle);

    glBindTexture(GL_TEXTURE_2D, texture->identifier);

    glUniform1i(samplerHandle, 1); // GL_TEXTURE1

    glDrawArrays(GL_TRIANGLE_STRIP, 0, sizeof(rect_positions) / (sizeof(rect_positions[0]) * 4) );
}

void texture_delete(struct texture* texture) {
    if (texture->identifier)
        glDeleteTextures (1, &texture->identifier);
}

void draw(GLuint rectProgram) {
    struct surface *surface;
    wl_list_for_each_reverse (surface, &surfaces, link) {
        texture_draw(&surface->texture, rectProgram);

        if (surface->frame_callback) {
            struct timespec t;
            clock_gettime (CLOCK_MONOTONIC, &t);
            wl_callback_send_done (surface->frame_callback, t.tv_sec * 1000 + t.tv_nsec / 1000000);
            wl_resource_destroy (surface->frame_callback);
            surface->frame_callback = NULL;
        }
    }

    glFlush();
}

void eglErrorCallback(EGLenum error, const char *command, EGLint messageType, EGLLabelKHR threadLabel, EGLLabelKHR objectLabel, const char* message) {
    printf("%s: %x, With command %s, Type: %d, Thread: %s, Object: %s, Message: '%s'\n", __FUNCTION__, error, command, messageType, (const char *)threadLabel, (const char *)objectLabel, (const char *)message);
}

GLuint loadShader(GLenum shaderType, const char * source);

GLuint loadShader(GLenum shaderType, const char * source) {
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        printf("glCreateShader() shaderType = 0x%x error: 0x%x\n", shaderType, glGetError());
        exit(EXIT_FAILURE);
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        printf("glCompileShader() shaderType = 0x%x error: ", shaderType);
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen) {
            char buf[infoLen];
            glGetShaderInfoLog(shader, infoLen, NULL, buf);
            printf("%s\n", buf);
        } else {
            printf("\n");
        }
        exit(EXIT_FAILURE);
    }
    return shader;
}

GLuint createProgram(const char * vertexShaderStr, const char * fragmentShaderStr) {
    GLuint program = glCreateProgram();
    if (!program) {
        printf("glCreateProgram() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderStr);
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderStr);

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

    if(linkStatus != GL_TRUE) {
        printf("glLinkProgram() link error: ");
        GLint bufLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
        if (bufLength) {
             char buf[bufLength];
             glGetProgramInfoLog(program, bufLength, NULL, buf);
             printf("%s\n", buf);
        } else {
            printf("\n");
        }
        exit(EXIT_FAILURE);
    }
    return program;
}

int main(int argc, const char ** argv)
{
    printf("[pid = %d] %s >>>\n", getpid(), argv[0]);

    // enable egl debug
    EGLAttrib debugAttribs[] = { EGL_DEBUG_MSG_INFO_KHR, EGL_TRUE, EGL_NONE };
    PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR = (PFNEGLDEBUGMESSAGECONTROLKHRPROC) eglGetProcAddress("eglDebugMessageControlKHR");
    EGLint result = eglDebugMessageControlKHR(eglErrorCallback, debugAttribs);

    PFNEGLLABELOBJECTKHRPROC eglLabelObject = (PFNEGLLABELOBJECTKHRPROC) eglGetProcAddress("eglLabelObjectKHR");
    // Label for the rendering thread.
    eglLabelObject(NULL, EGL_OBJECT_THREAD_KHR, NULL, (EGLLabelKHR)"RenderThread");

    PFNEGLBINDWAYLANDDISPLAYWL eglBindWaylandDisplayWL = (PFNEGLBINDWAYLANDDISPLAYWL) eglGetProcAddress("eglBindWaylandDisplayWL");
    eglQueryWaylandBufferWL = (PFNEGLQUERYWAYLANDBUFFERWL) eglGetProcAddress("eglQueryWaylandBufferWL");
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
    printf("eglBindWaylandDisplayWL = %p eglQueryWaylandBufferWL = %p glEGLImageTargetTexture2DOES = %p\n", eglBindWaylandDisplayWL, eglQueryWaylandBufferWL, glEGLImageTargetTexture2DOES);

    wl_list_init(&clients);
    wl_list_init(&surfaces);

    static struct wl_display * display = wl_display_create();
    printf("display = %p\n", display);

    wl_display_add_socket(display, NULL);

    wl_global_create(display, &wl_compositor_interface, 3, NULL, &compositor_bind);

    const char * dri_path = "/dev/dri/card0";
    int dri_fd = open(dri_path, O_RDWR);
    if(dri_fd == -1) {
        printf("error while open %s: %d:%s\n", dri_path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    drmModeRes * resources = drmModeGetResources(dri_fd);
    if(!resources) {
        printf("drmModeGetResources failed: %d:%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("resources->count_connectors = %d\n", resources->count_connectors);

    drmModeConnector * connector = NULL;

    for(int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(dri_fd, resources->connectors[i]);
        // pick the first connected connector
        if (connector->connection == DRM_MODE_CONNECTED) {
            break;
        }
        drmModeFreeConnector(connector);
    }

    if (!connector) {
        printf("no connector found\n");
        exit(EXIT_FAILURE);
    }

    // save the connector_id
    uint32_t connector_id = connector->connector_id;
    // save the first mode
    drmModeModeInfo mode_info = connector->modes[0];

    printf ("resolution: %ix%i\n", mode_info.hdisplay, mode_info.vdisplay);
    // find an encoder
    drmModeEncoder * encoder = drmModeGetEncoder(dri_fd, connector->encoder_id);

    if (!encoder) {
        printf("no encoder found\n");
        exit(EXIT_FAILURE);
    }

    // find a CRTC
    drmModeCrtc * crtc = NULL;
    if (encoder->crtc_id)
        crtc = drmModeGetCrtc(dri_fd, encoder->crtc_id);

    if (!encoder) {
        printf("no crtc found\n");
        exit(EXIT_FAILURE);
    }

    struct gbm_device * gbm_dev = gbm_create_device(dri_fd);
    printf("gbm_dev = %p\n", gbm_dev);

    const char * egl_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("egl extensions: %s\n", egl_extensions);

    if(strstr(egl_extensions, "EGL_EXT_platform_base")) {
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
        egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm_dev, NULL);
    }

    if (egl_display == EGL_NO_DISPLAY) {
        printf("eglGetDisplay() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    int major, minor;
    int rc = eglInitialize(egl_display, &major, &minor);
    if (rc == EGL_FALSE) {
        printf("eglInitialize() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    eglBindWaylandDisplayWL(egl_display, display);

    EGLint attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };

    EGLConfig egl_config;
    EGLint num_config;
    eglChooseConfig(egl_display, attributes, &egl_config, 1, &num_config);
    printf("eglChooseConfig(): egl_config = %p num_config = %d\n", egl_config, num_config);

    EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };

    EGLContext egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attribs);
    printf("eglCreateContext(): egl_context = %p\n", egl_context);

    EGLint window_attribs[] = { EGL_NONE };
    struct gbm_surface * gbm_surf = gbm_surface_create(gbm_dev, WIDTH, HEIGHT, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);
    EGLSurface egl_surface = eglCreateWindowSurface(egl_display, egl_config, (EGLNativeWindowType) gbm_surf, window_attribs);
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    glViewport(0,  0,  WIDTH,  HEIGHT);

    static const char rectVertexShaderStr[] = R"EOS(
        attribute vec4 aPosition;
        attribute vec2 aTexCoord;

        varying vec2 vTexCoord;

        void main() {
          gl_Position = aPosition;
          vTexCoord = aTexCoord;
        }
    )EOS";

    static const char rectFragmentShaderStr[] = R"EOS(
        precision mediump float;

        uniform sampler2D uSampler;
        varying vec2 vTexCoord;

        void main() {
          gl_FragColor = texture2D(uSampler, vTexCoord);
        }
    )EOS";

    static GLuint rectProgram = 0;
    if(rectProgram == 0) {
        rectProgram = createProgram(rectVertexShaderStr, rectFragmentShaderStr);
        printf("rectProgram = %d\n", rectProgram);
    }
    glUseProgram(rectProgram);

    struct wl_event_loop *event_loop = wl_display_get_event_loop(display);
    int wayland_fd = wl_event_loop_get_fd(event_loop);
    printf("wayland_fd = %d\n", wayland_fd);

    struct gbm_bo * previous_bo_1 = NULL;
    struct gbm_bo * previous_bo_2 = NULL;
    uint32_t previous_fb_1, previous_fb_2;

    while (true) {
        wl_event_loop_dispatch(event_loop, 0);
        wl_display_flush_clients(display);
        if (redraw_needed) {
            draw(rectProgram);

            asciiScreenshot(WIDTH, HEIGHT);
            eglSwapBuffers(egl_display, egl_surface);

            printf("before gbm_surface_lock_front_buffer\n");
            struct gbm_bo * bo = gbm_surface_lock_front_buffer(gbm_surf);

            uint32_t fb_id = (uintptr_t)gbm_bo_get_user_data(bo);
            printf("after gbm_surface_lock_front_buffer bo = %p fb_id = %d\n", bo, fb_id);

            printf("gbm_bo_get_handle(bo).u32 = %d\n", gbm_bo_get_handle(bo).u32);
            // print surface content
//             uint32_t stride;
//             void * map_data = nullptr;
//             void * ret = gbm_bo_map(bo, 0, 0, WIDTH, HEIGHT, 0, &stride, &map_data);
//             printf("gbm_bo_map ret = %p stride = %d map_data = %p\n", ret, stride, map_data);
// 
//             for(int i = 0; i < HEIGHT; i+= 100) {
//                 printf("\ni = %d: \n", i);
//                 for(int j = 0; j < WIDTH; j++) {
//                     if(j % 20 == 0) printf("\n");
//                     printf("0x%08x ", ((uint32_t *)ret)[i * WIDTH + j]);
//                 }
//             }

            drmModeAddFB(dri_fd, mode_info.hdisplay, mode_info.vdisplay, 24, 32, gbm_bo_get_stride(bo), gbm_bo_get_handle(bo).u32, &fb_id);
            drmModeSetCrtc(dri_fd, crtc->crtc_id, fb_id, 0, 0, &connector_id, 1, &mode_info);

            auto destroy_callback = [](struct gbm_bo * bo, void * data) {
                printf("destroy_callback bo = %p data = %p\n", bo, data);
                int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
                uint32_t fb_id = (uintptr_t) data;
                if (fb_id)
                    drmModeRmFB(drm_fd, fb_id);
            };
            gbm_bo_set_user_data(bo, (void *)(uintptr_t)fb_id, destroy_callback);

            // printf("drmModeSetCrtc fb_id = %d gbm_surface_has_free_buffers = %d\n", fb_id, gbm_surface_has_free_buffers(gbm_surf));

            if (previous_bo_2) {
                drmModeRmFB(dri_fd, previous_fb_2);
                gbm_surface_release_buffer(gbm_surf, previous_bo_2);
            }

            previous_bo_2 = previous_bo_1;
            previous_fb_2 = previous_fb_1;

            previous_bo_1 = bo;
            previous_fb_1 = fb_id;

            redraw_needed = 0;
        } else {
            struct pollfd fds[1] = {{wayland_fd, POLLIN}};
            poll(fds, 1, -1);
        }
    }

    wl_display_destroy(display);

    printf("%s <<<\n", argv[0]);
    return 0;
}
