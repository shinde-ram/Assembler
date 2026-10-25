
; Test opcode recognition, symbol table, data size, ModR/M and SIB

GLOBAL START

SECTION .data
NUMBERS DB 10,20,30       ; 3 bytes, address 0
PAIRS DW 100,200         ; 4 bytes, address 3
TOTAL DD 500            ; 4 bytes, address 7
MSG DB 'HI',0            ; 3 bytes, address 11
AFTER_MSG DB 7          ; 1 byte, address 14

SECTION .bss
BUFFER RESB 8            ; 8 bytes, address 0
WORDS RESW 3            ; 6 bytes, address 8
DWORDS RESD 2           ; 8 bytes, address 14

SECTION .text
START:
    MOV EAX, EBX             ; Inline comment handling
    MOV EAX, [EBX+4]         ; Base + small displacement
    MOV EAX, [EBX+0128]      ; Decimal 128, not octal
    MOV EAX, [8]             ; Absolute memory addressing
    MOV EAX, [128]           ; Absolute memory addressing
    MOV EAX, [EAX+ESI*4]     ; Scaled index
    MOV EAX, [EAX+ESI*4+8]   ; Scaled index + displacement
    MOV EAX, [EAX+ESI]       ; Two-register addressing
    MOV EAX, [EAX+ESI+8]     ; Two registers + displacement
    MOV EAX, [ESP]           ; ESP requires SIB
    MOV EAX, 010             ; Decimal immediate
    MOV EDX, 0x10            ; Hexadecimal immediate
    ADD EAX, EBX             ; Arithmetic opcode
    CMP EAX, EBX             ; Comparison opcode
    INC EAX                  ; Single operand opcode
    NOP                      ; No operands
    RET                      ; No operands


