#include <stdio.h>
#include <unistd.h>

void sig_handler(int signo) {
    printf("sig_handler: signo = %d\n", signo);
}

int main(int, char *argv[]) {
    printf("hello from %s. sleep 30\n", argv[0]);
    const char str1[] = "17 22 0:15 / /sys rw,nosuid,nodev,noexec,relatime - sysfs sysfs rw\n";
    int n1, n2, n3, n4, n5;
    const char * buf, * buf3;
    char buf2[128];
    int ret = sscanf(str1, "%i %i %u:%u %ms %s  %ms %n", &n1, &n2, &n3, &n4, &buf, buf2, &buf3, &n5);
    printf("str1 = %s\n", str1);
    printf("ret = %d n1 = %d n2 = %d n3 = %d n5 = %d buf = '%s' buf2 = '%s' buf3 = '%s' n5 = %d\n", ret, n1, n2, n3, n4, buf, buf2, buf3, n5);
    
    const char str2[] = "92 54 0:17 / / ro,relatime shared:1 - ramfs rootfs ro,seclabel\n";
    printf("str2 = %s\n", str2);
    ret = sscanf(str2, "%i %i %u:%u %ms %s  %ms %n", &n1, &n2, &n3, &n4, &buf, buf2, &buf3, &n5);
    printf("ret = %d n1 = %d n2 = %d n3 = %d n5 = %d buf = '%s' buf2 = '%s' buf3 = '%s' n5 = %d\n", ret, n1, n2, n3, n4, buf, buf2, buf3, n5);

    sleep(30);
    printf("after sleep. exit\n");
    return 0;
}
