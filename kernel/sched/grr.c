#include <linux/sched/grr.h>

#ifdef CONFIG_SMP

cpumask_t cp_d;
cpumask_t cp_p;

raw_spinlock_t bl_d;
raw_spinlock_t bl_p;

void init_grr_locks(void)
{
	raw_spin_lock_init(&bl_d);
	raw_spin_lock_init(&bl_p);
}	
	
#endif

void init_grr_rq(struct grr_rq *grr_rq)
{
	INIT_LIST_HEAD(grr_rq->group);
	INIT_LIST_HEAD(grr_rq->group + 1);
	grr_rq->grr_nr_running = 0; 

	#ifdef CONFIG_SMP

	grr_rq->lb_timeslice = LB_TIMESLICE;
	cpumask_copy(&cp_d, cpumask_of(0));
	cpumask_copy(&cp_p, cpumask_of(nr_cpu_ids / 2));
	
	for (int i = 1; i < nr_cpu_ids / 2; i ++) {
		cpumask_or(&cp_d, cpumask_of(i), &cp_d);
		cpumask_or(&cp_p, cpumask_of(nr_cpu_ids / 2 + i), &cp_p);
	}

	#endif
}

static void update_curr_grr(struct rq *rq)
{
	update_curr_common(rq);
}

static inline struct task_struct *grr_task_of(struct sched_grr_entity *grr_se)
{
	return container_of(grr_se, struct task_struct, grr);
}

static void requeue_task_grr(struct rq *rq, struct task_struct *p)
{	
	struct sched_grr_entity *grr_se = &p->grr;
	struct grr_rq * grr_rq = &rq->grr;
	int idx = grr_se->prio;

	list_move_tail(&grr_se->run_list, grr_rq->group + idx);
}

static void wakeup_preempt_grr(struct rq *rq, struct task_struct *p, int flags)
{

}

static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grr;
	struct grr_rq * grr_rq = &rq->grr;
	int idx = grr_se->prio;

	if (idx > 1 || idx < 0)
		return ;

	list_add_tail(&grr_se->run_list, grr_rq->group + idx);
	++grr_rq->grr_nr_running;
	add_nr_running(rq, 1);
}

static bool dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{	
	struct grr_rq * grr_rq = &rq->grr;

	update_curr_grr(rq);
	if (!list_empty(&p->grr.run_list)) { 
		list_del(&p->grr.run_list);
		--grr_rq->grr_nr_running;
		sub_nr_running(rq, 1);
		return true;
	}
	return false;
}

static void yield_task_grr(struct rq *rq)
{
	struct sched_grr_entity *grr_se = &rq->curr->grr;
	struct grr_rq * grr_rq = &rq->grr;
	list_move_tail(&grr_se->run_list, grr_rq->group + grr_se->prio);
}

static struct task_struct *pick_task_grr(struct rq *rq)
{
	struct grr_rq *grr_rq = &rq->grr;
	struct sched_grr_entity *grr_se; 

	if(!list_empty(grr_rq->group + 1)) {
		grr_se = list_first_entry( grr_rq->group + 1 , struct sched_grr_entity , run_list);
		return grr_task_of(grr_se);
	}

	if(!list_empty(grr_rq->group)) {
		grr_se = list_first_entry( grr_rq->group , struct sched_grr_entity , run_list);
		return grr_task_of(grr_se);
	}

	return NULL;
}

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
	
	if (grr_se->run_list.prev != grr_se->run_list.next) {
		requeue_task_grr(rq, p);
		resched_curr(rq);
		return;
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

static int find_busiest_cpu(cpumask_t * group_mask)
{		
	int cpu, max = -1, max_cpu = 0;
	struct rq * rq ; 
	
	for_each_cpu(cpu , group_mask){
		rq = cpu_rq(cpu);
		if(rq->nr_running > max){
			max = rq->nr_running;
			max_cpu = cpu;
		}
	}
	return max_cpu;
}


static int find_idlest_cpu(cpumask_t * group_mask)
{		
	int cpu, min = INT_MAX, min_cpu=0;
	struct rq * rq ; 
	
	for_each_cpu(cpu , group_mask){
		rq = cpu_rq(cpu);
		if(rq->nr_running < min){
			min = rq->nr_running;
			min_cpu = cpu;
		}
	}
	return min_cpu;
}


void load_balance_grr(struct rq * this_rq)
{

	int cpu , busiest_cpu , idlest_cpu , perf = 0 ;
	struct rq *busiest_rq , *idlest_rq ;


	if (--this_rq->grr.lb_timeslice)
		return;
	
	this_rq->grr.lb_timeslice = LB_TIMESLICE;

	cpu =  smp_processor_id();

	raw_spin_lock(&bl_d);

	if(cpumask_test_cpu(cpu , &cp_d)){
		busiest_cpu = find_busiest_cpu(&cp_d);
		idlest_cpu = find_idlest_cpu(&cp_d);
	}
	else{
		raw_spin_unlock(&bl_d);
		raw_spin_lock(&bl_p);
		perf=1;
		busiest_cpu = find_busiest_cpu(&cp_p);
		idlest_cpu = find_idlest_cpu(&cp_p);
	}

	busiest_rq = cpu_rq(busiest_cpu);
	idlest_rq = cpu_rq(idlest_cpu);

	if( (busiest_cpu == idlest_cpu) || (busiest_rq->nr_running - idlest_rq->nr_running ) <=1 )
		goto unlock;
	
	double_raw_lock(&busiest_rq->__lock , &idlest_rq->__lock);

	struct grr_rq * grr = &busiest_rq->grr ;
	struct list_head * iterator = grr->group + perf; 
	struct task_struct * task ; 

	if(busiest_rq->curr->sched_class == &grr_sched_class)
		iterator= iterator->next;

	list_for_each_continue(iterator, grr->group + perf){
		task = list_entry(iterator ,struct task_struct , grr.run_list );
		if(cpumask_test_cpu(idlest_cpu , task->cpus_ptr)){
			dequeue_task_grr(busiest_rq , task , 0);
			set_task_cpu(task, idlest_cpu);
			enqueue_task_grr(idlest_rq , task , 0);
			break;
		}
	}

	double_raw_unlock(&busiest_rq->__lock , &idlest_rq->__lock);
unlock:
	perf? raw_spin_unlock(&bl_p) : raw_spin_unlock(&bl_d);
	printk_deferred(KERN_INFO "GRR BUSIEST CPU: %d , IDLEST_CPU: %d , NR_RUNNNING_BUSIEST: %d , NR_RUNNING_IDLEST: %d , TASK_PID: %d , TASK_POLICY: %d\n" , busiest_cpu , idlest_cpu , busiest_rq->nr_running , idlest_rq->nr_running , task->pid , task->policy );
}


static int select_task_rq_grr(struct task_struct *p, int cpu, int flags)
{
	return cpu;
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