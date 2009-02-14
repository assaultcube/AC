CXXOPTFLAGS=-O3 -fomit-frame-pointer
INCLUDES1=-I/usr/X11R6/include -I../enet/include -I../src
INCLUDES2=`sdl-config --cflags`
CXXCUSTOMFLAGS=-fsigned-char -Wall -Wextra -Wno-missing-field-initializers -Wno-unused -Wpointer-arith -Wwrite-strings -Wconversion -rdynamic
CXXFLAGS=$(CXXOPTFLAGS) $(INCLUDES1) $(INCLUDES2) $(CUSTOMFLAGS)
CXXFLAGSSERVER=$(CXXOPTFLAGS) $(INCLUDES1) $(CUSTOMFLAGS)

PLATFORM_PREFIX=native
CLIENT_LIBS=-L/usr/X11R6/lib -L../enet -lenet `sdl-config --libs` -lSDL_image -lz -lGL -lopenal -lvorbisfile
CLIENT_OBJS1= \
	client.o \
	clientgame.o \
	clients2c.o \
	command.o \
	console.o \
	docs.o \
	editing.o \
	entities.o \
	log.o \
	main.o \
	menus.o \
	packetqueue.o \
	physics.o \
	protocol.o \
	rendercubes.o \
	rendergl.o \
	renderhud.o \
	rendermodel.o \
	renderparticles.o \
	rendertext.o \
	rndmap.o \
	scoreboard.o \
	server.o \
	serverbrowser.o \
	serverms.o \
	shadow.o \
	sound.o \
	texture.o \
	tools.o \
	water.o \
	weapon.o \
	wizard.o \
	world.o \
	worldio.o \
	worldlight.o \
	worldocull.o \
	worldrender.o
CLIENT_OBJS2= \
	bot/bot.o \
	bot/botmanager.o \
	bot/bot_ai.o \
	bot/bot_util.o \
	bot/bot_waypoint.o \
	bot/ac_bot.o \
	bot/ac_bot_ai.o 


SERVER_LIBS=-L../enet -lenet -lz
SERVER_FILES= \
	log.cpp \
	serverms.cpp \
	server.cpp \
	protocol.cpp \
	tools.cpp \
	wizard.cpp

default: all

all: libenet client server

../enet/Makefile:
	cd ../enet; ./configure

libenet: ../enet/Makefile
	-$(MAKE) -C ../enet all

clean:
	-$(RM) -f $(SERVER_OBJS) $(CLIENT_OBJS1) ac_server ac_client
#	-$(MAKE) -C ../enet/ clean

mrproper: clean
	-$(RM) -f $(CLIENT_OBJS2)
	-$(MAKE) -C ../enet/ clean

client: $(CLIENT_OBJS1) $(CLIENT_OBJS2)
	$(CXX) $(CXXFLAGS) -o ac_client $(CLIENT_OBJS1) $(CLIENT_OBJS2) $(CLIENT_LIBS)

server: libenet $(SERVER_FILES)
	$(CXX) $(CXXFLAGSSERVER) -o ac_server -DSTANDALONE $(SERVER_FILES) $(SERVER_LIBS)

install: all
	cp ac_client ../../bin_unix/$(PLATFORM_PREFIX)_client
	cp ac_server ../../bin_unix/$(PLATFORM_PREFIX)_server
#	strip ../../bin_unix/$(PLATFORM_PREFIX)_client
#	strip ../../bin_unix/$(PLATFORM_PREFIX)_server

server_install: server
	cp ac_server ../../bin_unix/$(PLATFORM_PREFIX)_server
#	strip ../../bin_unix/$(PLATFORM_PREFIX)_server
