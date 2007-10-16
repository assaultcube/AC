CXXOPTFLAGS=-O3 -fomit-frame-pointer
INCLUDES=-I/usr/X11R6/include -I../enet/include `sdl-config --cflags`
CXXFLAGS=$(CXXOPTFLAGS) $(INCLUDES) -fsigned-char -Wall -Wno-deprecated

PLATFORM_PREFIX=native
LIBS=-L/usr/X11R6/lib -L../enet -lenet `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL -lGLU 

default:
	$(CXX) $(CXXFLAGSS) wizard.cpp -o server_wizard $(LIBBS) 

clean:
	-$(RM) -f $(OBJS) server_wizard

install: default
	cp server_wizard ../../../bin_unix/server_wizard
	strip ../../../bin_unix/server_wizard
