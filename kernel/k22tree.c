#include <linux/k22info.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/list.h>

static int do_k22tree(struct k22info* buf, int* ne)
{
        return 0;
}

SYSCALL_DEFINE2(k22tree, struct k22info __user *, buf, int __user *, ne)
{
        return do_k22tree(buf, ne);
}