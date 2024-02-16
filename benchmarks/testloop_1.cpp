// Testing breaks within the loop


#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

int main() {
    int sum = 0;
    int j = 20;
    int upper_bound = 10;
    int cap = 20;

    #pragma omp parallel num_threads(10)
    {

         std::cout<<"----------------------------- test called-----------------"<<std::endl;

        #pragma omp for
        { 
            for(int i = 0 ; i < 5 ; i++) {
                sleep(10);
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