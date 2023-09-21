// Testing multi exits: also try this with unpredictable condition like taking input at runtime

#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

void check() {
    exit(0);
}

int main() {
    int start = 0;
    int sum = 0;
    int j = 20;
    int upper_bound = 10;
    int cap = 20;
    bool check = false, check_2 = false;
    int test = 60;
    int upper_bound_2 = 30;
    //int t = 0;

    // for(int k = 0 ; k < test ; k++) {
    //     sum += 1;
    // }

    #pragma omp parallel num_threads(4)
    {
        for(int i = start, t = 0 ; i < upper_bound_2 && t < test ; i++) {
            for(int j = 0 ; j < cap ; j++) {
                usleep(1000);
                std::cout<<"printing out"<<std::endl;
            }

            if(i % 2 == 0) {
                t = t+1;
            }

            else {
                t = t+2;
            }
        }

        std::cout<<"printing out outer"<<std::endl;
    }

    return 0;
}