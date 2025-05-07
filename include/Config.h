#pragma once

#include <string>
#include <vector>

class Config
{
private:
	// collection to hold dataref names
	std::vector<std::string> dataRefNames;
	// ip address of UDP receiver
	std::string udpIpAddress;
	// port of UDP receiver
	int udpPort;
	// update interval in milliseconds
	int updateInterval;
	int serverPort;
public:
	Config() : updateInterval(1000), serverPort(8080) {};
	bool readConfigFile(std::string pConfigPath);
	std::vector<std::string> getDataRefNames() { return dataRefNames; }
	std::string getUdpIpAddress() { return udpIpAddress; }
	int getUdpPort() { return udpPort; }
	int getUpdateInterval() { return updateInterval; }
	int getServerPort() { return serverPort; }
};