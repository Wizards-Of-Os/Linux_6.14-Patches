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

void load_balance_grr(struct rq *);

#define LB_TIMESLICE 500 * HZ / 1000

#endif

#endif