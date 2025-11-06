#include <linux/k22info.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
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
        buf[i].first_child_pid = task_pid_nr(list_first_entry(&p->children, struct task_struct, sibling));
        buf[i].next_sibling_pid = task_pid_nr(list_next_entry(p, sibling));
        buf[i].nvcsw = p->nvcsw;
        buf[i].nivcsw = p->nivcsw;
        buf[i].start_time = p->start_time;
}

// static struct task_struct* next_proc_child(struct task_struct* t)
// {
//         struct task_struct* i;
//         for(i = list_next_entry(t, children); !thread_group_leader(i);)
//                 i = list_next_entry(i, children);
//         return i;
// }

// static struct task_struct* next_proc_sibling(struct task_struct* t)
// {
//         struct task_struct* i;
//         for(i = list_next_entry(t, sibling); !thread_group_leader(i); i = list_next_entry(t, sibling));
//         return i;
// }

static int do_k22tree(struct k22info* buf, int* ne)
{
        int return_value;
        int i;
        int pnum;
        int flag;
        struct task_struct* p;
        struct k22info* proc_buf;
        
        pnum = 0;
        for_each_process(p)
                ++pnum;
        
        proc_buf = kmalloc((pnum + 15) * sizeof(*buf), GFP_KERNEL);
        if (!proc_buf) {
                return_value = -ENOMEM;
                goto out;
        }
        flag = 0;
        p = &init_task;
        i = 0;

        read_lock(&tasklist_lock);
        int dbg_count = 0;
        do {
                //check pnum
                if (!thread_group_leader(p)) {
                        pr_info("K22-TWOS | Not Group leader!\n");
                        break;
                }
                ++dbg_count;
                pr_info("K22-TWOS | Iter: %d, i: %d, PID: %d, Next child PID: %d, Next child prt: %p, Next sibling PID: %d, Flag: %d\n", dbg_count, i, task_pid_nr(p), task_pid_nr(list_first_entry(&p->children, struct task_struct, sibling)), (p->children).next,task_pid_nr(list_next_entry(p, sibling)), flag);
                if (list_empty(&(p->children)) || (flag == 1)) {
                        if (flag != 1)
                                insert_proc(proc_buf, p, i++);
                        if ((p->sibling).next == &(p->parent->children)) {
                                pr_info("K22-TWOS | Flag set to 1. Visiting parent\n");
                                flag = 1;
                                p = p->parent;
                        } else {
                                pr_info("K22-TWOS | Visiting next sibling\n");
                                flag = 0;
                                p = list_next_entry(p, sibling);
                        }
                } else {
                        insert_proc(proc_buf, p, i++);
                        pr_info("K22-TWOS | Inserted process. Visiting next child\n");
                        p = list_first_entry(&p->children, struct task_struct, sibling);
                }
        } while ((p != &init_task) || (flag != 1));

        read_unlock(&tasklist_lock);
        
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