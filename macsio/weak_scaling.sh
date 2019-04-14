#!/bin/sh

n=32

nfiles()
{
    if [[ $1 -le 32 ]]; then
        echo 32
    elif [[ $1 -le 8192 ]]; then
        echo 64
    elif [[ $1 -le 65536 ]]; then
        echo 128
    else
        echo 256
    fi
}

while [[ $n -le 262144 ]] ; do
    nf=$(nfiles $n)
    cmd="mpirun -np $n macsio --interface hdf5 --avg_num_parts 8 --part_size 100K --parallel_file_mode MIF $nf"
    echo $cmd
    n=$(expr $n \* 2)
done
