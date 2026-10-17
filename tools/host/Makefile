# Host build (SPEC §10.1): unit tests, header checks and screen renders from the same src/ the device compiles.
# Run from the repository root: make -f tools/host/Makefile <target>   (tools/build.sh does this)
U8G2  ?= $(firstword $(wildcard /dev/shm/arduino-esp32/user/libraries/U8g2/src) /mnt/wd/arduino-esp32/user/libraries/U8g2/src)
BUILD ?= /dev/shm/arduino-esp32/build/hg2-host
ifeq ($(filter /dev/shm/% /mnt/wd/%,$(BUILD)),)
$(error BUILD must start with /dev/shm/ or /mnt/wd/ (the system disk is full), got '$(BUILD)')
endif
ifeq ($(wildcard $(U8G2)/clib/u8g2.h),)
$(error U8g2 clib not found at '$(U8G2)/clib' - run /mnt/wd/arduino-esp32/ramcache.sh)
endif

CXXFLAGS = -std=c++17 -O1 -g -DHG_HOST -Wall -Wextra -Werror -Isrc -I$(U8G2)/clib -Itools/host
DEPFLAGS = -MMD -MP
ifeq ($(MAKELEVEL),0)
MAKEFLAGS += -j$(shell nproc)
endif
MAKEFLAGS += --no-print-directory
SELF := $(firstword $(MAKEFILE_LIST))

U8G2_SRC := $(filter-out $(U8G2)/clib/mui%,$(wildcard $(U8G2)/clib/*.c))
U8G2_OBJ := $(patsubst $(U8G2)/clib/%.c,$(BUILD)/u8g2/%.o,$(U8G2_SRC))
LIBU8G2  := $(BUILD)/libu8g2.a

LIB_SRC    := $(wildcard src/core/*.cpp src/drivers/*.cpp src/ui/*.cpp src/ui/screens/*.cpp src/app/*.cpp)
FAKE_SRC   := $(wildcard tools/host/fakes/*.cpp)
TEST_SRC   := $(wildcard tools/host/tests/*.cpp)
FIXT_SRC   := $(wildcard tools/host/fixtures*.cpp)
RENDER_SRC := $(wildcard tools/host/bindings_doc.cpp tools/host/render_main.cpp)
HEADERS    := $(filter-out src/ui/strings_%.h,$(wildcard src/config/*.h src/core/*.h src/drivers/*.h src/ui/*.h \
                src/ui/screens/*.h src/app/*.h))
obj = $(patsubst %.cpp,$(BUILD)/obj/%.o,$(1))

.PHONY: all test sanitize headers render audit golden golden-update docs check clean
.DEFAULT_GOAL := all

all: headers $(BUILD)/hg_tests

# Unit tests; 'make test ONLY=substr' runs the matching tests only.
test: $(BUILD)/hg_tests
	$(BUILD)/hg_tests $(ONLY)

# The same tests under AddressSanitizer + UBSan, in a separate build dir (slower; not part of 'check').
sanitize:
	@$(MAKE) -f $(SELF) BUILD=$(BUILD)-san LDFLAGS="-fsanitize=address,undefined" \
	  CXXFLAGS="$(CXXFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer" test

# Every header must compile alone (include-what-you-use; SPEC §4.4).
headers: $(HEADERS:%=$(BUILD)/hdr/%.ok)
	@echo "headers: $(words $(HEADERS)) compile standalone"

$(BUILD)/hdr/%.h.ok: %.h
	@mkdir -p $(dir $@)
	@printf '#include "%s"\n' "$(CURDIR)/$<" > $(@:.ok=.cpp)
	@$(CXX) $(CXXFLAGS) -fsyntax-only -MMD -MP -MF $(@:.ok=.d) -MT $@ $(@:.ok=.cpp)
	@touch $@

$(BUILD)/obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	@$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/u8g2/%.o: $(U8G2)/clib/%.c
	@mkdir -p $(dir $@)
	@$(CC) -O1 -w -c $< -o $@

$(LIBU8G2): $(U8G2_OBJ)
	@rm -f $@
	@ar rcs $@ $^ && echo "AR  $@"

$(BUILD)/hg_tests: $(call obj,$(LIB_SRC) $(FAKE_SRC) $(FIXT_SRC) $(TEST_SRC)) $(LIBU8G2)
	@$(CXX) $(LDFLAGS) -o $@ $^ -lm && echo "LD  $@"

$(BUILD)/hg_render: $(call obj,$(LIB_SRC) $(FAKE_SRC) $(FIXT_SRC) $(RENDER_SRC)) $(LIBU8G2)
	@$(CXX) $(LDFLAGS) -o $@ $^ -lm && echo "LD  $@"

# Screens -> $(BUILD)/screens/NN_name.{pbm,png} + contact_sheet.png (never into docs/)
render: $(BUILD)/hg_render
	@rm -rf $(BUILD)/screens && mkdir -p $(BUILD)/screens
	$(BUILD)/hg_render $(BUILD)/screens
	@python3 tools/host/pbm2png.py $(BUILD)/screens
	$(BUILD)/hg_render --bindings > $(BUILD)/BUTTONS.md
	$(BUILD)/hg_render --bindings --lang ru > $(BUILD)/BUTTONS_RU.md

# Layout audit + keymap rules over every fixture and every sweep (§10.2, §10.3)
audit: $(BUILD)/hg_tests
	$(BUILD)/hg_tests layout_audit
	$(BUILD)/hg_tests keymap

# Pixel-exact comparison with the reviewed renders in tools/host/golden/ (keyed by fixture name).
golden: render
	@python3 tools/host/golden.py compare $(BUILD)/screens tools/host/golden $(BUILD)/golden_diff

golden-update: render
	@python3 tools/host/golden.py update $(BUILD)/screens tools/host/golden

docs: render
	@mkdir -p docs/screens
	cp $(BUILD)/screens/*.png docs/screens/
	cp $(BUILD)/BUTTONS.md $(BUILD)/BUTTONS_RU.md docs/

check:
	@$(MAKE) -f $(SELF) headers
	@$(MAKE) -f $(SELF) test
	@$(MAKE) -f $(SELF) golden
	@$(MAKE) -f $(SELF) audit

clean:
	rm -rf $(BUILD)

-include $(shell find $(BUILD)/obj $(BUILD)/hdr -name '*.d' 2>/dev/null)
