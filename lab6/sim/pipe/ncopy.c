#include <stdio.h>

typedef int word_t;

#define SIZE 13
word_t src[SIZE], dst[SIZE];

/* $begin ncopy */
/*
 * ncopy - copy src to dst, returning number of positive ints
 * contained in src array.
 */
word_t ncopy(word_t *src, word_t *dst, word_t len)
{
    word_t count1 = 0;
    word_t count2 = 0;
    word_t count3 = 0;
    word_t val;

    while (len > 2) {
        len -= 2;
        *(dst+1) = *(src+1);
        if (val > 0)
            count1++;
        *(dst+2) = *(src+2);
        if (val > 0)
            count2++;
        *(dst+3) = *(src+3);
        if (val > 0)
            count3++;
        dst += 3;
        src += 3;
    }
    while (len > 0) {
        *dst++ = *src++;
        if (val > 0)
            count1++;
        len--;
    }
    return count1 + count2 + count3;
}
/* $end ncopy */

int main()
{
    word_t i, count;
    for (i=0; i<SIZE; i++)
	src[i]= i+1;
    count = ncopy(src, dst, SIZE);
    printf ("count=%d\n", count);
    //exit(0);
}



