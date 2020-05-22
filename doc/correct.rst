-------
correct
-------

give command suggestions for a given string
===========================================

:date: May 2020
:version: 0.0
:manual section: 1
:manual group: General Commands Manual

synopsis
--------
| correct `<string>`

description
-----------
correct tries to correct a string to a valid command by finding the closest match in the user's $PATH

it outputs a list of commands, one per line

example
-------
::

    $ correct makee
    make
    cmake
    faked
    makedb
    qmake
