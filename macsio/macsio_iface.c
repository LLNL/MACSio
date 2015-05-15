#include <string.h>

#include <macsio_iface.h>
#include <macsio_utils.h>

/* We assume the mem for this static object is zerod */
static MACSIO_IFACE_Handle_t iface_table[MACSIO_IFACE_MAX_COUNT];

int MACSIO_IFACE_Register(MACSIO_IFACE_Handle_t const *iface)
{
    int i;
    for (i = 0; i < MACSIO_IFACE_MAX_COUNT; i++)
    {
        if (iface_table[i].slotUsed) continue;
        iface_table[i] = *iface;
        iface_table[i].slotUsed = 1;
        return 1;
    }
    return 0;
}

int MACSIO_IFACE_GetId(char const *name)
{
    int i;
    for (i = 0; i < MACSIO_IFACE_MAX_COUNT; i++)
    {
        if (!iface_table[i].slotUsed) continue;
        if (!strcmp(iface_table[i].name, name))
            return i;
    }
    return -1;
}

char const *MACSIO_IFACE_GetName(int i)
{
    if (i < 0 || i >= MACSIO_IFACE_MAX_COUNT || !iface_table[i].slotUsed)
        return "unknown";
    return iface_table[i].name;
}

void MACSIO_IFACE_GetIdsMatchingFileExtension(int *cnt, int **ids, char const *ext)
{
    int i, n, pass;
    for (pass = 0; pass < (ids?2:1); pass++)
    {
        n = 0;
        for (i = 0; i < MACSIO_IFACE_MAX_COUNT; i++)
        {
            if (iface_table[i].slotUsed && (!ext || !strcmp(iface_table[i].ext, ext)))
            {
                if (pass == 1)
                    (*ids)[n] = i;
                n++; 
            }
        }
        if (ids && *ids == 0)
            *ids = (int *) malloc(n * sizeof(int));
    }
    *cnt = n;
}

void MACSIO_IFACE_GetIds(int *cnt, int **ids)
{
    return MACSIO_IFACE_GetIdsMatchingFileExtension(cnt, ids, 0);
}

MACSIO_IFACE_Handle_t const *MACSIO_IFACE_GetById(int i)
{
    if (i < 0 || i >= MACSIO_IFACE_MAX_COUNT || !iface_table[i].slotUsed)
        return 0;
    return &iface_table[i];
}

MACSIO_IFACE_Handle_t const *MACSIO_IFACE_GetByName(char const *name)
{
    int id = MACSIO_IFACE_GetId(name);
    if (id < 0) return 0;
    return MACSIO_IFACE_GetById(id);
}
