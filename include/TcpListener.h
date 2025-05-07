#pragma once

#include <WS2tcpip.h>


#pragma comment (lib, "ws2_32.lib")

class TcpListener
{

public:

	TcpListener() :	m_ipAddress("0.0.0.0"), m_port(8080), m_running(false) { }

	// Initialize the listener
	int init();

	// Run the listener
	int run();

	void stop();

	void setPort(int port) { m_port = port; }
	void setIpAddress(const char* ipAddress) { m_ipAddress = ipAddress; }

protected:

	// Handler for client connections
	virtual void onClientConnected(int clientSocket);

	// Handler for client disconnections
	virtual void onClientDisconnected(int clientSocket);

	// Handler for when a message is received from the client
	virtual void onMessageReceived(int clientSocket, const char* msg, int length);

	// Send a message to a client
	void sendToClient(int clientSocket, const char* msg, int length);

	// Broadcast a message from a client
	void broadcastToClients(int sendingClient, const char* msg, int length);

private:

	const char* m_ipAddress;	// IP Address server will run on
	int				m_port;			// Port # for the web service
	int				m_socket;		// Internal FD for the listening socket
	fd_set			m_master;		// Master file descriptor set
	bool			m_running;		// Flag indicating the listener is running
};