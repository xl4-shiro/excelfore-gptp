## Prerequisites
C99 standard level library functions and C compiler functions must be supported to
build and use this library.

## update
Because 'unibase' is very basic module, the decided APIs shouldn't be changed unless
it has strong enough reason.

When a new layer is added, it should update this this 'README.md' to include the
description of 'why the layer is needed'
The added layer MUST have unittest program which runs by 'make check'.
Finally it should have peer review before merged into the main branch.

For minor improvement, e.g. adding one macro, it needs to update the corresponding
API document and unittest.  Even minor changes must have update of the unittest.

Whatever changes must be noticed to all software engineers in Excelfore Japan.

## overriding standard functions
Some platforms don't allow to use malloc.
In such case, malloc function must be overridden in user applicatoin.
```
#define malloc user_malloc
```
can replace 'malloc' to 'user_malloc', but 'user_malloc' definition must exist.

To accommodate such overriding of standard functions, 'unibase.h' has the next
statements at the very beginning of the file.
```
#ifdef UB_OVERRIDE_H
#include UB_OVERRIDE_H
#endif
```
adding
```
-DUB_OVERRIDE_H=\"user_override.h\"
```
in the preprocessor command line,
'user_override.h' is included, and the user can add any definition of
overriding in the file.

## initialization
The initialization is done by 'unibase_init', and it must be closed by
calling 'unibase_close'.

In the initialization parameters, there is a set of callback functions,
that support all of needed platform specific functions.
To be simple we minimized the callback functions.
For the detail of the callback functions, look at unibase.h

The other part of the initialization is defining logging levels.
The logging level can be initialized by a single string, and applications
can change it at run time.
For the detail of the logging control, look at ub_logging.h

## ub_logging
We know there is no best way for logging messages.
It is always too few lines or too noisy.

To be close to the best fitting of logging, flexible and reasonably easy
run-time configuration is important.
We got a hint from [the way in 'gstreamer']
(https://gstreamer.freedesktop.org/documentation/tutorials/basic/debugging-tools.html).

'ub_logging' supports configuration by a string.

For more detail, look at ub_logging.h

## ub_esarray
We often meet a situation in which data array is needed but the element
size can't be predefined.
In many cases, we define MAX_NUMBER_OF_... kind of macro, and allocate
possible maximum size.

That way wastes memory and requires tiresome checks not to exceed the maximum size.

'ub_esarray' can ease such situation.
Set 'esunit' as maximum element size in common usage.  The element size
expands by the unit of 'esunit' up to 'maxeles', and shrinks when it has 2*'esunit'
of unused elements.  Because of the hysteresis of 'esunit' between expand and shrink,
re-allocation doesn't happen so often.

For more detail, look at ub_esarray.h

## ub_strutils
Some string related functions are supported here.
Especially handling MAC address and Stream ID are focused,
because of our working area.

For more detail, look at ub_strutils.h

## ub_llist
simple doubly linke list.

For more detail, look at ub_llist.h

## ub_confuitls
When there are so many configuration items, using command line arguments is
not practically useful.

'ub_genconf.py' generates c codes from a default definition file.
Using the generated codes as a call back function for 'read_config_file',
runtime configuration by a configuration file is easily achieved.
