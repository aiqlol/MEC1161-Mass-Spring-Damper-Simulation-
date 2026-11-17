/* model.c - Part 1 (Aqil): mathematical model and numerical solvers */
#include <stdio.h>
#include <math.h>
#include "msd.h"

double accel(double t, double x, double v, Params *p)
{
    double F;                                    /* external force at time t */

    F = p->F0 * sin(p->w * t);                   /* F(t) = F0 sin(wt), 0 when F0 = 0 */
    return (F - p->c * v - p->k * x) / p->m;     /* Newton's 2nd law: a = sum of forces / m */
}

void step_euler(State *s, Params *p)
{
    double a;                                    /* acceleration at start of step */

    a = accel(s->t, s->x, s->v, p);              /* slope of v at start */
    s->x = s->x + s->v * p->dt;                  /* new x from old v */
    s->v = s->v + a * p->dt;                     /* new v from old a */
    s->t = s->t + p->dt;                         /* advance time */
    s->a = accel(s->t, s->x, s->v, p);           /* acceleration at new state */
}

void step_rk4(State *s, Params *p)
{
    double dt, h;                                /* full step and half step */
    double t, x, v;                              /* values at start of step */
    double k1x, k1v, k2x, k2v, k3x, k3v, k4x, k4v;   /* the four pairs of slopes */

    dt = p->dt;
    h = dt / 2.0;
    t = s->t;
    x = s->x;
    v = s->v;

    k1x = v;                                             /* slope of x at start */
    k1v = accel(t, x, v, p);                             /* slope of v at start */
    k2x = v + h * k1v;                                   /* slope of x at midpoint (using k1) */
    k2v = accel(t + h, x + h * k1x, v + h * k1v, p);     /* slope of v at midpoint (using k1) */
    k3x = v + h * k2v;                                   /* slope of x at midpoint (using k2) */
    k3v = accel(t + h, x + h * k2x, v + h * k2v, p);     /* slope of v at midpoint (using k2) */
    k4x = v + dt * k3v;                                  /* slope of x at end (using k3) */
    k4v = accel(t + dt, x + dt * k3x, v + dt * k3v, p);  /* slope of v at end (using k3) */

    s->x = x + dt / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);   /* weighted average slope of x */
    s->v = v + dt / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);   /* weighted average slope of v */
    s->t = t + dt;                                               /* advance time */
    s->a = accel(s->t, s->x, s->v, p);                           /* acceleration at new state */
}

int num_steps(Params *p)
{
    return (int)(p->t_total / p->dt + 0.5);      /* t_total / dt, rounded to nearest whole step */
}

void simulate(State out[], int n, Params *p, int method)
{
    int i;                                       /* step counter */

    out[0].t = 0.0;                              /* start time */
    out[0].x = p->x0;                            /* initial displacement */
    out[0].v = p->v0;                            /* initial velocity */
    out[0].a = accel(0.0, p->x0, p->v0, p);      /* initial acceleration */

    for (i = 1; i < n; i++)                      /* each remaining time step */
    {
        out[i] = out[i - 1];                     /* start from previous state */
        if (method == METHOD_EULER)
        {
            step_euler(&out[i], p);              /* advance one step with Euler */
        }
        else
        {
            step_rk4(&out[i], p);                /* advance one step with RK4 */
        }
        out[i].t = i * p->dt;                    /* exact time (stops round-off building up) */
    }
}
