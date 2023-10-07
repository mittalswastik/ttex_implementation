// Testing breaks within the loop


#include <iostream>
#include <omp.h>
#include "ttex_pass_update.h"

const int upper_val = 20;
int t = 0;
int a[upper_val];

void check(){
    std::cout << "Checking";
}

int main() {

    // int upper_val = 20;

    for(int i = 0 ; i < upper_val ; i++) {
        a[i] = i;
    }

    #pragma omp parallel
    {
        #pragma omp for
        {
          for(int i = 0 ; i < upper_val ; i++) {
            for(int j = 0 ;  j < a[i] ; j++){
                if(j % 2 == 0) {
                    t+=3;
                }

                else {
                    t-=1;
                }

                while(t!=-1){
                    a[j] = t*2;
                    t--;
                }
            }
          }  
        }
    }

    return 0;
}