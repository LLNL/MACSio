.. _macsio_timing:

Timing
------

MACSio_'s timing package includes utilities to support the creation of a number of timers to time
sections of code.

Timers are initialized/started with a user-defined label, and an optional group mask and iteration number. 
A hash of the timer is computed from its label and group mask combined with the source file name and line number.
The resulting hash value is used to identify the timer and returned as the timer's ``id`` when the timer
is created/started.

Timers can be iterated and re-started. These are two different concepts. During iteration, the same timer can be
used to time the same section of code as it gets executed multiple times. Each time represents a different
iteration of the timer. The timer will keep track of the minimum time, maximum time, average time and variance
of times over all iterations it is invoked. However, apart from these *running* statistics, a timer maintains no
memory of past values. If a timer is iterated 10 times, it does not maintain knowledge of all 10 individual times.
It maintains only knowledge of the *running* statistics; min, max, avg, var. The algorithm it uses to maintain these
running statistics is the `Welford Online algorithm
<https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_Online_algorithm>`_.

Within any timer iteration, a timer can be stopped and restarted. If a timer is being used to time 
a block of code that involves some occasional and perhaps irrlevant alternative logic path,
the timer can be stopped until execution returns from the alternative path. Then the
timer can be restarted. This can only be done *within* a given iteration.

Timers can also be grouped into a small number (<64) of different groups. A timer *group* is created with
a call to :any:`MACSIO_TIMING_GroupMask`. Timers are included in one or more groups by or'ing the associated
group masks into the ``gmask`` argument to :any:`MACSIO_TIMING_StartTimer`. A timer can be a member of multiple
groups For example, it is possible to maintain a set of timers for MIF file I/O apart from a set of timers being
used to time a particular plugin's operations to marshal a mesh or variable to/from persistent storage. Or,
operations use to interact with filesystem metadata directly (e.g. ``mkdir``, ``chdir``, ``readir``, etc.) can
be maintained separately from timers used for entirely other purposes.

Finally, timers can be *reduced* parallel tasks thereby creating a statistical summary of timer information
across tasks.

A timer is initialized/started by a call to :any:`MACSIO_TIMING_StartTimer` or the shorthand macro :any:`MT_StartTimer`.
This call returns the timer's ``ID`` which is used in a subsequent call to :any:`MACSIO_TIMING_StopTimer` to stop
the timer.

.. code-block:: c

    MACSIO_TIMING_TimerId_t tid = MT_StartTimer("my timer", MACSIO_TIMING_GROUP_NONE, MACSIO_TIMING_ITER_AUTO);

    ...do some work here...

    MT_StopTimer(tid);

In the above code, the call to :any:`MT_StartTimer` starts a timer for a new (automatic) iteration. In this simple
examle, we do not worry about timer group masks.

By default, the timing package uses `MPI_Wtime()
<https://www.mpich.org/static/docs/v3.2/www3/MPI_Wtime.html>`_ but a caller can set
:any:`MACSIO_TIMING_UseMPI_Wtime` to zero to instead use `gettimeofday()
<http://man7.org/linux/man-pages/man2/gettimeofday.2.html>`_.

Examples of the use of the logging package can be found in :ref:`tsttiming_c`.

Timing API
^^^^^^^^^^

.. doxygengroup:: MACSIO_TIMING

.. _tsttiming_c:

Example (tsttiming.c)
^^^^^^^^^^^^^^^^^^^^^

.. include:: ../macsio/tsttiming.c
   :code: c
   :start-line: 27
   :number-lines:
