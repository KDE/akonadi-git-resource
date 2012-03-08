#! /bin/sh
$EXTRACTRC *.ui >> rc.cpp
$XGETTEXT `find -name '*.cpp'` -o $podir/akonadi_git_resource.pot
rm -f rc.cpp
