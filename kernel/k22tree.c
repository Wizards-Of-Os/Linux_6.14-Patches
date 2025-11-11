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
#include <linux/sort.h>

#define is_sibling(p1, p2) \
(parent_pid(p1) == parent_pid(p2))

#define is_child(p1, p2) \
(task_pid_nr(p1) == parent_pid(p2))

static int cmp_children(const void *ch1, const void *ch2)
{
	const struct task_struct *task1 = *(const struct task_struct **)ch1;
	const struct task_struct *task2 = *(const struct task_struct **)ch2;

	if (task1->start_time > task2->start_time)
		return -1;
	if (task1->start_time < task2->start_time)
		return 1;
	return 0;
}

static pid_t parent_pid(struct task_struct *p)
{
	if (thread_group_leader(p->real_parent))
		return task_pid_nr(p->real_parent);
	return task_pid_nr(p->real_parent->group_leader);
}

static void insert_children(struct task_struct *p, struct task_struct **p_stack, int *stack_i)
{
	struct task_struct *curr_task;
	struct task_struct *curr_child;
	int stack_inc;

	stack_inc = 0;
	for_each_thread(p, curr_task) {
		if (list_empty(&(curr_task->children)))
			continue;
		list_for_each_entry(curr_child, &(curr_task->children), sibling)
			p_stack[*stack_i + (++stack_inc)] = curr_child;
	}

	// Changed sizeof(struct task_struct *) to sizeof(*p_stack)
	if (stack_inc > 1)
		sort(&p_stack[*stack_i + 1], stack_inc, sizeof(*p_stack), cmp_children, NULL);

	*stack_i += stack_inc;
}

static void insert_rest(struct k22info *buf, struct task_struct *p, int i)
{
	snprintf(buf[i].comm, sizeof(buf[i].comm), "%s", p->comm);
	buf[i].pid = task_pid_nr(p);
	buf[i].parent_pid = parent_pid(p);
	buf[i].nvcsw = p->nvcsw;
	buf[i].nivcsw = p->nivcsw;
	buf[i].start_time = p->start_time;
}

static int do_k22tree(struct k22info *buf, int *ne)
{
	int return_value;
	int i;
	int pnum;
	int buffer_size;
	int new_buffer_size;
	int upbound;
	int stack_i;
	struct task_struct *p;
	struct task_struct **p_stack;
	struct k22info *proc_buf;

	if (!buf || !ne) {
		return_value = -EINVAL;
		goto out;
	}

	if (copy_from_user(&upbound, ne, sizeof(int))) {//Needs fixing on i
		return_value = -EFAULT;
		goto out;
	}

	if (upbound <= 0) {
		return_value = -EINVAL;
		goto out;
	}

	pnum = 1;
	for_each_process(p)
		++pnum;
	buffer_size = (pnum <= upbound) ? pnum : upbound;
	proc_buf = kmalloc((buffer_size) * sizeof(*proc_buf), GFP_KERNEL);
	if (!proc_buf) {
		return_value = -ENOMEM;
		goto out;
	}
	p_stack = kmalloc((buffer_size) * sizeof(struct task_struct *), GFP_KERNEL);
	if (!p_stack) {
		return_value = -ENOMEM;
		goto free_pbuf;
	}

	read_lock(&tasklist_lock);
	do {
		pnum = 1;
		for_each_process(p)
			++pnum;

		new_buffer_size = (pnum <= upbound) ? pnum : upbound;
		if (new_buffer_size > buffer_size) {
			read_unlock(&tasklist_lock);
			buffer_size *= 2;
			kfree(proc_buf);
			kfree(p_stack);
			proc_buf = kmalloc((buffer_size) * sizeof(*buf), GFP_KERNEL);
			if (!proc_buf) {
				return_value = -ENOMEM;
				goto out;
			}
			p_stack = kmalloc((buffer_size) * sizeof(struct task_struct *), GFP_KERNEL);
			if (!p_stack) {
				return_value = -ENOMEM;
				goto free_pbuf;
			}
			read_lock(&tasklist_lock);
		} else {
			break;
		}
	} while (1);
	p_stack[0] = &init_task;

	stack_i = 0;
	i = 0;
	do {
		p = p_stack[stack_i--];

		if (stack_i >= 0)
			if (is_sibling(p, p_stack[stack_i]))
				proc_buf[i].next_sibling_pid = task_pid_nr(p_stack[stack_i]);
			else
				proc_buf[i].next_sibling_pid = 0;
		else
			proc_buf[i].next_sibling_pid = 0;

		insert_rest(proc_buf, p, i++);
		insert_children(p, p_stack, &stack_i);

		if (stack_i >= 0)
			if (is_child(p, p_stack[stack_i]))
				proc_buf[i - 1].first_child_pid = task_pid_nr(p_stack[stack_i]);
			else
				proc_buf[i - 1].first_child_pid =  0;
		else
			proc_buf[i - 1].first_child_pid = 0;
	} while ((stack_i >= 0) && (i < upbound));
	read_unlock(&tasklist_lock);

	if (copy_to_user(buf, proc_buf, i * sizeof(*proc_buf))) {
		return_value = -EFAULT;
		goto free_stack;
	}
	return_value = pnum;
	if (copy_to_user(ne, &i, sizeof(i)))
		return_value = -EFAULT;

free_stack:
	kfree(p_stack);
free_pbuf:
	kfree(proc_buf);
out:
	return return_value;
}

SYSCALL_DEFINE2(k22tree, struct k22info __user *, buf, int __user *, ne)
{
	return do_k22tree(buf, ne);
}
