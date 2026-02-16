# Assignment 3 

## Objective

The goal of this assignment is to **expose the page table of a process to user space** through the use of a custom device.  

Key features of this implementation:

- A **custom page fault handler** is triggered whenever a page fault occurs at a virtual address of the flat table.  
- The handler performs a **walk of the multilevel page table** of the process to locate the corresponding **Page Table Entry (PTE)**  that has the translation for the specific virtual page.
- If a valid PTE exists, it is returned to the user space.  
- If no PTE exists for the given virtual page, the handler makes a mapping to the **zero page**.  
- This approach allows user-space programs to safely inspect the page table entries of a process using mmap and the custom device.

## Demonstration

The test program **`test.c`** demonstrates how we can expose the page table of a process using the custom device as well as the custom page fault handler.

To compile and run the test program:

```bash
gcc -o test test.c && ./test
```

## Patch

The file **`hmwk3.patch`** contains all the additions we made to the Linux kernel.
