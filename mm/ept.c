#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/pgtable.h>
#include <linux/highmem.h>
#include <asm/tlbflush.h>
#include <linux/hugetlb.h>
#include <linux/pfn_t.h>

extern void (*ept_invalidate_hook)(struct mm_struct *mm, unsigned long addr);

/* --- ... --- */


/* --- Page Fault Handler --- */
static vm_fault_t ept_fault(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    struct mm_struct *mm = vma->vm_mm;
    unsigned long address = vmf->pgoff << PMD_SHIFT;
    unsigned long pfn;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    vm_fault_t ret;

    /* Page Walk */
    pgd = pgd_offset(mm, address);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) goto map_zero;

    p4d = p4d_offset(pgd, address);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) goto map_zero;

    pud = pud_offset(p4d, address);
    if (pud_none(*pud) || pud_bad(*pud)) goto map_zero;

    pmd = pmd_offset(pud, address);
    if (pmd_none(*pmd) || !pmd_present(*pmd)) goto map_zero;

    /* PFN of the Page Table Page */
    pfn = page_to_pfn(pmd_page(*pmd));
    
    /* USE vmf_insert_mixed INSTEAD OF vmf_insert_pfn valid for pages in ram as well*/
    ret = vmf_insert_mixed(vma, vmf->address, __pfn_to_pfn_t(pfn, PFN_DEV));
    return ret;

map_zero:
    /* Map the zero page safely */
    pfn = my_zero_pfn(vmf->address);
    ret = vmf_insert_mixed(vma, vmf->address, __pfn_to_pfn_t(pfn, PFN_DEV));
    return ret;
}

static const struct vm_operations_struct ept_vm_ops = {
    .fault = ept_fault,
};

/* --- File Operations --- */
static int ept_open(struct inode *inode, struct file *file)
{
    if (!capable(CAP_SYS_ADMIN)) return -EPERM;
    return 0;
}

static int ept_mmap(struct file *file, struct vm_area_struct *vma)
{
    vma->vm_ops = &ept_vm_ops;
    vm_flags_set(vma, VM_MIXEDMAP | VM_DONTEXPAND | VM_DONTDUMP);
    return 0;
}

static const struct file_operations ept_fops = {
    .owner = THIS_MODULE,
    .open = ept_open,
    .mmap = ept_mmap,
};

static struct miscdevice ept_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ept",
    .fops = &ept_fops,
};

/* --- Init / Exit --- */
static int __init ept_init(void)
{
    int ret;
    
    ret = misc_register(&ept_device);
    if (ret) {
        pr_err("ept: misc_register failed\n");
        return ret;
    }

    /* * NOTE: If the spec requires invalidation support, 
     * you would assign ept_invalidate_hook here. 
     * e.g., ept_invalidate_hook = my_invalidation_func;
     */
    
    pr_info("ept: loaded\n");
    return 0;
}

static void __exit ept_exit(void)
{
    /* Clear the hook if you used it */
    // if (ept_invalidate_hook == my_invalidation_func)
    //     ept_invalidate_hook = NULL;

    misc_deregister(&ept_device);
    pr_info("ept: unloaded\n");
}

module_init(ept_init);
module_exit(ept_exit);