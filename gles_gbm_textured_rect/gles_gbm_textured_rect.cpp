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

GLuint loadShader(GLenum shaderType, const char * source);
GLuint createProgram(const char * vertexShaderStr, const char * fragmentShaderStr);
static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE);
static void checkGlError(const char* op);

int main(int argc, char ** argv) {
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

    EGLDisplay egl_display = EGL_NO_DISPLAY;

    const char * egl_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("egl extensions: %s\n", egl_extensions);

    if(strstr(egl_extensions, "EGL_EXT_platform_base")) {
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
        egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm_dev, NULL);
    }
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

    EGLint window_attribs[] = { EGL_NONE };

    struct gbm_surface * gbm_surf = gbm_surface_create(gbm_dev, mode_info.hdisplay, mode_info.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);

    EGLSurface offscreen_egl_surface = eglCreateWindowSurface(egl_display, egl_config, (EGLNativeWindowType) gbm_surf, window_attribs);

    eglMakeCurrent(egl_display, offscreen_egl_surface, offscreen_egl_surface, egl_context);

    glViewport(0,  0,  1920,  1080);

    const GLfloat rect_positions[] = {
        // X, Y, Z, W
         0.0f, -1.0f, 0.0f, 1.0f,
        -1.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  0.0f, 0.0f, 1.0f,
         0.0f,  1.0f, 0.0f, 1.0f,
    };

    const GLfloat text_coords[] = {
        // U, V
        0.0, 0.0,
        0.0, 1.0,
        1.0, 0.0,
        1.0, 1.0,
    };

    static const char rectVertexShaderStr[] =
        "attribute vec2 aTexCoord;\n"
        "attribute vec4 aPosition;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "  gl_Position = aPosition;\n"
        "  vTexCoord = aTexCoord;\n"
        "}\n";

    static const char rectFragmentShaderStr[] =
        "precision mediump float;\n"
        "uniform sampler2D uSampler;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(uSampler, vTexCoord);\n"
        "}\n";

    GLuint rectProgram = createProgram(rectVertexShaderStr, rectFragmentShaderStr);
    printf("rectProgram = %d\n", rectProgram);

    GLuint textCoordHandle = glGetAttribLocation(rectProgram, "aTexCoord");
    GLuint positionHandle = glGetAttribLocation(rectProgram, "aPosition");

    GLuint samplerHandle = glGetUniformLocation(rectProgram, "uSampler");

    printf("positionHandle = %d aTexCoord = %d samplerHandle = %d\n", positionHandle, textCoordHandle, samplerHandle);

//    glClearColor(1.0, 0.0, 0.0, 0.0);
    glUseProgram(rectProgram);
//    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glVertexAttribPointer(positionHandle, 4 /* X, Y, Z, W = vec4*/, GL_FLOAT, GL_FALSE, 0, rect_positions);
    glEnableVertexAttribArray(positionHandle);

    glVertexAttribPointer(textCoordHandle, 2 /*U,V = vec2 */, GL_FLOAT, GL_FALSE, 0, text_coords);
    glEnableVertexAttribArray(textCoordHandle);

    uint8_t pixels[] = {
        0xff, 0x00, 0x00, // red
        0x00, 0xff, 0x00, // green
        0xff, 0xff, 0x00, // yellow
        0x00, 0x00, 0xff, // blue
    };

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // tightly packed data

    GLuint textureID;
    glGenTextures(1, &textureID);
    glGenTextures(1, &textureID);
    printf("textureID = %d\n", textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2 /* width */, 2 /* height */, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    printf("GL_TEXTURE0 = %d GL_TEXTURE1 = %d\n", GL_TEXTURE0, GL_TEXTURE1);

    // glActiveTexture(GL_TEXTURE0); // not working

    glUniform1i(samplerHandle, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, sizeof(rect_positions) / (sizeof(rect_positions[0]) * 4) );

//    glDisableVertexAttribArray(positionHandle);

// swap
    eglSwapBuffers(egl_display, offscreen_egl_surface);

    struct gbm_bo * bo = gbm_surface_lock_front_buffer(gbm_surf);

    uint32_t fb_id = (uintptr_t)gbm_bo_get_user_data(bo);
    if(fb_id == 0) {
        drmModeAddFB(dri_fd, mode_info.hdisplay, mode_info.vdisplay, 24, 32, gbm_bo_get_stride(bo), gbm_bo_get_handle(bo).u32, &fb_id);

        auto destroy_callback = [](struct gbm_bo * bo, void * data) {
            printf("destroy_callback bo = %p data = %p\n", bo, data);
            int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
            uint32_t fb_id = (uintptr_t) data;
            if (fb_id)
                drmModeRmFB(drm_fd, fb_id);
        };
        gbm_bo_set_user_data(bo, (void *)(uintptr_t)fb_id, destroy_callback);
    }

//        printf("drmModeSetCrtc fb_id = %d gbm_surface_has_free_buffers = %d\n", fb_id, gbm_surface_has_free_buffers(gbm_surf));

    drmModeSetCrtc(dri_fd, crtc->crtc_id, fb_id, 0, 0, &connector_id, 1, &mode_info);
    gbm_surface_release_buffer(gbm_surf, bo);
    sleep(3);
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
