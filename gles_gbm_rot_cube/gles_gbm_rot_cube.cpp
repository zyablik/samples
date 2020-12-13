#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <gbm.h>

#include <drm/drm.h>
#include <drm/drm_mode.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#define GLM_FORCE_RADIANS

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.inl"

#if defined(STATIC_BUILD)
extern "C" {
    void * dlopen(const char *filename, int flags) {
        printf("dlopen filename = %s flags = 0x%x\n", filename, flags);
        return strdup(filename);
    }

    const void **__driDriverGetExtensions_i965();

    void *dlsym(void *handle, const char *symbol) {
        printf("dlsym handle = %s symbol = %s\n", (const char *)handle, symbol);
        if(strcmp(symbol, "__driDriverGetExtensions_i965") == 0)
            return (void *)__driDriverGetExtensions_i965;

        return NULL;
    }

    typedef struct {
        const char *dli_fname;  /* Pathname of shared object that
        void       *dli_fbase;  /* Base address at which shared
        const char *dli_sname;  /* Name of symbol whose definition
        void       *dli_saddr;  /* Exact address of symbol named */
    } Dl_info;

    int dladdr(void *addr, Dl_info *info) {
        printf("dladdr addr = %p info = %p\n", addr, info);
        memset(info, 0, sizeof(*info));
        return 0;
    }

    char *dlerror(void) {
        printf("dlerror\n");
        return NULL;
    }

    int dlclose(void *handle) {
        printf("dlclose handle = %s\n", (char *)handle);
        return 0;
    }
}
#endif

GLuint loadShader(GLenum shaderType, const char * source);
GLuint createProgram(const char * vertexShaderStr, const char * fragmentShaderStr);
static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE);
static void checkGlError(const char* op);

#define WIDTH 1920
#define HEIGHT 1080

int main(int argc, char ** argv) {
    setenv("EGL_LOG_LEVEL", "debug", 1);
    setenv("LIBGL_DEBUG", "verbose", 1);

    setenv("MESA_VERBOSE", "all", 1);
    setenv("MESA_DEBUG", "1", 1);

//    setenv("INTEL_DEBUG", "bat,vs,fs,color", 1);
    setenv("INTEL_DEBUG", "vs", 1);

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

//     drmModeFreeEncoder(encoder);
//     drmModeFreeConnector(connector);
//     drmModeFreeResources(resources);

    struct gbm_device * gbm_dev = gbm_create_device(dri_fd);
    printf("gbm_dev = %p\n", gbm_dev);

    EGLDisplay egl_display = EGL_NO_DISPLAY;

    printf("egl_display = %p\n", egl_display);
    const char * egl_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("egl extensions: %s\n", egl_extensions);

    if(strstr(egl_extensions, "EGL_EXT_platform_base")) {
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
        printf("eglGetPlatformDisplayEXT = %p\n", eglGetPlatformDisplayEXT);
        egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm_dev, NULL);
    }
    printf("egl_display = %p\n", egl_display);

