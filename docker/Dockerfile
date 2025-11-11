# Download base image ubuntu 20.04
FROM ubuntu:24.04

# LABEL about the custom image
LABEL description="This is Docker Image for bbb cross build env with bb debian13 sysroot."

# Disable Prompt During Packages Installation
ARG DEBIAN_FRONTEND=noninteractive

RUN apt update

RUN apt install -y g++-14-arm-linux-gnueabihf wget xz-utils ninja-build git cmake pkgconf sudo && \
    rm -rf /var/lib/apt/lists/* && \
    apt clean

ENV CROSS_ENV_ROOT=/bbsysroot
ARG BBSYSROOT_URL=bbrootfs.tar.xz
ADD ${BBSYSROOT_URL} ${CROSS_ENV_ROOT}
RUN test -d ${CROSS_ENV_ROOT}/usr/lib 
ENV PKG_CONFIG_PATH=${CROSS_ENV_ROOT}/usr/local/libgpiod-1.6.5/lib/pkgconfig

RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
USER ubuntu

COPY build.sh /build.sh
CMD ["./build.sh"]

