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
#include <errno.h>

#if defined (_WIN32)
   #include <windows.h>
   #include <winsock2.h>
   #include <winsock.h>
   #include <ws2tcpip.h>
#else
   #include <unistd.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <arpa/inet.h>
   #include <netinet/in.h>
   #include <netdb.h>
#endif

#include "statsd.h"

//Define the private functions
static const char *networkToPresentation(int af, const void *src, char *dst, size_t size);
static int sendToServer(Statsd* stats, const char* bucket, StatsType type, int delta, double sampleRate);
static int buildStatString(char* stat, const char* nameSpace, const char* bucket, StatsType type, int delta, double sampleRate);

static const char *networkToPresentation(int af, const void *src, char *dst, size_t size){
   return inet_ntop(af, src, dst, size);
}

/**
   This is a helper function that will do the dirty work of sending
   the stats to the statsd server. 

   @param[in] stats - The stats client object
   @param[in] bucket - The optional bucket name, if this is null then
      the defualt bucket name is used from the stats object.
   @param[in] type - The type of stat being sent
   @param[in] delta - The value to send
   @param[in] sampleRate - The sample rate of this stat. If the value is
      less then or equal to 0, or greater then or equal to 1, it is ignored.

   @return STATSD_SUCCESS on success, STATSD_BAD_STATS_TYPE if the type is 
      not recognized. STATSD_UDP_SEND if the sendto() failed.
*/
static int sendToServer(Statsd* stats, const char* bucket, StatsType type, int delta, double sampleRate){
   //See if we randomly fall under the sample rate
   if (sampleRate > 0 && sampleRate < 1 && (double)((double)stats->random() / RAND_MAX) >= sampleRate){
      return STATSD_SUCCESS;
   }
 
   int dataLength = 0;
   char data[256];
   memset(&data, 0, 256);

  
   //If the user has not specified a bucket, we will use the defualt
   //bucket instead.
   if (!bucket){
      bucket = stats->bucket;
   }
   
   dataLength = buildStatString(data, stats->nameSpace, bucket, type, delta, sampleRate);

   if (dataLength < 0){
      return -dataLength;
   }

   //Send the packet
   int sent = sendto(stats->socketFd, data, dataLength, 0, (const struct sockaddr*)&stats->destination, sizeof(struct sockaddr_in));

   if (sent == -1){
      return STATSD_UDP_SEND;
   }

   return STATSD_SUCCESS;
}

/**
   This is a helper function that will build up a stats string and return its
   length. 

   @param[in,out] stat - This is where the final string will be placed
   @param[in] nameSpace - The namespace of the stat
   @param[in] bucket - The bucket where to put the stat
   @param[in] type - The type of stat being packed
   @param[in] delta - The value of the stat
   @param[in] sampleRate - The intervals at which this data was gathered

   @return The length of the stat string, or -1 on error
*/
static int buildStatString(char* stat, const char* nameSpace, const char* bucket, StatsType type, int delta, double sampleRate){
   char* statType = NULL;
   char bucketName [128];
   int statLength = 0;

   //Build up the bucket name, with the nameSpace.
   if (nameSpace){
      sprintf(bucketName, "%s.%s", nameSpace, bucket);
   }
   else {
      sprintf(bucketName, "%s", bucket);
   }

   //Figure out what type of message to generate
   switch(type){
      case STATSD_COUNT:
         statType = "c";
         break;
      case STATSD_GAUGE:
         statType = "g";
         break;
      case STATSD_SET:
         statType = "s";
         break;
      case STATSD_TIMING:
         statType = "ms";
         break;
      default:
         return -STATSD_BAD_STATS_TYPE;
   }

   //Do we have a sample rate?
   if (sampleRate > 0.0 && sampleRate < 1.0){
      sprintf(stat, "%s:%d|%s|@%.2f", bucketName, delta, statType, sampleRate);
   }
   else {
      sprintf(stat, "%s:%d|%s", bucketName, delta, statType);
   }

   statLength = strlen(stat);
   return statLength;
}


//Implement the public functions

/**
   This is allocate the memory for a statsd object from the heap
   initialize it with the givin values and return the newly constructed
   object ready for use. 

   @param[out] stats - This is where the statsd object will be placed.
   @param[in] serverAddress - The hostname, or ip address of the statsd server
   @param[in] port - The port number to use for statsd packets
   @param[in] bucket - The unique bucket name to use for reporting stats

   @return STATSD_SUCCESS on success, or an error if something went wrong
   @see StatsError
*/
int ADDCALL statsd_new(Statsd** stats, const char* serverAddress, int port, const char* nameSpace, const char* bucket){
   Statsd* newStats = (Statsd*)malloc(sizeof(Statsd));
   if (!newStats){
      return STATSD_MALLOC;
   }

   memset(newStats, 0, sizeof(Statsd));
   newStats->socketFd = -1;
   *stats = newStats;
   return statsd_init(*stats, serverAddress, port, nameSpace, bucket);
}

