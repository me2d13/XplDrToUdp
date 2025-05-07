#include "ApiController.h"
#include "global.h"


ApiResponse ApiController::handleRequest(std::string request) {
	// check if request is for xpl
	if (request == "xpl") {
		return { true, handleXplRequest(), 200 };
	}
	if (request == "reload") {
		glb()->getXplData()->init();
		return { true, "Xpl datarefs reloaded", 200 };
	}
	// request not handled
	return { false, "", 400 };
}

std::string ApiController::handleXplRequest() {
	return glb()->getXplData()->valuesAsJson();
}