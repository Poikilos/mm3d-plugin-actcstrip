PLUGINS =  \
			  actcstrip.so \

LFLAGS = -g -Wall 
CFLAGS = -g -Wall -I../../src -DPLUGIN

all: actc_lib ${PLUGINS}

actc_lib:
	make -C actc libactc.a
	
%.o: %.cc
	g++ ${CFLAGS} -c $<

%.so: %.o
	g++ -Wall -shared -o $*.so $< actc/libactc.a

clean:
	make -C actc clean
	rm -rf *.o *.so core core.*
