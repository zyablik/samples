#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/ion.h>

// https://lwn.net/Articles/480055/ The Android ION memory allocator
void pexit(const char * format, ...);
void wait4enter(const char * msg);

int main(int argc, char* argv[]) {
    printf("%s started\n", argv[0]);

    const char * sock_name = "/run/hello-socket";
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    socklen_t addr_len = snprintf(addr.sun_path, UNIX_PATH_MAX, "%s", sock_name);
    addr_len += sizeof(addr.sun_family);

    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    printf("created socket fd = %d\n", sock_fd);
    
    if(connect(sock_fd, (const struct sockaddr *)&addr, addr_len) != 0)
        pexit("error while connect sock_fd '%d' to %s", sock_fd, addr.sun_path);
    
    char* child_buf[100];
    struct iovec count_vec = {
        .iov_base = child_buf,
        .iov_len = sizeof(child_buf),
    };

    char buf[CMSG_SPACE(sizeof(int))];
    struct msghdr child_msg = {
        .msg_control = buf,
        .msg_controllen = sizeof(buf),
        .msg_iov = &count_vec,
        .msg_iovlen = 1,
    };

    if (recvmsg(sock_fd, &child_msg, 0) < 0)
        pexit("error while recvmsg(sock_fd = %d)", sock_fd);

    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&child_msg);
    if (cmsg == NULL) {
        printf("no cmsg rcvd in child\n");
        exit(1);
    }

    int ion_shared_fd = *(int*) CMSG_DATA(cmsg);
    int ion_shared_len = 1024 * 1024;

    printf("received ion_shared_fd = %d ion_shared_len = %d from server\n", ion_shared_fd, ion_shared_len);

    wait4enter("to mmap shared mem");

    void * ion_shared_mem = mmap(NULL, ion_shared_len, PROT_READ | PROT_WRITE, MAP_SHARED, ion_shared_fd, 0);
    if (ion_shared_mem == MAP_FAILED)
        pexit("error while mmap(ion_shared_fd) fd = %d length = %d", ion_shared_fd, ion_shared_len);

    printf("mmap(ion_shared_fd = %d) = %p\n", ion_shared_fd, ion_shared_mem);

    wait4enter("to read from shared mem");

    printf("ion_shared_mem = '%17s'\n", ion_shared_mem);

    wait4enter("to write to shared mem");

    printf("write 'hello from client' to shared memory\n");

    strcpy((char *)ion_shared_mem, "hello from client");

    wait4enter("to free resources");

    printf("munmap(ion_shared_mem)");
    if(munmap(ion_shared_mem, ion_shared_len) < 0)
        pexit("error while munmap(ion_shared_mem = %p)", ion_shared_mem);

    printf("close(ion_shared_fd)\n");
    if(close(ion_shared_fd) < 0)
        pexit("error while close(ion_shared_fd = %d)", ion_shared_fd);

    return 0;
}

void pexit(const char * format, ...) {
    va_list arglist;
    va_start(arglist, format);
    vprintf(format, arglist);
    va_end(arglist);
    printf(": errno = %d: %s. exit(1)\n", errno, strerror(errno));
    exit(1);
}

void wait4enter(const char * msg) {
    printf("press enter %s\n", msg);
    char tmp[32];
    gets(tmp);
}
