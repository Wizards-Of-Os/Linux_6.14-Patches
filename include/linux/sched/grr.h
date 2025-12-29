/* SPDX-License-Identifier: GPL-2.0 */
#ifndef H_GRR
#define H_GRR

#include <linux/sched.h>

#define GRR_DEFAULT 1
#define GRR_PERFORMANCE 2

/*
 * @PREF_BIAS and @DEF_BIAS are used to ensure a starvation freescheduling in
 * the case where an rq has both performance and default tasks.
 * These macros ensure that after @PERF_BIAS number of performance tasks have been picked,
 * @DEF_BIAS number of default tasks will be picked right after.
 *
 * For performance tasks to get more cpu time, @PERF_BIAS should be greater than @DEF_BIAS
 * though, one if free to configure it otherwise.
 */

#define PERF_BIAS 5
#define DEF_BIAS 3

#ifdef CONFIG_SMP

// For the masks and lockes used, see comments in grr.c
extern cpumask_t cp_d;
extern cpumask_t cp_p;

int find_idlest_cpu(cpumask_t *group_mask);
int find_busiest_cpu(cpumask_t *group_mask);
void load_balance_grr(struct rq *this_rq);
inline void migrate_grr_task(struct task_struct *current_task,
	struct rq *task_rq, struct rq *idlest_rq);
inline void migrate_all_grr_tasks(struct rq *src_rq, struct rq *pref_dest_rq, int group);

#define LB_TIMESLICE (500 * HZ / 1000)

#endif

#endif
