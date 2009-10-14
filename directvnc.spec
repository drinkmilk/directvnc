%define name directvnc 
%define version 0.7.5
%define release 1

Summary: VNC client for the GNU/Linux framebuffer device using the DirectFB library.
Name: %{name}
Version: %{version}
Release: %{release}
Source: http://www.adam-lilienthal.de/directvnc/download/%{name}-%{version}.tar.gz
Url: http://www.adam-lilienthal.de/directvnc/
License: GPL
Group: Networking/Remote Access
BuildRoot: %{_tmppath}/%{name}-buildroot
Prefix: %{_prefix}
Requires: libdirectfb >= 0.9.16

%description
DirectVNC is a client implementing the remote framebuffer protocol (rfb)
which is used by VNC servers. If a VNC server is running on a machine you
can connect to it using this client and have the contents of its display
shown on your screen. Keyboard and mouse events are sent to the server, so
you can basically control a VNC server remotely. There are servers (and
other clients) freely available for all operating systems. To find out more
about VNC check out its home on the web at AT&T labs. 

www.uk.research.att.com/vnc/

What makes DirectVNC different from other unix vnc clients is that it uses
the linux framebuffer device through the DirectFB library which enables it
to run on anything that has a framebuffer without the need for a running X
server. This includes embedded devices. DirectFB even uses acceleration
features of certain graphics cards. Find out all about DirectFB here:

www.directfb.org

DirectVNC basically provides a very thin VNC client for unix framebuffer
systems.

%prep
rm -rf $RPM_BUILD_ROOT 
%setup

%build
%configure
make

%install
%makeinstall


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,755)
%doc AUTHORS ChangeLog README TODO
%{_mandir}/man1/directvnc.1*
%{_bindir}/directvnc



%changelog
* Sat Dec 08 2001  Till Adam  <till@adam-lilienthal.de>
- initial spec file


# end of file
