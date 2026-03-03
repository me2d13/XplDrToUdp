#pragma once

#include "IDataSender.h"
#include "Config.h"
#include <string>
#include <mutex>
#include <condition_variable>
#include <windows.h>


class SerialDataPushService : public IDataSender
{
private:
	SerialConfig            serialConfig;
	HANDLE                  hPort{ INVALID_HANDLE_VALUE };
	bool                    isConnected{ false };
	bool                    shouldTerminate{ false };
	std::string             dataToSend{ };
	std::mutex              mtx;
	std::condition_variable cv;
	int                     successPacketsCount{ 0 };
	int                     failedPacketsCount{ 0 };
	void doSendData();

public:
	explicit SerialDataPushService(const SerialConfig& config) : serialConfig(config) {}

	bool connect()                         override;
	bool disconnect()                      override;
	void sendData(const std::string& data) override;
	void run()                             override;
	void terminate()                       override;

	int  getSuccessPacketsCount() { return successPacketsCount; }
	int  getFailedPacketsCount()  { return failedPacketsCount; }
};
