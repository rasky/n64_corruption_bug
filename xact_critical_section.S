.set noat
.set noreorder

.section .text.xact_critical_section

read_rdram_cc:
    # Result in $v0, four 2 MiB module's CC values in upper to lower bytes.
    move $a3, $ra
    lui $a1, 0xA430 # MI mode register
    li $a2, 0x2000 # Set upper mode
    sw $a2, (0)($a1)
    lui $a0, 0xA3F0 # Base RDRAM register address
    lw $t0, (0x000C)($a0) # 2 MiB module 0
    lw $t1, (0x080C)($a0) # 2 MiB module 1
    lw $t2, (0x100C)($a0) # 2 MiB module 2 if expansion pak, else 0s
    lw $t3, (0x180C)($a0) # 2 MiB module 3 if expansion pak, else 0s
    
    jal shuffle_rdram_cc
     move $v0, $zero
    jal shuffle_rdram_cc
     move $t0, $t1
    jal shuffle_rdram_cc
     move $t0, $t2
    jal shuffle_rdram_cc
     move $t0, $t3
    
    li $a2, 0x1000 # Clear upper mode
    jr $a3 # saved ra
     sw $a2, (0)($a1)
    
shuffle_rdram_cc:
    # Input in $t0, result shifted into $v0. Not an independent function but
    # codependent on read_rdram_cc.
    sll    $v0, $v0, 8
    # Bits 5-0 of cc are in bits 23, 15, 7, 22, 14, 6.
    lui    $t4, 0x0080
    ori    $t4, $t4, 0x8080
    and    $t5, $t4, $t0 # Just bits 5-3 in 23, 15, 7
    srl    $t4, $t4, 1
    and    $t4, $t4, $t0 # Just bits 2-0 in 22, 14, 6
    srl    $t5, $t5, 4 # 5-3 in 19, 11, 3
    srl    $t4, $t4, 6 # 2-0 in 16, 8, 0
    or     $t0, $t4, $t5 # All of the above
    andi   $t4, $t0, 0x0900 # 4 and 1 in 11 and 8
    srl    $t4, $t4, 7 # 4 and 1
    lui    $t5, 0x0009
    and    $t5, $t5, $t0 # 5 and 2 in 19 and 16
    srl    $t5, $t5, 14 # 5 and 2
    andi   $t0, $t0, 0x0009 # 3 and 0
    or     $t0, $t0, $t4
    or     $t0, $t0, $t5
    jr     $ra
     or    $v0, $v0, $t0
    

# Need to separate this out to ensure we do not use the data cache at all
# (except in the under-test ways) across trigger_setup, prime_go, and trigger_go.
.global xact_critical_section
xact_critical_section:
    lw $v0, (0x24)($sp)
    addiu $sp, $sp, -0x48
    sd $s0, (0x00)($sp)
    sd $s1, (0x08)($sp)
    sd $s2, (0x10)($sp)
    sd $s3, (0x18)($sp)
    sd $s4, (0x20)($sp)
    sd $s5, (0x28)($sp)
    sd $s6, (0x30)($sp)
    sd $s7, (0x38)($sp)
    sd $ra, (0x40)($sp)
    move $s0, $a0
    move $s1, $a1
    move $s2, $a2
    move $s3, $a3
    move $s4, $v0

    move $a0, $s3
    jal trigger_setup
    move $a1, $s4

    move $a0, $s0
    move $a1, $s1
    jal prime_go
    move $a2, $s2
    
    jal read_rdram_cc
     nop
    move $s7, $v0
    
    move $a0, $s3
    jal trigger_go
    move $a1, $s4

    jal read_rdram_cc
     nop
    lui $t0, %hi(dstate + 0)
    sw $s7, (%lo(dstate + 0))($t0)
    lui $t0, %hi(dstate + 4)
    sw $v0, (%lo(dstate + 4))($t0)

    ld $s0, (0x00)($sp)
    ld $s1, (0x08)($sp)
    ld $s2, (0x10)($sp)
    ld $s3, (0x18)($sp)
    ld $s4, (0x20)($sp)
    ld $s5, (0x28)($sp)
    ld $s6, (0x30)($sp)
    ld $s7, (0x38)($sp)
    ld $ra, (0x40)($sp)
    jr $ra
     addiu $sp, $sp, 0x48

# Dummy function to mark the end of code we need to put in the icache.
.global dummy_function_end
dummy_function_end:
    jr $ra
    nop
