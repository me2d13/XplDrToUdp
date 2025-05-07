#include "UdpDataPushSerice.h"
#include "global.h"
#include <iostream>
#include <string>
#include <plog/Log.h>
#include <WS2tcpip.h>


bool UdpDataPushSerice::connect() {
	// check if already connected
	if (isConnected) {
        PLOGD << "Already connected";
		return true;
	}

    // Initialise Winsock DLL
    // See https://beej.us/guide/bgnet/html/#windows 
    WSADATA wsaData;
    // MAKEWORD(1,1) for Winsock 1.1, MAKEWORD(2,0) for Winsock 2.0
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        PLOGE << "WSAStartup failed: " << WSAGetLastError();
		return false;
    }

    // Set up connection and send 
    std::string hostname{ glb()->getConfig()->getUdpIpAddress()};
    uint16_t port = glb()->getConfig()->getUdpPort();

    sock = ::socket(AF_INET, SOCK_DGRAM, 0);

    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    std::wstring stemp = std::wstring(hostname.begin(), hostname.end());
    InetPton(AF_INET, stemp.c_str(), &destination.sin_addr.s_addr);

    isConnected = true;
    PLOGD << "Connected to " << hostname << ":" << port;
    return true;
}

bool UdpDataPushSerice::disconnect() {
    // check if already connected
    if (!isConnected) {
        PLOGD << "Already disconnected";
        return true;
    }
    ::closesocket(sock);

    // Clean up sockets library
    WSACleanup();
    return true;
}

void UdpDataPushSerice::doSendData() {
	// check if connected
    if (!isConnected) {
		PLOGD << "Not connected on send attempt, trying to connect now...";
        if (!connect()) {
			PLOGE << "Failed to connect on send attempt";
            failedPacketsCount++;
			return;
		}
	}

	int n_bytes = ::sendto(sock, dataToSend.c_str(), dataToSend.length(), 0, reinterpret_cast<sockaddr*>(&destination), sizeof(destination));
    if (n_bytes == SOCKET_ERROR) {
		PLOGE << "sendto failed: " << WSAGetLastError();
		failedPacketsCount++;
	}
    else {
        PLOGD << "Sent " << n_bytes << " bytes to " << glb()->getConfig()->getUdpIpAddress() << ":" << glb()->getConfig()->getUdpPort();
		successPacketsCount++;
	}
	dataToSend.clear();
}

void UdpDataPushSerice::run() {
	// check if connected
    if (!isConnected) {
		PLOGD << "Not connected on run attempt, trying to connect now...";
        if (!connect()) {
			PLOGE << "Failed to connect on run attempt";
			return;
		}
	}

    while (!shouldTerminate) {
		std::unique_lock<std::mutex> lock(mtx);
        // wait for data to be available
        while (dataToSend.empty()) {
			cv.wait(lock);
            if (shouldTerminate) {
				break;
			}
		}
		doSendData();
	}
    PLOGD << "Terminating UDP push service";
    disconnect();
}

void UdpDataPushSerice::sendData(std::string data) {
	std::unique_lock<std::mutex> lock(mtx);
	dataToSend = data;
	cv.notify_one();
}

void UdpDataPushSerice::terminate() {
    PLOGD << "Setting shouldTerminate to true in UDP push service";
	shouldTerminate = true;
	cv.notify_one();
}