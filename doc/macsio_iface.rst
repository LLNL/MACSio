Plugins
-------

Ordinarily, when we think of plugins in software, we think of shared libraries being opened
with `dlopen() <https://linux.die.net/man/3/dlopen>`_ at *runtime*. To avoid the requirement
for shared libraries and ``dlopen()``, MACSio_ uses a simpler *link-time* approach to handling
plugins called *static plugins*. In this case, the plugin's available to MACSio_ are determined
when MACSio_'s main() is linked. Whatever plugins are included on the link line are then
available to MACSio_.

MACSio_ exploits a feature in C++ which permits `initialization of static variables via
non-constant expressions
<https://eli.thegreenplace.net/2011/03/08/non-constant-global-initialization-in-c-and-c>`_
In fact, the only reason MACSio_ requires a C++ compiler is for the final link to support this
feature.

All symbols in a plugin source file are defined with ``static`` scope. Every plugin defines
an ``int register_this_plugin(void)`` function and initializes a static dummy integer to the
result of a call to ``register_this_plugin()`` like so...

.. code-block:: c

    static int register_this_plugin(void)
    {
      MACSIO_IFACE_Handle_t iface;

      strcpy(iface.name, iface_name);
      strcpy(iface.ext, iface_ext);

      if (!MACSIO_IFACE_Register(&iface))
          MACSIO_LOG_MSG(Die, ("Failed to register interface \"%s\"", iface.name));
    }
    static int dummy = register_this_plugin();

In the above code example, the call to :any:`MACSIO_IFACE_Register` calls a method
in MACSio_'s main which does the work of adding the plugin's interface specification
(:any:`MACSIO_IFACE_Handle_t`) to a statically defined table of plugins. The plugin's
``id`` is its location in that table.

Each plugin is defined by a file such as ``macsio_foo.c``
for a plugin named foo in the ``plugins`` directory.
``macsio_foo.c`` implements the ``MACSIO_IFACE`` interface for the foo plugin.

At the time the executable loads, the ``register_this_plugin()`` method is called. Note that
this is called long before even ``main()`` is called. The
call to ``MACSIO_IFACE_Register()`` from within ``register_this_plugin()`` winds up
adding the plugin to MACSio_'s global list of plugins. This happens for each plugin. The order
in which they are added to MACSio_ doesn't matter because plugins are identified by their
(unique) names. If MACSio_ encounters a case where two different plugins have the same
name, then it will abort and inform the user of the problem. The remedy is to
adjust the name of one of the two plugins. MACSio_ is able to call ``static`` methods
defined within the plugin via function callback pointers registered with the interface.

.. doxygengroup:: MACSIO_IFACE

.. toctree::
   :maxdepth: 1
   :caption: Plugins:

   Simple Example <macsio_miftmpl>
   macsio_hdf5
   macsio_silo
   macsio_pdb
   macsio_exodus
   macsio_typhonio
