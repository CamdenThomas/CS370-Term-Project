# carwatch — CS370 term project
# Systems core is C17 and must stay -Wall -Wextra -Werror clean (CLAUDE.md §2.4).

CC       ?= gcc
PY       ?= python3
CSTD      = -std=c17
WARN      = -Wall -Wextra -Werror -Wshadow -Wconversion -Wvla
OPT      ?= -O2 -g
CPPFLAGS += -Iinclude -D_GNU_SOURCE
CFLAGS   += $(CSTD) $(WARN) $(OPT)
LDLIBS   += -lm -lpthread

BUILD    = build
BIN      = $(BUILD)/bin

ALL_SRC    = $(wildcard src/*/*.c)
# Library objects: everything except the per-daemon entry points.
COMMON_SRC = $(filter-out %/main.c,$(ALL_SRC))
COMMON_OBJ = $(COMMON_SRC:%.c=$(BUILD)/%.o)

DAEMONS    = candaemon storaged analyzed supervisor obdctl
DAEMON_BIN = $(addprefix $(BIN)/,$(DAEMONS))

TEST_SRC   = $(wildcard tests/test_*.c)
TEST_BIN   = $(TEST_SRC:tests/%.c=$(BIN)/%)

.PHONY: all test asan memcheck soakcheck replay deploy clean boundary boardcheck \
        hwcheck hwdocs hwclean

all: $(DAEMON_BIN)

$(BIN)/candaemon:  $(BUILD)/src/obd/main.o        $(COMMON_OBJ)
$(BIN)/storaged:   $(BUILD)/src/store/main.o      $(COMMON_OBJ)
$(BIN)/analyzed:   $(BUILD)/src/analysis/main.o   $(COMMON_OBJ)
$(BIN)/supervisor: $(BUILD)/src/supervisor/main.o $(COMMON_OBJ)
$(BIN)/obdctl:     $(BUILD)/src/interface/main.o  $(COMMON_OBJ)

$(DAEMON_BIN):
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN)/test_%: tests/test_%.c $(COMMON_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: $(TEST_BIN) boundary boardcheck
	@fail=0; for t in $(TEST_BIN); do \
		printf '%-28s ' "$$(basename $$t)"; \
		if $$t; then echo PASS; else echo FAIL; fail=1; fi; done; \
	exit $$fail

# CLAUDE.md §2.1 — a grader must be able to confirm the boundary in sixty seconds.
# No network client anywhere in the product except src/interface/.
boundary:
	@if grep -rnE '\b(curl_|SSL_|getaddrinfo|gethostbyname|socket)\b' src \
	     --include='*.c' --include='*.h' | grep -v '^src/interface/'; then \
		echo "BOUNDARY VIOLATION: network symbol outside src/interface/"; exit 1; \
	else echo "boundary                     PASS"; fi

# docs/board.toml is a graded artifact too - every task, question and decision
# in the project is written there. Validate it in the same gate as the C.
boardcheck:
	@$(PY) tools/test_board.py

asan:
	$(MAKE) clean
	$(MAKE) test OPT="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"

memcheck: $(TEST_BIN)
	@for t in $(TEST_BIN); do valgrind --error-exitcode=1 --leak-check=full $$t || exit 1; done

soakcheck: all
	./soak/run_soak.sh --mini

replay: all
	@test -n "$(FIX)" || { echo "usage: make replay FIX=fixtures/<name>"; exit 1; }
	./tools/replay.py --fixture $(FIX) --label replay

deploy: all
	@test -n "$(PI)" || { echo "usage: make deploy PI=pi@<host>"; exit 1; }
	rsync -a --exclude build --exclude .git ./ $(PI):~/carwatch/
	ssh $(PI) 'cd ~/carwatch && make'

# ---------------------------------------------------------------- hardware
# KiCad 10. On Windows kicad-cli.exe is under
# "C:/Program Files/KiCad/10.0/bin/" and is NOT on PATH by default:
#   make hwcheck KICAD="/c/Program Files/KiCad/10.0/bin/kicad-cli.exe"
KICAD ?= kicad-cli
SCH    = electricalDrawing/electricalDrawing.kicad_sch

# ERC as a build failure — the hardware equivalent of -Werror (CLAUDE.md 7.12).
hwcheck:
	@command -v $(KICAD) >/dev/null 2>&1 || { \
		echo "kicad-cli not found. Set KICAD=/path/to/kicad-cli"; exit 1; }
	$(KICAD) sch erc --exit-code-violations --format json \
	         -o electricalDrawing/erc.json $(SCH)
	@echo "erc                          PASS"

# Regenerate everything derived from the schematic. Never hand-edit the outputs.
hwdocs:
	@mkdir -p docs/figures
	$(KICAD) sch export svg --exclude-drawing-sheet -o docs/figures/ $(SCH)
	$(KICAD) sch export bom \
	         --fields "Reference,Value,Footprint,MPN,Datasheet,Qty" \
	         --group-by Value -o docs/hardware/bom.csv $(SCH)
	$(KICAD) sch export netlist -o electricalDrawing/electricalDrawing.net $(SCH)
	@echo "Regenerated: schematic SVG, bom.csv, netlist."
	@echo "Commit the SVG — PRs touching hw must show a picture."

hwclean:
	rm -f electricalDrawing/erc.json electricalDrawing/drc.json

clean:
	rm -rf $(BUILD)

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
