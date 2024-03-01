# Start from an Ubuntu image with a pre-installed GCC compiler
FROM ubuntu:20.04

# Prevent any interactive dialogue during the installation process
ARG DEBIAN_FRONTEND=noninteractive

# Install C++ build environment and necessary packages, including Boost libraries
# and build-essential which includes 'make'
RUN apt-get update && apt-get install -y \
    g++ \
    libboost-all-dev \
    build-essential \
    wget \
    && rm -rf /var/lib/apt/lists/*

# If you're on an ARM machine, download the correct CMake binary for ARM
RUN wget https://github.com/Kitware/CMake/releases/download/v3.27.0/cmake-3.27.0-Linux-aarch64.sh \
 && chmod +x cmake-3.27.0-Linux-aarch64.sh \
 && ./cmake-3.27.0-Linux-aarch64.sh --skip-license --prefix=/usr/local \
 && rm cmake-3.27.0-Linux-aarch64.sh

# Set the working directory in the container
WORKDIR /app

# Copy the project files into the working directory inside the container
COPY . /app

# Build the project using the newly installed CMake and make
RUN cmake . \
    && make


## When the container starts, run the compiled proxy server
#CMD ["./my_proxy"]

# Expose the port the server is running on
EXPOSE 12345

# Use a wrapper script as the entry point
COPY run.sh /app/run.sh
RUN chmod +x /app/run.sh
ENTRYPOINT ["/app/run.sh"]
