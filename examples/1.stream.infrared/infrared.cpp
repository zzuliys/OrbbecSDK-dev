#include <libobsensor/ObSensor.hpp>

#include "utils_opencv.hpp"

std::map<OBSensorType, ob_stream_type> sensorStreamMap = {
    {OB_SENSOR_IR, OB_STREAM_IR},
    {OB_SENSOR_IR_LEFT, OB_STREAM_IR_LEFT},
    {OB_SENSOR_IR_RIGHT, OB_STREAM_IR_RIGHT}
};

int main() try {
    // Create a pipeline with default device.
    ob::Pipeline pipe;
    // Get the device from pipeline.
    std::shared_ptr<ob::Device> device = pipe.getDevice();
    // Get the sensor list from device.
    std::shared_ptr<ob::SensorList> sensorList = device->getSensorList();
    // Create a config for pipeline.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

    for(uint32_t index = 0; index < sensorList->getCount(); index++) {
        // Query all supported infrared sensor typ.e and enable the infrared stream.
        // For dual infrared device, enable the left and right infrared streams.
        // For single infrared device, enable the infrared stream.
        OBSensorType sensorType = sensorList->getSensorType(index);
        if(sensorType == OB_SENSOR_IR || sensorType == OB_SENSOR_IR_LEFT || sensorType == OB_SENSOR_IR_RIGHT) {
            // Enable the stream with specified requirements.
            config->enableVideoStream(sensorStreamMap[sensorType], OB_WIDTH_ANY, OB_HEIGHT_ANY, 30, OB_FORMAT_ANY);
        }
    }

    pipe.start(config);

    // Create a window for rendering and set the resolution of the window
    Window app("InfraredViewer", 1280, 720, RENDER_ONE_ROW);
    while(app) {
        // Wait for up to 100ms for a frameset in blocking mode.
        auto frameSet = pipe.waitForFrameset(100);
        if(frameSet == nullptr) {
            continue;
        }

        std::vector<std::shared_ptr<const ob::Frame>> frames;
        for(uint32_t index = 0; index < frameSet->getCount(); index++) {
            auto frame = frameSet->getFrameByIndex(index);
            if(frame != nullptr){
                frames.push_back(frame);
            }
        }

        // Render a set of frame in the window, only the infrared frame is rendered here.
        // If the open stream type is not OB_SENSOR_IR, use the getFrame interface to get the frame.
        app.renderFrameData(frames);
    }

    // Stop the pipeline, no frame data will be generated
    pipe.stop();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    exit(EXIT_FAILURE);
}
