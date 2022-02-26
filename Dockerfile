FROM ubuntu:latest
RUN apt-get update && apt-get install -y libev-dev libssl-dev gcc g++ clang cmake
COPY . /usr/src/jackpot
WORKDIR /usr/src/jackpot
RUN cmake . && make && make install && make clean
RUN apt-get remove -y gcc g++ clang cmake && apt-get -y clean
WORKDIR /
RUN rm -rf /usr/src/jackpot
