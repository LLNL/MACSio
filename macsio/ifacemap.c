#include <ifacemap.h>
#include <util.h>

/* Ensure this object is initialized during static initializations */
MACSIO_IFaceHandle_t iface_map[MAX_IFACES] =
{
    {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0},
    {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0},
    {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0},
    {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0},
    {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0},
    {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0},
    {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0},
    {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}, {"", 0, 0, 0, 0, 0, 0}
};

int MACSIO_GetInterfaceId(char const *name)
{
    int i;
    for (i = 0; i < MAX_IFACES; i++)
    {
        if (!iface_map[i].slotUsed) continue;
        if (!strcmp(iface_map[i].name, name))
            return i;
    }
    return -1;
}

char const *MACSIO_GetInterfaceName(int i)
{
    if (i < 0 || i >= MAX_IFACES || !iface_map[i].slotUsed)
        return "unknown";
    return iface_map[i].name;
}

void MACSIO_GetInterfaceIds(int *cnt, int **ids)
{
    int i, n, pass;
    for (pass = 0; pass < (ids?2:1); pass++)
    {
        n = 0;
        for (i = 0; i < MAX_IFACES; i++)
        {
            if (iface_map[i].slotUsed)
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

MACSIO_IFaceHandle_t const *MACSIO_GetInterfaceById(int i)
{
    if (i < 0 || i >= MAX_IFACES || !iface_map[i].slotUsed)
        return 0;
    return &iface_map[i];
}

MACSIO_IFaceHandle_t const *MACSIO_GetInterfaceByName(char const *name)
{
    int id = MACSIO_GetInterfaceId(name);
    if (id < 0) return 0;
    return MACSIO_GetInterfaceById(id);
}
