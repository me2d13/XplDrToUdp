#include "SerialDataPushService.h"
#include <plog/Log.h>
#include <string>


// ---------------------------------------------------------------------------
// connect — open the COM port and configure baud rate / framing
// ---------------------------------------------------------------------------
bool SerialDataPushService::connect()
{
	if (isConnected) {
		PLOGD << "Serial: already connected to " << serialConfig.port;
		return true;
	}

	// Windows requires the \\.\  prefix for port names (mandatory for COM10+,
	// harmless for lower-numbered ports)
	std::string portPath = "\\\\.\\" + serialConfig.port;
	hPort = CreateFileA(
		portPath.c_str(),
		GENERIC_WRITE,        // write-only — we only push data out
		0,                    // exclusive access
		NULL,
		OPEN_EXISTING,
		0,                    // synchronous I/O
		NULL
	);

	if (hPort == INVALID_HANDLE_VALUE) {
		PLOGE << "Serial: failed to open " << serialConfig.port
		      << " (error " << GetLastError() << ")";
		return false;
	}

	// Configure the port state
	DCB dcb{};
	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(hPort, &dcb)) {
		PLOGE << "Serial: GetCommState failed (error " << GetLastError() << ")";
		CloseHandle(hPort);
		hPort = INVALID_HANDLE_VALUE;
		return false;
	}

	dcb.BaudRate = serialConfig.baudRate;
	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity   = NOPARITY;
	dcb.fBinary  = TRUE;

	if (!SetCommState(hPort, &dcb)) {
		PLOGE << "Serial: SetCommState failed (error " << GetLastError() << ")";
		CloseHandle(hPort);
		hPort = INVALID_HANDLE_VALUE;
		return false;
	}

	// Set write timeout (5 s max per write, 100 ms per byte — generous)
	COMMTIMEOUTS timeouts{};
	timeouts.WriteTotalTimeoutConstant   = 5000;
	timeouts.WriteTotalTimeoutMultiplier = 100;
	SetCommTimeouts(hPort, &timeouts);

	// Set transmit/receive queue sizes
	SetupComm(hPort, 0, 4096);

	isConnected = true;
	PLOGD << "Serial: connected to " << serialConfig.port
	      << " @ " << serialConfig.baudRate << " baud";
	return true;
}

// ---------------------------------------------------------------------------
// disconnect — close the COM port handle
// ---------------------------------------------------------------------------
bool SerialDataPushService::disconnect()
{
	if (!isConnected) {
		PLOGD << "Serial: already disconnected";
		return true;
	}
	CloseHandle(hPort);
	hPort = INVALID_HANDLE_VALUE;
	isConnected = false;
	PLOGD << "Serial: disconnected from " << serialConfig.port;
	return true;
}

// ---------------------------------------------------------------------------
// doSendData — write the pending string to the COM port (called from worker)
// ---------------------------------------------------------------------------
void SerialDataPushService::doSendData()
{
	if (!isConnected) {
		PLOGD << "Serial: not connected on send attempt, trying to connect...";
		if (!connect()) {
			PLOGE << "Serial: failed to connect on send attempt";
			failedPacketsCount++;
			return;
		}
	}

	// Append newline so the receiver can frame messages easily
	std::string frame = dataToSend + "\n";

	DWORD written = 0;
	BOOL  ok = WriteFile(hPort, frame.c_str(), (DWORD)frame.size(), &written, NULL);

	if (!ok || written != frame.size()) {
		PLOGE << "Serial: WriteFile failed (error " << GetLastError()
		      << ", wrote " << written << "/" << frame.size() << " bytes)";
		failedPacketsCount++;
	}
	else {
		PLOGD << "Serial: sent " << written << " bytes to " << serialConfig.port;
		successPacketsCount++;
	}
	dataToSend.clear();
}

// ---------------------------------------------------------------------------
// run — worker thread: waits for data, writes it, loops
// ---------------------------------------------------------------------------
void SerialDataPushService::run()
{
	if (!isConnected) {
		PLOGD << "Serial: not connected on run, trying to connect...";
		if (!connect()) {
			PLOGE << "Serial: failed to connect on run — thread exiting";
			return;
		}
	}

	while (!shouldTerminate) {
		std::unique_lock<std::mutex> lock(mtx);
		while (dataToSend.empty()) {
			cv.wait(lock);
			if (shouldTerminate) break;
		}
		doSendData();
	}

	PLOGD << "Serial: worker thread terminating";
	disconnect();
}

// ---------------------------------------------------------------------------
// sendData — called from XplData flight-loop thread, posts data to worker
// ---------------------------------------------------------------------------
void SerialDataPushService::sendData(const std::string& data)
{
	std::unique_lock<std::mutex> lock(mtx);
	dataToSend = data;
	cv.notify_one();
}

// ---------------------------------------------------------------------------
// terminate — signals the worker thread to exit
// ---------------------------------------------------------------------------
void SerialDataPushService::terminate()
{
	PLOGD << "Serial: terminate requested";
	shouldTerminate = true;
	cv.notify_one();
}
