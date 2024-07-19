#!/bin/sh


echo "-- start make depend:"

SHDIR=$(dirname `readlink -f $0`)

echo "make_depend.sh execute dir:" $SHDIR

OPENSSL_DIR=./openssl

ROCKSDB_DIR=./rocksdb

PROTOBUF_DIR=./protobuf

BOOST_DIR=./boost

LIBFMT_DIR=./libfmt
SPDLOG_DIR=./spdlog

EVMONE_DIR=./evmone
WASMTIME_DIR=./wasmtime-cpp

SILKPRE_DIR=./silkpre

CRYPTOPP_DIR=./cryptopp

GMP_DIR=./gmp

COMPILE_NUM=`cat /proc/cpuinfo| grep  "processor" | wc -l`;

# openssl
cd $SHDIR
if [ -d ${OPENSSL_DIR} ];
then 
    echo "openssl compile";
else
    tar -xvf ./3rd/openssl-3.0.5.tar.gz;
    mv openssl-3.0.5 openssl;
    cd ${OPENSSL_DIR} && ./Configure && make -j$COMPILE_NUM && make install;
fi;

# rocksdb
cd $SHDIR

if [ -d ${ROCKSDB_DIR} ]; 
then 
    echo "rocksdb compile";
else
    unzip ./3rd/rocksdb-8.3.2.zip -d ./;
    mv rocksdb-8.3.2 rocksdb;
    cd ${ROCKSDB_DIR} && make static_lib USE_RTTI=1 -j$COMPILE_NUM;
fi;

# protobuf
cd $SHDIR
if [ -d ${PROTOBUF_DIR} ]; 
then 
    echo "protobuf compile";
else
    unzip ./3rd/protobuf-cpp-3.21.9.zip -d ./;
    mv protobuf-3.21.9 protobuf;
    cd ${PROTOBUF_DIR} && ./autogen.sh && ./configure && make -j$COMPILE_NUM;
fi;


# boost
cd $SHDIR
if [ -d ${BOOST_DIR} ];
then 
    echo "boosk compile";
else
    wget https://boostorg.jfrog.io/artifactory/main/release/1.75.0/source/boost_1_75_0.tar.gz;
    mv boost_1_75_0.tar.gz ./3rd/;
    tar -xvf ./3rd/boost_1_75_0.tar.gz;
    mv boost_1_75_0 boost;
    cd ${BOOST_DIR} && ./bootstrap.sh && ./b2 install;
fi;

# libfmt
cd $SHDIR
if [ -d ${LIBFMT_DIR} ]; \
then \
    echo "libfmt compile";\
else\
    tar -xvf ./3rd/libfmt-7.1.3.tar.gz ;\
    mv fmt-7.1.3 libfmt;\
    cd ${LIBFMT_DIR} && cmake . && make -j$COMPILE_NUM;\
fi;\

# spdlog
cd $SHDIR
if [ -d ${SPDLOG_DIR} ]; \
then \
    echo "spdlog compile";\
else\
    tar -xvf ./3rd/spdlog-1.8.2.tar.gz ;\
    mv spdlog-1.8.2 spdlog;\
    cd ${SPDLOG_DIR} && fmt_DIR=../libfmt cmake -DSPDLOG_FMT_EXTERNAL=yes . && make -j$COMPILE_NUM;\
fi;\

# gmp
cd $SHDIR
if [ -d ${GMP_DIR} ]; \
then \
    echo "gmp compile";\
else\
    tar -xvf ./3rd/gmp-6.1.0.tar.xz ;\
    mv gmp-6.1.0 gmp;\
    cd ${GMP_DIR} && ./configure -prefix=/usr/local/gmp && make -j$COMPILE_NUM && make install;\
fi;\

#wasmtime

cd $SHDIR
if [ -d ${WASMTIME_DIR} ]; \
then \
    echo "wasmtime compile";\
else\
    tar -xvf ./3rd/wasmtime-cpp.tar.xz ;\
fi;\

# silkpre
cd $SHDIR
if [ -d ${SILKPRE_DIR} ]; \
then \
    echo "silkpre compile";\
else\
    tar -xvf ./3rd/silkpre.tar.gz ;\
    cp -f ./utils/silkpre/CMakeLists.txt ./silkpre/CMakeLists.txt
    cp -f ./utils/silkpre/lib/CMakeLists.txt ./silkpre/lib/CMakeLists.txt
    cd ${SILKPRE_DIR} && cmake -S . -B build && cd build && make -j$COMPILE_NUM;\
fi;\

# evmone
cd $SHDIR
if [ -d ${EVMONE_DIR} ]; \
then \
    echo "evmone compile";\
else\
    tar -xvf ./3rd/evmone-0.11.0.tar.gz ;\
    cd ${EVMONE_DIR} && cmake -S . -B build  -DBUILD_SHARED_LIBS=OFF -DEVMONE_TESTING=ON && cd build && make -j$COMPILE_NUM && make install;\
    cd /root/.hunter/_Base/0dfbc2c/e4a7a73/ffb6f53/Install/include
    cp -r ethash /usr/local/include
    cp -r intx /usr/local/include
fi;\

#cryptopp
cd $SHDIR
if [ -d ${CRYPTOPP_DIR}  ]; \
then \
        echo "cryptopp compile";\
    else\
            unzip ./3rd/cryptopp-CRYPTOPP_8_9_0.zip -d ./;                                                                                   
            mv cryptopp-CRYPTOPP_8_9_0 cryptopp;\
            cd ${CRYPTOPP_DIR} && make -j$COMPILE_NUM && make test && sudo make install;\
fi;\

cd $1
echo "-- make depend done"






