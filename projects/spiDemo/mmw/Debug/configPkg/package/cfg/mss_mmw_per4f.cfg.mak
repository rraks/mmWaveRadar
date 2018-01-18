# invoke SourceDir generated makefile for mss_mmw.per4f
mss_mmw.per4f: .libraries,mss_mmw.per4f
.libraries,mss_mmw.per4f: package/cfg/mss_mmw_per4f.xdl
	$(MAKE) -f /home/thepro/Documents/mmwave_workspace/mmw/src/makefile.libs

clean::
	$(MAKE) -f /home/thepro/Documents/mmwave_workspace/mmw/src/makefile.libs clean

