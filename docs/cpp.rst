
C++ API
=======

 .. default-domain:: cpp



Errors
------

.. class:: csvmonkey::Error : public std::exception

    Thrown in various places during setup, due to failed file IO or field
    extraction.

    .. function:: const char \*what() const

        Describe the reason for the error.


Cursors
-------

.. class:: csvmonkey::StreamCursor

    Abstract base for implementing iteration over an input stream.
    :class:`StreamCursor` is the interface used by :class:`CsvReader` to
    acquire and manage its input buffer.

    .. function:: virtual const char \*buf() = 0

        Current stream position. Must guarantee access to
        `buf()[0..size()+31]`, with 31 trailing NULs to allow safely running
        ``PCMPSTRI`` on the final data byte.

    .. function:: virtual size_t size() = 0

        Size of the buffer pointed to by :func:`buf`.

    .. function:: virtual void consume(size_t n) = 0

        Called by :class:`CsvReader` to indicate `n` bytes from the front of
        :func:`buf` have been consumed. The return value of :func:`buf` and
        :func:`size` should now reflect a buffer positioned on the first byte
        following `n`.

    .. function:: virtual bool fill() = 0

        Called by :class:`CsvReader` to request more input. This function
        returns `true` to indicate the buffer provided by :func:`buf` and
        :func:`size` has been extended, or `false` to indicate EOF.


.. class:: csvmonkey::MappedFileCursor : public StreamCursor

    Implement zero-copy input using a memory-mapped file.

    .. function:: void open(const char \*filename)

        Open `filename` for reading. Throws :class:`Error` on failure.


.. class:: csvmonkey::BufferedStreamCursor : public StreamCursor

    Base class for any cursor implementation that requires buffering.

    .. member:: std::vector<char> vec_

        The buffer

    .. member:: size_t write_pos_

        Current write offset within the buffer. New data appended to
        :member:`vec_` by :func:`readmore` should append past `write_pos_`.

    .. function:: void ensure(size_t capacity)

        Ensure at least `capacity` additional bytes are available in the buffer
        starting at the current write position.

    .. function:: virtual ssize_t readmore() = 0

        Arrange for more data to fill the buffer. Your implementation should
        issue some IO request and copy the result into `vec_[write_pos_:]`.
        The function should return -1 on error, 0 on EOF, or nonzero to
        indicate how many bytes were appended.


.. class:: csvmonkey::FdStreamCursor : public BufferedStreamCursor

    Implement buffered input from a UNIX file descriptor.

    .. function:: FdStreamCursor(int fd)

        Construct a new instance using `fd`.


CsvCell
-------

.. class:: csvmonkey::CsvCell

    Descriptor for a single parsed CSV field.

    Cells describe fields in terms of references to :func:`StreamCursor::buf`,
    and thus become invalid once the underlying stream cursor is mutated.
    :class:`CsvReader` reuses a single vector of cells throughout the run,
    therefore any cell returned after a successful :func:`CsvReader::parse_row`
    call are invalidated by the next call to :func:`CsvReader::parse_row`.
  
    .. member:: const char \*ptr

        Pointer to the start of the CSV field.
 
    .. member:: size_t size

        Size of the CSV field.

    .. member:: char escapechar

        Escape character configured for the :class:`CsvReader`.

    .. member:: char quotechar

        Quote character configured for the :class:`CsvReader`.

    .. member:: bool escaped

        If `true`, at least one escape character exists in the field. Its
        value must be accessed via :func:`CsvCell::as_str`.

    .. function:: std::string as_str()

        Return a string with the any quote and escapes decoded.
