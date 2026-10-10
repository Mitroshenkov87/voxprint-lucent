#!/bin/sh
# Build the documentation site: Doxygen HTML plus the MkDocs pages.
# GitHub Pages publishes the site/ directory. Pull requests run the same
# Doxygen and MkDocs commands and do not publish.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
mkdir -p build/doxygen
doxygen Doxyfile
python -m mkdocs build --strict
rm -rf site/cpp
mkdir -p site/cpp
cp -a build/doxygen/html/. site/cpp/
