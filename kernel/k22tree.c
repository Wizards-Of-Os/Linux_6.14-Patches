// SPDX-License-Identifier: GPL-2.0
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

#define task_next_child(p) \
	list_first_entry(&p->children, struct task_struct, sibling)

static void insert_proc(struct k22info *buf, struct task_struct *p, int i)
{
	snprintf(buf[i].comm, sizeof(buf[i].comm), "%s", p->comm);
	buf[i].pid = task_pid_nr(p);
	buf[i].parent_pid = task_pid_nr(p->parent);
	buf[i].first_child_pid = task_pid_nr(task_next_child(p));
	buf[i].next_sibling_pid = task_pid_nr(list_next_entry(p, sibling));
	buf[i].nvcsw = p->nvcsw;
	buf[i].nivcsw = p->nivcsw;
	buf[i].start_time = p->start_time;
}

static int do_k22tree(struct k22info *buf, int *ne)
{
	int return_value;
	int i;
	int pnum;
	int flag;
	int buffer_size;
	int new_buffer_size;
	int dbg_count;
	int upbound;
	struct task_struct *p;
	struct k22info *proc_buf;

	if (!buf || !ne) {
		return_value = -EINVAL;
		goto out;
	}
	pnum = 0;
	for_each_process(p)
		++pnum;
	if (copy_from_user(&upbound, ne, sizeof(int))) {//Needs fixing on i
		return_value = -EFAULT;
		goto out;
	}
	buffer_size = (pnum <= upbound) ? pnum : upbound;
	proc_buf = kmalloc((buffer_size) * sizeof(*buf), GFP_KERNEL);
	if (!proc_buf) {
		return_value = -ENOMEM;
		goto out;
	}
	flag = 0;
	p = &init_task;
	i = 0;
	dbg_count = 0;
	read_lock(&tasklist_lock);
	do {
		pnum = 0;
		for_each_process(p)
			++pnum;
		new_buffer_size = (pnum <= upbound) ? pnum : upbound;
		if (new_buffer_size > buffer_size) {
			read_unlock(&tasklist_lock);
			buffer_size *= 2;
			kfree(proc_buf);
			proc_buf = kmalloc((buffer_size) * sizeof(*buf), GFP_KERNEL);
			if (!proc_buf) {
				return_value = -ENOMEM;
				goto out;
			}
			read_lock(&tasklist_lock);
		} else {
			break;
		}
	} while (1);
	do {
		++dbg_count;
		pr_info("K22-TWOS | Iter: %d, i: %d, PID: %d, Next child PID: %d, Next child prt:\
			 %p, Next sibling PID: %d, Flag: %d\n", dbg_count, i, task_pid_nr(p), \
			 task_pid_nr(task_next_child(p)), \
			 (p->children).next, task_pid_nr(list_next_entry(p, sibling)), flag);
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
			p = task_next_child(p);
		}
		if (i == upbound)
			break;
	} while ((p != &init_task) || (flag != 1));

	read_unlock(&tasklist_lock);
	if (copy_to_user(buf, proc_buf, i * sizeof(*proc_buf))) {//Needs fixing on i
		return_value = -EFAULT;
		goto free_pbuf;
	}
	return_value = pnum;
	if (copy_to_user(ne, &i, sizeof(i)))
		return_value = -EFAULT;

free_pbuf:
	kfree(proc_buf);
out:
	return return_value;
}

SYSCALL_DEFINE2(k22tree, struct k22info __user *, buf, int __user *, ne)
{
	return do_k22tree(buf, ne);
}
