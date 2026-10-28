# First drive: survey and log the Forester

**Feeds:** `pidforester`, `firstcap`, `baseline` (M2). **Tools:** `tools/pidscan.py`,
`tools/obdlog.py` (D-033). **Adapter:** OBDLink LX (D-019).

The adapter takes its power from the car's OBD2 port (pin 16), so none of this works at a
desk. Everything below has been rehearsed against `obdsim`; the first real run is the
only unknown, so expect the port name and the timing to need a look.

## Before leaving the house

1. Set up the tools once (needs the network):

   ```sh
   make toolcheck          # builds .venv (pyserial, matplotlib) and runs the tool tests
   ```

2. Rehearse the whole path against the simulator:

   ```sh
   PYTHONPATH=tools .venv/bin/python tools/obdlog.py --sim --max-seconds 20
   ```

   It should report about 20 samples per second and write
   `data/drive-<time>-synth.jsonl`.
3. Charge the laptop. Optional: `pip install -r tools/requirements-gps.txt` into the
   venv for `--gps`, then run the rehearsal once with `--gps` so macOS asks for the
   location permission at the desk, not in the car.
4. Turn off sleep for the drive: run the logger under `caffeinate -i` (below), and keep
   the lid open. `caffeinate -i` does not stop the sleep a closed lid causes on battery.

## In the car, parked (about 10 minutes)

1. Plug the LX into the OBD2 port, under the dash on the driver's side. Start the engine.
2. Pair it: System Settings, Bluetooth. If it does not show up, check the quick-start
   card in the box for how the LX enters pairing mode.
3. Find its serial port:

   ```sh
   ls /dev/cu.*
   ```

   Record the name here. If no new `/dev/cu.*` appears after pairing, the Mac route has
   failed: use the Pi (last section).
4. Survey, engine idling:

   ```sh
   PYTHONPATH=tools .venv/bin/python tools/pidscan.py --port /dev/cu.<name>
   ```

   It prints the protocol, the battery voltage, the VIN (on screen only) and one reading
   per supported PID, and writes `data/pidscan-<time>-live.{json,md}`. If it hangs at
   the start, the adapter is still searching for the protocol; give it 15 s.

## The drive

```sh
caffeinate -i env PYTHONPATH=tools .venv/bin/python tools/obdlog.py \
    --port /dev/cu.<name>            # add --gps if it was set up
```

- Start it parked, engine running. It needs no input; a status line prints every 5 s.
- A passenger watches the status line, or nobody does. Never the driver.
- Drive a mix: a cold start if possible, idle, town, a highway stretch, a stop.
- Ctrl-C ends the session cleanly. A crash loses at most a second; a closed lid stops
  logging until it is opened again.
- A Bluetooth drop shows as `link_lost` and the logger reconnects by itself.

## Afterward

1. Check the capture (rates, gaps and events per PID):

   ```sh
   PYTHONPATH=tools .venv/bin/python tools/obdsum.py data/drive-*.jsonl
   ```

   A `multi_ecu` event means more than one controller answered that PID; its samples
   keep every line in `raw_all` (D-044). Note which PIDs it names: that list decides how
   the analysis picks one controller's reading.

2. Look at it: `tools/obdplot.py` (PNG figures in `data/plots/`), `tools/obdview.py`
   (an offline HTML page per drive) and `tools/obdbins.py` (how well each rpm and load
   cell is filled), each run the same way as `obdsum.py`.
3. Paste the survey's markdown block, from `data/pidscan-*-live.md`, into
   `docs/hardware/pid-survey.md` under the Forester column.
4. A capture made with `scripts/capture.sh` is committed whole by the script, under
   `captures/<car>/` (D-046). For a drive logged with `obdlog` directly, the full log
   stays out of git (`data/` is ignored): commit a short excerpt as
   evidence in its own commit, with the `obdsum` output. If the drive used `--gps`, leave
   the `gps` lines out of the excerpt: they show where the car is parked at home.
5. Tick `pidforester`, `firstcap` and `baseline` yourself when they are true, then run
   `python3 tools/board_sync.py`.

## If the Mac cannot see the adapter: use the Pi

On the Pi, with the car running and the LX in the port:

```sh
bluetoothctl            # then: power on, scan on, pair <MAC>, trust <MAC>, quit
sudo rfcomm bind 0 <MAC> 1
ls -l /dev/rfcomm0
```

Then run the same two commands with `--port /dev/rfcomm0`, without `caffeinate`, which is
macOS only. If opening the port is refused, add the user to the `dialout` group (`sudo
usermod -aG dialout $USER`, then log in again). Record the MAC address in
`docs/hardware/wiring.md`.

## Results

| Date | Port name | Survey file | Drive file | Samples/s | Notes |
| --- | --- | --- | --- | --- | --- |
| | | | | | |
