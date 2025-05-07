#pragma once

#include <string>
#include <mutex>
#include <condition_variable>
#include <winsock2.h>


class UdpDataPushSerice
{
	private:
		int successPacketsCount{ 0 };
		int failedPacketsCount{ 0 };
		bool isConnected{ false };
		bool shouldTerminate{ false };
		std::string dataToSend{ };
		SOCKET sock;
		sockaddr_in destination;
		std::mutex mtx;
		std::condition_variable cv;
		void doSendData();
public:
		bool connect();
		bool disconnect();
		void sendData(std::string data);
		void run();
		int getSuccessPacketsCount() { return successPacketsCount; };
		int getFailedPacketsCount() { return failedPacketsCount; };
		void terminate();
};

