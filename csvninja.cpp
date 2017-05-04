
#include <Python.h>

#include "csvninja.hpp"
// #include "callable_stream_cursor.hpp"


extern PyTypeObject CellType;
extern PyTypeObject ReaderType;


struct ReaderObject
{
    PyObject_HEAD
    //CallableStreamCursor cursor;
    MappedFileCursor cursor;
    CsvReader reader;

    // Map header string -> index.
    PyObject *header_map;
};


struct CellObject
{
    PyObject_HEAD;
    ReaderObject *reader; // strong ref.
    CsvCell *cell;
};


/*
 * Cell methods.
 */

static PyObject *
cell_as_double(CellObject *self)
{
    return PyFloat_FromDouble(self->cell->as_double());
}


static PyObject *
cell_as_str(CellObject *self)
{
    return PyString_FromStringAndSize(self->cell->ptr, self->cell->size);
}


static PyObject *
cell_equals(CellObject *self, PyObject *args)
{
    if(! PyTuple_GET_SIZE(args)) {
        return NULL;
    }

    PyObject *py_s = PyTuple_GET_ITEM(args, 0);
    if(! PyString_CheckExact(py_s)) {
        return NULL;
    }

    const char *s = PyString_AS_STRING(py_s);
    PyObject *py_true = (
        self->cell->equals(s) ?
        Py_True :
        Py_False
    );
    Py_INCREF(py_true);
    return py_true;
}


static int
cell_compare(CellObject *self, PyObject *o2)
{
    if(! PyString_CheckExact(o2)) {
        return -1;
    }

    const char *s = PyString_AS_STRING(o2);
    return self->cell->equals(s) ? 0 : -1;
}


static PyObject *
cell_richcmp(CellObject *self, PyObject *o2, int op)
{
    if(! PyString_CheckExact(o2)) {
        return NULL;
    }

    PyObject *out = Py_NotImplemented;
    if(op == Py_EQ) {
        const char *s = PyString_AS_STRING(o2);
        out = self->cell->equals(s) ? Py_True : Py_False;
    }

    Py_INCREF(out);
    return out;
}


static PyObject *
cell_new(ReaderObject *reader, CsvCell *cell)
{
    CellObject *py_cell = (CellObject *) PyObject_New(CellObject, &CellType);
    if(! py_cell) {
        return NULL;
    }

    Py_INCREF(reader);
    py_cell->reader = reader;
    py_cell->cell = cell;
    return (PyObject *) py_cell;
}


static void
cell_dealloc(CellObject *self)
{
    Py_DECREF(self->reader);
    self->reader = NULL;
    self->cell = NULL;
}


/*
 * Reader methods
 */

static void
reader_dealloc(ReaderObject *self)
{
    self->reader.~CsvReader();
    self->cursor.~MappedFileCursor();
    if(self->header_map) {
        Py_CLEAR(self->header_map);
    }
    PyObject_Del(self);
}


static void
build_header_map(ReaderObject *self)
{
    self->header_map = PyDict_New();
    assert(self->header_map);

    CsvCell *cell = &self->reader.row_.cells[0];
    for(int i = 0; i < self->reader.row_.count; i++) {
        PyObject *key = PyString_FromStringAndSize(cell->ptr, cell->size);
        PyObject *value = PyInt_FromLong(i);
        assert(key && value);
        PyDict_SetItem(self->header_map, key, value);
        Py_DECREF(key);
        Py_DECREF(value);
        cell++;
    }
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
    assert(self->reader.read_row());
    build_header_map(self);
    return (PyObject *) self;
}


static Py_ssize_t
reader_length(ReaderObject *self)
{
    return self->reader.row_.count;
}


static PyObject *
reader_getitem(ReaderObject *self, Py_ssize_t index)
{
    if(index < 0 || index > self->reader.row_.count) {
        PyErr_Format(PyExc_IndexError,
                     "index %ld greater than parsed col count %lu",
                     (unsigned long) index,
                     (unsigned long) self->reader.row_.count);
        return NULL;
    }

    CsvCell &cell = self->reader.row_.cells[index];
    return PyString_FromStringAndSize(cell.ptr, cell.size);
}


static Py_ssize_t
reader_getlength(ReaderObject *self)
{
    return (Py_ssize_t) self->reader.row_.count;
}


static PyObject *
reader_subscript(ReaderObject *self, PyObject *key)
{
    int index;

    if(PyInt_CheckExact(key)) {
        index = (int) PyInt_AS_LONG(key);
    } else {
        PyObject *py_index = PyDict_GetItem(self->header_map, key);
        if(! py_index) {
            PyErr_Format(PyExc_KeyError, "No such key.");
            return NULL;
        }
        index = (int) PyInt_AS_LONG(py_index);
        Py_DECREF(py_index);
    }

    if(index > self->reader.row_.count) {
        PyErr_Format(PyExc_IndexError,
                     "index %ld greater than parsed col count %lu",
                     (unsigned long) index,
                     (unsigned long) self->reader.row_.count);
        return NULL;
    }

    CsvCell *cell = &self->reader.row_.cells[index];
    return PyString_FromStringAndSize(cell->ptr, cell->size);
}


