#include <iostream>
#include <omp.h>

int main() {
    const int size = 10;
    int array[size];

    // Initialize the array
    for (int i = 0; i < size; ++i) {
        array[i] = i + 1;
    }

    int sum = 0;

    #pragma omp parallel
    {
        int local_sum = 0;

        #pragma omp for
        for (int i = 0; i < size; ++i) {
            int thread_id = omp_get_thread_num();
            int local_i = i;

            // Load the induction variable using a load instruction
            int& local_iv = local_i;

            local_sum += array[local_iv];
        }

        #pragma omp critical
        {
            sum += local_sum;
        }
    }

    std::cout << "Sum: " << sum << std::endl;

    return 0;
}