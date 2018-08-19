#
#
# Makefile for glutcam


TARGET = glutcam
#leave it blank for YUV
DO_RGB = 1

OBJS = callbacks.o  capabilities.o  device.o  display.o  glutcam.o \
       parseargs.o  shader.o  testpattern.o textfile.o controls.o cvProcess.o




ifeq ($(DO_RGB),)
LDFLAGS = -L/usr/X11R6/lib -g -lGLEW -lglut -lGLU -lGL -lXmu -lX11
OPT =
else
LDFLAGS = -L/usr/X11R6/lib -L/usr/lib/x86_64-linux-gnu -g -lGLEW -lglut -lGLU -lGL -lXmu -lX11\
	-lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_features2d\
	-lopencv_objdetect -lopencv_calib3d
#	-lopencv_contrib -lopencv_gpu -lopencv_objdetect -lopencv_calib3d
OPT += -DDEF_RGB `pkg-config --cflags opencv` -I/usr/include/opencv
endif

# profiling:
# 
# to turn on profiling, recompile with
# PROFILE set to -pg and OPT set to -g
#

#PROFILE= -pg 

# no profiling
PROFILE=

# optimization:
#
# for max optimization, turn OPT to O3.
# for debugging or profiling, use -g

# profiling, debugging
OPT += -g
CPU_OPT =



# max optimization
#OPT = -O3

# set CPU_OPT based on the contents of /proc/cpuinfo. in particular pay 
# attention to the model name and flags. compare against the listing for 
# cpu type on the gcc info pages entry for cpu type 

CPU_TYPE := $(shell uname -m)
ifeq ($(CPU_TYPE),x86_64)
CPU_OPT = -march=x86-64
CC = gcc
CXX = g++
else
CPU_OPT = -march=armv7-a
CC = /usr/bin/arm-linux-gnueabihf-gcc
CXX = /usr/bin/arm-linux-gnueabihf-g++
LDFLAGS += -lopencv_facedetect
endif

DFLAGS = -DGL_CHECK

CFLAGS = -Wformat=2 -Wall -Wswitch-default -Wswitch-enum \
         -Wunused-parameter -Wextra -Wshadow \
         -Wbad-function-cast -Wsign-compare -Wstrict-prototypes \
         -Wmissing-prototypes -Wmissing-declarations -Wunreachable-code \
	 -ffast-math $(CPU_OPT) $(PROFILE) $(OPT)


$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS)  $(PROFILE) $(OPT) $(LDFLAGS)



%.o : %.c
	$(CC) -c $(CFLAGS) $(DFLAGS) $<

%.o : %.cpp
	$(CXX) -c $(CFLAGS) $(DFLAGS) $<


flow:
	cflow $(OBJS:.o=.c)

# lint -O2 -Wuninitialized -Wextra -fsyntax-only -pedantic 

lint:
	$(CC) -O2  $(DFLAGS) -Wuninitialized -Wextra -fsyntax-only \
	         -pedantic  $(OBJS:.o=.c)

clean:
	@rm -f $(TARGET) $(OBJS)
