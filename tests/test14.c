extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int f(int x) {
  int i = 0;
  while (i < x) {
     i = i + 2;
  }
  return i + 10;
}
int main() {
   int a;
   int b;
   a = 1;
   b = f(a);
   PRINT(b);
}
