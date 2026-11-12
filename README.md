# MEC1161 Mass-Spring-Damper Simulation

C program that simulates a mass-spring-damper system using Newton's second law, `m*a = F(t) - c*v - k*x`, solved with Forward Euler or 4th-order Runge-Kutta (RK4).

## Files

| File | What it holds |
|---|---|
| `main.c` | Input menu (Part 3, Ahmad) and calls to the solver; prints t, x, v, a for every step |
| `model.c` | Model and solvers (Part 1, Aqil): `accel()`, `step_euler()`, `step_rk4()`, `num_steps()`, `simulate()` |
| `msd.h` | Shared structs (`Params`, `State`) and function prototypes |
| `Project_MassSpringDamper_Parts1and2.cbp` | Code::Blocks project (C99, `-Wall`) |

## Build and run

- **Code::Blocks:** open `Project_MassSpringDamper_Parts1and2.cbp`, press F9.
- **Command line:** `gcc -std=c99 -Wall -o msd main.c model.c -lm`, then run `./msd`.

## Quick check

Inputs: m = 1, c = 0, k = 20, x0 = 1, v0 = 0, t_total = 10, dt = 0.01, method = 2 (RK4).
The last row must show `t = 10.0000`, `x = 0.739101` (exact answer: cos(sqrt(20) * 10) = 0.739100).

## To do

- Menu: add forcing inputs F0 (N) and w (rad/s) for the forced-vibration case (`main.c` sets both to 0 for now).
- Part 3: analysis (peaks, settling time) and CSV export.
- Part 4: Gnuplot graphs.
