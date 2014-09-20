#ifndef TCPKALI_ATOMIC_H
#define TCPKALI_ATOMIC_H

typedef uint32_t atomic_t;

static inline void __attribute__((unused)) atomic_increment(atomic_t *i) {
    asm volatile("lock incl %0" : "+m" (*i));
}

static inline void __attribute__((unused)) atomic_decrement(atomic_t *i) {
    asm volatile("lock decl %0" : "+m" (*i));
}

#endif  /* TCPKALI_ATOMIC_H */
