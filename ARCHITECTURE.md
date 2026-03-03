# XplDrToUdp — Architecture

## Overview

X-Plane plugin that reads datarefs from X-Plane and pushes them to one or more
output destinations (UDP, Serial, …) on independent schedules.

---

## Config File

### New format — `streams` array (preferred)

Each element of `streams` describes one independent output channel with its own
output method, update interval, enabled flag, and dataref list.

```json
{
  "serverPort": 8080,
  "streams": [
    {
      "enabled": true,
      "interval": 500,
      "udp": { "host": "192.168.1.121", "port": 49152 },
      "dataRefs": [
        { "name": "laminar/B738/axis/throttle1", "alias": "thr1" },
        { "name": "laminar/B738/axis/throttle2" }
      ]
    },
    {
      "enabled": true,
      "interval": 100,
      "serial": { "port": "COM12", "baudRate": 115200 },
      "dataRefs": [
        { "name": "sim/cockpit2/gauges/indicators/airspeed_kts_pilot", "alias": "spd" },
        { "name": "sim/cockpit2/gauges/indicators/altitude_ft_pilot",  "alias": "alt" },
        { "name": "sim/flightmodel/position/phi",                      "alias": "roll" },
        { "name": "sim/flightmodel/position/theta",                    "alias": "pitch" },
        { "name": "sim/flightmodel/position/psi",                      "alias": "hdg" },
        { "name": "sim/flightmodel/position/vh_ind_fpm",               "alias": "vs" }
      ]
    }
  ]
}
```

Rules:
- `alias` inside a `dataRefs` entry is **optional** — defaults to the full `name`.
- Exactly one of `udp` or `serial` must be present per stream.
- `enabled` defaults to `true` if omitted.
- `interval` is in milliseconds.
- Serial messages are JSON strings terminated with `\n` for easy framing.

### Legacy format — top-level fields (deprecated, still supported)

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

When the config file does **not** contain a `streams` key, the parser
synthesises a single UDP stream from the legacy top-level fields.  
This keeps all existing configs working without modification.

---

## Internal Data Structures

### `DataRefDef` (POD struct)
```cpp
struct DataRefDef {
    std::string name;   // X-Plane dataref path used with XPLMFindDataRef()
    std::string alias;  // key used in the JSON output message
                        // (equals name when not specified in config)
};
```

### `UdpConfig` / `SerialConfig`
```cpp
struct UdpConfig {
    std::string host{ "127.0.0.1" };
    int         port{ 5000 };
};

struct SerialConfig {
    std::string port;          // e.g. "COM12"
    int         baudRate{ 115200 };
};
```

### `StreamConfig`
Holds everything parsed from one element of the `streams` array.

```cpp
struct StreamConfig {
    bool enabled{ true };
    int intervalMs{ 1000 };

    // exactly one of these is set, rest are std::nullopt
    std::optional<UdpConfig>    udp;
    std::optional<SerialConfig> serial;

    std::vector<DataRefDef> dataRefs;
};
```

### `Stream` (runtime object)
```cpp
struct Stream {
    StreamConfig                 config;
    std::unique_ptr<IDataSender> sender;        // UdpDataPushSerice or SerialDataPushService
    std::thread                  senderThread;
    uint64_t                     lastSentMillis{ 0 };
};
```

---

## Class Responsibilities

### `Config`
- Parses `config.json` using nlohmann/json.
- Detects presence of top-level `streams` key:
  - **Present** → parse each element into `vector<StreamConfig>`.
  - **Absent** → build one `StreamConfig` from legacy fields.
- Exposes `const vector<StreamConfig>& getStreams()`.

### `IDataSender` (abstract interface — `include/IDataSender.h`)
```cpp
class IDataSender {
public:
    virtual bool connect()    = 0;
    virtual bool disconnect() = 0;
    virtual void sendData(const std::string& data) = 0;
    virtual void run()        = 0;
    virtual void terminate()  = 0;
    virtual ~IDataSender()    = default;
};
```

### `UdpDataPushSerice`
- Implements `IDataSender`.
- Owns a Winsock UDP socket and a worker thread.
- Constructor takes `UdpConfig` — does not touch `glb()->getConfig()`.
- Worker thread blocks on a condition variable; `sendData()` posts data and notifies.

