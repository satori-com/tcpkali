# libcows

C Open WebSockets library

Some useful functions to build WebSockets applications:

 - encoding/decoding of WebSocket protocol frames
 - code for base64
 - buffer management

## Build

To build the library you need to run

    autoreconf -iv && ./configure && make

To test:

    make check
