FROM ubuntu:latest AS builder
RUN apt-get update && apt-get -y install build-essential intltool libtool m4 automake
RUN apt-get -y install libjack-dev
RUN apt-get -y install libglib2.0-dev
RUN apt-get -y install libgtk2.0-dev
WORKDIR /src/alsaplayer
COPY . .
RUN ./autogen.sh
RUN ./configure
RUN make install
