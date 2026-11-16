#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L                           // makes popen visible on Mac / Linux
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "viz.h"

#ifdef _WIN32
#define POPEN _popen                                      // Windows name for popen
#define PCLOSE _pclose                                    // Windows name for pclose
#else
#define POPEN popen                                       // Mac / Linux name
#define PCLOSE pclose
#endif

static FILE *open_gnuplot(void)
{
    FILE *gp = POPEN("gnuplot -persistent", "w");         // start Gnuplot, keep window open after
    if (gp == NULL)                                        // Gnuplot missing or not on PATH
        printf(" [Error] Could not start Gnuplot. Is it installed and on PATH?\n");
    return gp;                                             // NULL means no plot
}

static void plot_one(const State *data, int n, int column, const char *title, const char *ylabel)
{
    FILE *gp = open_gnuplot();                             // open a pipe to Gnuplot
    if (gp == NULL) return;                                // stop if Gnuplot did not start
    fprintf(gp, "set title '%s'\n", title);                // graph title
    fprintf(gp, "set xlabel 'Time t (s)'\n");              // x-axis label
    fprintf(gp, "set ylabel '%s'\n", ylabel);              // y-axis label
    fprintf(gp, "set grid\n");                             // background grid
    fprintf(gp, "plot '-' using 1:2 with lines lw 2 notitle\n");   // read points sent below
    for (int i = 0; i < n; i++) {                          // send every time step
        double y = (column == 1) ? data[i].x : (column == 2) ? data[i].v : data[i].a;  // pick x, v or a
        fprintf(gp, "%g %g\n", data[i].t, y);              // one point: time, value
    }
    fprintf(gp, "e\n");                                    // end of data
    fflush(gp);                                            // make Gnuplot draw now
    PCLOSE(gp);                                            // close the pipe
}

static void plot_regimes(int n, const Params *p, void (*step)(State *, const Params *))
{
    double cc = 2.0 * sqrt(p->m * p->k);                   // critical damping c = 2*sqrt(m*k)
    double cs[3] = {0.2 * cc, cc, 3.0 * cc};               // under, critical, over
    const char *names[3] = {"Underdamped", "Critically damped", "Overdamped"};
    State *run = malloc((size_t)n * sizeof(State));        // array for one re-run
    if (run == NULL) { printf(" [Error] Not enough memory.\n"); return; }
    FILE *gp = open_gnuplot();                             // open a pipe to Gnuplot
    if (gp == NULL) { free(run); return; }                 // stop if Gnuplot did not start
    fprintf(gp, "set title 'Damping regimes (m = %g kg, k = %g N/m)'\n", p->m, p->k);
    fprintf(gp, "set xlabel 'Time t (s)'\nset ylabel 'Displacement x (m)'\nset grid\n");
    fprintf(gp, "plot '-' with lines lw 2 title '%s', '-' with lines lw 2 title '%s', "
                "'-' with lines lw 2 title '%s'\n", names[0], names[1], names[2]);   // three lines
    for (int r = 0; r < 3; r++) {                          // each regime
        Params q = *p;                                     // copy the user's inputs
        q.c = cs[r];                                       // change only the damping
        simulate(run, n, &q, step);                        // re-run with the same solver
        for (int i = 0; i < n; i++)                        // send this run's points
            fprintf(gp, "%g %g\n", run[i].t, run[i].x);
        fprintf(gp, "e\n");                                // end of this line's data
    }
    fflush(gp);                                            // make Gnuplot draw now
    PCLOSE(gp);                                            // close the pipe
    free(run);                                             // release the re-run array
}

