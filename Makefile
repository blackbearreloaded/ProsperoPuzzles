# ps5-native-app-boilerplate - Linux/WSL build entry points.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

SHELL := /bin/bash
.DEFAULT_GOAL := app

-include .env

APP_DEFINITIONS ?=
APP_INCLUDE_PATHS ?=
APP_STATIC_ARCHIVES ?=
APP_IMPORT_STUBS ?=
APP_RUNTIME_MODULES ?=
# Empty selects the pinned ps5-opengl release (tools/fetch-opengl-sdk.sh).
PS5_OPENGL_PREFIX ?=

# ProsperoPuzzles always builds against ps5-opengl; user APP_* values append.
OPENGL_SDK := .deps/ps5-opengl/current
# COMBINED/NO_TGMATH_H configure the vendored Tatham puzzles (the PS5 SDK's
# <tgmath.h> needs complex functions its libc does not declare).
override APP_DEFINITIONS := $(strip GL_GLEXT_PROTOTYPES=1 COMBINED NO_TGMATH_H $(APP_DEFINITIONS))
override APP_INCLUDE_PATHS := $(strip src src/third_party/sgt-puzzles $(OPENGL_SDK)/include \
	$(APP_INCLUDE_PATHS))
override APP_STATIC_ARCHIVES := $(strip .deps/ps5-opengl/libps5opengl-group.a $(APP_STATIC_ARCHIVES))
override APP_IMPORT_STUBS := $(strip $(OPENGL_SDK)/lib/libSceAgc.so \
	$(OPENGL_SDK)/lib/libSceAgcDriver.so $(APP_IMPORT_STUBS))
PACBREW_PACKAGES ?=
APP_WRAP_SYMBOLS ?=
APP_ROOT_FILES ?=
# Filesystem access (docs/STORAGE.md): the package carries upstream Lapy's
# exact-title one-shot helper. 0 leaves it out; the app then stays sandboxed.
APP_LAPY_HELPER ?= 1
# The update check and self-update (docs/UPDATES.md): libcurl with its fcntl
# wrapper, and the helper the app sends to the console's payload loader.
SELF_UPDATE_HELPER := build/self-update/self-updater.elf
override PACBREW_PACKAGES := $(strip libcurl $(PACBREW_PACKAGES))
override APP_WRAP_SYMBOLS := $(strip fcntl $(APP_WRAP_SYMBOLS))
override APP_ROOT_FILES := $(strip $(SELF_UPDATE_HELPER) $(APP_ROOT_FILES))
PACBREW_INCLUDE_PATHS ?=
PACBREW_STATIC_ARCHIVES ?=
PS5_HOST ?=
FTP_PORT ?= 2121
DEPLOY_FORMAT ?= folder
PS5_FTP_USER ?= anonymous
PS5_FTP_PASSWORD ?= codex
DEPLOY_DRY_RUN ?= 0
TITLE_ID ?=
APP_NAME ?=
APP_CATEGORY ?= game
CONTENT_SUFFIX ?=
HOST_CXX ?= clang++
HOST_CC ?= clang
HOST_TEST_CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
	-ffunction-sections -fdata-sections
