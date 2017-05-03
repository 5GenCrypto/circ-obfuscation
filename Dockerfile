#
# To use, run:
#   docker build -t mio .
#   docker run -it mio /bin/bash
#

FROM ubuntu:14.04

RUN apt-get -y update
RUN apt-get -y install software-properties-common
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test
RUN apt-get -y update
RUN apt-get -y install git
RUN apt-get -y install gcc-6 g++-6
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-6
RUN apt-get -y install autoconf libtool make
RUN apt-get -y install libgmp3-dev libmpfr-dev libmpfr4 libssl-dev
RUN apt-get -y install wget zip
RUN apt-get -y install flex bison

WORKDIR /inst
RUN wget http://flintlib.org/flint-2.5.2.tar.gz
RUN tar xvf flint-2.5.2.tar.gz

WORKDIR /inst/flint-2.5.2
RUN ./configure
RUN make -j
RUN make install
RUN ldconfig

WORKDIR /inst
RUN git clone https://github.com/5GenCrypto/circ-obfuscation.git

WORKDIR /inst/circ-obfuscation
RUN git pull origin master
# RUN ./build.sh
