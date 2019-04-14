#!/bin/sh

# target total byte count
ttbc=6710886400

nparts_and_part_size()
{
    # start by assming just one part per task
    nparts=1

    # nominal part size is total target size divided by
    # number of tasks (arg $1 to function). Note that
    # integer arithmetic here will cause some variation from
    # target ttbc
    psize=$(expr $ttbc / $1)

    # if the part size is bigger than the 100K we used in the weak study,
    # lets reduce it and then increase the number of parts
    if [[ $psize -ge 102400 ]]; then
        nparts=$(echo "$psize/102400" | bc -l)
        nparts=$(echo $nparts | cut -d'.' -f1)
        psize=102400
    fi

    echo $nparts $psize
}

n=32
while [[ $n -le 262144 ]] ; do
    nparts=$(nparts_and_part_size $n | cut -d' ' -f1)
    psize=$(nparts_and_part_size $n | cut -d' ' -f2)
    nf=$n
    # allow file count to trak task count to 1024 tasks
    # then keep it constant after that
    if [[ $nf -ge 1024 ]]; then
        nf=1024
    fi
    cmd="mpirun -np $n ./macsio --interface hdf5 --avg_num_parts $nparts --part_size $psize --parallel_file_mode MIF $nf"
    echo $cmd
    n=$(expr $n \* 2)
done
