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

/* Λύνει το compile error για το flush_tlb_range (Εικόνα 2) */
#include <linux/hugetlb.h> 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("K22 Team");
MODULE_DESCRIPTION("Export Page Table (EPT) Module");

/* Εξωτερικός δείκτης από mm/memory.c */
extern void (*ept_invalidate_hook)(struct mm_struct *mm, unsigned long addr);

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

    /* Page Walk */
    pgd = pgd_offset(mm, address);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) goto map_zero;

    p4d = p4d_offset(pgd, address);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) goto map_zero;

    pud = pud_offset(p4d, address);
    if (pud_none(*pud) || pud_bad(*pud)) goto map_zero;

    pmd = pmd_offset(pud, address);
    if (pmd_none(*pmd) || !pmd_present(*pmd)) goto map_zero;

    /* Εύρεση PFN */
    pfn = page_to_pfn(pmd_page(*pmd));
    
    /* vmf_insert_pfn (απαιτεί VM_PFNMAP) */
    return vmf_insert_pfn(vma, vmf->address, pfn);

map_zero:
    return vmf_insert_pfn(vma, vmf->address, my_zero_pfn(vmf->address));
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
    
    /* * ΑΛΛΑΓΗ: Χρησιμοποιούμε VM_MIXEDMAP.
     * Το VM_PFNMAP προκαλεί loop όταν το PFN δείχνει σε κανονική RAM (backed pages).
     * Το VM_MIXEDMAP επιτρέπει την εισαγωγή PFNs χωρίς αυτά τα προβλήματα.
     */
    vm_flags_set(vma, VM_MIXEDMAP | VM_DONTEXPAND | VM_DONTDUMP);
    
    return 0;
}

static const struct file_operations ept_fops = {
    .owner = THIS_MODULE,
    .open  = ept_open,
    .mmap  = ept_mmap,
};

static struct miscdevice ept_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .fops  = &ept_fops,
    .name  = "ept",
    .mode  = 0600,
};

/* --- Synchronization Logic (Anti-Deadlock) --- */

/* Callback που απλά μηδενίζει το PTE χωρίς locks */
static int ept_zap_pte_fn(pte_t *pte, unsigned long addr, void *data)
{
    struct mm_struct *mm = (struct mm_struct *)data;
    pte_clear(mm, addr, pte);
    return 0;
}

static void my_ept_invalidate(struct mm_struct *mm, unsigned long target_addr)
{
    struct vm_area_struct *vma;
    VMA_ITERATOR(vmi, mm, 0);

    for_each_vma(vmi, vma) {
        if (vma->vm_ops == &ept_vm_ops) {
            unsigned long pmd_index = target_addr >> PMD_SHIFT;
            unsigned long ept_offset = pmd_index << PAGE_SHIFT;
            unsigned long address_to_zap = vma->vm_start + ept_offset;

            if (address_to_zap >= vma->vm_start && address_to_zap < vma->vm_end) {
                
                /* ΑΝΤΙ ΓΙΑ ZAP_PAGE_RANGE: Χρησιμοποιούμε apply_to_page_range.
                 * Αυτό αποφεύγει το DEADLOCK και το Implicit Declaration error.
                 */
                apply_to_page_range(mm, address_to_zap, PAGE_SIZE, ept_zap_pte_fn, mm);
                
                /* Flush TLB (απαραίτητο) */
                flush_tlb_range(vma, address_to_zap, address_to_zap + PAGE_SIZE);
            }
        }
    }
}

/* --- Init / Exit --- */
static int __init ept_init(void)
{
    int ret = misc_register(&ept_dev);
    if (ret) return ret;
    ept_invalidate_hook = my_ept_invalidate;
    return 0;
}

static void __exit ept_exit(void)
{
    ept_invalidate_hook = NULL;
    misc_deregister(&ept_dev);
}

module_init(ept_init);
module_exit(ept_exit);