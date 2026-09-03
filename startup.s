.syntax unified
.cpu cortex-m4
.thumb

.global _start
.global Reset_Handler

.section .isr_vector,"a",%progbits
.word 0x20020000
.word Reset_Handler + 1

.section .text.Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    bl main

1:
    b 1b

.size Reset_Handler, .-Reset_Handler
