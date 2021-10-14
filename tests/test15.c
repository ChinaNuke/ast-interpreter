extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int f(int x) {
  int a[3];
  int i=0;
  for (; i<3; i = i+1) {
    a[i] = x + i;
  }
  
  if (x> 0) return a[1];
  return a[2];
}
int main() {
   int a;
   int b;
   a = -10;
   b = f(a);
   PRINT(b);
}
