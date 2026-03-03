#include <string>
#include <istream>
#include <sstream>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <vector>
#include <iterator>
#include "WebServer.h"
#include "global.h"
#include "json.hpp"
#include <plog/Log.h>


using namespace std;

// ---------------------------------------------------------------------------
// buildStatusPage — generates the / home page showing streams + live values
// ---------------------------------------------------------------------------
static string buildStatusPage()
{
	const auto& streams = glb()->getConfig()->getStreams();

	// parse current master-registry values (best-effort, no lock needed for display)
	nlohmann::json values;
	try {
		values = nlohmann::json::parse(glb()->getXplData()->valuesAsJson());
	}
	catch (...) {}

	ostringstream h;
	h << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta http-equiv="refresh" content="5"/>
<title>XplDrToUdp Status</title>
<style>
  body{font-family:monospace;background:#1a1a2e;color:#e0e0e0;margin:24px}
  h1{color:#00d4ff;margin-bottom:4px}
  h2{color:#7ec8e3;border-bottom:1px solid #2a2a4a;padding-bottom:4px;margin-top:24px}
  a{color:#00d4ff;text-decoration:none}a:hover{text-decoration:underline}
  .nav{margin:12px 0 24px}
  .nav a{margin-right:18px;font-size:1.05em}
  .stream{background:#16213e;border:1px solid #2a2a4a;border-radius:6px;
          padding:14px;margin-bottom:14px}
  .stream-udp   {border-left:4px solid #00d4ff}
  .stream-serial{border-left:4px solid #f0a500}
  .stream-off   {border-left:4px solid #555;opacity:.6}
  .tag{display:inline-block;padding:1px 8px;border-radius:3px;
       font-size:.8em;margin-right:6px;vertical-align:middle}
  .tag-udp    {background:#00d4ff22;color:#00d4ff}
  .tag-serial {background:#f0a50022;color:#f0a500}
  .tag-off    {background:#88888822;color:#888}
  .meta{font-size:.9em;color:#aaa;margin:4px 0 10px}
  table{border-collapse:collapse;width:100%;font-size:.9em}
  th{text-align:left;color:#666;padding:3px 10px;font-weight:normal}
  td{padding:4px 10px;border-bottom:1px solid #1e1e3a}
  tr:last-child td{border-bottom:none}
  .dr-name {color:#ccc}
  .dr-alias{color:#ffd700}
  .dr-val  {color:#90ee90}
  .dr-na   {color:#444;font-style:italic}
  .footer  {margin-top:20px;color:#444;font-size:.8em}
</style>
</head>
<body>
<h1>XplDrToUdp</h1>
<div class="nav">
  <a href="/log">&#128203; Log</a>
  <a href="/api/xpl">&#128225; All values (JSON)</a>
</div>
)";

	h << "<h2>Streams (" << streams.size() << ")</h2>\n";

	for (size_t i = 0; i < streams.size(); i++) {
		const auto& s = streams[i];

		string cls = !s.enabled  ? "stream-off"
		           : s.udp.has_value() ? "stream-udp"
		           :                     "stream-serial";
		h << "<div class=\"stream " << cls << "\">\n";

		// --- headline row ---
		if (s.udp.has_value()) {
			h << "<span class=\"tag tag-udp\">UDP</span>"
			  << " <strong>" << s.udp->host << ":" << s.udp->port << "</strong>";
		} else if (s.serial.has_value()) {
			h << "<span class=\"tag tag-serial\">SERIAL</span>"
			  << " <strong>" << s.serial->port << " @ " << s.serial->baudRate << " baud</strong>";
		}
		if (!s.enabled)
			h << " <span class=\"tag tag-off\">DISABLED</span>";

		h << "\n<div class=\"meta\">interval: " << s.intervalMs << " ms"
		  << " &nbsp;|&nbsp; " << s.dataRefs.size() << " dataref(s)</div>\n";

		// --- datarefs table ---
		h << "<table>\n"
		  << "<tr><th>Dataref</th><th>Alias</th><th>Current value</th></tr>\n";

		for (const auto& dr : s.dataRefs) {
			h << "<tr><td class=\"dr-name\">" << dr.name << "</td>";

			// alias cell — only show if it differs from the name
			if (dr.alias != dr.name)
				h << "<td class=\"dr-alias\">" << dr.alias << "</td>";
			else
				h << "<td class=\"dr-na\">&mdash;</td>";

			// value — look up by name in the master registry dump
			if (!values.is_null() && values.contains(dr.name))
				h << "<td class=\"dr-val\">" << values[dr.name].dump() << "</td>";
			else
				h << "<td class=\"dr-na\">n/a</td>";

			h << "</tr>\n";
		}
		h << "</table></div>\n";
	}

	h << "<p class=\"footer\">Auto-refreshes every 5 s</p>\n"
	  << "</body></html>";
	return h.str();
}

// ---------------------------------------------------------------------------
// onMessageReceived — request router
// ---------------------------------------------------------------------------
void WebServer::onMessageReceived(int clientSocket, const char* msg, int length)
{
	istringstream iss(msg);
	vector<string> parsed((istream_iterator<string>(iss)), istream_iterator<string>());

	string content     = "<h1>404 Not Found</h1>";
	string contentType = "text/html";
	string path        = "/index.html";
	int    statusCode  = 404;

	if (parsed.size() >= 3 && parsed[0] == "GET") {
		path = parsed[1];
		if (path == "/") path = "/index.html";
	}

	// --- /api/* ---
	if (path.find("/api/") == 0) {
		string parameter = path.substr(5);
		ApiResponse response = m_apiController.handleRequest(parameter);
		if (response.handled) {
			content     = response.response;
			contentType = "application/json";
			statusCode  = 200;
		} else {
			PLOGE << "API call not found: " << path;
			content     = "API call not found";
			contentType = "text/plain";
			statusCode  = 404;
		}

	// --- /log ---
	} else if (path == "/log") {
		ostringstream oss;
		oss << "<html><head><title>Log</title></head><body><h1>Log</h1><ul>";
		if (m_logMessages != nullptr) {
			for (auto it = m_logMessages->rbegin(); it != m_logMessages->rend(); ++it) {
				string line(it->begin(), it->end());
				oss << "<li>" << line << "</li>";
			}
		}
		oss << "</ul></body></html>";
		content    = oss.str();
		statusCode = 200;

	// --- / (status page) ---
	} else if (path == "/index.html") {
		content    = buildStatusPage();
		statusCode = 200;

	// --- static files from wwwroot ---
	} else {
		ifstream f(".\\wwwroot" + path);
		if (f.good()) {
			content    = string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
			statusCode = 200;
		} else {
			cerr << "File not found: " << path << endl;
		}
		f.close();
	}

	// write HTTP response
	ostringstream oss;
	oss << "HTTP/1.1 " << statusCode << " OK\r\n";
	oss << "Cache-Control: no-cache, private\r\n";
	oss << "Content-Type: " << contentType << "\r\n";
	oss << "Content-Length: " << content.size() << "\r\n";
	oss << "\r\n";
	oss << content;

	string output = oss.str();
	sendToClient(clientSocket, output.c_str(), (int)output.size() + 1);
}

void WebServer::onClientConnected(int clientSocket)    {}
void WebServer::onClientDisconnected(int clientSocket) {}