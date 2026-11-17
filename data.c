/* data.c - Part 3 (Syed): storing, printing and exporting the results */
#include <stdio.h>
#include <stdlib.h>
#include "msd.h"

void simulation_data_init(SimulationData *data)
{
    data->states = NULL;                         /* no array yet */
    data->count = 0;                             /* no States yet */
}

int simulation_data_allocate(SimulationData *data, int count)
{
    State *new_states;                           /* address of the new array */

    if (count <= 0)                              /* nothing to allocate */
    {
        return 0;
    }

    new_states = (State *)calloc(count, sizeof(State));   /* count States, all set to 0 */
    if (new_states == NULL)                      /* not enough memory */
    {
        return 0;
    }

    free(data->states);                          /* release any old array (free(NULL) is safe) */
    data->states = new_states;                   /* keep the new array */
    data->count = count;                         /* remember its size */
    return 1;
}

void simulation_data_free(SimulationData *data)
{
    free(data->states);                          /* return the memory to the heap */
    data->states = NULL;                         /* stop the old address being used */
    data->count = 0;
}

int simulation_data_print(SimulationData *data)
{
    int i;                                       /* row counter */

    if (data->states == NULL || data->count <= 0)   /* nothing to print */
    {
        return 0;
    }

    printf("\n%10s %12s %12s %14s\n", "t (s)", "x (m)", "v (m/s)", "a (m/s^2)");   /* table heading */
    for (i = 0; i < data->count; i++)            /* one row per State */
    {
        printf("%10.4f %12.6f %12.6f %14.6f\n",
               data->states[i].t, data->states[i].x, data->states[i].v, data->states[i].a);
    }
    return 1;
}

int simulation_data_export_csv(SimulationData *data, char filename[])
{
    FILE *file_ptr;                              /* the CSV file */
    int i;                                       /* row counter */

    if (data->states == NULL || data->count <= 0)   /* nothing to save */
    {
        return 0;
    }

    if ((file_ptr = fopen(filename, "w")) == NULL)  /* open for writing (overwrites old file) */
    {
        return 0;                                /* could not open, e.g. file open in Excel */
    }

    fprintf(file_ptr, "time_s,displacement_m,velocity_m_per_s,acceleration_m_per_s2\n");   /* column names */
    for (i = 0; i < data->count; i++)            /* one line per State */
    {
        fprintf(file_ptr, "%.10g,%.10g,%.10g,%.10g\n",
                data->states[i].t, data->states[i].x, data->states[i].v, data->states[i].a);
    }

    if (fclose(file_ptr) != 0)                   /* fclose returns 0 when the file is saved */
    {
        return 0;
    }
    return 1;
}

State *simulation_data_get(SimulationData *data, int index)
{
    if (data->states == NULL || index < 0 || index >= data->count)   /* index outside the array */
    {
        return NULL;
    }
    return &data->states[index];                 /* address of the requested State */
}
