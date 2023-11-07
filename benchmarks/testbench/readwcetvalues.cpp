#include <bits/stdc++.h>

using namespace std;

typedef struct loop_details_pass {
  int parallel_id;
  int loop_id;
  int split_factor;
  int unique_loop_id;
  int seq_split;
  int total_inst;
  long int wcet_ns;
  int total_threads;
  std::vector<int> unique_function_ids;
} loop_details_pass;

typedef struct para_details {
  int parallel_id;
  int id;
  int ref;
  int seq_split;
  int total_inst;
  long int wcet_ns;
  int total_threads;
  std::vector<int> unique_function_ids;
} para_details;

int main(){

    std::vector< std::vector<loop_details_pass> > l_data_temp;
  std::vector< std::vector<para_details> > p_data_temp;  


    std::ifstream inFile("data_log_to_pass.txt", std::ios::binary);

    if (inFile) {
        // Read the data from the file
        size_t vectorSizeRow;
        inFile.read(reinterpret_cast<char*>(&vectorSizeRow), sizeof(vectorSizeRow));
        l_data_temp.resize(vectorSizeRow);
        for (auto& row : l_data_temp) {
            size_t vectorSizeColumn;
            inFile.read(reinterpret_cast<char*>(&vectorSizeColumn), sizeof(vectorSizeColumn));
            row.resize(vectorSizeColumn);
            for (auto& cell : row) {
                inFile.read(reinterpret_cast<char*>(&cell), sizeof(loop_details_pass));
            }
        }

        size_t vectorSize2Row;
        inFile.read(reinterpret_cast<char*>(&vectorSize2Row), sizeof(vectorSize2Row));
        p_data_temp.resize(vectorSize2Row);
        for (auto& row : p_data_temp) {
            size_t vectorSize2Column;
            inFile.read(reinterpret_cast<char*>(&vectorSize2Column), sizeof(vectorSize2Column));
            row.resize(vectorSize2Column);
            for (auto& cell : row) {
                inFile.read(reinterpret_cast<char*>(&cell), sizeof(para_details));
            }
        }

        inFile.close();
        
        // // // Print the read data
        for (const auto& item : l_data_temp) {
          for(const loop_details_pass& item_2: item) {
            std::cout<<"Wcet of loop" << item_2.wcet_ns << std::endl;
          }

          std::cout<<std::endl;
        }

        for (const auto& item : p_data_temp) {
          for(const para_details& item_2: item) {
            std::cout<<"Wcet of regions:" << item_2.wcet_ns << std::endl;
          }

          std::cout<<std::endl;
        }

    } else {
        std::cerr << "Error opening the file for reading." << std::endl;
    }

    return 0;
}