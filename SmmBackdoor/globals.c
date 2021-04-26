#include "globals.h"
#include "AutoGen.h"
#include "Protocol/LoadedImage.h"
#include "Uefi/UefiSpec.h"

void Init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    gIM = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;

    gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **) &gLoadedImage);
}

EFI_HANDLE gIM;
EFI_RUNTIME_SERVICES* gRT;

EFI_LOADED_IMAGE_PROTOCOL* gLoadedImage;

