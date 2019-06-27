PLUGINS =  \
			  actcstrip.so \

LFLAGS = -O2 -Wall 
CFLAGS = -O2 -Wall -DMM3D_EDIT -I../.. -I../../src -I../../src/libmm3d -DPLUGIN

all: actc_lib ${PLUGINS}

actc_lib:
	make -C actc libactc.a
	
%.o: %.cc
	g++ ${CFLAGS} -c $<

%.so: %.o
	g++ -g -Wall -shared -o $*.so $< actc/libactc.a

clean:
	make -C actc clean
	rm -rf *.o *.so core core.*
