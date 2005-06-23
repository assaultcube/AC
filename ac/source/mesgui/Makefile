CC = gcc
BUILD = build
OPT = -Iinclude -I/usr/include/freetype2 -I/usr/include/SDL -fPIC
OBJECTS = \
	$(BUILD)/misc.o \
	$(BUILD)/font.o \
	$(BUILD)/texture.o \
	$(BUILD)/ggControl.o \
	$(BUILD)/ggButton.o \
	$(BUILD)/ggCheckBox.o \
	$(BUILD)/ggDDMenu.o \
	$(BUILD)/ggFrame.o \
	$(BUILD)/ggMenu.o \
	$(BUILD)/ggProgressBar.o \
	$(BUILD)/ggRadioBox.o \
	$(BUILD)/ggSpinner.o \
	$(BUILD)/ggTable.o \
	$(BUILD)/ggText.o \
	$(BUILD)/mesgui.o

all: lib bin $(BUILD) $(OBJECTS) lib/libmesgui.a lib/libmesgui.so.1.0

lib:
	 -@mkdir lib

bin:
	 -@mkdir bin

$(BUILD):
	 -@mkdir $(BUILD)

$(BUILD)/misc.o:
	$(CC) $(OPT) -c src/misc.cpp -o $@

$(BUILD)/font.o:
	$(CC) $(OPT) -c src/font.cpp -o $@

$(BUILD)/texture.o:
	$(CC) $(OPT) -c src/texture.cpp -o $@

$(BUILD)/ggControl.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggControl.cpp -o $@

$(BUILD)/ggButton.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggButton.cpp -o $@

$(BUILD)/ggCheckBox.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggCheckBox.cpp -o $@

$(BUILD)/ggDDMenu.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggDDMenu.cpp -o $@

$(BUILD)/ggFrame.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggFrame.cpp -o $@

$(BUILD)/ggMenu.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggMenu.cpp -o $@

$(BUILD)/ggProgressBar.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggProgressBar.cpp -o $@

$(BUILD)/ggRadioBox.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggRadioBox.cpp -o $@

$(BUILD)/ggSpinner.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggSpinner.cpp -o $@

$(BUILD)/ggTable.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h
	$(CC) $(OPT) -c src/ggTable.cpp -o $@

$(BUILD)/ggText.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h src/scancodes.h
	$(CC) $(OPT) -c src/ggText.cpp -o $@

$(BUILD)/mesgui.o: src/control_common.h include/mesgui.h src/texture.h src/font.h src/misc.h src/scancodes.h
	$(CC) $(OPT) -c src/mesgui.cpp -o $@

lib/libmesgui.a: $(OBJECTS)
	ar rcs $@ $(OBJECTS)

lib/libmesgui.so.1.0: $(OBJECTS)
	gcc -shared -Wl,-soname,libmesgui.so.1 -o lib/libmesgui.so.1.0  $(OBJECTS) -lc

test: bin/test_mesgui

bin/test_mesgui: test/misc.h test/misc.cpp test/test.cpp test/scancodes.h
	$(CC) $(OPT) test/test.cpp test/misc.cpp -L/usr/X11R6/lib -Llib -lGL -lSDL -lfreetype -lGLU -lmesgui -o $@

install:
	cp include/mesgui.h /usr/local/include
	cp lib/libmesgui.a lib/libmesgui.so.1.0 /usr/local/lib/
	/sbin/ldconfig -n /usr/local/lib
	ln -sf /usr/local/lib/libmesgui.so.1 /usr/local/lib/libmesgui.so

uninstall:
	rm -f /usr/local/lib/libmesgui.*

clean:
	-@rm -rvf bin lib $(BUILD) 
