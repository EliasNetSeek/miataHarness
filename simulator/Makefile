##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#

#
# As of Sep 2019, this is known to compile on Linux
# in my case I had to
# sudo apt-get install gcc-multilib g++-multilib
# 
# todo: automatically execute that command same as we automatically fetch submodules?
# 



CHIBIOS = ../firmware/ChibiOS
CHIBIOS_CONTRIB= ../firmware/ChibiOS-Contrib
RULESPATH = $(CHIBIOS)/os/common/startup/SIMIA32/compilers/GCC
RULESFILE = $(RULESPATH)/rules.mk

PROJECT_CPU=simulator

include ../firmware/rusefi.mk
RULESFILE = ../firmware/rusefi_rules.mk

# Configure precompiled header
PCH_DIR = $(PROJECT_DIR)/pch
PCHSRC = $(PCH_DIR)/pch.h
PCHSUB = simulator

# used by USE_SMART_BUILD
CONFDIR = .

ifneq ($(OS),Windows_NT)
  SANITIZE = yes
else
  SANITIZE = no
endif

# Compiler options here.
ifeq ($(USE_OPT),)
  USE_OPT = -Wall -Wno-error=implicit-fallthrough -Wno-error=write-strings -Wno-error=strict-aliasing

# If you would like to debug the simulator, swap which of these lines is commented. It's commented because debug info causes a 10-15x increase in binary size.
  USE_OPT += -O2
#  USE_OPT += -O0 -g

  ifeq ($(OS),Windows_NT)
    USE_OPT += -DEFI_SIM_IS_WINDOWS=1
  else
    USE_OPT += -m32 -DEFI_SIM_IS_WINDOWS=0
  endif
endif

# See explanation in main firmware Makefile for these three defines
USE_OPT += $(RUSEFI_OPT) -DEFI_UNIT_TEST=0 -DEFI_PROD_CODE=0 -DEFI_SIMULATOR=1

# Enable address sanitizer, but not on Windows since x86_64-w64-mingw32-g++ doesn't support it.
ifeq ($(SANITIZE),yes)
  USE_OPT += -fsanitize=address
endif

ifeq ($(CCACHE_DIR),)
  $(info No CCACHE_DIR)
else
  $(info CCACHE_DIR is ${CCACHE_DIR})
  CCPREFIX=ccache
endif

# C specific options here (added to USE_OPT).
ifeq ($(USE_COPT),)
  USE_COPT = -std=gnu17
endif

# C++ specific options here (added to USE_OPT).
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -std=c++2a -Wno-register -fno-rtti
endif

# Enable this if you want the linker to remove unused code and data.
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Linker extra options here.
ifeq ($(USE_LDOPT),)
  USE_LDOPT = 
endif

# Enable this if you want link time optimizations (LTO)
ifeq ($(USE_LTO),)
  USE_LTO = no
endif

# Enable this if you want to see the full log while compiling.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

# If enabled, this option makes the build process faster by not compiling
# modules not used in the current configuration.
ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = no
endif

#
# Build global options
##############################################################################

##############################################################################
# Architecture or project specific options
#

#
# Architecture or project specific options
##############################################################################

##############################################################################
# Project, sources and paths
#

# Define project name here
ifeq ($(OS),Windows_NT)
  PROJECT = rusefi_simulator.exe
else
  PROJECT = rusefi_simulator
endif
PROJECT_DIR = ../firmware

# Imported source files and paths

# Configure libfirmware Paths/Includes
RUSEFI_LIB = $(PROJECT_DIR)/libfirmware
include $(RUSEFI_LIB)/util/util.mk
include $(RUSEFI_LIB)/pt2001/pt2001.mk

# Licensing files.
include $(CHIBIOS)/os/license/license.mk
# Startup files.
# HAL-OSAL files (optional).
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/boards/simulator/board.mk
include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk
# RTOS files (optional).
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/SIMIA32/compilers/GCC/port.mk
# Other files (optional).
include $(CHIBIOS)/os/hal/lib/streams/streams.mk
#include $(CHIBIOS)/os/various/cpp_wrappers/chcpp.mk

