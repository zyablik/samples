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

int main(int argc, char* argv[]) {
    char tmp[32];
    printf("%s started\n", argv[0]);

    const char * ion_dev_path = "/dev/ion";
    int ion_dev_fd = open(ion_dev_path, O_RDONLY);
    if (ion_dev_fd < 0)
        pexit("error while open %s", ion_dev_path);

    printf("open('%s') ion_dev_fd = %d\n", ion_dev_path, ion_dev_fd);

    struct ion_allocation_data ion_alloc_data = {
        .len = 1024 * 1025,
        .align = 0,
        .heap_id_mask = ION_HEAP_TYPE_SYSTEM_CONTIG,
        .flags = 0
    };

    if(ioctl(ion_dev_fd, ION_IOC_ALLOC, &ion_alloc_data) < 0)
        pexit("error while ioctl(ION_IOC_ALLOC)");

    printf("ioctl(ION_IOC_ALLOC): ion_user_handle = %d\n", ion_alloc_data.handle);

    struct ion_fd_data ion_share_data = {
        .handle = ion_alloc_data.handle,
    };

    if(ioctl(ion_dev_fd, ION_IOC_SHARE, &ion_share_data) < 0)
        pexit("error while ioctl(ION_IOC_SHARE)");

    if(ion_share_data.fd < 0) {
        printf("ioctl(ION_IOC_SHARE): error: ion_share_data.fd = %d\n", ion_share_data.fd);
        exit(1);
    }

    void * ion_shared_mem = mmap(NULL, ion_alloc_data.len, PROT_READ | PROT_WRITE, MAP_SHARED, ion_share_data.fd, 0);
    if (ion_shared_mem == MAP_FAILED)
        pexit("error while mmap(ion_share_data.fd) fd = %d length = %d", ion_share_data.fd, ion_alloc_data.len);

    printf("mmap(ion_share_data.fd = %d) = %p\n", ion_share_data.fd, ion_shared_mem);

    strcpy((char *)ion_shared_mem, "hello from server");
    printf("ion_shared_mem = '%17s'\n", ion_shared_mem);

// -------------------------------------------------------------------------
// wait for client connection and send shared_fd of ion mem to it    
    const char * sock_name = "/run/hello-socket";
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    socklen_t addr_len = snprintf(addr.sun_path, UNIX_PATH_MAX, "%s", sock_name);
    addr_len += sizeof(addr.sun_family);

    int listen_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    printf("created socket fd = %d\n", listen_sock_fd);

    unlink(addr.sun_path);

    if (bind(listen_sock_fd, (const struct sockaddr *)&addr, addr_len) < 0)
        pexit("error while bind() socket '%s'", sock_name);

    if(listen(listen_sock_fd, 1) < 0)
        pexit("error while listen() socket '%s'", sock_name);

    printf("wait for client connections\n");

    int client_sock_fd = accept(listen_sock_fd, NULL, NULL);
    if (client_sock_fd < 0)
        pexit("error in accept() listen_sock_fd = %d", listen_sock_fd);

    printf("client connection accepted(): fd = %d\n", client_sock_fd);

    int num_fd = 1;
    struct iovec count_vec = {
        .iov_base = &num_fd,
        .iov_len = sizeof(num_fd),
    };
    char buf[CMSG_SPACE(sizeof(int) * 2)];

    struct msghdr msg = {
        .msg_control = buf,
        .msg_controllen = sizeof(buf),
        .msg_iov = &count_vec,
        .msg_iovlen = 1,
    };

    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int *)CMSG_DATA(cmsg) = ion_share_data.fd;

    printf("send ion_share_fd = %d to client\n", *(int *)CMSG_DATA(cmsg));
    if(sendmsg(client_sock_fd, &msg, 0) < 0)
        pexit("error while sendmsg(client_sock_fd) fd = %d\n", client_sock_fd);

    printf("press enter to read from shared mem\n");
    gets(tmp);

    printf("ion_shared_mem = '%17s'\n", ion_shared_mem);

    printf("press enter to free resources\n");
    gets(tmp);

    close(client_sock_fd);

    struct ion_handle_data ion_free_data = {
        .handle = ion_alloc_data.handle,
    };

    printf("ioctl(ION_IOC_FREE)\n");
    if(ioctl(ion_dev_fd, ION_IOC_FREE, &ion_free_data) < 0)
        pexit("error while ioctl(ION_IOC_FREE, ion_free_data.handle = %d)", ion_free_data.handle);

    printf("close(ion_dev_fd)\n");
    if(close(ion_dev_fd) < 0)
        pexit("error while close(ion_dev_fd = %d)", ion_dev_fd);

    printf("munmap(ion_shared_mem)\n");
    if(munmap(ion_shared_mem, ion_alloc_data.len) < 0)
        pexit("error while munmap(ion_shared_mem = %p)", ion_shared_mem);

    if(close(ion_share_data.fd) < 0)
        pexit("error while close(ion_share_data.fd = %d)", ion_share_data.fd);

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
