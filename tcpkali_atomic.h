#ifndef TCPKALI_ATOMIC_H
#define TCPKALI_ATOMIC_H

typedef uint32_t atomic_t;
typedef uint64_t atomic64_t;

static inline void __attribute__((unused)) atomic_increment(atomic_t *i) {
    asm volatile("lock incl %0" : "+m" (*i));
}

static inline void __attribute__((unused)) atomic_decrement(atomic_t *i) {
    asm volatile("lock decl %0" : "+m" (*i));
}

static inline void __attribute__((unused)) atomic_add(atomic64_t *i, uint64_t v) {
    asm volatile("lock addq %1, %0" : "+m" (*i) : "r" (v));
}

#endif  /* TCPKALI_ATOMIC_H */
