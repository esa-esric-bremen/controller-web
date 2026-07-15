FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
  --mount=type=cache,target=/var/lib/apt,sharing=locked \
  apt-get update -qq \
    && apt-get install -y --no-install-recommends \
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

RUN mkdir -p /app/controller/thirdparty
WORKDIR /app/controller/thirdparty
RUN git clone https://github.com/dfki-ric/robot_remote_control.git \
    && cd robot_remote_control \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make  -j$(nproc) \
    && make install \
    && ldconfig

WORKDIR /app/controller/
COPY ./src /app/controller/src
COPY CMakeLists.txt /app/controller/CMakeLists.txt

RUN mkdir build \
    && cd build \
    && cmake .. \
    && make  -j$(nproc) \
    && mv ./controller_web ..

CMD ["/app/controller/controller_web"]
