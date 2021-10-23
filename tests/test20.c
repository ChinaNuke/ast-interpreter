extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

   
int  fibonacci(int b) {
   int c;
   if (b < 2)
      return b;
   c = fibonacci(b-1) + fibonacci(b-2);
   return c;
}  
   
int main() {
   int a;
   int b;
   a = 5;

   b = fibonacci(5);
   PRINT(b);
   return 0;
}

