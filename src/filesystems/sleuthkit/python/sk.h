/* Sleuthkit python module */

#include <Python.h>
#include "structmember.h"

#include "list.h"
#include "talloc.h"
#include "fs_tools.h"

/******************************************************************
 * Helpers and SK integration stuff
 * ***************************************************************/

/* used in walks to determine what to return */
#define SK_FLAG_INODES	0x1	// inodes in result
#define SK_FLAG_NAMES	0x2	// names in result

/* stores the major elements of FS_DENT */
struct dentwalk {
    char *path;
    INUM_T inode;
    uint8_t ent_type;
    uint32_t type;
    uint32_t id;
    char alloc;
    struct list_head list;
};

/* implements an sk img subsystem */
typedef struct {
	IMG_INFO img_info;
    PyObject *fileobj;
} IMG_PYFILE_INFO;

/* tracks block lists */
struct block {
    DADDR_T addr;
    int size;
    struct list_head list;
};

/* callback functions for dent_walk, populate a file list */
static uint8_t
listdent_walk_callback_dent(FS_INFO *fs, FS_DENT *fs_dent, int flags, void *ptr);
static uint8_t
listdent_walk_callback_list(FS_INFO *fs, FS_DENT *fs_dent, int flags, void *ptr);

/* callback function for file_walk, populates a block list */
static u_int8_t
getblocks_walk_callback(FS_INFO *fs, DADDR_T addr, char *buf, int size, int flags, void *ptr);

/* lookup an inode from a path */
INUM_T lookup_inode(FS_INFO *fs, char *path);
int lookup_path(FS_INFO *fs, struct dentwalk *dent);

/******************************************************************
 * SKFS - Sleuthkit Filesystem Python Type
 * ***************************************************************/

typedef struct {
    PyObject_HEAD
    // The talloc context that everything is hanged from:
    void *context;
	IMG_INFO *img;
	FS_INFO *fs;
    PyObject *root_inum;
} skfs;

static void skfs_dealloc(skfs *self);
static int skfs_init(skfs *self, PyObject *args, PyObject *kwds);
static PyObject *skfs_listdir(skfs *self, PyObject *args, PyObject *kwds);
static PyObject *skfs_open(skfs *self, PyObject *args, PyObject *kwds);
static PyObject *skfs_walk(skfs *self, PyObject *args, PyObject *kwds);
static PyObject *skfs_iwalk(skfs *self, PyObject *args, PyObject *kwds);
static PyObject *skfs_stat(skfs *self, PyObject *args, PyObject *kwds);
static PyObject *skfs_fstat(skfs *self, PyObject *args);

static PyMemberDef skfs_members[] = {
    {"root_inum", T_OBJECT, offsetof(skfs, root_inum), 0,
     "root inode"},
    {NULL}  /* Sentinel */
};

static PyMethodDef skfs_methods[] = {
    {"listdir", (PyCFunction)skfs_listdir, METH_VARARGS|METH_KEYWORDS,
     "List directory contents" },
    {"open", (PyCFunction)skfs_open, METH_VARARGS|METH_KEYWORDS,
     "Open a file" },
    {"walk", (PyCFunction)skfs_walk, METH_VARARGS|METH_KEYWORDS,
     "Walk filesystem from the given path" },
    {"iwalk", (PyCFunction)skfs_iwalk, METH_VARARGS|METH_KEYWORDS,
     "Walk inodes" },
    {"stat", (PyCFunction)skfs_stat, METH_VARARGS|METH_KEYWORDS,
     "Stat a file path" },
    {"fstat", (PyCFunction)skfs_fstat, METH_VARARGS,
     "Stat a skfile" },
    {NULL}  /* Sentinel */
};

static PyTypeObject skfsType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "sk.skfs",                 /* tp_name */
    sizeof(skfs),              /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)skfs_dealloc,  /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    "Sleuthkit Filesystem Object",     /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    skfs_methods,              /* tp_methods */
    skfs_members,              /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)skfs_init,       /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

/******************************************************************
 * SKFSWalkIter - Support the skfs.walk iterator
 * ***************************************************************/

typedef struct {
    PyObject_HEAD
    skfs *skfs;
    struct dentwalk *walklist;
    int flags;
    int myflags;
    void *context;
} skfs_walkiter;

static void skfs_walkiter_dealloc(skfs_walkiter *self);
static int skfs_walkiter_init(skfs_walkiter *self, PyObject *args, PyObject *kwds);
static PyObject *skfs_walkiter_iter(skfs_walkiter *self);
static PyObject *skfs_walkiter_iternext(skfs_walkiter *self);

