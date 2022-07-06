# libbedic

This is a fork of libbedic 1.1 from https://sourceforge.net/projects/bedic/,
a library for building and handling electronic dictionaries.

Originally used for [zbedic](http://bedic.sourceforge.net/) (please note that
the page on the end of this link has been subtly vandalised in at least 2 places, one
to add an extra link to the list of man pages and another to the end of the side menu).

# Objectives

The primary aim of this project is to make the code buildable with a modern C++ compiler
and add support for Thai letter order.

A secondary aim is to update the code to modern C++ standards and extend support
to any languages added to Unicode since the original development of libbedic.

# Licence and Acknowledgements

The code for libbedic 1.1 was released on SourceForge under GPL v2 although some of the source
files include headers indicating slightly different terms. Therefore the majority
of this code should be considered GPL v2 unless individual files state otherwise.

## List of Known Authors and Contributors 

* Lyndon Hill (myself)
* Rafal Mantiuk
* Latchesar Ionkov
* Radostin Radnev, author of kbedic
* Simakov Alexander
* Rob Pike and Howard Trickey, for utf8.h and utf8.cpp from 9libs library, (c) Lucent Technologies

# Changelog

- 06.07.2022  More reformatting and fixing static analysis warnings
- 03.07.2022  Fixes for static analysis warnings
- 29.06.2022  Removing compiler warnings and reformatting code
- 28.06.2022  Test unit for DynamicDictionary built and working. Reformatting code
- 24.06.2022  First pass of fixing compilation errors, successfully built all objects
- 23.06.2022  Reviewed test_dynamic_dictionary and first pass of mkbedic
- 21.06.2022  Start of this new fork to make a publicly releaseable version
