XBE_TITLE = Super\ Mario\ War\ nxdk\ test
NXDK_DIR ?= $(CURDIR)/../../../nxdk
NXDK_SDL = y
NXDK_CXX = y
SMW_ROOT := $(CURDIR)/../..
SRC_COMMON := $(shell find $(SMW_ROOT)/src/common -name '*.cpp' ! -name 'sfx.cpp')
SRC_GAME := $(shell find $(SMW_ROOT)/src/smw -name '*.cpp' ! -path '*/platform/network/enet/*' ! -name 'NetConfigManager.cpp' ! -name 'FileCompressor.cpp' ! -name 'MenuTemplate.cpp')
SRCS := $(SRC_COMMON) $(SRC_GAME) $(CURDIR)/Platform.cpp $(CURDIR)/Filesystem.cpp $(CURDIR)/Audio.cpp $(CURDIR)/Vorbis.c $(CURDIR)/OfflineNetwork.cpp
CFLAGS += -O2 -DNDEBUG
CXXFLAGS += -std=c++20 -O2 -DNDEBUG -DNETWORK_DISABLED -DFMT_HEADER_ONLY -DFMT_EXCEPTIONS=0 -DFMT_USE_LOCALE=0 -DFMT_USE_NONTYPE_TEMPLATE_ARGS=0 -DFMT_CONSTEVAL=
# Stable source identity: use commit date, not the machine's build time.
# Tracked uncommitted edits are visible in the title/log and config stamp.
SMW_GIT_REVISION := $(shell git -C "$(SMW_ROOT)" rev-parse --short=12 HEAD)$(shell git -C "$(SMW_ROOT)" diff --quiet HEAD -- || printf '%s' '-dirty')
SMW_GIT_DATE := $(shell git -C "$(SMW_ROOT)" show -s --format=%cs HEAD)
CXXFLAGS += -DGIT_REVISION=\"$(SMW_GIT_REVISION)\" -DGIT_DATE=\"$(SMW_GIT_DATE)\"
CXXFLAGS += -I$(SMW_ROOT)/src/common -I$(SMW_ROOT)/src/common_netplay -I$(SMW_ROOT)/src/smw -I$(CURDIR) -I$(CURDIR)/vendor/fmt/include
CXXFLAGS += -Wno-c++11-narrowing -Wno-switch -Wno-unknown-pragmas
MENU_FRAME_CAPTURE ?= 0
ifeq ($(MENU_FRAME_CAPTURE),1)
CXXFLAGS += -DNXDK_MENU_FRAME_CAPTURE
endif
MENU_TEST_SPIN ?= 0
ifeq ($(MENU_TEST_SPIN),1)
ifneq ($(MENU_FRAME_CAPTURE),1)
$(error MENU_TEST_SPIN requires MENU_FRAME_CAPTURE)
endif
CXXFLAGS += -DNXDK_MENU_TEST_SPIN
endif
AUDIO_DIAGNOSTICS ?= 0
ifeq ($(AUDIO_DIAGNOSTICS),1)
CXXFLAGS += -DNXDK_AUDIO_HW_DIAGNOSTICS
endif
AUDIO_STATUS_HUD ?= 0
ifeq ($(AUDIO_STATUS_HUD),1)
CXXFLAGS += -DNXDK_AUDIO_STATUS_HUD
endif
AUDIO_INIT_STATE_LOG ?= 0
ifeq ($(AUDIO_INIT_STATE_LOG),1)
CXXFLAGS += -DNXDK_AUDIO_INIT_STATE_LOG
endif
AUDIO_FAILURE_DMA_LOG ?= 0
ifeq ($(AUDIO_FAILURE_DMA_LOG),1)
CXXFLAGS += -DNXDK_AUDIO_FAILURE_DMA_LOG
endif
AUDIO_PCI_LOG ?= 0
ifeq ($(AUDIO_PCI_LOG),1)
ifneq ($(AUDIO_FAILURE_DMA_LOG),1)
$(error AUDIO_PCI_LOG=1 requires AUDIO_FAILURE_DMA_LOG=1)
endif
CXXFLAGS += -DNXDK_AUDIO_PCI_LOG
endif
# Experimental startup-only DMA restart protocol; never enabled implicitly.
AUDIO_LINK_RECOVERY ?= 0
ifeq ($(AUDIO_LINK_RECOVERY),1)
CXXFLAGS += -DNXDK_AUDIO_LINK_RECOVERY
endif
STARTUP_WATCHDOG_MS ?= 0
ifneq ($(STARTUP_WATCHDOG_MS),0)
CXXFLAGS += -DNXDK_STARTUP_WATCHDOG_MS=$(STARTUP_WATCHDOG_MS)
endif
STARTUP_TEST_HANG_STAGE ?= 0
ifneq ($(STARTUP_TEST_HANG_STAGE),0)
ifeq ($(STARTUP_WATCHDOG_MS),0)
$(error STARTUP_TEST_HANG_STAGE requires a startup watchdog)
endif
CXXFLAGS += -DNXDK_STARTUP_TEST_HANG_STAGE=$(STARTUP_TEST_HANG_STAGE)
endif
# Compile the selected SDK HAL source locally with diagnostic call wrappers.
# All XAudio symbols resolve here rather than pulling audio.obj from the SDK.
AUDIO_HAL_INIT_TRACE ?= 0
ifeq ($(AUDIO_HAL_INIT_TRACE),1)
ifeq ($(STARTUP_WATCHDOG_MS),0)
$(error AUDIO_HAL_INIT_TRACE=1 requires a startup watchdog)
endif
CXXFLAGS += -DNXDK_AUDIO_HAL_INIT_TRACE
SRCS += $(CURDIR)/AudioHalInitTrace.c
endif
AUDIO_START_SETTLE_EXPERIMENT ?= 0
ifeq ($(AUDIO_START_SETTLE_EXPERIMENT),1)
ifeq ($(STARTUP_WATCHDOG_MS),0)
$(error AUDIO_START_SETTLE_EXPERIMENT=1 requires a startup watchdog)
endif
ifeq ($(AUDIO_HAL_INIT_TRACE),1)
$(error Select either HAL tracing or the paired start-delay experiment)
endif
ifeq ($(AUDIO_LINK_RECOVERY),1)
$(error Paired start-delay experiment must not enable a recovery protocol)
endif
CXXFLAGS += -DNXDK_AUDIO_START_SETTLE_EXPERIMENT
SRCS += $(CURDIR)/AudioStartSettle.c
endif
EXIT_WATCHDOG_MS ?= 0
AUDIO_CLOSE_EXPERIMENT ?= 0
AUDIO_CLEAN_SHUTDOWN ?= 0
ifeq ($(AUDIO_CLOSE_EXPERIMENT)$(AUDIO_CLEAN_SHUTDOWN),11)
$(error Select paired close experiment or compiled clean shutdown, not both)
endif
ifeq ($(AUDIO_CLEAN_SHUTDOWN),1)
CFLAGS += -DNXDK_AUDIO_CLEAN_SHUTDOWN
endif
ifneq ($(filter 1,$(AUDIO_CLOSE_EXPERIMENT) $(AUDIO_CLEAN_SHUTDOWN)),)
ifeq ($(STARTUP_WATCHDOG_MS),0)
$(error Audio close interposer requires a startup watchdog)
endif
ifeq ($(EXIT_WATCHDOG_MS),0)
$(error Audio close interposer requires an exit watchdog)
endif
ifneq ($(filter 1,$(AUDIO_HAL_INIT_TRACE) $(AUDIO_START_SETTLE_EXPERIMENT) $(AUDIO_LINK_RECOVERY)),)
$(error Close experiment must not combine HAL tracing, start delays or recovery)
endif
CXXFLAGS += -DNXDK_AUDIO_CLOSE_EXPERIMENT
SRCS += $(CURDIR)/AudioCloseExperiment.c
endif
PREFERENCES ?= 0
ifeq ($(PREFERENCES),1)
CXXFLAGS += -DNXDK_PREFERENCES
SRCS += $(CURDIR)/Preferences.cpp
endif
EXIT_WATCHDOG_MS ?= 0
ifneq ($(EXIT_WATCHDOG_MS),0)
CXXFLAGS += -DNXDK_EXIT_WATCHDOG_MS=$(EXIT_WATCHDOG_MS)
endif
# Opt-in deterministic PE timestamp; cxbe needs the companion SDK patch.
ifdef SOURCE_DATE_EPOCH
LDFLAGS += /timestamp:$(SOURCE_DATE_EPOCH)
endif
include $(NXDK_DIR)/Makefile

# nxdk does not otherwise track compiler-command changes. Rebuild game objects
# when switching diagnostic/preferences flags, without cleaning unrelated SDKs.
.PHONY: smw-port-config-force
$(CURDIR)/.build-config: smw-port-config-force
	@printf '%s\n' '$(CFLAGS) $(NXDK_CFLAGS)' '$(CXXFLAGS) $(NXDK_CXXFLAGS)' '$(LDFLAGS) $(NXDK_LDFLAGS)' 'SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH)' > '$@.tmp'
	@cmp -s '$@.tmp' '$@' || mv '$@.tmp' '$@'
	@rm -f '$@.tmp'
$(OBJS): $(CURDIR)/.build-config