static PyTypeObject skfs_walkiterType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "sk.skfs_walkiter",        /* tp_name */
    sizeof(skfs_walkiter),     /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)skfs_walkiter_dealloc, /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    "Sleuthkit Filesystem Walk Iterator Object", /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    PyObject_SelfIter,         /* tp_iter */
    (iternextfunc)skfs_walkiter_iternext, /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)skfs_walkiter_init, /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

/******************************************************************
 * A very simple type to represent an inode
 * ***************************************************************/

typedef struct {
    PyObject_HEAD
    INUM_T inode;
	uint32_t type;
	uint32_t id;
    char alloc;
} skfs_inode;

static PyMemberDef skfs_inode_members[] = {
    {"type", T_INT, offsetof(skfs_inode, type), 0,
     "inode attribute type"},
    {"id", T_INT, offsetof(skfs_inode, id), 0,
     "inode attribute id"},
    {NULL}  /* Sentinel */
};

static int skfs_inode_init(skfs_inode *self, PyObject *args, PyObject *kwds);
static PyObject *skfs_inode_str(skfs_inode *self);
static PyObject *skfs_inode_long(skfs_inode *self);
static PyObject *skfs_inode_getinode(skfs_inode *self, void *closure);
static PyObject *skfs_inode_getalloc(skfs_inode *self, void *closure);

static PyGetSetDef skfs_inode_getseters[] = {
    {"inode", (getter)skfs_inode_getinode, NULL,
     "inode number", NULL},
    {"alloc", (getter)skfs_inode_getalloc, NULL,
     "allocation status", NULL},
    {NULL}  /* Sentinel */
};

static PyNumberMethods skfs_inode_as_number = {
	0,                          /*nb_add*/
	0,                          /*nb_subtract*/
	0,                          /*nb_multiply*/
	0,                          /*nb_divide*/
	0,                          /*nb_remainder*/
	0,                          /*nb_divmod*/
	0,                          /*nb_power*/
	0,                          /*nb_negative*/
	0,                          /*tp_positive*/
	0,                          /*tp_absolute*/
	0,                          /*tp_nonzero*/
	0,                          /*nb_invert*/
	0,                          /*nb_lshift*/
	0,                          /*nb_rshift*/
	0,                          /*nb_and*/
	0,                          /*nb_xor*/
	0,                          /*nb_or*/
	0,                          /*nb_coerce*/
	(unaryfunc)	skfs_inode_long,/*nb_int*/
	(unaryfunc)	skfs_inode_long,/*nb_long*/
};


static PyTypeObject skfs_inodeType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "sk.skfs_inode",           /* tp_name */
    sizeof(skfs_inode),        /* tp_basicsize */
    0,                         /* tp_itemsize */
    0,                         /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    &skfs_inode_as_number,     /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    (reprfunc)skfs_inode_str,  /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    "Sleuthkit Inode Object",  /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    skfs_inode_members,        /* tp_members */
    skfs_inode_getseters,      /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)skfs_inode_init, /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};


/******************************************************************
 * SKFILE - Sleuthkit File Python Type
 * ***************************************************************/

typedef struct {
    PyObject_HEAD
    void *context;
    PyObject *skfs;
    FS_INODE *fs_inode;
	uint32_t type;
	uint32_t id;
    struct block *blocks;
    char *resdata;
    long long readptr;
    long long size;
} skfile;

static void skfile_dealloc(skfile *self);
static int skfile_init(skfile *self, PyObject *args, PyObject *kwds);
static PyObject *skfile_str(skfile *self);
static PyObject *skfile_read(skfile *self, PyObject *args);
static PyObject *skfile_seek(skfile *self, PyObject *args);
static PyObject *skfile_tell(skfile *self);
static PyObject *skfile_blocks(skfile *self);

static PyMethodDef skfile_methods[] = {
    {"read", (PyCFunction)skfile_read, METH_VARARGS,
     "Read data from file" },
    {"seek", (PyCFunction)skfile_seek, METH_VARARGS,
     "Seek within a file" },
    {"tell", (PyCFunction)skfile_tell, METH_NOARGS,
     "Return possition within file" },
    {"blocks", (PyCFunction)skfile_blocks, METH_NOARGS,
     "Return a list of blocks which the file occupies" },
    {NULL}  /* Sentinel */
};

static PyTypeObject skfileType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "sk.skfile",               /* tp_name */
    sizeof(skfile),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)skfile_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    (reprfunc)skfile_str,      /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    "Sleuthkit File Object",   /* tp_doc */
    0,	                       /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    skfile_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)skfile_init,     /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};