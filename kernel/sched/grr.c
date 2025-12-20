#define GRR_DEFAULT 1
#define GRR_PERFORMANCE 2

#define LB_TIMESLICE 500 * HZ / 1000

cpumask_t cpD = {};
cpumask_t cpP = {};

//PUCCINI 3.0
raw_spinlock_t giannis;
raw_spinlock_t thanos;
raw_spinlock_t vaggos;
raw_spinlock_t alberto;

void init_grr_locks(void)
{
	raw_spin_lock_init(&giannis);
	raw_spin_lock_init(&thanos);
	raw_spin_lock_init(&vaggos);
	raw_spin_lock_init(&alberto);
}

void init_grr_rq(struct grr_rq *grr_rq)
{
	INIT_LIST_HEAD(grr_rq->group);
	INIT_LIST_HEAD(grr_rq->group + 1);
	grr_rq->grr_nr_running = 0 ; 
	grr_rq->lb_timeslice = LB_TIMESLICE;
	cpD = (*)cpumask_of(0);
	cpP = (*)cpumask_of(nr_cpu_ids / 2);
	for (int i = 1; i < nr_cpu_ids / 2; i ++) {
		cpumask_or(&cpD, cpumask_of(i), cpD);
		cpumask_or(&cpP, cpumask_of(nr_cpu_ids / 2 + i), cpP);
	}
}

static inline struct task_struct *grr_task_of(struct sched_grr_entity *grr_se)
{
	return container_of(grr_se, struct task_struct, grr);
}

static void requeue_task_grr(struct rq *rq, struct task_struct *p)
{	
	struct sched_grr_entity *grr_se = &p->grr;
	struct grr_rq * grr_rq = &rq->grr;
	int idx = grr_se->prio ;

	list_move_tail(&grr_se->run_list, grr_rq->group + idx);
}

static void wakeup_preempt_grr(struct rq *rq, struct task_struct *p, int flags)
{

}

static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grr;
	struct grr_rq * grr_rq = &rq->grr;
	int idx = grr_se->prio ;

	if (idx >1 || idx < 0) {
		return ; 
	}
	
	list_add_tail(&grr_se->run_list , grr_rq->group + idx );
	grr_rq->grr_nr_running++;
	add_nr_running(rq,1);
}

static bool dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{	
	struct grr_rq * grr_rq = &rq->grr;

	if (!list_empty(&p->grr.run_list)) { 
		list_del(&p->grr.run_list);
		grr_rq->grr_nr_running--;
		sub_nr_running(rq,1);
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

	if(!list_empty(grr_rq->group)) {
		grr_se = list_first_entry( grr_rq->group , struct sched_grr_entity , run_list);
		return grr_task_of(grr_se);
	}
	
	if(!list_empty(grr_rq->group + 1)) {
		grr_se = list_first_entry( grr_rq->group + 1 , struct sched_grr_entity , run_list);
		return grr_task_of(grr_se);
	}

	return NULL;
}

static void put_prev_task_grr(struct rq *rq, struct task_struct *p, struct task_struct *next)
{
	//!
}

static inline void set_next_task_grr(struct rq *rq, struct task_struct *p, bool first)
{
	//!
}

static void load_balance_grr(void)
{
	struct *rq this_rq = this_rq();
	if (--this_rq->grr.lb_timeslice)
		return;
	this_rq->grr.lb_timeslice = LB_TIMESLICE;

	//find_busiest

	//find_lowest
	
	//swap

}

static void task_tick_grr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_grr_entity *grr_se = &p->grr;

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
	//Μπορει να χρειαστει να ελεγξουμε εαν το priority γινει πολυ μικρο και χρειαστει να αλλαξη η κλαση -> call reschedule
}

static void switched_to_grr(struct rq *rq, struct task_struct *p)
{

}

static void update_curr_grr(struct rq *rq)
{

}

DEFINE_SCHED_CLASS(grr) = {

	.enqueue_task		= enqueue_task_grr,
	.dequeue_task		= dequeue_task_grr,
	.yield_task		= yield_task_grr,

	.wakeup_preempt		= wakeup_preempt_grr,

	.pick_task		= pick_task_grr,
	.put_prev_task		= put_prev_task_grr,
	.set_next_task          = set_next_task_grr,

#ifdef CONFIG_SMP
	.balance		= balance_grr,
	.select_task_rq		= select_task_rq_grr,
	.set_cpus_allowed       = set_cpus_allowed_common,
	.rq_online              = rq_online_grr,
	.rq_offline             = rq_offline_grr,
	.task_woken		= task_woken_grr,
	.switched_from		= switched_from_grr,
	.find_lock_rq		= find_lock_lowest_rq,
#endif

	.task_tick		= task_tick_grr,

	.get_rr_interval	= get_rr_interval_grr,

	.prio_changed		= prio_changed_grr,
	.switched_to		= switched_to_grr,

	.update_curr		= update_curr_grr,
};