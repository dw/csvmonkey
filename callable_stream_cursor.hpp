
class CallableStreamCursor
    : public BufferedStreamCursor
{
    PyObject *callable_;
    PyObject *empty_tuple_;

    public:
    CallableStreamCursor(PyObject *callable)
        : BufferedStreamCursor()
        , callable_(callable)
        , empty_tuple_(PyTuple_New(0))
    {
        assert(empty_tuple_ != NULL);
    }

    ~CallableStreamCursor()
    {
        Py_CLEAR(callable_);
        Py_CLEAR(empty_tuple_);
    }

    virtual ssize_t readmore()
    {
        PyObject *result = PyObject_Call(callable_, empty_tuple_, NULL);
        if(! PyString_CheckExact(result)) {
            PyErr_SetString(PyExc_TypeError,
                "CSV callable must generate exactly a string.");
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
