// Testing breaks within the loop


#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

int main() {
    printf("-------------------------- main function ---------------------------\n");
    int sum = 0;
    int j = 20;
    int upper_bound = 10;
    int cap = 20;

    #pragma omp parallel num_threads(2)
    {
        #pragma omp for
        { 
            for(int i = 0 ; i < 4 ; i++) {
                sleep(5);
                for(int j = 0 ; j < cap ; j++) {
                    if(j == upper_bound){
                        break;
                    }
                }
            }
        }   
    }

    printf("------------------------------------- another parallel region ----------------------\n");

    #pragma omp parallel num_threads(2)
    {
        #pragma omp for
        { 
            for(int i = 0 ; i < 4 ; i++) {
                sleep(5);
                for(int j = 0 ; j < cap ; j++) {
                    if(j == upper_bound){
                        break;
                    }
                }
            }
        }   
    }

    std::cout<<"--------------------------------- end of parallel region -----------------------------"<<std::endl;

    return 0;
}