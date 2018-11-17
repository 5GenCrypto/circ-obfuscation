#
# To use, run:
#   docker build -t mio .
#   docker run -it mio /bin/bash
# To mount a host directory when running:
#   docker run -v /host/path:/container/path:z -it mio /bin/bash
#

FROM ubuntu:16.04

RUN apt-get -y update
RUN apt-get -y install git
RUN apt-get -y install make cmake
RUN apt-get -y install libgmp3-dev libmpfr-dev libmpfr4 libssl-dev
RUN apt-get -y install flex bison python

WORKDIR /inst
RUN git clone https://github.com/5GenCrypto/circ-obfuscation.git -b dev

WORKDIR /inst/circ-obfuscation
RUN ./build.sh

CMD git pull origin dev && ./build.sh
