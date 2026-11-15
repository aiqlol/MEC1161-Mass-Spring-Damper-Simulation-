#ifndef VIZ_H
#define VIZ_H

#include "msd.h"

/* Interactive visualization menu (Part 4).
   data = results already computed by simulate(), n = number of rows,
   p    = the parameters used, step = the solver chosen (Euler or RK4);
   step is reused to re-run the model for the damping comparisons.
   Returns when the user chooses Exit. */
void viz_menu(const State *data, int n, const Params *p,
              void (*step)(State *, const Params *));

#endif

