################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
dss_config_edma_util.oe674: ../dss_config_edma_util.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: C6000 Compiler'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/bin/cl6x" -mv6740 --abi=eabi -O3 --include_path="/home/thepro/Documents/mmwave_workspace/mmw_dss" --include_path="/home/thepro/ti/mmwave_sdk_01_00_00_05/packages" --include_path="/home/thepro/ti/mathlib_c674x_3_1_2_1/packages" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft16x16/c64P" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft32x32/c64P" --include_path="/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/include" -g --gcc --define=SOC_XWR16XX --define=SUBSYS_DSS --define=DOWNLOAD_FROM_CCS --define=DebugP_ASSERT_ENABLED --diag_wrap=off --diag_warning=225 --display_error_number --gen_func_subsections=on --obj_extension=.oe674 --preproc_with_compile --preproc_dependency="dss_config_edma_util.d" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '

dss_data_path.oe674: ../dss_data_path.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: C6000 Compiler'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/bin/cl6x" -mv6740 --abi=eabi -O3 --include_path="/home/thepro/Documents/mmwave_workspace/mmw_dss" --include_path="/home/thepro/ti/mmwave_sdk_01_00_00_05/packages" --include_path="/home/thepro/ti/mathlib_c674x_3_1_2_1/packages" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft16x16/c64P" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft32x32/c64P" --include_path="/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/include" -g --gcc --define=SOC_XWR16XX --define=SUBSYS_DSS --define=DOWNLOAD_FROM_CCS --define=DebugP_ASSERT_ENABLED --diag_wrap=off --diag_warning=225 --display_error_number --gen_func_subsections=on --obj_extension=.oe674 --preproc_with_compile --preproc_dependency="dss_data_path.d" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '

dss_main.oe674: ../dss_main.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: C6000 Compiler'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/bin/cl6x" -mv6740 --abi=eabi -O3 --include_path="/home/thepro/Documents/mmwave_workspace/mmw_dss" --include_path="/home/thepro/ti/mmwave_sdk_01_00_00_05/packages" --include_path="/home/thepro/ti/mathlib_c674x_3_1_2_1/packages" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft16x16/c64P" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft32x32/c64P" --include_path="/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/include" -g --gcc --define=SOC_XWR16XX --define=SUBSYS_DSS --define=DOWNLOAD_FROM_CCS --define=DebugP_ASSERT_ENABLED --diag_wrap=off --diag_warning=225 --display_error_number --gen_func_subsections=on --obj_extension=.oe674 --preproc_with_compile --preproc_dependency="dss_main.d" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '

build-481470351:
	@$(MAKE) -Onone -f subdir_rules.mk build-481470351-inproc

build-481470351-inproc: ../dss_mmw.cfg
	@echo 'Building file: $<'
	@echo 'Invoking: XDCtools'
	"/home/thepro/ti/xdctools_3_50_01_12_core/xs" --xdcpath="/home/thepro/ti/bios_6_51_00_15/packages;/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages;/home/thepro/ti/mathlib_c674x_3_1_2_1/packages;/home/thepro/ti/ccsv7/ccs_base;" xdc.tools.configuro -o configPkg -t ti.targets.elf.C674 -p ti.platforms.c6x:IWR16XX:false:600 -r release -c "/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3" "$<"
	@echo 'Finished building: $<'
	@echo ' '

configPkg/linker.cmd: build-481470351 ../dss_mmw.cfg
configPkg/compiler.opt: build-481470351
configPkg/: build-481470351

gen_twiddle_fft16x16.oe674: ../gen_twiddle_fft16x16.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: C6000 Compiler'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/bin/cl6x" -mv6740 --abi=eabi -O3 --include_path="/home/thepro/Documents/mmwave_workspace/mmw_dss" --include_path="/home/thepro/ti/mmwave_sdk_01_00_00_05/packages" --include_path="/home/thepro/ti/mathlib_c674x_3_1_2_1/packages" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft16x16/c64P" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft32x32/c64P" --include_path="/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/include" -g --gcc --define=SOC_XWR16XX --define=SUBSYS_DSS --define=DOWNLOAD_FROM_CCS --define=DebugP_ASSERT_ENABLED --diag_wrap=off --diag_warning=225 --display_error_number --gen_func_subsections=on --obj_extension=.oe674 --preproc_with_compile --preproc_dependency="gen_twiddle_fft16x16.d" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '

gen_twiddle_fft32x32.oe674: ../gen_twiddle_fft32x32.c $(GEN_OPTS) | $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: C6000 Compiler'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/bin/cl6x" -mv6740 --abi=eabi -O3 --include_path="/home/thepro/Documents/mmwave_workspace/mmw_dss" --include_path="/home/thepro/ti/mmwave_sdk_01_00_00_05/packages" --include_path="/home/thepro/ti/mathlib_c674x_3_1_2_1/packages" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft16x16/c64P" --include_path="/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages/ti/dsplib/src/DSP_fft32x32/c64P" --include_path="/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.1.3/include" -g --gcc --define=SOC_XWR16XX --define=SUBSYS_DSS --define=DOWNLOAD_FROM_CCS --define=DebugP_ASSERT_ENABLED --diag_wrap=off --diag_warning=225 --display_error_number --gen_func_subsections=on --obj_extension=.oe674 --preproc_with_compile --preproc_dependency="gen_twiddle_fft32x32.d" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '


