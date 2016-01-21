#ifndef TCPKALI_EVENTS
#define TCPKALI_EVENTS

#include <config.h>

#if HAVE_LIBUV == 1 && HAVE_UV_H == 1 /* Use libuv */
/*************/
/* Use libuv */
/*************/
#define USE_LIBUV 1

#include <uv.h>

#define TK_DEFAULT (uv_default_loop())
#define TK_P_ uv_loop_t *loop,
#define TK_P uv_loop_t* loop
#define TK_A_ loop,
#define TK_A loop

#define TK_READ UV_READABLE
#define TK_WRITE UV_WRITABLE

#define tk_now(loop) (uv_now(loop) / 1000.0)
#define tk_now_update(loop) uv_update_time(loop)

typedef uv_poll_t tk_io;
typedef uv_timer_t tk_timer;
typedef uv_loop_t tk_loop;

#define tk_fd(w)                                   \
    ({                                             \
        uv_os_fd_t fd;                             \
        int r = uv_fileno((uv_handle_t*)(w), &fd); \
        assert(r == 0);                            \
        fd;                                        \
    })
#define tk_close(w, free_cb)                               \
    do {                                                   \
        int fd = tk_fd(w);                                 \
        uv_close((uv_handle_t*)(w), (uv_close_cb)free_cb); \
        int r = close(fd);                                 \
        assert(r == 0);                                    \
    } while(0)
#define tk_userdata(loop) ((loop)->data)
#define tk_set_userdata(loop, p) ((loop)->data = (p))
#define tk_loop_new() uv_loop_new()
#define tk_stop(loop) uv_stop(loop)
#define tk_io_stop(loop, p) uv_poll_stop((p))
#define tk_timer_stop(loop, t) uv_timer_stop((t))

#else /* Use libev */
/*************/
/* Use libev */
/*************/

#include <ev.h>

#define TK_DEFAULT EV_DEFAULT
#define TK_P_ EV_P_
#define TK_P EV_P
#define TK_A_ EV_A_
#define TK_A EV_A

#define TK_READ EV_READ
#define TK_WRITE EV_WRITE

#define tk_now_update(loop) ev_now_update(loop)
#define tk_now(loop) ev_now(loop)

typedef ev_io tk_io;
typedef ev_timer tk_timer;
typedef struct ev_loop tk_loop;

#define tk_fd(w) ((w)->fd)
#define tk_close(w, free_cb) \
    do {                     \
        close(tk_fd(w));     \
        (w)->fd = -1;        \
        free_cb(w);          \
    } while(0)
#define tk_userdata ev_userdata
#define tk_set_userdata ev_set_userdata
#define tk_loop_new()                     \
    ev_loop_new(ev_recommended_backends() \
                | (ev_supported_backends() & EVBACKEND_KQUEUE))
#define tk_stop(loop) ev_break((loop), EVBREAK_ALL)
#define tk_io_stop(loop, p) ev_io_stop((loop), (p))
#define tk_timer_stop(loop, t) ev_timer_stop((loop), (t))

#endif /* libuv vs libev */

#endif /* TCPKALI_EVENTS */