ifeq ($(OS),Windows_NT)
  include ${CHIBIOS}/os/hal/ports/simulator/win32/platform.mk
else
  include ${CHIBIOS}/os/hal/ports/simulator/posix/platform.mk
endif

include $(PROJECT_DIR)/console/binary/tunerstudio.mk
include $(PROJECT_DIR)/console/console.mk
include $(PROJECT_DIR)/common.mk


# C sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CSRC =  $(ALLCSRC) \
  $(RUSEFI_LIB_C)


# C++ sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CPPSRC = $(ALLCPPSRC) \
  $(CHIBIOS)/os/various/cpp_wrappers/ch.cpp \
  $(HW_LAYER_DRIVERS_CPP) \
  $(HW_LAYER_DRIVERS_CORE_CPP) \
  $(CONSOLE_SRC_CPP) \
  $(DEV_SRC_CPP) \
  simulator/rusEfiFunctionalTest.cpp \
  simulator/can/hal_can_lld.cpp \
  simulator/framework.cpp \
  simulator/boards.cpp \
  $(TEST_SRC_CPP) \
  $(RUSEFI_LIB_CPP) \
  main.cpp


# List ASM source files here
ASMSRC = $(PORTASM)

INCDIR = . \
  $(PCH_DIR) \
  $(ALLINC) \
  $(CHIBIOS)/os/various/cpp_wrappers \
  $(PROJECT_DIR)/hw_layer/drivers/can \
  ${CHIBIOS}/os/various \
  $(PROJECT_DIR)/development/hw_layer \
  $(CHIBIOS)/os/hal/lib/streams \
  $(RUSEFI_LIB_INC) \
  simulator/can \
  simulator

# List ASM source files here
ASMSRC =
ASMXSRC = $(STARTUPASM) $(PORTASM) $(OSALASM)

#
# Project, sources and paths
##############################################################################

##############################################################################
# Compiler settings
#

$(info OS is [${OS}])

ifeq ($(OS),Windows_NT)
  # ChibiOS seem to require 32 bit compiler at least on Windows
  # base 32-bit Cygwin is needed with mingw64 32-bit version 
  # Cygwin64 would not debug if used with mingw64 32-bit version
  # 32 bit GDB available at http://www.equation.com/servlet/equation.cmd?fa=gdb

  TRGT = i686-w64-mingw32-
else
  TRGT = 
endif

CC   = $(CCPREFIX) $(TRGT)gcc
CPPC = $(CCPREFIX) $(TRGT)g++
# Enable loading with g++ only if you need C++ runtime support.
# NOTE: You can use C++ even without C++ support if you are careful. C++
#       runtime support makes code size explode.
#LD   = $(CC)
LD   = $(CPPC)
CP   = $(TRGT)objcopy
AS   = $(CC) -x assembler-with-cpp
AR   = $(TRGT)ar
OD   = $(TRGT)objdump
SZ   = $(TRGT)size
BIN  = $(CP) -O binary
COV  = gcov

# Define C warning options here
CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes

# Define C++ warning options here
CPPWARN = -Wall -Wextra -Wundef

#
# Compiler settings
##############################################################################

##############################################################################
# Start of user section
#

# List all user C define here, like -D_DEBUG=1
UDEFS = -DSIMULATOR
DDEFS += -DFIRMWARE_ID=\"simulator\" -DSHORT_BOARD_NAME=f407-discovery


# Define ASM defines here
UADEFS =

# List all user directories here
UINCDIR =

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ifeq ($(OS),Windows_NT)
ULIBS = -lws2_32 -static
else
ULIBS =
endif

ifeq ($(SANITIZE),yes)
	ULIBS += -fsanitize=address
endif

#
# End of user defines
##############################################################################

include $(RULESPATH)/rules.mk

# Enable precompiled header
include $(PROJECT_DIR)/rusefi_pch.mk
