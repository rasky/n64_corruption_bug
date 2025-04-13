.set noat
.set noreorder

.section .text.xact_critical_section

# Need to separate this out to ensure we do not use the data cache at all
# (except in the under-test ways) across trigger_setup, prime_go, and trigger_go.
.global xact_critical_section
xact_critical_section:
    lbu	$v0, (39)($sp)
    lw	$v1, (44)($sp)
    addiu $sp, $sp, -0x38
    sd $s0, (0x00)($sp)
    sd $s1, (0x08)($sp)
    sd $s2, (0x10)($sp)
    sd $s3, (0x18)($sp)
    sd $s4, (0x20)($sp)
    sd $s5, (0x28)($sp)
    sd $ra, (0x30)($sp)
    move $s0, $a0
    move $s1, $a1
    move $s2, $a2
    move $s3, $a3
    move $s4, $v0
    move $s5, $v1

    move $a0, $s4
    jal trigger_setup
    move $a1, $s5

    move $a0, $s0
    move $a1, $s1
    move $a2, $s2
    jal prime_go
    move $a3, $s3
    
    move $a0, $s4
    jal trigger_go
    move $a1, $s5

    addiu $a0, $zero, 0x1234
    ld $s0, (0x00)($sp)
    ld $s1, (0x08)($sp)
    ld $s2, (0x10)($sp)
    ld $s3, (0x18)($sp)
    ld $s4, (0x20)($sp)
    ld $s5, (0x28)($sp)
    ld $ra, (0x30)($sp)
    jr $ra
    addiu $sp, $sp, 0x38

# Dummy function to mark the end of code we need to put in the icache.
.global dummy_function_end
dummy_function_end:
    jr $ra
    nop
