#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <wayland-client.h>
#include "gen/wcp-client-protocol.h"

struct wl_display * display = NULL;
struct wcp_callback_factory * factory = NULL;

void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    printf("Got a registry event for %s id %d\n", interface, id);
    if (strcmp(interface, "wcp_callback_factory") == 0)
        factory = (wcp_callback_factory *) wl_registry_bind(registry, id, &wcp_callback_factory_interface, 7);
}

void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
    printf("Got a registry losing event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};

void wcp_callback_invoked(void * data, struct wcp_callback * callback, uint32_t magic_number_1, uint32_t magic_number_2) {
    printf("wcp_callback_invoked data = %p callback = %p magic_number_1 = %d magic_number_2 = %d\n", data, callback, magic_number_1, magic_number_2);
    wcp_callback_destroy(callback);
}

static const struct wcp_callback_listener listener = {
    wcp_callback_invoked
};

int main(int argc, const char ** argv)
{
    printf("[pid = %d] %s >>>\n", getpid(), argv[0]);

    struct wl_display * display = wl_display_connect("wayland-socket");
    printf("display = %p\n", display);

    struct wl_registry * registry = wl_display_get_registry(display);
    printf("registry = %p\n", display);

    wl_registry_add_listener(registry, &registry_listener, NULL);

	wl_display_dispatch(display);
    wl_display_roundtrip(display);

    struct wcp_callback * callback = wcp_callback_factory_create(factory, 666);
    wcp_callback_add_listener(callback, &listener, NULL);

    printf("callback = %p\n", callback);

    wcp_callback_invoke(callback, 777);

    wl_display_dispatch(display);

    // while(true) {
    //     printf("wl_display_dispatch\n");
    //     wl_display_dispatch(display);
    // }

    sleep(2);
    wl_display_disconnect(display);

    printf("%s <<<\n", argv[0]);
    return 0;
}

