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


#ifndef LIB_STATS_D_H
#define LIB_STATS_D_H

#define ADDAPI
#define ADDCALL

#if defined (_WIN32)
   //Windows Specific includes
   #include <winsock2.h>
   #include <ws2tcpip.h>

   //Undefine the macros, and redefine them to perform
   //windows specific tasks
   #undef ADDAPI
   #undef ADDCALL

   #ifdef ADD_EXPORTS
      #define ADDAPI __declspec(dllexport)
   #else
      #define ADDAPI __declspec(dllimport)
   #endif

   #define ADDCALL __cdecl

#elif defined (__linux__) || defined (__unix__) || defined (__APPLE__)
   //Unix specific includes
   #include <sys/socket.h>
   #include <arpa/inet.h>
   #include <netinet/in.h>

#endif //end of windows/unix/linux OS stuff

#define STATSD_PORT 8125
#define NO_SAMPLE_RATE 0
#ifndef BATCH_MAX_SIZE
#define BATCH_MAX_SIZE 512
#endif

typedef struct _statsd_t {
   const char* serverAddress;
   char ipAddress[128];
   const char* nameSpace;
   const char* bucket;
   int port;
   int socketFd;
   struct sockaddr_in destination;
   
   int (*random)(void);

   char batch[BATCH_MAX_SIZE];
   int batchIndex;
} Statsd;

typedef enum {
   STATSD_NONE = 0,
   STATSD_COUNT,
   STATSD_GAUGE,
   STATSD_SET,
   STATSD_TIMING,
   STATSD_BATCH
} StatsType;

typedef enum {
   STATSD_SUCCESS = 0,
   STATSD_BAD_BUCKET,
   STATSD_SOCKET,
   STATSD_SOCKET_INIT,
   STATSD_NTOP,
   STATSD_MALLOC,
   STATSD_BAD_SERVER_ADDRESS,
   STATSD_UDP_SEND,
   STATSD_BATCH_IN_PROGRESS,
   STATSD_NO_BATCH,
   STATSD_BATCH_FULL,
   STATSD_BAD_STATS_TYPE
} StatsError;

#ifdef __cplusplus
extern "C" {
#endif

ADDAPI int ADDCALL statsd_new(Statsd **stats, const char* serverAddress, int port, const char* nameSpace, const char* bucket);
ADDAPI void ADDCALL statsd_free(Statsd* statsd);
ADDAPI void ADDCALL statsd_release(Statsd* statsd);
ADDAPI int ADDCALL statsd_init(Statsd* statsd, const char* server, int port, const char* nameSpace, const char* bucket);
ADDAPI int ADDCALL statsd_increment(Statsd* statsd, const char* bucket);
ADDAPI int ADDCALL statsd_decrement(Statsd* statsd, const char* bucket);
ADDAPI int ADDCALL statsd_count(Statsd* statsd, const char* bucket, int count, double sampleRate);
ADDAPI int ADDCALL statsd_gauge(Statsd* statsd, const char* bucket, int value, double sampleRate);
ADDAPI int ADDCALL statsd_set(Statsd* statsd, const char* bucket, int value, double sampleRate);
ADDAPI int ADDCALL statsd_timing(Statsd* statsd, const char* bucket, int timing, double sampleRate);
ADDAPI int ADDCALL statsd_resetBatch(Statsd* statsd);
ADDAPI int ADDCALL statsd_addToBatch(Statsd* statsd, StatsType type, const char* bucket, int value, double sampleRate);
ADDAPI int ADDCALL statsd_sendBatch(Statsd* statsd);

#ifdef __cplusplus
}
#endif

#endif //LIB_STATS_D_H
