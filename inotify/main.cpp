#include <sys/inotify.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <log/log.h>

/* size of the event structure, not counting name */
#define EVENT_SIZE  (sizeof (struct inotify_event))

/* reasonable guess as to size of 32 events */
#define BUF_LEN        (32 * (EVENT_SIZE + 16))

int main(int argc, char **argv) {
    if(argc != 2) {
       printf("usage: %s PATH\n", argv[0]);
       exit(-1);
    }
    ALOGD("%s %s\n", argv[0], argv[1]);
    int fd,wd,len,i;
    char buf[BUF_LEN];
    struct inotify_event *event;
    fd_set watch_set;

    fd = inotify_init();
    if (fd < 0) {
        perror("init failed");
        exit(EXIT_FAILURE);
    }

    wd = inotify_add_watch(fd, argv[1], IN_ALL_EVENTS);
    if (wd < 0) {
        perror("add watch failed");
        exit(EXIT_FAILURE);
    }

    /* put the file descriptor to the watch list for select() */
    FD_ZERO(&watch_set);
    FD_SET(fd,&watch_set);

    while(1) {
        select(fd+1, &watch_set, NULL,NULL,NULL);
        len = read(fd,buf,BUF_LEN);
        i=0;
        while(i < len) {
            event = (struct inotify_event *) &buf[i];
            printf("event->mask  = 0x%x event->name = %s\n", event->mask, event->name);
            ALOGD("inotify: event->mask  = 0x%x event->name = %s\n", event->mask, event->name);
            if ((event->mask & IN_CREATE) != 0) {
                printf ("%s created\n",event->name);
            }
            else if ((event->mask & IN_DELETE) != 0) {
                printf ("%s deleted\n",event->name);
            }
            else {
                printf ("wd=%d mask=0x%X cookie=%u len=%u name=%s\n",
                                event->wd, event->mask,
                                event->cookie, event->len, event->name);
            }

            i += EVENT_SIZE + event->len;

        }
    }
}

