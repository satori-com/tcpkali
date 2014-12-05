# libstatsd
libstatsd is a C library for sending stats data to a statsd server. statsd is a network 
daemon that runs on Node.js. It was written by the engineers at Etsy and can be 
cloned from git://github.com/etsy/statsd.git. 

## Installing
### Cloning from github
If you have cloned this project from github the first thing that needs to be done is to
run the command 

```
autoreconf --install
```
This will run the GNU autotools suite on the project and generate the configure file. After
this you can run 

```
./configure && make
sudo make install
```
This will install the library, headers, and a binary called statsd-cli.

### Downloading from jamesslocum.com
You can download official release version from jamesslocum.com. Once you download
the archive you can install it like any other Unix program.

```
tar -zxvf libstatsd-X.Y.Z.tar.gz
cd libstatsd-X.Y.Z
./configure
make
sudo make install
```

### Installing on windows
The ultimate goal is to make this library cross platform. Currently I have no automated
way to build or install a windows version of the library. I will post info on compiling
and installing on windows when I have a solid method of doing so. 

## Library Usage
### including and linking
Once the library has been installed, you can use it by including the statsd.h header
file and linking against libstatsd.
```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <statsd.h>

int main(int argc, char* argv[]){
   //Recommended before you use the default sampling features
   srand(time(NULL));

   //program that uses statsd...

   return 0;
}
```

```bash
gcc --std=c99 prog.c -o prog -lstatsd
```
### Initializing the client object
Before any stats can be sent to the server, you must first build a client
object. The Statsd data type has been created for this purpose. There are two ways to 
initialize the object.

```c
int statsd_new(Statsd **stats, const char* serverAddress, int port, const char* nameSpace, const char* bucket);
int statsd_init(Statsd* statsd, const char* server, int port, const char* nameSpace, const char* bucket);
```
The first way will use malloc to allocate new memory for the Statsd client object. The 
second way can be used for static allocation.

#### Static allocation

```c
Statsd stats;
int ret = statsd_init(&stats, "localhost", STATSD_PORT, "application.test", "times");
if (ret != STATSD_SUCCESS){
   //Error
   return ret;
}
```
If you have used statsd_init() to initialize a client object, you must call
statsd_release() to free the resources. 

```c
int statsd_release(Statsd *stats);
``` 

#### Dynamic allocation

```c
Statsd *stats = NULL;
int ret = statsd_new(&stats, "localhost", STATSD_PORT, "application.test", "times");
if (ret != STATSD_SUCCESS){
   //Error
   return ret;
}
```
If you have used statsd_new() to dynamically create a client object, you must call
statsd_free() to free the resources and avoid memory leaks. 

```c
int statsd_free(Statsd *stats);
```

### Stat types and usage
The stats types supported are count, set, timing, and gauge. They can be called through 
their respective functions. Each of these functions takes in 4 parameters. 

* statsd - This is the statsd client object
* bucket - This is an optional bucket name that will override the value in the statsd client
* value - This is the value of the stat to send
* sampleRate - If you are gathering a stat incrementally, this is the rate.

```c
int statsd_count(Statsd* statsd, const char* bucket, int count, double sampleRate);
int statsd_gauge(Statsd* statsd, const char* bucket, int value, double sampleRate);
int statsd_set(Statsd* statsd, const char* bucket, int value, double sampleRate);
int statsd_timing(Statsd* statsd, const char* bucket, int timing, double sampleRate);
```
There are also two convience functions for changing a count stat by 1. They take
2 parameters.

* statsd - This is the statsd client object
* bucket - An optional bucket name to override the value in the statsd client.

```c
int statsd_increment(Statsd* statsd, const char* bucket);
int statsd_decrement(Statsd* statsd, const char* bucket);
```
### Sampling
By default, the sampling RNG is the C rand() function. This is not a very good
RNG so it can be overridden with a better one if you wish. The Statsd client
object has a function pointer called random.

```c
int (*random)(void);
```

You can assign this to another RNG function the generates an integer number
between [0 - RAND_MAX]. It is recommended that before you use the default
sampling RNG, you call srand(time(NULL)) to initialize the RNG and get
better random numbers. 

### Batching
Statsd also supports sending multiple stats at once in a single UDP packet. 

* statsd - This is the statsd client object
* type - The type of stat. STATSD_COUNT, STATSD_SET, STATSD_GAUGE, and STATSD_TIMING are valid types.
* bucket - This is an optional bucket name that will override the value in the statsd client
* value - This is the value of the stat to send
* sampleRate - If you are gathering a stat incrementally, this is the rate.

```c
int statsd_addToBatch(Statsd* statsd, StatsType type, const char* bucket, int value, double sampleRate);
int statsd_sendBatch(Statsd* statsd);
int statsd_resetBatch(Statsd* statsd);
```

### Errors
The following values can be returned from the library functions

* STATSD_SUCCESS - The function completed successfully.

* STATSD_SOCKET - The socket could not be  created  during  the  initialization  of  the
client object.

* STATSD_NTOP  -  The  IP  address  of the server could not be converted into a readable
form. see inet_ntop(3).

* STATSD_MALLOC - Out of memory. Memory allocation failed.

* STATSD_BAD_SERVER_ADDRESS - Unable to look up server name in DNS. see getaddrinfo(3).

* STATSD_UDP_SEND - Unable to send data through the socket. see sendto(2).

* STATSD_NO_BATCH - If you try to call statsd_sendBatch() before you have added any data
to the batch.

* STATSD_BATCH_FULL  -  If  you  try  to add more stats to the batch then the mtu of udp
packet can handle. The default mtu limit is set to 512 bytes. you  can  override  this
value at compile time by specifying a new value for BATCH_MAX_SIZE.

* STATSD_BAD_STATS_TYPE - The type field specified was invalid.

## Command line
This project comes with a command line tool called statsd-cli. 

```bash
$ statsd-cli -h
statsd-cli version 1.0.0 [j.m.slocum@gmail.com]
usage:
   -h --help : print this help message
   -s --server : specify the server name or ip address
   -p --port : specify the port (default = 8125)
   -n --namespace : specify the bucket namespace
   -b --bucket : specify the bucket
   -t --type : specify the stat type
      types: count, set, gauge, timing
   -r --rate : specify the sample rate
   example:
      statsd-cli -s statsd.example.com -n some.statsd -b counts -t count 25
```
## Tested systems
This project has been compiled and installed on Ubuntu 12.04 and OSX Lion.

## License
This project is available for use under the MIT license.

Copyright (c) 2013 James Slocum (jamesslocum.com)

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
