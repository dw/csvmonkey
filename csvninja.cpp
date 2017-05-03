
#include <Python.h>

#include "csvninja.hpp"


static PyTypeObject CursorType;
static PyTypeObject ReaderType;



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


struct ReaderObject
{
    PyObject_HEAD
    //CallableStreamCursor cursor;
    MappedFileCursor cursor;
    CsvReader reader;
};


static PyMethodDef key_methods[] = {
    {"__sizeof__",  (PyCFunction)key_sizeof,   METH_NOARGS, ""},
    {"from_hex",    (PyCFunction)key_from_hex, METH_VARARGS|METH_CLASS, ""},
    {"from_raw",    (PyCFunction)key_from_raw, METH_VARARGS|METH_CLASS, ""},
    {"to_raw",      (PyCFunction)key_to_raw,   METH_VARARGS,            ""},
    {"to_hex",      (PyCFunction)key_to_hex,   METH_VARARGS|METH_KEYWORDS, ""},
    {"prefix_bound",(PyCFunction)key_prefix_bound, METH_NOARGS, ""},
    {0,             0,                         0,                       0}
};


static void
reader_dealloc(ReaderObject *self)
{
    self->reader.~CsvReader();
    self->cursor.~MappedFileCursor();
    PyObject_Del(self);
}


static PyObject *
reader_from_path(PyObject *_self, PyObject *args)
{
    if(PyTuple_GET_SIZE(args) < 1) {
        PyErr_SetString(PyExc_TypeError, "from_path() requires at least one argument.");
        return NULL;
    }

    PyObject *path = PyTuple_GET_ITEM(args, 0);
    if(! PyString_CheckExact(path)) {
        PyErr_SetString(PyExc_TypeError, "from_path() argument must be string.");
        return NULL;
    }

    ReaderObject *self = (ReaderObject *) PyObject_Malloc(sizeof(ReaderObject));
    if(! self) {
        return NULL;
    }

    PyObject_Init((PyObject *) self, &ReaderType);
    new (&(self->cursor)) MappedFileCursor();
    if(! self->cursor.open(PyString_AS_STRING(path))) {
        self->cursor.~MappedFileCursor();
        PyErr_SetString(PyExc_IOError, "Could not open file.");
        Py_DECREF(self);
        return NULL;
    }

    new (&(self->reader)) CsvReader(self->cursor);
    return (PyObject *) self;
}


static PyTypeObject ReaderType = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "csvninja._Reader",
    .tp_basicsize = sizeof(ReaderObject),
    .tp_itemsize = 1,
    .tp_iter = (getiterfunc) reader_iter,
    .tp_new = key_new,
    .tp_dealloc = (destructor) reader_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "acid._keylib.Key",
    .tp_methods = key_methods,
    .tp_as_sequence = &key_seq_methods,
    .tp_as_mapping = &key_mapping_methods
};



static struct PyMethodDef module_methods[] = {
    {"from_path", (PyCFunction) reader_from_path, METH_VARARGS},
    {0, 0, 0, 0}
};


PyMODINIT_FUNC
initcsvninja(void)
{
    PyObject *mod = Py_InitModule3("csvninja", module_methods, "");
}
