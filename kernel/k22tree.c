#include <linux/k22info.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/list.h>
#include <linux/pid.h>

/* Summary of the Non-Recursive Pre-Order DFS algorithm 
* Variables: 
* curr: The current task that is being examined
* flag: Flag used to determine whether a tasks' children have been visited
* 
* curr <- init
* flag <- 0
* WHILE curr != init OR flag != 1:
*   IF curr does not have any children OR flag = 1:
*     IF curr does not have any new sibling
*       flag = 1
*       curr <- curr.parent
*     ELSE
*       flag = 0
*       curr <- curr.sibling
*   ELSE
*     process(curr) # Fill the k22info buffer and stuff
*     curr <- curr.child
*/

static void insert_proc(struct k22info* buf, struct task_struct* p, int i)
{
        snprintf(buf[i].comm, sizeof(buf[i].comm), "%s", p->comm);
        buf[i].pid = task_pid_nr(p);
        buf[i].parent_pid = task_pid_nr(p->parent);
        // TMP \/\/\/
        buf[i].first_child_pid = 0;
        buf[i].next_sibling_pid = 0;
        // TMP ^^^
        buf[i].nvcsw = p->nvcsw;
        buf[i].nivcsw = p->nivcsw;
        buf[i].start_time = p->start_time;
}

static int do_k22tree(struct k22info* buf, int* ne)
{
        int return_value;
        int i;
        int pnum;
        int flag;
        struct task_struct* p;
        struct k22info* proc_buf;
        rwlock_t mylock; // tmp name
        
        pnum = 0;
        for_each_process(p)
                ++pnum;
        
        proc_buf = kmalloc((pnum + 10) * sizeof(*buf), GFP_KERNEL);
        if (!proc_buf) {
                return_value = -ENOMEM;
                goto out;
        }
        flag = 0;
        p = &init_task;
        i = 0;

        rwlock_init(&mylock);

        read_lock(&mylock);

        do {
                //check pnum
                if (list_empty(&(p->children)) || (flag == 1)) {
                        if (task_pid_nr(list_next_entry(p, sibling)) == task_pid_nr(list_next_entry(p->parent, children))) {
                                flag = 1;
                                p = p->parent;
                        } else {
                                flag = 0;
                                p = list_next_entry(p, sibling);
                        }
                } else {
                        insert_proc(proc_buf, p, i);
                        ++i;
                        p = list_next_entry(p, children);
                }
        } while ((p != &init_task) || (flag != 1));

        read_unlock(&mylock);
        
        if (copy_to_user(buf, proc_buf, i*sizeof(*proc_buf))) { //Needs fixing on i 
                return_value = -EFAULT;
                goto free_pbuf;
        }
        //*ne = i;
        return_value = i;

free_pbuf:
        kfree(proc_buf);
out:
        return return_value;
}

SYSCALL_DEFINE2(k22tree, struct k22info __user *, buf, int __user *, ne)
{
        return do_k22tree(buf, ne);
}