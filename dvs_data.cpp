#define LIBCAER_FRAMECPP_OPENCV_INSTALLED 0

#include <libcaercpp/devices/davis.hpp>
#include <libcaer/devices/davis.h>
#include "dvs_data.hpp"
#include "convert.hpp"

#include <atomic>
#include <csignal>

//=================================
// include guard
#ifndef PYBIND11_H
#define PYBIND11_H
#include <pybind11/pybind11.h>
#endif

#ifndef STL_H
#define STL_H
#include <pybind11/stl.h>
#endif

namespace py = pybind11;

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

// Constructor 
DVSData::DVSData(uint32_t interval){
    container_interval = interval; 
}

// Open a DAVIS, given ID, and don't care about USB bus or SN restrictions.
libcaer::devices::davis DVSData::connect2camera(int ID, void (&globalShutdownSignalHandler)(int signal)){

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

        shutdownAction.sa_handler = &globalShutdownSignalHandler;
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
    uint16_t fine;
    struct caer_bias_coarsefine coarseFineBias;

    coarseFineBias.coarseValue        = 2;
    coarseFineBias.fineValue          = 116;
    coarseFineBias.enabled            = true;
    coarseFineBias.sexN               = false;
    coarseFineBias.typeNormal         = true;
    coarseFineBias.currentLevelNormal = true;


    davisHandle.configSet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP, caerBiasCoarseFineGenerate(coarseFineBias));	// caerBiasCoarseFineGenerate(coarseFineBias)

    coarseFineBias.coarseValue        = 1;
    coarseFineBias.fineValue          = 33;
    coarseFineBias.enabled            = true;
    coarseFineBias.sexN               = false;
    coarseFineBias.typeNormal         = true;
    coarseFineBias.currentLevelNormal = true;


    davisHandle.configSet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP, caerBiasCoarseFineGenerate(coarseFineBias));	//caerBiasCoarseFineGenerate

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

// Now let's get start getting some data from the device. We just loop in blocking mode,
// no notification needed regarding new events. The shutdown notification, for example if
// the device is disconnected, should be listened to.
libcaer::devices::davis DVSData::startdatastream(libcaer::devices::davis davisHandle, void (&usbShutdownHandler)(void *ptr)){

    davisHandle.dataStart(nullptr, nullptr, nullptr, &usbShutdownHandler, nullptr);

    // Let's turn on blocking data-get mode to avoid wasting resources.
    davisHandle.configSet(CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true);

return davisHandle; 
}


// Process an event and convert any given PolarityEvents to sparse tensor structure
std::vector<torch::Tensor> DVSData::update(libcaer::devices::davis davisHandle){
    std::vector<torch::Tensor> polarity_tensors;
    std::unique_ptr<libcaer::events::EventPacketContainer> packetContainer = nullptr;

    do {
      packetContainer = davisHandle.dataGet();
    } while (packetContainer == nullptr);


    printf("\nGot event container with %d packets (allocated).\n", packetContainer->size());

    for (auto &packet : *packetContainer) {
        if (packet == nullptr) {
            printf("Packet is empty (not present).\n");
            continue; // Skip if nothing there.
        }

        printf("Packet of type %d -> %d events, %d capacity.\n", packet->getEventType(), packet->getEventNumber(),
            packet->getEventCapacity());


        if (packet->getEventType() == POLARITY_EVENT) {
            std::shared_ptr<const libcaer::events::PolarityEventPacket> polarity = std::static_pointer_cast<libcaer::events::PolarityEventPacket>(packet);
            // Print out timestamps and addresses.

            std::vector<AEDAT::PolarityEvent> polarity_events;

            printf("Lowest Timestamp: %ld\n", (packetContainer->getLowestEventTimestamp()));
            printf("Highest Timestamp: %ld\n", (packetContainer->getHighestEventTimestamp()));
            printf("Time span of polarity packet: %ld\n", (packetContainer->getHighestEventTimestamp() - packetContainer->getLowestEventTimestamp()));

            for (const auto &evt : *polarity) {
                if (evt.isValid() == true){
                    AEDAT::PolarityEvent polarity_event;

                    polarity_event.timestamp = evt.getTimestamp64(*polarity);
                    polarity_event.x = evt.getX();
                    polarity_event.y = evt.getY();
                    polarity_event.polarity   = evt.getPolarity();

                    polarity_events.push_back(polarity_event);

                    //printf("Time: %d\n", polarity_event.timestamp);
                    //printf("x: %d\n", polarity_event.x);
                    //printf("y: %d\n", polarity_event.y);
                    //printf("polarity: %d\n", polarity_event.polarity);
                }
            }

            printf("Polarity event processed! Convert to tensor...\n");
            auto event_tensors = convert_polarity_events(polarity_events);
            polarity_tensors.push_back(event_tensors);
            std::cout << event_tensors.sizes() << std::endl;

        }
    }
    return polarity_tensors;
}

// Stops the datastream
int DVSData::stopdatastream(libcaer::devices::davis davisHandle){
    davisHandle.dataStop();
    // Close automatically done by destructor.
    printf("Shutdown successful.\n");
    return (EXIT_FAILURE);
}


PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  py::class_<DVSData>(m, "DVSData")
      .def("connect2camera", &DVSData::connect2camera)
      .def("startdatastream", &DVSData::startdatastream)
      .def("update", &DVSData::update)
      .def("stopdatastream", &DVSData::stopdatastream);

  m.def("globalShutdownSignalHandler", globalShutdownSignalHandler,
    py::arg("signal"));

  m.def("usbShutdownHandler", usbShutdownHandler,
    py::arg("ptr"));
}



