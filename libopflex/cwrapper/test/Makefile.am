#
# libopflex: a framework for developing opflex-based policy agents
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with automake to produce a Makefile.in

AM_CPPFLAGS = $(BOOST_CPPFLAGS) -DBOOST_TEST_DYN_LINK \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/modb/test 
AM_LDFLAGS = $(BOOST_LDFLAGS)

TESTS = cwrapper_test
cwrapper_test_SOURCES = \
	main.cpp \
	cwrapper_test.cpp

cwrapper_test_LDADD = ../libcwrapper.la \
	../../ofcore/libcore.la \
	../../engine/libengine.la \
	../../modb/libmodb.la \
	../../util/libutil.la \
	../../logging/liblogging.la \
	$(BOOST_UNIT_TEST_FRAMEWORK_LIB)

noinst_PROGRAMS = $(TESTS)