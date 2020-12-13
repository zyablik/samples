#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <EGL/egl.h>
// #define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <wayland-client.h>
#include <wayland-egl-core.h>

static struct wl_compositor * compositor;
static struct wl_shell * shell;

void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    printf("Got a registry event for %s id %d\n", interface, id);

    if (strcmp(interface, "wl_compositor") == 0)
        compositor = (wl_compositor *) wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    else if (!strcmp(interface,"wl_shell"))
        shell = (wl_shell *) wl_registry_bind(registry, id, &wl_shell_interface, 1);
}

void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
    printf("Got a registry losing event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};

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

GLuint loadShader(GLenum shaderType, const char * source);
GLuint createProgram(const char * vertexShaderStr, const char * fragmentShaderStr);

void eglErrorCallback(EGLenum error, const char *command, EGLint messageType, EGLLabelKHR threadLabel, EGLLabelKHR objectLabel, const char* message) {
    printf("%s: %x, With command %s, Type: %d, Thread: %s, Object: %s, Message: '%s'\n", __FUNCTION__, error, command, messageType, (const char *)threadLabel, (const char *)objectLabel, (const char *)message);
}

#define WIDTH 1920
#define HEIGHT 1080

int main(int argc, const char ** argv)
{
    printf("[pid = %d] %s >>>\n", getpid(), argv[0]);

    struct wl_display * display = wl_display_connect(NULL);
    printf("display = %p\n", display);

    struct wl_registry * registry = wl_display_get_registry(display);
    printf("registry = %p\n", display);

    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    printf("compositor = %p\n", compositor);

// enable egl debug
    EGLAttrib debugAttribs[] = { EGL_DEBUG_MSG_INFO_KHR, EGL_TRUE, EGL_NONE };
    PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR = (PFNEGLDEBUGMESSAGECONTROLKHRPROC) eglGetProcAddress("eglDebugMessageControlKHR");
    EGLint result = eglDebugMessageControlKHR(eglErrorCallback, debugAttribs);

    PFNEGLLABELOBJECTKHRPROC eglLabelObject = (PFNEGLLABELOBJECTKHRPROC) eglGetProcAddress("eglLabelObjectKHR");
    // Label for the rendering thread.
    eglLabelObject(NULL, EGL_OBJECT_THREAD_KHR, NULL, (EGLLabelKHR)"RenderThread");

// create egl_display
    EGLDisplay egl_display = EGL_NO_DISPLAY;

    const char * egl_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("egl extensions: %s\n", egl_extensions);

    if(strstr(egl_extensions, "EGL_EXT_platform_base")) {
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
        egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_KHR, display, NULL);
    }
    printf("eglGetDisplay(): egl_display = %p\n", egl_display);

    int major, minor;
    int rc = eglInitialize(egl_display, &major, &minor);
    printf("eglInitialize(): major = %d minor = %d\n", major, minor);

    EGLint attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_config;
    eglChooseConfig(egl_display, attributes, &config, 1, &num_config);
    printf("eglChooseConfig(): config = %p num_config = %d\n", config, num_config);

    EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };

    EGLContext egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, context_attribs);
    printf("eglCreateContext(): egl_context = %p\n", egl_context);

    struct wl_surface * surface = wl_compositor_create_surface(compositor);
    printf("wl_compositor_create_surface(): surface = %p\n", surface);

    if(shell) {
        wl_shell_surface * shell_surface = wl_shell_get_shell_surface(shell, surface);
        wl_shell_surface_set_toplevel(shell_surface);
    }

    struct wl_egl_window * egl_window = wl_egl_window_create(surface, WIDTH, HEIGHT);
    printf("wl_egl_window_create(): egl_window = %p\n", egl_window);

    EGLSurface egl_surface = eglCreateWindowSurface(egl_display, config, (EGLNativeWindowType)egl_window, NULL);
    printf("eglCreateWindowSurface(): egl_window = %p\n", egl_window);

    eglMakeCurrent (egl_display, egl_surface, egl_surface, egl_context);

    eglSwapInterval(egl_display, 0);

    glViewport(0,  0,  WIDTH,  HEIGHT);

    const GLfloat triangle[] = {
       -0.5f, -0.5f, 0.0f, 1.0f,
        0.0f,  0.5f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f
    };

    const GLfloat color_triangle[] = {
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 0.0f
    };

    GLint num_vertex = 3;

    static const char triangleVertexShaderStr[] =
        "attribute vec4 vPosition;\n"
        "attribute vec4 vColor;\n"
        "uniform float angle;\n"
        "varying vec4 colorinfo;\n"
        "void main() {\n"
        "  mat3 rot_z = mat3( vec3( cos(angle),  sin(angle), 0.0),\n"
        "                     vec3(-sin(angle),  cos(angle), 0.0),\n"
        "                     vec3(       0.0,         0.0, 1.0));\n"
        "  gl_Position = vec4(rot_z * vPosition.xyz, 1.0);\n"
        "  colorinfo = vColor;\n"
        "}\n";

    static const char triangleFragmentShaderStr[] =
        "precision mediump float;\n"
        "varying vec4 colorinfo;\n"
        "void main() {\n"
        "  gl_FragColor = colorinfo;\n"
        "}\n";

    GLuint triangle_program = createProgram(triangleVertexShaderStr, triangleFragmentShaderStr);

    GLuint position_handle = glGetAttribLocation(triangle_program, "vPosition");
    GLuint color_handle = glGetAttribLocation(triangle_program, "vColor");
    GLuint rotation_uniform = glGetUniformLocation(triangle_program, "angle");
    printf("position_handle = %d color_handle = %d rotation_uniform = %d\n", position_handle, color_handle, rotation_uniform);

    glClearColor(0.1, 0.5, 0.0, 0.0);
    glUseProgram(triangle_program);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    GLfloat rotation_angle = 0.0f;

    size_t frame_counter = 0;
    size_t nframes = 60;
    struct timeval last_time;
    gettimeofday(&last_time, nullptr);

    while(true) {
        wl_display_dispatch_pending(display);
        sleep(3);

        frame_counter++;
        if(frame_counter % nframes == 0) {
            struct timeval current_time;
            gettimeofday(&current_time, nullptr);
            printf("[pid = %d] fb fps = %2.1f\n", getpid(), nframes * 1000.0 / ((current_time.tv_sec - last_time.tv_sec) * 1000 + (current_time.tv_usec - last_time.tv_usec) / 1000));
            last_time = current_time;
        }

        rotation_angle += 0.3;

// draw
        glClear(GL_COLOR_BUFFER_BIT /* | GL_DEPTH_BUFFER_BIT */);

        glUniform1fv(rotation_uniform, 1, &rotation_angle);

        glVertexAttribPointer(position_handle, num_vertex, GL_FLOAT, GL_FALSE, 0, triangle);
        glEnableVertexAttribArray(position_handle);

        glVertexAttribPointer(color_handle, num_vertex, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, color_triangle);
        glEnableVertexAttribArray(color_handle);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, num_vertex);

        glDisableVertexAttribArray(position_handle);
        glDisableVertexAttribArray(color_handle);
// swap
        asciiScreenshot(WIDTH, HEIGHT);
        eglSwapBuffers(egl_display, egl_surface);
    }

    wl_display_disconnect(display);

    printf("%s <<<\n", argv[0]);
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
