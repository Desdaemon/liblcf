#!/bin/bash

# release-helper.sh - maintainer utility script to change
# the library version and update all copyright dates
# by carstene1ns 2021, released under the MIT license

set -e

year=$(date +%Y)
version=$1

if [[ ! -z $version ]]; then

  if [[ ! $version =~ ^[0-9]\.[0-9]\.[0-9]$ ]]; then

    echo "Invalid version argument. Only digits and dots allowed."
    exit 1

  fi

  echo "Updating Version in:"

  echo "  CMakeLists.txt"
  sed -i "/liblcf VERSION/,1 s/[0-9]\.[0-9]\.[0-9]/$version/" CMakeLists.txt

  echo "  configure.ac"
  sed -i "/AC_INIT/,1 s/[0-9]\.[0-9]\.[0-9]/$version/" configure.ac

  echo "  README.md"
  sed -i "s/\(liblcf-\)[0-9]\.[0-9]\.[0-9]/\1$version/g" README.md

else

  echo "No new version argument, only updating copyright years!"

fi

echo "Updating License…"
sed -i "1,1 s/2014-2[0-9][0-9][0-9]/2014-$year/" COPYING

cat << EOF

If everything is ready and committed, use these commands to publish the git tag:
$ git tag -a (-s) $version -m "Codename \"\fancy codename\""
$ git push (-n) --tags upstream
EOF
