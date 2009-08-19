#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Josh Aas <josh@mozilla.com>
#  Vaughn Cato <vaughn_cato@yahoo.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# Debugging
#OPTIMIZATION=-g

# Release
OPTIMIZATION=-O2 -DNDEBUG -fexpensive-optimizations -fomit-frame-pointer -frename-registers -fvisibility-inlines-hidden
PRE_OPTIMIZATION=-march=armv4t -mtune=arm920t

#CXXFLAGS = -MD -Wall -DXP_UNIX=1 -fPIC $(OPTIMIZATION) `pkg-config --cflags glib-2.0`
CXXFLAGS = -MD -Wall -DXP_UNIX=1 -fPIC $(OPTIMIZATION)
CFLAGS = $(CXXFLAGS)
#LDFLAGS = `pkg-config --libs glib-2.0`
LDFLAGS =
CC = g++

ifeq ($(PRE_DEV_PREFIX),)
	PRE_DEV_DIR:=$(shell [ -d /usr/local/arm-2009q1 ] && echo /usr/local/arm-2009q1 || echo /pre/optware/cs08q1armel/toolchain/arm-2008q1)
	PRE_DEV_PREFIX=$(PRE_DEV_DIR)/bin/arm-none-linux-gnueabi-
endif
PRE_CC = $(PRE_DEV_PREFIX)g++

PRE_STRIP = $(PRE_DEV_PREFIX)strip
PRE_CXXFLAGS = -Wall -DXP_UNIX=1 -fPIC $(OPTIMIZATION) $(PRE_OPTIMIZATION) -I$(PRE_DEV_DIR)/include
PRE_CFLAGS = $(PRE_CXXFLAGS)
PRE_LDFLAGS = $(OPTIMIZATION) $(PRE_OPTIMIZATION)
PRE_OUT_DIR=pre-out

EMHOST = localhost
EMPORT = 5522
PLUGIN=termplugin.so
PRE_PLUGIN=$(PRE_OUT_DIR)/termplugin.so

OBJS=screen.o \
keyman.o termstate.o spawndata.o spawn.o pty.o charbuffer.o pixelbuffer.o termplugin.o fontinfo.o api.o
PRE_OBJS = \
	$(PRE_OUT_DIR)/keyman.o \
	$(PRE_OUT_DIR)/termstate.o \
	$(PRE_OUT_DIR)/spawndata.o \
	$(PRE_OUT_DIR)/spawn.o \
	$(PRE_OUT_DIR)/pty.o \
	$(PRE_OUT_DIR)/charbuffer.o \
	$(PRE_OUT_DIR)/pixelbuffer.o \
	$(PRE_OUT_DIR)/termplugin.o \
	$(PRE_OUT_DIR)/fontinfo.o \
	$(PRE_OUT_DIR)/api.o \

all : $(PLUGIN)

pre:	$(PRE_OUT_DIR) $(PRE_PLUGIN)

$(PLUGIN): $(OBJS)
	$(CC)  $(LDFLAGS) -shared $(OBJS) -o $(PLUGIN)

install : $(PLUGIN)
	scp -P $(EMPORT) $(PLUGIN) root@$(EMHOST):/usr/lib/BrowserPlugins/; echo done

put: spotless
	(cd ..;tar zcvf termplugin.tar.gz termplugin;novaterm -d 8e8e9807af8dd375c9fc5814f5442b3804e1e2dd put file:///media/internal/developer/termplugin/termplugin.tar.gz <termplugin.tar.gz)

get:
	novaterm -d 8e8e9807af8dd375c9fc5814f5442b3804e1e2dd get file:////usr/lib/BrowserPlugins/termplugin.so >pre/termplugin.so
	(cd pre;tar zcvf ../termplugin_pre_so.tar.gz termplugin.so)

clean :
	rm -f $(PLUGIN) $(OBJS) $(PRE_OBJS) *.d
	touch dummy.d
spotless: clean
	rm -f $(PRE_PLUGIN)
	( [ -d $(PRE_OUT_DIR) ] && rmdir -p $(PRE_OUT_DIR) || true )

include *.d

$(PRE_PLUGIN): $(PRE_OBJS)
	$(PRE_CC) $(PRE_LDFLAGS) -shared $(PRE_OBJS) -o $@

$(PRE_OUT_DIR):
	mkdir -p $(PRE_OUT_DIR)

$(PRE_OUT_DIR)/keyman.o : keyman.cpp
	$(PRE_CC) $(PRE_CXXFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/termstate.o : termstate.c
	$(PRE_CC) $(PRE_CFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/spawndata.o : spawndata.c
	$(PRE_CC) $(PRE_CFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/spawn.o : spawn.c
	$(PRE_CC) $(PRE_CFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/pty.o : pty.c
	$(PRE_CC) $(PRE_CFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/api.o : api.c
	$(PRE_CC) $(PRE_CFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/charbuffer.o : charbuffer.cpp
	$(PRE_CC) $(PRE_CXXFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/pixelbuffer.o : pixelbuffer.cpp
	$(PRE_CC) $(PRE_CXXFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/termplugin.o : termplugin.cpp
	$(PRE_CC) $(PRE_CXXFLAGS) -c $< -o $@
$(PRE_OUT_DIR)/fontinfo.o : fontinfo.cpp
	$(PRE_CC) $(PRE_CXXFLAGS) -c $< -o $@
