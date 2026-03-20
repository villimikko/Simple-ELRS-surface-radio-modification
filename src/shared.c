#include "shared.h"
#include <string.h>

shared_state_t g_shared;
spin_lock_t *g_shared_lock;

void shared_init(void) {
    memset(&g_shared, 0, sizeof(g_shared));
    g_shared_lock = spin_lock_init(spin_lock_claim_unused(true));
}
