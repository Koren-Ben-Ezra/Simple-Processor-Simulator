
.word 0x100 40 # radius = 40

# given pixel at (x,y) turn white if (x-127)^2 + (y-127)^2 <= R^2
# $t0 = x, $t1 = y, $t2 = R^2

# initialization
lw $t2, $imm1, $zero, $zero, 0x100, 0				# radius = memory[0x100]
mac $t2, $t2, $t2, $zero, 0, 0						

add $t0, $imm1, $zero, $zero, -1, 0					# x = -1
out $zero, $imm1, $zero, $imm2, 21, 255				# set color to white

# handle stack
add $sp, $imm1, $zero, $zero, -3, 0
sw $s0, $sp, $imm1, $zero, 0, 0
sw $s1, $sp, $imm1, $zero, 1, 0
sw $s2, $sp, $imm1, $zero, 2, 0

check_next_x:
add $t0, $t0, $imm1, $zero, 1, 0					# x += 1
add $t1, $zero, $zero, $zero, 0, 0					# y = 0
bge $zero, $t0, $imm1, $imm2, 256, terminate		# if x >= 256: goto terminate

check_next_y:
add $s0, $imm1, $t0, $zero, -127, 0					# $s0 = (x-127)
mac $s0, $s0, $s0, $zero, 0, 0						# $s0 = (x-127)^2

add $s1, $imm1, $t1, $zero, -127, 0					# $s1 = (y-127)
mac $s1, $s1, $s1, $zero, 0, 0						# $s1 = (y-127)^2

add $s2, $s0, $s1, $zero, 0, 0						# $s2 = (x-127)^2 + (y-127)^2
bgt $zero, $s2, $t2, $imm2, 0, check_next_pixel		# if (x-127)^2 + (y-127)^2 > R^2: goto check_next_pixel

# color current pixel
mac $s0, $t0, $imm1, $t1, 256, 0					# $s0 = 256x + y
out $zero, $imm1, $zero, $s0, 20, 0					# monitorarr = 256*x + y
out $zero, $imm1, $zero, $imm2, 22, 1				# monitorcmd = 1

check_next_pixel:
add $t1, $t1, $imm1, $zero, 1, 0					# y += 1
blt $zero, $t1, $imm1, $imm2, 256, check_next_y		# if y < 256: goto check_next_y
beq $zero, $zero, $zero, $imm2, 0, check_next_x		# goto check_next_x

terminate:
lw $s0, $sp, $imm1, $zero, 0, 0
lw $s1, $sp, $imm1, $zero, 1, 0
lw $s2, $sp, $imm1, $zero, 2, 0
add $sp, $imm1, $zero, $zero, 3, 0
halt $zero, $zero, $zero, $zero, 0, 0
