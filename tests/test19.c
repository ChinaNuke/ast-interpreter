extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
   int* a;
   int* b;
   int* c[2];
   a = (int*)MALLOC(sizeof(int)*2);

   *a = 10;
   *(a+1) = 20;
   c[0] = a;
   c[1] = a+1;

   PRINT(*c[0]);
   PRINT(*c[1]);
}
