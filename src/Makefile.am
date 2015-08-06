# $Id$
#
# This file is part of PulseAudio Volume Meter.
#
# PulseAudio Volume Meter is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# PulseAudio Volume Meter is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with PulseAudio Volume Meter; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

bin_PROGRAMS=pavumeterc

pavumeterc_SOURCES=vumeter.cc

pavumeterc_LDADD=$(AM_LDADD) $(GUILIBS_LIBS) $(PULSE_LIBS)
pavumeterc_CXXFLAGS=$(AM_CXXFLAGS) $(GUILIBS_CFLAGS) $(PULSE_CFLAGS) 
