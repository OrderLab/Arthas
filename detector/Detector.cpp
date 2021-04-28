#include <fstream>
#include <iostream>
#include <map>
#include <string>

extern "C" {
#include <execinfo.h>
#include <string.h>
}

using namespace std;

int main(int argc, char *argv[]) {

  if(strcmp(argv[1], "run") == 0){
    string temp_command;
    // string gdb = "gdb -x file ";
    string gdb = "gdb --args ";
    string line_command;

    cout << "Start System you wish to run \n";
    getline(cin, line_command);
    temp_command = gdb + line_command;
    const char *command = temp_command.c_str();

    system(command);

  }
  else if(strcmp(argv[1], "analyze") == 0){
    ifstream myfile("gdb.txt");

    std::size_t found;
    string line, line2, func_name, line_no, combined, token;
    string delimiter = ":";
    bool next_line = false;
 
    map<string, string> func_line_mapping;
    if (myfile.is_open()) {
      while (getline(myfile, line)) {
        if (line[0] == '#' || next_line) {
          found = line.find_last_of(' ');
          combined = line.substr(found + 1);
          if (combined.find(':') != std::string::npos){
             // found
          }
          else{
            // not found
             next_line = true;
             continue;
          }
          func_name = combined.substr(0, combined.find(delimiter));
          std::cout << "file: " << func_name << "\n";
          line_no = combined.substr(combined.find(delimiter) + 1);
          std::cout << "line no: " << line_no << "\n";
	  std::cout << "Run extractor with these inputs\n";
          break;
          //func_line_mapping.insert(pair<string, string>(func_name, line_no));
        }
      }
    }

  }
  else if(strcmp(argv[1], "fault") == 0){
    // Code to compare two different runs and detect if bug is pmem
    // or not
    string line, line2, func_name, line_no, combined, token;
    string delimiter = ":";
    std::size_t found;
    ifstream myfile("gdb.txt");
    ifstream myfile2("gdb2.txt");
    // std::vector <llvm::Value *> trace_values;

    // Iteration 1
    map<string, string> func_line_mapping;
    bool next_line = false;
    if (myfile.is_open()) {
      while (getline(myfile, line)) {
        if (line[0] == '#' || next_line) {
          found = line.find_last_of(' ');
          combined = line.substr(found + 1);
          if (combined.find(':') != std::string::npos){
             // found
             next_line = false;
          }
          else{
            // not found
             next_line = true;
             continue;
          }
          func_name = combined.substr(0, combined.find(delimiter));
          //std::cout << func_name << "\n";
          line_no = combined.substr(combined.find(delimiter) + 1);
          //std::cout << line_no << "\n";
          func_line_mapping.insert(pair<string, string>(func_name, line_no));
        }
      }
    }

    // Iteration 2
    map<string, string> func_line_mapping2;
    next_line = false;
    if (myfile2.is_open()) {
      while (getline(myfile2, line2)) {
        if (line2[0] == '#') {
          found = line2.find_last_of(' ');
          combined = line2.substr(found + 1);
          if (combined.find(':') != std::string::npos){
             // found
             next_line = false;
          }
          else{
            // not found
             next_line = true;
             continue;
          }
          func_name = combined.substr(0, combined.find(delimiter));
          //std::cout << func_name << "\n";
          line_no = combined.substr(combined.find(delimiter) + 1);
          //std::cout << line_no << "\n";
          func_line_mapping2.insert(pair<string, string>(func_name, line_no));
        }
      }
    }

    // Compare lines instead??
    myfile.clear();
    myfile.seekg(0);
    myfile2.clear();
    myfile2.seekg(0);
    next_line = false;
    bool hard_fault = true;
    if (myfile.is_open() && myfile2.is_open()) {
      while (getline(myfile, line) && getline(myfile2, line2)) {
        if ((line[0] == '#' && line2[0] == '#' )|| next_line) {
          found = line2.find_last_of(' ');
          combined = line2.substr(found + 1);
          std::size_t found2 = line2.find_last_of(' ');
          string combined2 = line2.substr(found2 + 1);
          if (combined.find(':') != std::string::npos &&
              combined2.find(':') != std::string::npos){
             // found
             next_line = false;
          }
          else{
            // not found
             next_line = true;
             continue;
          }
          if (combined != combined2) {
            std::cout << "not persistent \n";
            std::cout << combined << " and " << combined2 << "\n";
            hard_fault = false;
          }
        }
      }
      if(!hard_fault)
        std::cout << "Result: likely not a hard fault\n";
      else
        std::cout << "Result: likely a hard fault\n";
    }
  }
  else{
    printf("please use options run, analyze or fault as argument\n");
  }
  /*void *callstack[128];
  int i, frames = backtrace(callstack, 128);
  char** strs = backtrace_symbols(callstack, frames);
  for (i = 0; i < frames; ++i) {
    printf("STACK TRACE: %s\n", strs[i]);
  }
  free(strs);*/



}
