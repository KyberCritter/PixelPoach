#include "cam_link.h"

static u16 cam_r;   // cached RCNT value (direction + output data bits)

// Timeout budgets (iterations of the poll loop, not wall-clock microseconds).
// CAM_BYTE_US bounds how long a slave byte waits between clock edges: a gap
// there means the camera finished the transfer.
#define CAM_SHORT_US  2000        // within a byte, between clock edges
#define CAM_BYTE_US   1500000     // between bytes: a gap here means "done"

// Small pause before the GBA takes the clock, letting the camera re-enter
// slave mode (pandocs: the master should delay after each transfer).
#define CAM_SETTLE_TICKS (CAM_HALF_NORMAL * 6)

static inline void delay_ticks(u16 t)
{
    u16 start = REG_TM2CNT_L;
    while ((u16)(REG_TM2CNT_L - start) < t) ;
}

void cam_link_init(void)
{
    REG_SIOCNT = 0;
    cam_r = R_GPIO | R_SO_OUT;    // SO out, SC/SI/SD in; SO idles low
    REG_RCNT = cam_r;

    REG_TM2CNT_H = 0;
    REG_TM2CNT_L = 0;
    REG_TM2CNT_H = TM_ENABLE;
}

static inline void sc_dir(bool out)
{
    if (out) {
        cam_r |= R_SC;            // idle high before driving the line
        cam_r |= R_SC_OUT;
    } else {
        cam_r &= ~R_SC_OUT;
    }
    REG_RCNT = cam_r;
}

static inline void so_out(bool high)
{
    if (high) cam_r |= R_SO; else cam_r &= ~R_SO;
    REG_RCNT = cam_r;
}

static inline bool sc_is_high(void) { return (REG_RCNT & R_SC) != 0; }

// Poll SC for `high` (true) or `low` (false), with timeout and B-cancel.
// Returns 0 = edge seen, 1 = timeout, 2 = cancelled.
static int wait_sc(bool high, u32 budget)
{
    u32 t = 0;
    for (;;) {
        if (sc_is_high() == high) return 0;
        if (++t >= budget) return 1;
        if ((~REG_KEYINPUT) & KEY_B) return 2;
    }
}

// Poll SC with no timeout: only returns on the edge (0) or B cancel (2). Used
// while waiting for the camera to start a transfer.
static int wait_sc_forever(bool high)
{
    for (;;) {
        if (sc_is_high() == high) return 0;
        if ((~REG_KEYINPUT) & KEY_B) return 2;
    }
}

// Raw master byte: GBA drives the clock. Caller has already masked IME.
static u8 master_byte_raw(u8 send)
{
    u8 recv = 0;
    sc_dir(true);
    for (int bit = 7; bit >= 0; bit--) {
        cam_r &= ~R_SC;                       // clock low
        if (send & (1 << bit)) cam_r |= R_SO; else cam_r &= ~R_SO;
        REG_RCNT = cam_r;
        delay_ticks(CAM_HALF_NORMAL);
        cam_r |= R_SC;                        // rising edge: both sides sample
        REG_RCNT = cam_r;
        recv = (u8)((recv << 1) | ((REG_RCNT >> 2) & 1));
        delay_ticks(CAM_HALF_NORMAL);
    }
    sc_dir(false);
    return recv;
}

// Raw slave byte: camera drives the clock. Returns 0 = ok, 1 = timeout,
// 2 = cancelled. `first_budget` bounds the wait for the first falling edge
// (long between bytes); 0 means wait forever (used before the camera has
// started clocking). Subsequent edges use a short bound.
static int slave_byte_raw(u8 send, u8 *recv, u32 first_budget)
{
    u8 r = 0;
    sc_dir(false);
    for (int bit = 7; bit >= 0; bit--) {
        so_out((send & (1 << bit)) != 0);
        int rc = (bit == 7 && first_budget == 0)
                   ? wait_sc_forever(false)
                   : wait_sc(false, (bit == 7) ? first_budget : CAM_SHORT_US);
        if (rc) return rc;
        rc = wait_sc(true, CAM_SHORT_US);
        if (rc) return rc;
        r = (u8)((r << 1) | ((REG_RCNT >> 2) & 1));
    }
    if (recv) *recv = r;
    return 0;
}

u8 cam_link_master_byte(u8 send)
{
    u32 ime = REG_IME;
    REG_IME = 0;
    u8 r = master_byte_raw(send);
    REG_IME = ime;
    return r;
}

bool cam_link_slave_byte(u8 send, u8 *recv)
{
    u32 ime = REG_IME;
    REG_IME = 0;
    int rc = slave_byte_raw(send, recv, CAM_BYTE_US);
    REG_IME = ime;
    return rc == 0;
}

int cam_link_exchange(bool is_a, const u8 *tx, int tx_len,
                      u8 *rx, int max_rounds, int *rx_len,
                      void (*progress)(int n))
{
    int n = 0;
    u32 ime = REG_IME;
    REG_IME = 0;                       // mask IRQs for the whole exchange

    while (n < max_rounds) {
        u8 send = (n < tx_len) ? tx[n] : 0x00;
        bool my_clock = ((n & 1) == (is_a ? 0 : 1));

        u8 recv;
        if (my_clock) {
            delay_ticks(CAM_SETTLE_TICKS);      // let the camera become slave
            // Report progress NOW, in the settle window, not right after a byte.
            // At this point the camera is the slave and is waiting for our
            // clock, so drawing the counter cannot make us miss a clock edge.
            // Running it after every byte put it directly in front of the next
            // slave read, and its cost made us drop the tail of the stream.
            if (progress) progress(n);
            recv = master_byte_raw(send);
        } else {
            // First byte: wait indefinitely for the camera to start clocking
            // (B cancels). Later bytes use a per-byte timeout to detect "done".
            u32 budget = (n == 0) ? 0 : CAM_BYTE_US;
            int rc = slave_byte_raw(send, &recv, budget);
            if (rc == 2) {                       // cancelled
                REG_IME = ime;
                if (progress) progress(n);
                if (rx_len) *rx_len = n;
                return CAM_XCHG_CANCELLED;
            }
            if (rc == 1) {                       // no clock: absent or finished
                REG_IME = ime;
                if (progress) progress(n);
                if (rx_len) *rx_len = n;
                return (n == 0) ? CAM_XCHG_TIMEOUT : CAM_XCHG_OK;
            }
        }
        rx[n++] = recv;
    }

    REG_IME = ime;
    if (progress) progress(n);
    if (rx_len) *rx_len = n;
    return CAM_XCHG_OK;
}
