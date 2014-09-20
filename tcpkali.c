#include <getopt.h>
#include <sysexits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* gethostbyname(3) */
#include <libgen.h> /* basename(3) */
#include <err.h>
#include <assert.h>

#include "tcpkali.h"

/*
 * Describe the command line options.
 */
static struct option cli_long_options[] = {
    { "help", 0, 0, 'h' }
};

/*
 * Bunch of utility functions defined at the end of this file.
 */
static void usage(char *argv0);

/*
 * Parse command line options and kick off the engine.
 */
int main(int argc, char **argv) {
    int opt_index;

    while(1) {
        int c;
        c = getopt_long(argc, argv, "h", cli_long_options, &opt_index);
        if(c == -1)
            break;
        switch(c) {
        case 'h':
            usage(argv[0]);
            exit(EX_USAGE);
        }
    }

    if(optind == argc) {
        fprintf(stderr, "Expecting target <host:port> as an argument. See --help.\n");
        exit(EX_USAGE);
    }

    /*
     * Pick multiple destinations from the command line.
     */
    struct addresses addresses;
    addresses = resolve_remote_addresses(&argv[optind], argc-optind);
    if(addresses.n_addrs == 0) {
        errx(EX_NOHOST, "DNS did not return usable addresses for given host(s)\n");
    }

    fprint_addresses(stderr, "Destination: ",
                         "\nDestination: ", "\n", addresses);

    int wr_pipe = start_engine(addresses);
    write(wr_pipe, "cccccccccccccccccccc", 20);
    usleep(1000 * 1000);
    write(wr_pipe, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", 40);
    usleep(10 * 1000);

    return 0;
}

/*
 * Display the Usage screen.
 */
static void usage(char *argv0) {
    fprintf(stderr, "Usage: %s [OPTIONS] <host:port>\n", basename(argv0));
    fprintf(stderr,
    "Where OPTIONS are:\n"
    "  -h, --help                  Display this help screen\n"
    "  -c, --connections <N>       Number of channels to open to the destinations\n"
    "  --connect-rate <R>          Number of new connections per second\n"
    "  --message-rate <N>          Generate N messages per second per channel\n"
    "  --channel-bandwidth  <Bw>   Limit channel bandwidth (see multipliers below)\n"
    "  --total-bandwidth  <Bw>     Limit total bandwidth (see multipliers below)\n"
    "  --time <T>                  Load test for the specified amount of time\n"
    "And variable multipliers are:\n"
    "  <R>:  k (1000, as in \"5k\" is 5000)\n"
    "  <Bw>: kbps, mbps (bits per second), kBps, mBps (megabytes per second)\n"
    "  <T>:  s, m, h (seconds, minutes, hours)\n"
    );
}
