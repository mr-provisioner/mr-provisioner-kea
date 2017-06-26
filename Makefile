KEA_SRC?=/usr/src/kea-1.2.0
KEA_PREFIX?=/usr/local

KEA_LIB_DIR?=$(KEA_PREFIX)/lib
KEA_MSG_COMPILER?=$(KEA_PREFIX)/bin/kea-msg-compiler
KEA_SRC_LIB_DIR?=$(KEA_SRC)/src/lib
INSTALL_DIR?=$(KEA_LIB_DIR)

BOOST_INCS?=
INCS= -I$(KEA_SRC_LIB_DIR) $(BOOST_INCS)
CXX?=g++

SONAME=libkea-hook-mr-provisioner.so

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

$(SONAME): plug.os plug_messages.os
	$(CXX) -shared $(CXXFLAGS) $(LIBADD) $^ -o $@ $(LDFLAGS_CURL)

plug.os: plug.cc plug_messages.h
	$(CXX) $(CXXFLAGS) -o $@ -c $(WARNFLAGS) -O2 -g3 -fPIC $(INCS) $<

plug_messages.os: plug_messages.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $(WARNFLAGS) -O2 -g3 -fPIC $(INCS) $<

plug_messages.cc: plug_messages.mes
	$(KEA_MSG_COMPILER) $<

plug_messages.h: plug_messages.cc
	# created by dep

.PHONY: clean
clean:
	rm -f plug_messages.h plug_messages.cc
	rm -f plug_messages.os plug.os
	rm -f $(SONAME)

.PHONY: install
install: $(SONAME)
	install -m 0755 $(SONAME) $(INSTALL_DIR)
