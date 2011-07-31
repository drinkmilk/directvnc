# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in				

#DEBUGFLAGS	  = -DDEBUG -DDEBUG_NEST

AM_LDFLAGS           = @DIRECTFB_LIBS@ 
AM_CFLAGS            = -Wall @DIRECTFB_CFLAGS@ $(DEBUGFLAGS)

LIBOBJS = @LIBOBJS@

bin_PROGRAMS      = directvnc
directvnc_SOURCES       = main.c debug.h dfb.c directvnc.h sockets.c args.c \
		       rfb.c getopt.c getopt1.c getopt.h \
		       d3des.c d3des.h vncauth.c vncauth.h jpeg.c jpeg.h \
		       tight.c tight.h rfbproto.h keysym.h \
		       cursor.c modmap.c

bin_SCRIPTS = directvnc-xmapconv

# setuid root. Is this really necessary? I cant access my framebuffer
# otherwise.
# 
# install-exec-hook:
# 	chown root $(bindir)/directvnc
# 	chmod u+s $(bindir)/directvnc
# 
		
