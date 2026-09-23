# Problem memo — carwatch (Camden Thomas, Lance Baron)

## The user

Our user is Lance, who drives his 2015 Honda CR-V EX-L every day and has no way to know
how it is doing between services. His car already has a sensor on nearly everything that
matters, but the only thing that ever reads them for him is the check-engine light, and it
speaks up only once something has already gone wrong. What he wants is simple: something
that knows how *his* car normally behaves and tells him when it starts acting differently,
before a small problem becomes a breakdown.

## The problem

Every modern engine reports what it is doing through its OBD2 port: coolant temperature,
air intake, fuel trims, oxygen sensors, RPM, load and more. Nothing in the car watches those
numbers over time, however, and nothing knows what normal looks like for this particular
engine. As a result, a slowly developing fault, such as a small vacuum leak, a lazy
thermostat or a sensor going bad, can sit inside the "acceptable" range for weeks before it
crosses the one threshold that lights the dashboard. What is missing is a device that
records that data continuously, learns this car's normal, and flags strange behavior early
enough to act on it, before the check-engine light of doom puts its owner on an hour-long
bus ride.

## Why a device

The signal we are after only shows up across many drives, and it is only interpretable with
the cold starts, the short trips and the long pulls, *all* of them, not just the drives
someone remembered to open an app for. A phone app is present only for the trips its owner
thought to record, which is a biased sample of exactly the behavior we are trying to learn.
Moreover, the interesting moment is never the moment anyone is looking: it is the third cold
start of a February week, when warm-up took ninety seconds longer than it did in January,
and nobody is holding a phone then. The device, by contrast, is already in the car, awake
at key-on and recording before anyone has decided anything is wrong. Finally, the data
stays in the car, with no account, no upload and no company holding a log of everywhere
this person drove.

## The sensors

The device reads every Mode 01 sensor the CR-V publishes through its OBD2 port, including
coolant temperature, intake air temperature, MAP, short- and long-term fuel trims, O2, RPM
and engine load. These sensors cooperate rather than merely coexist, because **no one of
them means anything alone.** Fuel trim, for example, is only interpretable against load,
and coolant temperature only points at the cooling system once ambient temperature and load
have been accounted for. The model therefore learns how the sensors normally move
*together* at each operating point, and a fault shows up as a broken partnership that no
single threshold could see. That same pattern also suggests *where* the fault is: cooling,
air/fuel mixture, or a sensor that has stopped making sense.

## The mechanisms

Our first choice is **mechanism D, custom append-only storage with crash consistency**,
because the power is cut mid-write every time the key turns off, so the storage layer never
gets a clean shutdown and must recover a torn tail on every boot. We pair it with
**mechanism E, a multi-process architecture with a supervisor**, because a recorder that
dies silently has harmed its owner, who believes it is still on duty; running capture,
storage and analysis as separate processes means a crash in the analysis cannot take the
recording down with it.

## The risk

The primary risk, named honestly, is learning time: the model knows nothing until it has
seen weeks of the CR-V's normal driving, so baseline collection has to start at M2, and
every week it slips is a week less of normal to compare against. The second risk is the
OBD2 protocol itself: each manufacturer implements it a little differently, the CR-V may
not publish every sensor we would like, and some signals may be impossible to decode
because the manufacturer keeps their meaning proprietary. Finally, the scope is large for
one semester, so we are building an honest first draft of the idea, one car, one Bluetooth
adapter and one Pi, rather than a finished product.
