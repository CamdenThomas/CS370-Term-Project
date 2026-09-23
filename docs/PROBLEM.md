# Problem memo — carwatch (Camden Thomas, Lance Baron)

> **STATUS: M1 DRAFT.** One page maximum. Its job is to survive being read by a
> skeptical stranger. Bracketed items are unfilled.

## The user

> Rubric: a person or place, **named or nameable**. "A cautious driver" is a persona and
> will be returned for revision the same way "people who might want to monitor things"
> would be. Name a human and a car.

The typical american drives a substantial amount, releying heavily on thier vihicle. Many people drive previosly own vihivles with a wide viriaty if wear. People everyday are forced to rely on a car they hardly undestand just hoping the average service milestone will keep you car getiing you to work each day

## The problem

> Observable and costly: what goes wrong, how often, what it costs in money or worry.

Maintenance intervals are generalizations. The sticker on the windshield knows nothing
about how this engine has actually been run — a commuter that never reaches operating
temperature and a highway car with the same odometer reading have not aged the same
way. The consequence runs both directions and both are expensive: oil changed far
earlier than it needed to be, or an engine run on oil that lost its pressure margin
weeks ago. While the sensors already present do a great job tracking all the need information,
people just need a device to monitor and store this data to learn the car and predict failures before the check engine light of doom puts you on the hour long bus ride.

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

> The single thing most likely to sink this project. Name it now.

the trends such as oil life needs a real viscosisty interval to develop, and that is calendar time we cannot buy back by working harder in Week 14. While there will be clear structure for a data logger, the ML model likely will not be able to demonstrate any real worl predictions on the model car (2015, Honda, CRV). Likely a fake obd2 port will need to be made to provide constant data for both training the diagnostic ML model and to perform the full 48 hour soak test, as we can not drive a car or run a car for 48 hours
