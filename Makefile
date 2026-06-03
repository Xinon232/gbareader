#---------------------------------------------------------------------------------------------------------------------
# GBA Vocab Trainer — v1 Makefile
#
# Stack: butano + devkitPro devkitARM, C++.
# Adapted from /home/hlm/butano/examples/text/Makefile (canonical butano template).
#
# Build:    make
# Test:     make test    (runs ROM in mgba debugger, checks for opcode errors)
# Clean:    make clean
#
# Requirements:
#   - devkitPro installed at /opt/devkitpro
#   - DEVKITARM and DEVKITPRO env vars set (set in ~/.bashrc)
#   - butano at /home/hlm/butano/butano
#---------------------------------------------------------------------------------------------------------------------

TARGET      :=  vocab
BUILD       :=  build
LIBBUTANO   ?=  /path/to/butano/butano
PYTHON      :=  python3
SOURCES     :=  src
INCLUDES    :=  include
# butano treats INCLUDES as relative to CURDIR. Symlink the common
# headers we need into our own include dir so butano finds them.
DATA        :=
GRAPHICS    :=  graphics $(dir $(LIBBUTANO))../common/graphics
AUDIO       :=
AUDIOBACKEND :=  null
AUDIOTOOL   :=
DMGAUDIO    :=
DMGAUDIOBACKEND :=  null
ROMTITLE    :=  VOCAB TRAIN
ROMCODE     :=  AVTB
USERFLAGS   :=
USERCXXFLAGS :=
USERASFLAGS :=
USERLDFLAGS :=
USERLIBDIRS :=
USERLIBS    :=
DEFAULTLIBS :=
STACKTRACE  :=
USERBUILD   :=
EXTTOOL     :=

#---------------------------------------------------------------------------------------------------------------------
# Export absolute butano path:
#---------------------------------------------------------------------------------------------------------------------
ifndef LIBBUTANOABS
	export LIBBUTANOABS	:=	$(realpath $(LIBBUTANO))
endif

#---------------------------------------------------------------------------------------------------------------------
# Include main makefile:
#---------------------------------------------------------------------------------------------------------------------
include $(LIBBUTANOABS)/butano.mak

#---------------------------------------------------------------------------------------------------------------------
# Test target: run the ROM through mgba headless for 5 seconds and check for opcode errors.
# Requires mgba-sdl (apt: sudo apt install mgba-sdl).
#---------------------------------------------------------------------------------------------------------------------
ROM := $(BUILD)/$(TARGET).gba

.PHONY: test
test: $(ROM)
	@echo "=== Testing $(ROM) in mgba ==="
	@rm -f $(BUILD)/test.log
	@SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
	 timeout 5 mgba -l 3 $(ROM) > $(BUILD)/test.log 2>&1 || true
	@if grep -q "Illegal opcode" $(BUILD)/test.log; then \
	   echo "FAIL: illegal opcodes (vector table bug)"; \
	   grep "Illegal opcode" $(BUILD)/test.log | head -3; \
	   exit 1; \
	 fi
	@if grep -q "Could not run game" $(BUILD)/test.log; then \
	   echo "FAIL: mgba rejected the ROM (header invalid)"; \
	   cat $(BUILD)/test.log; \
	   exit 1; \
	 fi
	@echo "PASS: no opcode errors, ROM is bootable"
	@echo "Test log tail:"
	@tail -10 $(BUILD)/test.log 2>/dev/null || true
