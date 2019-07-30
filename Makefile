#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of object file images to be generated ()
#   GEN_BINS - list of binaries to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
TARGET = eagle
FLAVOR = release
BUILD =
#FLAVOR = debug

#EXTRA_CCFLAGS += -u

ifndef PDIR # {
GEN_IMAGES= eagle.app.v6.out
GEN_BINS= eagle.app.v6.bin
SPECIAL_MKTARGETS=$(APP_MKTARGETS)
SUBDIRS=    \
	user	\
	common

endif # } PDIR

APPDIR = .
LDDIR = ../ld

CCFLAGS += -Os -Wpointer-arith -Wundef -Wpointer-sign -Wreturn-type -Wunused-variable -Wl,-EL \
			-fno-inline-functions -fsigned-char \
			-nostdlib -mlongcalls -mtext-section-literals \
			-D__ets__ -fPIC -DBUTTON

ifeq ($(HW),plus)
	CCFLAGS += -DIQS
endif

ifeq ($(DEBUG), 1)
	CCFLAGS += -DDEBUG_PRINTF
	BUILD := develop
else
	BUILD := release
endif

TARGET_LDFLAGS =		\
	-nostdlib		\
	-Wl,-EL \
	--longcalls \
	--text-section-literals

ifeq ($(FLAVOR),debug)
    TARGET_LDFLAGS += -g -O2
endif

ifeq ($(FLAVOR),release)
    TARGET_LDFLAGS += -Os
endif

COMPONENTS_eagle.app.v6 = \
	user/libuser.a	\
	common/libcommon.a

LINKFLAGS_eagle.app.v6 = \
	-L../lib        \
	-nostdlib	\
	-T$(LD_FILE)   \
	-Wl,--no-check-sections	\
	-u call_user_start	\
	-Wl,-static		\
	-Xlinker --gc-sections	\
	-Wl,--start-group	\
	-lmicroc \
	-lg \
	-lgcc	\
	-lphy	\
	-lpp	\
	-lnet80211	\
	-llwip_536	\
	-lwpa	\
	-lcrypto	\
	-lmain	\
	-lupgrade\
	-lssl	\
	-lwps	\
	$(DEP_LIBS_eagle.app.v6)	\
	-Wl,--end-group	\
	-fPIC \
	-Xlinker -Map=output.map

DEPENDS_eagle.app.v6 = \
                $(LD_FILE) \
                $(LDDIR)/eagle.rom.addr.v6.ld

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#

#UNIVERSAL_TARGET_DEFINES =		\

# Other potential configuration flags include:
#	-DTXRX_TXBUF_DEBUG
#	-DTXRX_RXBUF_DEBUG
#	-DWLAN_CONFIG_CCX
CONFIGURATION_DEFINES =	-DICACHE_FLASH

DEFINES +=				\
	$(UNIVERSAL_TARGET_DEFINES)	\
	$(CONFIGURATION_DEFINES)

DDEFINES +=				\
	$(UNIVERSAL_TARGET_DEFINES)	\
	$(CONFIGURATION_DEFINES)


#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES := $(INCLUDES) -I$(PDIR)include -I$(PDIR)common
PDIR := ../$(PDIR)
INCLUDES += -I $(PDIR)include
sinclude $(PDIR)Makefile

IMG_BIN_PATH = ../bin/upgrade/user1.2048.new.5.bin

simple:
	make COMPILE=gcc BOOT=new APP=1 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE_MAP=5 HW=simple
	$(eval VERSION:=$(shell ./version.sh include/version.h))
	cp $(IMG_BIN_PATH) ../bin/upgrade/Simple-FW-$(VERSION)-$(BUILD).bin

plus:
	make COMPILE=gcc BOOT=new APP=1 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE_MAP=5 HW=plus
	$(eval VERSION:=$(shell ./version.sh include/version.h))
	cp $(IMG_BIN_PATH) ../bin/upgrade/Plus-FW-$(VERSION)-$(BUILD).bin


.PHONY: FORCE
FORCE:

