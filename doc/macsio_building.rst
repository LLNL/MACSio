Building MACSio_
----------------

MACSio_'s design is described in detail in
:download:`an accompanying design document <macsio_design.pdf>`.

A key aspect about MACSio_'s design relevant to the installation process is that
MACSio_ is composed of a growing list of I/O *plugins*. Each plugin implements one
or more *strategies* for marshalling MACSio_'s data object(s) between primary (volitile)
and secondary (non-volitile) storage (e.g. memory and disk). An I/O strategy may involve
the use of one or more strategy-specific I/O libraries. Therefore, each plugin often
introduces additional, but optional, build dependencies. A given installation of MACSio_
can include all or just a subset of the available plugins. This is all determined at
MACSio_ build time.

Installing via Spack
^^^^^^^^^^^^^^^^^^^^

The easiest way to build MACSio_ may be to use `spack <https://spack.io>`_. In theory, the
steps involved are as simple as

.. code-block:: shell

   % git clone https://github.com/spack/spack.git
   % . spack/share/spack/setup-env.sh
   % spack install macsio

Note, however, that these few steps may install *a lot* of software dependencies, many which
you may not ordinarily expect. The operation may take more than an hour to complete and the
resulting package installations will be *hashed* according to Spack's `hashing
<https://spack.readthedocs.io/en/latest/tutorial_basics.html?highlight=hash#installing-packages>`_
specifications which may impact any subsequent software development activity you intend to do with
the resulting MACSio_ installation.

Nonetheless, Spack is often the simplest way to install MACSio_.

The Spack installation of MACSio_ attempts to build *all* of MACSio_'s plugins.

Installing Manually
^^^^^^^^^^^^^^^^^^^

MACSio_'s only **required** dependency is `json_cwx <https://github.com/LLNL/json-cwx>`_.
MPI is not even a **required** dependency. However, building without MPI will in all likelihood
result in a MACSio_ installation that isn't very useful in assessing I/O performance at scale.
Building without MPI, however, is useful in debugging basic functionality of an installation.

MACSio has several **optional** dependencies, each one associated with a given *plugin*. For
example, the Exodus plugin requires the `Exodus <https://github.com/gsjaardema/seacas#exodus>`_
and `netCDF <https://www.unidata.ucar.edu/software/netcdf/docs/index.html>`_ libraries which may,
in turn, require the `HDF5 <https://www.hdfgroup.org/downloads/hdf5/>`_ library. The Silo
plugin requires the `Silo <https://wci.llnl.gov/simulation/computer-codes/silo>`_ library and,
optionally, the HDF5 library.

For dependencies associated with each plugin, see the `CMakeLists.txt
<https://github.com/LLNL/MACSio/blob/master/plugins/CMakeLists.txt>`_ file in the plugin
directory for telltail notes about plugin dependencies.

Once json-cwx and any optional dependencies have been successfully installed, CMake is used
to build MACSio_ and any of the desired plugins (builds with silo by default)

.. code-block:: shell

   % mkdir build
   % cd build
   % cmake -DCMAKE_INSTALL_PREFIX=[desired-install-location] \
         -DWITH_JSON-CWX_PREFIX=[path to json-cwx] \
         -DWITH_SILO_PREFIX=[path to silo] ..
   % make
   % make install

NOTE: Some options for the cmake line:
  - Build docs:             ``-DBUILD_DOCS=ON``
  - Build HDF5 Plugin:      ``-DENABLE_HDF5_PLUGIN=ON -DWITH_HDF5_PREFIX=[path to hdf5]``
  - Build TyphonIO Plugin:  ``-DENABLE_TYPHONIO_PLUGIN=ON -DWITH_TYPHONIO_PREFIX=[path to typhonio]``
  - Build PDB Plugin:       ``-DENABLE_PBD_PLUGIN=ON``
  - Build Exodus Plugin:    ``-DENABLE_EXODUS_PLUGIN=ON -DWITH_EXODUS_PREFIX=[path to exodus]``

Although MACSio_ is C Language, at a minimum it must be linked using a C++ linker due to
its use of non-constant expressions in static initializers to affect the static plugin
behavior. However, its conceivable that some C++'isms have crept into the code causing
warnings or outright errors with some C compilers. If you encounter such a situation,
please file an issue for it so we are aware of the issue and can fix it.

Note that the list of available plugins is determined by which plugins are compiled and
linked with MACSio_'s main. So, plugins are determined at link-time, not run-time.
