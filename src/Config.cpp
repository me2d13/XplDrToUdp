#include "Config.h"

#include <iostream>
#include <fstream>
#include <string>
#include "json.hpp"
#include <plog/Log.h>

// ---------------------------------------------------------------------------
// Helper: parse a JSON array of dataRef entries into vector<DataRefDef>
// Each element can be either:
//   - a string:  "laminar/B738/axis/throttle1"
//   - an object: { "name": "laminar/B738/axis/throttle1", "alias": "thr1" }
// ---------------------------------------------------------------------------
static std::vector<DataRefDef> parseDataRefs(const nlohmann::json& arr)
{
	std::vector<DataRefDef> result;
	for (const auto& item : arr) {
		DataRefDef def;
		if (item.is_string()) {
			def.name  = item.get<std::string>();
			def.alias = def.name;          // no alias → use full name
		}
		else if (item.is_object()) {
			def.name  = item.value("name", std::string{});
			def.alias = item.value("alias", def.name);  // default alias = name
		}
		else {
			PLOGE << "Unexpected dataRef entry type, skipping";
			continue;
		}
		if (def.name.empty()) {
			PLOGE << "DataRef entry with empty name, skipping";
			continue;
		}
		result.push_back(def);
	}
	return result;
}

// ---------------------------------------------------------------------------
// Parse the new "streams" array format
// ---------------------------------------------------------------------------
static std::vector<StreamConfig> parseStreams(const nlohmann::json& streamsArr)
{
	std::vector<StreamConfig> result;
	for (const auto& s : streamsArr) {
		StreamConfig sc;
		sc.enabled    = s.value("enabled",  true);
		sc.intervalMs = s.value("interval", 1000);

		if (s.contains("udp")) {
			UdpConfig udp;
			udp.host = s["udp"].value("host", std::string("127.0.0.1"));
			udp.port = s["udp"].value("port", 5000);
			sc.udp = udp;
		}
		if (s.contains("serial")) {
			SerialConfig serial;
			serial.port     = s["serial"].value("port", std::string("COM1"));
			serial.baudRate = s["serial"].value("baudRate", 115200);
			sc.serial = serial;
		}
		if (!sc.udp.has_value() && !sc.serial.has_value()) {
			PLOGE << "Stream has no output method (udp/serial), skipping";
			continue;
		}
		if (s.contains("dataRefs")) {
			sc.dataRefs = parseDataRefs(s["dataRefs"]);
		}
		result.push_back(sc);
	}
	return result;
}

// ---------------------------------------------------------------------------
// Parse the legacy flat format into a single StreamConfig
// ---------------------------------------------------------------------------
static StreamConfig parseLegacy(const nlohmann::json& j)
{
	StreamConfig sc;
	sc.enabled    = true;
	sc.intervalMs = j.value("updateInterval", 1000);

	UdpConfig udp;
	udp.host = j.value("receiverHost", std::string("127.0.0.1"));
	udp.port = j.value("receiverPort", 5000);
	sc.udp   = udp;

	// Legacy dataRefs is an array of plain strings
	auto names = j.value("dataRefs", std::vector<std::string>());
	for (const auto& name : names) {
		DataRefDef def;
		def.name  = name;
		def.alias = name;   // no alias in legacy format
		sc.dataRefs.push_back(def);
	}

	PLOGD << "Legacy config parsed as single UDP stream: "
	      << udp.host << ":" << udp.port
	      << " interval=" << sc.intervalMs << "ms"
	      << " datarefs=" << sc.dataRefs.size();
	return sc;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------
bool Config::readConfigFile(std::string pConfigPath)
{
	std::string fullPath = pConfigPath + "\\config.json";
	std::ifstream fileStream(fullPath);
	if (!fileStream.is_open()) {
		PLOGE << fullPath << " not found";
		return false;
	}
	PLOGD << "Reading config from " << fullPath;

	nlohmann::json j;
	try {
		fileStream >> j;


		// parse web config
		if (j.contains("web")) {
			webConfig.enabled = j["web"].value("enabled", true);
			webConfig.port    = j["web"].value("port", 8080);
		} else {
			// legacy top-level serverPort
			webConfig.enabled = true;
			webConfig.port    = j.value("serverPort", 8080);
		}
		PLOGD << "Web server: enabled=" << webConfig.enabled << " port=" << webConfig.port;

		if (j.contains("streams")) {
			PLOGD << "Found 'streams' array — using new config format";
			streams = parseStreams(j["streams"]);
			PLOGD << "Parsed " << streams.size() << " stream(s)";
		}
		else {
			PLOGD << "No 'streams' key — using legacy config format";
			streams.clear();
			streams.push_back(parseLegacy(j));
		}
	}
	catch (nlohmann::json::exception& e) {
		PLOGE << "Error reading config.json: " << e.what();
		return false;
	}

	fileStream.close();

	// Log summary
	for (size_t i = 0; i < streams.size(); i++) {
		const auto& s = streams[i];
		PLOGD << "Stream[" << i << "] enabled=" << s.enabled
		      << " interval=" << s.intervalMs << "ms"
		      << " datarefs=" << s.dataRefs.size();
		for (const auto& dr : s.dataRefs) {
			PLOGD << "  DR: " << dr.name << " -> " << dr.alias;
		}
	}
	return true;
}
