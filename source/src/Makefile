CXXFLAGS= -O3 -fomit-frame-pointer
CXX=clang++	# Use clang++, as g++ optimizations cause crashes...
override CXXFLAGS+= -Wall -fsigned-char

PLATFORM= $(shell uname -s)
PLATFORM_PREFIX=native

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
CLIENT_LIBS= -mwindows -L../lib -lSDL -lSDL_image -lzdll -lopengl32 -lenet -lOpenAL32 -llibvorbisfile -llibintl -lws2_32 -lwinmm -lcurl
else
USRLIB=$(shell if [ -e /usr/lib64 ]; then echo "/usr/lib64"; else echo "/usr/lib"; fi)
# override CXXFLAGS+= -rdynamic		# clang++ doesn't use this...
CLIENT_INCLUDES= $(INCLUDES) -I/usr/include `sdl-config --cflags` -idirafter ../include
CLIENT_LIBS= -L../enet/.libs -lenet -L$(USRLIB) -lX11 `sdl-config --libs` -lSDL_image -lz -lGL -lopenal -lvorbisfile -lcurl
endif

CLIENT_OBJS= \
	crypto.o \
	audiomanager.o \
	client.o \
	clientgame.o \
	clients2c.o \
	command.o \
	console.o \
	docs.o \
	editing.o \
	entities.o \
	i18n.o \
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
	sound.o \
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
SERVER_LIBS= -L../lib -lzdll -lenet -llibintl -lws2_32 -lwinmm
else
SERVER_INCLUDES= -DSTANDALONE $(INCLUDES)
SERVER_LIBS= -L../enet/.libs -lenet -lz
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
#	-$(MAKE) -C ../enet/ clean

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
	$(WINDRES) -I ../vcpp -i ../vcpp/cube.rc -J rc -o ../vcpp/cube.res -O coff
	$(CXX) $(CXXFLAGS) -o ../../bin_win32/ac_client.exe ../vcpp/cube.res $(CLIENT_OBJS) $(CLIENT_LIBS)

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
crypto.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
crypto.o: console.h protos.h
audiomanager.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
audiomanager.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
audiomanager.o: console.h protos.h
client.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
client.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
client.o: console.h protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
client.o: bot/ac_bot.h
clientgame.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
clientgame.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
clientgame.o: console.h protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
clientgame.o: bot/ac_bot.h
clients2c.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
clients2c.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
clients2c.o: console.h protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
clients2c.o: bot/ac_bot.h
command.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
command.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
command.o: console.h protos.h
console.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
console.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
console.o: console.h protos.h
docs.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
docs.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
editing.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
editing.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
editing.o: console.h protos.h
entities.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
entities.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
entities.o: console.h protos.h
i18n.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
i18n.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
log.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
log.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
main.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
main.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
main.o: jpegenc.h
menus.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
menus.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
oggstream.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
oggstream.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
oggstream.o: console.h protos.h
openal.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
openal.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
openal.o: console.h protos.h
packetqueue.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
packetqueue.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
packetqueue.o: console.h protos.h
physics.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
physics.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
physics.o: console.h protos.h
protocol.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
protocol.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
protocol.o: console.h protos.h
rendercubes.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rendercubes.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
rendercubes.o: console.h protos.h
rendergl.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rendergl.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
rendergl.o: console.h protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
rendergl.o: bot/ac_bot.h
renderhud.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
renderhud.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
renderhud.o: console.h protos.h
rendermodel.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rendermodel.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
rendermodel.o: console.h protos.h tristrip.h modelcache.h vertmodel.h md2.h
rendermodel.o: md3.h
renderparticles.o: cube.h platform.h tools.h geom.h model.h protocol.h
renderparticles.o: sound.h weapon.h entity.h world.h i18n.h command.h
renderparticles.o: varray.h vote.h console.h protos.h
rendertext.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rendertext.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
rendertext.o: console.h protos.h
rndmap.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
rndmap.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
rndmap.o: console.h protos.h
scoreboard.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
scoreboard.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
scoreboard.o: console.h protos.h
serverms.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
serverms.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
serverms.o: console.h protos.h
server.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
server.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
server.o: console.h protos.h server.h servercontroller.h serverfiles.h
server.o: serverchecks.h serverevents.h serveractions.h
serverbrowser.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
serverbrowser.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
serverbrowser.o: console.h protos.h
shadow.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
shadow.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
shadow.o: console.h protos.h
sound.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
sound.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
soundlocation.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
soundlocation.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
soundlocation.o: console.h protos.h
soundscheduler.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
soundscheduler.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
soundscheduler.o: console.h protos.h
stream.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
stream.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
stream.o: console.h protos.h
texture.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
texture.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
texture.o: console.h protos.h scale.h
tools.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
tools.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
water.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
water.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
weapon.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
weapon.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
weapon.o: console.h protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
weapon.o: bot/ac_bot.h hudgun.h
wizard.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
wizard.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
wizard.o: console.h protos.h
world.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
world.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
world.o: bot/bot.h bot/bot_util.h bot/bot_waypoint.h bot/ac_bot.h
worldio.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
worldio.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
worldio.o: console.h protos.h
worldlight.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
worldlight.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
worldlight.o: console.h protos.h
worldocull.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
worldocull.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
worldocull.o: console.h protos.h
worldrender.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
worldrender.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
worldrender.o: console.h protos.h
zip.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
zip.o: entity.h world.h i18n.h command.h varray.h vote.h console.h protos.h
bot/bot.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/bot.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
bot/bot.o: console.h protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
bot/bot.o: bot/ac_bot.h
bot/botmanager.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/botmanager.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
bot/botmanager.o: console.h protos.h bot/bot.h bot/bot_util.h
bot/botmanager.o: bot/bot_waypoint.h bot/ac_bot.h
bot/bot_ai.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/bot_ai.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
bot/bot_ai.o: console.h protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
bot/bot_ai.o: bot/ac_bot.h
bot/bot_util.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/bot_util.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
bot/bot_util.o: console.h protos.h bot/bot.h bot/bot_util.h
bot/bot_util.o: bot/bot_waypoint.h bot/ac_bot.h
bot/bot_waypoint.o: cube.h platform.h tools.h geom.h model.h protocol.h
bot/bot_waypoint.o: sound.h weapon.h entity.h world.h i18n.h command.h
bot/bot_waypoint.o: varray.h vote.h console.h protos.h bot/bot.h
bot/bot_waypoint.o: bot/bot_util.h bot/bot_waypoint.h bot/ac_bot.h
bot/ac_bot.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/ac_bot.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
bot/ac_bot.o: console.h protos.h bot/bot.h bot/bot_util.h bot/bot_waypoint.h
bot/ac_bot.o: bot/ac_bot.h
bot/ac_bot_ai.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
bot/ac_bot_ai.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
bot/ac_bot_ai.o: console.h protos.h bot/bot.h bot/bot_util.h
bot/ac_bot_ai.o: bot/bot_waypoint.h bot/ac_bot.h

