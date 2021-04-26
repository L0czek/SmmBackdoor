#include "locate.h"
#include "IndustryStandard/PeImage.h"

void* get_image_base() {
    UINT64 rip = get_rip();
    UINT64 base = rip & ( ~(DEFAULT_IMAGE_ALIGN - 1) ); 

    while ( *((short *) base ) != EFI_IMAGE_DOS_SIGNATURE )
        base -= DEFAULT_IMAGE_ALIGN;

    return (void *) base;
}

void reloc_image(void* image, void* new_image_base) {
    EFI_IMAGE_DOS_HEADER* dos_header = (EFI_IMAGE_DOS_HEADER *) image;
    EFI_IMAGE_NT_HEADERS64* nt_headers = (EFI_IMAGE_NT_HEADERS64 *) 
        ((UINT64) image) + dos_header->e_lfanew;
    const UINT64 old_base = nt_headers->OptionalHeader.ImageBase;

    UINT64 relocation_size = nt_headers->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
    EFI_IMAGE_BASE_RELOCATION *relocation_ptr = (EFI_IMAGE_BASE_RELOCATION *)
        ((UINT64) image) + nt_headers->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;

    if (relocation_ptr) {
        UINT64 size = 0;
        while (relocation_size > size && relocation_ptr->SizeOfBlock) {
            const UINT64 number_of_relocs = (relocation_ptr->SizeOfBlock - 8) / 2;
            UINT16 *rel = (UINT16 *) ( ( (UINT64) relocation_ptr) + 8 );

            for (UINT64 i=0; i < number_of_relocs; ++i) {
                UINT16 type = (rel[i] >> 12) & 0x0f;
                UINT64 *addr = (UINT64 *) 
                    ((UINT64) image) + relocation_ptr->VirtualAddress + (rel[i] & 0x0fff);
    
                if (type != EFI_IMAGE_REL_BASED_DIR64)
                    return;

                *addr += ((UINT64) new_image_base) - old_base;
            }
        }

        relocation_ptr = (EFI_IMAGE_BASE_RELOCATION *)
            ((UINT64) relocation_ptr) + relocation_ptr->SizeOfBlock;
        size += relocation_ptr->SizeOfBlock;

    }
}

UINT64 get_rip()  {
    UINT64 rip;

    asm volatile (
        "call _hack_to_get_rip \n\
        _hack_to_get_rip: \n\
        pop %%rax \n\
        movq %%rax, %0 \n\
        "
        : "=r"(rip) : : "rax"
    );

    return rip;
}

