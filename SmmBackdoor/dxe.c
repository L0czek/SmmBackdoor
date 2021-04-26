#include "efivars.h"

void backdoor_dxe_entry(void) {
    set_backdoor_efivar_str(DxeVar, "DXE_INIT_OK");
}