static PyObject *
reader_find_cell(ReaderObject *self, PyObject *args)
{
    const char *s;
    if(! PyArg_ParseTuple(args, "s:find_cell", &s)) {
        return NULL;
    }

    CsvCell *cell;
    if(! self->reader.row_.by_value(s, cell)) {
        PyErr_Format(PyExc_KeyError, "%s", s);
        return NULL;
    }

    return cell_new(self, cell);
}


static PyObject *
reader_iter(PyObject *self)
{
    Py_INCREF(self);
    return self;
}


static PyObject *
reader_iternext(ReaderObject *self)
{
    if(! self->reader.read_row()) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    Py_INCREF((PyObject *) self);
    return (PyObject *)self;
}


/*
 * Cell Type.
 */

static PyMethodDef cell_methods[] = {
    {"as_double",   (PyCFunction)cell_as_double, METH_NOARGS, ""},
    {"as_str",      (PyCFunction)cell_as_str, METH_NOARGS, ""},
    {"equals",      (PyCFunction)cell_equals, METH_VARARGS, ""},
    {0, 0, 0, 0}
};

PyTypeObject CellType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_Cell",                    /*tp_name*/
    sizeof(CellObject),         /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor) cell_dealloc,  /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    (cmpfunc) cell_compare,     /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash*/
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,         /*tp_flags*/
    "csvninja._Cell",           /*tp_doc*/
    0,                          /*tp_traverse*/
    0,                          /*tp_clear*/
    (richcmpfunc) cell_richcmp, /*tp_richcompare*/
    0,                          /*tp_weaklistoffset*/
    0,                          /*tp_iter*/
    0,                          /*tp_iternext*/
    cell_methods,               /*tp_methods*/
    0,                          /*tp_members*/
    0,                          /*tp_getset*/
    0,                          /*tp_base*/
    0,                          /*tp_dict*/
    0,                          /*tp_descr_get*/
    0,                          /*tp_descr_set*/
    0,                          /*tp_dictoffset*/
    0,                          /*tp_init*/
    0,                          /*tp_alloc*/
    0,                          /*tp_new*/
    0,                          /*tp_free*/
};


/*
 * Reader tpye.
 */

static PySequenceMethods reader_sequence_methods = {
    (lenfunc) reader_getlength,  /* sq_length */
    NULL,                        /* sq_concat */
    NULL,                        /* sq_repeat */
    (ssizeargfunc) reader_getitem,  /* sq_item */
};


static PyMappingMethods reader_mapping_methods = {
    (lenfunc) reader_getlength,      /* mp_length */
    (binaryfunc) reader_subscript,   /* mp_subscript */
    0,                               /* mp_ass_subscript */
};

static PyMethodDef reader_methods[] = {
    {"find_cell",   (PyCFunction)reader_find_cell, METH_VARARGS, ""},
    {0, 0, 0, 0}
};

PyTypeObject ReaderType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_Reader",                  /*tp_name*/
    sizeof(ReaderObject),       /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor) reader_dealloc,/*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    &reader_sequence_methods,   /*tp_as_sequence*/
    &reader_mapping_methods,    /*tp_as_mapping*/
    0,                          /*tp_hash*/
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,         /*tp_flags*/
    "csvninja._Reader",         /*tp_doc*/
    0,                          /*tp_traverse*/
    0,                          /*tp_clear*/
    0,                          /*tp_richcompare*/
    0,                          /*tp_weaklistoffset*/
    (getiterfunc) reader_iter,  /*tp_iter*/
    (iternextfunc) reader_iternext,  /*tp_iternext*/
    reader_methods,             /*tp_methods*/
    0,                          /*tp_members*/
    0,                          /*tp_getset*/
    0,                          /*tp_base*/
    0,                          /*tp_dict*/
    0,                          /*tp_descr_get*/
    0,                          /*tp_descr_set*/
    0,                          /*tp_dictoffset*/
    0,                          /*tp_init*/
    0,                          /*tp_alloc*/
    0,                          /*tp_new*/
    0,                          /*tp_free*/
};


/*
 * Module constructor.
 */

static struct PyMethodDef module_methods[] = {
    {"from_path", (PyCFunction) reader_from_path, METH_VARARGS},
    {0, 0, 0, 0}
};


PyMODINIT_FUNC
initcsvninja(void)
{
    PyObject *mod = Py_InitModule3("csvninja", module_methods, "");
    if(! mod) {
        return;
    }

    if(PyType_Ready(&CellType)) {
        return;
    }
    PyModule_AddObject(mod, "Cell", (PyObject *) &CellType);

    if(PyType_Ready(&ReaderType)) {
        return;
    }
    PyModule_AddObject(mod, "Reader", (PyObject *) &ReaderType);
}
