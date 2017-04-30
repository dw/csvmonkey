
#include <Python.h>

#include "csvninja.hpp"


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

    virtual ssize_t readmore()
    {
        PyObject *result = PyObject_Call(callable_, empty_tuple_, NULL);
        if(! PyString_CheckExact(result)) {
            PyErr_SetString(PyExc_TypeError,
                "CSV callable must generate exactly a string.");
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




PyMODINIT_FUNC
initcsvninja(void)
{
    PyObject *mod = Py_InitModule3("csvninja", module_methods, "");
}