static void plot_resonance(const Params *p, void (*step)(State *, const Params *))
{
    if (p->c <= 0.0) {                                     // no damping: amplitude never settles
        printf(" [Error] The resonance curve needs damping c > 0.\n");
        return;
    }
    double wn = sqrt(p->k / p->m);                         // natural frequency
    double tau = 2.0 * p->m / p->c;                        // time for the start-up motion to fade
    double t_end = fmax(8.0 * tau, 40.0 * 3.14159265358979 / (0.2 * wn));  // long enough to settle
    Params q = *p;                                         // copy the user's inputs
    q.F0 = (p->F0 > 0.0) ? p->F0 : 1.0;                    // use 1 N if the user had no force
    q.x0 = 0.0;                                            // start at rest
    q.v0 = 0.0;
    q.t_total = t_end;
    q.dt = fmin(p->dt, (2.0 * 3.14159265358979 / (3.0 * wn)) / 50.0);  // 50+ steps per fastest cycle
    if (t_end / q.dt > 200000.0) q.dt = t_end / 200000.0;  // limit the work per run
    int n = num_steps(&q) + 1;                             // states per run
    State *run = malloc((size_t)n * sizeof(State));        // array for one run
    if (run == NULL) { printf(" [Error] Not enough memory.\n"); return; }
    FILE *gp = open_gnuplot();                             // open a pipe to Gnuplot
    if (gp == NULL) { free(run); return; }
    printf("Running 100 simulations, please wait...\n");
    fprintf(gp, "set title 'Resonance curve (m = %g kg, c = %g N*s/m, k = %g N/m, F0 = %g N)'\n",
            q.m, q.c, q.k, q.F0);
    fprintf(gp, "set xlabel 'Forcing frequency w (rad/s)'\nset ylabel 'Steady-state amplitude (m)'\n");
    fprintf(gp, "set grid\nset arrow from %g, graph 0 to %g, graph 1 nohead dt 2\n", wn, wn);  // line at wn
    fprintf(gp, "plot '-' with lines lw 2 title 'Simulation', '-' with lines dt 2 title 'Formula'\n");
    for (int j = 0; j < 100; j++) {                        // 100 forcing frequencies
        q.w = wn * (0.2 + 2.8 * j / 99.0);                 // from 0.2 wn to 3 wn
        simulate(run, n, &q, step);                        // run with the user's solver
        double amp = 0.0;                                  // largest |x| in the last quarter
        for (int i = 3 * n / 4; i < n; i++)
            if (fabs(run[i].x) > amp) amp = fabs(run[i].x);
        fprintf(gp, "%g %g\n", q.w, amp);                  // one point: w, amplitude
    }
    fprintf(gp, "e\n");                                    // end of simulation data
    for (int j = 0; j < 100; j++) {                        // formula for comparison
        double ww = wn * (0.2 + 2.8 * j / 99.0);
        double X = q.F0 / sqrt(pow(q.k - q.m * ww * ww, 2) + pow(q.c * ww, 2));  // X = F0/sqrt(...)
        fprintf(gp, "%g %g\n", ww, X);
    }
    fprintf(gp, "e\n");                                    // end of formula data
    fflush(gp);                                            // make Gnuplot draw now
    PCLOSE(gp);                                            // close the pipe
    free(run);                                             // release the run array
}

void viz_menu(const State *data, int n, const Params *p, void (*step)(State *, const Params *))
{
    int choice = -1;                                       // menu choice
    while (choice != 0) {                                  // repeat until Exit
        printf("\n===== Visualisation =====\n");
        printf("1. Displacement vs time\n");
        printf("2. Velocity vs time\n");
        printf("3. Acceleration vs time\n");
        printf("4. Compare damping regimes\n");
        printf("5. Resonance curve (amplitude vs forcing frequency)\n");
        printf("0. Exit\n");
        printf("Enter choice: ");
        if (scanf("%d", &choice) != 1) {                   // not a number
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF); // clear the bad input
            if (ch == EOF) return;                         // input closed: leave the menu
            printf(" [Error] Please enter 0 to 5.\n");
            choice = -1;                                   // stay in the menu
            continue;
        }
        if (choice == 1) plot_one(data, n, 1, "Displacement vs time", "Displacement x (m)");
        else if (choice == 2) plot_one(data, n, 2, "Velocity vs time", "Velocity v (m/s)");
        else if (choice == 3) plot_one(data, n, 3, "Acceleration vs time", "Acceleration a (m/s^2)");
        else if (choice == 4) plot_regimes(n, p, step);
        else if (choice == 5) plot_resonance(p, step);
        else if (choice != 0) printf(" [Error] Please enter 0 to 5.\n");
    }
}
