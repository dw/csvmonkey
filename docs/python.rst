
Python API
==========



Factory Functions
-----------------


Common Arguments
^^^^^^^^^^^^^^^^

* **yields**: Specify the kind of value returned during iteration.

  * `row`: Cause a :class:`Row` to be yielded. Rows are dict/sequence-like
    objects that support lazy decoding.
  * `list`: Cause a fully decoded list to be yielded.
  * `tuple`: Cause a fully decoded tuple to be yielded.
  * `dict`: Cause a fully decoded dict to be yielded, in the style of
    :class:`csv.DictReader`.

* **header**: Specify whether a header row exists, or specifies an explicit set
  of column names. The header row is used to form keys available via the
  :class:`Row` object, or for constructing dicts. May be any of:

  * :data:`True`: a header row exists and should be read away during
    construction.
  * :data:`False`: no header row exists.


:param: header
    Foo bar baz.
:param: delimiter
    Foo bar baz.
:param: quotechar
    Foo bar baz.
:param: escapechar
    Foo bar baz.
:param bool: yield_incomplete_row
    Foo bar baz.
:param: encoding
    Name of the encoding
:param str: errors
    One of "strict", "ignore" or "replace".




.. function:: from_iter



.. function:: from_file
.. function:: from_path

