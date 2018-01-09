
XOPTS = -I"/home/thepro/ti/xdctools_3_50_01_12_core/packages/" -Dxdc_target_types__=/home/thepro/ti/bios_6_51_00_15/packages/ti/targets/arm/elf/std.h -Dxdc_target_name__=R4F

vpath % /home/thepro/ti/bios_6_51_00_15/packages/ti/sysbios/
vpath %.c /home/thepro/ti/xdctools_3_50_01_12_core/packages/

CCOPTS    = --float_support=vfpv3d16 --endian=little -mv7R4 --abi=eabi -q -ms --opt_for_speed=2  --program_level_compile -o3 -g --optimize_with_debug  -Dti_sysbios_knl_Task_minimizeLatency__D=FALSE -Dti_sysbios_family_arm_exc_Exception_enableDecode__D=TRUE -Dti_sysbios_knl_Clock_stopCheckNext__D=FALSE -Dti_sysbios_family_arm_v7r_vim_Hwi_lockstepDevice__D=TRUE -Dti_sysbios_family_arm_v7r_vim_Hwi_errataInitEsm__D=TRUE -Dti_sysbios_family_arm_v7r_vim_Hwi_resetVIM__D=TRUE -Dti_sysbios_hal_Core_numCores__D=1

XDC_ROOT  = /home/thepro/ti/xdctools_3_50_01_12_core/packages/

BIOS_ROOT = /home/thepro/ti/bios_6_51_00_15/packages/ti/sysbios/

BIOS_DEFS =  -Dti_sysbios_BIOS_swiEnabled__D=TRUE -Dti_sysbios_BIOS_taskEnabled__D=TRUE -Dti_sysbios_BIOS_clockEnabled__D=TRUE -Dti_sysbios_BIOS_runtimeCreatesEnabled__D=TRUE -Dti_sysbios_knl_Task_moduleStateCheckFlag__D=FALSE -Dti_sysbios_knl_Task_objectCheckFlag__D=FALSE -Dti_sysbios_hal_Hwi_DISABLE_ALL_HOOKS -Dti_sysbios_knl_Swi_DISABLE_ALL_HOOKS -Dti_sysbios_BIOS_smpEnabled__D=FALSE -Dti_sysbios_Build_useHwiMacros -Dti_sysbios_Build_useIndirectReferences=FALSE -Dti_sysbios_knl_Swi_numPriorities__D=16 -Dti_sysbios_knl_Task_deleteTerminatedTasks__D=FALSE -Dti_sysbios_knl_Task_numPriorities__D=16 -Dti_sysbios_knl_Task_checkStackFlag__D=TRUE -Dti_sysbios_knl_Task_initStackFlag__D=TRUE -Dti_sysbios_knl_Task_DISABLE_ALL_HOOKS -Dti_sysbios_knl_Clock_TICK_SOURCE=ti_sysbios_knl_Clock_TickSource_TIMER -Dti_sysbios_knl_Clock_TICK_MODE=ti_sysbios_knl_Clock_TickMode_PERIODIC -Dti_sysbios_hal_Core_delegate_getId=ti_sysbios_family_arm_v7r_tms570_Core_getId__E -Dti_sysbios_hal_Core_delegate_interruptCore=ti_sysbios_family_arm_v7r_tms570_Core_interruptCore__E -Dti_sysbios_hal_Core_delegate_lock=ti_sysbios_family_arm_v7r_tms570_Core_lock__E -Dti_sysbios_hal_Core_delegate_unlock=ti_sysbios_family_arm_v7r_tms570_Core_unlock__E -Dti_sysbios_hal_Core_numCores__D=1 -Dti_sysbios_family_arm_v7r_tms570_Core_numCores__D=1 -Dti_sysbios_utils_Load_taskEnabled__D=TRUE -Dti_sysbios_utils_Load_swiEnabled__D=FALSE -Dti_sysbios_utils_Load_hwiEnabled__D=FALSE -Dti_sysbios_family_arm_v7r_vim_Hwi_dispatcherSwiSupport__D=TRUE -Dti_sysbios_family_arm_v7r_vim_Hwi_dispatcherTaskSupport__D=TRUE -Dti_sysbios_family_arm_v7r_vim_Hwi_dispatcherAutoNestingSupport__D=TRUE -Dti_sysbios_family_arm_v7r_vim_Hwi_dispatcherIrpTrackingSupport__D=TRUE -Dti_sysbios_knl_Semaphore_supportsEvents__D=FALSE -Dti_sysbios_knl_Semaphore_supportsPriority__D=TRUE 

BIOS_INC  = -I"/home/thepro/ti/bios_6_51_00_15/packages/"

INCS =  $(BIOS_INC) -I"/home/thepro/ti/bios_6_51_00_15/packages/" -I"/home/thepro/ti/mathlib_c674x_3_1_2_1/packages/" -I"/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/" -I"/home/thepro/ti/ccsv7/ccs_base/" -I"/home/thepro/ti/xdctools_3_50_01_12_core/packages/" -I"../" -I"/packages//" 

