/*
Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Mark C. Miller

LLNL-CODE-676051. All rights reserved.

This file is part of MACSio

Please also read the LICENSE file at the top of the source code directory or
folder hierarchy.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2, dated June 1991.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

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

void MACSIO_IFACE_GetIdsMatchingFileExtension(char const *ext, int *cnt, int **ids)
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
    return MACSIO_IFACE_GetIdsMatchingFileExtension(0, cnt, ids);
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
