/*************************************************************************************
Copyright (C) 2012, 2013 James Slocum

Permission is hereby granted, free of charge, to any person obtaining a copy of this 
software and associated documentation files (the "Software"), to deal in the Software 
without restriction, including without limitation the rights to use, copy, modify, 
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to 
permit persons to whom the Software is furnished to do so, subject to the following 
conditions:

The above copyright notice and this permission notice shall be included in all copies 
or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE 
OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <config.h>
#include "statsd.h"

#define STRING_MATCH 0

#if defined (_WIN32)
#pragma comment (lib, "Ws2_32.lib")
#endif

static char* serverAddress = NULL;
static int port = STATSD_PORT;
static char* prefix = NULL;
static char* bucket = NULL;
static int type = STATSD_NONE;
static int value = 0;
static double samplerate = 0.0;

static bool isDigit(const char* str){
   if (str[0] >= '0' && str[0] <= '9'){
      return true;
   }
   
   return false;
}

static void usageAndExit(char* prog, FILE* where, int returnCode){
   fprintf(where, "%s version %s [%s]\n", prog, VERSION, PACKAGE_BUGREPORT);
   fprintf(where, "  usage:\n");
   fprintf(where, "  -h --help : print this help message\n");
   fprintf(where, "  -s --server : specify the server name or ip address\n");
   fprintf(where, "  -p --port : specify the port (default = 8125)\n");
   fprintf(where, "  -n --namespace : specify the bucket namespace\n");
   fprintf(where, "  -b --bucket : specify the bucket\n");
   fprintf(where, "  -t --type : specify the stat type\n");
   fprintf(where, "    types: count, set, gauge, timing\n");
   fprintf(where, "  -r --rate : specify the sample rate\n");
   fprintf(where, "example:\n");
   fprintf(where, "  %s -s statsd.example.com -n some.statsd -b counts -t count 25\n", prog);

   exit(returnCode);
}

static void parseCommandLine(int argc, char* argv[]){
   for (int i = 1; i < argc; i++){
      if (strcmp(argv[i], "-s") == STRING_MATCH || strcmp(argv[i], "--server") == STRING_MATCH){
         serverAddress = argv[i+1];
         i++;
      }
      else if (strcmp(argv[i], "-p") == STRING_MATCH || strcmp(argv[i], "--port") == STRING_MATCH){
         port = atoi(argv[i+1]);
         i++;
      }
      else if (strcmp(argv[i], "-n") == STRING_MATCH || strcmp(argv[i], "--namespace") == STRING_MATCH){
         prefix = argv[i+1];
         i++;
      }
      else if (strcmp(argv[i], "-b") == STRING_MATCH || strcmp(argv[i], "--bucket") == STRING_MATCH){
         bucket = argv[i+1];
         i++;
      }
      else if (strcmp(argv[i], "-t") == STRING_MATCH || strcmp(argv[i], "--type") == STRING_MATCH){
         char* typeStr = argv[i+1];
         if (strcmp(typeStr, "count") == STRING_MATCH){
            type = STATSD_COUNT;
            i++;
         }
         else if (strcmp(typeStr, "gauge") == STRING_MATCH){
            type = STATSD_GAUGE;
            i++;
         }
         else if (strcmp(typeStr, "set") == STRING_MATCH){
            type = STATSD_SET;
            i++;
         }
         else if (strcmp(typeStr, "timing") == STRING_MATCH){
            type = STATSD_TIMING;
            i++;
         }
         else {
            usageAndExit(argv[0], stderr, 1);
         }
      }
      else if (strcmp(argv[i], "-h") == STRING_MATCH || strcmp(argv[i], "--help") == STRING_MATCH) {
         usageAndExit(argv[0], stdout, EXIT_SUCCESS);
      }
      else if (strcmp(argv[i], "-r") == STRING_MATCH || strcmp(argv[i], "--rate") == STRING_MATCH) {
         samplerate = strtod(argv[i+1], NULL);
         i++;
      }
      else if (isDigit(argv[i])){
         value = atoi(argv[i]);
      }
      else {
         usageAndExit(argv[0], stderr, 1);
      }
   }
}

int main(int argc, char* argv[]){
#if defined (_WIN32)
   //Initialize the windows socket library
   WSADATA wsaData = {0};

   if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
      return STATSD_SOCKET_INIT;
   }
#endif
   
   //Parse the command line
   parseCommandLine(argc, argv); 
   
   srand(time(NULL));

   if (prefix == NULL && bucket == NULL){
      fprintf(stderr, "You must specify a bucket name!\n");
      usageAndExit(argv[0], stderr, 1);
   }

   if (serverAddress == NULL){
      fprintf(stderr, "You must specify a server address\n");
      usageAndExit(argv[0], stderr, 1);
   }

   if (type == STATSD_NONE){
      fprintf(stderr, "You must specify a stat type\n");
      usageAndExit(argv[0], stderr, 1);
   }

   //Now we know we have all of the necesary parameters, open the 
   //UDP socket and create the statsd client object.
   Statsd* stats = NULL;
   int ret = statsd_new(&stats, serverAddress, port, prefix, bucket);

   if (ret != STATSD_SUCCESS){
      fprintf(stderr, "Unable to create statsd object (%d)\n", ret);
      return 1;
   }

   switch(type){
      case STATSD_COUNT:
         ret = statsd_count(stats, NULL, value, samplerate);
         break;
      case STATSD_SET:
         ret = statsd_set(stats, NULL, value, samplerate);
         break;
      case STATSD_GAUGE:
         ret = statsd_gauge(stats, NULL, value, samplerate);
         break;
      case STATSD_TIMING:
         ret = statsd_timing(stats, NULL, value, samplerate);
         break;
      default:
         //Don't know how I got here...
         return 2;
   }

   if (ret != STATSD_SUCCESS){
      fprintf(stderr, "Error sending stat to server (%d)\n", ret);
      return 1;
   }

   statsd_free(stats);

#if defined (_WIN32)
   WSACleanup();  
#endif

   return EXIT_SUCCESS;
}
