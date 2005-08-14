CXX=g++
CXXOPTFLAGS=-Wall -O3 -fsigned-char -fomit-frame-pointer
CXXFLAGS=$(CXXOPTFLAGS) -I../enet/include `sdl-config --cflags`

CLIENT_LIBS=-L../enet -lenet `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL -lGLU 
CLIENT_OBJS= \
	client.o \
	clientextras.o \
	clientgame.o \
	clients2c.o \
	command.o \
	console.o \
	editing.o \
	entities.o \
	main.o \
	menus.o \
	physics.o \
	rendercubes.o \
	renderextras.o \
	rendergl.o \
	rendermd2.o \
	renderparticles.o \
	rendertext.o \
	rndmap.o \
	savegamedemo.o \
	server.o \
	serverbrowser.o \
	serverms.o \
	serverutil.o \
	sound.o \
	tools.o \
	weapon.o \
	world.o \
	worldio.o \
	worldlight.o \
	worldocull.o \
	worldrender.o

SERVER_LIBS=-L../enet -lenet
SERVER_OBJS= \
	standalone.o \
	serverms.o \
	server.o \
	tools.o

default: all

all: enet client server

enet:
	-make -C ../enet all

clean:
	-rm -f $(SERVER_OBJS) $(CLIENT_OBJS) ../../bin/ac_server ../../bin/ac_client
	-make -C ../enet/ clean

standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -o standalone.o -c serverutil.cpp

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o ../../bin/ac_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o ../../bin/ac_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
