extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int b=10;
int f(int x,int y) {
  if (y > 0) 
  	return x + f(x,y-1);
  else 
    return 0;
}
int main() {
   int a=2;
   PRINT(f(b,a));
}


#20