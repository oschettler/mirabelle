# Bequeme Hülle um CMake, damit "make test" das tut, was die Dokumentation sagt.
#
#   make            übersetzen
#   make test       Tests laufen lassen
#   make test ACCEPT=1   abweichende Sollbilder übernehmen
#   make clean      Baumverzeichnis wegwerfen

BUILD  ?= build
ACCEPT ?=

.PHONY: all test clean

all: $(BUILD)/CMakeCache.txt
	@cmake --build $(BUILD)

$(BUILD)/CMakeCache.txt:
	@cmake -S . -B $(BUILD)

test: all
	@PDA_GOLDEN_ACCEPT=$(ACCEPT) ctest --test-dir $(BUILD) --output-on-failure

clean:
	@rm -rf $(BUILD)
