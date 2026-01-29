#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define SYS_assign_ncores_to_group 467

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <group_id> <ncores>\n",
                argv[0]);
        return 1;
    }

    int group_id = atoi(argv[1]);
    int ncores   = atoi(argv[2]);

    long ret = syscall(SYS_assign_ncores_to_group,
                       ncores, group_id);

    if (ret < 0) {
        perror("assign_ncores_to_group syscall failed");
        return 1;
    }

    return 0;
}