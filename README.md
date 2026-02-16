# Assignment 1 

## Objective

The goal of this assignment is to implement a Linux syscall named **`k22tree`**. 

Given a user-space buffer and an integer **`ne`**, which represents the maximum number of processes the caller wishes to store in the buffer, the syscall performs a **Depth-First Search (DFS) traversal** of the system's process tree. 

The syscall then:

- Returns the total number of processes found.
- Fills the provided buffer with **process-specific information** for each process found.

The integer **`ne`** acts as an **upper bound** on the number of processes that can be written to the buffer.  

- If the syscall finds **fewer processes** than `ne`, it updates it's value to reflect the actual number of processes found.  
- If the syscall finds **more processes** than `ne`, then the syscall stores `ne` entries in the buffer and it is the responsibility of the user to recognize, based on the output, that the buffer was not large enough. In that case, the user must increase the buffer size accordingly and recall the syscall in order to ensure that all processes can be stored.


## Demonstration

The test program **`test_k22tree.c`** demonstrates the usage of the **`k22tree`** syscall.


It performs the following actions:

- Allocates a user-space buffer.  
- Calls the `k22tree` syscall with a specified `ne` value.  
- Prints all process-specific information retrieved in the buffer according to the DFS traversal.

## Patch

The file **`k22tree.patch`** contains all the additions we made to the Linux kernel.
