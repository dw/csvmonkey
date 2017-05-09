
class IteratorStreamCursor
    : public BufferedStreamCursor
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
        Py_CLEAR(iter_);
    }

    virtual ssize_t readmore()
    {
        PyObject *result = PyIter_Next(iter_);
        if(! result) {
            return -1;
        }

        if(! PyString_CheckExact(result)) {
            PyErr_SetString(PyExc_TypeError,
                "CSV iterable must yield exactly a string.");
            Py_DECREF(result);
            return -1;
        }

        size_t sz = PyString_GET_SIZE(result);
        size_t need = sz + size_;
        if(need > buf_.size()) {
            buf_.resize(need);
        }

        memcpy(&buf_[size_], PyString_AS_STRING(result), sz);
        Py_DECREF(result);
        return sz;
    }
};
