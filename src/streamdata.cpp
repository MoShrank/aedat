#include "stream_dvs.hpp"
#include <string>
#include <iostream>


int main(int argc, char *argv[]) {
	const char* filename = "None"; 
	std::string command = "Nonset";
	uint32_t interval;
	uint32_t buffer_size;
	const char* port = "4950";	// Port number
	const char* IPAdress = NULL;	// IP Adress - if NULL, use own IP
	

	int nr_packets = 0;
	int id = 1;
	int exitcode, sockfd;

    if (argc < 3) {
        fprintf(stderr,"usage: insert interval and buffer size\n");
        exit(1);
    }

	if (argc == 4){
		filename = argv[3];
	}

	interval = strtol(argv[1], NULL, 0);
	buffer_size = strtol(argv[2], NULL, 0);

	DVSStream dvsstream(interval, buffer_size, port, IPAdress, NULL, filename); 

	auto davisHandler = dvsstream.connect2camera(id);
	davisHandler = dvsstream.startdatastream(davisHandler);

	while (nr_packets < 3000) { //command!="q"
		dvsstream.sendpacket(davisHandler, false);
		nr_packets += 1;
		//std::getline(std::cin, command);
	}

	exitcode = dvsstream.stopdatastream(davisHandler);

	dvsstream.closesocket();

}