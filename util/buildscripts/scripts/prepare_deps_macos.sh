#!/bin/sh

framework_name() {
  echo $1 | grep -o '[a-zA-Z0-9]*\.framework' | sed 's/.framework$//'
}

framework_version() {
  echo $1 | grep -o 'Versions/[0-9.]*' | sed 's/^Versions.//'
}

relative_dylib_path() {
  name=`framework_name $1`
  version=`framework_version $1`
  echo "${name}.framework/Versions/${version}/${name}"
}

install_framework_base_path() {
  echo $1 | grep -o '.*/[A-Za-z0-9]*.framework'
}

change_framework_id() {
  install_name_tool -id "@executable_path/../Frameworks/"`relative_dylib_path "${1}"` "${1}"
}

change_framework_dep() {
  install_name_tool -change "${1}" "@executable_path/../Frameworks/"`relative_dylib_path "${1}"` "${2}"
}

bundle_dir=`dirname $1`/../..
binary_name=`basename $1`

cd $bundle_dir

if [ ! -f Contents/MacOS/$binary_name ]; then
  echo "Usage: $0 Contents/MacOS/<binary>";
  exit 1;
fi

bin_deps=`otool -L Contents/MacOS/$binary_name | grep /usr/local | awk '{print $1}'`

# Find the Qt install and copy our plugins over that we need, then add their dependencies
# since for some reason qcocoa depends on QtPrintSupport :(
first_dep=`echo $bin_deps | tr ' ' '\n' | grep 'Qt' | tail -n 1`
first_dep_loC=`install_framework_base_path "${first_dep}"`
qt_install=`dirname "${first_dep_loC}" | sed 's#/lib##'`

echo "Copying plugins from $qt_install"
plugins="imageformats/libqsvg.dylib platforms/libqcocoa.dylib"

# Remove any symlink that might exist here
if [ -L Contents/qtplugins ]; then
	rm Contents/qtplugins;
fi

for plugin in $plugins; do
  mkdir -p "Contents/qtplugins/"`dirname "${plugin}"`

  cp "${qt_install}/plugins/${plugin}" "contents/qtplugins/"`dirname "${plugin}"`;

  install_name_tool -id "@executable_path/../qtplugins/${plugin}" "contents/qtplugins/${plugin}"
done

for I in `otool -L Contents/qtplugins/*/* | grep /usr/local | awk '{print $1}'`; do
  name=`framework_name $I`

  if echo $bin_deps | grep -q "${name}"; then
    continue;
  fi

  echo "Plugins need new dependency ${name}";

  bin_deps+=" ${I}";
done

# First copy in all the binary's dependencies and update their IDs
for I in $bin_deps; do
  name=`framework_name $I`
  version=`framework_version $I`
  echo "Application depends on $name version $version from $I"

  # Create and copy the framework locally
  local_path="Contents/Frameworks/${name}.framework/Versions/${version}"

  echo "Copying to ${local_path}"
  mkdir -p "${local_path}"
  cp -R `install_framework_base_path $I`/Versions/${version}/* "${local_path}"/

  chmod -R +w "${local_path}"/

  change_framework_id "${local_path}/${name}"
done

# Now check all of those libraries have repointed dependencies and that nothing is missing
for I in $bin_deps; do
  name=`framework_name $I`
  version=`framework_version $I`

  local_path="Contents/Frameworks/${name}.framework/Versions/${version}"

  for dep in `otool -L "${local_path}/${name}" | grep /usr/local | awk '{print $1}'`; do
    echo "Library ${local_path}/${name} depends on $dep"

    dep_name=`framework_name $dep`
    dep_version=`framework_version $dep`

    dep_local_path="Contents/Frameworks/${dep_name}.framework/Versions/${dep_version}"

    if [ ! -f "${dep_local_path}/${dep_name}" ]; then
      echo "Which is missing (expected ${dep_local_path}/${dep_name})";
      exit 1;
    fi

    echo "Patching..."

    change_framework_dep $dep "${local_path}/${name}"
  done

  # Finally change this dependency in the binary
  change_framework_dep "${I}" Contents/MacOS/$binary_name
done

# And the same for plugins
for plugin in $plugins; do
  for dep in `otool -L "Contents/qtplugins/${plugin}" | grep /usr/local | awk '{print $1}'`; do
    echo "Plugin ${plugin} depends on $dep"

    dep_name=`framework_name $dep`
    dep_version=`framework_version $dep`

    dep_local_path="Contents/Frameworks/${dep_name}.framework/Versions/${dep_version}"

    if [ ! -f "${dep_local_path}/${dep_name}" ]; then
      echo "Which is missing (expected ${dep_local_path}/${dep_name})";
      exit 1;
    fi

    echo "Patching..."

    change_framework_dep $dep "contents/qtplugins/${plugin}"
  done
done

# Remove Qt headers folders
rm -rf Contents/Frameworks/Qt*/Versions/*/Headers

# Remove python binaries, headers, and docs
rm -rf Contents/Frameworks/Python.framework/Versions/*/{bin,Resources,include,share/doc}

# Trim python libraries
rm -rf Contents/Frameworks/Python.framework/Versions/*/lib/python*/{site-packages,test,ensurepip,distutils,idlelib,config-*}

# Remove any __pycache__ folders
for pycache in `find Contents/Frameworks/Python.framework -name __pycache__`; do
  rm -rf ./"${pycache}";
done
