# Robot Remoter Control Web-Controller
C++ WebSocket Server and Robot Remote Control Controller to run on the Control System of the Beneficiation and Excavation systems for the ESA ESRIC Space Resource Challenge

This Robot Remote controller/web bridge was developed at the [Robotics Innovation Center](http://robotik.dfki-bremen.de/en/startpage.html) of the [German Research Center for Artificial Intelligence (DFKI)](http://www.dfki.de) in Bremen.

See https://github.com/esa-esric-bremen/.github/ for more details, including a system overview.

<img src="https://avatars.githubusercontent.com/u/277213809?s=400&u=25dd27e12b5732cb78306bf27b43139214900f58" style="width:200px;">

[![Pioneers of Sustainable Lunar Missions: Team BREMEN Wins the International Space Resources Challenge](https://img.youtube.com/vi/bCoAqg_29Lo/0.jpg)](https://www.youtube.com/watch?v=bCoAqg_29Lo)

## Motivation
This is code for the **winning** team B.R.E.M.E.N. of the [ESA ESRIC Space Resource Challenge](https://src.esa.int) as reference implementation for [Robot Remote Control](https://github.com/dfki-ric/robot_remote_control) and general information for the public.

### Installation
Either use the Dev-container for local devleopment or for deployment on a Server (or Notebook) install the requirements using apt:

```bash
$ sudo apt-get install -y --no-install-recommends \
    build-essential \
    git \
    cmake \
    nlohmann-json3-dev \
    pkg-config \
    libzmq3-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libreadline-dev \
    libncurses-dev \
    libopencv-dev \
    ca-certificates \
    gettext-base \
    fakeroot \
    libboost-all-dev \
    libyaml-cpp-dev
$ sudo mkdir -p /app/controller/thirdparty
$ sudo mkdir -p /app/controller/src/
$ sudo chown $USER:$GROUP -R /app/controller/
# clone or copy this repository to /app/controller/src/
$ cd /app/controller/thirdparty
$ git clone https://github.com/dfki-ric/robot_remote_control.git \
    && cd robot_remote_control \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make -j$(nproc) \
    && sudo make install \
    && sudo ldconfig
$ cd /app/controller/thirdparty
$ mkdir build \
    && cd build \
    && cmake .. \
    && make  -j$(nproc) \
    && mv ./controller_web ..
```

You can start the web server and robot remote controller using the command

```bash
$ /app/controller/controller_web
```

### Dependencies
Robot-Remote-Control depends on libzmq and protobuf.

### Dev Container
An easy alternative is to use the devcontroller:

Simply open this folder in VS Code and build&run the devcontainer.

## License
BSD-3

## Maintainer / Authors / Contributers
Andreas Bresser, Andreas.Bresser@dfki.de

Copyright 2025-2026, DFKI GmbH / Robotics Innovation Center
