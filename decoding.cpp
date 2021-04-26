#include "dvs_data.hpp"
#include <string>
#include <iostream>


int main(int argc, char *argv[]) {
	std::string command = "Nonset";
	uint32_t interval;
	uint32_t buffer_size; 
	
	int id = 1;
	int exitcode;

	interval = strtol(argv[1], NULL, 0);
	buffer_size = strtol(argv[2], NULL, 0);

	
	DVSData dvsdata(interval, buffer_size); 

	auto davisHandler = dvsdata.connect2camera(id);
	davisHandler = dvsdata.startdatastream(davisHandler);

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