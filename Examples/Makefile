RT = /Users/ramici
TEEM = $(RT)/teem-install
GLM = $(RT)/glm
ATB = $(RT)/AntTweakBar

CC = g++ -std=c++11 

OS := $(shell uname)
ifeq ($(OS), Darwin)
OS_FLAGS = -lglfw -framework CoreVideo  -framework Cocoa -framework OpenGL -framework IOKit 
else
OS_FLAGS = -lglfw3 -lGLU -lGL -lX11 -lXxf86vm -lXrandr -lpthread -lXi
endif

FLAGS =  -Wl,-rpath,$(TEEM)/lib -Wl,-rpath,$(ATB)/lib -lteem  -lAntTweakBar -fpermissive
INCLUDE = -I$(TEEM)/include -I$(GLM) -I$(ATB)/include/
LIB = -L$(TEEM)/lib -L$(GLM) -L$(ATB)/lib/

SRC_POLYDEMO = polydemo.cpp shader.cpp
OUT_POLYDEMO = -o polydemo

SRC_ISORENDER = isorender.cpp shader.cpp
OUT_ISORENDER = -o isorender

All: poly iso
poly:
	$(CC) $(SRC_POLYDEMO) $(OUT_POLYDEMO) $(OS_FLAGS) $(FLAGS) $(LIB) $(INCLUDE)
iso:
	$(CC) $(SRC_ISORENDER) $(OUT_ISORENDER) $(OS_FLAGS) $(FLAGS) $(LIB) $(INCLUDE)
clean: rm -rf isorender polydemo