extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
   int a;
   int b = 0;
   a = 10;
   while ( b < a) {
     b = b + 1;
   }

   PRINT(b);
}
