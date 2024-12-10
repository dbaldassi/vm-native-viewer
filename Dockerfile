FROM debian:bookworm

RUN apt-get update && apt-get install -y build-essential cmake git clang clang-15 libc++-15-dev libc++abi-15-dev libx11-dev pulseaudio kmod iproute2

WORKDIR /usr/bin

RUN rm clang clang++ && ln -s clang-15 clang && ln -s clang++-15 clang++

WORKDIR /usr/include/c++

RUN ln -s ../../lib/llvm-15/include/c++/v1 v1

WORKDIR /root

RUN git clone  https://github.com/dbaldassi/vm-native-viewer.git

ADD lib/libwebrtc /opt/libwebrtc

WORKDIR vm-native-viewer

RUN git submodule update --init

RUN mkdir build

WORKDIR build

ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

RUN cmake .. -DFAKE_RENDERER=ON -Dlibwebrtc_DIR=/opt/libwebrtc/cmake && make -j4

ADD run.sh .

RUN chmod +x run.sh

ENTRYPOINT [ "./run.sh" ]
