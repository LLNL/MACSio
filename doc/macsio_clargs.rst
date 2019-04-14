Command Line Argument Parsing
-----------------------------

The command line argument package simplifies a number of aspects of defining and also
parsing command line arguments for MACSio_'s main code or any of its plugins. The
package supports command line arguments and their options with the followin features...

* boolean arguments (the argument's presence or absence maps to ``0`` or ``1``)
* integer, double and string valued arguments
* multiple arguments for a given command-line option
* default values for arguments
* option *groupings*
* help strings and printing of *usage* with ``--help`` option
* pretty (somewhat) formatting of long help strings to terminal width
* separating arguments into sub-command groups.
* routing the parsed results to caller supplied local variables or constructing
  a returned JSON object.
* error detection and logging
* collective execution in parallel
  * ``argc``, ``argv`` inputs ensured consistent across all tasks
  * results collectively unified across all tasks in parallel

The package is designed to do parsing of simple command-line arguments
and assign values associated with them either to caller-supplied scalar variables or to populate a
json_object. The work-horse function, :any:`MACSIO_CLARGS_ProcessCmdline`, is used in the
following manner...

After the ``argi``, ``argc``, ``argv`` trio of arguments, the remaining arguments
come in groups of either 3 (when mapping to a json_object) or 4 (when
mapping to scalar program variables).

The first of these is an argument format specifier much like a printf
statement. It indicates the type of each parameter to the argument as well as
the number of parameters. Presently, it understands only %d, %f and %s types.

The second is a string argument indicating the default value, as a string.

The third is a help string for the argument. Note, you can make this string as
long as the C-compiler will permit. You *should not* embed any newline
characters as the routine to print the help string will do that for you.
However, you may embed newline characters if you wish to control specific line
breaking when output.

The fourth argument is present only when mapping to scalar program variables
and is a pointer to the variable location(s) where values will be stored.

Command line arguments for which only existence on the command-line is tested
assume a caller-supplied return value of int and will be assigned ``1`` if the
argument exists on the command line and ``0`` otherwise.

Do not name any argument with a substring ``help`` as that is reserved for
obtaining help. Also, do not name any argument with the string ``end_of_args``
as that is used to indicate the end of the list of arguments passed to the function.

If any argument on the command line has the substring ``help``, help will be
printed by task 0 and then this function calls ``MPI_Finalize()``
(in parallel) and ``exit(1)``.

This function must be called collectively in ``MPI_COMM_WORLD``. All tasks are
guaranteed to complete with identical results.

CLARGS API
^^^^^^^^^^

.. doxygengroup:: MACSIO_CLARGS

Example
^^^^^^^

.. code-block:: c

    int doMultifile, numCycles, Ni, Nj;
    double Xi, Xj;
    MACSIO_CLARGS_ArgvFlags_t flags;
    json_object *obj = 0;
    int argi = 1; /* start at argv[1] not argv[0] */

    flags.error_mode = MACSIO_CLARGS_ERROR;
    flags.route_mode = MACSIO_CLARGS_TOMEM;

    MACSIO_CLARGS_ProccessCmdline(obj, flags, argi, argc, argv,
        "--multifile", "", /* example boolean with no default */
            "if specified, use a file-per-timestep",
            &doMultifile,
        "--numCycles %d", "10", /* example integer with default of 10 */
            "specify the number of cycles to run",
            &numCycles,
        "--dims %d %f %d %f", "25 5 35 7", /* example multiple values */
            "specify size (logical and geometric) of mesh",
            &Ni, &Xi, &Nj, &Xj,
    MACSIO_CLARGS_END_OF_ARGS);

.. code-block:: shell

    ./macsio --help
    Usage and Help for "./macsio" Defaults, if any, in square brackets after argument definition
      --multifile [] if specified, use a file-per-timestep
      --numCycles %d [10] specify the number of cycles to run
      --dims %d %f %d %f [25 5 35 7] specify size (logical and geometric) of mesh

.. only:: internals

    Implementation Issues
    ^^^^^^^^^^^^^^^^^^^^^

    The implementation has a few weaknesses which should be fixed

    * Overly complex implementation
    * Inconsistencies in JSON and MEM routing usage
        * JSON handles *boolean* as *boolean*. MEM handles *boolean* as *int*.
        * When no-assign defaults is used, TOJSON routing creates JSON object which is
          missing the default entries entirely whereas TOMEM routing leaves specified
          memory locations untouched.
    * Do we really need *all* the english language help for an option in the executable itself?
        * Could put one-liners in the code and a URL to the RTD help page with full text there
        * Does create a bit more of a maintenance burden in that most changes then require
          editing two places.
    * Help output is raggedy and unprofessional looking
        * Adding piping through ``fmt`` seems to have helped a bit
        * Identation of argument groups is funky
    * Uses ``%f`` for *double* arguments. Should be ``%g``.
    * error logging dependency
    * in parallel MPI_COMM_WORLD is assumed
    * should probably return ``warn`` code if warnings were issued
