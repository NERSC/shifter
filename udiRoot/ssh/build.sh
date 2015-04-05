#!/bin/bash
set -e

INST_PREFIX=${INST_PREFIX:-/opt/udiImage}
SPRT_PREFIX=$( mktemp -d )
PREFIX=$( mktemp -d )
MUSL_VERSION=1.1.8
LIBRESSL_VERSION=2.1.6
ZLIB_VERSION=1.2.8
OPENSSH_VERSION=6.8p1

origdir=$( pwd )
mkdir -p build
cd build
builddir=$( pwd )

if [[ -z "$INST_PREFIX" || "$INST_PREFIX" == "/" ]]; then
    echo "Invalid installation target: $INST_PREFIX" 1>&2
    exit 1
fi

if [[ ! -e "musl-${MUSL_VERSION}.tar.gz" ]]; then
    curl -o "musl-${MUSL_VERSION}.tar.gz" "http://www.musl-libc.org/releases/musl-${MUSL_VERSION}.tar.gz"
fi
if [[ ! -e "libressl-${LIBRESSL_VERSION}.tar.gz" ]]; then
    curl -o "libressl-${LIBRESSL_VERSION}.tar.gz" "http://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-${LIBRESSL_VERSION}.tar.gz"
fi
if [[ ! -e "zlib-${ZLIB_VERSION}.tar.gz" ]]; then
    curl -o "zlib-${ZLIB_VERSION}.tar.gz" "http://zlib.net/zlib-${ZLIB_VERSION}.tar.gz"
fi
if [[ ! -e "openssh-${OPENSSH_VERSION}.tar.gz" ]]; then
    curl -o "openssh-${OPENSSH_VERSION}.tar.gz" "http://mirrors.sonic.net/pub/OpenBSD/OpenSSH/portable/openssh-${OPENSSH_VERSION}.tar.gz"
fi

mkdir -p musl
tar xf "musl-${MUSL_VERSION}.tar.gz" -C musl --strip-components=1
cd musl
./configure "--prefix=${SPRT_PREFIX}" --enable-static --disable-shared
make
make install
cd "${builddir}"

dirs="linux asm asm-generic"
for dir in $dirs; do
    if [[ -L "/usr/include/$dir" ]]; then
        # SLES has symlinks for asm
        realpath=$(readlink -f "/usr/include/$dir")
        cp -rp "$realpath" "${SPRT_PREFIX}/include/"
    fi
    cp -rp "/usr/include/$dir" "${SPRT_PREFIX}/include/"
done

mkdir -p libressl
tar xf "libressl-${LIBRESSL_VERSION}.tar.gz" -C libressl --strip-components=1
cd libressl
CC="${SPRT_PREFIX}/bin/musl-gcc" ./configure "--prefix=${SPRT_PREFIX}" --enable-static --disable-shared
make
make install

cd "${builddir}"
mkdir -p zlib
tar xf "zlib-${ZLIB_VERSION}.tar.gz" -C zlib --strip-components=1
cd zlib
CC="${SPRT_PREFIX}/bin/musl-gcc" ./configure "--prefix=${SPRT_PREFIX}"
make
make install

cd "${builddir}"
mkdir -p openssh
tar xf "openssh-${OPENSSH_VERSION}.tar.gz" -C openssh --strip-components=1
cd openssh
CC="${SPRT_PREFIX}/bin/musl-gcc" ./configure --without-pam "--with-ssl-dir=${SPRT_PREFIX}" --without-ssh1 --enable-static --disable-shared "--with-zlib=${SPRT_PREFIX}" "--prefix=${INST_PREFIX}"
make
make install "DESTDIR=${PREFIX}"
cd "${builddir}"

cat <<EOF > "${PREFIX}${INST_PREFIX}/etc/sshd_config"
Port 204
StrictModes yes
PermitRootLogin no
AuthorizedKeysFile ${INST_PREFIX}/etc/user_auth_keys
IgnoreUserKnownHosts yes
PasswordAuthentication no
ChallengeResponseAuthentication no
X11Forwarding yes
PermitUserEnvironment no
UseDNS no
Subsystem sftp ${INST_PREFIX}/libexec/sftp-server
AcceptEnv PBS_HOSTFILE
AcceptEnv SLURM_JOB_NODELIST
AcceptEnv SLURM_NODELIST
AcceptEnv BASIL_RESERVATION_ID
## The following is typically a bad practice -- but is ok here since all our security is
## to protect the system from the container not the other way around.  Allowing all variables
## through should be safe within the clustered environment.
AcceptEnv *
AllowUsers ToBeReplaced
EOF
cat <<EOF > ${PREFIX}${INST_PREFIX}/etc/ssh_config
Host *
  StrictHostKeyChecking no
  Port 204
# IdentityFile ...
EOF

cd "${PREFIX}"
tar cf "${origdir}/udiRoot_sshd.tar" .
cd "${origdir}"
rm -r "${PREFIX}"
rm -r "${SPRT_PREFIX}"
