#include <stdio.h>
#include <stdlib.h>
#include "msd.h"

int main()
{
    double m, c, k;
    double x0, v0;
    double t_total, dt;
    int choice;

    printf(" ======= Mass-Spring-Damper Simulation Setup ======= \n");
    printf("\n");


    printf(" 1. Gathering Inputs for System Paramters ");
    printf(" \n-------------------------------------------------- ");

    while (printf(" \nEnter mass (m in kg): "), scanf("%lf", &m) != 1 || m <= 0) {
        printf(" [Error] Invalid input. Mass must be greater than 0.\n");
        while (getchar() != '\n');
    }

    while (printf(" \nEnter damping coefficient (c in N*s/m): "), scanf("%lf", &c) != 1 || c < 0) {
        printf(" [Error] Invalid input. Damping cannot be negative.\n");
        while (getchar() != '\n');
    }

    while (printf(" \nEnter spring stiffness (K in N/m): "), scanf("%lf", &k) != 1 || k <= 0) {
        printf(" [Error] Invalid input. Stiffness must be greater than 0.\n");
        while (getchar() != '\n');
    }


    printf(" \n2. Gathering Inputs for Initial Conditions  ");
    printf(" \n-------------------------------------------------- ");

    while (printf(" \nEnter initial displacement (x0 in m): "), scanf("%lf", &x0) != 1) {
        printf(" [Error] Invalid input. Please enter a numeric value.\n");
        while (getchar() != '\n');
    }

    while (printf(" \nEnter initial velocity (v0 in m/s): "), scanf("%lf", &v0) != 1) {
        printf(" [Error] Invalid input. Please enter a numeric value.\n");
        while (getchar() != '\n');
    }



    printf(" \n3. Gathering Inputs for Simulation Time Parameters  ");
    printf(" \n-------------------------------------------------- ");

    while (printf(" \nEnter total simulation time (t_total in s): "), scanf("%lf", &t_total) != 1 || t_total <= 0) {
        printf(" [Error] Invalid input. Total simulation time must be greater than 0.\n");
        while (getchar() != '\n');
    }

    while (printf(" \nEnter time step size (dt in s): "), scanf("%lf", &dt) != 1 || dt <= 0 || dt > t_total) {
        printf(" [Error] Invalid input. dt must be > 0 and less than or equal to t_total.\n");
        while (getchar() != '\n');
    }

    printf("\nInputs received successfully. Ready to run simulation!\n");


    printf("\n =================================================== ");

    printf("\n");
    printf("\nPlease select your preferred method for the calculations");
    printf(" \n-------------------------------------------------- ");
    printf("\n1. Forward Euler's Method");
    printf("\n2. Runge-Kutta 4th Order Method");
    printf("\n3. Recommended Method");
    printf(" \n-------------------------------------------------- ");
    while (printf("\nEnter Choice: "), scanf("%d", &choice) != 1 || (choice < 1 || choice > 3)) {
    printf(" [Error] Invalid choice. Please enter 1, 2, or 3.\n");
    while (getchar() != '\n'); // Clear the bad input from the buffer
}

    Params p = {.m = m, .c = c, .k = k, .F0 = 0.0, .w = 0.0,
                .x0 = x0, .v0 = v0, .dt = dt, .t_total = t_total};    // pack inputs (no forcing yet)

    if (t_total / dt > 10000000) {                                    // stop runs too big for memory
        printf(" [Error] Too many steps. Use a larger dt.\n");          // tell user why
        return 1;                                                     // end program
    }
    int n = num_steps(&p) + 1;                                        // number of states incl. t = 0
    State *data = malloc(n * sizeof(State));                          // results array
    if (data == NULL) {                                               // memory not available
        printf(" [Error] Not enough memory.\n");                      // tell user why
        return 1;                                                     // end program
    }

    if(choice == 1)
    {
    simulate(data, n, &p, step_euler);                                // eulers method function
    }
    else if(choice == 2 || choice == 3 )
    {
    simulate(data, n, &p, step_rk4);                                  // rk4 method function ( recommended )
    }

    printf("\n%10s %12s %12s %14s\n", "t (s)", "x (m)", "v (m/s)", "a (m/s^2)");    // table header
    for (int i = 0; i < n; i++)                                                      // each stored state
        printf("%10.4f %12.6f %12.6f %14.6f\n", data[i].t, data[i].x, data[i].v, data[i].a);  // one row

    free(data);                                                       // release results array
    return 0;
}
