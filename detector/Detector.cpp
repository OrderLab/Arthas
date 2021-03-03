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
  string temp_command;
  // string gdb = "gdb -x file ";
  string gdb = "gdb --args ";
  string line_command;

  /*cout << "Start System you wish to run \n";
  getline(cin, line_command);
  temp_command = gdb + line_command;
  const char *command = temp_command.c_str();

  system(command);*/

  /*void *callstack[128];
  int i, frames = backtrace(callstack, 128);
  char** strs = backtrace_symbols(callstack, frames);
  for (i = 0; i < frames; ++i) {
    printf("STACK TRACE: %s\n", strs[i]);
  }
  free(strs);*/
  string line, line2, func_name, line_no, combined, token;
  string delimiter = ":";
  std::size_t found;
  ifstream myfile("gdb.txt");
  ifstream myfile2("gdb2.txt");
  // std::vector <llvm::Value *> trace_values;

  // Iteration 1
  map<string, string> func_line_mapping;
  if (myfile.is_open()) {
    while (getline(myfile, line)) {
      if (line[0] == '#') {
        found = line.find_last_of(' ');
        combined = line.substr(found + 1);
        func_name = combined.substr(0, combined.find(delimiter));
        std::cout << func_name << "\n";
        line_no = combined.substr(combined.find(delimiter) + 1);
        std::cout << line_no << "\n";
        func_line_mapping.insert(pair<string, string>(func_name, line_no));
      }
    }
  }

  // Iteration 2
  map<string, string> func_line_mapping2;
  if (myfile2.is_open()) {
    while (getline(myfile2, line2)) {
      if (line2[0] == '#') {
        found = line2.find_last_of(' ');
        combined = line2.substr(found + 1);
        func_name = combined.substr(0, combined.find(delimiter));
        std::cout << func_name << "\n";
        line_no = combined.substr(combined.find(delimiter) + 1);
        std::cout << line_no << "\n";
        func_line_mapping2.insert(pair<string, string>(func_name, line_no));
      }
    }
  }

  // Compare lines instead??
  if (myfile.is_open() && myfile2.is_open()) {
    while (getline(myfile, line) && getline(myfile2, line2)) {
      if (line[0] == '#' && line2[0] == '#') {
        found = line2.find_last_of(' ');
        combined = line2.substr(found + 1);
        std::size_t found2 = line2.find_last_of(' ');
        string combined2 = line2.substr(found2 + 1);
        if (combined != combined2) {
          std::cout << "not persistent \n";
          std::cout << combined << " and " << combined2 << "\n";
        }
      }
    }
  }

  /*if (myfile.is_open() && myfile2.is_open()){
     while ( getline (myfile, line) && getline(myfile2, line2)){
       if(line[0] == '#' && line2[0] == '#'){
         if(line != line2){
           cout << line << "\n";
         }
         cout << "DIFFERENT LINES\n";
         cout << line << "\n";
         cout << line2 << "\n";
         // TODO: use locator to collect llvm instruction
         // trace_values.insert(result);
       }
     }
  }*/

  /*bool persistent_flag = false;
  PMemVariableLocator locator;
  locator.runOnFunction(*F);
  for (auto vi = locator.var_begin(); vi != locator.var_end(); ++vi) {
    for (auto t = trace_values.begin(); t != trace_values.end();
         ++trace_values) {
      if(*vi == *t){
        //Persistent Value is involved
        persistent_flag = true;
        break;
      }
    }
  }*/
}
