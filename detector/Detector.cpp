#include <string>
#include <iostream>

using namespace std;

int main(){
  string temp_command;
  
  cout << "Start System you wish to run \n";
  getline(cin, temp_command);
  const char *command = temp_command.c_str();
  
  system(command);

}
