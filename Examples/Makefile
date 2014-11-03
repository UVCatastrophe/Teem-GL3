# NOTE: Change these next three lines as needed
TEEM = $(HOME)/teem-install
GLM = /usr/local/include/glm/
ATB = ../AntTweakBar

#GLM = /home/glk/roman/glm/
#ATB = /home/glk/roman/AntTweakBar

OS := $(shell uname)
ifeq ($(OS), Darwin)
OS_LIBS = -framework CoreVideo  -framework Cocoa -framework OpenGL -framework IOKit
else
OS_LIBS = -lGL -lX11 -lXxf86vm -lXrandr -lpthread -lXi
endif

LIBS = -lglfw3 -lteem -lAntTweakBar

RPATH = -Wl,-rpath,$(TEEM)/lib -Wl,-rpath,$(ATB)/lib
IPATH = -I$(TEEM)/include -I$(GLM) -I$(ATB)/include/
LPATH = -L$(TEEM)/lib -L$(GLM) -L$(ATB)/lib/

CC = g++ -std=c++11

all: glkdemo polydemo isorender

glkdemo: glkdemo.cpp shader.cpp
	$(CC) $(IPATH) glkdemo.cpp shader.cpp -o glkdemo $(RPATH) $(LPATH) $(LIBS) $(OS_LIBS)
polydemo: polydemo.cpp shader.cpp
	$(CC) $(IPATH) polydemo.cpp shader.cpp -o polydemo $(RPATH) $(LPATH) $(LIBS) $(OS_LIBS)
isorender: isorender.cpp shader.cpp
	$(CC) $(IPATH) isorender.cpp shader.cpp -o isorender $(RPATH) $(LPATH) $(LIBS) $(OS_LIBS)

clean:
	rm -rf polydemo isorender glkdemo
