#!/bin/bash

# Command file for Sphinx documentation, based on make.bat
# For use in msys or similar environment with bash & python
# in the path, but no make to run the makefile

# Autodetect sphinx-build or sphinx-build.exe
if [ "z$SPHINXBUILD" == "z" ]; then

	if which sphinx-build.exe > /dev/null 2>&1; then
		SPHINXBUILD=sphinx-build.exe
	elif which sphinx-build > /dev/null 2>&1; then
		SPHINXBUILD=sphinx-build
	else
		echo "Can't find sphinx-build in PATH. Add path or set $SPHINXBUILD";
		exit 1;
	fi

fi

# Autodetect hhc.exe (windows only) in PATH or common locations
if [ "z$HHCBUILD" == "z" ]; then

	if which hhc.exe > /dev/null 2>&1; then
		HHCBUILD="$(which hhc.exe)";
	elif [ -f "C:\Program Files (x86)\HTML Help Workshop\hhc.exe" ]; then
		HHCBUILD="C:\Program Files (x86)\HTML Help Workshop\hhc.exe"
	elif [ -f "/c/Program Files (x86)/HTML Help Workshop/hhc.exe" ]; then
		HHCBUILD="/c/Program Files (x86)/HTML Help Workshop/hhc.exe"
	elif [ -f "/mnt/c/Program Files (x86)/HTML Help Workshop/hhc.exe" ]; then
		HHCBUILD="/mnt/c/Program Files (x86)/HTML Help Workshop/hhc.exe"
	else
		# we won't invoke hhc.exe
		HHCBUILD=""
	fi
fi

# Autodetect python3 executable
if which python3.exe > /dev/null 2>&1; then
	PYTHON=python3.exe
elif which python.exe > /dev/null 2>&1; then
	PYTHON=python.exe
elif which python3 > /dev/null 2>&1; then
	PYTHON=python3
elif which python > /dev/null 2>&1; then
	PYTHON=python
else
	echo "Can't find python in PATH.";
	exit 1;
fi

BUILDDIR="../Documentation"
ALLSPHINXOPTS="-d $BUILDDIR/doctrees $SPHINXOPTS ."
I18NSPHINXOPTS="$SPHINXOPTS ."
if [ z$PAPER != "z" ]; then
	ALLSPHINXOPTS="-D latex_paper_size=$PAPER $ALLSPHINXOPTS"
	I18NSPHINXOPTS="-D latex_paper_size=$PAPER $I18NSPHINXOPTS"
fi

