// SPDX-License-Identifier: GPL-2.0
#include <linux/sched/grr.h>

#ifdef CONFIG_SMP



/*
 * About the masks and the per-rq locks used
 *
 * These masks are used to represent the cpus each group includes.
 * @cp_d: The cpus belonging to the default group
 * @cp_p: The cpus belonging to the performance group
 *
 * They are used by load_balance_grr, select_task_rq_grr and do_sched_assign_process_to_group
 * just for reading. They are only mutated in do_sched_assign_ncores_to_group, so we need
 * to synchronise them.
 *
 * 2 per core locks are used, the mask_lock for select_task_rq_grr and
 * do_sched_assign_process_to_group, and the balance_lock, in load_balance_grr.
 * These functions need to aquire their respective locks before they start reading from a mask.
 * That way, select_task_rq_grr does not affect load ballancing, and the per-core key,
 * means that these functions can run simultaniouly on different cpus.
 *
 * When do_sched_assign_ncores_to_group is called, it locks all those locks,
 * and unlockes them once the masks have been updated corrently. That eliminates the
 * race conditions regarding the masks
 *
 * Last but not least, these functions do a set of operations using the group masks.
 * The per-rq temp_mask is used to store the results from such operations. They are protected
 * from the mask_lock, and the balancer does not use the temp_mask.
 */

cpumask_t cp_d;
cpumask_t cp_p;



/*
 * @bl_d: Lock used when balancing among default cores
 * @bl_p: Lock used when balancing among performance cores
 *
 * These make sure that only 1 balance can occur at a time for a group
 * Without those, 2 or more simultanious balancing action, could have migrated
 * excess amount of tasks.
 */
raw_spinlock_t bl_d;
raw_spinlock_t bl_p;



/*
 * This is called once in sched_init_smp.
 * We initialize the global masks and locks here.
 * This is where the default cpus are initialized to be the first half of the systems cores
 * and the other half are assigned to the performance.
 */
void init_sched_grr_class(void)
{
	raw_spin_lock_init(&bl_d);
	raw_spin_lock_init(&bl_p);

	cpumask_copy(&cp_d, cpumask_of(0));
	cpumask_copy(&cp_p, cpumask_of(nr_cpu_ids - 1));

	for (int i = 1; i < nr_cpu_ids / 2; i++) {
		cpumask_or(&cp_d, cpumask_of(i), &cp_d);
		cpumask_or(&cp_p, cpumask_of(nr_cpu_ids - i - 1), &cp_p);
	}
}

#endif /* CONFIG_SMP */


/*
 * Initializing both lists and other values
 * In multicore, we also initialize the per-queue lock
 */
void init_grr_rq(struct grr_rq *grr_rq)
{
	INIT_LIST_HEAD(grr_rq->group);
	INIT_LIST_HEAD(grr_rq->group + 1);
	grr_rq->grr_nr_running = 0;
	grr_rq->perf_bias = PERF_BIAS;
	grr_rq->def_bias = DEF_BIAS;
#ifdef CONFIG_SMP
	raw_spin_lock_init(&grr_rq->mask_lock);
	raw_spin_lock_init(&grr_rq->balance_lock);
	grr_rq->lb_timeslice = LB_TIMESLICE;
#endif
}

static void update_curr_grr(struct rq *rq)
{
	// This was a common function found in other schedulers as well
	update_curr_common(rq);
}


// Moves a task ta the end of its respective queue, based on the grr group it is in
static void requeue_task_grr(struct rq *rq, struct task_struct *p)
{
	struct sched_grr_entity *grr_se = &p->grr;
	struct grr_rq *grr_rq = &rq->grr;
	int idx = grr_se->prio;

	list_move_tail(&grr_se->run_list, grr_rq->group + idx);
}

static void wakeup_preempt_grr(struct rq *rq, struct task_struct *p, int flags)
{

}

