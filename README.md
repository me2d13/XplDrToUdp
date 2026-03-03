# XplDrToUdp

X-Plane 12 plugin that reads dataref values and pushes them over the network
(UDP) or serial port to external hardware or software — on a configurable
schedule.

Originally created to feed a motorized throttle quadrant over WiFi.
Updated March 2026 with support for **multiple independent output streams**
and **serial port output**.

---

## Why this plugin?

X-Plane has a built-in UDP output, but it has limitations:

- You can only send pre-defined data groups or a small set of custom datarefs
- You can't set the update interval per custom dataref independently
- Datarefs from third-party aircraft plugins (e.g. Zibo B737) can't be sent

This plugin solves all of the above — any dataref, any interval, any number of
output destinations.

---

## Output format

Each stream sends a compact JSON object. For example:

```json
{"laminar/B738/flt_ctrls/speedbrake_lever":0.0,"sim/flightmodel/engine/ENGN_thro":[0.82,0.82]}
```

When aliases are defined the keys are the short alias names instead:

```json
{"spd":245.3,"alt":10850.0,"roll":1.2,"pitch":-2.4,"hdg":275.6,"vs":142.0}
```

Serial output: same JSON, one message per line (`\n` terminated).

---

## Configuration

Config file is named `config.json` and must be placed in the same folder as
the plugin (`win.xpl`).

### Streams

The top-level `streams` array defines one or more independent output channels.
Each stream has its own destination, update rate and list of datarefs.

```json
{
  "streams": [
    {
      "enabled": true,
      "interval": 500,
      "udp": { "host": "192.168.1.20", "port": 49152 },
      "dataRefs": [
        { "name": "sim/flightmodel/engine/ENGN_thro" },
        { "name": "laminar/B738/axis/throttle1", "alias": "thr1" }
      ]
    },
    {
      "enabled": true,
      "interval": 100,
      "serial": { "port": "COM3", "baudRate": 115200 },
      "dataRefs": [
        { "name": "sim/flightmodel/position/phi",   "alias": "roll",  "multiplier": -1 },
        { "name": "sim/flightmodel/position/theta", "alias": "pitch", "multiplier": -1 }
      ]
    }
  ]
}
```

#### Stream fields

| Field | Required | Description |
|---|---|---|
| `enabled` | no | `true`/`false` — skipped entirely when false. Default: `true` |
| `interval` | yes | How often to send, in milliseconds |
| `udp` | one of udp/serial | UDP destination: `host` (IP address) and `port` |
| `serial` | one of udp/serial | Serial port: `port` (e.g. `"COM3"`) and `baudRate` |
| `dataRefs` | yes | List of datarefs to include in this stream's message |

#### DataRef fields

| Field | Required | Description |
|---|---|---|
| `name` | yes | Full X-Plane dataref path |
| `alias` | no | Short key used in output JSON. Defaults to full `name` |
| `multiplier` | no | Scale factor applied to the value before output. Default: `1.0` (no change). Useful for unit conversion or sign inversion |

### Web server (optional)

The plugin includes a small built-in web server for monitoring.

```json
{
  "web": { "enabled": true, "port": 8080 }
}
```

If `web` is omitted, defaults to enabled on port 8080.

Pages available while X-Plane is running:
- `http://localhost:8080/` — status page: active streams, datarefs and live values
- `http://localhost:8080/log` — plugin log (newest first)
- `http://localhost:8080/api/xpl` — all current values as raw JSON

### Legacy format (still supported)

Older single-stream configs continue to work without modification:

```json
{
  "receiverHost": "192.168.1.121",
  "receiverPort": 49152,
  "updateInterval": 3000,
  "dataRefs": [
    "sim/aircraft/view/acf_descrip",
    "laminar/B738/autopilot/lock_throttle"
  ]
}
```

---

## Real-world example

Two streams running simultaneously — one to a throttle quadrant over WiFi,
one to a helicopter cyclic controller over USB serial:

```json
{
  "streams": [
    {
      "enabled": true,
      "interval": 500,
      "udp": { "host": "192.168.1.20", "port": 49152 },
      "dataRefs": [
        { "name": "sim/aircraft/view/acf_descrip" },
        { "name": "laminar/B738/parking_brake_pos" },
        { "name": "laminar/B738/flight_model/stab_trim_units" },
        { "name": "sim/flightmodel/engine/ENGN_thro" },
        { "name": "laminar/B738/autopilot/autothrottle_status1" },
        { "name": "laminar/B738/flt_ctrls/speedbrake_lever" }
      ]
    },
    {
      "enabled": true,
      "interval": 100,
      "serial": { "port": "COM12", "baudRate": 115200 },
      "dataRefs": [
        { "name": "sim/cockpit2/gauges/indicators/airspeed_kts_pilot", "alias": "spd" },
        { "name": "sim/cockpit2/gauges/indicators/altitude_ft_pilot",  "alias": "alt" },
        { "name": "sim/flightmodel/position/phi",   "alias": "roll",  "multiplier": -1 },
        { "name": "sim/flightmodel/position/theta", "alias": "pitch", "multiplier": -1 },
        { "name": "sim/flightmodel/position/psi",   "alias": "hdg" },
        { "name": "sim/flightmodel/position/vh_ind_fpm", "alias": "vs" }
      ]
    }
  ]
}
```

**Stream 1 — Motorized throttle quadrant** ([mtu-pio](https://github.com/me2d13/mtu-pio))  
Sends Boeing 737 throttle-related values over UDP at 500 ms to an ESP32-based
motorized throttle quadrant connected via Wi-Fi. The quadrant uses engine
throttle position, autothrottle status, trim and speedbrake lever to keep its
physical levers in sync with the sim.

**Stream 2 — Helicopter cyclic with autopilot** ([esp32-heli-joystick](https://github.com/me2d13/esp32-heli-joystick))  
Sends attitude and flight data at 100 ms (10 Hz) over USB serial to an ESP32
controller that implements a force-feedback helicopter cyclic with autopilot
assist. Roll and pitch are multiplied by `-1` to match the sign convention used
by the controller firmware (same as MSFS SimConnect variables).

---

## Installation

1. Copy `win.xpl` into a subfolder of `X-Plane 12/Resources/plugins/`
2. Create `config.json` in the same folder
3. Start X-Plane — the plugin loads automatically

Requires X-Plane 12 (64-bit Windows).