HOST_TEST_LDFLAGS ?= -Wl,--gc-sections
GTEST_ARGS ?=
BUILD_JOBS ?= $(shell nproc 2>/dev/null || echo 2)
USE_CCACHE ?= 1
export BUILD_JOBS USE_CCACHE
export HOST_CXX HOST_TEST_CXXFLAGS HOST_TEST_LDFLAGS
export APP_DEFINITIONS APP_INCLUDE_PATHS APP_STATIC_ARCHIVES APP_IMPORT_STUBS APP_RUNTIME_MODULES
export PS5_OPENGL_PREFIX
export APP_WRAP_SYMBOLS APP_ROOT_FILES APP_LAPY_HELPER
export PACBREW_PACKAGES PACBREW_INCLUDE_PATHS PACBREW_STATIC_ARCHIVES
export PS5_HOST FTP_PORT DEPLOY_FORMAT PS5_FTP_USER PS5_FTP_PASSWORD DEPLOY_DRY_RUN
export TITLE_ID APP_NAME APP_CATEGORY CONTENT_SUFFIX

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh tools/build-host-tools.sh tools/ninja-build.sh \
	$(wildcard tooling/native/*.cpp tooling/native/*.hpp) \
	$(wildcard tooling/native/runtime/*.txt)
HOST_UNIT_TEST := build/tests/unit_tests

.PHONY: all app build init doctor test test-deps test-unit test-integration test-elevation test-update-check test-self-update self-update-helper libc deps opengl host-snapshots host-transition fonts pacbrew pacbrew-list assets-check format format-check tidy lint check ffpkg deploy undeploy clean distclean help

all: app
build: app

init:
	@printf '%s\n' '==> [init] Configuring the application identity in sce_sys/param.json'
	@bash tools/init-project.sh sce_sys/param.json

doctor:
	@printf '%s\n' '==> [doctor] Checking the Linux/WSL host without changing it'
	@bash tools/doctor.sh

test: test-unit test-integration test-elevation test-update-check test-self-update

test-elevation:
	@printf '%s\n' '==> [test-elevation] Running the Lapy client exchange and proof checks'
	@bash tools/setup-native-dependencies.sh >/dev/null
	@mkdir -p build/tests
	@$(HOST_CXX) $(HOST_TEST_CXXFLAGS) -Isrc -idirafter .deps/native/ps5-payload-sdk/target/include \
		tests/kits/test_elevation.cpp $(HOST_TEST_LDFLAGS) -o build/tests/test_elevation
	@build/tests/test_elevation

test-update-check:
	@printf '%s\n' '==> [test-update-check] Running the catalog answer and version checks'
	@mkdir -p build/tests
	@$(HOST_CXX) $(HOST_TEST_CXXFLAGS) -g -fsanitize=address,undefined -fno-sanitize-recover=all \
		-Isrc tests/kits/test_update_check.cpp $(HOST_TEST_LDFLAGS) -o build/tests/test_update_check
	@build/tests/test_update_check

test-self-update:
	@printf '%s\n' '==> [test-self-update] Running the update engine against the helper'
	@mkdir -p build/tests/self-update
	@for name in miniz miniz_tinfl miniz_tdef miniz_zip; do \
		$(HOST_CC) -std=c11 -O2 -w -g -fsanitize=address,undefined \
			-c third_party/miniz/$$name.c -o build/tests/self-update/$$name.o || exit 1; \
	done
	@$(HOST_CXX) $(HOST_TEST_CXXFLAGS) -g -fsanitize=address,undefined -fno-sanitize-recover=all \
		-Ithird_party -Isrc -Isrc/update \
		tests/kits/test_self_update.cpp payloads/self-update-helper/updater.cpp \
		payloads/self-update-helper/archive.cpp payloads/self-update-helper/files.cpp \
		build/tests/self-update/*.o -pthread $(HOST_TEST_LDFLAGS) -o build/tests/test_self_update
	@build/tests/test_self_update

self-update-helper:
	@printf '%s\n' '==> [self-update] Building the helper for the payload loader'
	@bash tools/setup-native-dependencies.sh >/dev/null
	@$(MAKE) --no-print-directory -s -C payloads/self-update-helper \
		PS5_PAYLOAD_SDK="$(CURDIR)/.deps/native/ps5-payload-sdk" \
		OUTPUT="$(CURDIR)/$(SELF_UPDATE_HELPER)"
	@python3 tools/validate-loader-elf.py "$(SELF_UPDATE_HELPER)"

test-deps:
	@printf '%s\n' '==> [test-deps] Fetching the pinned host-only GoogleTest source'
	@bash tools/setup-test-dependencies.sh >/dev/null

test-unit:
	@bash tools/build-tests.sh
	@printf '%s\n' '==> [test-unit] Running host-native GoogleTest application tests'
	@ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=tests/unit/lsan.supp \
		UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		$(HOST_UNIT_TEST) --gtest_brief=1 $(GTEST_ARGS)

test-integration:
	@printf '%s\n' '==> [test-integration] Running host tooling integration tests'
	@python3 -m unittest discover -s tests -p 'test_*.py' -v

deps: test-deps
	@printf '%s\n' '==> [deps] Fetching declared native dependencies'
	@bash tools/setup-native-dependencies.sh
	@bash tools/setup-pacbrew-dependencies.sh --environment
	@bash tools/prepare-opengl.sh

opengl:
	@printf '%s\n' '==> [opengl] Preparing the ps5-opengl SDK link group'
	@bash tools/prepare-opengl.sh

pacbrew:
	@printf '%s\n' '==> [pacbrew] Fetching the pinned prebuilt ports sysroot'
	@bash tools/setup-pacbrew-dependencies.sh --all

pacbrew-list:
	@printf '%s\n' '==> [pacbrew] Listing available pkg-config modules'
	@bash tools/setup-pacbrew-dependencies.sh --list

assets-check:
	@printf '%s\n' '==> [assets] Validating icon, backgrounds, and selection audio'
	@bash tools/validate-assets.sh

audio-check:
	@printf '%s\n' '==> [audio] Validating sound effects and music (PLAN.md Appendix A)'
	@python3 tools/audio-check.py

libc:
	@printf '%s\n' '==> [libc] Rebuilding and verifying the clean-room runtime'
	@bash tools/rebuild-libc.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@printf '%s\n' '==> [libc] Generating the missing or outdated runtime'
	@bash tools/rebuild-libc.sh

app: $(RUNTIME) opengl self-update-helper
	@printf '%s\n' '==> [app] Compiling, linking, signing, and assembling the app folder'
	@bash tools/build.sh Folder

ffpkg: $(RUNTIME) opengl self-update-helper
	@printf '%s\n' '==> [ffpkg] Building the app folder and UFS2 image'
	@bash tools/build.sh Ffpkg

deploy:
	@printf '%s\n' '==> [deploy] Building and publishing the selected app output over FTP'
	@bash tools/deploy.sh

undeploy:
	@printf '%s\n' '==> [undeploy] Removing staged development files for this title over FTP'
	@bash tools/deploy.sh undeploy

format:
	@printf '%s\n' '==> [format] Formatting C and C++ sources'
	@bash tools/run_clang_format.sh

format-check:
	@printf '%s\n' '==> [format] Checking C and C++ formatting'
	@bash tools/run_clang_format.sh --check

tidy:
	@printf '%s\n' '==> [tidy] Running Clang static analysis'
	@bash tools/run_clang_tidy.sh

lint:
	@printf '%s\n' '==> [lint] Running source, metadata, and shell checks'
	@bash tools/lint.sh

check: lint test app

host-snapshots:
	@printf '%s\n' '==> [host-snapshots] Rendering UI scenes to build/snapshots (Mesa llvmpipe)'
	@bash tools/host-snapshots.sh

host-transition:
	@printf '%s\n' '==> [host-transition] Writing the frames of leaving a game to build/transition'
	@mkdir -p build/transition/data
	@bash tools/host-transition.sh build/transition

fonts:
	@printf '%s\n' '==> [fonts] Baking SDF font atlases into assets/fonts'
	@bash tools/bake-fonts.sh

clean:
	@printf '%s\n' '==> [clean] Removing generated build outputs'
	@rm -rf -- build dist
	@rm -f -- $(RUNTIME)

distclean: clean
	@printf '%s\n' '==> [distclean] Removing downloaded dependency caches'
	@rm -rf -- .deps

help:
	@printf '%s\n' \
	  'make                 Generate libc.prx and build the ProsperoPuzzles folder' \
	  'make opengl          Fetch/verify the ps5-opengl SDK and write its link group' \
	  'make host-snapshots  Render UI scenes to PNG on the host (Mesa llvmpipe)' \
	  'make fonts           Rebake the SDF font atlases in assets/fonts' \
	  'make init TITLE_ID=PPSA12345 APP_NAME="My App"  Configure app identity' \
	  'make doctor          Check required and optional Linux/WSL tools' \
	  'make test            Run all host unit and integration tests' \
	  'make test-deps       Fetch verified host-only GoogleTest source' \
	  'make test-unit       Run host-native GoogleTest application tests' \
	  'make test-integration  Run host tooling integration tests' \
	  'make test-elevation    Run the Lapy client host tests' \
	  'make test-update-check Run the update-check host tests' \
	  'make test-self-update  Run the self-update host tests' \
	  'make self-update-helper  Build the helper the app sends to the payload loader' \
	  'make deps            Fetch native dependencies into .deps/' \
	  'make pacbrew         Fetch the pinned PacBrew ports sysroot' \
	  'make pacbrew-list    List PacBrew pkg-config module names' \
	  'make assets-check    Validate the current presentation assets' \
	  'make libc            Force a deterministic runtime/libc.prx rebuild' \
	  'make format          Apply the shared Clang formatting policy' \
	  'make format-check    Check formatting without modifying files' \
	  'make tidy            Run the shared Clang static-analysis policy' \
	  'make lint            Run format, tidy, metadata, and shell checks' \
	  'make check           Run lint and build the skeleton app' \
	  'make ffpkg           Build the folder and UFS2 .ffpkg image' \
	  'make deploy PS5_HOST=<address>  Build and FTP-deploy the app folder' \
	  'make undeploy PS5_HOST=<address>  Remove this title from /data/homebrew' \
	  'Build variables:     APP_DEFINITIONS, APP_INCLUDE_PATHS, APP_STATIC_ARCHIVES, APP_IMPORT_STUBS, APP_RUNTIME_MODULES' \
	  'OpenGL SDK:          PS5_OPENGL_PREFIX=<sdk dir with manifest.sha256> (default: pinned release)' \
	  'PacBrew variables:   PACBREW_PACKAGES, PACBREW_INCLUDE_PATHS, PACBREW_STATIC_ARCHIVES' \
	  'Filesystem access:   APP_LAPY_HELPER=0 leaves the Lapy helper out of the package' \
	  'Deploy variables:    FTP_PORT=2121, DEPLOY_FORMAT=folder|ffpkg, DEPLOY_DRY_RUN=0|1' \
	  'Local defaults:      Copy .env.example to the ignored .env file' \
	  'Build speed:         BUILD_JOBS defaults to all CPUs; USE_CCACHE=0 disables ccache' \
	  'make clean           Remove build/, dist/, and generated libc.prx' \
	  'make distclean       Also remove the ignored .deps/ cache'
