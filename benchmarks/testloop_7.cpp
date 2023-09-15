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
    bool check = false, check_2 = false;
    int test = 30;
    int t = 0;

    for(int k = 0 ; k < test ; k++) {
        sum += 1;
    }

    #pragma omp parallel num_threads(4)
    {
        for(int i = 0 ; i < 30 ; i++) {
            for(int j = 0 ; j < cap ; j++) {
                usleep(1000);
                std::cout<<"printing out"<<std::endl;
                if(j == upper_bound) {
                    check = true;
                }

                if(j == 30){
                    check_2 = true;
                }
            }

            if(check_2) {
                i = i+2;
            }

            if(check){
                break;
            }

            if(check_2){
                break;
            }

            //t = t+1;

        }
        std::cout<<"printing out outer"<<std::endl;
    }

    return 0;
}