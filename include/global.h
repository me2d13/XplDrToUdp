#pragma once

#include "Config.h"
#include "XplData.h"

class Globals
{
private:
	Config  config;
	XplData xplData;
public:
	Config*  getConfig()  { return &config; }
	XplData* getXplData() { return &xplData; }
};

Globals* glb();