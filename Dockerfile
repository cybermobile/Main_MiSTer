FROM ubuntu:22.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    xz-utils \
    make \
    git \
    && rm -rf /var/lib/apt/lists/*

# Download and install ARM cross-compiler
ENV MISTER_GCC_VER=10.2-2020.11
ENV GCC_PACKAGE_NAME=gcc-arm-${MISTER_GCC_VER}-x86_64-arm-none-linux-gnueabihf

RUN wget --no-check-certificate -q https://developer.arm.com/-/media/Files/downloads/gnu-a/${MISTER_GCC_VER}/binrel/${GCC_PACKAGE_NAME}.tar.xz \
    && tar xf ${GCC_PACKAGE_NAME}.tar.xz -C /opt \
    && rm ${GCC_PACKAGE_NAME}.tar.xz

# Set up environment
ENV PATH="/opt/${GCC_PACKAGE_NAME}/bin:${PATH}"

# Set working directory
WORKDIR /src

# Default command: build the project
CMD ["make", "-j4"]

