#include <iostream>
#include <omp.h>

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

    #pragma omp parallel
    {
        int local_sum = 0;

        #pragma omp for
        for (int i = 0; i < upper_bound; ++i) { // what if upper bound was j which keeps changing
            int thread_id = omp_get_thread_num();
            int local_i = i;

            // Load the induction variable using a load instruction
            int& local_iv = local_i;
            int local_d = j;

            local_sum += array[local_iv];
            j--;
            std::cout << "checking the number of times it prints " << std::endl;
        }

        #pragma omp critical
        {
            sum += local_sum;
        }
    }

    std::cout << "Sum: " << sum << std::endl;

    return 0;
}