#ifndef H_GRR
#define H_GRR

#include <linux/sched.h>

#define GRR_DEFAULT 1 
#define GRR_PERFORMANCE 2 

#ifdef CONFIG_SMP

void load_balance_grr(struct rq *);

#define LB_TIMESLICE 500 * HZ / 1000

#endif

#endif