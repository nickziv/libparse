DISCLAIMER
==========

THIS SOFTWARE IS HARDLY TESTED AND IS UNDER HEAVY DEVELOPMENT.

UNSUPERVISED EXPOSURE CAN RESULT IN:

 - stroke
 - brain-cancer
 - cardiac arythmia
 - sterility
 - miscairage
 - bubonic plague

Intro
=====

libparse is a new parsing library for C. It can be used directly, it can be
targeted by parser-generators, it can be used through any arbitrary dynamic
domain specific language, or it can generate 'native' parsers.

Terminology
===========

We use some terms like `REGEX` which do not mean what you think they mean.
These are vestigial terms from an earlier phase in the design of the library.
We will eventually want to change them.

Design
======

The design philosophy of libparse is as follows:

Variable char-width: libparse doesn't assume 8-bit characters. It can deal with
any number of bits, and it doesn't have to be uniform. This means that libparse
can be used to parse ascii, utf8, utf64, and pretty much any kind of binary
data.

Library architecture: implementing the parsing framework as a library, allows
developers to use it directly, while preserving the capability to build domain
specific languages on top of it. Additionally, because there is no global state
in the library, developers can specify multiple parsers in the same library
consumer. This can be done in yacc, with a hack. The hack involves running a
script over the generated C code files and changing the variable and function
naming. The developer shouldn't have to bend over backwards like this.

Graph-based: The grammars are specified as a libgraph graph. The grammar is
essentially static data. We can run a grammar on an input by 'interpreting' the
graph. However, since the grammar is just data, one can write code to compile
the grammar-graph into code (any code). Furthermore, the AST is implemented as
a graph, and is built automatically, when running a grammar on an input. The
calls for manipulating the AST are also available to the consumer, so that the
consumer can -- for example -- programatically modify the AST. Additionally
grammars can be programmatically modified, which makes it easy to automate away
repetitive tasks.

Limitations
===========

Dynamic Tokens
--------------

Some things can only be known at run time. For example the terminating token of
a Perl here-doc is only known at run time. We have no solution for this, yet.