//    egl_display = eglGetDisplay(gbm_dev);

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

    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   0x8,
        EGL_GREEN_SIZE, 0x8,
        EGL_BLUE_SIZE,  0x8,
        EGL_ALPHA_SIZE, 0x8,
        EGL_NONE
    };

    int n;
    EGLConfig egl_config;
    if (eglChooseConfig(egl_display, attribs, &egl_config, 1, &n) == EGL_FALSE) {
        printf("eglChooseConfig() error: 0x%x\n", eglGetError());
        exit(EXIT_FAILURE);
    }

    if(n == 0) {
        printf("eglChooseConfig() no appropriate configs found\n");
        exit(EXIT_FAILURE);
    }

    EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };

    EGLContext egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attribs);

    printf("egl_context = %p\n", egl_context);

    EGLint window_attribs[] = { EGL_NONE };

    struct gbm_surface * gbm_surf = gbm_surface_create(gbm_dev, mode_info.hdisplay, mode_info.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);

    EGLSurface egl_surface = eglCreateWindowSurface(egl_display, egl_config, (EGLNativeWindowType) gbm_surf, window_attribs);

    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    glViewport(0,  0,  WIDTH,  HEIGHT);

    GLfloat cube_vertices[] = {
        // front
        -1.0, -1.0,  1.0,
         1.0, -1.0,  1.0,
         1.0,  1.0,  1.0,
        -1.0,  1.0,  1.0,
        // back
        -1.0, -1.0, -1.0,
         1.0, -1.0, -1.0,
         1.0,  1.0, -1.0,
        -1.0,  1.0, -1.0,
    };

    GLfloat cube_colors[] = {
        // front colors
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
        1.0, 1.0, 1.0,
        // back colors
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
        1.0, 1.0, 1.0,
    };

    GLushort cube_elements[] = {
        // front
        0, 1, 2,
        2, 3, 0,
        // top
        1, 5, 6,
        6, 2, 1,
        // back
        7, 6, 5,
        5, 4, 7,
        // bottom
        4, 0, 3,
        3, 7, 4,
        // left
        4, 5, 1,
        1, 0, 4,
        // right
        3, 2, 6,
        6, 7, 3,
    };

    static const char vertexShaderStr[] = R"EOS(
        attribute vec3 coord3d;
        attribute vec3 v_color;
        uniform mat4 mvp;
        varying vec3 f_color;

        void main(void) {
            gl_Position = mvp * vec4(coord3d, 1.0);
            f_color = v_color;
        }
    )EOS";

    static const char fragmentShaderStr[] = R"EOS(
        precision mediump float;

        varying vec3 f_color;

        void main(void) {
            gl_FragColor = vec4(f_color.r, f_color.g, f_color.b, 1.0);
        }
    )EOS";

    // glEnable(GL_BLEND);
    // glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    // glDepthFunc(GL_LEQUAL);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glDepthMask(GL_TRUE);

    GLuint vbo_cube_vertices;
    glGenBuffers(1, &vbo_cube_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    GLuint vbo_cube_colors;
    glGenBuffers(1, &vbo_cube_colors);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_colors);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_colors), cube_colors, GL_STATIC_DRAW);

    GLuint ibo_cube_elements;
    glGenBuffers(1, &ibo_cube_elements);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cube_elements);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_elements), cube_elements, GL_STATIC_DRAW);

    GLuint program = createProgram(vertexShaderStr, fragmentShaderStr);
    printf("program = %d\n", program);

    GLuint attribute_coord3d = glGetAttribLocation(program, "coord3d");
    GLuint attribute_v_color = glGetAttribLocation(program, "v_color");
    GLuint uniform_mvp = glGetUniformLocation(program, "mvp");

    printf("attribute_coord3d = %d\n", attribute_coord3d);
    printf("attribute_v_color = %d\n", attribute_v_color);
    printf("uniform_mvp = %d\n", uniform_mvp);

    float angle = 0;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.0, -4.0));
    glm::mat4 view = glm::lookAt(glm::vec3(0.0, 2.0, 0.0), glm::vec3(0.0, 0.0, -4.0), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 projection = glm::perspective(45.0f, 1.0f * WIDTH / HEIGHT, 0.0f, 100.0f);

    glUseProgram(program);

    struct gbm_bo * previous_bo_1 = NULL;
    struct gbm_bo * previous_bo_2 = NULL;
    uint32_t previous_fb_1, previous_fb_2;

    while(true) {
        angle += 1;
        glm::vec3 axis_y(0, 1, 0);
        glm::mat4 anim = glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis_y);
        glm::mat4 mvp = projection * view * model * anim;

        glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT /* | GL_DEPTH_BUFFER_BIT */);

        glEnableVertexAttribArray(attribute_coord3d);
        // Describe our vertices array to OpenGL (it can't guess its format automatically)
        glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_vertices);
        glVertexAttribPointer(
                              attribute_coord3d, // attribute
                              3,                 // number of elements per vertex, here (x,y,z)
                              GL_FLOAT,          // the type of each element
                              GL_FALSE,          // take our values as-is
                              0,                 // no extra data between each position
                              0                  // offset of first element
                              );

        glEnableVertexAttribArray(attribute_v_color);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_colors);
        glVertexAttribPointer(
                              attribute_v_color, // attribute
                              3,                 // number of elements per vertex, here (R,G,B)
                              GL_FLOAT,          // the type of each element
                              GL_FALSE,          // take our values as-is
                              0,                 // no extra data between each position
                              0                  // offset of first element
                              );

        /* Push each element in buffer_vertices to the vertex shader */
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cube_elements);
        int size;
        glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

        glDrawElements(GL_TRIANGLES, size/sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

        glDisableVertexAttribArray(attribute_coord3d);
        glDisableVertexAttribArray(attribute_v_color);

        // swap
        printf("eglSwapBuffers() >>>\n");
        eglSwapBuffers(egl_display, egl_surface);
        printf("eglSwapBuffers() <<<\n");

        printf("before gbm_surface_lock_front_buffer\n");
        struct gbm_bo * bo = gbm_surface_lock_front_buffer(gbm_surf);

        uint32_t fb_id = (uintptr_t)gbm_bo_get_user_data(bo);
        printf("after gbm_surface_lock_front_buffer bo = %p fb_id = %d\n", bo, fb_id);

        printf("gbm_bo_get_handle(bo).u32 = %d\n", gbm_bo_get_handle(bo).u32);
        // print surface content
        // uint32_t stride;
        // void * map_data = nullptr;
        // void * ret = gbm_bo_map(bo, 0, 0, WIDTH, HEIGHT, 0, &stride, &map_data);
        // printf("gbm_bo_map ret = %p stride = %d map_data = %p\n", ret, stride, map_data);

        // for(int i = 0; i < HEIGHT; i+= 100) {
        //     printf("\ni = %d: \n", i);
        //     for(int j = 0; j < WIDTH; j++) {
        //         if(j % 20 == 0) printf("\n");
        //         printf("0x%08x ", ((uint32_t *)ret)[i * WIDTH + j]);
        //     }
        // }

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

//        gbm_surface_release_buffer(gbm_surf, bo);

        static size_t frame_counter = 0;
        static struct timeval last_time = { 0 };
        if(last_time.tv_sec == 0) 
            gettimeofday(&last_time, NULL);

        frame_counter++;
        const size_t nframes = 60;
        if(frame_counter % nframes == 0) {
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            printf("[pid = %d] fb fps = %2.1f\n", getpid(), nframes * 1000.0 / ((current_time.tv_sec - last_time.tv_sec) * 1000 + (current_time.tv_usec - last_time.tv_usec) / 1000));
            last_time = current_time;
        }
        //sleep(2);
    }
    return 0;
}

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

const char * egl_strerror(EGLint err) {
    switch (err){
        case EGL_SUCCESS:           return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:   return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:        return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:         return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:     return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONFIG:        return "EGL_BAD_CONFIG";
        case EGL_BAD_CONTEXT:       return "EGL_BAD_CONTEXT";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:       return "EGL_BAD_DISPLAY";
        case EGL_BAD_MATCH:         return "EGL_BAD_MATCH";
        case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_BAD_PARAMETER:     return "EGL_BAD_PARAMETER";
        case EGL_BAD_SURFACE:       return "EGL_BAD_SURFACE";
        case EGL_CONTEXT_LOST:      return "EGL_CONTEXT_LOST";
        default: return "UNKNOWN";
    }
}

static void checkEglError(const char* op, EGLBoolean returnVal) {
    if (returnVal != EGL_TRUE)
        fprintf(stderr, "%s() returned %d\n", op, returnVal);

    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error = eglGetError()) {
        fprintf(stderr, "after %s() eglError %s (0x%x)\n", op, egl_strerror(error), error);
        exit(EXIT_FAILURE);
    }
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        fprintf(stderr, "after %s() glError (0x%x)\n", op, error);
        exit(EXIT_FAILURE);
    }
}
