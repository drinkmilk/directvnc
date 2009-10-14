## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4 foreign

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in

man_MANS = directvnc.1

docs_DATA = README AUTHORS ChangeLog TODO
docsdir = $(prefix)/doc/@PACKAGE@

EXTRA_DIST = \
directvnc.1 $(docs_DATA)\
directvnc.spec

SUBDIRS = src
