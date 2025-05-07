#pragma once

#include "TcpListener.h"
#include <deque>
#include <string>
#include "ApiController.h"


class WebServer : public TcpListener
{
public:

	WebServer() : TcpListener(), m_logMessages(nullptr) { }

	void setLogMessages(std::deque<std::wstring>* logMessages) { m_logMessages = logMessages; }

protected:

	// Handler for client connections
	virtual void onClientConnected(int clientSocket);

	// Handler for client disconnections
	virtual void onClientDisconnected(int clientSocket);

	// Handler for when a message is received from the client
	virtual void onMessageReceived(int clientSocket, const char* msg, int length);

private:
	std::deque<std::wstring>* m_logMessages;
	ApiController m_apiController;
};