/**
   Free a statsd object created through a call to statsd_new()

   @param[in] statsd - The statsd object created by a call to
      statsd_new(). 
*/
void ADDCALL statsd_free(Statsd* statsd){
   if(!statsd)
      return;

   statsd_release(statsd);
   free(statsd);
}

/**
   Free the resources in a statsd object initialized through a
      call to statsd_init()

   @param[in] statsd - The statsd object initialized by a call
      to statsd_init().
*/
void ADDCALL statsd_release(Statsd* statsd){
   if(!statsd)
      return;

   if (statsd->socketFd > 0){
      close(statsd->socketFd);
      statsd->socketFd = -1;
   }
}

/**
   This will initialize (or reinitialize) a statsd object that
   has already been created by a call to statsd_new() or has been
   allocated statically on the stack. 

   @param[in,out] statsd - A previously allocated statsd object
   @param[in] server - The hostname or ip address of the server
   @param[in] port - The port number that the packets will be sent to
   @param[in] bucket - The default bucket name that will be used for 
      the stats

   @return SCTE_SUCCESS on success, or an error otherwise
   @see StatsError
*/
int ADDCALL statsd_init(Statsd* statsd, const char* server, int port, const char* nameSpace, const char* bucket){
   //Do a DNS lookup (or IP address conversion) for the serverAddress
   struct addrinfo hints, *result = NULL;
   memset(&hints, 0, sizeof(hints));

   //Set the hints to narrow downs the DNS entry we want
   hints.ai_family = AF_INET;

   int addrinfoStatus = getaddrinfo(server, NULL, &hints, &result);
   if (addrinfoStatus != 0){
      return STATSD_BAD_SERVER_ADDRESS;
   }

   //Copy the result into the UDP destination socket structure
   memcpy(&statsd->destination, result->ai_addr, sizeof(struct sockaddr_in));
   statsd->destination.sin_port = htons((short)port);

   statsd->serverAddress = server;
   statsd->port = port;
   statsd->nameSpace = nameSpace;
   statsd->bucket = bucket;
   statsd->random = rand;

   //Free the result now that we have copied the data out of it.
   freeaddrinfo(result);

   //Store the IP address in readable form
   if (networkToPresentation(AF_INET, &statsd->destination.sin_addr, statsd->ipAddress, sizeof(statsd->ipAddress)) == NULL){
      return STATSD_NTOP;
   }
  
   // Check to see if there is already an open socket, and close it
   if (statsd->socketFd > 0){
      close(statsd->socketFd);
      statsd->socketFd = -1;
   }

   //Open up the socket file descriptor
   statsd->socketFd = socket(AF_INET, SOCK_DGRAM, 0);
   if (statsd->socketFd == -1){
      return STATSD_SOCKET;
   }
   
   return STATSD_SUCCESS;
}

/**
   Increment the bucket value by 1

   @param[in] stats - The statsd client object
   @param[in] bucket - The optional bucket name. If this is not
      provided, the default bucket will be used from the statsd
      object.

   @return STATSD_SUCCESS on success, an error if there is a problem.
   @see sendToServer
*/
int ADDCALL statsd_increment(Statsd* stats, const char* bucket){
   return sendToServer(stats, bucket, STATSD_COUNT, 1, 1);   
}

/**
   Decrement the bucket value by 1

   @param[in] stats - The statsd client object
   @param[in] bucket - The optional bucket name. If this is not
      provided, the defualt bucket will be used from the statsd
      object.
   
   @return STATSD_SUCCESS on success, an error if there is a problem.
   @see sendToServer
*/
int ADDCALL statsd_decrement(Statsd* stats, const char* bucket){
   return sendToServer(stats, bucket, STATSD_COUNT, -1, 1);   
}

/**
   Add a count value to the bucket

   @param[in] stats - The statsd client object.
   @param[in] bucket - The optional bucket name. If this is
      not provided, the default bucket will be used from the
      statsd object.
   @param[in] count - The value to increment (or decrement) the bucket
      by. If the value is negative, it will be decremented.
   @param[in] sampleRate - The sample rate of this statistic. If you specify
      a value 0 or less, or 1 or more then this value is ignored. Otherwise
      this value is sent on to the server.

   @return STATSD_SUCCESS on success, an error if there is a problem.
   @see sendToServer
*/
int ADDCALL statsd_count(Statsd* stats, const char* bucket, int count, double sampleRate){
   return sendToServer(stats, bucket, STATSD_COUNT, count, sampleRate);
}

