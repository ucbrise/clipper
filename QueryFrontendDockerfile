ARG CODE_VERSION=develop
FROM clipper/lib_base:${CODE_VERSION}

# Build Clipper

COPY ./ /clipper

RUN cd /clipper/src/libs/spdlog \
    && git apply ../patches/make_spdlog_compile_linux.patch \
    && cd /clipper \
    && ./configure --cleanup-quiet \
    && ./configure --release \
    && cd release \
    && make query_frontend

ENTRYPOINT ["/clipper/release/src/frontends/query_frontend"]

# vim: set filetype=dockerfile:
