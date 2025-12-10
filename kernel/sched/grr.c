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

#ifdef CONFIG_SCHED_CORE
	.task_is_throttled	= task_is_throttled_grr,
#endif

#ifdef CONFIG_UCLAMP_TASK
	.uclamp_enabled		= 1,
#endif
};