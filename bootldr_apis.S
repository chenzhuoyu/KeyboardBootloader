.global bootldr_apis
.section .bootldr_apis, "ax"

; bootloader API table
bootldr_apis:
    jmp enter_bootloader        ; 0x00  : enter_bootloader
    ret                         ; 0x01  : nop
