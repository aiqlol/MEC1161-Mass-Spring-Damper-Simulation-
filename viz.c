/* viz.c - Part 4 (Haziq and Subhas): visualisation with Gnuplot           */
/* Each graph is written as a Gnuplot script file (Lecture 9: file         */
/* handling). The script holds the commands and the data, so Gnuplot can   */
/* draw it on its own: open the file in Gnuplot (File > Open), or type     */
/*     load 'plot_displacement.gp'                                         */
/* in Gnuplot after changing to this folder.                               */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "msd.h"

void write_time_plot(SimulationData *results, int column, char filename[], char title[], char ylabel[]);
void write_regimes_plot(SimulationData *results, Params *p, int method);
void write_resonance_plot(Params *p, int method);
void show_open_message(char filename[]);

void show_open_message(char filename[])
{
    printf(" Saved %s\n", filename);
    printf(" To view it: open Gnuplot, then File > Open > %s\n", filename);
    printf(" (or in Gnuplot type: load '%s')\n", filename);
}

void write_time_plot(SimulationData *results, int column, char filename[], char title[], char ylabel[])
{
    FILE *gp;                                    /* the Gnuplot script file */
    int i;                                       /* row counter */
    double y;                                    /* value to plot */

    if ((gp = fopen(filename, "w")) == NULL)
    {
        printf(" [Error] Could not create %s\n", filename);
        return;
    }
    fprintf(gp, "set title '%s'\n", title);      /* graph title */
    fprintf(gp, "set xlabel 'Time t (s)'\n");    /* x-axis label */
    fprintf(gp, "set ylabel '%s'\n", ylabel);    /* y-axis label */
    fprintf(gp, "set grid\n");                   /* background grid */
    fprintf(gp, "plot '-' using 1:2 with lines lw 2 notitle\n");   /* data follows in this file */
    for (i = 0; i < results->count; i++)         /* one point per time step */
    {
        switch (column)                          /* pick x, v or a */
        {
        case 1:
            y = results->states[i].x;
            break;
        case 2:
            y = results->states[i].v;
            break;
        default:
            y = results->states[i].a;
            break;
        }
        fprintf(gp, "%g %g\n", results->states[i].t, y);
    }
    fprintf(gp, "e\n");                          /* end of data */
    fprintf(gp, "pause -1 'Press Enter to close the graph'\n");   /* keep the window open */
    fclose(gp);
    show_open_message(filename);
}

void write_regimes_plot(SimulationData *results, Params *p, int method)
{
    FILE *gp;                                    /* the Gnuplot script file */
    State *run;                                  /* array for one re-run */
    Params q;                                    /* copy of the user's inputs */
    double cc;                                   /* critical damping 2*sqrt(m*k) */
    double factor[3] = {0.2, 1.0, 3.0};          /* under, critical, over */
    int r, i;                                    /* regime and row counters */
    int n;                                       /* States per run */

    n = results->count;
    cc = 2.0 * sqrt(p->m * p->k);
    if ((run = (State *)malloc(n * sizeof(State))) == NULL)
    {
        printf(" [Error] Not enough memory.\n");
        return;
    }
    if ((gp = fopen("plot_regimes.gp", "w")) == NULL)
    {
        printf(" [Error] Could not create plot_regimes.gp\n");
        free(run);
        return;
    }
    fprintf(gp, "set title 'Damping regimes (m = %g kg, k = %g N/m)'\n", p->m, p->k);
    fprintf(gp, "set xlabel 'Time t (s)'\n");
    fprintf(gp, "set ylabel 'Displacement x (m)'\n");
    fprintf(gp, "set grid\n");
    fprintf(gp, "plot '-' with lines lw 2 title 'Underdamped (c = %g)', ", factor[0] * cc);
    fprintf(gp, "'-' with lines lw 2 title 'Critically damped (c = %g)', ", factor[1] * cc);
    fprintf(gp, "'-' with lines lw 2 title 'Overdamped (c = %g)'\n", factor[2] * cc);
    for (r = 0; r < 3; r++)                      /* each regime */
    {
        q = *p;                                  /* copy the user's inputs */
        q.c = factor[r] * cc;                    /* change only the damping */
        simulate(run, n, &q, method);            /* re-run with the same solver (Part 1) */
        for (i = 0; i < n; i++)
        {
            fprintf(gp, "%g %g\n", run[i].t, run[i].x);
        }
        fprintf(gp, "e\n");                      /* end of this line's data */
    }
    fprintf(gp, "pause -1 'Press Enter to close the graph'\n");
    fclose(gp);
    free(run);                                   /* release the re-run array */
    show_open_message("plot_regimes.gp");
}

