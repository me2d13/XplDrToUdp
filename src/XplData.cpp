#include "XplData.h"
#include <plog/Log.h>
#include "XPLMPlugin.h"
#include <chrono>
#include "global.h"
#include "json.hpp"


void XplData::init()
{
	initDataRefs();
}

bool XplData::initDataRef(XplDataRefMeta& dataRefMeta, const char* name) {
	if (dataRefMeta.dataRef == NULL) {
		dataRefMeta.dataRef = XPLMFindDataRef(name);
		if (dataRefMeta.dataRef == NULL) {
			PLOGE << "Dataref " << name << " not found";
		} else {
			PLOGD << "Dataref " << name << " found";
			// type of dataref
			dataRefMeta.type = XPLMGetDataRefTypes(dataRefMeta.dataRef);
			dataRefMeta.value.fValue = 0.0f;
			if (dataRefMeta.type == xplmType_Float) {
				PLOGD << "Dataref " << name << " is a float";
			}
			else if (dataRefMeta.type == xplmType_Int) {
				dataRefMeta.value.iValue = 0;
				PLOGD << "Dataref " << name << " is an int";
			}
			else if (dataRefMeta.type == xplmType_Data) {
				dataRefMeta.value.cArrayValue[0] = '\0'; // ensure null termination
				PLOGD << "Dataref " << name << " is a data";
			}
			else if (dataRefMeta.type == xplmType_IntArray) {
				PLOGD << "Dataref " << name << " is a int array";
				// get the size of the data array
				dataRefMeta.valuesCount = XPLMGetDatavi(dataRefMeta.dataRef, NULL, 0, 0);
				for (size_t i = 0; i < dataRefMeta.valuesCount; i++)
				{
					dataRefMeta.value.iArrayValue[i] = 0;
				}
				PLOGD << "Dataref " << name << " has " << dataRefMeta.valuesCount << " values";
			}
			else if (dataRefMeta.type == xplmType_FloatArray) {
				PLOGD << "Dataref " << name << " is a float array";
				// get the size of the data array
				dataRefMeta.valuesCount = XPLMGetDatavf(dataRefMeta.dataRef, NULL, 0, 0);
				for (size_t i = 0; i < dataRefMeta.valuesCount; i++)
				{
					dataRefMeta.value.fArrayValue[i] = 0.0f;
				}
				PLOGD << "Dataref " << name << " has " << dataRefMeta.valuesCount << " values";
			}
			else {
				PLOGE << "Dataref " << name << " is of unknown type " << dataRefMeta.type;
			}

		}
	}
	return dataRefMeta.dataRef != NULL;
}

int XplData::initDataRefs()
{
	int count = 0;
	if (dataRefs.size() > 0) {
		dataRefs.clear(); // clear the vector if it is not empty
	}
	for (const auto& dataRefName : glb()->getConfig()->getDataRefNames()) {
		XplDataRefMeta dataRefMeta;
		dataRefMeta.dataRef = NULL;
		if (initDataRef(dataRefMeta, dataRefName.c_str())) {
			count++;
		}
		dataRefs.push_back(dataRefMeta);
	}
	dataRefInitializedCount = count;
	return count;
}

uint64_t timeSinceEpochMillisec() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void XplData::logDataRefValues()
{
	// loop through all datarefs and log their values
	for (size_t i = 0; i < dataRefs.size(); i++) {
		if (dataRefs[i].dataRef != NULL) {
			std::string name = glb()->getConfig()->getDataRefNames()[i];
			// read the dataref value
			if (dataRefs[i].type == xplmType_Float) {
				PLOGD << "XplData::logDataRefValues() - Dataref " << name << ": " << dataRefs[i].value.fValue;
			}
			else if (dataRefs[i].type == xplmType_Int) {
				PLOGD << "XplData::logDataRefValues() - Dataref " << name << ": " << dataRefs[i].value.iValue;
			}
			else if (dataRefs[i].type == xplmType_FloatArray) {
				PLOGD << "XplData::logDataRefValues() - Dataref " << name << ": ";
				for (int j = 0; j < dataRefs[i].valuesCount; j++) {
					PLOGD << dataRefs[i].value.fArrayValue[j] << " ";
				}
			}
			else if (dataRefs[i].type == xplmType_IntArray) {
				PLOGD << "XplData::logDataRefValues() - Dataref " << name << ": ";
				for (int j = 0; j < dataRefs[i].valuesCount; j++) {
					PLOGD << dataRefs[i].value.iArrayValue[j] << " ";
				}
			}
			else if (dataRefs[i].type == xplmType_Data) {
				PLOGD << "XplData::logDataRefValues() - Dataref " << name << ": " << dataRefs[i].value.cArrayValue;
			}
			else {
				PLOGE << "XplData::logDataRefValues() - Unknown type for dataref " << name;
			}
		}
	}
}

