Contributing to MACSio Documentation
------------------------------------

  * Use ``:any:`` for links into Doxygon content instead of ``:c:func:`` as described in
    breathe documentation. The reason is that in order for breathe's ``:c:func:`` to work,
    one has to create breathe-specific sphinx directives for each and every Doxygen target
    (which IMHO kinda defeats the purpose of breathe). Also, it apparently only works for
    *functions* anyways, not *types* or *macros* or *structs*
  * Put Doxygen markup only in header files. Avoid using it in implementation (``c``) files.
  * Keep Doxygen markup fairly terse and to the point and put more of the *prose* and lengthy
    descriptions in ``.rst`` files.
  * Use ``.. only:: internals`` to conditionally include blocks that are used solely to document
    internal functionality of a given package. See example in ``macsio_mif.rst``.
  * To build locally
    ``env DONT_INSTALL_BREATHE=1 READTHEDOCS=1 sphinx-build -b html . _build -a [-t internals]``
