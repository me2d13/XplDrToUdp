#pragma once

#include "XPLMDataAccess.h"
#include <string>
#include <vector>

union XplValue
{
	float fValue;
	int iValue;
	float fArrayValue[16];
	int iArrayValue[16];
	char cArrayValue[256];
};

// DR metadata struct holding XPLMDataRef and type
struct XplDataRefMeta {
	XPLMDataRef dataRef{ NULL };
	int type;
	int valuesCount{ 0 };
	XplValue value;
};

class XplData {
private:
	// vector to hold XPLMDataRef for each dataref
	std::vector<XplDataRefMeta> dataRefs;
	uint64_t lastDataRefInitAttemptMillis{ 0 };
	uint64_t lastValuesUpdateMillis{ 0 };
	int dataRefInitializedCount{ 0 };
	bool inFlight{ false };
	bool initDataRef(XplDataRefMeta& dataRefMeta, const char* name);
	int initDataRefs();
	void logDataRefValues();
public:
	void init();
	void update();
	void getDataRefValue(XplDataRefMeta& dataRefMeta);
	void onPluginReceiveMessage(XPLMPluginID inFromWho, int	inMessage, void* inParam);
	bool isInFlight() { return inFlight; }
	std::string valuesAsJson();
};

