#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define NIX -1

static char const *paste_my_path(char const *first, ...)
{
    static char retbuf[4096];
    int n = sizeof(retbuf);
    static char tmpstr[32];
    int val;
    char *str;

    va_list ap;
    va_start(ap, first);

    retbuf[0] = '\0';
    strncat(retbuf, first, n);
    n -= strlen(first);
    val = va_arg(ap, int);

    while (val != NIX && n)
    {
        snprintf(tmpstr, sizeof(tmpstr), "%d", val);
        tmpstr[sizeof(tmpstr)-1] = '\0';
        if (retbuf[sizeof(retbuf)-n-1] != '/')
        {
            strncat(retbuf, "/", n);
            n -= 1;
        }
        strncat(retbuf, tmpstr, n);
        n -= strlen(tmpstr);
        char *str = va_arg(ap, char*);
        strncat(retbuf, str, n);
        n -= strlen(str);
        val = va_arg(ap, int);
    }
    va_end(ap);

    return retbuf;
}


/* Using Variadic macros */
#define FOO_MY_PATH(...) #__VA_ARGS__
#define PASTE_MY_PATH(...) paste_my_path(__VA_ARGS__,NIX)

int main()
{
    int i=13, j=1234;
    
    printf("\"%s\"\n", PASTE_MY_PATH("/foo/bar/gorfo",i,"/alice/mark",j,"/bob"));
    printf("\"%s\"\n", PASTE_MY_PATH("/foo/bar/gorfo/",i,"/alice/mark/",j,"/bob"));
    printf("\"%s\"\n", FOO_MY_PATH(/foo/bar/gorfo[,i,]/alice/mark[,j,]/bob));
    printf("\"%s\"\n", paste_my_path("/mark/steve",NIX));
    return 0;
}
