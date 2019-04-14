.. _macsio_logging:

Logging
-------

MACSio_'s logging package is a small utility for logging various kinds of messages from processors.
This includes debugging messages, warnings and errors.

A log is a single text file that is divided into groups of message lines. Each processor
gets its own group of lines within the file it may write to. Rank 0 gets the first group
of lines. Rank 1, the next group of lines and so forth. Each line has a maximum length too.

When a log is created with :any:`MACSIO_LOG_LogInit`, the caller specifies the MPI communicator
the log will be used for, the number of message lines per task to allocate plus a count of 
extra lines for rank 0 and the maximum length of any message. Rank 0 initializes the text file
with all space characters except for header lines to distinguish each processor's group of lines
in the file. Note that for a large run on say 10^5 processors, it may take rank 0 several seconds
to initialize this file.

Messages are restricted to a single line of text. Any embedded new-line characters are removed
from a message and replaced with a '!' character. If a processor's message is longer than the
maximum length of a line for the log, the message is truncated to fit within the line. As
processors issue messages, they are written to the processors group of lines in round-robin 
fashion. As the set of messages issued from a processor reaches the end of its group of lines
within the file, it starts issuing new messages at the beginning of the group. So, older messages
can wind up getting overwritten by newer messages. However, all the most recent messages prior
to any significant error will at least be captured in the log.

Parallelism in writing to a MACSIO_LOG file is achieved by ensuring no two processor attempt
to write data in overlapping regions in the file by using ``pwrite()`` to do the actual
writes.

MACSio_'s main creates a default log, :any:`MACSIO_LOG_MainLog`, on the ``MACSIO_MAIN_Comm``. That
log is probably the only log needed by MACSio_ proper or any of its plugins. The convenience macro,
:any:`MACSIO_LOG_MSG`, is the only method one need to worry about to log messages to the
main log. That macro will also capture more detailed information regarding error states around
the time the message issue issued including the file and line number, the system errno and the
most recent MPI error state.

If you use :any:`MACSIO_LOG_MSG`, messages are allowed one of several severities; ``Dbg1``,
``Dbg2``, ``Dbg3``, ``Warn``, ``Err`` and ``Die``. A severity of ``Die`` causes an abort.
Depending on the current debug level setting, ``Dbg1`` - ``Dbg3`` messages may or may not be
recorded to a log.

MACSio_'s main also creates a log for issuing messages to stderr, :any:`MACSIO_LOG_StdErr`. However,
there is no convenience macro like :any:`MACSIO_LOG_MSG` for logging messages to :any:`MACSIO_LOG_StdErr`.
A caller simply has to use the logging interface methods to issue messages to :any:`MACSIO_LOG_StdErr`.

However, any plugin or other portion of MACSio may, optionally, create its own, private, log using
:any:`MACSIO_LOG_LogInit`. Once a log is created, any processor can independently issue messages
to the log. There is no parallel communication involved in issuing messages to logs.

Examples of the use of the logging package can be found in :ref:`tstlog_c`.

Logging API
^^^^^^^^^^^

.. doxygengroup:: MACSIO_LOG


.. _tstlog_c:

Example (tstlog.c)
^^^^^^^^^^^^^^^^^^

.. include:: ../macsio/tstlog.c
   :code: c
   :start-line: 27
   :number-lines:

