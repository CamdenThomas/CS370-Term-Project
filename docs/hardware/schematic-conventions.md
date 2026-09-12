# Schematic conventions

**Owner: Lance.** The KiCad project lives in `electricalDrawing/`.
`make hwcheck` must pass before any PR touching it is opened.

## Scope: schematic yes, PCB probably no

**Decision A.4 in `docs/DECISIONS.md`.** We capture the schematic; we do not fabricate a
board unless we take The Reach (handout §14).

A custom PCB earns **zero points** — the rubric grades mechanisms, measurements and the
soak — and adds fab turnaround on top of critical path B, which is already the thing most
likely to delay M2. A breadboard plus an off-the-shelf MCP2515 module clears every
guardrail, and the handout is explicit that the enclosure can be a food container.

The schematic is worth the time anyway, because three graded artifacts depend on it:

- `DESIGN.md` needs an architecture diagram and a defensible wiring story
- `docs/hardware/wiring.md` needs pin assignments that cannot drift from reality
- The **three-isolated-workers test** needs a parts-only buyer who can order from the BOM
  without ever seeing the design

`electricalDrawing/electricalDrawing.kicad_pcb` stays in the repo, empty, on purpose — the
same way `src/capture/` does. Its emptiness is a recorded position, not an oversight.

## Sheet structure

Lance has blocked out functional areas on the root sheet. Keep that shape; it maps onto the
architecture, which makes both documents easier to defend:

| Block | Contains |
|---|---|
| **Power** | OBD2 pin 16 (+12 V, **live with the key off**), fuse, 12 V→5 V buck, bulk and bypass caps, the Pi's 5 V rail |
| **CAN** | OBD2 pin 6 / pin 14, TJA1050 transceiver, MCP2515, crystal + load caps, termination decision, `INT` to GPIO |
| **Pi interface** | 40-pin header, SPI0 (CE0/MISO/MOSI/SCLK), the interrupt GPIO, grounds |
| **Sensors** *(conditional, D-006)* | MPU-6050 on I2C with pull-ups; DS18B20 on 1-Wire with its 4.7 kΩ |

## Rules

1. **Every net that leaves a block is a named label**, not a wire crossing a boundary. The
   netlist is read by humans at the defense.
2. **Annotate the voltage domain on every power net** (`+12V_SW`, `+5V`, `+3V3`). The Pi has
   no 5 V-tolerant inputs and no over-voltage protection; a mislabeled rail is how a Pi dies
   silently in Week 13.
3. **Every IC gets its decoupling drawn**, even when the module already has it. The
   schematic documents what is *electrically true*, not what we happened to buy assembled.
4. **The MCP2515 crystal frequency is a value field, not a comment** — 8 MHz vs 16 MHz
   changes the device-tree overlay, and a wrong value produces a silently dead bus.
5. **No `DNP`/`no-connect` without a reason in the field.** Floating inputs are an ERC
   violation for good reasons.
6. **`make hwcheck` green before the PR.** ERC violations are build failures, exactly like
   `-Werror`.

## What must not be trusted to a datasheet alone

The cheap MCP2515 breakout modules vary: some run the TJA1050 at 5 V and level-shift the
controller side, and some do not — those drive **5 V onto MISO** and will kill a Pi.

**Meter it. With the Pi disconnected. Before anything is plugged in.** This is the exact
case `CLAUDE.md` §8.4 covers: no hardware conclusion from a verbal description or a
datasheet, on a board nobody has measured. Record the measured voltages in
`docs/hardware/wiring.md` with the date.

## Generated, never hand-edited

`make hwdocs` regenerates these from the schematic. If a pin table and the schematic
disagree, the schematic wins and the table was stale:

- `docs/figures/electricalDrawing.svg` — rendered schematic, **committed**, so pull requests
  show a picture instead of an S-expression diff
- `docs/hardware/bom.csv` — parts list feeding `BOM.md`
- `electricalDrawing/electricalDrawing.net` — netlist, the source for `wiring.md` pin tables
