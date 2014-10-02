
# Building

    autoreconf -iv
    ./configure
    make

# Installing

    make install

# Sysctls for high load (N connections, such as 50k):

    kern.maxfiles=10000+2*N         # BSD
    kern.maxfilesperproc=100+N      # BSD
    fs.file-max=10000+2*N           # Linux

    # For load-generating clients.
    net.ipv4.ip_local_port_range="10000  65535"  # Linux.
    net.inet.ip.portrange.hifirst=10000          # BSD/Mac.
    net.inet.ip.portrange.hilast=65535           # (Enough for N < 55535)
    net.ipv4.tcp_tw_reuse=1

    # If using netfilter on Linux:
    net.netfilter.nf_conntrack_max=N
    net.netfilter.nf_conntrack_max=N/4

# Readings:

 * On TIME-WAIT state and its reuse:
     http://vincent.bernat.im/en/blog/2014-tcp-time-wait-state-linux.html
 * On netfliter settings:
     http://serverfault.com/questions/482480/