static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grr;
	struct grr_rq *grr_rq = &rq->grr;
	int idx = grr_se->prio;

	if (idx > 1 || idx < 0)
		return;

	list_add_tail(&grr_se->run_list, grr_rq->group + idx);
	grr_se->on_rq = 1;
	++grr_rq->grr_nr_running;
	add_nr_running(rq, 1); // PUCCINI
}

static bool dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct grr_rq *grr_rq = &rq->grr;

	update_curr_grr(rq);
	if (!list_empty(&p->grr.run_list)) {
		list_del(&p->grr.run_list);
		p->grr.on_rq = 0;
		--grr_rq->grr_nr_running;
		sub_nr_running(rq, 1); // PUCCINI
		return true;
	}
	return false;
}

static void yield_task_grr(struct rq *rq)
{
	struct sched_grr_entity *grr_se = &rq->curr->grr;
	struct grr_rq *grr_rq = &rq->grr;

	list_move_tail(&grr_se->run_list, grr_rq->group + grr_se->prio);
}

/*
 * Implemented in a starvation free way, to ensure all tasks in the rq
 * will eventually be picked. Also see grr.h
 */
static struct task_struct *pick_task_grr(struct rq *rq)
{
	struct grr_rq *grr_rq = &rq->grr;
	struct task_struct *p;
	int prio = 0;

	if (unlikely(list_empty(grr_rq->group + 1) && list_empty(grr_rq->group)))
		return NULL;

	// If any signle list is empty, pick from it
	if (!list_empty(grr_rq->group + 1) && list_empty(grr_rq->group))
		goto task_pick_perf;

	if (list_empty(grr_rq->group + 1) && !list_empty(grr_rq->group))
		goto task_pick_def;

	// Both groups have tasks, checking whether a perf tasks should be picked
	if (grr_rq->perf_bias) {
		--grr_rq->perf_bias;
		goto task_pick_perf;
	}

	// If this is the last def tasks to get picked, reset the counters
	if (!--grr_rq->def_bias) {
		grr_rq->perf_bias = PERF_BIAS;
		grr_rq->def_bias = DEF_BIAS;
	}
	goto task_pick_def;

task_pick_perf:
	prio = 1;
task_pick_def:
	p = list_first_entry(grr_rq->group + prio, struct task_struct, grr.run_list);
	return p;
}


// Common functions called from other scheduler classes
static void put_prev_task_grr(struct rq *rq, struct task_struct *p, struct task_struct *next)
{
	update_curr_grr(rq);
}

static inline void set_next_task_grr(struct rq *rq, struct task_struct *p, bool first)
{
	p->se.exec_start = rq_clock_task(rq);
}



static void task_tick_grr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_grr_entity *grr_se = &p->grr;

	update_curr_grr(rq);
	if (--p->grr.time_slice)
		return;

	p->grr.time_slice = RR_TIMESLICE;

	// No need to reschedule if there are no other tasks in any group
	if (grr_se->run_list.prev != grr_se->run_list.next ||
		!list_empty(rq->grr.group + !grr_se->prio)) {

		requeue_task_grr(rq, p);
		resched_curr(rq);
	}

}

static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *task)
{
	return RR_TIMESLICE;
}

static void prio_changed_grr(struct rq *rq, struct task_struct *p, int oldprio)
{

}

static void switched_to_grr(struct rq *rq, struct task_struct *p)
{

}


#ifdef CONFIG_SMP

/*
 * find_busiest_cpu and find_idlest_cpu find the busiest and idlest cpus from the ones
 * one their @group_mask argument.
 * They use rq->nr_running as a metric.
 *
 * Whilst this value is constantly changing, I cannot thing of a spimple way to read all the
 * values without locking all rqs at the same time.
 * This is not so serious, since because of added some checks, the worst that can happen is
 * missing an opportunity for balancing, or not balancing from the *currently*
 * the actaul busiest to the actual idlest.
 *
 * These are benign consequences, and we ddem them far better than blocking all the rqs every 500ms
 */

