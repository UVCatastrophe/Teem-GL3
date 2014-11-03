# NOTE: Change these next three lines as needed
TEEM = $(HOME)/teem-install
GLM = /usr/local/include/glm
ATB = ../AntTweakBar


OS := $(shell uname)
ifeq ($(OS), Darwin)
OS_FLAGS = -lglfw3 -framework CoreVideo  -framework Cocoa -framework OpenGL -framework IOKit
else
OS_FLAGS = -lglfw3 -lGLU -lGL -lX11 -lXxf86vm -lXrandr -lpthread -lXi
endif

FLAGS =  -Wl,-rpath,$(TEEM)/lib -Wl,-rpath,$(ATB)/lib -lteem  -lAntTweakBar -fpermissive
INCLUDE = -I$(TEEM)/include -I$(GLM) -I$(ATB)/include/
LIB = -L$(TEEM)/lib -L$(GLM) -L$(ATB)/lib/

CC = g++ -std=c++11 $(OS_FLAGS) $(FLAGS) $(LIB) $(INCLUDE)

SRC_ISORENDER = isorender.cpp shader.cpp

all: glkdemo polydemo isorender

glkdemo: glkdemo.cpp shader.cpp
	$(CC) $^ -o $@
polydemo: polydemo.cpp shader.cpp
	$(CC) $^ -o $@
isorender: isorender.cpp shader.cpp
	$(CC) $^ -o $@

clean:
	rm -rf polydemo isorender glkdemo
