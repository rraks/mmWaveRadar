################################################################################
# Automatically-generated file. Do not edit!
################################################################################

CG_TOOL_ROOT := /home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS

GEN_OPTS__FLAG := --cmd_file="configPkg/compiler.opt"
GEN_CMDS__FLAG := -l"configPkg/linker.cmd"

ORDERED_OBJS += \
"./cli.obj" \
"./mss_main.obj" \
"../r4f_linker.cmd" \
$(GEN_CMDS__FLAG) \
-l"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/spi/lib/libspi_xwr16xx.aer4f" \
-llibosal_xwr16xx.aer4f \
-llibesm_xwr16xx.aer4f \
-llibgpio_xwr16xx.aer4f \
-llibsoc_xwr16xx.aer4f \
-llibpinmux_xwr16xx.aer4f \
-llibcrc_xwr16xx.aer4f \
-llibuart_xwr16xx.aer4f \
-llibmailbox_xwr16xx.aer4f \
-llibmmwavelink_xwr16xx.aer4f \
-llibmmwave_xwr16xx.aer4f \
-llibcli_xwr16xx.aer4f \
-lrtsv7R4_T_le_v3D16_eabi.lib \
-llibc.a \
-l"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/dma/lib/libdma_xwr16xx.aer4f" \

-include ../makefile.init

RM := rm -rf
RMDIR := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(INO_DEPS)),)
-include $(INO_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(PDE_DEPS)),)
-include $(PDE_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
xwr16xx_mmw_mss.xer4f \

EXE_OUTPUTS__QUOTED += \
"xwr16xx_mmw_mss.xer4f" \

BIN_OUTPUTS += \
xwr16xx_mmw_mss.hex \

BIN_OUTPUTS__QUOTED += \
"xwr16xx_mmw_mss.hex" \


# All Target
all:
	@$(MAKE) --no-print-directory pre-build
	@$(MAKE) --no-print-directory main-build

# Main-build Target
main-build: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "xwr16xx_mmw_mss.xer4f"

# Tool invocations
xwr16xx_mmw_mss.xer4f: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@echo 'Building target: $@'
	@echo 'Invoking: ARM Linker'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/bin/armcl" -mv7R4 --code_state=32 --float_support=VFPv3D16 -me -O3 --define=SOC_XWR16XX --define=SUBSYS_MSS --define=DOWNLOAD_FROM_CCS --define=DebugP_ASSERT_ENABLED -g --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --enum_type=packed --abi=eabi -z -m"xwr16xx_mmw_mss.map" --heap_size=0x800 --stack_size=0x800 -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/control/mmwave/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/control/mmwavelink/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/crc/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/esm/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/gpio/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/mailbox/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/osal/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/pinmux/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/soc/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/drivers/uart/lib" -i"/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/ti/utils/cli/lib" -i"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/lib" -i"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/include" --reread_libs --disable_auto_rts --diag_warning=225 --diag_wrap=off --display_error_number --warn_sections --xml_link_info="xwr16xx_mmw_mss_linkInfo.xml" --rom_model --unused_section_elimination=on -o "xwr16xx_mmw_mss.xer4f" $(ORDERED_OBJS)
	@echo 'Finished building target: $@'
	@echo ' '
	@$(MAKE) --no-print-directory post-build

xwr16xx_mmw_mss.hex: $(EXE_OUTPUTS)
	@echo 'Invoking: ARM Hex Utility'
	"/home/thepro/ti/ccsv7/tools/compiler/ti-cgt-arm_16.9.1.LTS/bin/armhex"  -o "xwr16xx_mmw_mss.hex" $(EXE_OUTPUTS__QUOTED)
	@echo 'Finished building: $@'
	@echo ' '
	@$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(GEN_CMDS__QUOTED)$(GEN_MISC_FILES__QUOTED)$(BIN_OUTPUTS__QUOTED)$(GEN_FILES__QUOTED)$(EXE_OUTPUTS__QUOTED)$(GEN_OPTS__QUOTED)
	-$(RMDIR) $(GEN_MISC_DIRS__QUOTED)
	-$(RM) "cli.obj" "mss_main.obj" 
	-$(RM) "cli.d" "mss_main.d" 
	-@echo 'Finished clean'
	-@echo ' '

pre-build:
	-rm -f /home/thepro/Documents/mmwave_workspace/mmw/xwr16xx_mmw_mss.bin
	-@echo ' '

post-build:
	-/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/scripts/ImageCreator/xwr16xx/out2rprc/out2rprc.exe xwr16xx_mmw_mss.xer4f xwr16xx_mmw_mss.bin
	-/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/scripts/ImageCreator/xwr16xx/multicore_image_generator/MulticoreImageGen.exe LE 37 0x01000005 xwr16xx_mmw.bin 0x35510000 xwr16xx_mmw_mss.bin 0xb5510000 /home/thepro/ti/mmwave_sdk_01_00_00_05/firmware/radarss/xwr16xx_radarss_rprc.bin 0xd5510000 /home/thepro/Documents/mmwave_workspace/mmw_dss/Debug/xwr16xx_mmw_dss.bin
	-/home/thepro/ti/mmwave_sdk_01_00_00_05/packages/scripts/ImageCreator/xwr16xx/crc_multicore_image/crc_multicore_image.exe xwr16xx_mmw.bin xwr16xx_mmw.bin.tmp
	-perl /home/thepro/ti/mmwave_sdk_01_00_00_05/packages/scripts/ImageCreator/xwr16xx/append_bin_crc/gen_bincrc32.pl xwr16xx_mmw.bin
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

