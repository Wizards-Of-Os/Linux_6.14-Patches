/* SPDX-License-Identifier: GPL-2.0 */
#ifndef H_GRR
#define H_GRR

#include <linux/sched.h>

#define GRR_DEFAULT 1
#define GRR_PERFORMANCE 2

// Used to ensure a startavtion free scheduling in the case where a core shares
// both performance and default tasks.
// To ensure performance tasks are still scheduled more often, these macros ensure
// that for every PERF_BIAS performance tasks DEF_BIAS will be scheduled

#define PERF_BIAS 5
#define DEF_BIAS 3

#ifdef CONFIG_SMP

extern cpumask_t cp_d;
extern cpumask_t cp_p;

extern raw_spinlock_t bl_d;
extern raw_spinlock_t bl_p;

int find_idlest_cpu(cpumask_t *group_mask);
int find_busiest_cpu(cpumask_t *group_mask);
void load_balance_grr(struct rq *this_rq);
void migrate_grr_task(struct task_struct *current_task, struct rq *task_rq, struct rq *idlest_rq);
inline void migrate_all_grr_tasks(struct rq *src_rq, struct rq *pref_dest_rq, int group);

#define LB_TIMESLICE (500 * HZ / 1000)

#else

static bool dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags);
static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags);

#endif

#endif
