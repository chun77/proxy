# Start from an Ubuntu image with a pre-installed GCC compiler
FROM ubuntu:20.04

# Prevent any interactive dialogue during the installation process
ARG DEBIAN_FRONTEND=noninteractive

# Install C++ build environment and necessary packages, including Boost libraries,
# OpenSSL development files, and build-essential which includes 'make'
RUN apt-get update && apt-get install -y \
    g++ \
    libboost-all-dev \
    build-essential \
    libssl-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# If you're on an ARM machine, download the correct CMake binary for ARM
# Note: The following command downloads CMake for x86_64, update if needed for ARM
RUN wget https://github.com/Kitware/CMake/releases/download/v3.29.0-rc2/cmake-3.29.0-rc2-linux-x86_64.sh \
 && chmod +x cmake-3.29.0-rc2-linux-x86_64.sh \
 && ./cmake-3.29.0-rc2-linux-x86_64.sh --skip-license --prefix=/usr/local \
 && rm cmake-3.29.0-rc2-linux-x86_64.sh

# Set the working directory in the container
WORKDIR /app

# Copy the project files into the working directory inside the container
COPY . /app

# Build the project using the newly installed CMake and make
# Now CMake should find OpenSSL since libssl-dev is installed
RUN cmake . \
    && make

# When the container starts, run the compiled proxy server
# Uncomment the line below if you wish to run the proxy by default
# CMD ["./my_proxy"]

# Expose the port the server is running on
EXPOSE 12345

# Use a wrapper script as the entry point
COPY run.sh /app/run.sh
RUN chmod +x /app/run.sh
ENTRYPOINT ["/app/run.sh"]
