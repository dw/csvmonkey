
class FileStreamCursor
    : public csvmonkey::BufferedStreamCursor
{
    PyObject *args_tuple_;
    PyObject *read_;

    public:
    FileStreamCursor(PyObject *read)
        : BufferedStreamCursor()
        , read_(read)
        , args_tuple_(Py_BuildValue("(i)", 8192))
    {
        assert(args_tuple_ != 0);
    }

    ~FileStreamCursor()
    {
        Py_DECREF(args_tuple_);
        Py_DECREF(read_);
    }

    virtual ssize_t readmore()
    {
        PyObject *result = PyObject_Call(read_, args_tuple_, NULL);
        CSM_DEBUG("result = %lu", result);
        if(! result) {
            return -1;
        }

        if(! PyString_CheckExact(result)) {
            PyErr_SetString(PyExc_TypeError,
                "CSV iterable must yield exactly a string.");
            Py_DECREF(result);
            return -1;
        }

        Py_ssize_t sz = PyString_GET_SIZE(result);
        if(! sz) {
            return -1;
        }

        ensure(sz);
        memcpy(&vec_[write_pos_], PyString_AS_STRING(result), sz);
        Py_DECREF(result);
        return sz;
    }
};
