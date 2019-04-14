Mesh and Field Data Generation
------------------------------
MACSio_ is capable of generating a wide variety of mesh and variable data and amorphous metadata
typical of HPC multi-physics applications. Currently, MACSio_ can generate structured, rectilinear,
unstructured and arbitrary polyhedral, multi-block meshes in 2 or 3 dimensions. In addition,
regardless of the particular type of mesh MACSio_ generates for purposes of I/O performance testing,
it stores and marshalls all of the resultant data in an uber JSON-C object that is passed around
witin MACSio_ and between MACSIO and its I/O plugins.

MACSio_ employs a very simple algorithm to generate and then decompose a mesh in parallel. However, the
decomposition is also general enough to create multiple mesh pieces on individual MPI ranks and for
the number of mesh pieces to vary, somewhat, between MPI ranks. At present, there is no support to
explicitly specify a particular arrangement of mesh pieces and MPI ranks. However, such enhancement
is planned for the near future.

Once the global whole mesh shape is determined as a count of total pieces and as counts of pieces in each
of the logical dimensions, MACSio_ uses a very simple algorithm to assign mesh pieces to MPI ranks.
The global list of mesh pieces is numbered starting from 0. First, the number
of pieces to assign to rank 0 is chosen. When the average piece count is non-integral, it is a value
between K and K+1. So, MACSio_ randomly chooses either K or K+1 pieces but being carful to weight the
randomness so that once all pieces are assigned to all ranks, the average piece count per rank target
is achieved. MACSio_ then assigns the next K or K+1 numbered pieces to the next MPI rank. It continues
assigning pieces to MPI ranks, in piece number order, until all MPI ranks have been assigned pieces.
The algorithm runs indentically on all ranks. When the algorithm reaches the part assignment for the
rank on which its executing, it then generates the K or K+1 mesh pieces for that rank. Although the
algorithm is essentially a sequential algorithm with asymptotic behavior O(#total pieces), it is primarily
a simple book-keeping loop which completes in a fraction of a second even for more than one million
pieces.

Each piece of the mesh is a simple rectangular region of space. The spatial bounds of that region are
easily determined. Any variables to be placed on the mesh can be easily handled as long as the variable's
spatial variation can be described in the global goemetric space.

Data API
^^^^^^^^

.. doxygengroup:: MACSIO_DATA

Random (Amourphous) Object Generation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
In addition to the data necessary to represent the main mesh and field MACSio_ produces, it is also
common for HPC multi-physics code to generate modest amount of amourphous metadata dealing with such
things as materials, material models and material properties tables, user input controls, performance
tracking information, library versions and data provenenance, debugging information, etc. This kind of
information often takes the form of a large hierarchy of key-value pairs where the *values* can sometimes
be arrays on the order of the size of the number of tasks, etc.

Two methods in MACSio_'s data generation package help to create some random but nonetheless statistically
similar kinds of data.

.. doxygengroup:: MACSIO_RANDOBJ

.. _macsio_data_randomization:

Randomization
^^^^^^^^^^^^^

Randomization of various of MACSio_'s behaviors may be explicitly enforced or
prevented. However, to ensure such behaviors, random number generation needs
to be handled carefully especially when dealing with reproducibility from run
to run and/or agreement across parallel tasks or both.

For this reason, MACSio_ creates and initializes several *default* pseudo random
number generators (PRNGs), all of which utilize the C standard library
`random() <http://man7.org/linux/man-pages/man3/random.3.html">`_ method.

Pseudo Random Number Generator (PRNG) support is handled by maintaining
a series of state vectors for calls to initstate/setstate of C library
`random() <http://man7.org/linux/man-pages/man3/random.3.html">`_
and friends. Multiple different PRNGs are supported each having
zero or more special properties depending on how they are initialized
(e.g. the *seed* used from run to run and/or from task to task) as well
as how they are expected to be used/called. For example, if a
PRNG is intended to produce identical values on each task, then it is up
to the caller to ensure the respective PRNG is called consistently (e.g. collectively)
across tasks. Otherwise, unintended or indeterminent behavior may result.

The *default* PRNGs MACSio_ creates are...

Naive : :any:`MD_random_naive`
    The naive prng is fine for any randomization that does not need special behaviors.
    This is the one that is expected to be used most of the time. Think of it as a
    wholesale replacement for using C library
    `random() <http://man7.org/linux/man-pages/man3/random.3.html>`_ directly.
    :any:`MD_random` is a shorthand alias for :any:`MD_random_naive`

Naive, Time Varying : :any:`MD_random_naive_tv`
    Like :any:`MD_random_naive` except that it is seeded based on *current time*
    and will therefore ensure variation from run to run (e.g. is time varying). It
    should be used in place of :any:`MD_random_naive` where randomization from
    run to run is needed.

Naive, Rank Varying : :any:`MD_random_naive_rv`
    Like :any:`MD_random_naive` but *ensures* variation across tasks (e.g. is
    rank varying) by seeding based on task rank. It should be used in place of
    :any:`MD_random_naive` where randomization from task to task is needed.

Naive, Rank and Time Varying : :any:`MD_random_naive_rtv`
    Like :any:`MD_random_naive_rv` but also *ensures* results vary from rank to rank
    and run to run.

Rank Invariant : :any:`MD_random_rankinv`
    Is neither seeded based on rank or time nor shall be used by a caller in way that
    would cause inconsistency across ranks. It is up to the caller to ensure it is called
    consistently (e.g. collectively) across ranks. Otherwise indeterminent behavior
    may result.

Rank Invariant, Time Varying : :any:`MD_random_rankinv_tv`
    Like :any:`MD_random_rankinv` except ensures variation in randomization from run
    to run. So, for each given run, all ranks will agree on randomization but from run
    to run, that agreement will vary.

If these PRNGs are not sufficient, developers are free to create other PRNGs for
other specific purposes using :any:`MACSIO_DATA_CreatePRNG`.

PRNG API
^^^^^^^^

.. doxygengroup:: MACSIO_PRNGS

PRNG Example (tstprng.c)
^^^^^^^^^^^^^^^^^^^^^^^^

.. include:: ../macsio/tstprng.c
   :code: c
   :start-line: 27
   :number-lines:
