#include "shared.h"
#include <string.h>
#include "hardware/sync.h"

static shared_state_t g_shared;
static spin_lock_t *g_shared_lock;

void shared_init(void) {
    memset(&g_shared, 0, sizeof(g_shared));
    g_shared_lock = spin_lock_init(spin_lock_claim_unused(true));
}

void shared_update(const shared_state_t *state) {
    uint32_t irq_state = spin_lock_blocking(g_shared_lock);
    g_shared = *state;
    spin_unlock(g_shared_lock, irq_state);
}

void shared_snapshot(shared_state_t *state) {
    uint32_t irq_state = spin_lock_blocking(g_shared_lock);
    *state = g_shared;
    spin_unlock(g_shared_lock, irq_state);
}
