#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_posix.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/unix_domain_socket_util.h"

int setup_client_socket(const char * socket_file_path) {
    int socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(socket_fd <= 0) {
        cout << " socket(PF_UNIX, SOCK_STREAM, 0) failed errno = " << errno << ": " << strerror(errno) << endl;
        return -1;
    }

    if(fcntl(socket_fd, F_SETFL, O_NONBLOCK) != 0) {
        cout << "fcntl(" << socket_fd << ", F_SETFL, O_NONBLOCK) failed: " << strerror(errno) << endl;
        return -1;
    }

    struct sockaddr_un server_address = { 0 };
    server_address.sun_family = AF_UNIX;
    int path_len = snprintf(server_address.sun_path, IPC::kMaxSocketNameLength, "%s", socket_file_path);
    int server_address_len = offsetof(struct sockaddr_un, sun_path) + path_len + 1;
    if(connect(socket_fd, reinterpret_cast<struct sockaddr *>(&server_address), server_address_len) < 0) {
        cout << "connect(" << socket_fd << ") " << server_address.sun_path << " error: " << errno << " : " << strerror(errno) << endl;
        return -1;
    }
    return socket_fd;
}