int find_busiest_cpu(cpumask_t *group_mask)
{
	int cpu, max = -42, max_cpu = 0;
	struct rq *rq;
	int nr_rn;
	int is_bigger;

	for_each_cpu(cpu, group_mask) {
		rq = cpu_rq(cpu);
		nr_rn = rq->nr_running;
		is_bigger = nr_rn > max;
		if (is_bigger) {
			max = rq->nr_running;
			max_cpu = cpu;
		}
	}
	return max_cpu;
}


int find_idlest_cpu(cpumask_t *group_mask)
{
	int cpu, min = INT_MAX, min_cpu = 0;
	struct rq *rq;

	for_each_cpu(cpu, group_mask) {
		rq = cpu_rq(cpu);
		if (rq->nr_running < min) {
			min = rq->nr_running;
			min_cpu = cpu;
		}
	}
	return min_cpu;
}


void load_balance_grr(struct rq *this_rq)
{
	int cpu, busiest_cpu, idlest_cpu;
	int cpu_group = 0; // GRR Group that the current cpu belongs to
	int balance_other = 0;
	struct rq *busiest_rq, *idlest_rq;
	raw_spinlock_t *bl_lock;
	cpumask_t *group_mask;

	// Do balancing only when 500ms have passed
	if (--this_rq->grr.lb_timeslice)
		return;

	this_rq->grr.lb_timeslice = LB_TIMESLICE;

	raw_spin_lock(&this_rq->grr.balance_lock);
	cpu =  smp_processor_id();
	if (cpumask_test_cpu(cpu, &cp_d)) {
		group_mask = &cp_d;
		bl_lock = &bl_d;
	} else {
		group_mask = &cp_p;
		bl_lock = &bl_p;
		cpu_group = 1;
	}
	raw_spin_lock(bl_lock);
	busiest_cpu = find_busiest_cpu(group_mask);
	idlest_cpu = find_idlest_cpu(group_mask);
	busiest_rq = cpu_rq(busiest_cpu);
	idlest_rq = cpu_rq(idlest_cpu);
	if ((busiest_cpu == idlest_cpu) || (busiest_rq->nr_running - idlest_rq->nr_running) <= 1)
		goto unlock;
	double_raw_lock(&busiest_rq->__lock, &idlest_rq->__lock);

	struct grr_rq *grr = &busiest_rq->grr;

	/*
	 * If the respected group of a cpu has no tasks, we check whether there are tasks
	 * in the other group.
	 * In that case, we later attempt to move that task out of this cpu and into a cpu with in a
	 * matching group
	 */
	if (!list_empty(grr->group + !cpu_group))
		balance_other = 1;

	struct list_head *iterator = grr->group + cpu_group;
	struct task_struct *task;

	if (busiest_rq->donor->sched_class == &grr_sched_class
		&& busiest_rq->donor->grr.prio == cpu_group)
		iterator = iterator->next;

	list_for_each_continue(iterator, grr->group + cpu_group) {
		task = list_entry(iterator, struct task_struct, grr.run_list);
		if (cpumask_test_cpu(idlest_cpu, task->cpus_ptr)) {
			migrate_grr_task(task, busiest_rq, idlest_rq);
			break;
		}
	}

	double_raw_unlock(&busiest_rq->__lock, &idlest_rq->__lock);

	// There is no task in the other queue
	if (!balance_other)
		goto unlock;

	// Doing a similar thing to before, but now the destination os from a different group
	cpu_group = !cpu_group;
	group_mask = cpu_group ? &cp_p : &cp_d;
	idlest_cpu = find_idlest_cpu(group_mask);
	idlest_rq = cpu_rq(idlest_cpu);

	double_raw_lock(&busiest_rq->__lock, &idlest_rq->__lock);

	// We really want to get rid of those tasks, so we dont check whether the imbalance reverts

	iterator = grr->group + cpu_group;
	if (busiest_rq->donor->sched_class == &grr_sched_class
		&& busiest_rq->donor->grr.prio == cpu_group)
		iterator = iterator->next;
	list_for_each_continue(iterator, grr->group + cpu_group) {
		task = list_entry(iterator, struct task_struct, grr.run_list);
		if (cpumask_test_cpu(idlest_cpu, task->cpus_ptr)) {
			migrate_grr_task(task, busiest_rq, idlest_rq);
			break;
		}
	}
	double_raw_unlock(&busiest_rq->__lock, &idlest_rq->__lock);
unlock:
	raw_spin_unlock(bl_lock);
	raw_spin_unlock(&this_rq->grr.balance_lock);
}

