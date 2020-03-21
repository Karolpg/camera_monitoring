#include <iostream>
#include <string>
#include <memory>

#include "VideoGrabber.h"
#include "Detector.h"
#include "Config.h"
#include "SlackCommunication.h"

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

    std::shared_ptr<SlackCommunication> slack = std::make_shared<SlackCommunication>(cfg.getValue("slackAddress"),
                                                                                     cfg.getValue("slackBearerId"));
    std::string reportingChannel = cfg.getValue("slackReportChannel");
    if (!slack->sendWelcomMessage(reportingChannel)) {
        std::cerr << "Slack is not ready and can't be used!\n";
        slack.reset();
    }

    std::shared_ptr<Detector> yoloDetector = std::shared_ptr<Detector>(
                new Detector(cfg.getValue("darknetCfgFilePath"),
                             cfg.getValue("darknetWeightsFilePath"),
                             cfg.getValue("darknetOutLabelsFilePath"),
                             cfg.getValue("validLabelsFilePath"),
                             cfg.getValue("probabilityThreshold", 0.1f)));

    VideoGrabber videoGrabber(cfg.getValue("cameraUrl"));
    videoGrabber.getFrameControler().setDetector(yoloDetector);
    videoGrabber.hangOnPlay();
    return 0;
}
