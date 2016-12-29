FROM frolvlad/alpine-gcc

COPY ./ /clipper

RUN apk add --no-cache git bash make boost-dev cmake libev-dev hiredis-dev zeromq-dev \
    && cd /clipper \
    && ./configure --cleanup \
    && ./configure --release \
    && cd release \
    && make query_frontend

ENTRYPOINT ["/clipper/release/src/frontends/query_frontend"]

