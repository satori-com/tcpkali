# PCG Random Number Generation, Minimal C Edition

[PCG-Random website]: http://www.pcg-random.org

This code provides a minimal implementation of one member of the PCG family
of random number generators, which are fast, statistically excellent,
and offer a number of useful features.

Full details can be found at the [PCG-Random website].  This version
of the code provides a single family member and skips some useful features
(such as jump-ahead/jump-back) -- if you want a more full-featured library, 
you may prefer the full version of the C library, or for all features,
the C++ library.

## Documentation and Examples

Visit [PCG-Random website] for information on how to use this library, or look
at the sample code -- hopefully it should be fairly self explanatory.

## Building

There is no library to build.  Just use the code.  You can however build the
three demo programs.

The code is written in C89-style C with no significant platform dependencies.
On a Unix-style system (e.g., Linux, Mac OS X), or any system with `make`,
you should be able to just type type

    make

Almost all the real code is in `pcg_basic.c`, with type and function
declarations in `pcg_basic.h`.  

On other systems, it should be straightforward to build.  For example, you
even run the code directly using the tinycc compiler, using

    cat pcg_basic.c pcg32-demo.c | tcc -run

## Testing

This command will build the three provided demo programs, `pcg32-global-demo`
(which uses the global rng), `pcg32-demo` (which uses a local generator), and
pcg32x2-demo (which gangs together two generators, showing the usefulness of
creating multiple generators).

To run the demos using a fixed seed (same output every time), run

    ./pcg32-demo
    
To produce different output, run

    ./pcg32-demo -r

You can also pass an integer count to specify how may rounds of output you
would like.