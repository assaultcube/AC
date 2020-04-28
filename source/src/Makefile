# Ideally, you can override these parameters directly via the commandline, or by
# creating a seperate 'Makefile_local' file (this way, your changes aren't
# accidentally commited to the AssaultCube repository).

# AssaultCube now uses clang++ as a compiler, as there have been random crashes
# found to have been caused by the g++ compiler in the past. This seems to have
# been fixed now by relaxing the optimization that g++ does, so although we'll
# continue using clang++ (just in case), you can use g++ if you prefer.
CXX=clang++

CPPFLAGS = -MMD -MP

# Changing this to ACDEBUG=yes will compile a debug version of AssaultCube.
ACDEBUG=no

PLATFORM=$(shell uname -s)

# This prefix is only used to name the installed executables.
PLATFORM_PREFIX=native

# As above, this file is optional only.
-include Makefile_local


ifeq ($(ACDEBUG),yes)
	CXXFLAGS+= -O0
	override CXXFLAGS+= -g -D_DEBUG
endif

ifneq (,$(findstring clang,$(CXX)))
	CXXFLAGS+= -O3 -fomit-frame-pointer
	override CXXFLAGS+= -Wall -fsigned-char
else
	CXXFLAGS+= -O3 -fomit-frame-pointer
	override CXXFLAGS+= -Wall -fsigned-char -ffast-math -rdynamic -Wno-format-zero-length
endif

INCLUDES= -I. -Ibot -I../enet/include

STRIP=
ifeq (,$(findstring -g,$(CXXFLAGS)))
ifeq (,$(findstring -pg,$(CXXFLAGS)))
  STRIP=strip
endif
endif

MV=mv
ifneq (,$(findstring MINGW,$(PLATFORM)))
	WINDRES= windres
	CLIENT_INCLUDES= $(INCLUDES) -I../include
	CLIENT_LIBS= -mwindows -L../lib -lSDL -lSDL_image -lzdll -lopengl32 -lenet -lOpenAL32 -llibvorbisfile -lws2_32 -lwinmm
else
	USRLIB=$(shell if [ -e /usr/lib64 ]; then echo "/usr/lib64"; else echo "/usr/lib"; fi)
	CLIENT_INCLUDES= $(INCLUDES) -I/usr/include `sdl-config --cflags` -idirafter ../include
	CLIENT_LIBS= -L../enet/.libs -lenet -L$(USRLIB) -lX11 `sdl-config --libs` -lSDL_image -lz -lGL -lopenal -lvorbisfile
endif

CLIENT_OBJS= \
	crypto.o \
	audiomanager.o \
	autodownload.o \
	client.o \
	clientgame.o \
	clients2c.o \
	command.o \
	console.o \
	docs.o \
	editing.o \
	entities.o \
	http.o \
	log.o \
	main.o \
	menus.o \
	oggstream.o \
	openal.o \
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
	serverms.o \
	server.o \
	serverbrowser.o \
	shadow.o \
	soundlocation.o \
	soundscheduler.o \
	stream.o \
	texture.o \
	tools.o \
	water.o \
	weapon.o \
	wizard.o \
	world.o \
	worldio.o \
	worldlight.o \
	worldocull.o \
	worldrender.o \
	zip.o \
	bot/bot.o \
	bot/botmanager.o \
	bot/bot_ai.o \
	bot/bot_util.o \
	bot/bot_waypoint.o \
	bot/ac_bot.o \
	bot/ac_bot_ai.o

CLIENT_PCH= cube.h.gch

ifneq (,$(findstring MINGW,$(PLATFORM)))
	SERVER_INCLUDES= -DSTANDALONE $(INCLUDES) -I../include
	SERVER_LIBS= -L../lib -lzdll -lenet -lws2_32 -lwinmm
else
	SERVER_INCLUDES= -DSTANDALONE $(INCLUDES)
	SERVER_LIBS= -L../enet/.libs -lenet -lz -lpthread
	ifeq (,$(findstring OpenBSD,$(PLATFORM)))
		SERVER_LIBS+= -lrt
	endif
endif

