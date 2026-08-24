CXX ?= c++
ARM_CXX ?= arm-none-eabi-c++
AUDIT_TEST_DIR ?= /tmp/capicola-audit-tests
BUILD_DIR ?= build
PLUGIN := $(BUILD_DIR)/plugins/capicola.o
CAPICOLA_SOURCE_DIR := vendor/capicola/lib
CAPICOLA_OVERLAY := $(BUILD_DIR)/capicola-overlay
CAPICOLA_OVERLAY_STAMP := $(CAPICOLA_OVERLAY)/.prepared
CAPICOLA_PATCHES := $(wildcard patches/capicola/*.patch)
CAPICOLA_HEADERS := $(wildcard $(CAPICOLA_SOURCE_DIR)/*.h)
COMMON_INCLUDES := -Iinclude -I$(CAPICOLA_OVERLAY)

.PHONY: all clean verify verify-audit verify-license test-upstream test-live plugin inspect-plugin source-package release-assets

all: plugin

verify: verify-audit verify-license test-upstream test-live plugin inspect-plugin

verify-audit:
	python3 tools/verify_capability_audit.py

verify-license:
	python3 tools/verify_license_release.py

test-upstream:
	mkdir -p "$(AUDIT_TEST_DIR)"
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror \
		-Ivendor/capicola/lib \
		vendor/capicola/tests/host/capicola_tests.cpp \
		-o "$(AUDIT_TEST_DIR)/capicola_tests"
	"$(AUDIT_TEST_DIR)/capicola_tests"

test-live: $(CAPICOLA_OVERLAY_STAMP)
	mkdir -p "$(AUDIT_TEST_DIR)"
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror \
		$(COMMON_INCLUDES) tests/live_path_tests.cpp \
		-o "$(AUDIT_TEST_DIR)/live_path_tests"
	"$(AUDIT_TEST_DIR)/live_path_tests"
	$(CXX) -std=gnu++17 -O2 -Wall -Wextra -Werror \
		-Ivendor/distingNT_API/include $(COMMON_INCLUDES) \
		src/capicola_nt.cpp tests/plugin_host_tests.cpp \
		-o "$(AUDIT_TEST_DIR)/plugin_host_tests"
	"$(AUDIT_TEST_DIR)/plugin_host_tests"

plugin: $(PLUGIN)

$(CAPICOLA_OVERLAY_STAMP): tools/prepare_capicola_overlay.sh $(CAPICOLA_PATCHES) $(CAPICOLA_HEADERS)
	sh tools/prepare_capicola_overlay.sh \
		"$(CAPICOLA_SOURCE_DIR)" "$(CAPICOLA_OVERLAY)" "patches/capicola"

$(PLUGIN): src/capicola_nt.cpp include/capicola_nt/live_path.h \
		include/capicola_nt/int64_to_double.h $(CAPICOLA_OVERLAY_STAMP)
	mkdir -p "$(@D)"
	$(ARM_CXX) -std=gnu++17 -mcpu=cortex-m7 -mfpu=fpv5-d16 \
		-mfloat-abi=hard -mthumb -fno-rtti -fno-exceptions -Os -fPIC \
		-Wall -Wextra -Werror \
		-Ivendor/distingNT_API/include $(COMMON_INCLUDES) -c -o "$@" $<

inspect-plugin: $(PLUGIN)
	arm-none-eabi-readelf -h "$(PLUGIN)" | grep -q 'Class:[[:space:]]*ELF32'
	arm-none-eabi-readelf -h "$(PLUGIN)" | grep -q 'Data:[[:space:]]*2.s complement, little endian'
	arm-none-eabi-readelf -h "$(PLUGIN)" | grep -q 'Type:[[:space:]]*REL (Relocatable file)'
	arm-none-eabi-readelf -h "$(PLUGIN)" | grep -q 'Machine:[[:space:]]*ARM'
	arm-none-eabi-nm -g "$(PLUGIN)" | grep -q ' T pluginEntry$$'
	python3 tools/verify_plugin_symbols.py "$(PLUGIN)"

source-package: verify-license
	python3 tools/build_source_archive.py

# These are the two adjacent downloadable assets for an approved tagged release.
release-assets: verify source-package

clean:
	rm -rf "$(BUILD_DIR)"
