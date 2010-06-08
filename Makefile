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
OPTIMIZATION=-DNDEBUG -O2 -fexpensive-optimizations -fomit-frame-pointer -frename-registers -fvisibility-inlines-hidden

CXXFLAGS = -MD -Wall -DXP_UNIX=1 -fPIC $(OPTIMIZATION)
LDFLAGS =
CC = g++
CXX = ${CC}

EMHOST = localhost
EMPORT = 5522
PLUGIN=termplugin.so

OBJS=screen.o keyman.o termstate.o spawndata.o spawn.o pty.o charbuffer.o pixelbuffer.o termplugin.o fontinfo.o api.o

all : $(PLUGIN)

$(PLUGIN): $(OBJS)
	$(CXX) $(LDFLAGS) -shared $(OBJS) -o $(PLUGIN)

install : $(PLUGIN)
	scp -P $(EMPORT) $(PLUGIN) root@$(EMHOST):/usr/lib/BrowserPlugins/; echo done

put: spotless
	(cd ..;tar zcvf termplugin.tar.gz termplugin;novaterm -d 8e8e9807af8dd375c9fc5814f5442b3804e1e2dd put file:///media/internal/developer/termplugin/termplugin.tar.gz <termplugin.tar.gz)

get:
	novaterm -d 8e8e9807af8dd375c9fc5814f5442b3804e1e2dd get file:////usr/lib/BrowserPlugins/termplugin.so >pre/termplugin.so
	(cd pre;tar zcvf ../termplugin_pre_so.tar.gz termplugin.so)

clean :
	rm -f $(PLUGIN) $(OBJS)
