# VolatileFunction
Makes functions containing a specific block of code volatile.

It could be "VolatileBlock", but I was unable to make block volatilization work (`Rewriter.cpp`).

Code is a bit dirty (a global variable, a bunch of bash scripts), but I got lazy at some point.

Also bash scripts carry some information of the way I run this, though tools shouldn't rely on the name of build directory (but `Runner.cpp` does).
