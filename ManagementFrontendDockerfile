ARG CODE_VERSION=develop
FROM clipper/lib_base:${CODE_VERSION}

COPY ./ /clipper

RUN cd /clipper/src/libs/spdlog \
    && git apply ../patches/make_spdlog_compile_linux.patch \
    && cd /clipper \
    && ./configure --cleanup-quiet \
    && ./configure --release \
    && cd release \
    && make management_frontend

ENTRYPOINT ["/clipper/release/src/management/management_frontend"]

# vim: set filetype=dockerfile:
