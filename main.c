/* main.c - Part 3 (Ahmad): input menu and program flow */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "msd.h"

int read_number(char prompt[], double *value)
{
    char text[81];                               /* what the user typed */
    char *end;                                   /* where strtod stopped reading */
    int valid;                                   /* 1 when text is a proper number */

    do
    {
        printf("%s", prompt);
        if (scanf("%80s", text) == EOF)          /* input closed: give up */
        {
            return 0;
        }
        *value = strtod(text, &end);             /* convert text to a number */
        valid = 1;
        if (end == text || *end != '\0')         /* no number read, or extra characters after it */
        {
            valid = 0;
        }
        else if (*value != *value || *value > 1.0e300 || *value < -1.0e300)   /* "nan" or "inf" */
        {
            valid = 0;
        }
        if (valid == 0)
        {
            printf(" [Error] Invalid input. Please enter a number.\n");
        }
    } while (valid == 0);                        /* ask again until it is a number */

    return 1;
}

int main()
{
    Params params;                               /* all inputs in one structure */
    SimulationData results;                      /* the results array (Part 3, Syed) */
    double choice;                               /* menu choice as typed */
    int method;                                  /* METHOD_EULER or METHOD_RK4 */
    int steps;                                   /* number of time steps */

    simulation_data_init(&results);

    printf("======= Mass-Spring-Damper Simulation =======\n\n");

    /* 1. System parameters */
    printf("1. System parameters\n");
    if (!read_number("Enter mass (m in kg): ", &params.m) || params.m <= 0.0)
    {
        printf("[Error] Mass must be greater than 0.\n");
        return 1;
    }
    if (!read_number("Enter damping coefficient (c in N*s/m): ", &params.c) || params.c < 0.0)
    {
        printf("[Error] Damping must be 0 or greater.\n");
        return 1;
    }
    if (!read_number("Enter spring stiffness (k in N/m): ", &params.k) || params.k <= 0.0)
    {
        printf("[Error] Stiffness must be greater than 0.\n");
        return 1;
    }

    /* 2. Initial conditions */
    printf("\n2. Initial conditions\n");
    if (!read_number("Enter initial displacement (x0 in m): ", &params.x0))
    {
        return 1;
    }
    if (!read_number("Enter initial velocity (v0 in m/s): ", &params.v0))
    {
        return 1;
    }

    /* 3. External force F(t) = F0 sin(w t) */
    printf("\n3. External force F(t) = F0 * sin(w * t)  (enter 0 for no force)\n");
    if (!read_number("Enter forcing amplitude (F0 in N): ", &params.F0) || params.F0 < 0.0)
    {
        printf("[Error] Forcing amplitude must be 0 or greater.\n");
        return 1;
    }
    params.w = 0.0;                              /* no force: frequency not needed */
    if (params.F0 > 0.0)                         /* only ask when there is a force */
    {
        if (!read_number("Enter forcing frequency (w in rad/s): ", &params.w) || params.w <= 0.0)
        {
            printf("[Error] Forcing frequency must be greater than 0.\n");
            return 1;
        }
        printf("Natural frequency wn = sqrt(k/m) = %.4f rad/s (resonance near w = wn)\n",
               sqrt(params.k / params.m));
    }

    /* 4. Simulation time */
    printf("\n4. Simulation time parameters\n");
    if (!read_number("Enter total simulation time (t_total in s): ", &params.t_total) || params.t_total <= 0.0)
    {
        printf("[Error] Total simulation time must be greater than 0.\n");
        return 1;
    }
    if (!read_number("Enter time step size (dt in s): ", &params.dt) || params.dt <= 0.0 || params.dt > params.t_total)
    {
        printf("[Error] dt must be greater than 0 and no larger than t_total.\n");
        return 1;
    }
    if (params.t_total / params.dt > MAX_STEPS)  /* stop a tiny dt using too much memory */
    {
        printf("[Error] Too many steps. Use a larger dt or a shorter time.\n");
        return 1;
    }

    /* 5. Numerical method */
    printf("\n5. Numerical method\n");
    printf("1. Forward Euler's Method\n");
    printf("2. Runge-Kutta 4th Order Method (RK4)\n");
    printf("3. Recommended Method (RK4)\n");
    if (!read_number("Enter choice (1-3): ", &choice) ||
        (choice != 1.0 && choice != 2.0 && choice != 3.0))
    {
        printf("[Error] Please select 1, 2 or 3.\n");
        return 1;
    }
    if (choice == 1.0)
    {
        method = METHOD_EULER;
    }
    else
    {
        method = METHOD_RK4;
    }

    /* Run, store, print and save */
    steps = num_steps(&params);                  /* Part 1 (Aqil) */
    if (steps < 1)
    {
        printf("[Error] The number of steps is invalid.\n");
        return 1;
    }
    if (!simulation_data_allocate(&results, steps + 1))   /* Part 3 (Syed) */
    {
        printf("[Error] Unable to allocate memory for the results.\n");
        return 1;
    }

    if (method == METHOD_EULER)
    {
        printf("\nRunning simulation using Forward Euler...\n");
    }
    else
    {
        printf("\nRunning simulation using Runge-Kutta 4th Order (RK4)...\n");
    }
    simulate(results.states, results.count, &params, method);   /* Part 1 (Aqil) */

    simulation_data_print(&results);             /* Part 3 (Syed) */
    if (!simulation_data_export_csv(&results, CSV_FILE))
    {
        printf("[Error] Simulation completed, but CSV export failed. Is the file open in Excel?\n");
        simulation_data_free(&results);
        return 1;
    }
    printf("\nSimulation completed successfully.\n");
    printf("%d data points stored.\n", results.count);
    printf("Results exported to %s.\n", CSV_FILE);

    viz_menu(&results, &params, method);         /* Part 4 (Haziq and Subhas) */

    simulation_data_free(&results);              /* Part 3 (Syed) */
    return 0;
}
