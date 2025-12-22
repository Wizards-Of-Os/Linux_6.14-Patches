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

int find_idlest_cpu(cpumask_t * );
int find_busiest_cpu(cpumask_t * );
void load_balance_grr(struct rq *);
void migrate_grr_task(struct task_struct * , struct rq* , struct rq*  , int);

#define LB_TIMESLICE 500 * HZ / 1000

#endif

#endif