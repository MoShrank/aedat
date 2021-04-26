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
        uint32_t buffer_size;

        DVSData(uint32_t interval, uint32_t bfsize);

        static void globalShutdownSignalHandler(int signal);
        static void usbShutdownHandler(void *ptr); 
        libcaer::devices::davis connect2camera(int ID);
        libcaer::devices::davis startdatastream(libcaer::devices::davis davisHandle);
        std::vector<torch::Tensor> update(libcaer::devices::davis davisHandle);
        int stopdatastream(libcaer::devices::davis davisHandle);
};

#endif