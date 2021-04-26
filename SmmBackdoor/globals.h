#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "Protocol/LoadedImage.h"
#include "Uefi/UefiBaseType.h"
#include "Uefi/UefiSpec.h"
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>

extern EFI_RUNTIME_SERVICES* gRT;
extern EFI_HANDLE gIM;

extern EFI_LOADED_IMAGE_PROTOCOL* gLoadedImage;

extern void* PayloadImageBase;
extern void* ImageBase;

extern EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;

void Init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable);

#endif
