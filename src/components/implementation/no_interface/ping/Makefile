C_OBJS=ping.o
ASM_OBJS=
COMPONENT=pingp.o
INTERFACES=
DEPENDENCIES=pong sched fork
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)

MANDITORY_LIB=simple_stklib.o
endif