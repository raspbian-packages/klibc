#
# arch/mips64/crt0.S
#
# Does arch-specific initialization and invokes __libc_init
# with the appropriate arguments.
#
# See __static_init.c or __shared_init.c for the expected
# arguments.
#

#include <machine/asm.h>

NESTED(__start, 64, sp)
	daddiu  sp,sp,-64
	sd	zero, 32(sp)

					# Initialize gp
	lui gp,%highest(_gp) 		# load highest "halfword"
	daddiu gp,gp,%higher(_gp) 	# merge next "halfword"
	dsll gp,gp,16 			# shift by one halfword
	daddiu gp,gp,%hi(_gp) 		# merge next "halfword"
	dsll gp,gp,16 			# shift into final position
	daddiu gp,gp,%lo(_gp) 		# merge lowest "halfword"

	daddiu	a0, sp, 64		# Pointer to ELF entry structure
	move	a1, v0			# Kernel-provided atexit() pointer

	ld  t9, %call16(__libc_init)(gp)
	jalr t9

	END(__start)
