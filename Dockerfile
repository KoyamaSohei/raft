FROM spack/ubuntu-bionic:latest

RUN apt-get -yqq update && \
    apt-get -yqq upgrade && \
    apt-get -yqq install autoconf automake libtool m4 bzip2 zlib1g-dev cmake pkg-config rdma-core git && \
    rm -rf /var/lib/apt/lists/*

RUN git clone https://xgitlab.cels.anl.gov/sds/sds-repo.git /opt/sds-repo

COPY spack.yaml /opt/spack-environment/spack.yaml

RUN spack env activate /opt/spack-environment && \
    spack install

COPY . .
RUN spack env activate /opt/spack-environment && \
    make

ENV SELF '127.0.0.1:30000'
ENV NODES '127.0.0.1:30000,127.0.0.1:30001,127.0.0.1:30002'

CMD spack env activate /opt/spack-environment && \
    ./raft.out ${SELF} ${NODES}