#include "Config.h"

#include <iostream>
#include <fstream>
#include <string>
#include "json.hpp"
#include <plog/Log.h>

bool Config::readConfigFile(std::string pConfigPath)
{
	auto configPath = pConfigPath;
	// read a JSON file if it exists
	std::string fullPath = configPath + "\\config.json";
	std::ifstream fileStream(fullPath);
	if (!fileStream.is_open())
	{
		PLOGE << fullPath << " not found";
		return false;
	}
	PLOGD << "Reading config from " << fullPath;
	nlohmann::json j;
	try {
		fileStream >> j;
		// read the values from the JSON file
		udpIpAddress = j.value("receiverHost", "127.0.0.1");
		udpPort = j.value("receiverPort", 5000);
		updateInterval = j.value("updateInterval", 1000);
		// read the dataref names from the JSON file
		dataRefNames = j.value("dataRefs", std::vector<std::string>());
		serverPort = j.value("serverPort", 8080);
	}
	catch (nlohmann::json::exception& e) {
		PLOGE << "Error reading config.json: " << e.what();
		return false;
	}
	// close the file
	fileStream.close();
	// print the values read from the JSON file
	PLOGD << "Receiver host: " << udpIpAddress;
	PLOGD << "Receiver port: " << udpPort;
	PLOGD << "Update interval: " << updateInterval;
	PLOGD << "Datarefs: ";
	for (const auto& dataRef : dataRefNames) {
		PLOGD << dataRef;
	}
	PLOGD << "Server port: " << serverPort;
	return true;
}

