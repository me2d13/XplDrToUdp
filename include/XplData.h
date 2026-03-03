#pragma once

#include "XPLMDataAccess.h"
#include "Config.h"
#include "IDataSender.h"
#include "UdpDataPushSerice.h"
#include "SerialDataPushService.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>

// ---------------------------------------------------------------------------
// Internal value cache for one X-Plane dataref
// ---------------------------------------------------------------------------
union XplValue
{
	float fValue;
	int   iValue;
	float fArrayValue[16];
	int   iArrayValue[16];
	char  cArrayValue[256];
};

struct XplDataRefMeta {
	XPLMDataRef dataRef{ NULL };
	int         type{ 0 };
	int         valuesCount{ 0 };
	XplValue    value;
};

// ---------------------------------------------------------------------------
// Stream — runtime object for one output stream
// ---------------------------------------------------------------------------
struct Stream {
	StreamConfig                   config;
	std::unique_ptr<IDataSender>   sender;   // UdpDataPushSerice or SerialDataPushService
	std::thread                    senderThread;
	uint64_t                       lastSentMillis{ 0 };
};

// ---------------------------------------------------------------------------
// XplData — master dataref registry + per-stream update loop
// ---------------------------------------------------------------------------
class XplData {
private:
	// master registry: dataref name -> meta (XPLMDataRef handle + cached value)
	std::map<std::string, XplDataRefMeta> masterRegistry;

	// runtime streams
	std::vector<Stream> streams;

	uint64_t lastDataRefInitAttemptMillis{ 0 };
	int      dataRefInitializedCount{ 0 };
	bool     inFlight{ false };

	// helpers
	bool        initDataRef(XplDataRefMeta& meta, const std::string& name);
	int         initDataRefs();
	void        readDataRefValue(XplDataRefMeta& meta);
	std::string buildJson(const Stream& stream) const;

public:
	void init();
	void update();
	void onPluginReceiveMessage(XPLMPluginID inFromWho, int inMessage, void* inParam);
	void stopAllStreams();
	bool isInFlight() const { return inFlight; }
	std::string valuesAsJson() const;  // returns all master-registry values as JSON (for web API)
};
