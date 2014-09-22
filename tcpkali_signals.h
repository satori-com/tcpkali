#ifndef TCPKALI_SIGNAL_H
#define TCPKALI_SIGNAL_H

/*
 * Protect this thread from receiving term (SIGINT) signals.
 */
void block_term_signals();

/*
 * Unblock term signals (SIGINT) and handle it by setting a flag.
 */
void flagify_term_signals(int *flag);


#endif  /* TCPKALI_SIGNAL_H */
