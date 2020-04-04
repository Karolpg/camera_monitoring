#include <iostream>
#include <string>
#include <memory>

#include "VideoGrabber.h"
#include "Detector.h"
#include "Config.h"
#include "SlackSubscriber.h"

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
                             cfg.getValue("validLabelsFilePath"),
                             cfg.getValue("probabilityThreshold", 0.1f)));

    VideoGrabber videoGrabber(cfg.getValue("cameraUrl"));
    videoGrabber.getFrameControler().setDetector(yoloDetector);

    SlackSubscriber slackSub(cfg);
    slackSub.subscribe(videoGrabber.getFrameControler());

    videoGrabber.hangOnPlay();

    return 0;
}
