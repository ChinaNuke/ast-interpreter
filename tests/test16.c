extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int  x(int y) {
	return y + 10;
}

int  f(int b) {
   return x(b) + 10;
}

int main() {
   int a;
   a = 10;

   a = f(a);
   PRINT(a);
   return 0;
}