SERVER_OBJS= \
	crypto-standalone.o \
	log-standalone.o \
	protocol-standalone.o \
	serverms-standalone.o \
	server-standalone.o \
	stream-standalone.o \
	tools-standalone.o \
	wizard-standalone.o
MASTER_OBJS= \
	crypto-standalone.o \
	stream-standalone.o \
	command-standalone.o \
	master-standalone.o

ifeq ($(PLATFORM),SunOS)
	CLIENT_LIBS+= -lsocket -lnsl -lX11
	SERVER_LIBS+= -lsocket -lnsl
endif

default: all

all: client server

../enet/Makefile:
	cd ../enet; ./configure --enable-shared=no --enable-static=yes

libenet: ../enet/Makefile
	-$(MAKE) -C ../enet all

clean-enet: ../enet/Makefile
	$(MAKE) -C ../enet/ clean

clean:
	-$(RM) $(CLIENT_PCH) $(CLIENT_OBJS) $(SERVER_OBJS) $(MASTER_OBJS) ac_client ac_server ac_master
	-$(RM) $(DEPENDS)
	-$(RM) $(CLIENT_PCH) $(CLIENT_PCH).d

mrproper: clean ../enet/Makefile
	$(MAKE) -C ../enet/ distclean
	-$(RM) ../enet/Makefile

%.h.gch: %.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -x c++-header -o $@.tmp $(subst .h.gch,.h,$@)
	$(MV) $@.tmp $@

rendermodel.o: CXXFLAGS += -fno-strict-aliasing
rendermodel.o: rendermodel.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $(subst .o,.cpp,$@)


%-standalone.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $(subst -standalone.o,.cpp,$@)

$(CLIENT_OBJS): CPPFLAGS += $(CLIENT_INCLUDES)
$(CLIENT_OBJS): $(CLIENT_PCH)
$(SERVER_OBJS): CPPFLAGS += $(SERVER_INCLUDES)
$(filter-out $(SERVER_OBJS),$(MASTER_OBJS)): CPPFLAGS += $(SERVER_INCLUDES)

ifneq (,$(findstring MINGW,$(PLATFORM)))
client: $(CLIENT_OBJS)
	$(WINDRES) -I ../vcpp -i ../vcpp/assaultcube.rc -J rc -o ../vcpp/assaultcube.res -O coff
	$(CXX) $(CXXFLAGS) -o ../../bin_win32/ac_client.exe ../vcpp/assaultcube.res $(CLIENT_OBJS) $(CLIENT_LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o ../../bin_win32/ac_server.exe $(SERVER_OBJS) $(SERVER_LIBS)

master: $(MASTER_OBJS)
	$(CXX) $(CXXFLAGS) -o ../../bin_win32/ac_master.exe $(MASTER_OBJS) $(SERVER_LIBS)

client_install: client
server_install: server

else

client: libenet $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o ac_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server: libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o ac_server $(SERVER_OBJS) $(SERVER_LIBS)

master: libenet $(MASTER_OBJS)
	$(CXX) $(CXXFLAGS) -o ac_master $(MASTER_OBJS) $(SERVER_LIBS)

client_install: client
	install -d ../../bin_unix/
	install -m755 ac_client ../../bin_unix/$(PLATFORM_PREFIX)_client
#ifneq (,$(STRIP))
#	$(STRIP) ../../bin_unix/$(PLATFORM_PREFIX)_client
#endif

server_install: server
	install -d ../../bin_unix/
	install -m755 ac_server ../../bin_unix/$(PLATFORM_PREFIX)_server
#ifneq (,$(STRIP))
#	$(STRIP) ../../bin_unix/$(PLATFORM_PREFIX)_server
#endif

endif

install: client_install server_install

DEPENDS = $(SERVER_OBJS:%.o=%.d)
DEPENDS+= $(CLIENT_OBJS:%.o=%.d)
DEPENDS+= $(MASTER_OBJS:%.o=%.d)

# %.d : CPPFLAGS += $(CLIENT_INCLUDES) $(SERVER_INCLUDES)
-include $(DEPENDS)
