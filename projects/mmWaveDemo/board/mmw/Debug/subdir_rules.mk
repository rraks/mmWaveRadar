################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
cli.obj: ../cli.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/bin/armcl" -mv7R4 --code_state=32 --float_support=VFPv3D16 -me -O3 --include_path="/home/thepro/Documents/mmwave_workspace/mmw" --include_path="/home/thepro/ti/mmwave_sdk_01_00_00_05/packages" --include_path="/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/include" --define=SOC_XWR16XX --define=SUBSYS_MSS --define=DOWNLOAD_FROM_CCS --define=DebugP_ASSERT_ENABLED -g --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --enum_type=packed --abi=eabi --preproc_with_compile --preproc_dependency="cli.d" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '

mss_main.obj: ../mss_main.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/bin/armcl" -mv7R4 --code_state=32 --float_support=VFPv3D16 -me -O3 --include_path="/home/thepro/Documents/mmwave_workspace/mmw" --include_path="/home/thepro/ti/mmwave_sdk_01_00_00_05/packages" --include_path="/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/include" --define=SOC_XWR16XX --define=SUBSYS_MSS --define=DOWNLOAD_FROM_CCS --define=DebugP_ASSERT_ENABLED -g --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --enum_type=packed --abi=eabi --preproc_with_compile --preproc_dependency="mss_main.d" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '

build-154886513:
	@$(MAKE) -Onone -f subdir_rules.mk build-154886513-inproc

build-154886513-inproc: ../mss_mmw.cfg
	@echo 'Building file: $<'
	@echo 'Invoking: XDCtools'
	"/home/thepro/ti/xdctools_3_50_01_12_core/xs" --xdcpath="/home/thepro/ti/bios_6_51_00_15/packages;/home/thepro/ti/mathlib_c674x_3_1_2_1/packages;/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages;/home/thepro/ti/ccsv7/ccs_base;" xdc.tools.configuro -o configPkg -t ti.targets.arm.elf.R4F -p ti.platforms.cortexR:IWR16XX:false:200 -r release -c "/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS" "$<"
	@echo 'Finished building: $<'
	@echo ' '

configPkg/linker.cmd: build-154886513 ../mss_mmw.cfg
configPkg/compiler.opt: build-154886513
configPkg/: build-154886513


