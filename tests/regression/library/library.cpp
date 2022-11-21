#include <iostream>

int func(int arg){
  //std::cerr << "printing: " << arg << "\n";
  int a = 0;
  a = arg;
  a = 5;

  return 1;
}

void func2(void){
  return;
}

int func3(void){
  return 3;
}

int func4(volatile char* p, int n, int s, int w){
  
  char t = 2;

  if(w){
  
    for(int i = 0; i < n; i+=s){
        p[i] = t;
      }
  }

  else{
    for(int i = 0; i < n; i+=s){
      t = p[i];
    }
  }

  return t;    

}