if [ $# -ne 1 ] || [ z$1 == "z" ] || [ $1 == "help" ]; then
	echo 'Please use "make <target>" where <target> is one of'
	echo "  html       to make standalone HTML files"
	echo "  dirhtml    to make HTML files named index.html in directories"
	echo "  singlehtml to make a single large HTML file"
	echo "  pickle     to make pickle files"
	echo "  json       to make JSON files"
	echo "  htmlhelp   to make HTML files and a HTML help project"
	echo "  qthelp     to make HTML files and a qthelp project"
	echo "  devhelp    to make HTML files and a Devhelp project"
	echo "  epub       to make an epub"
	echo "  epub3      to make an epub3"
	echo "  latex      to make LaTeX files, you can set PAPER=a4 or PAPER=letter"
	echo "  text       to make text files"
	echo "  man        to make manual pages"
	echo "  texinfo    to make Texinfo files"
	echo "  gettext    to make PO message catalogs"
	echo "  changes    to make an overview over all changed/added/deprecated items"
	echo "  xml        to make Docutils-native XML files"
	echo "  pseudoxml  to make pseudoxml-XML files for display purposes"
	echo "  linkcheck  to check all external links for integrity"
	echo "  doctest    to run all doctests embedded in the documentation if enabled"
	echo "  coverage   to run coverage check of the documentation if enabled"
	echo "  dummy      to check syntax errors of document sources"
	exit
fi

if [ $1 == "clean" ]; then
	rm -rf $BUILDDIR
	exit
fi

# Check if sphinx-build is available and fallback to Python version if any
"$SPHINXBUILD" --help > /dev/null 2>&1

if [ $? != 0 ]; then
	echo "The 'sphinx-build' command was not found at '$SPHINXBUILD'."
	echo "Install sphinx-build with 'pip install sphinx' or set SPHINXBUILD"
	echo "to the path of the sphinx-build command"
	exit
fi

if [ $1 == "html" ]; then
	"$SPHINXBUILD" -b html $ALLSPHINXOPTS $BUILDDIR/html
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The HTML pages are in $BUILDDIR/html."
	exit
fi

if [ $1 == "dirhtml" ]; then
	"$SPHINXBUILD" -b dirhtml $ALLSPHINXOPTS $BUILDDIR/dirhtml
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The HTML pages are in $BUILDDIR/dirhtml."
	exit
fi

if [ $1 == "singlehtml" ]; then
	"$SPHINXBUILD" -b singlehtml $ALLSPHINXOPTS $BUILDDIR/singlehtml
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The HTML pages are in $BUILDDIR/singlehtml."
	exit
fi

if [ $1 == "pickle" ]; then
	"$SPHINXBUILD" -b pickle $ALLSPHINXOPTS $BUILDDIR/pickle
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished; now you can process the pickle files."
	exit
fi

if [ $1 == "json" ]; then
	"$SPHINXBUILD" -b json $ALLSPHINXOPTS $BUILDDIR/json
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished; now you can process the JSON files."
	exit
fi

if [ $1 == "htmlhelp" ]; then
	"$SPHINXBUILD" -b htmlhelp -t htmlhelp $ALLSPHINXOPTS $BUILDDIR/htmlhelp
	if [ $? != 0 ]; then exit 1; fi
	# Copy handwritten index file to output, overwriting auto-generated one
	cp renderdoc.hhk $BUILDDIR/htmlhelp
	# Copy introduction page over index.html
	cp $BUILDDIR/htmlhelp/introduction.html $BUILDDIR/htmlhelp/index.html
	# Filter out the auto-generated TOC to remove anchor links and root index.html
	cat $BUILDDIR/htmlhelp/renderdoc.hhc | "$PYTHON" remove_lines.py ".html#" | "$PYTHON" remove_lines.py "\"index.html\"" > $BUILDDIR/htmlhelp/tmp
	mv $BUILDDIR/htmlhelp/tmp $BUILDDIR/htmlhelp/renderdoc.hhc
	if [ -f "${HHCBUILD}" ]; then
		"${HHCBUILD}" $BUILDDIR/htmlhelp/renderdoc.hhp
		echo "Build finished."
		exit
	fi
	echo
	echo "Build finished; now you can run HTML Help Workshop with the "
	echo ".hhp project file in $BUILDDIR/htmlhelp."
	echo "For future, you can either install HTML Help Workshop to the"
	echo "default path [C:\Program Files (x86)\HTML Help Workshop] or "
	echo "set the variable HHCBUILD to the path to hhc.exe."
	exit
fi

if [ $1 == "qthelp" ]; then
	"$SPHINXBUILD" -b qthelp $ALLSPHINXOPTS $BUILDDIR/qthelp
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished; now you can run "qcollectiongenerator" with the "
	echo ".qhcp project file in $BUILDDIR/qthelp, like this:"
	echo "> qcollectiongenerator $BUILDDIR\qthelp\RenderDoc.qhcp"
	echo "To view the help file:"
	echo "> assistant -collectionFile $BUILDDIR\qthelp\RenderDoc.ghc"
	exit
fi

if [ $1 == "devhelp" ]; then
	"$SPHINXBUILD" -b devhelp $ALLSPHINXOPTS $BUILDDIR/devhelp
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished."
	exit
fi

if [ $1 == "epub" ]; then
	"$SPHINXBUILD" -b epub $ALLSPHINXOPTS $BUILDDIR/epub
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The epub file is in $BUILDDIR/epub."
	exit
fi

if [ $1 == "epub3" ]; then
	"$SPHINXBUILD" -b epub3 $ALLSPHINXOPTS $BUILDDIR/epub3
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The epub3 file is in $BUILDDIR/epub3."
	exit
fi

if [ $1 == "latex" ]; then
	"$SPHINXBUILD" -b latex $ALLSPHINXOPTS $BUILDDIR/latex
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished; the LaTeX files are in $BUILDDIR/latex."
	exit
fi

if [ $1 == "latexpdf" ]; then
	"$SPHINXBUILD" -b latex $ALLSPHINXOPTS $BUILDDIR/latex
	cd $BUILDDIR/latex
	echo "Need to now run 'make all-pdf' in $BUILDDIR/latex"
	exit
fi

if [ $1 == "latexpdfja" ]; then
	"$SPHINXBUILD" -b latex $ALLSPHINXOPTS $BUILDDIR/latex
	cd $BUILDDIR/latex
	echo "Need to now run 'make all-pdf-ja' in $BUILDDIR/latex"
	exit
fi

if [ $1 == "text" ]; then
	"$SPHINXBUILD" -b text $ALLSPHINXOPTS $BUILDDIR/text
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The text files are in $BUILDDIR/text."
	exit
fi

if [ $1 == "man" ]; then
	"$SPHINXBUILD" -b man $ALLSPHINXOPTS $BUILDDIR/man
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The manual pages are in $BUILDDIR/man."
	exit
fi

if [ $1 == "texinfo" ]; then
	"$SPHINXBUILD" -b texinfo $ALLSPHINXOPTS $BUILDDIR/texinfo
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The Texinfo files are in $BUILDDIR/texinfo."
	exit
fi

if [ $1 == "gettext" ]; then
	"$SPHINXBUILD" -b gettext $I18NSPHINXOPTS $BUILDDIR/locale
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The message catalogs are in $BUILDDIR/locale."
	exit
fi

if [ $1 == "changes" ]; then
	"$SPHINXBUILD" -b changes $ALLSPHINXOPTS $BUILDDIR/changes
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "The overview file is in $BUILDDIR/changes."
	exit
fi

if [ $1 == "linkcheck" ]; then
	"$SPHINXBUILD" -b linkcheck $ALLSPHINXOPTS $BUILDDIR/linkcheck
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Link check complete; look for any errors in the above output "
	echo "or in $BUILDDIR/linkcheck/output.txt."
	exit
fi

if [ $1 == "doctest" ]; then
	"$SPHINXBUILD" -b doctest $ALLSPHINXOPTS $BUILDDIR/doctest
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Testing of doctests in the sources finished, look at the "
	echo "results in $BUILDDIR/doctest/output.txt."
	exit
fi

if [ $1 == "coverage" ]; then
	"$SPHINXBUILD" -b coverage $ALLSPHINXOPTS $BUILDDIR/coverage
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Testing of coverage in the sources finished, look at the "
	echo "results in $BUILDDIR/coverage/python.txt."
	exit
fi

if [ $1 == "xml" ]; then
	"$SPHINXBUILD" -b xml $ALLSPHINXOPTS $BUILDDIR/xml
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The XML files are in $BUILDDIR/xml."
	exit
fi

if [ $1 == "pseudoxml" ]; then
	"$SPHINXBUILD" -b pseudoxml $ALLSPHINXOPTS $BUILDDIR/pseudoxml
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. The pseudo-XML files are in $BUILDDIR/pseudoxml."
	exit
fi

if [ $1 == "dummy" ]; then
	"$SPHINXBUILD" -b dummy $ALLSPHINXOPTS $BUILDDIR/dummy
	if [ $? != 0 ]; then exit 1; fi
	echo
	echo "Build finished. Dummy builder generates no files."
	exit
fi

