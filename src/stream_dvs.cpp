#define LIBCAER_FRAMECPP_OPENCV_INSTALLED 0

#include <libcaercpp/devices/davis.hpp>
#include <libcaer/devices/davis.h>
#include "stream_dvs.hpp"
#include "convert.hpp"

#include <atomic>
#include <csignal>

// socket programming
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


// Constructor - initialize socket 
DVSStream::DVSStream(uint32_t interval, uint32_t bfsize, const char* port, const char* IP, struct addrinfo *point, const char* file){
    struct addrinfo hints, *servinfo;
    int rv;

    container_interval = interval; 
    buffer_size = bfsize;
    serverport = port;
    IPAdress = IP;
    p = point;
    filename = file;

    // Open file for writing events if specified
    if (strcmp (filename,"None") != 0){
        printf("Write output to file: %s\n", filename);
        fileOutput.open(filename, std::fstream::app);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4, to AF_INET6 to use IPv6
    hints.ai_socktype = SOCK_DGRAM;

    if (IPAdress == NULL){
        hints.ai_flags = AI_PASSIVE; // if IP adress not specified, use my IP
    }

    if ((rv = getaddrinfo(IPAdress, serverport, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        throw "Error raised";
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        throw "Error raised";
    }
}

// Global Shutdown Handler
void DVSStream::globalShutdownSignalHandler(int signal) {
    static atomic_bool globalShutdown(false);
    // Simply set the running flag to false on SIGTERM and SIGINT (CTRL+C) for global shutdown.
    if (signal == SIGTERM || signal == SIGINT) {
        globalShutdown.store(true);
    }
}

// USB Shutdown Handler
void DVSStream::usbShutdownHandler(void *ptr) {
    static atomic_bool globalShutdown(false);
	(void) (ptr); // UNUSED.

	globalShutdown.store(true);
}


// Open a DAVIS given a USB ID, and don't care about USB bus or SN restrictions.
libcaer::devices::davis DVSStream::connect2camera(int ID){

    #if defined(_WIN32)
        if (signal(SIGTERM, &globalShutdownSignalHandler) == SIG_ERR) {
            libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
                "Failed to set signal handler for SIGTERM. Error: %d.", errno);
            return (EXIT_FAILURE);
        }

        if (signal(SIGINT, &globalShutdownSignalHandler) == SIG_ERR) {
            libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
                "Failed to set signal handler for SIGINT. Error: %d.", errno);
            return (EXIT_FAILURE);
        }
    #else
        struct sigaction shutdownAction;
        
        shutdownAction.sa_handler = &DVSStream::globalShutdownSignalHandler;
        shutdownAction.sa_flags   = 0;
        sigemptyset(&shutdownAction.sa_mask);
        sigaddset(&shutdownAction.sa_mask, SIGTERM);
        sigaddset(&shutdownAction.sa_mask, SIGINT);

        if (sigaction(SIGTERM, &shutdownAction, NULL) == -1) {
            libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
                "Failed to set signal handler for SIGTERM. Error: %d.", errno);
            return (EXIT_FAILURE);
        }

        if (sigaction(SIGINT, &shutdownAction, NULL) == -1) {
            libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
                "Failed to set signal handler for SIGINT. Error: %d.", errno);
            return (EXIT_FAILURE);
        }
    #endif

    libcaer::devices::davis davisHandle = libcaer::devices::davis(1);

    // Let's take a look at the information we have on the device.
    struct caer_davis_info davis_info = davisHandle.infoGet();

    printf("%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n", davis_info.deviceString,
        davis_info.deviceID, davis_info.deviceIsMaster, davis_info.dvsSizeX, davis_info.dvsSizeY,
        davis_info.logicVersion);

    // Send the default configuration before using the device.
    // No configuration is sent automatically!
    davisHandle.sendDefaultConfig();
    

    // Tweak some biases, to increase bandwidth in this case.
    struct caer_bias_coarsefine coarseFineBias;

    coarseFineBias.coarseValue        = 2;
    coarseFineBias.fineValue          = 116;
    coarseFineBias.enabled            = true;
    coarseFineBias.sexN               = false;
    coarseFineBias.typeNormal         = true;
    coarseFineBias.currentLevelNormal = true;


    davisHandle.configSet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP, caerBiasCoarseFineGenerate(coarseFineBias));

    coarseFineBias.coarseValue        = 1;
    coarseFineBias.fineValue          = 33;
    coarseFineBias.enabled            = true;
    coarseFineBias.sexN               = false;
    coarseFineBias.typeNormal         = true;
    coarseFineBias.currentLevelNormal = true;


    davisHandle.configSet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP, caerBiasCoarseFineGenerate(coarseFineBias));

    // Set parsing intervall 
    davisHandle.configSet(CAER_HOST_CONFIG_PACKETS, CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_INTERVAL, container_interval);

    // Let's verify they really changed!
    uint32_t prBias   = davisHandle.configGet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP);
    uint32_t prsfBias = davisHandle.configGet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP);

    printf("New bias values --- PR-coarse: %d, PR-fine: %d, PRSF-coarse: %d, PRSF-fine: %d.\n",
    caerBiasCoarseFineParse(prBias).coarseValue, caerBiasCoarseFineParse(prBias).fineValue,
    caerBiasCoarseFineParse(prsfBias).coarseValue, caerBiasCoarseFineParse(prsfBias).fineValue);

    return davisHandle;
}


