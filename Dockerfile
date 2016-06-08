FROM	fritzprix/cbuild:0.0.1

MAINTAINER	fritzprix


RUN	apt-get update 
RUN	git clone https://github.com/fritzprix/wtree.git
WORKDIR	wtree
RUN	make config DEFCONF=wtree_default.conf
RUN	make clean 
RUN	make all
RUN	make test
CMD	./yamalloc

