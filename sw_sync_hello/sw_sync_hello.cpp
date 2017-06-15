#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/sync.h>
#include <linux/sw_sync.h>

void pexit(const char * format, ...);
void wait4enter(const char * msg);

// from system/core/libsync
int sw_sync_timeline_create(void) {
    return open("/dev/sw_sync", O_RDWR);
}

void sw_sync_timeline_inc(int fd, unsigned count) {
    printf("sw_sync_timeline_inc(): fd = %d count = %d\n", fd, count);
    __u32 arg = count;
    if(ioctl(fd, SW_SYNC_IOC_INC, &arg) < 0)
        pexit("error while ioctl(SW_SYNC_IOC_INC) fd = %d count = %d\n", fd, count);
}

int sw_sync_fence_create(int fd, const char * name, unsigned value) {
    printf("sw_sync_fence_create(): fd = %d name = %s value = %d\n", fd, name, value);
    struct sw_sync_create_fence_data data;

    data.value = value;
    strlcpy(data.name, name, sizeof(data.name));

    if(ioctl(fd, SW_SYNC_IOC_CREATE_FENCE, &data) < 0)
        pexit("error while ioctl(SW_SYNC_IOC_CREATE_FENCE) fd = %d name = %s value = %d\n", fd, name, value);

    return data.fence;
}

int sync_wait(int fd, int timeout) {
    printf("sync_wait(): fd = %d timeout = %d millisecs\n", fd, timeout);
    __s32 to = timeout;
    return ioctl(fd, SYNC_IOC_WAIT, &to);
}

int sync_merge(const char * name, int fd1, int fd2) {
    printf("sync_merge(): name = %s fd1 = %d fd2 = %d\n", name, fd1, fd2);

    struct sync_merge_data data;
    data.fd2 = fd2;
    strlcpy(data.name, name, sizeof(data.name));

    if(ioctl(fd1, SYNC_IOC_MERGE, &data) < 0)
        pexit("error while ioctl(SYNC_IOC_MERGE) name = %s fd1 = %d fd2 = %d", name, fd1, fd2);

    return data.fence;
}

void print_sync_fence_info(int fd) {
    struct sync_fence_info_data * info = (struct sync_fence_info_data *)malloc(4096);
    info->len = 4096;

    if(ioctl(fd, SYNC_IOC_FENCE_INFO, info) < 0)
        pexit("error while ioctl(SYNC_IOC_FENCE_INFO) fd = %d", fd);
    printf("sync_fence_info: fd = %d name = %s status = %d(%s) len = %d\n", fd, info->name, info->status,
                                                                   info->status == 1 ? "signaled" : (info->status == 0 ? "active" : "error"), info->len);
    for(sync_pt_info * itr = (sync_pt_info *) info->pt_info;
        (__u8 *)itr - (__u8 *)info < info->len;
        itr = (sync_pt_info *) ((__u8 *)itr + itr->len))
    {
        printf("  sync_pt_info: obj_name = %s driver_name = %s status = %d(%s) timestamp_ns = %llu len = %u driver_data = %p\n",
               itr->obj_name, itr->driver_name, itr->status,
               itr->status == 1 ? "signaled" : (itr->status == 0 ? "active" : "error"),
               itr->timestamp_ns, itr->len, itr->driver_data);
    }

    free(info);
}

int main(int argc, char* argv[]) {
    printf("%s started\n", argv[0]);

    wait4enter("to create timeline");
    int sync_timeline_fd = sw_sync_timeline_create();

    wait4enter("to create single_fence");
    int single_fence_fd = sw_sync_fence_create(sync_timeline_fd, "single_fence", 2);
    print_sync_fence_info(single_fence_fd);
    
    wait4enter("to sync_wait(to = 1000)");
    sync_wait(single_fence_fd, 1000);
    print_sync_fence_info(single_fence_fd);

    wait4enter("to sw_sync_timeline_inc(2)");
    sw_sync_timeline_inc(sync_timeline_fd, 2);

    wait4enter("to sync_wait(to = 1000)");
    sync_wait(single_fence_fd, 1000);
    print_sync_fence_info(single_fence_fd);

    wait4enter("to create fences for merge");
    int first_fence_fd = sw_sync_fence_create(sync_timeline_fd, "first_fence", 3);
    print_sync_fence_info(first_fence_fd);

    int second_fence_fd = sw_sync_fence_create(sync_timeline_fd, "second_fence", 4);
    print_sync_fence_info(second_fence_fd);

    wait4enter("to merge fences");
    int merged_fence_fd = sync_merge("merged_fence", first_fence_fd, second_fence_fd);
    print_sync_fence_info(merged_fence_fd);

    sw_sync_timeline_inc(sync_timeline_fd, 1);

    wait4enter("to sync_wait(to = 1000)");
    sync_wait(merged_fence_fd, 1000);
    print_sync_fence_info(merged_fence_fd);

    wait4enter("to sw_sync_timeline_inc(1)");
    sw_sync_timeline_inc(sync_timeline_fd, 1);

    wait4enter("to sync_wait(to = 1000)");
    sync_wait(merged_fence_fd, 1000);
    print_sync_fence_info(merged_fence_fd);

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
