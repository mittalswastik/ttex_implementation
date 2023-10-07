// Testing breaks within the loop


#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

int main() {
    int sum = 0;
    int j = 20;
    int upper_bound = 10;
    int cap = 20;

    #pragma omp parallel
    {
        #pragma omp for
        {
            for(int i = 0 ; i < upper_bound ; i++) {
                for(int j = 0 ; j < cap ; j++) {
                    usleep(1000);
                    if(j == upper_bound){
                        break;
                    }
                }
            }
        }
    }

    return 0;
}