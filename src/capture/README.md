# src/capture — physical sensors on the Pi

**Empty on purpose.** This directory exists because decision **D-006** is open: if the
48-hour soak must run on live sensors, an MPU-6050 and a DS18B20 land here and the
architecture is already shaped to receive them (they push into the same SPSC ring that
`candaemon` uses, so nothing downstream changes).

Do not delete this directory. Its emptiness is a documented design position, not an
oversight.
