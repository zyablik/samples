#include <Grapevine/Vanda/Vanda.hpp>
#include <Grapevine/Vanda/BoundingVolumeComponent.hpp>
#include <Grapevine/Vanda/InputComponent.hpp>
#include <Grapevine/Vanda/GestureManager.hpp>
#include <Grapevine/Math/Vector.hpp>
#include <Grapevine/Core/Timer.hpp>
#include <Grapevine/Vanda/ApplicationPrivate.hpp>
#include <appman/appman.h>

#include <stdlib.h>
#include <vector>
#include <fcntl.h>
#include <sys/mman.h>
extern "C" {
#define MALI_PRODUCT_ID_TMIX 1
// should math the values used for build libGLES_mali_v2.so (especially those which used in struct cctx_context)
#define MALI_USE_COMMON_GRAPHICS 1
#define MALI_USE_CL 1
#define MALI_USE_GLES 1
#define MALI_DEBUG 1
#include <egl/src/mali_egl_display.h>
#include <cctx/mali_cctx_structs.h>
}

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <gui/Surface.h>
#include <android/native_window.h>

#include <ngShm.hpp>

bool ng_graphicbuffer_is_observer_set();
void wait4enter(const char * msg);

#define GL_CHECK(x) x;
void asciiScreenshot(int width, int height){
  GLint readType, readFormat;
  GL_CHECK(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &readType));
  GL_CHECK(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &readFormat));
  printf("readType = 0x%x, readFormat = 0x%x\n", readType, readFormat);

  unsigned int bytesPerPixel = 0;
  switch(readType) {
    case GL_UNSIGNED_BYTE:
      switch(readFormat) {
        case GL_RGBA:            bytesPerPixel = 4; break;
        case GL_RGB:             bytesPerPixel = 3; break;
        case GL_LUMINANCE_ALPHA: bytesPerPixel = 2; break;
        case GL_ALPHA:
        case GL_LUMINANCE:       bytesPerPixel = 1; break;
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
  GL_CHECK(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
  int i, j;
  unsigned k;
  for(i = height; i >= 0; i-=20){
    for(j = 0; j < width; j+= 10){
      int color = 0;
      for(k = 0; k < bytesPerPixel; k++){
        color += pixels[(i * width + j) * bytesPerPixel + k];
      }
      if(color == 0)
        printf(" ");
      else if(color == 765)
        printf("=");
      else if(color > 0 && color < 128)
        printf(".");
      else if(color >= 128 && color < 256)
        printf("*");
      else if(color >= 256 && color < 256 + 128)
        printf("#");
      else if(color >= 256 + 128 && color < 512)
        printf("@");
      else if(color >= 512 && color < 512  + 128)
        printf("k");
      else if(color >= 512 + 128)
        printf("d");
    }
    printf("\n");
  }
  free(pixels);
}

const char * app_cbs[] = {
    "START",
    "RESUME",
    "PAUSE",
    "STOP",
    "TERMINATE",
};

class GlesApp: public Grapevine::Vanda::Application
{
public:
    GlesApp(uint16_t width, uint16_t height):
        Grapevine::Vanda::Application()
//        , texture(width, height, vanda::Texture::Format::BUFFERQUEUE)
        , texture(new Grapevine::Vanda::TextureComponent(width, height, vanda::Texture::Format::HWLAYER))
        , transformation()
    {
         appman::ApplicationContext * ac = appman::ApplicationContext::getInstance();
         ac->sigOnStateChange = [](int cbId){ printf("appman: sigOnStateChange: cbId = %d: %s\n", cbId, app_cbs[cbId]); };
         ac->sigOnAction = [](const appman::Action& action){ printf("appman: sigOnAction: action = %s\n", action.getAction().c_str()); return 0; };
         ac->sigOnBroadcast = [](const appman::Action& action){ printf("appman: sigOnBroadcast: action = %s\n", action.getAction().c_str()); return 0; };
         ac->sigOnError = [](int status, int id){ printf("appman: sigOnError: status = %d, id = %d\n", status, id); return 0; };

        transformation.setScale(Grapevine::Vector3(1, -1, 1));
        transformation.setLocation({ 0, 0, 0.0f });

        wait4enter("to add texture");

        Node node;
        node.add(transformation);
        node.add(*texture);
        node.add(Grapevine::Vanda::MeshComponent(width, height));

        add(node);

        wait4enter("to continue");
        
        add
        (
            Grapevine::Vanda::StateManagerComponent("ApplicationStateManager", {
                Grapevine::Vanda::State("uninitialized",
                        [](Node&, Grapevine::Vanda::StateManagerEvent const&) { printf("OpenGLES2App entered uninitialized state\n"); },
                        [](Node&, Grapevine::Vanda::StateManagerEvent const&) {printf("OpenGLES2App left uninitialized state\n"); },
                {
                        Grapevine::Vanda::StateTransition("application.create", "running"),
                }),

                Grapevine::Vanda::State("running",
                        [this](Node&, Grapevine::Vanda::StateManagerEvent const&) { printf("OpenGLES2App entered running state\n"); },
                        [](Node&, Grapevine::Vanda::StateManagerEvent const&) { printf("OpenGLES2App left running state\n"); },
                {
                        Grapevine::Vanda::StateTransition("application.pause", "paused"),
                        Grapevine::Vanda::StateTransition("application.destroy", "destroyed"),
                }),

                Grapevine::Vanda::State("paused",
                        [](Node&, Grapevine::Vanda::StateManagerEvent const&) { printf("OpenGLES2App entered paused state\n"); },
                        [](Node&, Grapevine::Vanda::StateManagerEvent const&) { printf("OpenGLES2App left paused state\n"); },
                {
                        Grapevine::Vanda::StateTransition("application.resume", "running"),
                        Grapevine::Vanda::StateTransition("application.destroy", "destroyed"),
                }),

                Grapevine::Vanda::State("destroyed",
                        [](Node&, Grapevine::Vanda::StateManagerEvent const&) { printf("OpenGLES2App entered destroyed state\n"); },
                {
                    // final state, no outgoing transitions
                }),
            })
        );

        init();
//        vanda::ClientApplication::volumeControl(vanda::MessageFactory::VolumeControl::Show);
   }

   void init() {
        initEGL();
        buildProgram();
        glUseProgram(program);
        
        glEnableVertexAttribArray(position);
    }


    // Render sync related to compositor buffer swap.
    void render() override {
        printf("press:\n enter to draw next frame\n k to kill gles\nr to run reinit\ni to run init\ns to skip frame\n");
        char buf[256];
        gets(buf);
        switch(buf[0]) {
            case 'k': {
                printf("kill all\n\n\n");
                std::vector<Node> nodes;
                children(nodes);
                nodes[0].remove<Grapevine::Vanda::TextureComponent>();
                printf("texture->pointer()->referenceCount() = %d\n", texture->pointer()->referenceCount());
                delete texture;

                vanda::ClientApplication::instance()->ClearPendingReleases();
                deinitEGL();
                android::IPCThreadState::expungeAllHandles();

//                vanda::ClientApplication::stopApplication();
                requestRender();
                return;
            }
            case 'r': {
                printf("call reinit()\n");
                reinit();
//                vanda::ClientApplication::instance()->handlePendingMessages();
                init();
//                requestRender();
//                return;
                break;
            }
            case 'i': {
                printf("call init()\n");
                init();
                requestRender();
                return;
            }
            case 's': {
                printf("skip frame\n");
                requestRender();
                return;
            }
            default: break;
        }

        printf("draw frame\n");
        static int frame = 0;
        frame++;
        frame += 10;

        glClearColor(std::sin(frame/97.f)*0.5 + 0.5, std::sin(frame/101.f)*0.5 + 0.5, std::sin(frame/103.f)*0.5 + 0.5, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        // generate triangle vertices
        float aspect = float(width) / height;

        GLfloat verts[6];
        verts[0] = cos(frame/20.f);
        verts[1] = sin(frame/20.f) * aspect;
        verts[2] = -cos(frame/40.f);
        verts[3] = sin(frame/40.f) * aspect;
        verts[4] = 0.f;
        verts[5] = 1.f;

        glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, verts);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        asciiScreenshot(1080, 1920);

        
        eglSwapBuffers(egl_display, egl_surface);

        requestRender();
    }

    void reinit() {
        printf("reinit tid = %d\n", gettid());

        ng_shm_detach();
        ng_shm_attach();

        printf("ng_graphicbuffer_is_observer_set = %d\n", ng_graphicbuffer_is_observer_set());

//        Grapevine::Vanda::ApplicationPrivate * app = dynamic_cast<Grapevine::Vanda::ApplicationPrivate *>(pointer());
//        app->clientApplication_.handleShellOpened();
//        app->context_->start();

        vanda::ClientApplication::sendShellMessage(vanda::MessageFactory::clientVolumeOpenCommand(0, 0, 0));
//        appman::ApplicationContext::getInstance()->start();

        vanda::ClientApplication::volumeControl(vanda::MessageFactory::VolumeControl::Resume);
        vanda::ClientApplication::volumeControl(vanda::MessageFactory::VolumeControl::Show);

        std::vector<Node> nodes;
        children(nodes);
        texture = new Grapevine::Vanda::TextureComponent(width, height, vanda::Texture::Format::HWLAYER);
        nodes[0].add(*texture);

        nodes[0].update(nodes[0].component<Grapevine::Vanda::MeshComponent>());
    }

    void initEGL() {
       const EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };

        EGLint attribList[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        printf("initEGL: eglGetDisplay() egl_display = %p\n", egl_display);

        int major, minor;
        EGLBoolean result = eglInitialize(egl_display, &major, &minor);
        printf("initEGL: eglInitialize() major = %d minor = %d\n", major, minor);

        cctx = cctx_get_default("gles-on-vanda-restore");
        cctx_release(cctx, "gles-on-vanda-restore"); // cctx_get_default() increases ref counter, so immaediately release it
        printf("initEGL: cctx_get_default() = %p cctxp_refcount = %d\n", cctx, cctx->cctxp_refcount.cutilsp_refcount.refcount.osup_internal_struct.val);

        EGLint numConfigs;
        EGLConfig config;
        eglChooseConfig(egl_display, attribs, &config, 1, &numConfigs);
        printf("initEGL: eglChooseConfig() config = %p\n", config);

        EGLint format;
        eglGetConfigAttrib(egl_display, config, EGL_NATIVE_VISUAL_ID, &format);
        printf("initEGL: eglGetConfigAttrib(EGL_NATIVE_VISUAL_ID) format = 0x%x\n", format);

        native_window_set_buffers_geometry(texture->surface().get(), 0, 0, format);

        egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, attribList);
        if(egl_context == EGL_NO_CONTEXT) {
            printf("initEGL: eglCreateContext() error: 0x%x\n", eglGetError());
            exit(1);
        }
        printf("initEGL: eglCreateContext() egl_context = %p\n", egl_context);

        egl_surface = eglCreateWindowSurface(egl_display, config, texture->surface().get(), NULL);
        if(egl_context == EGL_NO_SURFACE) {
            printf("initEGL: eglCreateContext() error: 0x%x\n", eglGetError());
            exit(1);
        }
        printf("initEGL: eglCreateWindowSurface() egl_surface = %p\n", egl_surface);

        result = eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
        if(result == EGL_FALSE) {
            printf("initEGL: eglMakeCurrent() error: 0x%x\n", eglGetError());
            exit(1);
        }

        eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &width);
        eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &height);

        printf("initEGL: egl_surface size: %ix%i\n", width, height);
    }

    void deinitEGL() {
        printf("deinitEGL\n");
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(egl_display, egl_surface);
        eglDestroyContext(egl_display, egl_context);
        EGLBoolean result;
        result = eglTerminate(egl_display);
        printf("eglTerminate result = %d\n", result);

        printf("cctx_get_default() = %p cctxp_refcount = %d\n", cctx, cctx->cctxp_refcount.cutilsp_refcount.refcount.osup_internal_struct.val);
        cctx_release(cctx, "gles-on-vanda-restore");
        printf("after cctx_release(): cctx_get_default() = %p cctxp_refcount = %d\n", cctx, cctx->cctxp_refcount.cutilsp_refcount.refcount.osup_internal_struct.val);
    }

    void resetTextures() {
        printf("resetTextures children().size() = %lu\n", children().size());
                printf("resetTextures children()[0].children().size() = %lu\n", children()[0].children().size());

        for(const Node& node : children()) {
            printf("before node %p component<Texture>.width = %d\n", &node, node.component<Grapevine::Vanda::TextureComponent>().width());
        }

        {
            std::vector<Node> nodes;
            children(nodes);

            printf("nodes[0].component<Grapevine::Vanda::TextureComponent>().pointer() = %p\n", nodes[0].component<Grapevine::Vanda::TextureComponent>().pointer());

            nodes[0].remove<Grapevine::Vanda::TextureComponent>();

            printf("texture->pointer()->referenceCount() = %d\n", texture->pointer()->referenceCount());
//            pointer()->ClearPendingReleases();
            delete texture;
            texture = new Grapevine::Vanda::TextureComponent(width, height, vanda::Texture::Format::HWLAYER);

            nodes[0].add(*texture);
        }

        for(const Node& node : children()) {
            printf("after node %p component<Texture>.width = %d\n", &node, node.component<Grapevine::Vanda::TextureComponent>().width());
        }
    }

    void buildProgram() {
        static const char *vshader =
            "attribute vec4 pos;\n"
            "void main() { gl_Position = pos; }\n";

        static const char *fshader =
            "precision mediump float;\n"
            "void main() { gl_FragColor = vec4(0.2, 0.7, 0.5, 1.0); }\n";

        program = glCreateProgram();

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(vertexShader, 1, &vshader, NULL);
        glShaderSource(fragmentShader, 1, &fshader, NULL);

        glCompileShader(vertexShader);
        glCompileShader(fragmentShader);

        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);

        glLinkProgram(program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        position = glGetAttribLocation(program, "pos");
    }

private:
    Grapevine::Vanda::TextureComponent * texture;
    Grapevine::Vanda::TransformationComponent transformation;
    
    cctx_context * cctx = nullptr;
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLContext egl_context = EGL_NO_CONTEXT;
    EGLSurface egl_surface = EGL_NO_SURFACE;

    GLuint program;
    GLuint position;

    int width;
    int height;
};

GlesApp * app;

void sig_handler(int signo) {
    printf("sig_handler signo = %d\n", signo);
    app->reinit();
}

int main(int, char **)
{
    signal(SIGUSR1, sig_handler);
    app = new GlesApp(1080, 1920);
    app->runLoop();
    printf("app->runLoop() exited\n");
//    wait4enter("app->runLoop() returned. press enter to run again()\n");
//    app->runLoop();
    return 0;
}

void wait4enter(const char * msg) {
    printf("press enter %s\n", msg);
    char tmp[32];
    gets(tmp);
}
