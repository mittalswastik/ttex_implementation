#include <bits/stdc++.h>

typedef struct loop_details_pass {
  int parallel_id;
  int loop_id;
  int split_factor;
  int unique_loop_id;
  int seq_split;
  long int total_inst;
  unsigned long int wcet_ns;
  int total_threads;
  int fns;
  int unique_function_ids[500];
} loop_details_pass;

typedef struct para_details {
  int parallel_id;
  int id;
  int ref;
  int seq_split;
  long int total_inst;
  unsigned long int wcet_ns;
  int total_threads;
  int fns;
  int unique_function_ids[500];
} para_details;

loop_details_pass** l_data;
int *l_data_size;
// std::vector< std::vector<loop_details_pass> > l_data;
// std::vector< std::vector<para_details> > p_data;
para_details** p_data;
int *p_data_size;

using namespace std;

int main(){
  std::vector< std::vector<loop_details_pass> > l_data_temp;
  std::vector< std::vector<para_details> > p_data_temp;

  std::ifstream inFile("/home/swastik/dev/ttex/llvm/ttex_implementation/benchmarks/testbench/data_log_to_pass.txt", std::ios::binary);

  std::cout<<"file data read"<<std::endl;

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

        std::cout<<"read the loop details"<<std::endl;

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

        std::cout<<"read the para details"<<std::endl;

        inFile.close();
        

        int parallel_size = l_data_temp.size();
        l_data_size = (int*) malloc(l_data_temp.size() * sizeof(int));
        p_data_size = (int*) malloc(p_data_temp.size() * sizeof(int));

        std::cout<<"allocated sizes: "<<p_data_temp.size()<<std::endl;

        for (const auto& item : l_data_temp) {
          for(const loop_details_pass& item: item) {
            std::cout<<"loop id: " << item.loop_id <<"\n";
            std::cout<<"Parallel id: " << item.parallel_id <<"\n";
            std::cout<<"split factor: " << item.split_factor << "\n";
            std::cout<<"seq id: " << item.seq_split <<"\n";
            std::cout<<"wcet_ns: " << item.wcet_ns <<"\n";
            std::cout<<"total instructions: " <<item.total_inst<<"\n";
          }
        }

        std::cout<<"------------------------------------------------------------------"<<std::endl<<std::endl;

        for (const auto& item : p_data_temp) {
          for(const para_details& item_2: item) {
            std::cout<<"sub id: " << item_2.id <<"\n";
            std::cout<<"Parallel id: " << item_2.parallel_id <<"\n";
            std::cout<<"split factor: " << item_2.seq_split << "\n";
            std::cout<<"total instructions: " <<item_2.total_inst<<"\n";
            std::cout<<"wcet_ns: " << item_2.wcet_ns <<"\n";
            std::cout<<"region id is: " <<item_2.ref<<"\n";
          }
        }
    }

    return 0;
}
