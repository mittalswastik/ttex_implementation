// Testing codes between nested loops

#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

void check(){
    usleep(100000);
}

int main() {
    int sum = 0;
    int j = 20;
    int upper_bound = 10;

    #pragma omp parallel num_threads(4)
    {
        for(int i = 0 ; i < 30 ; i++) {
            for(int j = 0 ; j < 20 ; j++) {
                usleep(1000);
            }

            usleep(1000);
            usleep(1000);
            usleep(1000);
            usleep(1000);
            check();
        }
    }

    return 0;
}