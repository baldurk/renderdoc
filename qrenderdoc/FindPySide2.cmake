# Find PySide2
#
# PYSIDE2_INCLUDE_DIR - the folder for include files that contains PySide2/ and shiboken2/
# PYSIDE2_PYTHON_PATH - the folder to add to python's sys.path to locate the PySide modules
# PYSIDE2_LIBRARY_DIR - the folder containing the libshiboken2.so to link against with -lshiboken2
# PYSIDE2_FOUND       - true if PySide2 was successfully located

find_library(SHIBOKEN2_LIBRARY
             NAMES shiboken2)

get_filename_component(PYSIDE2_LIBRARY_DIR ${SHIBOKEN2_LIBRARY} DIRECTORY)

find_path(PYSIDE2_PACKAGE_INIT_PY
          NAMES PySide2/__init__.py)

get_filename_component(PYSIDE2_PACKAGE_DIR "${PYSIDE2_PACKAGE_INIT_PY}" DIRECTORY)
get_filename_component(PYSIDE2_PYTHON_PATH "${PYSIDE2_PACKAGE_DIR}" DIRECTORY)

find_path(PYSIDE2_INCLUDE_DIR
          NAMES PySide2/pyside.h
          HINTS ${PYSIDE2_PACKAGE_DIR}/include
          )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PySide2 DEFAULT_MSG PYSIDE2_INCLUDE_DIR PYSIDE2_PYTHON_PATH PYSIDE2_LIBRARY_DIR)

mark_as_advanced(PYSIDE2_INCLUDE_DIR PYSIDE2_PYTHON_PATH PYSIDE2_LIBRARY_DIR)
