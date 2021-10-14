extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
   int* a;
   int **b;
   int *c;
   a = (int*)MALLOC(sizeof(int)*2);
   b = (int **)MALLOC(sizeof(int *));

   *b = a;
   *a = 10;
   *(a+1) = 20;

   c = *b;
   PRINT(*c);
   PRINT(*(c+1));
   FREE(a);
   FREE((int *)b);
}
