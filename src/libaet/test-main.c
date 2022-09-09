#include <locale.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    char *re=setlocale (LC_ALL, "");
   printf("hello aet!\n");
   return 0;

}
