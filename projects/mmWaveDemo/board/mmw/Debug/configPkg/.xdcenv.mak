#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = /home/thepro/ti/bios_6_51_00_15/packages;/home/thepro/ti/mathlib_c674x_3_1_2_1/packages;/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages;/home/thepro/ti/ccsv7/ccs_base
override XDCROOT = /home/thepro/ti/xdctools_3_50_01_12_core
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = /home/thepro/ti/bios_6_51_00_15/packages;/home/thepro/ti/mathlib_c674x_3_1_2_1/packages;/home/thepro/ti/dsplib_c64Px_3_4_0_0/packages;/home/thepro/ti/ccsv7/ccs_base;/home/thepro/ti/xdctools_3_50_01_12_core/packages;..
HOSTOS = Linux
endif
