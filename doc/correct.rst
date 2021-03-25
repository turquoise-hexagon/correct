-------
correct
-------

give correction suggestions for a given string
==============================================

:date: May 2020
:version: 0.0
:manual section: 1
:manual group: General Commands Manual

synopsis
--------
`<list>` | correct `<string>`

description
-----------
correct tries to correct a string by finding the closest match in a list of strings provided via stdin

it outputs a list of the best matches, one per line

example
-------
::

    $ compgen -c | correct take
    make
    rake
    case
    cmake
    date
