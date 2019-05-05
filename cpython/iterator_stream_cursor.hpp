
class IteratorStreamCursor
    : public csvmonkey::BufferedStreamCursor
{
    PyObject *iter_;

    public:
    IteratorStreamCursor(PyObject *iter)
        : BufferedStreamCursor()
        , iter_(iter)
    {
    }

    ~IteratorStreamCursor()
    {
        Py_DECREF(iter_);
    }

    virtual ssize_t readmore()
    {
        PyObject *result = PyIter_Next(iter_);
        if(! result) {
            return -1;
        }

        if(! PyBytes_CheckExact(result)) {
            PyErr_SetString(PyExc_TypeError,
                "CSV iterable must yield exactly a string.");
            Py_DECREF(result);
            return -1;
        }

        Py_ssize_t sz = PyBytes_GET_SIZE(result);
        if(! sz) {
            return -1;
        }

        ensure(sz);
        memcpy(&vec_[write_pos_], PyBytes_AS_STRING(result), sz);
        Py_DECREF(result);
        return sz;
    }
};
