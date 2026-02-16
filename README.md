# Assignment 2 

## Objective

The goal of this assignment is to implement a **Group-Round Robin (GRR) scheduling policy** and a custom **scheduling class** in the Linux kernel.  

Key features of this implementation:

- The custom scheduling class sits **above the Fair scheduling class** but **below the Real-Time scheduling class**. 
- Supports **periodic load balancing** every **500ms** per core:
  - GRR tasks are moved from the **idlest CPU** to the **busiest CPU**.
  - The load balancing function acquires the appropriate locks to ensure correctness. 
- Additionally, via the syscall **`assign_ncores_to_group`**, the **root user** can dynamically adjust the number of cores allocated to each group for its tasks.  
  - This means that tasks belonging to one group may experience **worse performance** compared to tasks in another group, depending on the core allocation.

- Tasks can be assigned by the **root user** to the **default** or to the **performance group** via the syscall **`assign_process_to_group`** 

## Patch

The file **`hmwk2.patch`** contains all the additions we made to the Linux kernel.
