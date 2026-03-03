#pragma once

#include <string>

// ---------------------------------------------------------------------------
// IDataSender — abstract interface for all output push services
// ---------------------------------------------------------------------------
class IDataSender
{
public:
	virtual bool connect()    = 0;
	virtual bool disconnect() = 0;
	virtual void sendData(const std::string& data) = 0;
	virtual void run()        = 0;
	virtual void terminate()  = 0;
	virtual ~IDataSender()    = default;
};
