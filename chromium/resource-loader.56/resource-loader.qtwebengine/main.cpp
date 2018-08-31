#include <stdio.h>

int resourse_loader_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    printf("[%s] main >>>", argv[0]);
    int ret = resourse_loader_main(argc, argv);
    printf("[%s] main ret = %d <<<", argv[0], ret);
    return ret;
}

