# carwatch — CS370 term project
# Systems core is C17 and must stay -Wall -Wextra -Werror clean (CLAUDE.md §2.4).

# CC is make's default, cc: gcc on the Pi and in CI, clang on macOS.

# The tool tests (tests/tools/, D-032) need Python 3.11 or newer, the version Raspberry Pi
# OS ships; the department machines put 3.9 on PATH as python3. Pick the first interpreter
# that is new enough; fall back to python3 so the error, if any, names the version.
PY       ?= $(shell for p in python3 python3.13 python3.12 python3.11; do \
              command -v $$p >/dev/null 2>&1 && \
              $$p -c 'import sys; sys.exit(sys.version_info < (3, 11))' >/dev/null 2>&1 && \
              { echo $$p; exit 0; }; \
            done; echo python3)
CSTD      = -std=c17
WARN      = -Wall -Wextra -Werror -Wshadow -Wconversion -Wvla
OPT      ?= -O2 -g
CPPFLAGS += -Iinclude -D_GNU_SOURCE
CFLAGS   += $(CSTD) $(WARN) $(OPT)
LDLIBS   += -lm -lpthread

BUILD    = build
BIN      = $(BUILD)/bin

ALL_SRC    = $(wildcard src/*/*.c)
# Library objects: everything except the per-daemon entry points, which are main.c and,
# where one directory holds two programs, <name>_main.c.
COMMON_SRC = $(filter-out %/main.c %_main.c,$(ALL_SRC))
COMMON_OBJ = $(COMMON_SRC:%.c=$(BUILD)/%.o)

DAEMONS    = obdd storaged analyzed supervisor obdctl statusd
DAEMON_BIN = $(addprefix $(BIN)/,$(DAEMONS))

TEST_SRC   = $(wildcard tests/test_*.c)
TEST_BIN   = $(TEST_SRC:tests/%.c=$(BIN)/%)

.PHONY: all test asan memcheck soakcheck replay deploy clean boundary boardcheck \
        hwcheck hwdocs hwclean toolcheck run-status

all: $(DAEMON_BIN)

$(BIN)/obdd:       $(BUILD)/src/obd/main.o        $(COMMON_OBJ)
$(BIN)/storaged:   $(BUILD)/src/store/main.o      $(COMMON_OBJ)
$(BIN)/analyzed:   $(BUILD)/src/analysis/main.o   $(COMMON_OBJ)
$(BIN)/supervisor: $(BUILD)/src/supervisor/main.o $(COMMON_OBJ)
$(BIN)/obdctl:     $(BUILD)/src/interface/main.o  $(COMMON_OBJ)
$(BIN)/statusd:    $(BUILD)/src/interface/statusd_main.o $(COMMON_OBJ)

$(DAEMON_BIN):
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN)/test_%: tests/test_%.c $(COMMON_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDLIBS)

# `all` first: every daemon must compile under the same flags as the tests, so `make asan`
# builds the daemons with the sanitizers too, not only the unit tests.
test: all $(TEST_BIN) boundary boardcheck toolcheck
	@fail=0; for t in $(TEST_BIN); do \
		printf '%-28s ' "$$(basename $$t)"; \
		if $$t; then echo PASS; else echo FAIL; fail=1; fi; done; \
	exit $$fail

# CLAUDE.md §2.1 — a grader must be able to confirm the boundary in sixty seconds (D-038).
# Network code only in src/interface/; Unix sockets only through src/ipc/uds.c. The
# check tests itself first: every case under tests/boundary/cases/bad_* must fail.
boundary:
	@tests/boundary/check.sh --selftest tests/boundary/cases
	@tests/boundary/check.sh . && echo "boundary                     PASS"

# docs/milestones.md is the board (D-020) - every task, question and decision
# in the project is a checkbox there. Validate it in the same gate as the C.
boardcheck:
	@$(PY) tools/test_board.py

# Python tool tests (stdlib unittest, tests/tools/). The venv is made on first use, and
# refreshed when a requirements file changes, so a fresh clone stays green; it needs the
# network for that install. Dev only (D-032).
.venv/.installed: tools/requirements.txt tools/requirements-analysis.txt
	@test -x .venv/bin/python || $(PY) -m venv .venv
	@.venv/bin/pip install -q -r tools/requirements-analysis.txt
	@touch $@

toolcheck: .venv/.installed
	@PYTHONPATH=tools .venv/bin/python -m unittest discover -s tests/tools -q && \
		echo "toolcheck                    PASS"

# Sanitized objects live in their own tree. Objects do not depend on the flags that built
# them, so sharing build/ let a plain `make` link against ASan objects and fail, and let
# `make test` rerun sanitized binaries without saying so.
# -fno-sanitize-recover makes a UBSan finding abort the test, so it cannot print PASS.
# LeakSanitizer is on by default on Linux (the Pi, CI); macOS arm64 does not have it.
asan:
	$(MAKE) BUILD=$(BUILD)/asan test \
	  OPT="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"

memcheck: $(TEST_BIN)
	@command -v valgrind >/dev/null 2>&1 || { \
		echo "valgrind not found: run make memcheck on the Pi or Linux (macOS has none)"; exit 1; }
	@for t in $(TEST_BIN); do valgrind --error-exitcode=1 --leak-check=full $$t || exit 1; done

soakcheck: all
	./soak/run_soak.sh --mini

replay: all
	@test -n "$(FIX)" || { echo "usage: make replay FIX=fixtures/<name>"; exit 1; }
	./tools/replay.py --fixture $(FIX) --label replay

# The phone page and API, on synthetic data, until it is wired to the real source (D-045).
# On the Pi, bind the access point's address: make run-status STATUS_ARGS="--bind 192.168.4.1"
STATUS_ARGS ?= --bind 127.0.0.1 --port 8080
run-status: $(BIN)/statusd
	$(BIN)/statusd $(STATUS_ARGS)

deploy: all
	@test -n "$(PI)" || { echo "usage: make deploy PI=pi@<host>"; exit 1; }
	@# Source only: no build trees, no Mac venv, no raw drive data or soak logs, no transcripts.
	rsync -a --exclude /build --exclude /.git --exclude /.venv --exclude /data \
	      --exclude /soak/logs --exclude /partners ./ $(PI):~/carwatch/
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
