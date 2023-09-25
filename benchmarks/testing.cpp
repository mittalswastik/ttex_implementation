#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

void check(){
    std::cout << "calling checkout function" << std::endl;
}

int main() {
    const int size = 30;
    int array[size];

    // Initialize the array
    for (int i = 0; i < size; ++i) {
        array[i] = i + 1;
    }

    int sum = 0;
    int j = 20;
    int upper_bound = 10;
    // int local_sum = 0;

    std::cout << "tesing" << std::endl;

    // for (int i = 0; i < j; ++i) {
    //         int thread_id = omp_get_thread_num();
    //         int local_i = i;

    //         // Load the induction variable using a load instruction
    //         int& local_iv = local_i;
    //         int local_d = j;

    //         local_sum += array[local_iv];
    //         j--;
    //         std::cout << "checking the number of times it prints " << std::endl;
    //     }

    #pragma omp parallel num_threads(4)
    {
        int local_sum = 0;
        int upper_val = 30;
        int upper_inner_val = 20;

        for(int i = 0 ; i < upper_val ; i++) {
            for(int j = 0 ; j < upper_inner_val ; j++) {
                check();
            }
        }

        // #pragma omp single
        // {
        //     std::cout << "tesing single" << std::endl;
        // }

        #pragma omp for
        for (int i = 0; i < upper_bound; ++i) { // what if upper bound was j which keeps changing
            int thread_id = omp_get_thread_num();
            int local_i = i;

            // Load the induction variable using a load instruction
            int& local_iv = local_i;
            int local_d = j;

            local_sum += array[local_iv];
            j--;
            //std::cout << "checking the number of times it prints " << std::endl;
        }

        #pragma omp sections
        {
            #pragma omp section
            {
                for(int i = 0 ; i < 20 ; i++) {
                    check();
                }

                // this above loop should work for.inc and for.body .... check why sections loop is processed
                //std::cout<< "testing section 1" << std::endl;
            }

            #pragma omp section
            {
                std::cout <<"testing section 2" << std::endl;
            }
        }

        // #pragma omp critical
        // {
        //     sum += local_sum;
        // }

        // #pragma omp sections
        // {
        //     #pragma omp section
        //     {
        //         std::cout<<"testing sections end"<<std::endl;
        //     }

        //     #pragma omp section
        //     {
        //         std::cout<<"testing sections end2"<<std::endl;
        //     }
        // }
    }

    // #pragma omp parallel
    // {
    //     #pragma omp sections
    //     {
    //         #pragma omp section
    //         {
    //             std::cout<<"testing section3"<<std::endl;
    //         }

    //         #pragma omp section
    //         {
    //             std::cout<<"testing section4"<<std::endl;
    //         }
    //     }
    // }

    std::cout << "Sum: " << sum << std::endl;

    return 0;
}