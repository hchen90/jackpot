FROM ubuntu:latest
RUN apt-get update && apt-get install -y libev-dev libssl-dev gcc g++ clang cmake
COPY . /usr/src/jackpot
WORKDIR /usr/src/jackpot
RUN cmake . && make && make install
