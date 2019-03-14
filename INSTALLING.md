1. Download and install json-cwx [json-c with extensions](https://github.com/LLNL/json-cwx)
2. Create a build directory
```shell
      % mkdir build
      % cd build
```
3. Use CMake to build MACSio and any of the desired plugins (builds with silo by default)
```shell
      % cmake -DCMAKE_INSTALL_PREFIX=[desired-install-location] -DWITH_JSON-CWX_PREFIX=[path to json-cwx] -DWITH_SILO_PREFIX=[path to silo] ..
      % make
      % make install
```
  - NOTE: Some options for the cmake line:
    - Build docs:             -DBUILD_DOCS=ON   
      Build HDF5 Plugin:      -DENABLE_HDF5_PLUGIN=ON -DWITH_HDF5_PREFIX=[path to hdf5]
    - Build TyphonIO Plugin:  -DENABLE_TYPHONIO_PLUGIN=ON -DWITH_TYPHONIO_PREFIX=[path to typhonio]
    - Build PDB Plugin:       -DENABLE_PBD_PLUGIN=ON
    - Build Exodus Plugin:    -DENABLE_EXODUS_PLUGIN=ON -DWITH_EXODUS_PREFIX=[path to exodus]
    - Caliper support:        -DENABLE_CALIPER=ON -Dcaliper_DIR=[caliper-install-dir]/share/cmake/caliper
4. MACSio has default values for all command-line arguments. So if you
just run the command './macsio', it will do something but probably
not what you want. Here are some example command-lines. . .

  - To run with Multiple Independent File (MIF) mode to 8 HDF5 files. . .
    ```shell
    mpirun -np 93 macsio --interface hdf5 --parallel_file_mode MIF 8
    ```
  - Same thing to but a Single Shared File (SIF) mode to 1 HDF5 file. . .
    ```shell
    mpirun -np 93 macsio --interface hdf5 --parallel_file_mode SIF 1
    ```
  - Default per-proc request size is 80,0000 bytes (10K doubles). To use
    a different request size, use --part_size . . .
    ```shell
    mpirun -np 128 macsio --interface hdf5 --parallel_file_mode MIF 8 --part_size 10M
    ```
    'M' means either decimal Megabytes (Mb) or binary Mibibytes (Mi)
    depending on setting for --units_prefix_system. Default is binary.
  - Default #parts/proc is 2, which is common for applications that support
    'domain overload' workflows. Change with --avg_num_parts <float> arg
    ```shell
    mpirun -np 128 macsio --interface hdf5 --parallel_file_mode MIF 8 --avg_num_parts 2.5
    ```
    means that 50% of procs have 2 parts and 50 % of procs have 3 parts.
  - Default number of dumps is 10, change with --num_dumps argument. . .
    ```shell
    mpirun -np 128 macsio --interface hdf5 --parallel_file_mode MIF 8 --num_dumps 2
    ```
  - Set various debugging levels (1,2 or 3, more info in logs with higher numbers)
    ```shell
    mpirun -np 128 macsio --interface hdf5 --parallel_file_mode MIF 8 --debug_level 2
    ```
    Note: debugging can degrade performance and skew timing results
  - Getting more help. . .
    ```shell
    ./macsio --help
    ```
    Will produce a lot of help output including MACSio's main arguments as well as arguments
    specific to any enabled plugins.
  - To use H5Z-ZFP compression plugin, be sure to have the plugin compiled and available
    with the same compiler and version of HDF5 you are using with MACSIO. Then to run it
    ```shell
    env HDF5_PLUGIN_PATH=<path-to-plugin-dir> mpirun -np 4 ./macsio/macsio --interface hdf5 --parallel_file_mode MIF 2 --avg_num_parts 2.5 --part_size 40000 --part_dim 2 --part_type rectilinear --num_dumps 2 --filebase macsio_mif --fileext h5 --debug_level 1 --plugin_args --compression zfp rate=4
    ```
    where `path-to-plugin-dir` is the path to the directory containing `libh5zzfp.{a,so,dylib}`
  - When enabled, Caliper can be used to collect performance profiles or traces. Configure
    Caliper through its environment variables or config file. For example, to print a basic
    runtime profile (serial or MPI):
    ```shell
    CALI_CONFIG_PROFILE=runtime-report macsio
    CALI_CONFIG_PROFILE=mpi-runtime-report mpirun -np 128 macsio
    ```
    See https://github.com/LLNL/Caliper for more information.
