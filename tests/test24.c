extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

    
int mul (int a, int b) {
   return a*b;
}

int foo(int b) {
   int c;
   if (b < 2) 
     return b;
   c = b* foo(b-1);
   return c;
}
  
int main() {
   int a, b, c, d;

   a = 2;
   b = 3;
   c = mul(a, b);
   d = foo(c);
   
   PRINT(d); 
   return 0;
}


