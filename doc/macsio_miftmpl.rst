Example (macsio_miftmpl.c)
^^^^^^^^^^^^^^^^^^^^^^^^^^
Writing a new plugin for MACSio involves a mininum of two new files in the plugins directory.
One is the C or C++ source code for the plugin. The other is a ``.make`` file that includes
various plugin-specific variable definitions and logic to decode library dependencies that
effect.

The *miftmpl* plugin is intended to serve as a template for how to create a basic MIF_ 
mode plugin for MACSio_.  This template code does indeed actually function correctly as a
MACSio_ plugin. It does so by writing MACSio_'s internal JSON objects repesenting each
mesh part as ascii strings to the individual files.

Each processor in a MIF_ group serializes each JSON object representing a mesh part to an
ascii string. Then, each of these strings is appended to the end of the file. For each
such string, the plugin maintains knowledge of the mesh part's ID, the filename it was
written to and the offset within the file.

The filenames, offsets and mesh part IDs are then written out as a separate JSON object
to a root or master file. Currently, there is no plugin in VisIt to read these files
and display them. But, this example code does help to outline the basic work to write a
MIF_ plugin.

In practice, this plugin could have simply written the entire JSON object from each
processor to its MIF_ group's file. However, in doing that, the resulting file would
not "know" such things as how many mesh parts there are or where a given
mesh part is located in the file set. So, we wind up writing JSON objects for each
part individually so that we can keep track of where they all are in the fileset.

Some of the aspects of this plugin code exist here only to serve as an example in
writing a MIF plugin and are non-essential to the proper operation of this plugin.

MACSio_ uses a static load approach to its plugins. The MACSio_ main executable must
be linked with all the plugins it expects to use.

In writing any MACSio_ plugin (MIF_ or SSF) be sure to declare *all* of your plugin's
symbols (functions, local variables, etc.) as static. Each plugin is being linked into
MACSio_'s main and any symbols that are not static file scope will wind up appearing
in and therefore being vulnerable too global namespace collisions. The plugin's
main interface methods to MACSio_ are handled via registration of a set of function
pointers.

.. doxygenfile:: macsio_miftmpl.c

.. include:: ../plugins/macsio_miftmpl.c
   :code: c
   :start-line: 27
   :number-lines:
