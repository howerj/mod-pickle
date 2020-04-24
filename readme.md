# Module System For Pickle Interpreter

This is a module system for the Pickle Interpreter, which is an interpreter for
a TCL like language derived from an interpreter made by Antirez called 'picol'.
The interpreter itself is licensed under the BSD license, everything in this
repository (unless noted elsewhere) is licensed under the Unlicense.

The interpreter is available at: <https://github.com/howerj/pickle>.

## Building and Project Layout


## To Do

* Manual pages and documentation
* Make a small test driver application
* Make sure all sub-module tests are run.
* Make a build system using Make that would use git to pull down and update
sub-modules automatically.

Add modules for:

* HTTP <https://github.com/howerj/httpc>
* CDB <https://github.com/howerj/cdb>
* Shrink <https://github.com/howerj/shrink>
* Base64, JSON, Operating System Stuff...
* Linenoise for CLI command completion <https://github.com/arangodb/linenoise-ng>

