#pragma once

#define LIBCAER_FRAMECPP_OPENCV_INSTALLED 0

#include <libcaercpp/devices/davis.hpp>
#include <torch/script.h>
#include <atomic>
#include <csignal>

#ifndef DVSData_H
#define DVSData_H

using namespace std;


class DVSData{
    public:
        // Parameters
        uint32_t container_interval;

        DVSData(uint32_t interval);

        //void globalShutdownSignalHandler(int signal);
        //void usbShutdownHandler(void *ptr); 
        libcaer::devices::davis connect2camera(int ID, void (&globalShutdownSignalHandler)(int signal));
        libcaer::devices::davis startdatastream(libcaer::devices::davis davisHandle, void (&usbShutdownHandler)(void *ptr));
        std::vector<torch::Tensor> update(libcaer::devices::davis davisHandle);
        int stopdatastream(libcaer::devices::davis davisHandle);
};

#endif