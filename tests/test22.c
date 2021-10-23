extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int get(int *a) {
   return *a;
}

int main() {
   int* a; 
   int b;
   a = (int *) MALLOC(sizeof(int));
   *a = 42;   

   b = get(a);
   PRINT(b);
   return 0;
}

