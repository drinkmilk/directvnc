## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4 foreign

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in

man_MANS = directvnc.1 directvnc-xmapconv.1 directvnc-kbmapping.7

docs_DATA = README AUTHORS changelog
docsdir = $(prefix)/share/doc/@PACKAGE@

EXTRA_DIST = \
$(man_MANS) \
$(docs_DATA) \
directvnc.spec

SUBDIRS = src
