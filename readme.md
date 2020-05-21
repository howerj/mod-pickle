# Module System For Pickle Interpreter

**This project is a work in progress**. It is in pre-alpha at the moment so
should not be counted on to actually do anything useful.

This is a module system for the Pickle Interpreter, which is an interpreter for
a TCL like language derived from an interpreter made by Antirez called 'picol'.
The interpreter itself is licensed under the BSD license, everything in this
repository (unless noted elsewhere) is licensed under the Unlicense.

The interpreter is available at: <https://github.com/howerj/pickle>.

## To Do

* Manual pages and documentation.
* Make a build system using Make that would use git to pull down and update
sub-modules automatically.
* Add targets to make and install the shell and modules, these are the 'dist'
  and 'install' makefile targets.
* Write wrappers for all of the modules that are pulled in.

Add modules for:

* HTTP <https://github.com/howerj/httpc>
* CDB <https://github.com/howerj/cdb>
* UTF-8 <https://github.com/howerj/utf8>
* Shrink <https://github.com/howerj/shrink>
* Base64, JSON, Operating System Stuff...
* Linenoise for CLI command completion <https://github.com/arangodb/linenoise-ng>
* Add a library for manipulating ANSI terminal escape sequences, this could be
done in TCL, which may make the linenoise library redundant if a few C
functions were added (such as 'isatty' and 'getch').

