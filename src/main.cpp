#include "WebServer.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include "XPLMUtilities.h"
#include "XPLMDefs.h"
#include "XPLMPlugin.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <deque>
#include <string>
#include <thread>
#if IBM
#include <windows.h>
#endif
#include <plog/Log.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Initializers/ConsoleInitializer.h>
#include <plog/Init.h>
#include "global.h"

namespace plog
{
	template<class Formatter> // Typically a formatter is passed as a template parameter.
	class MyAppender : public IAppender // All appenders MUST inherit IAppender interface.
	{
	public:
		virtual void write(const Record& record) PLOG_OVERRIDE // This is a method from IAppender that MUST be implemented.
		{
			util::nstring str = Formatter::format(record); // Use the formatter to get a string from a record.

			// if queue is full, remove the oldest message
			if (messages.size() >= maxMessages)
				messages.pop_front();

			messages.push_back(str); // Store a log message in a list.
		}

		std::deque<std::wstring>& getMessages()
		{
			return messages;
		}

	private:
		std::deque<util::nstring> messages;
		const int maxMessages = 100;
	};
}



static float	MyFlightLoopCallback(
	float                inElapsedSinceLastCall,
	float                inElapsedTimeSinceLastFlightLoop,
	int                  inCounter,
	void* inRefcon);

WebServer webServer;
std::thread webServerThread;
std::thread udpPushThread;

PLUGIN_API int XPluginStart(
	char* outName,
	char* outSig,
	char* outDesc)
{

	static plog::MyAppender<plog::TxtFormatter> myAppender; // Create our custom appender.
	plog::init(plog::debug, &myAppender);
	webServer.setLogMessages(&myAppender.getMessages());
	PLOG_INFO << "Logging is on";

	char	outputPath[255];

	strcpy(outName, "XplDrToUdpPlugin");
	strcpy(outSig, "eu.me2d.xpldrtoudp");
	strcpy(outDesc, "A plugin to publish dataref values over UDP");

	XPLMGetSystemPath(outputPath);
	PLOG_INFO <<"System path: " << outputPath;

	// get my plugin ID
	XPLMPluginID myID = XPLMGetMyID();
	PLOG_INFO << "My plugin ID: " << myID;
	XPLMGetPluginInfo(myID, NULL, outputPath, NULL, NULL);
	PLOG_INFO << "Plugin full path: " << outputPath;
	XPLMExtractFileAndPath(outputPath);
	PLOG_INFO << "Plugin directory: " << outputPath;
	glb()->getConfig()->readConfigFile(outputPath);
	glb()->getXplData()->init();
	//glb()->getSerialService()->connect(glb()->getConfig()->getComPort());
	//glb()->getXpl2SerialSyncService()->start();

	// start web server in a separate thread


	if (webServer.init() != 0) {
		XPLMDebugString("Failed to start web server\n");
	}
	else {
		XPLMDebugString("Started web server\n");
		webServerThread = std::thread(&WebServer::run, &webServer);
	}

	// start UDP push service in a separate thread
	udpPushThread = std::thread(&UdpDataPushSerice::run, glb()->getUdpDataPushService());


	/* Register our callback for once a second.  Positive intervals
	 * are in seconds, negative are the negative of sim frames.  Zero
	 * registers but does not schedule a callback for time. */
	XPLMRegisterFlightLoopCallback(
		MyFlightLoopCallback,	/* Callback */
		1.0,					/* Interval */
		NULL);					/* refcon not used. */

	return 1;
}

PLUGIN_API void	XPluginStop(void)
{
	/* Unregister the callback */
	XPLMUnregisterFlightLoopCallback(MyFlightLoopCallback, NULL);

	XPLMDebugString("Stopping web server\n");
	webServer.stop();
	XPLMDebugString("Waiting for web server to finish\n");
	webServerThread.join();
	XPLMDebugString("Web server stopped\n");

	XPLMDebugString("Stopping UDP push service\n");
	glb()->getUdpDataPushService()->terminate();
	udpPushThread.join();

	//glb()->getXpl2SerialSyncService()->stop();
	//glb()->getSerialService()->shutdown();

}

PLUGIN_API void XPluginDisable(void)
{
}

PLUGIN_API int XPluginEnable(void)
{
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(
	XPLMPluginID	inFromWho,
	int				inMessage,
	void* inParam)
{
	glb()->getXplData()->onPluginReceiveMessage(inFromWho, inMessage, inParam);
}

float	MyFlightLoopCallback(
	float                inElapsedSinceLastCall,
	float                inElapsedTimeSinceLastFlightLoop,
	int                  inCounter,
	void* inRefcon)
{
	/* The actual callback.  First we read the sim's time and the data. */
	float	elapsed = XPLMGetElapsedTime();

	glb()->getXplData()->update();

	/* Return 1.0 to indicate that we want to be called again in 1 second. */
	return 0.1;
}