cube.h.gch: platform.h tools.h geom.h model.h protocol.h sound.h weapon.h
cube.h.gch: entity.h world.h i18n.h command.h varray.h vote.h console.h
cube.h.gch: protos.h

crypto-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
crypto-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
crypto-standalone.o: varray.h vote.h console.h protos.h
log-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h sound.h
log-standalone.o: weapon.h entity.h world.h i18n.h command.h varray.h vote.h
log-standalone.o: console.h protos.h
protocol-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
protocol-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
protocol-standalone.o: varray.h vote.h console.h protos.h
serverms-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
serverms-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
serverms-standalone.o: varray.h vote.h console.h protos.h
server-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
server-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
server-standalone.o: varray.h vote.h console.h protos.h server.h
server-standalone.o: servercontroller.h serverfiles.h serverchecks.h
server-standalone.o: serverevents.h serveractions.h
stream-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
stream-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
stream-standalone.o: varray.h vote.h console.h protos.h
tools-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
tools-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
tools-standalone.o: varray.h vote.h console.h protos.h
wizard-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
wizard-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
wizard-standalone.o: varray.h vote.h console.h protos.h

command-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
command-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
command-standalone.o: varray.h vote.h console.h protos.h
master-standalone.o: cube.h platform.h tools.h geom.h model.h protocol.h
master-standalone.o: sound.h weapon.h entity.h world.h i18n.h command.h
master-standalone.o: varray.h vote.h console.h protos.h
