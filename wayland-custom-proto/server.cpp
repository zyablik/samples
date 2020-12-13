#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#include <wayland-server.h>
#include "gen/wcp-server-protocol.h"

static struct wl_display * display;

void wcp_callback_invoke(struct wl_client * client, struct wl_resource * resource, uint32_t magic_number_2) {
    uint32_t magic_number_1 = (uint32_t)(uintptr_t)wl_resource_get_user_data(resource);
    printf("wcp_callback_invoke client = %p resource = %p magic_number_1 = %d magic_number_2 = %d\n", client, resource, magic_number_1, magic_number_2);
    wcp_callback_send_invoked(resource, magic_number_1, magic_number_2);
}

static const struct wcp_callback_interface wcp_callback_impl = {
    wcp_callback_invoke
};

void wcp_callback_factory_create(struct wl_client * client, struct wl_resource * resource, uint32_t new_id, uint32_t magic_number) {
    printf("wcp_callback_factory_create client = %p resource = %p new_id = %d magic_number = %d\n", client, resource, new_id, magic_number);

    wl_resource * handle_resource = wl_resource_create(client, &wcp_callback_interface, wl_resource_get_version(resource), new_id);
    if (handle_resource == NULL) {
        wl_resource_post_no_memory(resource);
        return;
    }

    wl_resource_set_implementation(handle_resource, &wcp_callback_impl, (void*)(uintptr_t)magic_number, NULL);
}

static const struct wcp_callback_factory_interface wcp_callback_factory_impl = {
    wcp_callback_factory_create
};

static void wcp_callback_factory_unbind(struct wl_resource * resource) {
    printf("wcp_create_unbind resource = %p\n", resource);
}

static void wcp_callback_factory_bind(struct wl_client * client, void * data, uint32_t version, uint32_t id) {
    printf("wcp_callback_factory_bind client = %p data = %p version = %d id = %d\n", client, data, version, id);
    struct wl_resource * resource = wl_resource_create(client, &wcp_callback_factory_interface, version, id);
    if (resource == NULL) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &wcp_callback_factory_impl, NULL /*data*/, wcp_callback_factory_unbind);
}

int main(int argc, const char ** argv)
{
    printf("[pid = %d] %s >>>\n", getpid(), argv[0]);

    display = wl_display_create();
    printf("display = %p\n", display);

    wl_display_add_socket(display, "wayland-socket"); // /${XDG_RUNTIME}/wayland-socket file

    wl_global_create(display, &wcp_callback_factory_interface, 7, NULL /*data*/, wcp_callback_factory_bind);

  	struct wl_event_loop *event_loop = wl_display_get_event_loop(display);
	int wayland_fd = wl_event_loop_get_fd(event_loop);
    printf("wayland_fd = %d\n", wayland_fd);
	while (true) {
		wl_event_loop_dispatch(event_loop, 0);
		wl_display_flush_clients(display);
        struct pollfd fds[1] = {{wayland_fd, POLLIN}};
        poll(fds, 1, -1);
    }

    printf("%s <<<\n", argv[0]);
    return 0;
}

