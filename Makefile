BUILD_TYPE := Debug
BUILD_DIR := build/$(shell echo $(BUILD_TYPE) | tr A-Z a-z)
PLUGIN_AU_CODE := ZtRm
PLUGIN_MFR_CODE := DsEr

.PHONY: all release clean validate test install uninstall check-tools

all: build-plugin

# Prerequisite checks -- auto-install via Homebrew
check-tools:
	@xcode-select -p >/dev/null 2>&1 || { echo "== Installing Xcode Command Line Tools =="; xcode-select --install; echo "Re-run make after installation completes."; exit 1; }
	@command -v cmake >/dev/null 2>&1 || { command -v brew >/dev/null 2>&1 || { echo "Error: cmake not found and Homebrew not available. Install cmake manually."; exit 1; }; echo "== Installing CMake =="; brew install cmake || { echo "Error: Failed to install cmake."; exit 1; }; }
	@command -v ninja >/dev/null 2>&1 || { command -v brew >/dev/null 2>&1 || { echo "Error: ninja not found and Homebrew not available. Install ninja manually."; exit 1; }; echo "== Installing Ninja =="; brew install ninja || { echo "Error: Failed to install ninja."; exit 1; }; }

# Ensure JUCE submodule is initialized
lib/JUCE/CMakeLists.txt:
	@echo "== Initializing JUCE submodule =="
	@git submodule update --init --recursive

# CMake configure -- re-runs if CMakeLists.txt changes
$(BUILD_DIR)/build.ninja: CMakeLists.txt lib/JUCE/CMakeLists.txt | check-tools
	@echo "== Configuring ($(BUILD_TYPE)) =="
	@cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

# Build
build-plugin: $(BUILD_DIR)/build.ninja
	@echo "== Building =="
	@cmake --build $(BUILD_DIR)
	@echo "== Build complete =="

# Release build
release:
	@$(MAKE) BUILD_TYPE=Release build-plugin

# Install alias (build handles install via COPY_PLUGIN_AFTER_BUILD)
install: build-plugin

# Clean
clean:
	@echo "== Cleaning =="
	@rm -rf build
	@echo "== Clean complete =="

# Validate AU with auval
validate: build-plugin
	@echo "== Validating AU plugin =="
	@killall -9 AudioComponentRegistrar 2>/dev/null; true
	@sleep 1
	@auval -v aufx $(PLUGIN_AU_CODE) $(PLUGIN_MFR_CODE)

# Run tests
test: build-plugin
	@echo "== Running tests =="
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Uninstall
uninstall:
	@echo "== Uninstalling =="
	@rm -rf "$(HOME)/Library/Audio/Plug-Ins/VST3/Zeitraum.vst3"
	@rm -rf "$(HOME)/Library/Audio/Plug-Ins/Components/Zeitraum.component"
	@killall -9 AudioComponentRegistrar 2>/dev/null; true
	@echo "== Uninstalled =="
