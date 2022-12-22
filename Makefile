######################################################################
# @author      : Enno Boland (mail@eboland.de)
# @file        : Makefile
# @created     : Sunday Dec 12, 2021 09:18:43 CET
######################################################################

NINJA_TARGETS := test benchmark install dist scan-build clang-format uninstall \
	all tidy doc coverage-html

MESON_FLAGS = -Dtest=true -Ddoc=internal -Db_coverage=true

SANATIZE = 0

CC = gcc

ifeq ($(PODMAN), 1)
	W = podman run --rm -ti -v .:/host gottox/sqsh-build env
	BUILD_DIR = ./build_dir-podman
else
	W =
	BUILD_DIR = ./build_dir
	ifeq ($(SANATIZE),1)
		MESON_FLAGS += -Db_sanitize=address,undefined
	endif
endif
ifeq ($(CC),clang)
	MESON_FLAGS += -Dfuzzer=true
endif
ifeq ($(OPTIMIZE),1)
	MESON_FLAGS += --optimization=2
endif

.PHONY: $(NINJA_TARGETS)

$(NINJA_TARGETS): $(BUILD_DIR)
	$W ninja -C $< $@

$(BUILD_DIR): meson.build Makefile
	[ -d "$@" ] && rm -r "$@" || true
	$W CC=$(CC) meson setup "$@" $(MESON_FLAGS)

.PHONY: clean

clean:
	[ -d "$(BUILD_DIR)" ] && rm -r $(BUILD_DIR) || true
