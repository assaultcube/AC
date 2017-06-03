# Ideally, you can override these parameters directly via the commandline, or by
# creating a seperate 'Makefile_local' file (this way, your changes aren't
# accidentally commited to the AssaultCube repository).

# AssaultCube now uses clang++ as a compiler, as there have been random crashes
# found to have been caused by the g++ compiler in the past. This seems to have
# been fixed now by relaxing the optimization that g++ does, so although we'll
# continue using clang++ (just in case), you can use g++ if you prefer.
CXX=clang++

# Changing this to ACDEBUG=yes will compile a debug version of AssaultCube.
ACDEBUG=no

PLATFORM=$(shell uname -s)

# This prefix is only used to name the installed executables.
PLATFORM_PREFIX=native

# As above, this file is optional only.
-include Makefile_local


ifeq ($(ACDEBUG),yes)
	CXXFLAGS= -O0
	override CXXFLAGS+= -g -D_DEBUG
endif

ifneq (,$(findstring clang,$(CXX)))
	CXXFLAGS= -O3 -fomit-frame-pointer
	override CXXFLAGS+= -Wall -fsigned-char
else
	CXXFLAGS= -O3 -fomit-frame-pointer
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

mrproper: clean ../enet/Makefile
	$(MAKE) -C ../enet/ distclean
	-$(RM) ../enet/Makefile

%.h.gch: %.h
	$(CXX) $(CXXFLAGS) -x c++-header -o $@.tmp $(subst .h.gch,.h,$@)
	$(MV) $@.tmp $@

rendermodel.o: rendermodel.cpp
	$(CXX) $(CXXFLAGS) -fno-strict-aliasing -c -o $@ $(subst .o,.cpp,$@)

%-standalone.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(subst -standalone.o,.cpp,$@)

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)
$(CLIENT_OBJS): $(CLIENT_PCH)
$(SERVER_OBJS): CXXFLAGS += $(SERVER_INCLUDES)
$(filter-out $(SERVER_OBJS),$(MASTER_OBJS)): CXXFLAGS += $(SERVER_INCLUDES)

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

depend:
	makedepend -Y -I. -Ibot $(subst .o,.cpp,$(CLIENT_OBJS))
	makedepend -a -o.h.gch -Y -I. -Ibot $(subst .h.gch,.h,$(CLIENT_PCH))
	makedepend -a -o-standalone.o -Y -I. -Ibot $(subst -standalone.o,.cpp,$(SERVER_OBJS))
	makedepend -a -o-standalone.o -Y -I. $(subst -standalone.o,.cpp,$(filter-out $(SERVER_OBJS), $(MASTER_OBJS)))

# DO NOT DELETE

