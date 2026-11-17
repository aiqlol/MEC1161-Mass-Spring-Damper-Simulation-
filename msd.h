/* msd.h - shared definitions for the mass-spring-damper simulation       */
/* One header for the whole program (Lecture 2: header files hold the     */
/* definitions and constants that several source files share)            */

#define PI 3.141592654             /* pi, as defined in the lectures */
#define METHOD_EULER 1             /* menu choice 1 = forward Euler */
#define METHOD_RK4 2               /* menu choice 2 or 3 = RK4 */
#define MAX_STEPS 10000000.0       /* largest number of time steps allowed */
#define CSV_FILE "simulation_results.csv"   /* name of the exported results file */

/* ---------- Part 1 (Aqil): model and solvers, model.c ---------- */

typedef struct
{
    double m, c, k;        /* mass (kg), damping (N*s/m), stiffness (N/m) */
    double F0, w;          /* force amplitude (N), forcing frequency (rad/s) */
    double x0, v0;         /* initial displacement (m), initial velocity (m/s) */
    double dt, t_total;    /* time step (s), total simulation time (s) */
} Params;

typedef struct
{
    double t, x, v, a;     /* time (s), displacement (m), velocity (m/s), acceleration (m/s^2) */
} State;

double accel(double t, double x, double v, Params *p);       /* a = (F - c*v - k*x) / m */
void step_euler(State *s, Params *p);                         /* move s forward one step (Euler) */
void step_rk4(State *s, Params *p);                           /* move s forward one step (RK4) */
int num_steps(Params *p);                                     /* number of time steps in t_total */
void simulate(State out[], int n, Params *p, int method);     /* fill out[0..n-1] with the chosen method */

/* ---------- Part 3 (Syed): data storage and CSV, data.c ---------- */

typedef struct
{
    State *states;         /* array of results, one State per time step */
    int count;             /* number of States in the array */
} SimulationData;

void simulation_data_init(SimulationData *data);                       /* start with an empty array */
int simulation_data_allocate(SimulationData *data, int count);         /* make room for count States */
void simulation_data_free(SimulationData *data);                       /* release the array */
int simulation_data_print(SimulationData *data);                       /* print the results table */
int simulation_data_export_csv(SimulationData *data, char filename[]); /* save the results to a CSV file */
State *simulation_data_get(SimulationData *data, int index);           /* address of one State, or NULL */

/* ---------- Part 3 (Ahmad): input menu, main.c ---------- */

int read_number(char prompt[], double *value);                /* ask until the user types a number */

/* ---------- Part 4 (Haziq and Subhas): visualisation, viz.c ---------- */

void viz_menu(SimulationData *results, Params *p, int method); /* graph menu after a run */
