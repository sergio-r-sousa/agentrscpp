# Makefile — Convenience wrapper around CMake for agentrscpp.
#
# Usage:
#   make              # Build in Release mode
#   make debug        # Build in Debug mode (ASan + UBSan)
#   make release      # Build in Release mode (-O3)
#   make clean        # Remove build artifacts
#   make test         # Build and run tests
#   make examples     # Build examples only
#   make rebuild      # Clean + rebuild in Release mode

BUILD_DIR     := build
BUILD_DIR_DBG := build-debug
JOBS          := $(shell nproc 2>/dev/null || echo 4)

.PHONY: all debug release clean test examples rebuild help

# ── Default target ─────────────────────────────────────────────────────────

all: release

# ── Release build ──────────────────────────────────────────────────────────

release:
	@-mkdir $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. \
		-DCMAKE_BUILD_TYPE=Release \
		-DAGENTRS_BUILD_EXAMPLES=ON \
		-DAGENTRS_BUILD_TESTS=OFF
	cd $(BUILD_DIR) && cmake --build . -j$(JOBS)

# ── Debug build (sanitizers enabled) ───────────────────────────────────────

debug:
	@-mkdir $(BUILD_DIR_DBG)
	cd $(BUILD_DIR_DBG) && cmake .. \
		-DCMAKE_BUILD_TYPE=Debug \
		-DAGENTRS_BUILD_EXAMPLES=ON \
		-DAGENTRS_BUILD_TESTS=ON
	cd $(BUILD_DIR_DBG) && cmake --build . -j$(JOBS)

# ── Clean ──────────────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR_DBG)

# ── Tests ──────────────────────────────────────────────────────────────────

test: debug
	cd $(BUILD_DIR_DBG) && ctest --output-on-failure -j$(JOBS)

# ── Examples only ──────────────────────────────────────────────────────────

examples:
	@-mkdir $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. \
		-DCMAKE_BUILD_TYPE=Release \
		-DAGENTRS_BUILD_EXAMPLES=ON \
		-DAGENTRS_BUILD_TESTS=OFF
	cd $(BUILD_DIR) && cmake --build . -j$(JOBS)

# ── Rebuild ────────────────────────────────────────────────────────────────

rebuild: clean release

# ── Help ───────────────────────────────────────────────────────────────────

help:
	@echo "agentrscpp build targets:"
	@echo "  make            Build in Release mode (default)"
	@echo "  make debug      Build in Debug mode with sanitizers"
	@echo "  make release    Build in Release mode (-O3)"
	@echo "  make clean      Remove all build artifacts"
	@echo "  make test       Build debug + run tests"
	@echo "  make examples   Build examples (Release)"
	@echo "  make rebuild    Clean + release build"
	@echo "  make help       Show this message"
