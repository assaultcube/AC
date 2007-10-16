CXXOPTFLAGS=-O3 -fomit-frame-pointer
INCLUDES=
CXXFLAGS=$(CXXOPTFLAGS) $(INCLUDES) -fsigned-char -Wall -Wno-deprecated
LIBS= 

default:
	$(CXX) $(CXXFLAGSS) wizard.cpp -o server_wizard $(LIBBS) 

clean:
	-$(RM) -f $(OBJS) server_wizard

install: default
	cp server_wizard ../../../bin_unix/server_wizard
	strip ../../../bin_unix/server_wizard
