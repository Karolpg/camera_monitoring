#include <iostream>
#include <string>
#include <memory>

#include "VideoGrabber.h"
#include "Detector.h"
#include "Config.h"

int main(int argc, char *argv[])
{
    std::string configFilePath;
    if (argc < 2) {
        configFilePath = "../../data.cfg";
    }
    else {
        configFilePath = argv[1];
    }
    Config cfg;
    cfg.insertFromFile(configFilePath);

    std::shared_ptr<Detector> yoloDetector = std::shared_ptr<Detector>(
                new Detector(cfg.getValue("darknetCfgFilePath"),
                             cfg.getValue("darknetWeightsFilePath"),
                             cfg.getValue("darknetOutLabelsFilePath"),
                             0.1f));

    VideoGrabber videoGrabber(cfg.getValue("cameraUrl"));
    videoGrabber.getFrameControler().setDetector(yoloDetector);
    videoGrabber.hangOnPlay();
    return 0;
}
