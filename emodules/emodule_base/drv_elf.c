#include "drv_elf.h"

static inline uintptr_t __pt2pte(uint16_t pt)
{
	// uintptr_t res = PTE_V;
	// if (pt & PF_R)
	//     res |= PTE_R;
	// if (pt & PF_W)
	//     res |= PTE_W;
	// if (pt & PF_X)
	//     res |= PTE_X;
	// return res;
	return PTE_V | PTE_R | PTE_X | PTE_W;
}

uintptr_t elf_load(uintptr_t pt_root, uintptr_t elf_addr, char id,
		   uintptr_t *prog_brk)
{
	if (elf_check(elf_addr) != EBI_OK)
		return EBI_ERROR;

	Elf64_Ehdr *ehdr  = (Elf64_Ehdr *)elf_addr;
	Elf64_Phdr *phdrs = (Elf64_Phdr *)(elf_addr + ehdr->e_phoff);
	uintptr_t n_pages;
	int i;

	em_debug("There is/are %d program header(s)\n", ehdr->e_phnum);
	for (i = 0; i < ehdr->e_phnum; i++) {
		switch (phdrs[i].p_type) {
		case PT_LOAD: {
			n_pages = (PAGE_UP(phdrs[i].p_filesz) >> EPAGE_SHIFT);
			em_debug("Mapping %d page(s) from %x to %x\n", n_pages,
				 PAGE_DOWN(phdrs[i].p_vaddr),
				 PAGE_DOWN(elf_addr + phdrs[i].p_offset));
			map_page(PAGE_DOWN(phdrs[i].p_vaddr),
				 PAGE_DOWN(elf_addr + phdrs[i].p_offset),
				 n_pages, PTE_U | __pt2pte(phdrs[i].p_flags));
			if ((n_pages << EPAGE_SHIFT) < phdrs[i].p_memsz) {
				n_pages = PAGE_UP(phdrs[i].p_memsz -
						  phdrs[i].p_filesz) >>
					  EPAGE_SHIFT;
				alloc_page(NULL,
					   PAGE_DOWN(phdrs[i].p_vaddr +
						     phdrs[i].p_filesz),
					   n_pages,
					   PTE_U | __pt2pte(phdrs[i].p_flags),
					   id);
			}
			break;
		}
		default: {
			/* TODO: implementation */
		}
		}
	}
	// *prog_brk = PAGE_DOWN(phdr.p_paddr) + PAGE_UP((n_pages + 1)<<EPAGE_SHIFT);
	*prog_brk = EUSR_HEAP_START;
	// Elf64_Shdr *shdr_arr = (Elf64_Shdr *)(elf_addr + ehdr->e_shoff);
	// em_debug("There is/are %d segment(s)\n", ehdr->e_shnum);
	// for (int i = 0; i < ehdr->e_shnum; i++) {
	// 	Elf64_Shdr shdr = shdr_arr[i];
	// 	if (shdr.sh_size && (shdr.sh_flags & SHF_ALLOC) &&
	// 	    (shdr.sh_type == SHT_NOBITS)) {
	// 		uintptr_t n_pages =
	// 			(PAGE_UP(shdr.sh_size) >> EPAGE_SHIFT);
	// 		em_debug("Mapping %d page(s) from %x to %x\n", n_pages,
	// 			 PAGE_DOWN(shdr.sh_addr),
	// 			 PAGE_DOWN(shdr.sh_addr) +
	// 				 PAGE_UP(n_pages << EPAGE_SHIFT));
	// 		if (shdr.sh_flags & SHF_WRITE)
	// 			alloc_page(NULL, PAGE_DOWN(shdr.sh_addr),
	// 				   n_pages,
	// 				   PTE_U | PTE_R | PTE_W | PTE_V, id);
	// 		else
	// 			alloc_page(NULL, PAGE_DOWN(shdr.sh_addr),
	// 				   n_pages, PTE_U | PTE_R | PTE_V, id);
	// 	}
	// }
	return ehdr->e_entry;
}

uintptr_t elf_check(uintptr_t elf_addr)
{
	Elf64_Ehdr *ehdr       = (Elf64_Ehdr *)elf_addr;
	unsigned char *e_ident = ehdr->e_ident;
	if (e_ident[EI_MAG0] != ELFMAG0 || e_ident[EI_MAG1] != ELFMAG1 ||
	    e_ident[EI_MAG2] != ELFMAG2 || e_ident[EI_MAG3] != ELFMAG3) {
		em_error("Not ELF file\n");
		return EBI_ERROR;
	}
	if (e_ident[EI_CLASS] != ELFCLASS64) {
		em_error("Not 64-bits file\n");
		return EBI_ERROR;
	}
	if (ehdr->e_type != ET_EXEC) {
		em_error("Not executable file\n");
		return EBI_ERROR;
	}
	if (ehdr->e_machine != EM_RISCV) {
		em_error("Invalid machine platform\n");
		return EBI_ERROR;
	}
	return EBI_OK;
}