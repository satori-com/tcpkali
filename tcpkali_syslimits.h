#ifndef TCPKALI_SYSLIMITS_H
#define TCPKALI_SYSLIMITS_H

/*
 * Attempt to adjust system limits (open file counts) for high load.
 */
int adjust_system_limits_for_highload(int expected_sockets, int workers);

/*
 * Check that the system is prepared to handle high load by
 * observing sysctls, and the like.
 */
int check_system_limits_sanity(int expected_sockets, int workers);

#endif  /* TCPKALI_SYSLIMITS_H */
