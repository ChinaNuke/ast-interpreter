extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

void set(int *a, int b) {
   int c;
   c = b + 1;
   *a = c;
}

int main() {
   int* a; 
   int b;
   a = (int *) MALLOC(sizeof(int));
   
   b = 10;
   set(a, b);
   PRINT(*a);
   FREE(a);
   return 0;
}


