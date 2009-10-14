/* 
 * Copyright (C) 2001  Till Adam
 * Authors: Till Adam <till@adam-lilienthal.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "directvnc.h"
#include "getopt.h"
#include <unistd.h>

dfb_vnc_options opt;
static void show_usage_and_exit();
static void show_version();
static void _parse_options_array(int argc, char **argv);

int
args_parse(int argc, char **argv)
{
   char *buf;
  
   if (argc <= 1)
      show_usage_and_exit();

   
   /* servername and display like so: 192.168.0.1:1 or so: localhost:2 */
   buf = argv[1];

   buf = strtok (buf, ":");
   opt.servername = strdup(buf);
   if ( (buf = strtok(NULL, "")) )
	 opt.port = atoi(buf);
   else
      printf("You did not specify a display to connect to. I will assume 0.\n");
    
   /* set some default values */
   opt.client.width = 1024;
   opt.client.height = 768;
   
   opt.client.bpp = 16;
   opt.client.depth = 16;
   opt.client.bigendian = 0;
   opt.client.truecolour = 1;
   opt.client.redmax = 31;
   opt.client.greenmax = 63;
   opt.client.bluemax = 31;
   opt.client.redshift =11;
   opt.client.greenshift = 5;
   opt.client.blueshift = 0;
   opt.client.compresslevel = 99;
   opt.client.quality = 99;

   opt.shared = 1;
   opt.localcursor = 1;
   opt.poll_freq = 50;

   opt.h_ratio = 1;
   opt.v_ratio = 1;
   opt.h_offset = 0;
   opt.v_offset = 0;

#ifdef DEBUG
   printf("server: %s\n", opt.servername);
   printf("port: %d\n", opt.port);
#endif

   /* now go and parse the command line */
   _parse_options_array(argc, argv);
   
   return 1;
}

static void
_parse_options_array(int argc, char **argv) 
{
   static char stropts[] = "hvob:p:e:c:q:snlf:";
   static struct option lopts[] = {
      /* actions */
      {"help", 0, 0, 'h'},
      {"version", 0, 0, 'v'},
      /* options */
      {"bpp", 1, 0, 'b'},
      {"compresslevel", 1, 0, 'c'},
      {"quality", 1, 0, 'q'},
      {"password", 1, 0, 'p'},
      {"encodings", 1, 0, 'e'},
      {"shared", 0, 0, 's'},
      {"noshared", 0, 0, 'n'},
      {"nolocalcursor", 0, 0, 'l'},
      {"pollfrequency", 1, 0, 'f'},
      {0, 0, 0, 0}
   };
   int optch = 0, cmdx = 0;
   int bpp = 0;
   int compresslevel = 0;
   int quality = 0;

   /* Now to parse some optionarinos */
   while ((optch = getopt_long_only(argc, argv, stropts, lopts, &cmdx)) != EOF)
   {
      switch (optch)
      {
	 case 0:
	    break;
	 case 'h':
	    show_usage_and_exit();
	    break;
	 case 'v':
            show_version();
	    exit(1);
	    break;
	 case 'b':
	    bpp = atoi(optarg);
	    switch (bpp) {
	       case 16:
		  opt.client.bpp = bpp;
		  break;
	       case 8:
	       case 24:
	       case 32:
	       default:
		  fprintf(stderr, "Depth currently not supported!\n");
		  exit(-1);
	    }
	 case 'f':
	    opt.poll_freq = atoi(optarg);
	    break;
	 case 'p':
	    opt.password = strdup(optarg);
	    break;
	 case 'e':
	    opt.encodings = strdup(optarg);
	    break;   
	 case 's':
	    opt.shared = 1;
	    break;
	 case 'n':
	    opt.shared = 0;
	    break;
	 case 'l':
	    opt.localcursor = 0;
	    break;

	 case 'c':
	    compresslevel = atoi(optarg);
	    if (compresslevel >=0 && compresslevel <= 9)
	       opt.client.compresslevel = compresslevel;
	    break;
	 case 'q':
	    quality = atoi(optarg);
	    if (quality >=0 && quality <= 9)
	       opt.client.quality = quality;
	    break;


      }
   }
}

static void
show_usage_and_exit()
{

   fprintf(stderr,"\n"
 "DirectVNC viewer version %s\n"
 "\n"
 "usage: directvnc <host>:<display#> [<options>]\n"
 "\n"
 "with options being:\n"
 "  -h, --help                 Show this and exit.\n"
 "  -v, --version              Show version information and exit\n"
 "  -b, --bpp NUM              Set the clients bit per pixel to NUM\n"
 "  -p, --password STRING      Password for the server\n"
 "  -e, --encodings \"STRING\" list of encodings to be used in order of\n"
 "                             preference (e.g. \"hextile copyrect\")\n"
 "  -f, --pollfrequency MS     time between checks for events in ms \n"
 "  -s, --shared (default)     Don't disonnect already connected clients\n"
 "  -n, --noshared             Disconnect already connected clients\n"
 "  -l, --nolocalcursor        Disable local cursor handling.\n"
 "  -c, --compresslevel        0..9 compression level to be used by zlib\n"
 "  -q, --quality              0..9 quality level to be used by jpeg compression in tight encoding\n"
 , VERSION);
   exit(1);
}

static void
show_version()
{
   fprintf(stderr, "This is version %s of DirectVNC\n", VERSION);
}

