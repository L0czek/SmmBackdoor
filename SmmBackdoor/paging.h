#ifndef __PAGING_H__
#define __PAGING_H__

#include "Uefi/UefiBaseType.h"

EFI_STATUS translate_virt_to_phys(EFI_VIRTUAL_ADDRESS address, EFI_PHYSICAL_ADDRESS cr3, EFI_PHYSICAL_ADDRESS *out);

#endif
