cmd_/home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/inv.o := gcc -Wp,-MD,/home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/.inv.o.d  -nostdinc -isystem /usr/lib/gcc/i486-linux-gnu/4.4.3/include -I/home/composite/linux-2.6.33.7/arch/x86/include -Iinclude  -include include/generated/autoconf.h -D__KERNEL__ -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -O2 -m32 -msoft-float -mregparm=3 -freg-struct-return -mpreferred-stack-boundary=2 -march=i486 -mtune=generic -Wa,-mtune=generic32 -ffreestanding -DCONFIG_AS_CFI=1 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 -pipe -Wno-sign-compare -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -Wframe-larger-than=1024 -fno-stack-protector -fno-omit-frame-pointer -fno-optimize-sibling-calls -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fno-dwarf2-cfi-asm -fconserve-stack  -DMODULE -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(inv)"  -D"KBUILD_MODNAME=KBUILD_STR(cos)"  -c -o /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/inv.o /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/inv.c

deps_/home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/inv.o := \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/inv.c \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/ipc.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/thread.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/spd.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/shared/cos_types.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/shared/./consts.h \
    $(wildcard include/config/page/offset.h) \
  include/linux/thread_info.h \
    $(wildcard include/config/compat.h) \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/types.h \
    $(wildcard include/config/x86/64.h) \
    $(wildcard include/config/highmem64g.h) \
  include/asm-generic/types.h \
  include/asm-generic/int-ll64.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/linux/posix_types.h \
  include/linux/stddef.h \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  include/linux/compiler-gcc4.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/posix_types.h \
    $(wildcard include/config/x86/32.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/posix_types_32.h \
  include/linux/bitops.h \
    $(wildcard include/config/generic/find/first/bit.h) \
    $(wildcard include/config/generic/find/last/bit.h) \
    $(wildcard include/config/generic/find/next/bit.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/bitops.h \
    $(wildcard include/config/x86/cmov.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/alternative.h \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/paravirt.h) \
  include/linux/stringify.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/asm.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/cpufeature.h \
    $(wildcard include/config/x86/invlpg.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/required-features.h \
    $(wildcard include/config/x86/minimum/cpu/family.h) \
    $(wildcard include/config/math/emulation.h) \
    $(wildcard include/config/x86/pae.h) \
    $(wildcard include/config/x86/cmpxchg64.h) \
    $(wildcard include/config/x86/use/3dnow.h) \
    $(wildcard include/config/x86/p6/nop.h) \
  include/asm-generic/bitops/sched.h \
  include/asm-generic/bitops/hweight.h \
  include/asm-generic/bitops/fls64.h \
  include/asm-generic/bitops/ext2-non-atomic.h \
  include/asm-generic/bitops/le.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/byteorder.h \
  include/linux/byteorder/little_endian.h \
  include/linux/swab.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/swab.h \
    $(wildcard include/config/x86/bswap.h) \
  include/linux/byteorder/generic.h \
  include/asm-generic/bitops/minix.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/thread_info.h \
    $(wildcard include/config/debug/stack/usage.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/page.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/page_types.h \
  include/linux/const.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/page_32_types.h \
    $(wildcard include/config/highmem4g.h) \
    $(wildcard include/config/4kstacks.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/page_32.h \
    $(wildcard include/config/hugetlb/page.h) \
    $(wildcard include/config/debug/virtual.h) \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/x86/3dnow.h) \
  include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
  /usr/lib/gcc/i486-linux-gnu/4.4.3/include/stdarg.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/string.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/string_32.h \
    $(wildcard include/config/kmemcheck.h) \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/sparsemem.h) \
  include/asm-generic/getorder.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/processor.h \
    $(wildcard include/config/x86/vsmp.h) \
    $(wildcard include/config/cc/stackprotector.h) \
    $(wildcard include/config/m386.h) \
    $(wildcard include/config/m486.h) \
    $(wildcard include/config/x86/debugctlmsr.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/processor-flags.h \
    $(wildcard include/config/vm86.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/vm86.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/ptrace.h \
    $(wildcard include/config/x86/ptrace/bts.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/ptrace-abi.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/segment.h \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/math_emu.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/sigcontext.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/current.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/percpu.h \
    $(wildcard include/config/x86/64/smp.h) \
  include/linux/kernel.h \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/spinlock/sleep.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
    $(wildcard include/config/ring/buffer.h) \
    $(wildcard include/config/tracing.h) \
    $(wildcard include/config/numa.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
  include/linux/linkage.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/linkage.h \
    $(wildcard include/config/x86/alignment/16.h) \
  include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  include/linux/typecheck.h \
  include/linux/dynamic_debug.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/bug.h \
    $(wildcard include/config/bug.h) \
    $(wildcard include/config/debug/bugverbose.h) \
  include/asm-generic/bug.h \
    $(wildcard include/config/generic/bug.h) \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/div64.h \
  include/asm-generic/percpu.h \
    $(wildcard include/config/debug/preempt.h) \
    $(wildcard include/config/have/setup/per/cpu/area.h) \
  include/linux/threads.h \
    $(wildcard include/config/nr/cpus.h) \
    $(wildcard include/config/base/small.h) \
  include/linux/percpu-defs.h \
    $(wildcard include/config/debug/force/weak/per/cpu.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/system.h \
    $(wildcard include/config/ia32/emulation.h) \
    $(wildcard include/config/x86/32/lazy/gs.h) \
    $(wildcard include/config/x86/ppro/fence.h) \
    $(wildcard include/config/x86/oostore.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/cmpxchg.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/cmpxchg_32.h \
    $(wildcard include/config/x86/cmpxchg.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/nops.h \
    $(wildcard include/config/mk7.h) \
  include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/irqflags.h \
    $(wildcard include/config/debug/lock/alloc.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/pgtable_types.h \
    $(wildcard include/config/compat/vdso.h) \
    $(wildcard include/config/proc/fs.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/pgtable_32_types.h \
    $(wildcard include/config/highmem.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/pgtable-2level_types.h \
  include/asm-generic/pgtable-nopud.h \
  include/asm-generic/pgtable-nopmd.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/msr.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/msr-index.h \
  include/linux/ioctl.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/ioctl.h \
  include/asm-generic/ioctl.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/errno.h \
  include/asm-generic/errno.h \
  include/asm-generic/errno-base.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/cpumask.h \
  include/linux/cpumask.h \
    $(wildcard include/config/cpumask/offstack.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/debug/per/cpu/maps.h) \
    $(wildcard include/config/disable/obsolete/cpumask/functions.h) \
  include/linux/bitmap.h \
  include/linux/errno.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/desc_defs.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/ds.h \
    $(wildcard include/config/x86/ds.h) \
  include/linux/err.h \
  include/linux/personality.h \
  include/linux/cache.h \
    $(wildcard include/config/arch/has/cache/line/size.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/cache.h \
    $(wildcard include/config/x86/l1/cache/shift.h) \
    $(wildcard include/config/x86/internode/cache/shift.h) \
  include/linux/math64.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/ftrace.h \
    $(wildcard include/config/function/tracer.h) \
    $(wildcard include/config/dynamic/ftrace.h) \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/atomic.h \
  /home/composite/linux-2.6.33.7/arch/x86/include/asm/atomic_32.h \
  include/asm-generic/atomic-long.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/shared/./../asm_ipc_defs.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/shared/../debug.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/shared/../shared/cos_config.h \
    $(wildcard include/config/h.h) \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/shared/../measurement.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/shared/consts.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/debug.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/shared/cos_config.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/spd.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/debug.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/measurement.h \
  /home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/include/mmap.h \

/home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/inv.o: $(deps_/home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/inv.o)

$(deps_/home/composite/CompositeOS_Project/src/platform/linux/module/../../../kernel/inv.o):
