extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
   int a;
   int b = 10;
   a = -10;
   if (a > 0 ) {
     b = a;
   } else {
     b = -a;
   }
   PRINT(b);
}