void write_resonance_plot(Params *p, int method)
{
    FILE *gp;                                    /* the Gnuplot script file */
    State *run;                                  /* array for one run */
    Params q;                                    /* copy of the user's inputs */
    double wn, tau, t_end, amp, x_abs, X, w;     /* see comments below */
    double w_list[100], amp_list[100];           /* simulated amplitude at each frequency */
    int i, j, n;                                 /* counters and States per run */

    if (p->c <= 0.0)                             /* no damping: amplitude never settles */
    {
        printf(" [Error] The resonance curve needs damping c > 0.\n");
        return;
    }
    wn = sqrt(p->k / p->m);                      /* natural frequency */
    tau = 2.0 * p->m / p->c;                     /* time for the start-up motion to fade */
    t_end = 8.0 * tau;                           /* long enough to settle ... */
    if (t_end < 40.0 * PI / (0.2 * wn))          /* ... and at least 20 cycles at the lowest w */
    {
        t_end = 40.0 * PI / (0.2 * wn);
    }
    q = *p;                                      /* copy the user's inputs */
    if (q.F0 <= 0.0)                             /* use 1 N if the user had no force */
    {
        q.F0 = 1.0;
    }
    q.x0 = 0.0;                                  /* start at rest */
    q.v0 = 0.0;
    q.t_total = t_end;
    q.dt = p->dt;
    if (q.dt > (2.0 * PI / (3.0 * wn)) / 50.0)   /* at least 50 steps per fastest cycle */
    {
        q.dt = (2.0 * PI / (3.0 * wn)) / 50.0;
    }
    if (t_end / q.dt > 200000.0)                 /* limit the work per run */
    {
        q.dt = t_end / 200000.0;
    }
    n = num_steps(&q) + 1;                       /* States per run */
    if ((run = (State *)malloc(n * sizeof(State))) == NULL)
    {
        printf(" [Error] Not enough memory.\n");
        return;
    }

    printf(" Running 100 simulations, please wait...\n");
    for (j = 0; j < 100; j++)                    /* 100 forcing frequencies */
    {
        w = wn * (0.2 + 2.8 * j / 99.0);         /* from 0.2 wn to 3 wn */
        q.w = w;
        simulate(run, n, &q, method);            /* run with the user's solver (Part 1) */
        amp = 0.0;                               /* largest |x| in the last quarter */
        for (i = 3 * n / 4; i < n; i++)
        {
            x_abs = run[i].x;
            if (x_abs < 0.0)                     /* absolute value */
            {
                x_abs = -x_abs;
            }
            if (x_abs > amp)
            {
                amp = x_abs;
            }
        }
        w_list[j] = w;
        amp_list[j] = amp;
    }
    free(run);                                   /* release the run array */

    if ((gp = fopen("plot_resonance.gp", "w")) == NULL)
    {
        printf(" [Error] Could not create plot_resonance.gp\n");
        return;
    }
    fprintf(gp, "set title 'Resonance curve (m = %g kg, c = %g N*s/m, k = %g N/m, F0 = %g N)'\n",
            q.m, q.c, q.k, q.F0);
    fprintf(gp, "set xlabel 'Forcing frequency w (rad/s)'\n");
    fprintf(gp, "set ylabel 'Steady-state amplitude (m)'\n");
    fprintf(gp, "set grid\n");
    fprintf(gp, "set arrow from %g, graph 0 to %g, graph 1 nohead dt 2\n", wn, wn);   /* line at wn */
    fprintf(gp, "plot '-' with lines lw 2 title 'Simulation', '-' with lines dt 2 title 'Formula'\n");
    for (j = 0; j < 100; j++)                    /* simulated points */
    {
        fprintf(gp, "%g %g\n", w_list[j], amp_list[j]);
    }
    fprintf(gp, "e\n");
    for (j = 0; j < 100; j++)                    /* formula X = F0 / sqrt((k - m w^2)^2 + (c w)^2) */
    {
        w = w_list[j];
        X = q.F0 / sqrt(pow(q.k - q.m * w * w, 2) + pow(q.c * w, 2));
        fprintf(gp, "%g %g\n", w, X);
    }
    fprintf(gp, "e\n");
    fprintf(gp, "pause -1 'Press Enter to close the graph'\n");
    fclose(gp);
    show_open_message("plot_resonance.gp");
}

void viz_menu(SimulationData *results, Params *p, int method)
{
    double choice;                               /* menu choice as typed */
    int option;                                  /* choice as a whole number, -1 if not whole */

    do
    {
        printf("\n===== Visualisation =====\n");
        printf("1. Displacement vs time\n");
        printf("2. Velocity vs time\n");
        printf("3. Acceleration vs time\n");
        printf("4. Compare damping regimes\n");
        printf("5. Resonance curve (amplitude vs forcing frequency)\n");
        printf("0. Exit\n");
        if (!read_number("Enter choice: ", &choice))   /* Ahmad's number reader (main.c) */
        {
            return;                              /* input closed: leave the menu */
        }
        option = (int)choice;
        if (option != choice)                    /* e.g. 1.5 is not a menu option */
        {
            option = -1;
        }
        switch (option)
        {
        case 1:
            write_time_plot(results, 1, "plot_displacement.gp", "Displacement vs time", "Displacement x (m)");
            break;
        case 2:
            write_time_plot(results, 2, "plot_velocity.gp", "Velocity vs time", "Velocity v (m/s)");
            break;
        case 3:
            write_time_plot(results, 3, "plot_acceleration.gp", "Acceleration vs time", "Acceleration a (m/s^2)");
            break;
        case 4:
            write_regimes_plot(results, p, method);
            break;
        case 5:
            write_resonance_plot(p, method);
            break;
        case 0:
            break;
        default:
            printf(" [Error] Please enter 0 to 5.\n");
            break;
        }
    } while (option != 0);
}