### `SerialDataPushService`
- Implements `IDataSender`.
- Owns a Win32 `HANDLE` to the COM port and a worker thread.
- Constructor takes `SerialConfig`.
- Uses `CreateFile` / `SetCommState` / `WriteFile` for COM port I/O.
- Appends `\n` to every message for easy framing on the receiver side.
- Same worker-thread pattern as `UdpDataPushSerice`.

### `XplData`
Central data hub. Key responsibilities:

1. **Master DataRef registry** — union of all unique dataref *names* across all
   streams, built at init. Each unique name is registered with
   `XPLMFindDataRef()` exactly once.  
   Stored as `map<string, XplDataRefMeta>`.

2. **Per-frame update** (called from the X-Plane flight loop callback):
   - Read all datarefs in the master registry into their cached values.
   - For each `Stream` in the stream list:
     - If `now - stream.lastSentMillis >= stream.config.intervalMs`:
       - Build a JSON object from the stream's `DataRefDefs`, looking up
         current values by `def.name` in the master map,
         and keying the JSON by `def.alias`.
       - Call `stream.sender->sendData(json)` (non-blocking — posts to worker).
       - Update `stream.lastSentMillis`.

3. **Re-init** — retries failed `XPLMFindDataRef()` calls on a 10 s cadence.

4. **Plane reload** — on `XPLM_MSG_PLANE_LOADED` for index 0, re-runs
   `initDataRefs()`.

5. **Web API** — `valuesAsJson()` dumps the full master registry (keyed by full
   dataref name) for the `/xpl` HTTP endpoint.

### `global` / `glb()`
- Exposes only `getConfig()` and `getXplData()`.
- Streams are owned and managed entirely inside `XplData`.

### `main.cpp`
- Plugin start: calls `XplData::init()` which creates the master DR registry,
  constructs the `Stream` list, and starts worker threads.
- Plugin stop: calls `XplData::stopAllStreams()` which terminates and joins all
  worker threads.

---

## Scheduling

No extra threads are needed for scheduling.  
The X-Plane **flight loop callback** fires every ~100 ms (configured in `main.cpp`).
`XplData::update()` is called there and uses millisecond timestamps (`std::chrono`)
to decide which streams are due to fire.

```
Every ~100ms:
  currentMillis = now()
  read ALL master-registry datarefs from X-Plane   ← cheap, one pass
  for each stream:
    if currentMillis - stream.lastSentMillis >= stream.config.intervalMs:
      json = buildJson(stream.dataRefs, masterValues)
      stream.sender->sendData(json)                 ← non-blocking, worker thread
      stream.lastSentMillis = currentMillis
```

Each `IDataSender` runs its own **worker thread** that blocks on a
condition variable waiting for `sendData()` to post new data.

---

## Migration Path

| Phase | What changes | Status |
|---|---|---|
| **1 — build** | Command-line build (`build.bat`) working | ✅ Done |
| **2 — streams refactor** | `DataRefDef`, `StreamConfig`, `IDataSender`; `Config` dual-parse; `UdpDataPushSerice` takes `UdpConfig`; `XplData` master-registry + stream loop | ✅ Done |
| **3 — serial** | `SerialDataPushService` implementing `IDataSender` via Win32 COM port API | ✅ Done |
| **4 — cleanup** | Remove legacy config parsing once all configs are migrated | ⬜ Later |

---

## File Index

| File | Role |
|---|---|
| `include/Config.h` | `DataRefDef`, `UdpConfig`, `SerialConfig`, `StreamConfig` structs + `Config` class |
| `src/Config.cpp` | Dual-parse: `streams` array or legacy flat fields |
| `include/IDataSender.h` | Abstract sender interface |
| `include/UdpDataPushSerice.h` | UDP sender header — inherits `IDataSender` |
| `src/UdpDataPushSerice.cpp` | UDP sender implementation (Winsock) |
| `include/SerialDataPushService.h` | Serial sender header — inherits `IDataSender` |
| `src/SerialDataPushService.cpp` | Serial sender implementation (Win32 COM port) |
| `include/XplData.h` | Master registry + `Stream` struct + `XplData` class |
| `src/XplData.cpp` | Per-frame update loop, JSON building, stream lifecycle |
| `include/global.h` | `Globals` — holds `Config` and `XplData` |
| `src/global.cpp` | `glb()` singleton |
| `src/main.cpp` | X-Plane plugin entry points, flight loop callback |
| `build.bat` | Command-line build script (MSBuild, Debug\|x64, English output) |
| `doc/config.json` | Example config showing both UDP and Serial streams |