void XplData::getDataRefValue(XplDataRefMeta& dataRefMeta) {
	if (dataRefMeta.dataRef != NULL) {
		// read the dataref value
		if (dataRefMeta.type == xplmType_Float) {
			dataRefMeta.value.fValue = XPLMGetDataf(dataRefMeta.dataRef);
		}
		else if (dataRefMeta.type == xplmType_Int) {
			dataRefMeta.value.iValue = XPLMGetDatai(dataRefMeta.dataRef);
		}
		else if (dataRefMeta.type == xplmType_FloatArray) {
			XPLMGetDatavf(dataRefMeta.dataRef, dataRefMeta.value.fArrayValue, 0, dataRefMeta.valuesCount);
		}
		else if (dataRefMeta.type == xplmType_IntArray) {
			XPLMGetDatavi(dataRefMeta.dataRef, dataRefMeta.value.iArrayValue, 0, dataRefMeta.valuesCount);
		}
		else if (dataRefMeta.type == xplmType_Data) {
			XPLMGetDatab(dataRefMeta.dataRef, dataRefMeta.value.cArrayValue, 0, sizeof(dataRefMeta.value.cArrayValue));
			// ensure null termination
			dataRefMeta.value.cArrayValue[sizeof(dataRefMeta.value.cArrayValue) - 1] = '\0';
		}
		else {
			PLOGE << "XplData::getDataRefValue() - Unknown type for dataref " << dataRefMeta.dataRef;
		}
	}

}

void XplData::update()
{
	// if any datarefs are not initialized, try to initialize them, but only once a minute
	// get current miliseconds
	uint64_t currentMillis = timeSinceEpochMillisec();
	// if we have not tried to initialize datarefs yet or it has been more than a 10s since we last tried
	if (dataRefInitializedCount < glb()->getConfig()->getDataRefNames().size() && lastDataRefInitAttemptMillis == 0 || currentMillis - lastDataRefInitAttemptMillis > 10000) {
		initDataRefs();
		// set last dataref init attempt time to now
		lastDataRefInitAttemptMillis = currentMillis;
		//logDataRefValues();
	}
	if (currentMillis - lastValuesUpdateMillis >= glb()->getConfig()->getUpdateInterval()) {
		for (size_t i = 0; i < dataRefs.size(); i++) {
			if (dataRefs[i].dataRef != NULL) {
				getDataRefValue(dataRefs[i]);
			}
		}
		lastValuesUpdateMillis = currentMillis;
		// log the dataref values
		//logDataRefValues();
		// 
		// send the dataref values to the UDP server
		glb()->getUdpDataPushService()->sendData(valuesAsJson());
	}
}

void XplData::onPluginReceiveMessage(XPLMPluginID inFromWho, int	inMessage, void* inParam)
{
	int planeIndex = (int)inParam;
	switch (inMessage)
	{
	case XPLM_MSG_PLANE_LOADED:
		PLOGD << "XplData::onPluginReceiveMessage() - XPLM_MSG_PLANE_LOADED with index " << planeIndex;
		if (planeIndex == 0) {
			// some datarefs are plane specific, so we need to reinitialize them
			initDataRefs();
			inFlight = true;
		}
		break;
	case XPLM_MSG_PLANE_UNLOADED:
		PLOGD << "XplData::onPluginReceiveMessage() - XPLM_MSG_PLANE_UNLOADED with index " << planeIndex;
		if (planeIndex == 0) {
			inFlight = false;
		}
		break;
	default:
		break;
	}
}

std::string XplData::valuesAsJson() {
	logDataRefValues();
	// create a json object
	nlohmann::json j;
	// loop through all datarefs and add them to the json object
	for (size_t i = 0; i < dataRefs.size(); i++) {
		if (dataRefs[i].dataRef != NULL) {
			std::string name = glb()->getConfig()->getDataRefNames()[i];
			// read the dataref value
			if (dataRefs[i].type == xplmType_Float) {
				j[name] = dataRefs[i].value.fValue;
			}
			else if (dataRefs[i].type == xplmType_Int) {
				j[name] = dataRefs[i].value.iValue;
			}
			else if (dataRefs[i].type == xplmType_FloatArray) {
				for (int jIndex = 0; jIndex < dataRefs[i].valuesCount; jIndex++) {
					j[name][jIndex] = dataRefs[i].value.fArrayValue[jIndex];
				}
			}
			else if (dataRefs[i].type == xplmType_IntArray) {
				for (int jIndex = 0; jIndex < dataRefs[i].valuesCount; jIndex++) {
					j[name][jIndex] = dataRefs[i].value.iArrayValue[jIndex];
				}
			}
			else if (dataRefs[i].type == xplmType_Data) {
				j[name] = std::string(dataRefs[i].value.cArrayValue);
			}
			else {
				PLOGE << "XplData::valuesAsJson() - Unknown type for dataref " << name;
			}
		}
	}
	return j.dump();
}