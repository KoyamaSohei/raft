FROM spack/ubuntu-bionic

WORKDIR /app

RUN git clone https://xgitlab.cels.anl.gov/sds/sds-repo.git src/sds-repo
RUN spack repo add src/sds-repo
RUN spack install mochi-thallium lmdb jsoncpp pkg-config
COPY . .
RUN spack load -r mochi-thallium lmdb jsoncpp pkg-config && \
    make
