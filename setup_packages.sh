mkdir -p ~/.spack
cat <<EOF > ~/.spack/packages.yaml
packages:
  autoconf:
    externals:
    - spec: autoconf
      prefix: /usr
      buildable: False
  automake:
    externals:
    - spec: automake
      prefix: /usr
      buildable: False
  libtool:
    externals:
    - spec: libtool
      prefix: /usr
      buildable: False
  m4:
    externals:
    - spec: m4
      prefix: /usr
      buildable: False
  bzip2:
    externals:
    - spec: bzip2
      prefix: /usr
      buildable: False
  zlib:
    externals:
    - spec: zlib
      prefix: /usr
      buildable: False
  cmake:
    externals:
    - spec: cmake
      prefix: /usr
      buildable: False
  mercury:
    variants: ~boostsys+ofi
  libfabric:
    variants: fabrics=tcp,rxm,sockets
EOF