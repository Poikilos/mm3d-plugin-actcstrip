PLUGINS =  \
			  actcstrip.so \

LFLAGS = -O2 -Wall -shared 

# Mac OS X flags. Uncomment the following line to compile on Mac OS X
#LFLAGS = -O2 -Wall -undefined suppress -bundle -flat_namespace

CFLAGS = -O2 -Wall -DMM3D_EDIT -I../.. -I../../src -I../../src/libmm3d -I../../src/mm3dcore -DPLUGIN

all: actc_lib ${PLUGINS}

actc_lib:
	make -C actc libactc.a
	
%.o: %.cc
	g++ ${CFLAGS} -c $<

%.so: %.o
	g++ ${LFLAGS} -Wall -o $*.so $< actc/libactc.a

clean:
	make -C actc clean
	rm -rf *.o *.so core core.*