/**
   Sets the value of a bucket to an arbitrary value

   @param[in] stats - The statsd client object.
   @param[in] bucket - The optional bucket name. If this is
      not provided, the default bucket will be used from the 
      statsd object.
   @param[in] value - The value to set the bucket to.
   @param[in] sampleRate - The sample rate of this statistic. If you specify
      a value 0 or less, or 1 or more then this value is ignored. Otherwise
      this value is sent on to the server. 

   @return STATSD_SUCCESS on success, an error if there is a problem.
   @see sendToServer
*/
int ADDCALL statsd_gauge(Statsd* stats, const char* bucket, int value, double sampleRate){
   return sendToServer(stats, bucket, STATSD_GAUGE, value, sampleRate);
}

/**
   Counts unique occurrences of events between flushes. 

   @param[in] stats - The statsd client object.
   @param[in] bucket - The optional bucket name. If this is
      not provided, the default bucket will be used from the 
      statsd object.
   @param[in] value - The value to set the bucket to.
   @param[in] sampleRate - The sample rate of this statistic. If you specify
      a value 0 or less, or 1 or more then this value is ignored. Otherwise
      this value is sent on to the server. 

   @return STATSD_SUCCESS on success, an error if there is a problem.
   @see sendToServer
*/
int ADDCALL statsd_set(Statsd* stats, const char* bucket, int value, double sampleRate){
   return sendToServer(stats, bucket, STATSD_SET, value, sampleRate);
}

/**
   Records the time it took something to take in milliseconds. 

   @param[in] stats - The statsd client object.
   @param[in] bucket - The optional bucket name. If this is
      not provided, the default bucket will be used from the 
      statsd object.
   @param[in] value - The value to set the bucket to.
   @param[in] sampleRate - The sample rate of this statistic. If you specify
      a value 0 or less, or 1 or more then this value is ignored. Otherwise
      this value is sent on to the server. NOTE: The actual sampling rate
      must be done externally, as each statistic will be sent on
      regardless of the sampleRate value.

   @return STATSD_SUCCESS on success, an error if there is a problem.
   @see sendToServer
*/
int ADDCALL statsd_timing(Statsd* stats, const char* bucket, int timing, double sampleRate){
   return sendToServer(stats, bucket, STATSD_TIMING, timing, sampleRate);
}

/**
   This function will reset the batch data that is being
   held. This is called automatically whenever you send
   the batch data to the server. You can also call this
   manually to removed any stored batch data.

   @param[in] statsd - The statsd client object
   @return STATSD_SUCCESS
*/
int ADDCALL statsd_resetBatch(Statsd* statsd){
   memset(statsd->batch, 0, BATCH_MAX_SIZE);
   statsd->batchIndex = 0;
   return STATSD_SUCCESS;
}

/**
   Add stats to the batch buffer to be sent later. 

   @param[in] statsd - The statsd client object
   @param[in] type - The type of stat being added
   @param[in] bucket - The name of a bucket to put the stat. This
      is optional and if not provided the default bucket name from
      the statsd object will be used.
   @param[in] value - The value of the stat
   @param[in] sampleRate - The rate at which the stat was gathered. 
   
   @return STATSD_SUCCESS if everything was successful. 
*/
int ADDCALL statsd_addToBatch(Statsd* statsd, StatsType type, const char* bucket, int value, double sampleRate){
   //See if we randomly fall under the sample rate
   if (sampleRate > 0 && sampleRate < 1 && (double)((double)statsd->random() / RAND_MAX) >= sampleRate){
      return STATSD_SUCCESS;
   }
 
   char statsString[256];
   if (!bucket){
      bucket = statsd->bucket;
   }

   int strLength = buildStatString(statsString, statsd->nameSpace, bucket, type, value, sampleRate);
   if (strLength < 0){
      return -strLength;
   }

   if (strLength + 1 + statsd->batchIndex < BATCH_MAX_SIZE){
      strcat(statsd->batch, statsString);
      strcat(statsd->batch, "\n");
      statsd->batchIndex += strLength + 1;
   }
   else {
      return STATSD_BATCH_FULL;
   }

   return STATSD_SUCCESS;
}

/**
   Send the batch message to the server. After a successful send
   this will reset the batch buffer.

   @param[in] statsd - The statsd client object
   @return STATSD_SUCCESS on success, STATSD_UDP_SEND if the 
      sendto() function failed.
*/
int ADDCALL statsd_sendBatch(Statsd* statsd){
   if (statsd->batchIndex <= 0){
      return STATSD_NO_BATCH;
   }

   int sent = sendto(statsd->socketFd, statsd->batch, statsd->batchIndex, 0, (const struct sockaddr*)&statsd->destination, sizeof(struct sockaddr_in));

   if (sent == -1){
      return STATSD_UDP_SEND;
   }
   
   statsd_resetBatch(statsd);
   return STATSD_SUCCESS;
}

