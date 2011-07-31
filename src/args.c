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
 * along with this program; if not, a copy can be downloaded from 
 * http://www.gnu.org/licenses/gpl.html, or obtained by writing to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA 02110-1301, USA.
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
   fprintf(stderr, "server: %s\n", opt.servername);
   fprintf(stderr, "port: %d\n", opt.port);
#endif

   /* now go and parse the command line */
   _parse_options_array(argc, argv);
   
   return 1;
}

static void
_parse_options_array(int argc, char **argv) 
{
   static char sopts[] = {
       /* actions */
       'h',
       'v',

       /* options */
       'b', ':',
       'c', ':',
       'q', ':',
       'p', ':',
       'e', ':',
       's',
       'n',
       'l',
       'f', ':',
       'm', ':',

       0
   };

   static struct option lopts[] = {
      /* actions */
      {"help",           0, NULL, 'h'},
      {"version",        0, NULL, 'v'},

      /* options */
      {"bpp",            1, NULL, 'b'},
      {"compresslevel",  1, NULL, 'c'},
      {"quality",        1, NULL, 'q'},
      {"password",       1, NULL, 'p'},
      {"encodings",      1, NULL, 'e'},
      {"shared",         0, NULL, 's'},
      {"noshared",       0, NULL, 'n'},
      {"nolocalcursor",  0, NULL, 'l'},
      {"pollfrequency",  1, NULL, 'f'},
      {"modmap",         1, NULL, 'm'},

      {0, 0, 0, 0}
   };
   int optch = 0, opti = 0;

   /* Now to parse some optionarinos */
   while ((optch = getopt_long_only(argc, argv, sopts, lopts, &opti)) != -1)
   {
      int intarg = 0;
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
	    intarg = atoi(optarg);
	    switch (intarg) {
	       case 24:
	          opt.client.bpp=32;
                  opt.client.depth=intarg;
		  opt.client.redmax=255;
		  opt.client.bluemax=255;
		  opt.client.greenmax=255;
		  opt.client.redshift=16;
		  opt.client.greenshift=8;
		  opt.client.blueshift=0;
	          break;
	       case 16:
		  opt.client.bpp = intarg;
		  break;
	       case 8:
	       case 32:
	       default:
		  fprintf(stderr, "Depth currently not supported!\n");
		  exit(-1);
	    }
	    break;
	 case 'f':
	    opt.poll_freq = atoi(optarg);
	    break;
	 case 'p':
	    opt.password = strdup(optarg);
	    break;
	 case 'e':
	    opt.encodings = strdup(optarg);
	    break;   
	 case 'm':
	    opt.modmapfile = strdup(optarg);
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
	    intarg = atoi(optarg);
	    if (intarg >= 0 && intarg <= 9) {
	       opt.client.compresslevel = intarg;
	    } else {
	       fprintf(stderr, "Invalid compression level: %s\n", optarg);
	       exit(-2);
	    }
	    break;
	 case 'q':
	    intarg = atoi(optarg);
	    if (intarg >= 0 && intarg <= 9) {
	       opt.client.quality = intarg;
	    } else {
	       fprintf(stderr, "Invalid quality level: %s\n", optarg);
	       exit(-2);
	    }
	    break;

      }
   }
}

static void
show_usage_and_exit()
{

   fprintf(stderr, "\n"
      "DirectVNC viewer version %s\n"
      "\n"
      "Usage: directvnc <server>[:<display>] [<options>]\n"
      "\n"
      "with\n"
      "  server                     "   "VNC host to connect to.\n"
      "  display                    "   "Optional display number (default: 0).\n"
      "\n"
      "and options being:\n"
     /*0        1         2         */ /*3         4         5         6         7        */
     /*12345678901234567890123456789*/ /*0123456789012345678901234567890123456789012345678*/
      "  -p, --password STRING      "   "Password for the server.\n"
      "  -b, --bpp NUM              "   "Set the clients bit per pixel to NUM.\n"
      "  -f, --pollfrequency MS     "   "Time between checks for events in milliseconds.\n"
      "  -l, --nolocalcursor        "   "Disable local cursor handling.\n"
      "  -s, --shared               "   "Don't disonnect already connected clients.\n"
      "  -n, --noshared             "   "Disconnect already connected clients.\n"
      "  -e, --encodings \"STRING\"   " "List of encodings to be used in order of\n"
      "                             "   "preference (e.g. \"hextile copyrect\").\n"
      "  -c, --compresslevel LEVEL  "   "Compression level (0..9) to be used by zlib.\n"
      "  -q, --quality LEVEL        "   "Quality level (0..9) to be used by jpeg\n"
      "                             "   "compression in tight encoding.\n"
      "  -m, --modmap STRING        "   "Path to the modmap (subset of X-style) file to load\n"
      "  -h, --help                 "   "Show this text and exit.\n"
      "  -v, --version              "   "Show version information and exit.\n"
      "\n"
        , VERSION
   );
   exit(1);
}

static void
show_version()
{
   fprintf(stderr, "This is version %s of DirectVNC\n", VERSION);
}
