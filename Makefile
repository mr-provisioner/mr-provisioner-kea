KEA_SRC_LIB_DIR?=/usr/src/kea-1.2.0/src/lib
KEA_LIB_DIR?=/usr/local/lib
KEA_MSG_COMPILER?=/usr/local/bin/kea-msg-compiler

BOOST_INCS?= #-isystem /arm/tools/boost/boost/1.54.0/rhe6-x86_64/include
INCS= -I$(KEA_SRC_LIB_DIR) $(BOOST_INCS)
CXX?=g++

WARNFLAGS=-Wall -W -Wno-unused-parameter -Wpointer-arith -Wreturn-type -Wwrite-strings -Wswitch -Wcast-align -Wchar-subscripts
CXXFLAGS=-std=c++11

LIBADD=
LIBADD += $(KEA_LIB_DIR)/libkea-hooks.a
LIBADD += $(KEA_LIB_DIR)/libkea-log.a
LIBADD += $(KEA_LIB_DIR)/libkea-threads.a
LIBADD += $(KEA_LIB_DIR)/libkea-util.a
LIBADD += $(KEA_LIB_DIR)/libkea-exceptions.a

LDFLAGS_CURL=$(shell curl-config --libs)

# -lkea-dhcpsrv -lkea-dhcp++ -lkea-hooks -lkea-log -lkea-util \
#      -lkea-exceptions

mr_provisioner.so: plug.os plug_messages.os
	$(CXX) -shared $(CXXFLAGS) $(LIBADD) $^ -o $@ $(LDFLAGS_CURL)

plug.os: plug.cc plug_messages.h
	$(CXX) $(CXXFLAGS) -o $@ -c $(WARNFLAGS) -O2 -g3 -fPIC $(INCS) $<

plug_messages.os: plug_messages.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $(WARNFLAGS) -O2 -g3 -fPIC $(INCS) $<

plug_messages.cc: plug_messages.mes
	$(KEA_MSG_COMPILER) $<

plug_messages.h: plug_messages.cc
	# created by dep

clean:
	rm -f plug_messages.h plug_messages.cc
	rm -f plug_messages.os plug.os
	rm -f mr_provisioner.so
