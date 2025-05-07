#include "global.h"

Globals* glb() {
	static Globals* globals = new Globals();
	return globals;
}