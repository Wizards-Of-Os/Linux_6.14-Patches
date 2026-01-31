// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pgtable.h>
#include <linux/highmem.h>
#include <asm/tlbflush.h>
#include <linux/hugetlb.h>
#include <linux/pfn_t.h>

extern void (*ept_inval_func)(struct mm_struct *mm, unsigned long address);

// Used to store the address space and it is needed by the invalidation function
static struct address_space *ept_mapping;

/*
 * This function is called from memory.c when a new PTE page is allocated. In that case we need
 * to invalidate that page from our mapping. That way a page fault will occur when trying to
 * access these pages, resulting in the device mapping the updated PTE to the address space.
 */
static void ept_inval(struct mm_struct *mm, unsigned long address)
{
	/*
	 * We need to modify address such that the correct virtual addresses of the device are
	 * invalidated. We simply need to notice that the address corresponds to a PMD entry
	 * pointing to a PTE, while we want a PTE entry that points to physical pages.
	 * So we need to shift the address by (PMD_SHIFT - PAGE_SHIFT)
	 * The way we do it below, also zeroes any offset.
	 */
	loff_t ept_of = (address >> PMD_SHIFT) << PAGE_SHIFT;

	// No need to do anything if no mapping has happended
	if (ept_mapping)
		unmap_mapping_range(ept_mapping, ept_of, PAGE_SIZE, 1);
}

/*
 * The custom fault handler
 * It walks the page table up to the PMD level. If the translation is not found,
 * the zero page is mapped.
 * Then, the physical address of the respective PTE table is mapped to the virtual address page
 * thay caused the fault. That way, the virual page will map to the PTE page containing the
 * translations for that virtual page.
 */
static vm_fault_t ept_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct mm_struct *mm = vma->vm_mm;
	// The address of the page that caused the fault should be used for the walking
	unsigned long address = vmf->pgoff << PMD_SHIFT;
	unsigned long pfn;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	vm_fault_t ret;

	// Page walking
	pgd = pgd_offset(mm, address);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		goto map_zero;

	// Just returns the pgd if the 5-lvl page table is disabled
	p4d = p4d_offset(pgd, address);
	if (p4d_none(*p4d) || p4d_bad(*p4d))
		goto map_zero;

	pud = pud_offset(p4d, address);
	if (pud_none(*pud) || pud_bad(*pud))
		goto map_zero;

	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd) || !pmd_present(*pmd))
		goto map_zero;

	// With this trick we can avoid writing architecture specific code
	// This works since page numbers do not change between architectures.
	pfn = page_to_pfn(pmd_page(*pmd));

	// Inserting the pfn into the vma of the device for the respected address
	ret = vmf_insert_mixed(vma, vmf->address, __pfn_to_pfn_t(pfn, PFN_DEV));

	return ret;

map_zero:
	/* Map the Zero Page */
	pfn = my_zero_pfn(vmf->address);
	ret = vmf_insert_mixed(vma, vmf->address, __pfn_to_pfn_t(pfn, PFN_DEV));
	return ret;
}


static int ept_open(struct inode *inode, struct file *file)
{
	// We just need to check whether the user is root
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	return 0;
}

static const struct vm_operations_struct ept_vm_ops = {
	.fault = ept_fault,
};

static int ept_mmap(struct file *file, struct vm_area_struct *vma)
{
	// Setting the vma so that it uses our custom handlers
	vma->vm_ops = &ept_vm_ops;

	// Setting flags to allow vmf insertion via the vmf_insert_mixed function.
	vm_flags_set(vma, VM_MIXEDMAP | VM_DONTEXPAND | VM_DONTDUMP);

	// This is needed for the invalidation
	ept_mapping = file->f_mapping;

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


/*
 * Initializing the device
 * __init is used by other initializers in more devices, so we included it as well
 */
static int __init ept_init(void)
{
	int ret;

	// Registering the device
	ret = misc_register(&ept_device);
	if (ret)
		return ret;

	// Make sure the custom ept invalidator is  used
	ept_inval_func = ept_inval;

	return 0;
}

static void __exit ept_exit(void)
{
	// The device invalidation function should no longer be used
	ept_inval_func = NULL;

	/* Wait for any running hooks to finish to prevent crashes */
	synchronize_rcu();

	// Unregistering the device
	misc_deregister(&ept_device);
}

module_init(ept_init);
module_exit(ept_exit);
