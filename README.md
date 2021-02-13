# Camera Monitoring

I have created this project because I was disappointed with my IP camera application, which I have got from vendor, so I decided to do it by my own.
Luckily, my camera provides RTSP, which was quite easy to handle. Well it is my own way/vision how it should work. 
But my solution is not limited to RTSP, because I used gstreamer, so you can find a way to take frame from other camera. 
I have run it on Nvidia Jetson Nano, on my quite old PC with GPU (AMD Athlon(tm) II X3 445 with Radeon RX 550 Series).

# How it works and what it uses

- I have used gstreamer to take frame from camera.
- Next, the frame is analyzed to find the movements - it is very simple difference between some previous frame and current frame and calculate size of regions
- This analyze can trigger to run neural network from pjreddie (expected citation and info how I have provided it you can find in 3rdParties directories).
- I want false positive so even if net does not detect any I store and send frame.
- If net detects something then (again using gstreamer) I create video. I try to collect some more frames to smoothly start and finish video.
- Currently I made possibility to send frame (e.g. when something is detected) to slack using their HTTP api. To send by HTTP, I use curl and RapidJSON, as they expect part of request and answers in json format.
- Recently I have added also back communication from slack, that bot can analyze my commands, by aplication side.
- Frames and videos are stored in provided directory. Frames are stored in PNG format with libpng. Videos are stored with x264enc encoder.

# Config file

Application use config file in **key = value** structure. All kind of keys and some description of how to fill it, you can find **base.cfg**

# Building

I have based building on CMake. So mostly what you need is to make build directory and run cmake then make inside it. 
I do not use any tweak button/flags from CMake level and there is only one target.

# Future plan

My application is not finished and I mostly work on it in my spare time.
In the nearest future I hope to add: 
- more communication with slack
- sending video to slack


# License

This project, I am developing on MIT license. Please see license file.


