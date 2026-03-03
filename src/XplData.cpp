#include "XplData.h"
#include <plog/Log.h>
#include "XPLMPlugin.h"
#include <chrono>
#include "global.h"
#include "json.hpp"


static uint64_t nowMillis() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// ---------------------------------------------------------------------------
// initDataRef — try to find one dataref by name and determine its type
// ---------------------------------------------------------------------------
bool XplData::initDataRef(XplDataRefMeta& meta, const std::string& name)
{
	if (meta.dataRef != NULL) {
		return true;  // already initialised
	}
	meta.dataRef = XPLMFindDataRef(name.c_str());
	if (meta.dataRef == NULL) {
		PLOGE << "Dataref not found: " << name;
		return false;
	}
	meta.type = XPLMGetDataRefTypes(meta.dataRef);
	meta.value.fValue = 0.0f;

	if (meta.type == xplmType_Float) {
		PLOGD << "DR float:      " << name;
	}
	else if (meta.type == xplmType_Int) {
		meta.value.iValue = 0;
		PLOGD << "DR int:        " << name;
	}
	else if (meta.type == xplmType_Data) {
		meta.value.cArrayValue[0] = '\0';
		PLOGD << "DR data(str):  " << name;
	}
	else if (meta.type == xplmType_IntArray) {
		meta.valuesCount = XPLMGetDatavi(meta.dataRef, NULL, 0, 0);
		for (int i = 0; i < meta.valuesCount; i++) meta.value.iArrayValue[i] = 0;
		PLOGD << "DR int[" << meta.valuesCount << "]: " << name;
	}
	else if (meta.type == xplmType_FloatArray) {
		meta.valuesCount = XPLMGetDatavf(meta.dataRef, NULL, 0, 0);
		for (int i = 0; i < meta.valuesCount; i++) meta.value.fArrayValue[i] = 0.0f;
		PLOGD << "DR float[" << meta.valuesCount << "]: " << name;
	}
	else {
		PLOGE << "DR unknown type " << meta.type << ": " << name;
	}
	return true;
}

// ---------------------------------------------------------------------------
// initDataRefs — build/refresh the master registry from all stream configs
// ---------------------------------------------------------------------------
int XplData::initDataRefs()
{
	int count = 0;
	const auto& streamConfigs = glb()->getConfig()->getStreams();

	// Collect the union of all unique dataref names across all streams
	for (const auto& sc : streamConfigs) {
		for (const auto& def : sc.dataRefs) {
			if (masterRegistry.find(def.name) == masterRegistry.end()) {
				masterRegistry[def.name] = XplDataRefMeta{};  // insert empty slot
			}
		}
	}

	// Try to initialise any slot that doesn't have a handle yet
	for (auto& [name, meta] : masterRegistry) {
		if (meta.dataRef == NULL) {
			if (initDataRef(meta, name)) {
				count++;
			}
		}
		else {
			count++;  // already initialised
		}
	}
	dataRefInitializedCount = count;
	PLOGD << "Master registry: " << count << "/" << masterRegistry.size() << " datarefs initialised";
	return count;
}

// ---------------------------------------------------------------------------
// readDataRefValue — read current X-Plane value into cached meta
// ---------------------------------------------------------------------------
void XplData::readDataRefValue(XplDataRefMeta& meta)
{
	if (meta.dataRef == NULL) return;

	if (meta.type == xplmType_Float) {
		meta.value.fValue = XPLMGetDataf(meta.dataRef);
	}
	else if (meta.type == xplmType_Int) {
		meta.value.iValue = XPLMGetDatai(meta.dataRef);
	}
	else if (meta.type == xplmType_FloatArray) {
		XPLMGetDatavf(meta.dataRef, meta.value.fArrayValue, 0, meta.valuesCount);
	}
	else if (meta.type == xplmType_IntArray) {
		XPLMGetDatavi(meta.dataRef, meta.value.iArrayValue, 0, meta.valuesCount);
	}
	else if (meta.type == xplmType_Data) {
		XPLMGetDatab(meta.dataRef, meta.value.cArrayValue, 0, sizeof(meta.value.cArrayValue));
		meta.value.cArrayValue[sizeof(meta.value.cArrayValue) - 1] = '\0';
	}
}

// ---------------------------------------------------------------------------
// buildJson — assemble JSON for a single stream using master registry values
// ---------------------------------------------------------------------------
std::string XplData::buildJson(const Stream& stream) const
{
	nlohmann::json j;
	for (const auto& def : stream.config.dataRefs) {
		auto it = masterRegistry.find(def.name);
		if (it == masterRegistry.end() || it->second.dataRef == NULL) {
			continue;  // not available yet
		}
		const auto& meta = it->second;
		const std::string& key = def.alias;
		const double mul = def.multiplier;  // 1.0 = no change

		if (meta.type == xplmType_Float) {
			j[key] = meta.value.fValue * static_cast<float>(mul);
		}
		else if (meta.type == xplmType_Int) {
			// keep as int if multiplier is exactly 1, otherwise promote to double
			if (mul == 1.0)
				j[key] = meta.value.iValue;
			else
				j[key] = meta.value.iValue * mul;
		}
		else if (meta.type == xplmType_FloatArray) {
			for (int i = 0; i < meta.valuesCount; i++)
				j[key][i] = meta.value.fArrayValue[i] * static_cast<float>(mul);
		}
		else if (meta.type == xplmType_IntArray) {
			for (int i = 0; i < meta.valuesCount; i++) {
				if (mul == 1.0)
					j[key][i] = meta.value.iArrayValue[i];
				else
					j[key][i] = meta.value.iArrayValue[i] * mul;
			}
		}
		else if (meta.type == xplmType_Data) {
			j[key] = std::string(meta.value.cArrayValue);  // strings: multiplier ignored
		}
	}
	return j.dump();
}

