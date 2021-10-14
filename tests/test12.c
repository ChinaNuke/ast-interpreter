extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
	int a[3];
        int i;
	int b = 0;
        for (i=0; i<3; i = i + 1) {
           a[i] = i * i;
        }
        b = a[2];
	PRINT(b);
}
