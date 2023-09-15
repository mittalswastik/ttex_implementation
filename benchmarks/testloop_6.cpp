// Testing multi exits: also try this with unpredictable condition like taking input at runtime

#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

void check() {
    exit(0);
}

int main() {
    int sum = 0;
    int j = 20;
    int upper_bound = 10;
    int cap = 20;
    bool check = false;

    #pragma omp parallel num_threads(4)
    {
        for(int i = 0 ; i < 30 ; i++) {
            for(int j = 0 ; j < cap ; j++) {
                usleep(1000);
                std::cout<<"printing out"<<std::endl;
                if(j == upper_bound) {
                    check = true;
                    break;
                }
            }

            if(check){
                break;
            }

        }
        std::cout<<"printing out outer"<<std::endl;
    }

    return 0;
}