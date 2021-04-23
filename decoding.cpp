#include "dvs_data.hpp"
#include <string>
#include <iostream>

static void globalShutdownSignalHandler(int signal) {
    static atomic_bool globalShutdown(false);
    // Simply set the running flag to false on SIGTERM and SIGINT (CTRL+C) for global shutdown.
    if (signal == SIGTERM || signal == SIGINT) {
        globalShutdown.store(true);
    }
}

static void usbShutdownHandler(void *ptr) {
	static atomic_bool globalShutdown(false);
	(void) (ptr); // UNUSED.

	globalShutdown.store(true);
}


int main(int argc, char *argv[]) {
	std::string command = "Nonset";
	uint32_t interval; 
	
	int id = 1;
	int exitcode;

	interval = strtol(argv[1], NULL, 0);
	
	DVSData dvsdata(interval); 

	auto davisHandler = dvsdata.connect2camera(id, globalShutdownSignalHandler);
	davisHandler = dvsdata.startdatastream(davisHandler, usbShutdownHandler);

	while (command!="q") {
		auto tensor = dvsdata.update(davisHandler);
		std::getline(std::cin, command);
	}

	//printf("Now we do a lot of funny stuff like count to 10:\n");
	//for(int i=0; i < 11; i++){
	//	printf("%d\n", i);
	//}
	//auto second_tensor = dvsdata.update(davisHandler);

	exitcode = dvsdata.stopdatastream(davisHandler);
}