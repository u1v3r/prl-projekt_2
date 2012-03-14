#ifndef PMS_H_INCLUDED
#define PMS_H_INCLUDED

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>

#define FILENAME "numbers"
#define TAG 0
#define INIT_NUMBERS_SIZE 64    /* pocet cisel pre ktore bude inicializovana pamat */

void print_numbers(int *numbers,int length);

#endif // PMS_H_INCLUDED