CC = /home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/bin/armcl -c $(CCOPTS) -I /home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/include
ASM = /home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/bin/armcl -c $(CCOPTS) -I /home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/include
AR = /home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/bin/armar rq

DEL = rm -f
CP = cp -f

define RM
	$(if $(wildcard $1),$(DEL) $1,:)
endef

define ASSEMBLE
	@echo asmer4f $< ...
	@$(ASM) $(BIOS_DEFS) $(XOPTS) $(INCS) $<
endef

all: sysbios.aer4f

arm_IntrinsicsSupport_asm.obj: family/arm/IntrinsicsSupport_asm.asm makefile
	@-$(call RM, $@)
	$(ASSEMBLE) --output_file=arm_IntrinsicsSupport_asm.obj

arm_TaskSupport_asm.obj: family/arm/TaskSupport_asm.asm makefile
	@-$(call RM, $@)
	$(ASSEMBLE) --output_file=arm_TaskSupport_asm.obj

vim_Hwi_asm.obj: family/arm/v7r/vim/Hwi_asm.sv7R makefile
	@-$(call RM, $@)
	$(ASSEMBLE) --output_file=vim_Hwi_asm.obj

vim_Hwi_asm_switch.obj: family/arm/v7r/vim/Hwi_asm_switch.sv7R makefile
	@-$(call RM, $@)
	$(ASSEMBLE) --output_file=vim_Hwi_asm_switch.obj

exc_Exception_asm.obj: family/arm/exc/Exception_asm.asm makefile
	@-$(call RM, $@)
	$(ASSEMBLE) --output_file=exc_Exception_asm.obj

tms570_Core_asm.obj: family/arm/v7r/tms570/Core_asm.sv7R makefile
	@-$(call RM, $@)
	$(ASSEMBLE) --output_file=tms570_Core_asm.obj


BIOS.obj: BIOS.c family/arm/IntrinsicsSupport.c family/arm/TaskSupport.c knl/Clock.c knl/Idle.c knl/Intrinsics.c knl/Event.c knl/Queue.c knl/Semaphore.c knl/Swi.c knl/Task.c heaps/HeapBuf.c heaps/HeapMem.c family/arm/v7r/vim/Hwi.c family/arm/exc/Exception.c hal/Cache.c hal/CacheNull.c hal/Core.c hal/Hwi.c hal/Hwi_stack.c hal/Hwi_startup.c family/arm/v7a/Pmu.c gates/GateHwi.c gates/GateMutex.c family/arm/v7r/tms570/Core.c timers/rti/Timer.c makefile
	@-$(call RM, $@)
	@echo cler4f $< ...
	@$(CC) $(BIOS_DEFS) $(XOPTS) $(INCS) \
	$(BIOS_ROOT)BIOS.c \
	$(BIOS_ROOT)family/arm/IntrinsicsSupport.c \
	$(BIOS_ROOT)family/arm/TaskSupport.c \
	$(BIOS_ROOT)knl/Clock.c \
	$(BIOS_ROOT)knl/Idle.c \
	$(BIOS_ROOT)knl/Intrinsics.c \
	$(BIOS_ROOT)knl/Event.c \
	$(BIOS_ROOT)knl/Queue.c \
	$(BIOS_ROOT)knl/Semaphore.c \
	$(BIOS_ROOT)knl/Swi.c \
	$(BIOS_ROOT)knl/Task.c \
	$(BIOS_ROOT)heaps/HeapBuf.c \
	$(BIOS_ROOT)heaps/HeapMem.c \
	$(BIOS_ROOT)family/arm/v7r/vim/Hwi.c \
	$(BIOS_ROOT)family/arm/exc/Exception.c \
	$(BIOS_ROOT)hal/Cache.c \
	$(BIOS_ROOT)hal/CacheNull.c \
	$(BIOS_ROOT)hal/Core.c \
	$(BIOS_ROOT)hal/Hwi.c \
	$(BIOS_ROOT)hal/Hwi_stack.c \
	$(BIOS_ROOT)hal/Hwi_startup.c \
	$(BIOS_ROOT)family/arm/v7a/Pmu.c \
	$(BIOS_ROOT)gates/GateHwi.c \
	$(BIOS_ROOT)gates/GateMutex.c \
	$(BIOS_ROOT)family/arm/v7r/tms570/Core.c \
	$(BIOS_ROOT)timers/rti/Timer.c \

sysbios.aer4f: BIOS.obj arm_IntrinsicsSupport_asm.obj arm_TaskSupport_asm.obj vim_Hwi_asm.obj vim_Hwi_asm_switch.obj exc_Exception_asm.obj tms570_Core_asm.obj
	@-$(call RM, $@)
	@echo arer4f $^ ...
	@$(AR) $@ $^


clean:
	@$(DEL) ../makefile.libs
	@-$(call RM, *)
