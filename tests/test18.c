extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
   int* a;
   int **b;
   int *c;
   a = (int*)MALLOC(sizeof(int)*2); // int[8]
   b = (int **)MALLOC(sizeof(int *)); // int[8][]

   *b = a; // b[0] = a
   *a = 10; // a[0] = 10
   *(a+1) = 20; // a[1] = 20

   c = *b; // c = b[0] -> c = a
   PRINT(*c); // a[0] -> 10
   PRINT(*(c+1)); // a[1] -> 20
   FREE(a);
   FREE((int *)b);
}
