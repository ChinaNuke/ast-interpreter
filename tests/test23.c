extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);


void swap(int *a, int *b) {
   int temp;
   temp = *a;
   *a = *b;
   *b = temp;
}

int main() {
   int* a; 
   int* b;
   a = (int *)MALLOC(sizeof(int)); // int[4]
   b = (int *)MALLOC(sizeof(int *)); // int[8][]
   
   *b = 24; // b[0] = 24
   *a = 42; // a[0] = 42

   swap(a, b);

   PRINT(*a); // 24
   PRINT(*b); // 42
   FREE(a);
   FREE(b);
   return 0;
}


