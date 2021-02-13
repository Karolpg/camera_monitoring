//
// The MIT License (MIT)
//
// Copyright 2020 Karolpg
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to #use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR #COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

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
                new Detector(cfg));

    VideoGrabber videoGrabber(cfg);
    videoGrabber.getFrameController().setDetector(yoloDetector);

    SlackSubscriber slackSub(cfg);
    slackSub.subscribe(videoGrabber.getFrameController());

    videoGrabber.hangOnPlay();
    return 0;
}
