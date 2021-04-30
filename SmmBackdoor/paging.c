#include "Uefi/UefiBaseType.h"
#include "VirtualMemory.h"
#include "smm.h"

#define PAGE_SHIFT 12
#define PML4_ADDRESS(addr) ((addr) & 0xfffffffffffff000)
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1ff)
#define PDPTE_INDEX(addr) (((addr) >> 30) & 0x1ff)
#define PDE_INDEX(addr) (((addr) >> 21) & 0x1ff)
#define PTE_INDEX_4KB(addr) (((addr) >> 12) & 0x1ff)
#define PAGE_OFFSET_4KB(addr) ((addr) & 0xfff)
#define PAGE_OFFSET_2MB(addr) ((addr) & 0x1fffff) 
#define PAGE_OFFSET_1GB(addr) ((addr) & 0x3fffffff)
#define PFN_TO_PAGE(addr) ((addr) << PAGE_SHIFT)
#define PAGE_TO_PFN(addr) ((addr) >> PAGE_SHIFT)

#define SIZE_BIT 0x80ULL

static EFI_STATUS translate_virt_to_phys_pte(EFI_VIRTUAL_ADDRESS address, EFI_PHYSICAL_ADDRESS pte, EFI_PHYSICAL_ADDRESS *out) {
    X64_PAGE_TABLE_ENTRY_4K pte_entry = {
        .Uint64 = ((UINT64 *) pte)[PTE_INDEX_4KB(address)]
    };

    if (pte_entry.Bits.Present) {

        *out = PFN_TO_PAGE(pte_entry.Bits.PageTableBaseAddress) + PAGE_OFFSET_4KB(address);
        return EFI_SUCCESS;

    } else {
        return EFI_NOT_FOUND;
    }
}

static EFI_STATUS translate_virt_to_phys_pde(EFI_VIRTUAL_ADDRESS address, EFI_PHYSICAL_ADDRESS pde, EFI_PHYSICAL_ADDRESS *out) {
    X64_PAGE_DIRECTORY_ENTRY_4K pde_entry = {
        .Uint64 = ((UINT64 *) pde)[PDE_INDEX(address)]
    };

    if (pde_entry.Bits.Present) {

        if (pde_entry.Uint64 & SIZE_BIT) {
            
            *out = PFN_TO_PAGE(pde_entry.Bits.PageTableBaseAddress) + PAGE_OFFSET_2MB(address);
            return EFI_SUCCESS;

        } else {
            return translate_virt_to_phys_pte(address, PFN_TO_PAGE(pde_entry.Bits.PageTableBaseAddress), out);
        }

    } else {
        return EFI_NOT_FOUND;
    }
}

static EFI_STATUS translate_virt_to_phys_pdpte(EFI_VIRTUAL_ADDRESS address, EFI_PHYSICAL_ADDRESS pdpte, EFI_PHYSICAL_ADDRESS *out) {
    X64_PAGE_MAP_AND_DIRECTORY_POINTER_2MB_4K pdpte_entry = {
        .Uint64 = ((UINT64 *) pdpte)[PDPTE_INDEX(address)]
    };

    if (pdpte_entry.Bits.Present) {

        if (pdpte_entry.Uint64 & SIZE_BIT) {

            *out = PFN_TO_PAGE(pdpte_entry.Bits.PageTableBaseAddress) + PAGE_OFFSET_1GB(address);
            return EFI_SUCCESS;

        } else {
            return translate_virt_to_phys_pde(address, PFN_TO_PAGE(pdpte_entry.Bits.PageTableBaseAddress), out);
        }
        
    } else {
        return EFI_NOT_FOUND;
    }
}

EFI_STATUS translate_virt_to_phys(EFI_VIRTUAL_ADDRESS address, EFI_PHYSICAL_ADDRESS cr3, EFI_PHYSICAL_ADDRESS *out) {
    X64_PAGE_MAP_AND_DIRECTORY_POINTER_2MB_4K pml4_entry = {
        .Uint64 = ((UINT64 *) PML4_ADDRESS(cr3))[PML4_INDEX(address)]
    };

    if (pml4_entry.Bits.Present) {
        return translate_virt_to_phys_pdpte(address, PFN_TO_PAGE(pml4_entry.Bits.PageTableBaseAddress), out);        
    } else {
        return EFI_NOT_FOUND;
    }
}
