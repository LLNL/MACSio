#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <macsio_data.h>

int main()
{
    struct timeval tv;
    int i;
    int id1 = MACSIO_DATA_CreatePRNG(0xDeadBeef);
    int id2;
    int id3 = MACSIO_DATA_CreatePRNG(0xBabeFace);
    int id5 = MACSIO_DATA_CreatePRNG(0xDeadBeef);
    long tseed;
    long series1[100], series2[100], series3[100], series4[100][5], series5[100][5];

    gettimeofday(&tv, 0);
    tseed = (long) (((unsigned int) (tv.tv_sec + tv.tv_usec)) & 0xFFFFFFFF);
    id2 = MACSIO_DATA_CreatePRNG(tseed); /* time varying output */

    for (i = 0; i < 100; i++)
    {
        int j, id4 = MACSIO_DATA_CreatePRNG(0x0BedFace);
        series1[i] = MACSIO_DATA_GetValPRNG(id1);
        series2[i] = i%3?0:MACSIO_DATA_GetValPRNG(id2);
        series3[i] = i%5?0:MACSIO_DATA_GetValPRNG(id3);
        for (j = 0; j < 5; j++)
            series4[i][j] = MACSIO_DATA_GetValPRNG(id4);
        MACSIO_DATA_DestroyPRNG(id4);
        for (j = 0; j < 5; j++)
            series5[i][j] = MACSIO_DATA_GetValPRNG(id5);
        MACSIO_DATA_ResetPRNG(id5);
        if (i && !(i%50))
            MACSIO_DATA_ResetPRNG(id2);
    }
    if (memcmp(&series2[0], &series2[51], 40*sizeof(long)))
        return 1;
    if (memcmp(&series4[0], &series4[1], 5*sizeof(long)))
        return 1;
    if (memcmp(&series4[1], &series4[23], 5*sizeof(long)))
        return 1;
    if (memcmp(&series5[0], &series5[1], 5*sizeof(long)))
        return 1;
    if (memcmp(&series5[1], &series5[23], 5*sizeof(long)))
        return 1;

    MACSIO_DATA_DestroyPRNG(id1);
    MACSIO_DATA_DestroyPRNG(id2);
    MACSIO_DATA_DestroyPRNG(id3);
    MACSIO_DATA_DestroyPRNG(id5);

    return 0;
}
