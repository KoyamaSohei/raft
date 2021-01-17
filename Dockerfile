# Build stage with Spack pre-installed and ready to be used
FROM spack/ubuntu-bionic:latest as builder

# Install OS packages needed to build the software
RUN apt-get -yqq update && apt-get -yqq upgrade \
 && apt-get -yqq install autoconf automake libtool m4 bzip2 zlib1g-dev cmake pkg-config rdma-core git \
 && rm -rf /var/lib/apt/lists/*

# clone sds-repo
RUN git clone https://xgitlab.cels.anl.gov/sds/sds-repo /opt/sds-repo

# What we want to install and how we want to install it
# is specified in a manifest file (spack.yaml)
RUN mkdir /opt/spack-environment \
&&  (echo "spack:" \
&&   echo "  specs:" \
&&   echo "  - mochi-thallium" \
&&   echo "  - lmdb" \
&&   echo "  - jsoncpp" \
&&   echo "  packages:" \
&&   echo "    autoconf:" \
&&   echo "      externals:" \
&&   echo "      - spec: autoconf" \
&&   echo "        prefix: /usr" \
&&   echo "        buildable: false" \
&&   echo "    automake:" \
&&   echo "      externals:" \
&&   echo "      - spec: automake" \
&&   echo "        prefix: /usr" \
&&   echo "        buildable: false" \
&&   echo "    libtool:" \
&&   echo "      externals:" \
&&   echo "      - spec: libtool" \
&&   echo "        prefix: /usr" \
&&   echo "        buildable: false" \
&&   echo "    m4:" \
&&   echo "      externals:" \
&&   echo "      - spec: m4" \
&&   echo "        prefix: /usr" \
&&   echo "        buildable: false" \
&&   echo "    bzip2:" \
&&   echo "      externals:" \
&&   echo "      - spec: bzip2" \
&&   echo "        prefix: /usr" \
&&   echo "        buildable: false" \
&&   echo "    zlib:" \
&&   echo "      externals:" \
&&   echo "      - spec: zlib" \
&&   echo "        prefix: /usr" \
&&   echo "        buildable: false" \
&&   echo "    cmake:" \
&&   echo "      externals:" \
&&   echo "      - spec: cmake" \
&&   echo "        prefix: /usr" \
&&   echo "        buildable: false" \
&&   echo "    pkg-config:" \
&&   echo "      externals:" \
&&   echo "      - spec: pkg-config" \
&&   echo "        prefix: /usr" \
&&   echo "        buildable: false" \
&&   echo "    mercury:" \
&&   echo "      variants: ~boostsys+ofi+bmi" \
&&   echo "    libfabric:" \
&&   echo "      variants: fabrics=tcp,rxm,sockets" \
&&   echo "    rdma-core:" \
&&   echo "      externals:" \
&&   echo "      - spec: rdma-core" \
&&   echo "        prefix: /usr" \
&&   echo "      buildable: false" \
&&   echo "  repos:" \
&&   echo "  - /opt/sds-repo" \
&&   echo "  concretization: together" \
&&   echo "  config:" \
&&   echo "    install_tree: /opt/software" \
&&   echo "  view: /opt/view") > /opt/spack-environment/spack.yaml

# Install the software, remove unnecessary deps
RUN cd /opt/spack-environment && spack env activate . && spack install --fail-fast && spack gc -y

# Strip all the binaries
RUN find -L /opt/view/* -type f -exec readlink -f '{}' \; | \
    xargs file -i | \
    grep 'charset=binary' | \
    grep 'x-executable\|x-archive\|x-sharedlib' | \
    awk -F: '{print $1}' | xargs strip -s

# Modifications to the environment that are necessary to run
RUN cd /opt/spack-environment && \
    spack env activate --sh -d . >> /etc/profile.d/z10_spack_environment.sh


# Bare OS image to run the installed executables
FROM ubuntu:18.04

COPY --from=builder /opt/spack-environment /opt/spack-environment
COPY --from=builder /opt/software /opt/software
COPY --from=builder /opt/view /opt/view
COPY --from=builder /etc/profile.d/z10_spack_environment.sh /etc/profile.d/z10_spack_environment.sh
COPY --from=builder /opt/sds-repo /opt/sds-repo

RUN apt-get -yqq update && apt-get -yqq upgrade \
 && apt-get -yqq install autoconf automake libtool m4 bzip2 zlib1g-dev cmake pkg-config rdma-core git \
 && rm -rf /var/lib/apt/lists/*


ENTRYPOINT ["/bin/bash", "--rcfile", "/etc/profile", "-l"]