// Start getting some data from the device. We just loop in blocking mode,
// no notification needed regarding new events. The shutdown notification, for example if
// the device is disconnected, should be listened to.
libcaer::devices::davis DVSStream::startdatastream(libcaer::devices::davis davisHandle){

    // Configs about buffer
    davisHandle.configSet(CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BUFFER_SIZE, buffer_size);

    uint32_t BFSize   = davisHandle.configGet(CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BUFFER_SIZE);
    printf("Buffer size: %d", BFSize);

    // Start data stream
    davisHandle.dataStart(nullptr, nullptr, nullptr, &DVSStream::usbShutdownHandler, nullptr);

    // Let's turn on blocking data-get mode to avoid wasting resources.
    davisHandle.configSet(CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true);

return davisHandle; 
}


// Process a packet of events and send it using UDP over the socket
void DVSStream::sendpacket(libcaer::devices::davis davisHandle, bool include_timestamp){
    std::unique_ptr<libcaer::events::EventPacketContainer> packetContainer = nullptr;
    int numbytes;
    int current_event = 0; 

    uint16_t UDP_max_bytesize = 512;
    uint16_t max_events;

    uint16_t message[UDP_max_bytesize/2];
    bool sent; 

    if (include_timestamp){
            max_events = UDP_max_bytesize / 8; 
        }
        else{
            max_events = UDP_max_bytesize / 4;
    }

    do {
      packetContainer = davisHandle.dataGet();
    } while (packetContainer == nullptr);


    printf("\nGot event container with %d packets (allocated).\n", packetContainer->size());

    for (auto &packet : *packetContainer) {
        if (packet == nullptr) {
            printf("Packet is empty (not present).\n");
            continue; // Skip if nothing there.
        }

        printf("Packet of type %d -> %d events, %d capacity.\n", packet->getEventType(), packet->getEventNumber(), packet->getEventCapacity());


        if (packet->getEventType() == POLARITY_EVENT) {
            std::shared_ptr<const libcaer::events::PolarityEventPacket> polarity = std::static_pointer_cast<libcaer::events::PolarityEventPacket>(packet);

            for (const auto &evt : *polarity) {
                if (evt.isValid() == true){
                    AEDAT::PolarityEventTransfer polarity_event;
                    sent = false; 

                    polarity_event.timestamp = evt.getTimestamp64(*polarity);
                    polarity_event.x = evt.getX();
                    polarity_event.y = evt.getY();
                    polarity_event.polarity   = evt.getPolarity();
                    
                    // If specified write to file
                    if (strcmp (filename,"None") != 0){
                        fileOutput << "DVS " << polarity_event.timestamp << " " << polarity_event.x << " " << polarity_event.y << " " << polarity_event.polarity << std::endl;
                    }

                    // Encoding according to protocol
                    if (include_timestamp){
                        message[2*current_event] = htons(polarity_event.x & 0x7FFF);

                        uint32_t timestamp = evt.getTimestamp();
                        uint16_t timearray[2];
                        std::memcpy(timearray, &timestamp, sizeof(timestamp));
                        for(int i=0; i<2; i++){
                            message[2*current_event+2+i] = htons(timearray[i]);
                        }
                    }
                    else{
                        message[2*current_event] = htons(polarity_event.x | 0x8000);
                    }

                    if (polarity_event.polarity){
                        message[2*current_event+1] = htons(polarity_event.y | 0x8000);
                    }
                    else{
                        message[2*current_event+1] = htons(polarity_event.y & 0x7FFF);
                    }

                    if (include_timestamp){
                        current_event += 2;
                    }
                    else{
                        current_event += 1;
                    }
                }

                if (current_event == max_events){
                    if ((numbytes = sendto(DVSStream::sockfd,  &message, sizeof(message), 0, p->ai_addr, p->ai_addrlen)) == -1) {
                        perror("talker error: sendto");
                        exit(1);
                    }
                    
                    sent = true; 
                    current_event = 0;
                }
            }
        }
    }

    if (sent == false){
        uint16_t small_array[current_event*2]; 
        std::memcpy(small_array, &message, current_event*4);
        if ((numbytes = sendto(DVSStream::sockfd,  &small_array, sizeof(small_array), 0, p->ai_addr, p->ai_addrlen)) == -1) {
            perror("talker error: sendto");
            exit(1);
        }
    }
}


// Stops the datastream
int DVSStream::stopdatastream(libcaer::devices::davis davisHandle){
    davisHandle.dataStop();
    // Close automatically done by destructor.
    printf("Shutdown successful.\n");
    return (EXIT_FAILURE);
}


// Close the socket
void DVSStream::closesocket(){
    close(sockfd);
}
