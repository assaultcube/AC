CXX=g++
CXXOPTFLAGS=-O3 -fomit-frame-pointer
CXXFLAGS=$(CXXOPTFLAGS) -I../enet/include `sdl-config --cflags` -fsigned-char -Wall -Wno-deprecated
PLATFORM_PREFIX=native

CLIENT_LIBS=-L../enet -lenet `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL -lGLU 
CLIENT_OBJS= \
	client.o \
	clientgame.o \
	clients2c.o \
	command.o \
	console.o \
	docs.o \
	editing.o \
	entities.o \
	hudgun.o \
	main.o \
	menus.o \
	physics.o \
	protocol.o \
	rendercubes.o \
	rendergl.o \
	renderhud.o \
	rendermodel.o \
	renderparticles.o \
	rendertext.o \
	rndmap.o \
	savegamedemo.o \
	scoreboard.o \
	server.o \
	serverbrowser.o \
	serverms.o \
	sound.o \
	texture.o \
	tools.o \
	weapon.o \
	world.o \
	worldio.o \
	worldlight.o \
	worldocull.o \
	worldrender.o \
	bot/bot.o \
	bot/botmanager.o \
	bot/bot_ai.o \
	bot/bot_util.o \
	bot/bot_waypoint.o \
	bot/ac_bot.o \
	bot/ac_bot_ai.o 


SERVER_LIBS=-L../enet -lenet
SERVER_FILES= \
	serverms.cpp \
	server.cpp \
	protocol.cpp \
	tools.cpp

default: all

all: libenet client server

../enet/Makefile:
	cd ../enet; ./configure

libenet: ../enet/Makefile
	-$(MAKE) -C ../enet all

clean:
	-$(RM) -f $(SERVER_OBJS) $(CLIENT_OBJS) ac_server ac_client
#	-$(MAKE) -C ../enet/ clean

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o ac_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_FILES)
	$(CXX) $(CXXFLAGS) -o ac_server -DSTANDALONE $(SERVER_FILES) $(SERVER_LIBS)

install: all
	cp ac_client ../../bin_unix/$(PLATFORM_PREFIX)_client
	cp ac_server ../../bin_unix/$(PLATFORM_PREFIX)_server
	strip ../../bin_unix/$(PLATFORM_PREFIX)_client
	strip ../../bin_unix/$(PLATFORM_PREFIX)_server
