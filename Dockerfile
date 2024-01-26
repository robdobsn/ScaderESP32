FROM espressif/idf:v5.1.2
WORKDIR /project
ENV NODE_VERSION 20.11.0
RUN apt-get update && apt-get install -y g++ nodejs npm
