#include <libobsensor/ObSensor.hpp>

#include <iostream>

#include "utils_opencv.hpp"

// get input option
int getInputOption() {
    std::string selected;
    std::cin >> selected;
    if(selected == "q" || selected == "Q") {
        return -1;
    }
    return std::stoul(selected);
}

// Print stream profile information.
void printStreamProfile(std::shared_ptr<ob::StreamProfile> profile, uint32_t index) {
    // Get the video profile.
    auto videoProfile = profile->as<ob::VideoStreamProfile>();
    // Get the format.
    auto formatName = profile->getFormat();
    // Get the width.
    auto width = videoProfile->getWidth();
    // Get the height.
    auto height = videoProfile->getHeight();
    // Get the fps.
    auto fps = videoProfile->getFps();
    std::cout << index << "."
              << "format: " << formatName << ", "
              << "revolution: " << width << "*" << height << ", "
              << "fps: " << fps << std::endl;
}

// Print accel profile information.
void printAccelProfile(std::shared_ptr<ob::StreamProfile> profile, uint32_t index) {
    // Get the profile of accel.
    auto accProfile = profile->as<ob::AccelStreamProfile>();
    // Get the rate of accel.
    auto accRate = accProfile->getSampleRate();
    std::cout << index << "."
              << "acc rate: " << accRate << std::endl;
}

// Print gyro profile information.
void printGyroProfile(std::shared_ptr<ob::StreamProfile> profile, uint32_t index) {
    // Get the profile of gyro.
    auto gyroProfile = profile->as<ob::GyroStreamProfile>();
    // Get the rate of gyro.
    auto gyroRate = gyroProfile->getSampleRate();
    std::cout << index << "."
              << "gyro rate: " << gyroRate << std::endl;
}

// Enumerate stream profiles.
void enumerateStreamProfiles(std::shared_ptr<ob::Sensor> sensor) {
    // Get the list of stream profiles.
    auto streamProfileList = sensor->getStreamProfileList();
    // Get the sensor type.
    auto sensorType = sensor->getType();
    for(uint32_t index = 0; index < streamProfileList->getCount(); index++) {
        // Get the stream profile.
        auto profile = streamProfileList->getProfile(index);
        if(sensorType == OB_SENSOR_IR || sensorType == OB_SENSOR_COLOR || sensorType == OB_SENSOR_DEPTH || sensorType == OB_SENSOR_IR_LEFT
           || sensorType == OB_SENSOR_IR_RIGHT) {
            printStreamProfile(profile, index);
        }
        else if(sensorType == OB_SENSOR_ACCEL) {
            printAccelProfile(profile, index);
        }
        else if(sensorType == OB_SENSOR_GYRO) {
            printGyroProfile(profile, index);
        }
        else {
            break;
        }
    }
}

// Enumerate sensors.
void enumerateSensors(std::shared_ptr<ob::Device> device) {
    while(true) {
        std::cout << "Sensor list: " << std::endl;
        // Get the list of sensors.
        auto sensorList = device->getSensorList();
        for(uint32_t index = 0; index < sensorList->getCount(); index++) {
            // Get the sensor type.
            auto sensorType = sensorList->getSensorType(index);
            std::cout << " - " << index << "."
                      << "sensor type: " << sensorType << std::endl;
        }

        std::cout << "Select a sensor to enumerate its streams(input sensor index or \'q\' to enumerate device): " << std::endl;

        // Select a sensor.
        int sensorSelected = getInputOption();
        if(sensorSelected == -1) {
            break;
        }

        // Get sensor from sensorList.
        auto sensor = sensorList->getSensor(sensorSelected);
        enumerateStreamProfiles(sensor);
    }
}

int main(void) try {

    // Create a context.
    ob::Context context;

    while(true) {
        // Query the list of connected devices.
        auto deviceList = context.queryDeviceList();
        if(deviceList->getCount() < 1) {
            std::cout << "Not found device !" << std::endl;
            return -1;
        }

        std::cout << "enumerated devices: " << std::endl;

        std::shared_ptr<ob::Device> device = nullptr;
        std::shared_ptr<ob::DeviceInfo> deviceInfo = nullptr;
        for(uint32_t index = 0; index < deviceList->getCount(); index++) {
            // Get device from deviceList.
            device = deviceList->getDevice(index);
            // Get device information from device
            deviceInfo = device->getDeviceInfo();
            std::cout << " - " << index << ". name: " << deviceInfo->getName() << " pid: " << deviceInfo->getPid() << " SN: " << deviceInfo->getSerialNumber()
                      << std::endl;
        }

        std::cout << "enumerate sensors of device (input device index or \'q\' to exit program):" << std::endl;

        // select a device.
        int deviceSelected = getInputOption();
        if(deviceSelected == -1) {
            break;
        }

        //  Get the device.
        auto device = deviceList->getDevice(deviceSelected);
        enumerateSensors(device);
    }

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    exit(EXIT_FAILURE);
}
