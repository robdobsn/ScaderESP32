# The ESP-IDF version is normally set by ESP_IDF_VERSION in systypes/Common/features.cmake (and can be
# overridden for a SysType in systypes/<SysType>/features.cmake) and is supplied as a build-arg when the
# image is built by "raft build", which overrides the default below.
# The default (set by "raft new" to match features.cmake) lets "docker build" work standalone and keeps
# Docker's InvalidDefaultArgInFrom lint quiet.
ARG ESP_IDF_VERSION=v6.1
FROM espressif/idf:${ESP_IDF_VERSION}
WORKDIR /project
# Install dependencies required for Node.js install
RUN apt-get update && apt-get install -y curl software-properties-common && \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get update && apt-get install -y nodejs g++
# Verify the installation of the specific Node.js version
RUN node -v && npm -v
# Configure Git to recognize /project as a safe directory
RUN git config --global --add safe.directory /project
