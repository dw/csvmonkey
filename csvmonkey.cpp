
#include <Python.h>

#include "csvmonkey.hpp"
#include "iterator_stream_cursor.hpp"
#include "file_stream_cursor.hpp"

using namespace csvmonkey;

extern PyTypeObject CellType;
extern PyTypeObject ReaderType;
extern PyTypeObject RowType;
struct RowObject;


enum CursorType
{
    CURSOR_MAPPED_FILE,
    CURSOR_ITERATOR,
    CURSOR_PYTHON_FILE
};


struct ReaderObject
{
    PyObject_HEAD
    CursorType cursor_type;
    StreamCursor *cursor;
    CsvReader reader;
    PyObject *(*yields)(RowObject *);
    int header;

    CsvCursor *row;
    PyObject *py_row;

    // Map header string -> index.
    PyObject *header_map;
};


struct CellObject
{
    PyObject_HEAD;
    ReaderObject *reader; // strong ref.
    CsvCell *cell;
};


struct RowObject
{
    PyObject_HEAD;
    ReaderObject *reader; // strong ref, keeps row alive.
    CsvCursor *row;
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
cell_to_str(CsvCell *cell)
{
    return PyString_FromStringAndSize(cell->ptr, cell->size);
}

static PyObject *
cell_as_str(CellObject *self)
{
    return cell_to_str(self->cell);
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
 * Row methods
 */

static int
row_clear(RowObject *self)
{
    Py_CLEAR(self->reader);
    return 0;
}


static void
row_dealloc(RowObject *self)
{
    self->row = NULL;
    Py_CLEAR(self->reader);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static int
row_traverse(RowObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->reader);
    return 0;
}


static PyObject *
row_new(ReaderObject *reader)
{
    RowObject *self = (RowObject *) PyObject_GC_New(RowObject, &RowType);
    if(self) {
        Py_INCREF(reader);
        self->reader = reader;
    }
    self->row = reader->row;
    PyObject_GC_Track((PyObject *) self);
    return (PyObject *) self;
}


static PyObject *
row_astuple(RowObject *self)
{
    PyObject *tup = PyTuple_New(self->row->count);
    if(tup) {
        int count = self->row->count;
        CsvCell *cell = &self->row->cells[0];

        for(int i = 0; i < count; i++, cell++) {
            PyObject *s = PyString_FromStringAndSize(cell->ptr, cell->size);
            if(! s) {
                Py_CLEAR(tup);
                break;
            }
            PyTuple_SET_ITEM(tup, i, s);
        }
    }

    return tup;
}


static PyObject *
row_asdict(RowObject *self)
{
    PyObject *out = PyDict_New();
    if(out) {
        Py_ssize_t ppos = 0;
        PyObject *key;
        PyObject *value;

        CsvCell *cells = &self->row->cells[0];
        while(PyDict_Next(self->reader->header_map, &ppos, &key, &value)) {
            int i = PyInt_AS_LONG(value);
            if(i < self->row->count) {
                PyObject *s = PyString_FromStringAndSize(
                    cells[i].ptr, cells[i].size);
                if(! s) {
                    Py_CLEAR(out);
                    break;
                }

                if(PyDict_SetItem(out, key, s)) {
                    Py_DECREF(s);
                    Py_CLEAR(out);
                    break;
                }

                Py_DECREF(s);
            }
        }
    }

    return out;
}


static PyObject *
row_return_self(RowObject *self)
{
    Py_INCREF(self);
    return (PyObject *) self;
}


static PyObject *
row_repr(RowObject *self)
{
    PyObject *obj;

    if(self->reader->header) {
        obj = row_asdict(self);
    } else {
        obj = row_astuple(self);
    }

    if(! obj) {
        return NULL;
    }

    PyObject *obj_repr = PyObject_Repr(obj);
    Py_DECREF(obj);
    if(! obj_repr) {
        return NULL;
    }

    PyObject *repr = PyString_FromFormat(
        "<csvmonkey._Row positioned at %s>",
        PyString_AsString(obj_repr)
    );
    Py_DECREF(obj_repr);
    return repr;
}


static Py_ssize_t
row_length(RowObject *self)
{
    return self->row->count;
}


static PyObject *
row_getitem(RowObject *self, Py_ssize_t index)
{
    if(index < 0) {
        index = self->row->count + index;
    }

    if(index < 0 || index > self->row->count) {
        PyErr_Format(PyExc_IndexError,
                     "index %ld greater than parsed col count %lu",
                     (unsigned long) index,
                     (unsigned long) self->row->count);
        return NULL;
    }

    CsvCell &cell = self->row->cells[index];
    return PyString_FromStringAndSize(cell.ptr, cell.size);
}


static Py_ssize_t
row_getlength(RowObject *self)
{
    return (Py_ssize_t) self->row->count;
}


static PyObject *
row_subscript(RowObject *self, PyObject *key)
{
    int index;

    if(PyInt_CheckExact(key)) {
        index = (int) PyInt_AS_LONG(key);
        if(index < 0) {
            index = self->row->count + index;
        }
    } else if(! self->reader->header_map) {
        PyErr_Format(PyExc_IndexError, "Reader instantiated with header=False");
        return NULL;
    } else {
        PyObject *py_index = PyDict_GetItem(self->reader->header_map, key);
        if(! py_index) {
            PyErr_Format(PyExc_KeyError, "No such key.");
            return NULL;
        }
        index = (int) PyInt_AS_LONG(py_index);
    }

    if(index < 0 || index > self->row->count) {
        PyErr_Format(PyExc_IndexError,
                     "index %ld greater than parsed col count %lu",
                     (unsigned long) index,
                     (unsigned long) self->row->count);
        return NULL;
    }

    CsvCell *cell = &self->row->cells[index];
    return PyString_FromStringAndSize(cell->ptr, cell->size);
}


static PyObject *
row_iter(RowObject *self)
{
    return NULL; /* TODO */
    /*
    if(self->reader->header) {
        return PyObject_CallMethodObjArgs(
            self->reader->header_map,
            "iteritems",
            NULL
        );
    }

    Py_INCREF(self);
    return self;
    */
}


/*
 * Reader methods
 */

static int
reader_clear(ReaderObject *self)
{
    Py_CLEAR(self->py_row);
    Py_CLEAR(self->header_map);
    return 0;
}


static void
reader_dealloc(ReaderObject *self)
{
    reader_clear(self);
    self->reader.~CsvReader();
    switch(self->cursor_type) {
    case CURSOR_MAPPED_FILE:
        delete (MappedFileCursor *)self->cursor;
        break;
    case CURSOR_ITERATOR:
        delete (IteratorStreamCursor *)self->cursor;
        break;
    case CURSOR_PYTHON_FILE:
        delete (FileStreamCursor *)self->cursor;
        break;
    default:
        assert(0);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static int
reader_traverse(ReaderObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->py_row);
    return 0;
}


static int
header_from_first_row(ReaderObject *self)
{
    if(! self->reader.read_row()) {
        if(! PyErr_Occurred()) {
            PyErr_Format(PyExc_IOError, "Could not read header row");
        }
        return -1;
    }

    self->header_map = PyDict_New();
    if(! self->header_map) {
        return -1;
    }

    CsvCell *cell = &self->row->cells[0];
    for(int i = 0; i < self->row->count; i++) {
        PyObject *key = PyString_FromStringAndSize(cell->ptr, cell->size);
        PyObject *value = PyInt_FromLong(i);
        assert(key && value);
        PyDict_SetItem(self->header_map, key, value);
        Py_DECREF(key);
        Py_DECREF(value);
        cell++;
    }

    return 0;
}


static int
header_from_sequence(ReaderObject *self, PyObject *header)
{
    self->header_map = PyDict_New();
    if(! self->header_map) {
        return -1;
    }

    Py_ssize_t length = PySequence_Length(header);
    for(Py_ssize_t i = 0; i < length; i++) {
        PyObject *key = PySequence_GetItem(header, i);
        if(! key) {
            return -1;
        }

        PyObject *value = PyInt_FromLong(i);
        if(! value) {
            return -1;
        }

        PyDict_SetItem(self->header_map, key, value);
        Py_DECREF(key);
        Py_DECREF(value);
    }

    return 0;
}


static PyObject *
finish_init(ReaderObject *self, const char *yields, PyObject *header,
            char delimiter, char quotechar, char escapechar,
            bool yield_incomplete_row)
{
    if(! strcmp(yields, "dict")) {
        self->yields = row_asdict;
    } else if(! strcmp(yields, "tuple")) {
        self->yields = row_astuple;
    } else {
        self->yields = row_return_self;
    }

    self->header = header && PyObject_IsTrue(header);
    new (&(self->reader)) CsvReader(*self->cursor, delimiter, quotechar, escapechar,
                                    yield_incomplete_row);
    self->row = &self->reader.row();
    self->py_row = row_new(self);

    if(self->header) {
        int rc;
        if(PySequence_Check(header)) {
            rc = header_from_sequence(self, header);
        } else {
            rc = header_from_first_row(self);
        }

        if(rc) {
            Py_DECREF((PyObject *) self);
            return NULL;
        }
    } else {
        self->header_map = NULL;
    }

    PyObject_GC_Track((PyObject *) self);
    return (PyObject *) self;
}


static PyObject *
reader_from_path(PyObject *_self, PyObject *args, PyObject *kw)
{
    static char *keywords[] = {"path", "yields", "header", "delimiter",
        "quotechar", "escapechar", "yield_incomplete_row"};
    const char *path;
    const char *yields = "row";
    PyObject *header = NULL;
    char delimiter = ',';
    char quotechar = '"';
    char escapechar = 0;
    int yield_incomplete_row = 0;

    if(! PyArg_ParseTupleAndKeywords(args, kw, "s|sOccci:from_path", keywords,
            &path, &yields, &header, &delimiter, &quotechar, &escapechar,
            &yield_incomplete_row)) {
        return NULL;
    }

    ReaderObject *self = PyObject_GC_New(ReaderObject, &ReaderType);
    if(! self) {
        return NULL;
    }

    self->py_row = NULL;
    self->header_map = NULL;

    MappedFileCursor *cursor = new MappedFileCursor();
    try {
        cursor->open(path);
    } catch(csvmonkey::Error &e) {
        delete cursor;
        PyErr_Format(PyExc_IOError, "%s: %s", path, e.what());
        Py_DECREF(self);
        return NULL;
    }

    self->cursor = cursor;
    self->cursor_type = CURSOR_MAPPED_FILE;
    return finish_init(self, yields, header, delimiter, quotechar, escapechar,
                       yield_incomplete_row);
}


static PyObject *
reader_from_iter(PyObject *_self, PyObject *args, PyObject *kw)
{
    static char *keywords[] = {"iter", "yields", "header",
        "delimiter", "quotechar", "escapechar", "yield_incomplete_row"};
    PyObject *iterable;
    const char *yields = "row";
    PyObject *header = NULL;
    char delimiter = ',';
    char quotechar = '"';
    char escapechar = 0;
    int yield_incomplete_row = 0;

    if(! PyArg_ParseTupleAndKeywords(args, kw, "O|sOccci:from_iter", keywords,
            &iterable, &yields, &header, &delimiter, &quotechar, &escapechar,
            &yield_incomplete_row)) {
        return NULL;
    }

    PyObject *iter = PyObject_GetIter(iterable);
    if(! iter) {
        return NULL;
    }

    ReaderObject *self = PyObject_GC_New(ReaderObject, &ReaderType);
    if(! self) {
        Py_DECREF(iter);
        return NULL;
    }

    self->py_row = NULL;
    self->header_map = NULL;
    self->cursor = new IteratorStreamCursor(iter);
    self->cursor_type = CURSOR_ITERATOR;
    return finish_init(self, yields, header, delimiter, quotechar, escapechar,
                       yield_incomplete_row);
}


static PyObject *
reader_from_file(PyObject *_self, PyObject *args, PyObject *kw)
{
    static char *keywords[] = {"fp", "yields", "header",
        "delimiter", "quotechar", "escapechar", "yield_incomplete_row"};
    PyObject *fp;
    const char *yields = "row";
    PyObject *header = NULL;
    char delimiter = ',';
    char quotechar = '"';
    char escapechar = 0;
    int yield_incomplete_row = 0;

    if(! PyArg_ParseTupleAndKeywords(args, kw, "O|sOccci:from_file", keywords,
            &fp, &yields, &header, &delimiter, &quotechar, &escapechar,
            &yield_incomplete_row)) {
        return NULL;
    }

    PyObject *py_read = PyObject_GetAttrString(fp, "read");
    if(! py_read) {
        CSM_DEBUG("py_read is null");
        return NULL;
    }

    ReaderObject *self = PyObject_GC_New(ReaderObject, &ReaderType);
    if(! self) {
        Py_DECREF(py_read);
        return NULL;
    }

    self->cursor = new FileStreamCursor(py_read);
    self->cursor_type = CURSOR_PYTHON_FILE;
    return finish_init(self, yields, header, delimiter, quotechar, escapechar,
                       yield_incomplete_row);
}


static PyObject *
reader_get_header(ReaderObject *self, PyObject *args)
{
    if(! self->header_map) {
        return PyList_New(0);
    }

    PyObject *lst = PyList_New(PyDict_Size(self->header_map));
    Py_ssize_t ppos = 0;
    PyObject *key;
    PyObject *value;

    while(PyDict_Next(self->header_map, &ppos, &key, &value)) {
        int i = PyInt_AS_LONG(value);
        PyList_SET_ITEM(lst, i, key);
        Py_INCREF(key);
    }

    return lst;
}


static PyObject *
reader_find_cell(ReaderObject *self, PyObject *args)
{
    const char *s;
    if(! PyArg_ParseTuple(args, "s:find_cell", &s)) {
        return NULL;
    }

    CsvCell *cell;
    if(! self->row->by_value(s, cell)) {
        PyErr_Format(PyExc_KeyError, "%s", s);
        return NULL;
    }

    return cell_new(self, cell);
}


static PyObject *
reader_repr(ReaderObject *self)
{
    return PyString_FromFormat(
        "<csvmonkey._Reader positioned at %d>",
        0
    );
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
    if(self->reader.read_row()) {
        return self->yields((RowObject *) self->py_row);
    }

    if(self->cursor->size() && !self->reader.in_newline_skip) {
        PyErr_Format(PyExc_IOError,
            "%lu unparsed bytes at end of input. The input may be missing a "
            "final newline, or unbalanced quotes are present.",
            (unsigned long) self->cursor->size()
        );
    }
    if(! PyErr_Occurred()) {
        PyErr_SetNone(PyExc_StopIteration);
    }
    return NULL;
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
    "csvmonkey._Cell",           /*tp_doc*/
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
 * Row type.
 */

static PySequenceMethods row_sequence_methods = {
    (lenfunc) row_getlength,  /* sq_length */
    NULL,                        /* sq_concat */
    NULL,                        /* sq_repeat */
    (ssizeargfunc) row_getitem,  /* sq_item */
};


static PyMappingMethods row_mapping_methods = {
    (lenfunc) row_getlength,      /* mp_length */
    (binaryfunc) row_subscript,   /* mp_subscript */
    0,                               /* mp_ass_subscript */
};

static PyMethodDef row_methods[] = {
    {"astuple",     (PyCFunction)row_astuple, METH_NOARGS, ""},
    {"asdict",     (PyCFunction)row_asdict, METH_NOARGS, ""},
    {0, 0, 0, 0}
};

PyTypeObject RowType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_Row",                     /*tp_name*/
    sizeof(RowObject),          /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor) row_dealloc,   /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    (reprfunc)row_repr,         /*tp_repr*/
    0,                          /*tp_as_number*/
    &row_sequence_methods,      /*tp_as_sequence*/
    &row_mapping_methods,       /*tp_as_mapping*/
    0,                          /*tp_hash*/
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_GC,         /*tp_flags*/
    "csvmonkey._Row",            /*tp_doc*/
    (traverseproc)row_traverse, /*tp_traverse*/
    (inquiry)row_clear,         /*tp_clear*/
    0,                          /*tp_richcompare*/
    0,                          /*tp_weaklistoffset*/
    (getiterfunc) row_iter,     /*tp_iter*/
    0,                          /*tp_iternext*/
    row_methods,                /*tp_methods*/
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
 * Reader type.
 */

static PyMethodDef reader_methods[] = {
    {"get_header",  (PyCFunction)reader_get_header, METH_NOARGS, ""},
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
    (reprfunc)reader_repr,      /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash*/
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_GC,         /*tp_flags*/
    "csvmonkey._Reader",         /*tp_doc*/
    (traverseproc)reader_traverse, /*tp_traverse*/
    (inquiry)reader_clear,      /*tp_clear*/
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
    {"from_path", (PyCFunction) reader_from_path, METH_VARARGS|METH_KEYWORDS},
    {"from_iter", (PyCFunction) reader_from_iter, METH_VARARGS|METH_KEYWORDS},
    {"from_file", (PyCFunction) reader_from_file, METH_VARARGS|METH_KEYWORDS},
    {0, 0, 0, 0}
};


PyMODINIT_FUNC
initcsvmonkey(void)
{
    static PyTypeObject *types[] = {
        &CellType, &RowType, &ReaderType
    };

    PyObject *mod = Py_InitModule3("csvmonkey", module_methods, "");
    if(! mod) {
        return;
    }

    for(int i = 0; i < (sizeof types / sizeof types[0]); i++) {
        PyTypeObject *type = types[i];
        if(PyType_Ready(type)) {
            return;
        }
        if(PyModule_AddObject(mod, type->tp_name, (PyObject *)type)) {
            return;
        }
    }
}
