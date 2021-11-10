#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <iostream>

int main() {
    const char * sock_name = "/tmp/hello-socket";

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    socklen_t addr_len = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_name);
    addr_len += sizeof(addr.sun_family);

    while(true) {
        int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        printf("created socket fd = %d\n", sock_fd);

        std::string line;
        printf("press enter to connect to %s\n", addr.sun_path);
        std::getline(std::cin, line);

        int ret = connect(sock_fd, (const struct sockaddr *)&addr, addr_len);
        if(ret != 0) {
            printf("error while connect sock_fd '%d' to %s %d:%s", sock_fd, addr.sun_path, errno, strerror(errno));
            exit(1);
        }
        printf("connected sock_fd = %d to %s\n", sock_fd, addr.sun_path);

        printf("press enter to send request to fd = %d\n", sock_fd);
        std::getline(std::cin, line);
        static int counter = 0;
        counter++;

        char request[256];
        size_t len = sprintf(request, "hello-%d", counter);

        ret = send(sock_fd, request, len + 1, 0);

        if(ret < 0) {
            printf("send() error %d:%s\n", errno, strerror(errno));
        } else {
            printf("send() ret = %d\n", ret);
        }

        printf("press enter to recv response from fd = %d\n", sock_fd);
        std::getline(std::cin, line);

        char response[256];
        ret = recv(sock_fd, response, sizeof(response), 0);
        if(ret < 0) {
            printf("read() error: %d:%s\n", errno, strerror(errno));
        } else {
            printf("read(): response: '%s'\n", response);
        }

        printf("close(%d)\n", sock_fd);
        close(sock_fd);
        return 0;
    }
    return 0;
}
