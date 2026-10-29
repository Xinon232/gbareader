#---------------------------------------------------------------------------------------------------------------------
# gbareader 10.0
#
# Stack: butano + devkitPro devkitARM, C++.
# Adapted from butano's examples/text/Makefile (canonical butano template).
#
# Build:    make LIBBUTANO=/absolute/path/to/butano/butano
# Test:     make test    (runs ROM in mgba debugger, checks for opcode errors)
# Clean:    make clean
#
# Requirements:
#   - devkitPro with devkitARM (DEVKITPRO and DEVKITARM set, e.g. /opt/devkitpro)
#   - butano (LIBBUTANO: its butano/ folder)
#---------------------------------------------------------------------------------------------------------------------

TARGET      :=  gbareader
BUILD       :=  build
LIBBUTANO   ?=  /path/to/butano/butano
PYTHON      :=  python3
SOURCES     :=  src gbasd/src gbasd/fatfs
INCLUDES    :=  include gbasd/include gbasd/fatfs references/superfw/src references/superfw/src/fonts
# butano treats INCLUDES as relative to CURDIR. Symlink the common
# headers we need into our own include dir so butano finds them.
DATA        :=
GRAPHICS    :=  graphics $(LIBBUTANO)/../common/graphics
AUDIO       :=
AUDIOBACKEND :=  null
AUDIOTOOL   :=
DMGAUDIO    :=
DMGAUDIOBACKEND :=  null
ROMTITLE    :=  GBA READER
ROMCODE     :=  AGBR
# Optional Supercard second-ROM-mirror transfers. Default 0 preserves the
# release-safe path; use `make SC_FAST_ROM_MIRROR=1 ...` only for hardware tests.
SC_FAST_ROM_MIRROR ?= 0
USERFLAGS   :=  -DSC_FAST_ROM_MIRROR=$(SC_FAST_ROM_MIRROR) -fstack-usage
# Butano's tables, cut to what the app uses (RAM): at most 48 sprites (24 UI
# glyphs + 24 overlay glyphs, one tile set each), the one bitmap background,
# no H-Blank effects. Going over is a Butano error screen, not memory damage.
USERFLAGS   +=  -DBN_CFG_SPRITES_MAX_ITEMS=64 -DBN_CFG_SPRITE_TILES_MAX_ITEMS=64 -DBN_CFG_HBES_MAX_ITEMS=1 \
                -DBN_CFG_BGS_MAX_ITEMS=1 -DBN_CFG_BG_BLOCKS_MAX_ITEMS=2
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
# Reproducible ROM: strip the checkout and butano locations from paths embedded in the
# ROM (__FILE__), so the same commit gives the same bytes in any folder or repository.
# The project root is fixed here because butano re-runs make from inside $(BUILD).
#---------------------------------------------------------------------------------------------------------------------
export GBAREADER_ROOT ?= $(CURDIR)
USERFLAGS   +=  -ffile-prefix-map=$(GBAREADER_ROOT)/= -ffile-prefix-map=$(LIBBUTANOABS)/=butano/

#---------------------------------------------------------------------------------------------------------------------
# Include main makefile:
#---------------------------------------------------------------------------------------------------------------------
include $(LIBBUTANOABS)/butano.mak

#---------------------------------------------------------------------------------------------------------------------
# Test target: run a short headless mGBA smoke check for immediate opcode/header errors.
# This is not proof of a successful boot or of SD/Supercard behavior.
#---------------------------------------------------------------------------------------------------------------------
ROM := $(TARGET).gba

.PHONY: test host-test memory-check
memory-check:
	@python3 tests/check_memory.py $(TARGET).elf --build $(BUILD)
	@python3 gbasd/tests/check_ram_code.py $(TARGET).elf ezfo_read ezfo_write ezfo_detect
host-test:
	@./tests/run_host_tests.sh

test:
	@test -f $(ROM) || { echo "ERROR: build $(ROM) first"; exit 1; }
	@command -v mgba >/dev/null 2>&1 || { echo "SKIP: mgba executable is not installed; no emulator claim"; exit 2; }
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
	@echo "PASS: no immediate opcode/header rejection (not a boot or hardware-I/O proof)"
	@echo "Test log tail:"
	@tail -10 $(BUILD)/test.log 2>/dev/null || true
