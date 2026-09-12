# Draft email — soak test on synthesized CAN traffic (D-006)

> Review, edit into your own voice, send from Camden with Lance cc'd. Do not wait on
> the answer to start M1 or M2.

---

**Subject:** CS370 term project — 48-hour soak with a bench CAN source

Professor Pallickara,

Camden Thomas and Lance Baron. Our term project is an in-vehicle diagnostic recorder: a
Pi on the OBD2 connector via an MCP2515, reading the car's own sensors off the CAN bus
and learning a per-vehicle baseline so it can flag developing faults that a fixed
maintenance interval misses.

We have a question about §3.4 that we would rather ask in Week 4 than discover in
Week 14.

The constraint is that a car cannot idle for 48 continuous hours. Our plan was to build
a bench rig — a PC generating realistic CAN traffic over a USB-to-CAN adapter, driving
varied operating conditions — so the device can run its full 48-hour soak against a live
bus while the vehicle sits. §5 says replayed or synthesized data must be labeled as
such everywhere, which we intend to do at the record level, but it also says the 48-hour
soak runs on live sensors, and we read a generated bus as synthesized by that
definition.

Three ways we could go, and we would rather do the one you intend:

1. The soak runs against the bench CAN rig, with every record and every log line tagged
   as synthesized, and the evaluation report stating it plainly.
2. We add two physical sensors to the Pi — an accelerometer on an engine mount and a
   temperature probe — so the soak runs on genuinely live sensors for the full 48 hours
   while CAN is replayed and labeled. This also gives us vibration data order-tracked
   against CAN-derived RPM, which the ECU itself cannot measure.
3. The soak runs in a parked vehicle on the live bus with the ignition in accessory,
   which is fully live but ties up a daily driver for two days.

We are building toward option 2 by default, since it satisfies the requirement as
written. But if option 1 is acceptable it saves us hardware and wiring time we would
rather spend on the analysis engine and the evaluation, and if you would prefer option
3 we would like to know now, while the design is still cheap to change.

Thank you,
Camden Thomas & Lance Baron
