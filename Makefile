CXX ?= c++
ARM_CXX ?= arm-none-eabi-c++
AUDIT_TEST_DIR ?= /tmp/capicola-audit-tests
BUILD_DIR ?= build
PLUGIN := $(BUILD_DIR)/plugins/capicola.o
COMMON_INCLUDES := -Iinclude -Ivendor/capicola/lib

.PHONY: all clean verify verify-audit verify-license test-upstream test-live plugin inspect-plugin source-package

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

test-live:
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

$(PLUGIN): src/capicola_nt.cpp include/capicola_nt/live_path.h
	mkdir -p "$(@D)"
	$(ARM_CXX) -std=gnu++17 -mcpu=cortex-m7 -mfpu=fpv5-d16 \
		-mfloat-abi=hard -mthumb -fno-rtti -fno-exceptions -Os -fPIC \
		-ffunction-sections -fdata-sections -Wall -Wextra -Werror \
		-Ivendor/distingNT_API/include $(COMMON_INCLUDES) -c -o "$@" $<

inspect-plugin: $(PLUGIN)
	arm-none-eabi-readelf -h "$(PLUGIN)" | grep -q 'Class:[[:space:]]*ELF32'
	arm-none-eabi-readelf -h "$(PLUGIN)" | grep -q 'Data:[[:space:]]*2.s complement, little endian'
	arm-none-eabi-readelf -h "$(PLUGIN)" | grep -q 'Type:[[:space:]]*REL (Relocatable file)'
	arm-none-eabi-readelf -h "$(PLUGIN)" | grep -q 'Machine:[[:space:]]*ARM'
	arm-none-eabi-nm -g "$(PLUGIN)" | grep -q ' T pluginEntry$$'

source-package: verify-license
	python3 tools/build_source_archive.py

clean:
	rm -rf "$(BUILD_DIR)"
