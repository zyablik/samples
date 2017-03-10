#include <Grapevine/Vanda/Vanda.hpp>
#include <Grapevine/Vanda/BoundingVolumeComponent.hpp>
#include <Grapevine/Vanda/InputComponent.hpp>
#include <Grapevine/Vanda/GestureManager.hpp>
#include <Grapevine/Math/Vector.hpp>
#include <Grapevine/Core/Timer.hpp>
#include <stdlib.h>
#include <vector>

class MyApp: public Grapevine::Vanda::Application
{
public:
    MyApp(float x, float y, float z):
        Grapevine::Vanda::Application(x, y, z)
        , texture(16, 16, Grapevine::Vector4(255, 0, 0, 255))
        , transformation(-500, -500, 0)
        , render_thread(&MyApp::render_thread_func, this)
    {
        input_.connectTouch(Grapevine::Vanda::GestureManager::RAW, sigc::mem_fun(this, &MyApp::input));
        input_.connectTouch(Grapevine::Vanda::GestureManager::TAP, sigc::mem_fun(this, &MyApp::tapEvent));

        // places the mesh inside the volume
        // create the surface where texture is placed
        // Texture is the framebuffer place holder for your 2D app

        add(Node(transformation, Grapevine::Vanda::MeshComponent(256, 256), texture));

        add
        (
            Grapevine::Vanda::StateManagerComponent("ApplicationStateManager",
            {
                Grapevine::Vanda::State("uninitialized",
                    [](Node&, const Grapevine::Vanda::StateManagerEvent&) { printf("Application entered uninitialized state\n"); },
                    [](Node&, const Grapevine::Vanda::StateManagerEvent&) { printf("Application left uninitialized state\n"); },
                    {
                        Grapevine::Vanda::StateTransition("application.create", "running"),
                    }),

                Grapevine::Vanda::State("running",
                    [](Node&, const Grapevine::Vanda::StateManagerEvent&) { printf("Application entered running state\n"); },
                    [](Node&, const Grapevine::Vanda::StateManagerEvent&) { printf("Application left running state\n"); },
                    {
                        Grapevine::Vanda::StateTransition("application.pause", "paused"),
                        Grapevine::Vanda::StateTransition("application.destroy", "destroyed"),
                    }),

                Grapevine::Vanda::State("paused",
                    [](Node&, const Grapevine::Vanda::StateManagerEvent&) { printf("Application entered paused state\n"); },
                    [](Node&, const Grapevine::Vanda::StateManagerEvent&) { printf("Application left paused state\n"); },
                    {
                        Grapevine::Vanda::StateTransition("application.resume", "running"),
                        Grapevine::Vanda::StateTransition("application.destroy", "destroyed"),
                    }),

                Grapevine::Vanda::State("destroyed",
                    [](Node&, const Grapevine::Vanda::StateManagerEvent&) { printf("Application entered destroyed state\n"); },
                    [](Node&, const Grapevine::Vanda::StateManagerEvent&) { printf("Application left destroyed state\n"); },
                    {
                        // final state, no outgoing transitions
                    }),
            })
        );
    }

    void input(const vanda::InputEvent& inputEvent) override
    {
      printf("input\n");
        vanda::InputEvent::InputEventStruct event = inputEvent.eventData;

        if(event.type == vanda::InputEvent::InputEventStruct::TOUCH_DATA)
            printf("input, type TOUCH_DATA\n");

        if(event.type == vanda::InputEvent::InputEventStruct::KEY_DATA)
            printf("input, type KEY_DATA\n");
    }

    void tapEvent(vanda::InputEvent &inputEvent)
    {
        printf("tap event type = %d\n", inputEvent.eventData.type);
    }

    // Change the color in the texture, shm is locked during this call
    void textureUpdate(char* data)
    {
        uint32_t * tmp = (uint32_t*)data;

        unsigned int length = texture.height() * texture.width();
        for(unsigned int i = 0; i < length; i++) {
            tmp[i] = 0xff000000 + (i << 16) + 128;
        }
    }

    // Render sync related to compositor buffer swap.
    void render() override
    {
        printf("[tid = %d] render\n", gettid());
        // Use texture's udpate functionality to re-write the data  with a callback function
        // in separate texture shared memory block
//        texture.update(sigc::mem_fun(this, &MyApp::textureUpdate));
//        transformation.setLocation(Vector2(transformation.location().x() + 1, transformation.location().y() + 1));

    }

    void render_thread_func() {
        while(true) {
            sleep(1);
            printf("[tid = %d] render_thread_func after sleep\n", gettid());
            texture.update(sigc::mem_fun(this, &MyApp::textureUpdate));
            transformation.setLocation(Grapevine::Vector2(transformation.location().x() + 20, transformation.location().y() + 20));
            requestRender();
        }
    }

private:
    Grapevine::Vanda::TextureComponent texture;
    Grapevine::Vanda::TransformationComponent transformation;
    Grapevine::Vanda::InputComponent input_;
    std::thread render_thread;
};

int main(int, char **)
{
    MyApp app(10, 10, 10);
    return app.runLoop();
}
