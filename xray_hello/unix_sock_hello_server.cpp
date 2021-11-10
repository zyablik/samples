#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    int listen_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    printf("created socket fd = %d\n", listen_sock_fd);

    const char * sock_name = "/tmp/hello-socket";

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    socklen_t addr_len = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_name);
    addr_len += sizeof(addr.sun_family);

    unlink(addr.sun_path);

    if (bind(listen_sock_fd, (const struct sockaddr *)&addr, addr_len) < 0) {
        printf("bind() socket '%s' error: %d:%s\n", sock_name, errno, strerror(errno));
        exit(1);
    }

    while(true) {
        if(listen(listen_sock_fd, 1) < 0) {
            printf("listen() socket '%s' error: %d:%s\n", sock_name, errno, strerror(errno));
            exit(1);
        }
        printf("wait for client connections\n");

        int client_sock_fd = accept(listen_sock_fd, NULL, NULL);
        if (client_sock_fd < 0) {
            printf("accept() listen_sock_fd = %d error: %d:%s ", listen_sock_fd, errno, strerror(errno));
            break;
        }
        printf("client connection accepted(): fd = %d\n", client_sock_fd);

        char request[256];
        int ret = recv(client_sock_fd, request, sizeof(request), 0);
        if(ret < 0) {
            printf("recv() error: %d:%s", errno, strerror(errno));
            exit(1);
        }
        if(ret == 0) {
            printf("client closed connection\n");
            close(client_sock_fd);
            continue;
        }
        printf("recv() %d bytes, request = '%s'\n", ret, request);

        printf("send reply to client_sock_fd = %d\n", client_sock_fd);
        ret = send(client_sock_fd, request, strlen(request) + 1, 0);
        if(ret < 0) {
            printf("send() error: %d:%s", errno, strerror(errno));
            exit(1);
        }
        printf("send() %d bytes\n", ret);
        printf("close client_sock_fd %d\n", client_sock_fd);
        close(client_sock_fd);
    }
    
    return 0;
}
