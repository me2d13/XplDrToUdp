#pragma once

#include <string>
#include <vector>
#include <optional>

// ---------------------------------------------------------------------------
// DataRefDef — one dataref entry: X-Plane path + output alias
// ---------------------------------------------------------------------------
struct DataRefDef {
	std::string name;     // X-Plane dataref path (used with XPLMFindDataRef)
	std::string alias;    // key used in the JSON output (defaults to name)
	double multiplier{ 1.0 }; // optional scale factor applied before output (e.g. -1 to invert)
};

// ---------------------------------------------------------------------------
// Output-method configs
// ---------------------------------------------------------------------------
struct UdpConfig {
	std::string host{ "127.0.0.1" };
	int         port{ 5000 };
};

// Placeholder for future serial support
struct SerialConfig {
	std::string port;
	int         baudRate{ 115200 };
};

// ---------------------------------------------------------------------------
// WebConfig — built-in HTTP status/log server
// ---------------------------------------------------------------------------
struct WebConfig {
	bool enabled{ true };
	int  port{ 8080 };
};

// ---------------------------------------------------------------------------
// StreamConfig — everything that describes one output stream
// ---------------------------------------------------------------------------
struct StreamConfig {
	bool                        enabled{ true };
	int                         intervalMs{ 1000 };
	std::optional<UdpConfig>    udp;
	std::optional<SerialConfig> serial;
	std::vector<DataRefDef>     dataRefs;
};

// ---------------------------------------------------------------------------
// Config — reads config.json, exposes parsed streams
// ---------------------------------------------------------------------------
class Config
{
private:
	std::vector<StreamConfig> streams;
	WebConfig webConfig;

public:
	Config() = default;
	bool readConfigFile(std::string pConfigPath);

	const std::vector<StreamConfig>& getStreams() const { return streams; }
	const WebConfig& getWebConfig() const { return webConfig; }
};