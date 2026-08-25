# SPDX-License-Identifier: GPL-3.0-or-later
# Bequeme Hülle um CMake, damit "make test" das tut, was die Dokumentation sagt.
#
#   make            übersetzen
#   make test       Tests laufen lassen
#   make test ACCEPT=1   abweichende Sollbilder übernehmen
#   make asan       Tests mit Address- und UB-Sanitizer laufen lassen
#   make clean      Baumverzeichnis wegwerfen

BUILD  ?= build
ACCEPT ?=

.PHONY: all test asan clean

all: $(BUILD)/CMakeCache.txt
	@cmake --build $(BUILD)

$(BUILD)/CMakeCache.txt:
	@cmake -S . -B $(BUILD)

test: all
	@PDA_GOLDEN_ACCEPT=$(ACCEPT) ctest --test-dir $(BUILD) --output-on-failure

# Eigenes Bauverzeichnis, damit der schnelle Lauf daneben bestehen bleibt.
asan:
	@cmake -S . -B $(BUILD)-asan -DPDA_SANITIZE=ON >/dev/null
	@cmake --build $(BUILD)-asan
	@ctest --test-dir $(BUILD)-asan --output-on-failure

clean:
	@rm -rf $(BUILD) $(BUILD)-asan
