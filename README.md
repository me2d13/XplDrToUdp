# XplDrToUdp

This X-plane 12 plugin can send values of configured datarefs to any network recevier via UDP protocol with configured time interval.
It's inspired by standard Xplane UDP output but overcomes its limitations and adds some fetaures

- can send value of any dataref. Standard Xplane is only sending some groups of data or/and custom dataref. But for custom dataref it can't configure the interval and datarefs from plugins (custom aircraft) can't be sent
- use json as output format

Sample config file (to be placed next to plugin file itself)
```json
{
  "receiverHost": "192.168.1.121",
  "receiverPort": 49152,
  "updateInterval": 3000,
  "dataRefs": [
    "sim/aircraft/view/acf_descrip",
    "laminar/B738/autopilot/lock_throttle",
    "sim/flightmodel2/controls/elevator_trim",
    "laminar/B738/axis/throttle1",
    "laminar/B738/axis/throttle2",
    "laminar/B738/autopilot/speed_mode"
  ]
}
```

Sample response
```json
{
    "laminar/B738/autopilot/lock_throttle": 0.0,
    "laminar/B738/autopilot/speed_mode": 0.0,
    "laminar/B738/axis/throttle1": 0.0,
    "laminar/B738/axis/throttle2": 0.0,
    "sim/aircraft/view/acf_descrip": "Boeing 737-800X",
    "sim/flightmodel2/controls/elevator_trim": -0.38999998569488525
}
```

The plugin produces web log output accessible (by default) at http://localhost:8080.
There's also kind of mini-api showing current values at http://localhost:8080/api/xpl.
Note these link works only when plugin is running.

For now I'm using this plugin to send values for motorized throttle quadrant  controlled by ESP32 (so connected to my wifi).

This code is in prototype phase, nothing nice, just focused on funcionality. It can be polished, extended (e.g. by serial output such as my older mtu-plugin) etc. Maybe in the future, now my focus is on hardware part.
