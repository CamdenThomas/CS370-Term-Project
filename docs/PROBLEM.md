# Problem memo — carwatch (Camden Thomas, Lance Baron)

> **STATUS: M1 DRAFT.** One page maximum. Its job is to survive being read by a
> skeptical stranger. Bracketed items are unfilled.

## The user
> Rubric: a person or place, **named or nameable**. "A cautious driver" is a persona and
> will be returned for revision the same way "people who might want to monitor things"
> would be. Name a human and a car.

[NAME], who drives a [YEAR MAKE MODEL] and [does X with it — commutes N miles, tows,
short-trips it in winter]. And [NAME 2], whose [YEAR MAKE MODEL] does something
measurably different with its miles.

The point of two users is the thesis: the same oil-change sticker says 5,000 miles to
both cars, and the cars disagree.

## The problem
> Observable and costly: what goes wrong, how often, what it costs in money or worry.

Maintenance intervals are generalizations. The sticker on the windshield knows nothing
about how this engine has actually been run — a commuter that never reaches operating
temperature and a highway car with the same odometer reading have not aged the same
way. The consequence runs both directions and both are expensive: oil changed far
earlier than it needed to be, or an engine run on oil that lost its pressure margin
weeks ago. [Fill in a real number: cost of an oil change × frequency, or the cost of
the failure you are trying to catch.]

Meanwhile the car already measures everything needed to know the difference, and
publishes it on a bus, continuously — and then throws it away. The check-engine light
is the only thing that ever reads it, and it is a **threshold on a single value that
fires after the damage.**

## Why a device
> The 3 a.m. test: why must something be physically present and always awake? And why
> doesn't a phone app already solve this?

The signal is a slow trend across weeks of driving, and it is only interpretable if you
have the cold starts, the short trips, and the long pulls — *all* of them, not the
drives someone remembered to open an app for. A phone app is present for the trips you
thought to instrument, which is a biased sample of exactly the variable being measured.

The interesting moment is also never the moment anyone is looking: it is the third cold
start of a February week where warm-up took ninety seconds longer than it did in
January. Nobody is holding a phone then. The device is in the car, awake at key-on,
recording before anyone has decided anything is wrong.

And the data stays in the car. No account, no upload, no fleet telematics company
holding a log of everywhere this person drove.

## The sensors
> Which two (or more) and how they **cooperate** rather than coexist.

The vehicle's own sensors, read as raw CAN frames: oil pressure, coolant temperature,
intake air temperature, MAP, O2 / fuel trims, RPM, engine load.

They cooperate because **no one of them means anything alone.** Oil pressure is a
function of RPM and oil temperature; the diagnostic signal is the *residual* after
normalizing for both, which requires all three sensors to reach one conclusion. Fuel
trim is only interpretable against load. Coolant temperature only indicts a thermostat
once ambient temperature and load have been accounted for. Every verdict this device
produces requires at least two sensors to agree on something a single threshold cannot
see.

[If D-006 resolves to adding physical sensors: an MPU-6050 on the engine mount and a
DS18B20 temperature probe, with the vibration spectrum order-tracked against CAN-derived
crank speed — a measurement the ECU itself cannot make.]

## The mechanisms
> First guess at two menu items, one sentence of justification each. May change by M2.

**B — interrupt-driven input with a polling comparison.** At 500 kbit/s the MCP2515's
two receive buffers overflow in milliseconds, so a poll loop either burns a core or
loses frames; the `INT` line lets us service the controller only when it has something.

**D — custom append-only storage with crash consistency.** The power is cut mid-write
every single time the key turns off, so the storage layer never gets a clean shutdown
and must recover a torn tail on every boot.

## The risk
> The single thing most likely to sink this project. Name it now.

**That neither testbed publishes analog oil pressure on the bus.** Many consumer
vehicles expose only a binary low-pressure switch, which would delete our headline
diagnostic and leave us with two. The mitigation is a $0 supported-PID scan on both
cars in Week 4 — before the design document, not after (**D-007**).

Second risk, named honestly: the oil-life trend needs a real oil interval to develop,
and that is calendar time we cannot buy back by working harder in Week 14. Baseline
collection starts at M2 or it does not happen.

Thirdly there lies another risk of how the OBD protocol works, were each car implements it differently, there being 4 different *data frames*, and even if we get past those worries, decoding the recieved signal may not be possible due it being proprietary to the manufacturer. The scale of the project is quite large, and we risk not completeing the project within one semester. 