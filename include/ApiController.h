#pragma once

#include <string>

struct ApiResponse {
	bool handled;
	std::string response;
	int status;
};

class ApiController {
	public:
	ApiResponse handleRequest(std::string request);
	std::string handleXplRequest();
};