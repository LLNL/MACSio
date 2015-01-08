#include <ifacemap.h>
#include <util.h>

/* Ensure this object is initialized during static initializations */
IOPIFaceHandle_t iface_map[MAX_IFACES] =
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

int IOPGetInterfaceId(char const *name)
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

char const *IOPGetInterfaceName(int i)
{
    if (i < 0 || i >= MAX_IFACES || !iface_map[i].slotUsed)
        return "unknown";
    return iface_map[i].name;
}

void IOPGetInterfaceIds(int *cnt, int **ids)
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

IOPIFaceHandle_t const *IOPGetInterfaceById(int i)
{
    if (i < 0 || i >= MAX_IFACES || !iface_map[i].slotUsed)
        return 0;
    return &iface_map[i];
}

IOPIFaceHandle_t const *IOPGetInterfaceByName(char const *name)
{
    int id = IOPGetInterfaceId(name);
    if (id < 0) return 0;
    return IOPGetInterfaceById(id);
}