crypto.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
crypto.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
crypto.o: protos.h crypto_tools.h ge25519_base.data
audiomanager.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
audiomanager.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
audiomanager.o: protos.h
autodownload.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
autodownload.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
autodownload.o: protos.h
client.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
client.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
client.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h bot/ac_bot.h
clientgame.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
clientgame.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
clientgame.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
clientgame.o: bot/ac_bot.h
clients2c.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
clients2c.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
clients2c.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
clients2c.o: bot/ac_bot.h
command.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
command.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
command.o: protos.h
console.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
console.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
console.o: protos.h
docs.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
docs.o: entity.h world.h command.h varray.h vote.h console.h protos.h
editing.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
editing.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
editing.o: protos.h
entities.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
entities.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
entities.o: protos.h
http.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
http.o: entity.h world.h command.h varray.h vote.h console.h protos.h
log.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
log.o: entity.h world.h command.h varray.h vote.h console.h protos.h
main.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
main.o: entity.h world.h command.h varray.h vote.h console.h protos.h
main.o: jpegenc.h
menus.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
menus.o: entity.h world.h command.h varray.h vote.h console.h protos.h
oggstream.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
oggstream.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
oggstream.o: protos.h
openal.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
openal.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
openal.o: protos.h
packetqueue.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
packetqueue.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
packetqueue.o: protos.h
physics.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
physics.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
physics.o: protos.h
protocol.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
protocol.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
protocol.o: protos.h
rendercubes.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rendercubes.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
rendercubes.o: protos.h
rendergl.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rendergl.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
rendergl.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h bot/ac_bot.h
renderhud.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
renderhud.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
renderhud.o: protos.h
rendermodel.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rendermodel.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
rendermodel.o: protos.h tristrip.h modelcache.h vertmodel.h md2.h md3.h
renderparticles.o: cube.h platform.h tools.h geom.h model.h protocol.h
renderparticles.o: sound.h weapon.h entity.h world.h command.h varray.h
renderparticles.o: vote.h console.h protos.h
rendertext.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rendertext.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
rendertext.o: protos.h
rndmap.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rndmap.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
rndmap.o: protos.h
scoreboard.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
scoreboard.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
scoreboard.o: protos.h
serverms.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
serverms.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
serverms.o: protos.h
server.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
server.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
server.o: protos.h server.h servercontroller.h serverfiles.h serverchecks.h
server.o: serverevents.h serveractions.h
serverbrowser.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
serverbrowser.o: weapon.h entity.h world.h command.h varray.h vote.h
serverbrowser.o: console.h protos.h
shadow.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
shadow.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
shadow.o: protos.h
soundlocation.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
soundlocation.o: weapon.h entity.h world.h command.h varray.h vote.h
soundlocation.o: console.h protos.h
soundscheduler.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
soundscheduler.o: weapon.h entity.h world.h command.h varray.h vote.h
soundscheduler.o: console.h protos.h
stream.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
stream.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
stream.o: protos.h
texture.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
texture.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
texture.o: protos.h scale.h
tools.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
tools.o: entity.h world.h command.h varray.h vote.h console.h protos.h
water.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
water.o: entity.h world.h command.h varray.h vote.h console.h protos.h
weapon.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
weapon.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
weapon.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h bot/ac_bot.h
weapon.o: hudgun.h
wizard.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
wizard.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
wizard.o: protos.h
world.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
world.o: entity.h world.h command.h varray.h vote.h console.h protos.h
world.o: bot/bot.h bot/bot_util.h bot/bot_waypoint.h bot/ac_bot.h
worldio.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
worldio.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
worldio.o: protos.h
worldlight.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
worldlight.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
worldlight.o: protos.h
worldocull.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
worldocull.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
worldocull.o: protos.h
worldrender.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
worldrender.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
worldrender.o: protos.h
zip.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
zip.o: entity.h world.h command.h varray.h vote.h console.h protos.h
bot/bot.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/bot.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
bot/bot.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h bot/ac_bot.h
bot/botmanager.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/botmanager.o: weapon.h entity.h world.h command.h varray.h vote.h
bot/botmanager.o: console.h protos.h bot/bot.h bot/bot_util.h
bot/botmanager.o: bot/bot_waypoint.h bot/ac_bot.h
bot/bot_ai.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/bot_ai.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
bot/bot_ai.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
bot/bot_ai.o: bot/ac_bot.h
bot/bot_util.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/bot_util.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
bot/bot_util.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
bot/bot_util.o: bot/ac_bot.h
bot/bot_waypoint.o: cube.h platform.h tools.h geom.h model.h protocol.h
bot/bot_waypoint.o: sound.h weapon.h entity.h world.h command.h varray.h
bot/bot_waypoint.o: vote.h console.h protos.h bot/bot.h bot/bot_util.h
bot/bot_waypoint.o: bot/bot_waypoint.h bot/ac_bot.h
bot/ac_bot.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/ac_bot.o: weapon.h entity.h world.h command.h varray.h vote.h console.h
bot/ac_bot.o: protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
bot/ac_bot.o: bot/ac_bot.h
bot/ac_bot_ai.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/ac_bot_ai.o: weapon.h entity.h world.h command.h varray.h vote.h
bot/ac_bot_ai.o: console.h protos.h bot/bot.h bot/bot_util.h
bot/ac_bot_ai.o: bot/bot_waypoint.h bot/ac_bot.h

cube.h.gch: platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
cube.h.gch: entity.h world.h command.h varray.h vote.h console.h protos.h

crypto-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
crypto-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
crypto-standalone.o: vote.h console.h protos.h crypto_tools.h
crypto-standalone.o: ge25519_base.data
log-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
log-standalone.o: weapon.h entity.h world.h command.h varray.h vote.h
log-standalone.o: console.h protos.h
protocol-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
protocol-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
protocol-standalone.o: vote.h console.h protos.h
serverms-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
serverms-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
serverms-standalone.o: vote.h console.h protos.h
server-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
server-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
server-standalone.o: vote.h console.h protos.h server.h servercontroller.h
server-standalone.o: serverfiles.h serverchecks.h serverevents.h
server-standalone.o: serveractions.h
stream-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
stream-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
stream-standalone.o: vote.h console.h protos.h
tools-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
tools-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
tools-standalone.o: vote.h console.h protos.h
wizard-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
wizard-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
wizard-standalone.o: vote.h console.h protos.h

command-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
command-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
command-standalone.o: vote.h console.h protos.h
master-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
master-standalone.o: sound.h weapon.h entity.h world.h command.h varray.h
master-standalone.o: vote.h console.h protos.h
