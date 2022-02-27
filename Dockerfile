FROM alpine:latest AS build
RUN apk --update --no-cache add libev-dev openssl-dev gcc g++ clang cmake make
COPY . /usr/src/jackpot
WORKDIR /usr/src/jackpot
RUN cmake . && make && make install
FROM alpine:latest
RUN apk --update --no-cache add libstdc++ libev libcrypto1.1 libssl1.1
COPY --from=build /usr/share/man/man1/jackpot.1 /usr/share/man/man1/jackpot.1
COPY --from=build /usr/share/man/man5/jackpot.conf.5 /usr/share/man/man5/jackpot.conf.5
COPY --from=build /usr/share/doc/jackpot/rootfs.sh /usr/share/doc/jackpot/rootfs.sh
COPY --from=build /usr/bin/jackpot /usr/bin/jackpot
WORKDIR /
CMD ["jackpot"]
