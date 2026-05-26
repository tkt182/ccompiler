#include "test.h"

int g1, g2[4];
int g3 = 3;
int g4[4] = {1, 2, 3, 4};
int g5[4] = {1, 2};
int *g6 = &g3;
char msg1[4] = "foo";
char msg2[] = "bar";
char *msg3 = "foo";
char *c = msg1 + 1;

int main() {
  ASSERT(3, ({ int a; a=3; a; }));
  ASSERT(3, ({ int a=3; a; }));
  ASSERT(8, ({ int a=3; int z=5; a+z; }));

  ASSERT(3, ({ int a=3; a; }));
  ASSERT(8, ({ int a=3; int z=5; a+z; }));
  ASSERT(6, ({ int a; int b; a=b=3; a+b; }));
  ASSERT(3, ({ int foo=3; foo; }));
  ASSERT(8, ({ int foo123=3; int bar=5; foo123+bar; }));

  ASSERT(8, ({ int x; sizeof(x); }));
  ASSERT(8, ({ int x; sizeof x; }));
  ASSERT(8, ({ int *x; sizeof(x); }));
  ASSERT(32, ({ int x[4]; sizeof(x); }));
  ASSERT(96, ({ int x[3][4]; sizeof(x); }));
  ASSERT(32, ({ int x[3][4]; sizeof(*x); }));
  ASSERT(8, ({ int x[3][4]; sizeof(**x); }));
  ASSERT(9, ({ int x[3][4]; sizeof(**x) + 1; }));
  ASSERT(9, ({ int x[3][4]; sizeof **x + 1; }));
  ASSERT(8, ({ int x[3][4]; sizeof(**x + 1); }));
  ASSERT(8, ({ int x=1; sizeof(x=2); }));
  ASSERT(1, ({ int x=1; sizeof(x=2); x; }));

  ASSERT(0, g1);
  ASSERT(3, ({ g1=3; g1; }));
  ASSERT(0, ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[0]; }));
  ASSERT(1, ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[1]; }));
  ASSERT(2, ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[2]; }));
  ASSERT(3, ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[3]; }));

  ASSERT(8, sizeof(g1));
  ASSERT(32, sizeof(g2));

  ASSERT(1, ({ char x=1; x; }));
  ASSERT(1, ({ char x=1; char y=2; x; }));
  ASSERT(2, ({ char x=1; char y=2; y; }));

  ASSERT(1, ({ char x; sizeof(x); }));
  ASSERT(10, ({ char x[10]; sizeof(x); }));

  ASSERT(2, ({ int x=2; { int x=3; } x; }));
  ASSERT(2, ({ int x=2; { int x=3; } int y=4; x; }));
  ASSERT(3, ({ int x=2; { x=3; } x; }));

  ASSERT(3, g3);
  ASSERT(3, *g6);
  ASSERT(5, ({ g3=5; *g6; }));

  ASSERT(1, g4[0]);
  ASSERT(4, g4[3]);
  ASSERT(2, g5[1]);
  ASSERT(0, g5[2]);
  ASSERT(0, g5[3]);

  ASSERT(102, msg1[0]);
  ASSERT(111, msg1[1]);
  ASSERT(111, msg1[2]);
  ASSERT(0, msg1[3]);

  ASSERT(98, msg2[0]);
  ASSERT(97, msg2[1]);
  ASSERT(114, msg2[2]);
  ASSERT(0, msg2[3]);

  ASSERT(102, msg3[0]);
  ASSERT(111, msg3[1]);
  ASSERT(111, msg3[2]);
  ASSERT(0, msg3[3]);

  ASSERT(111, c[0]);
  ASSERT(111, c[1]);
  ASSERT(0, c[2]);

  printf("OK\n");
  return 0;
}
