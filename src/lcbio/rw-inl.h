/**
 * Inline routines for reading and writing socket buffers
 */
#include <errno.h>
#ifndef INLINE
#ifdef _MSC_VER
#define INLINE __inline
#elif __GNUC__
#define INLINE __inline__
#else
#define INLINE inline
#endif /* MSC_VER */
#endif /* !INLINE */

#define RWINL_IOVSIZE 32

#ifndef USE_EAGAIN
#define C_EAGAIN 0
#else
#define C_EAGAIN EAGAIN
#endif

static INLINE lcbio_IOSTATUS
lcbio_E_rdb_slurp(lcbio_SOCKET *sock, rdb_IOROPE *ior)
{
    lcb_ssize_t rv;
    lcb_IOV iov[RWINL_IOVSIZE];
    unsigned niov;
    lcbio_TABLE *iot = sock->io;

    do {
        niov = rdb_rdstart(ior, (nb_IOV *)iov, RWINL_IOVSIZE);
        GT_READ:
        rv = IOT_V0IO(iot).recvv(IOT_ARG(iot), sock->u.fd, iov, niov);
        if (rv > 0) {
            rdb_rdend(ior, rv);
        } else if (rv == -1) {
            switch (IOT_ERRNO(iot)) {
            case EWOULDBLOCK:
            case C_EAGAIN:
                return LCBIO_PENDING;
            case EINTR:
                goto GT_READ;
            default:
                sock->last_error = IOT_ERRNO(iot);
                return LCBIO_IOERR;
            }
        } else {
            return LCBIO_SHUTDOWN;
        }
    } while (1);
    /* UNREACHED */
    return LCBIO_IOERR;
}

static INLINE lcbio_IOSTATUS
lcbio_E_rb_write(lcbio_SOCKET *sock, ringbuffer_t *buf)
{
    lcb_IOV iov[2];
    lcb_ssize_t nw;
    lcbio_TABLE *iot = sock->io;
    while (buf->nbytes) {
        unsigned niov;
        ringbuffer_get_iov(buf, RINGBUFFER_READ, iov);
        niov = iov[1].iov_len ? 2 : 1;
        nw = IOT_V0IO(iot).sendv(IOT_ARG(iot), sock->u.fd, iov, niov);
        if (nw == -1) {
            switch (IOT_ERRNO(iot)) {
            case EINTR:
                break;
            case EWOULDBLOCK:
            case C_EAGAIN:
                return LCBIO_PENDING;
            default:
                sock->last_error = IOT_ERRNO(iot);
                return LCBIO_IOERR;
            }
        }
        if (nw) {
            ringbuffer_consumed(buf, nw);
        }
    }
    return LCBIO_COMPLETED;
}
