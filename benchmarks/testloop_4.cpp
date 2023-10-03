// Testing multilevel loops

#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

int main() {
    int sum = 0;
    int j = 20;
    int upper_bound = 10;

    #pragma omp parallel num_threads(4)
    {
        for(int i = 0 ; i < sum ; i++) {
            for(int j = 0 ; j < upper_bound ; j++) {
                usleep(1000);
                for(int k = 0 ; k < upper_bound ; k++) {
                    usleep(1000);
                }
            }
        }
    }

    return 0;
}