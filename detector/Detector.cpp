#include <string>
#include <iostream>

using namespace std;

int main(){
  string temp_command;
  string gdb = "gdb ";
  string line_command;

  cout << "Start System you wish to run \n";
  getline(cin, line_command);
  temp_command = gdb + line_command;
  const char *command = temp_command.c_str();
  
  system(command);

}