// ---------------------------------------------------------------------------
// valuesAsJson — dump the entire master registry for the web API (/xpl endpoint)
//   Uses the dataref name as key (not alias) for a complete diagnostic view.
// ---------------------------------------------------------------------------
std::string XplData::valuesAsJson() const
{
	nlohmann::json j;
	for (const auto& [name, meta] : masterRegistry) {
		if (meta.dataRef == NULL) continue;
		if (meta.type == xplmType_Float) {
			j[name] = meta.value.fValue;
		}
		else if (meta.type == xplmType_Int) {
			j[name] = meta.value.iValue;
		}
		else if (meta.type == xplmType_FloatArray) {
			for (int i = 0; i < meta.valuesCount; i++)
				j[name][i] = meta.value.fArrayValue[i];
		}
		else if (meta.type == xplmType_IntArray) {
			for (int i = 0; i < meta.valuesCount; i++)
				j[name][i] = meta.value.iArrayValue[i];
		}
		else if (meta.type == xplmType_Data) {
			j[name] = std::string(meta.value.cArrayValue);
		}
	}
	return j.dump();
}

// ---------------------------------------------------------------------------
// init — called once at plugin start
// ---------------------------------------------------------------------------
void XplData::init()
{
	// Stop any previously running streams (e.g. on reload)
	stopAllStreams();

	// Build stream runtime objects from config
	const auto& streamConfigs = glb()->getConfig()->getStreams();

	for (const auto& sc : streamConfigs) {
		if (!sc.enabled) {
			PLOGD << "Stream disabled, skipping";
			continue;
		}

		Stream s;
		s.config = sc;

		if (sc.udp.has_value()) {
			s.sender = std::make_unique<UdpDataPushSerice>(sc.udp.value());
			PLOGD << "Created UDP stream -> "
			      << sc.udp->host << ":" << sc.udp->port
			      << "  interval=" << sc.intervalMs << "ms";
		}
		else if (sc.serial.has_value()) {
			s.sender = std::make_unique<SerialDataPushService>(sc.serial.value());
			PLOGD << "Created Serial stream -> "
			      << sc.serial->port << " @ " << sc.serial->baudRate << " baud"
			      << "  interval=" << sc.intervalMs << "ms";
		}
		else {
			PLOGE << "Stream has no supported output method (udp/serial), skipping";
			continue;
		}

		// Start the sender's worker thread
		IDataSender* senderPtr = s.sender.get();
		s.senderThread = std::thread([senderPtr]() { senderPtr->run(); });
		streams.push_back(std::move(s));
	}

	initDataRefs();
}

// ---------------------------------------------------------------------------
// update — called every ~100ms from the X-Plane flight loop callback
// ---------------------------------------------------------------------------
void XplData::update()
{
	uint64_t currentMillis = nowMillis();

	// Re-try failed dataref lookups periodically
	if (dataRefInitializedCount < (int)masterRegistry.size() &&
	    (lastDataRefInitAttemptMillis == 0 ||
	     currentMillis - lastDataRefInitAttemptMillis > 10000))
	{
		initDataRefs();
		lastDataRefInitAttemptMillis = currentMillis;
	}

	// Read all datarefs in the master registry
	for (auto& [name, meta] : masterRegistry) {
		readDataRefValue(meta);
	}

	// Check each stream — send if its interval has elapsed
	for (auto& stream : streams) {
		if (stream.sender == nullptr) continue;
		if (currentMillis - stream.lastSentMillis >= (uint64_t)stream.config.intervalMs) {
			std::string json = buildJson(stream);
			stream.sender->sendData(json);
			stream.lastSentMillis = currentMillis;
		}
	}
}

// ---------------------------------------------------------------------------
// stopAllStreams — called at plugin stop
// ---------------------------------------------------------------------------
void XplData::stopAllStreams()
{
	for (auto& stream : streams) {
		if (stream.sender) {
			stream.sender->terminate();
		}
	}
	for (auto& stream : streams) {
		if (stream.senderThread.joinable()) {
			stream.senderThread.join();
		}
	}
	streams.clear();
}

// ---------------------------------------------------------------------------
// onPluginReceiveMessage
// ---------------------------------------------------------------------------
void XplData::onPluginReceiveMessage(XPLMPluginID inFromWho, int inMessage, void* inParam)
{
	int planeIndex = (int)(intptr_t)inParam;
	switch (inMessage)
	{
	case XPLM_MSG_PLANE_LOADED:
		PLOGD << "XPLM_MSG_PLANE_LOADED index=" << planeIndex;
		if (planeIndex == 0) {
			initDataRefs();
			inFlight = true;
		}
		break;
	case XPLM_MSG_PLANE_UNLOADED:
		PLOGD << "XPLM_MSG_PLANE_UNLOADED index=" << planeIndex;
		if (planeIndex == 0) {
			inFlight = false;
		}
		break;
	default:
		break;
	}
}