# Problem memo — carwatch (Camden Thomas, Lance Baron)

## The user

An average car owner who drives everyday to work, is vastly different from someone who
works from home and does the yearly road trip, although the recommended maintenance is
still the same.

The point of two users is the thesis: the same oil-change sticker says 5,000 miles to
both cars, and the cars disagree.

## The problem

> Observable and costly: what goes wrong, how often, what it costs in money or worry.

Maintenance intervals are generalizations. The sticker on the windshield knows nothing
about how this engine has actually been run — a commuter that never reaches operating
temperature and a highway car with the same odometer reading have not aged the same way.
The consequence runs both directions and both are expensive: oil changed far earlier than
it needed to be, or an engine run on oil that lost its pressure margin weeks ago. While
the sensors already present do a great job tracking all the need information, people just
need a device to monitor and store this data to learn the car and predict failures before
the check engine light of doom puts you on the hour long bus ride.

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

The vehicle's own sensors, read via the OBD2 port: oil pressure, coolant temperature,
intake air temperature, MAP, O2 / fuel trims, RPM, engine load.

They cooperate because **no one of them means anything alone.** Oil pressure is a
function of RPM and oil temperature; the diagnostic signal is the *residual* after
normalizing for both, which requires all three sensors to reach one conclusion. Fuel
trim is only interpretable against load. Coolant temperature only indicts a thermostat
once ambient temperature and load have been accounted for. Every verdict this device
produces requires at least two sensors to agree on something a single threshold cannot
see.

## The mechanisms

> First guess at two menu items, one sentence of justification each. May change by M2.

**D — custom append-only storage with crash consistency.** The power is cut mid-write
every single time the key turns off, so the storage layer never gets a clean shutdown
and must recover a torn tail on every boot.

**E — multi-process architecture with a supervisor.** A recorder that dies silently has
harmed its owner, who believes it is on duty; capture, storage and analysis run as
separate processes so that a crash in the analysis cannot take the recording down with it.

## The risk

The primary risk, named honestly: the oil-life trend needs a real oil interval to develop,
and that is calendar time we cannot buy back by working harder in Week 14. Baseline
collection starts at M2 or it does not happen.

Secondly there lies another risk of how the OBD protocol works, were each car implements
it differently, there being 4 different *data frames*, and even if we get past those
worries, decoding the recieved signal may not be possible due it being proprietary to the
manufacturer. The scale of the project is quite large, and we risk not completeing the
project within one semester.
