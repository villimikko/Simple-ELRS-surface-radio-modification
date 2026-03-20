#ifndef PAGES_H
#define PAGES_H

#include "menu.h"
#include "shared.h"
#include "config.h"
#include "calibrate.h"

// Render the current display page/menu to LCD
// Called at ~30Hz from Core 1
void pages_render(const menu_t *menu, const shared_state_t *state,
                  const config_t *cfg, const calibrate_t *cal);

#endif
