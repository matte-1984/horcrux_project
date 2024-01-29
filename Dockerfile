FROM ubuntu:22.04

RUN apt update
RUN apt upgrade -y
RUN apt install  -y
RUN apt-get install libboost-all-dev -y
RUN apt-get install g++ -y
RUN apt-get install make -y
RUN apt clean

RUN mkdir /horcrux_project
COPY Makefile /horcrux_project/Makefile

RUN mkdir /horcrux_project/server
COPY ./server/server.cpp /horcrux_project/server

RUN mkdir /horcrux_project/client
COPY ./client/client.cpp /horcrux_project/client
COPY ./client/test_file.txt /horcrux_project/client

WORKDIR /horcrux_project/

RUN make