/*
 * The task is inserted into the idlest cpu that belongs to the task's group and
 * respects its affinity.
 * If there is no such cpu, we just return one that respects the affinity.
 */
static int select_task_rq_grr(struct task_struct *p, int cpu, int flags)
{
	struct cpumask *group_mask, *tmp_mask;
	struct rq *task_rq;
	int picked;

	task_rq = this_rq();
	raw_spin_lock(&task_rq->grr.mask_lock);

	tmp_mask = &task_rq->grr.temp_mask;

	group_mask = p->grr.prio ? &cp_p : &cp_d;
	cpumask_and(tmp_mask, group_mask, p->cpus_ptr);

	if (cpumask_empty(tmp_mask)) {
		picked = cpu;
		goto unlock;
	}

	picked = find_idlest_cpu(tmp_mask);

unlock:
	raw_spin_unlock(&task_rq->grr.mask_lock);
	return picked;
}

/*
 * Dequeues the @current_task from the @task_rq and enqueues it in @idlest_rq
 * It mimics the move_queued_task function in core.c
 */
inline void migrate_grr_task(struct task_struct *current_task,
	struct rq *task_rq, struct rq *idlest_rq)
{
	int idlest_cpu = idlest_rq->cpu;

	deactivate_task(task_rq, current_task, 0);
	set_task_cpu(current_task, idlest_cpu);
	activate_task(idlest_rq, current_task, 0);
}


/*
 * Moves all the tasks of the @group from @src_rq to another cpu of the same group
 * We idealy want the destination cpu to be the idlest, but is the tasks cannot go there, we
 * simply migrate it to another valid.
 */
inline void migrate_all_grr_tasks(struct rq *src_rq, struct rq *pref_dest_rq, int group)
{
	struct task_struct *curr_task, *next;
	struct rq *dest_rq;
	struct rq *curr_rq = this_rq();
	cpumask_t *tmp_mask = &curr_rq->grr.temp_mask;
	int pref_dest_cpu = pref_dest_rq->cpu;

	struct cpumask *group_mask = group ? &cp_p : &cp_d;

	list_for_each_entry_safe(curr_task, next, src_rq->grr.group + group, grr.run_list) {

		if (curr_task == src_rq->donor)
			continue;
		cpumask_and(tmp_mask, group_mask, curr_task->cpus_ptr);

		// Task cannot be migrated
		if (cpumask_empty(tmp_mask))
			continue;

		if (cpumask_test_cpu(pref_dest_cpu, tmp_mask))
			dest_rq = pref_dest_rq;
		else
			dest_rq = cpu_rq(cpumask_any(tmp_mask));

		migrate_grr_task(curr_task, src_rq, dest_rq);
	}
}

#endif


DEFINE_SCHED_CLASS(grr) = {

	.enqueue_task		= enqueue_task_grr,
	.dequeue_task		= dequeue_task_grr,
	.yield_task		= yield_task_grr,

	.wakeup_preempt		= wakeup_preempt_grr,

	.pick_task		= pick_task_grr,
	.put_prev_task		= put_prev_task_grr,
	.set_next_task          = set_next_task_grr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_grr,
	.set_cpus_allowed       = set_cpus_allowed_common,

#endif

	.task_tick		= task_tick_grr,

	.get_rr_interval	= get_rr_interval_grr,

	.prio_changed		= prio_changed_grr,
	.switched_to		= switched_to_grr,

	.update_curr		= update_curr_grr,
};
