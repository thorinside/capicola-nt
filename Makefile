CXX ?= c++
AUDIT_TEST_DIR ?= /tmp/capicola-audit-tests

.PHONY: verify verify-audit test-upstream

verify: verify-audit test-upstream

verify-audit:
	python3 tools/verify_capability_audit.py

test-upstream:
	mkdir -p "$(AUDIT_TEST_DIR)"
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror \
		-Ivendor/capicola/lib \
		vendor/capicola/tests/host/capicola_tests.cpp \
		-o "$(AUDIT_TEST_DIR)/capicola_tests"
	"$(AUDIT_TEST_DIR)/capicola_tests"
