//
// embed.i
// SWIG file embedding the Python interpreter in something else.
// This file is deprecated and no longer actively maintained, but it still
// seems to work with Python 2.7.  Status with Python 3 is unknown.
//
// This file makes it possible to extend Python and all of its
// built-in functions without having to hack its setup script.
//


#ifdef AUTODOC
%subsection "embed.i"
%text %{
This module provides support for building a new version of the
Python executable.  This will be necessary on systems that do
not support shared libraries and may be necessary with C++
extensions.  This file contains everything you need to build
a new version of Python from include files and libraries normally
installed with the Python language.

This module will automatically grab all of the Python modules
present in your current Python executable (including any special
purpose modules you have enabled such as Tkinter).   Thus, you
may need to provide additional link libraries when compiling.

As far as I know, this module is C++ safe.
%}
#endif

%wrapper %{

#include <Python.h>

#ifdef __cplusplus
extern "C"
#endif
void SWIG_init();  /* Forward reference */

#define _PyImport_Inittab swig_inittab

/* Grab Python's inittab[] structure */

#ifdef __cplusplus
extern "C" {
#endif
#include <config.c>

#undef _PyImport_Inittab

/* Now define our own version of it.
   Hopefully someone does not have more than 1000 built-in modules */

struct _inittab SWIG_Import_Inittab[1000];

static int  swig_num_modules = 0;

/* Function for adding modules to Python */

static void swig_add_module(char *name, void (*initfunc)()) {
	SWIG_Import_Inittab[swig_num_modules].name = name;
	SWIG_Import_Inittab[swig_num_modules].initfunc = initfunc;
	swig_num_modules++;
	SWIG_Import_Inittab[swig_num_modules].name = (char *) 0;
	SWIG_Import_Inittab[swig_num_modules].initfunc = 0;
}

/* Function to add all of Python's built-in modules to our interpreter */

static void swig_add_builtin() {
	int i = 0;
	while (swig_inittab[i].name) {
		swig_add_module(swig_inittab[i].name, swig_inittab[i].initfunc);
		i++;
	}
#ifdef SWIGMODINIT
	SWIGMODINIT
#endif
	/* Add SWIG builtin function */
	swig_add_module(SWIG_name, SWIG_init);
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int Py_Main(int, char **);

#ifdef __cplusplus
}
#endif

extern struct _inittab *PyImport_Inittab;

int
main(int argc, char **argv) {
	swig_add_builtin();
	PyImport_Inittab = SWIG_Import_Inittab;
	return Py_Main(argc,argv);
}

%}
