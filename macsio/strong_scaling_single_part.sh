#!/bin/sh

# target total byte count
ttbc=6710886400

part_size()
{
    psize=$(expr $ttbc / $1)
    echo $psize
}

n=32
while [[ $n -le 262144 ]] ; do
    psize=$(part_size $n)
    nf=$n
    # allow file count to trak task count to 1024 tasks
    # then keep it constant after that
    if [[ $nf -ge 1024 ]]; then
        nf=1024
    fi
    cmd="mpirun -np $n ./macsio --interface hdf5 --avg_num_parts 1 --part_size $psize --parallel_file_mode MIF $nf"
    echo $cmd
    n=$(expr $n \* 2)
done
