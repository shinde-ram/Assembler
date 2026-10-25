
SECTION .text
START:
    MOV EAX, 0xFFFFFFFF
    MOV EBX, 1
    ADD EAX, EBX       ; 0, CF=1, ZF=1
    MOV EAX, 0x7FFFFFFF
    ADD EAX, EBX       ; 0x80000000, OF=1, SF=1
    MOV EAX, 0
    SUB EAX, EBX       ; 0xFFFFFFFF, CF=1, SF=1
    MOV EAX, 5
    MOV EBX, 5
    CMP EAX, EBX       ; ZF=1, EAX remains 5
    STC                ; CF=1
    CLC                ; CF=0
    CLD                ; DF=0
    STD                ; DF=1
    RET
