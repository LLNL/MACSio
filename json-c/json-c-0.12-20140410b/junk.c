#define MEMBER0(TYPE, NAME, DIMS) TYPE NAME ## DIMS;
#define MEMBER1(TYPE, NAME, DIM1) TYPE NAME[DIM1];
#define MEMBER2(TYPE, NAME, DIM1, DIM2) TYPE NAME[DIM1][DIM2];

typedef struct _foo_t {
    MEMBER0(int, a, /*void*/);
    MEMBER1(int, array, 7);
    MEMBER2(double, gorfo, 7, 4);
} foo_t;

int main()
{
    foo_t foo;

    foo.a = 57;
    foo.array[3] = 234;
    foo.gorfo[2][1] = 2.11;

    return 0;
}
