#
# libBlu mainMuxer build instructions.
#

1. Dependencies

Please see DEPENDENCIES for building and runtime dependencies.

2. Configuration

Currently, the projet uses a simple GNU Makefile. Configuration is done by
triggering flags and using command line options.

Inside makefile, look for the header to set your compiler (by default, the
project uses GCC).
Below, in "Compilation switches" section, IGS compiler can be disabled
(removing libpng and libxml dependencies) as INI files based configuration
(removing lex/yacc dependencies).

3. Compilation flags

Following flags can be used:

  linux          Builds for linux.
  windows        Builds for windows.
  no_debug       Disable any debugging functionnality.
  build          Compiles in 'release' mode, turning compiler optimizations
                 on and assertions off.
  leaks          Compiles with fsanitize for memory leaks/error looking.
  pg             Compiles with profiling informations.

4. Compilation

On linux:

make linux build

On windows:

make windows build
