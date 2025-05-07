#pragma once

#include "Config.h"
#include "XplData.h"
#include "UdpDataPushSerice.h"

class Globals
{
private:
	Config config;
	XplData xplData;
	UdpDataPushSerice udpDataPushService;
public:
	Config* getConfig() { return &config; }
	XplData* getXplData() { return &xplData;}
	UdpDataPushSerice* getUdpDataPushService() { return &udpDataPushService; }
};

Globals* glb();