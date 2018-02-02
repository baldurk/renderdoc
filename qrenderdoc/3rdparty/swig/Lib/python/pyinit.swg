/* ------------------------------------------------------------
 * The start of the Python initialization function 
 * ------------------------------------------------------------ */

%insert(init) "swiginit.swg"

#if defined(SWIGPYTHON_BUILTIN)
%fragment("<stddef.h>"); // For offsetof
#endif

%init %{

#ifdef __cplusplus
extern "C" {
#endif

/* Python-specific SWIG API */
#define SWIG_newvarlink()                             SWIG_Python_newvarlink()
#define SWIG_addvarlink(p, name, get_attr, set_attr)  SWIG_Python_addvarlink(p, name, get_attr, set_attr)
#define SWIG_InstallConstants(d, constants)           SWIG_Python_InstallConstants(d, constants)
 
/* -----------------------------------------------------------------------------
 * global variable support code.
 * ----------------------------------------------------------------------------- */
 
typedef struct swig_globalvar {   
  char       *name;                  /* Name of global variable */
  PyObject *(*get_attr)(void);       /* Return the current value */
  int       (*set_attr)(PyObject *); /* Set the value */
  struct swig_globalvar *next;
} swig_globalvar;

typedef struct swig_varlinkobject {
  PyObject_HEAD
  swig_globalvar *vars;
} swig_varlinkobject;

SWIGINTERN PyObject *
swig_varlink_repr(swig_varlinkobject *SWIGUNUSEDPARM(v)) {
#if PY_VERSION_HEX >= 0x03000000
  return PyUnicode_InternFromString("<Swig global variables>");
#else
  return PyString_FromString("<Swig global variables>");
#endif
}

SWIGINTERN PyObject *
swig_varlink_str(swig_varlinkobject *v) {
#if PY_VERSION_HEX >= 0x03000000
  PyObject *str = PyUnicode_InternFromString("(");
  PyObject *tail;
  PyObject *joined;
  swig_globalvar *var;
  for (var = v->vars; var; var=var->next) {
    tail = PyUnicode_FromString(var->name);
    joined = PyUnicode_Concat(str, tail);
    Py_DecRef(str);
    Py_DecRef(tail);
    str = joined;
    if (var->next) {
        tail = PyUnicode_InternFromString(", ");
        joined = PyUnicode_Concat(str, tail);
        Py_DecRef(str);
        Py_DecRef(tail);
        str = joined;
    }
  }
  tail = PyUnicode_InternFromString(")");
  joined = PyUnicode_Concat(str, tail);
  Py_DecRef(str);
  Py_DecRef(tail);
  str = joined;
#else
  PyObject *str = PyString_FromString("(");
  swig_globalvar *var;
  for (var = v->vars; var; var=var->next) {
    PyString_ConcatAndDel(&str,PyString_FromString(var->name));
    if (var->next) PyString_ConcatAndDel(&str,PyString_FromString(", "));
  }
  PyString_ConcatAndDel(&str,PyString_FromString(")"));
#endif
  return str;
}

SWIGINTERN int
swig_varlink_print(swig_varlinkobject *v, FILE *fp, int SWIGUNUSEDPARM(flags)) {
  char *tmp;
  PyObject *str = swig_varlink_str(v);
  fprintf(fp,"Swig global variables ");
  fprintf(fp,"%s\n", tmp = SWIG_Python_str_AsChar(str));
  SWIG_Python_str_DelForPy3(tmp);
  Py_DECREF(str);
  return 0;
}

SWIGINTERN void
swig_varlink_dealloc(swig_varlinkobject *v) {
  swig_globalvar *var = v->vars;
  while (var) {
    swig_globalvar *n = var->next;
    free(var->name);
    free(var);
    var = n;
  }
}

SWIGINTERN PyObject *
swig_varlink_getattr(swig_varlinkobject *v, char *n) {
  PyObject *res = NULL;
  swig_globalvar *var = v->vars;
  while (var) {
    if (strcmp(var->name,n) == 0) {
      res = (*var->get_attr)();
      break;
    }
    var = var->next;
  }
  if (res == NULL && !PyErr_Occurred()) {
    PyErr_Format(PyExc_AttributeError, "Unknown C global variable '%s'", n);
  }
  return res;
}

SWIGINTERN int
swig_varlink_setattr(swig_varlinkobject *v, char *n, PyObject *p) {
  int res = 1;
  swig_globalvar *var = v->vars;
  while (var) {
    if (strcmp(var->name,n) == 0) {
      res = (*var->set_attr)(p);
      break;
    }
    var = var->next;
  }
  if (res == 1 && !PyErr_Occurred()) {
    PyErr_Format(PyExc_AttributeError, "Unknown C global variable '%s'", n);
  }
  return res;
}

SWIGINTERN PyTypeObject*
swig_varlink_type(void) {
  static char varlink__doc__[] = "Swig var link object";
  static PyTypeObject varlink_type;
  static int type_init = 0;
  if (!type_init) {
    const PyTypeObject tmp = {
#if PY_VERSION_HEX >= 0x03000000
      PyVarObject_HEAD_INIT(NULL, 0)
#else
      PyObject_HEAD_INIT(NULL)
      0,                                  /* ob_size */
#endif
      (char *)"swigvarlink",              /* tp_name */
      sizeof(swig_varlinkobject),         /* tp_basicsize */
      0,                                  /* tp_itemsize */
      (destructor) swig_varlink_dealloc,  /* tp_dealloc */
      (printfunc) swig_varlink_print,     /* tp_print */
      (getattrfunc) swig_varlink_getattr, /* tp_getattr */
      (setattrfunc) swig_varlink_setattr, /* tp_setattr */
      0,                                  /* tp_compare */
      (reprfunc) swig_varlink_repr,       /* tp_repr */
      0,                                  /* tp_as_number */
      0,                                  /* tp_as_sequence */
      0,                                  /* tp_as_mapping */
      0,                                  /* tp_hash */
      0,                                  /* tp_call */
      (reprfunc) swig_varlink_str,        /* tp_str */
      0,                                  /* tp_getattro */
      0,                                  /* tp_setattro */
      0,                                  /* tp_as_buffer */
      0,                                  /* tp_flags */
      varlink__doc__,                     /* tp_doc */
      0,                                  /* tp_traverse */
      0,                                  /* tp_clear */
      0,                                  /* tp_richcompare */
      0,                                  /* tp_weaklistoffset */
#if PY_VERSION_HEX >= 0x02020000
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* tp_iter -> tp_weaklist */
#endif
#if PY_VERSION_HEX >= 0x02030000
      0,                                  /* tp_del */
#endif
#if PY_VERSION_HEX >= 0x02060000
      0,                                  /* tp_version_tag */
#endif
#if PY_VERSION_HEX >= 0x03040000
      0,                                  /* tp_finalize */
#endif
#ifdef COUNT_ALLOCS
      0,                                  /* tp_allocs */
      0,                                  /* tp_frees */
      0,                                  /* tp_maxalloc */
#if PY_VERSION_HEX >= 0x02050000
      0,                                  /* tp_prev */
#endif
      0                                   /* tp_next */
#endif
    };
    varlink_type = tmp;
    type_init = 1;
#if PY_VERSION_HEX < 0x02020000
    varlink_type.ob_type = &PyType_Type;
#else
    if (PyType_Ready(&varlink_type) < 0)
      return NULL;
#endif
  }
  return &varlink_type;
}

/* Create a variable linking object for use later */
SWIGINTERN PyObject *
SWIG_Python_newvarlink(void) {
  swig_varlinkobject *result = PyObject_NEW(swig_varlinkobject, swig_varlink_type());
  if (result) {
    result->vars = 0;
  }
  return ((PyObject*) result);
}

SWIGINTERN void 
SWIG_Python_addvarlink(PyObject *p, char *name, PyObject *(*get_attr)(void), int (*set_attr)(PyObject *p)) {
  swig_varlinkobject *v = (swig_varlinkobject *) p;
  swig_globalvar *gv = (swig_globalvar *) malloc(sizeof(swig_globalvar));
  if (gv) {
    size_t size = strlen(name)+1;
    gv->name = (char *)malloc(size);
    if (gv->name) {
      strncpy(gv->name,name,size);
      gv->get_attr = get_attr;
      gv->set_attr = set_attr;
      gv->next = v->vars;
    }
  }
  v->vars = gv;
}

SWIGINTERN PyObject *
SWIG_globals(void) {
  static PyObject *_SWIG_globals = 0; 
  if (!_SWIG_globals) _SWIG_globals = SWIG_newvarlink();  
  return _SWIG_globals;
}

/* -----------------------------------------------------------------------------
 * constants/methods manipulation
 * ----------------------------------------------------------------------------- */

/* Install Constants */
SWIGINTERN void
SWIG_Python_InstallConstants(PyObject *d, swig_const_info constants[]) {
  PyObject *obj = 0;
  size_t i;
  for (i = 0; constants[i].type; ++i) {
    switch(constants[i].type) {
    case SWIG_PY_POINTER:
      obj = SWIG_InternalNewPointerObj(constants[i].pvalue, *(constants[i]).ptype,0);
      break;
    case SWIG_PY_BINARY:
      obj = SWIG_NewPackedObj(constants[i].pvalue, constants[i].lvalue, *(constants[i].ptype));
      break;
    default:
      obj = 0;
      break;
    }
    if (obj) {
      PyDict_SetItemString(d, constants[i].name, obj);
      Py_DECREF(obj);
    }
  }
}

/* -----------------------------------------------------------------------------*/
/* Fix SwigMethods to carry the callback ptrs when needed */
/* -----------------------------------------------------------------------------*/

SWIGINTERN void
SWIG_Python_FixMethods(PyMethodDef *methods,
		       swig_const_info *const_table,
		       swig_type_info **types,
		       swig_type_info **types_initial) {
  size_t i;
  for (i = 0; methods[i].ml_name; ++i) {
    const char *c = methods[i].ml_doc;
    if (!c) continue;
    c = strstr(c, "swig_ptr: ");
    if (c) {
      int j;
      swig_const_info *ci = 0;
      const char *name = c + 10;
      for (j = 0; const_table[j].type; ++j) {
	if (strncmp(const_table[j].name, name, 
		    strlen(const_table[j].name)) == 0) {
	  ci = &(const_table[j]);
	  break;
	}
      }
      if (ci) {
	void *ptr = (ci->type == SWIG_PY_POINTER) ? ci->pvalue : 0;
	if (ptr) {
	  size_t shift = (ci->ptype) - types;
	  swig_type_info *ty = types_initial[shift];
	  size_t ldoc = (c - methods[i].ml_doc);
	  size_t lptr = strlen(ty->name)+2*sizeof(void*)+2;
	  char *ndoc = (char*)malloc(ldoc + lptr + 10);
	  if (ndoc) {
	    char *buff = ndoc;
	    strncpy(buff, methods[i].ml_doc, ldoc);
	    buff += ldoc;
	    strncpy(buff, "swig_ptr: ", 10);
	    buff += 10;
	    SWIG_PackVoidPtr(buff, ptr, ty->name, lptr);
	    methods[i].ml_doc = ndoc;
	  }
	}
      }
    }
  }
} 

#ifdef __cplusplus
}
#endif

/* -----------------------------------------------------------------------------*
 *  Partial Init method
 * -----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C"
#endif

SWIGEXPORT 
#if PY_VERSION_HEX >= 0x03000000
  PyObject*
#else
  void
#endif
SWIG_init(void) {
  PyObject *m, *d, *md;
#if PY_VERSION_HEX >= 0x03000000
  static struct PyModuleDef SWIG_module = {
# if PY_VERSION_HEX >= 0x03020000
    PyModuleDef_HEAD_INIT,
# else
    {
      PyObject_HEAD_INIT(NULL)
      NULL, /* m_init */
      0,    /* m_index */
      NULL, /* m_copy */
    },
# endif
    (char *) SWIG_name,
    NULL,
    -1,
    SwigMethods,
    NULL,
    NULL,
    NULL,
    NULL
  };
#endif

#if defined(SWIGPYTHON_BUILTIN)
  static SwigPyClientData SwigPyObject_clientdata = {0, 0, 0, 0, 0, 0, 0};
  static PyGetSetDef this_getset_def = {
    (char *)"this", &SwigPyBuiltin_ThisClosure, NULL, NULL, NULL
  };
  static SwigPyGetSet thisown_getset_closure = {
    (PyCFunction) SwigPyObject_own,
    (PyCFunction) SwigPyObject_own
  };
  static PyGetSetDef thisown_getset_def = {
    (char *)"thisown", SwigPyBuiltin_GetterClosure, SwigPyBuiltin_SetterClosure, NULL, &thisown_getset_closure
  };
  PyTypeObject *builtin_pytype;
  int builtin_base_count;
  swig_type_info *builtin_basetype;
  PyObject *tuple;
  PyGetSetDescrObject *static_getset;
  PyTypeObject *metatype;
  PyTypeObject *swigpyobject;
  SwigPyClientData *cd;
  PyObject *public_interface, *public_symbol;
  PyObject *this_descr;
  PyObject *thisown_descr;
  PyObject *self = 0;
  int i;
  PyObject *enum_module;
  PyObject *Enum_class;
  PyObject *Flag_class;

  (void)builtin_pytype;
  (void)builtin_base_count;
  (void)builtin_basetype;
  (void)tuple;
  (void)static_getset;
  (void)self;

  /* Metaclass is used to implement static member variables */
  metatype = SwigPyObjectType();
  assert(metatype);
#endif

  /* Fix SwigMethods to carry the callback ptrs when needed */
  SWIG_Python_FixMethods(SwigMethods, swig_const_table, swig_types, swig_type_initial);

#if PY_VERSION_HEX >= 0x03000000
  m = PyModule_Create(&SWIG_module);
#else
  m = Py_InitModule((char *) SWIG_name, SwigMethods);
#endif

  /* Try to find strong enum module */
  Enum_class = NULL;
  Flag_class = NULL;
#if PY_VERSION_HEX >= 0x03000000
  enum_module = PyImport_ImportModule("enum");

  if (enum_module) {
    Enum_class = PyObject_GetAttrString(enum_module, "IntEnum");
    Flag_class = PyObject_GetAttrString(enum_module, "IntFlag");
  }
#endif

  md = d = PyModule_GetDict(m);
  (void)md;

  SWIG_InitializeModule(0);

#ifdef SWIGPYTHON_BUILTIN
  swigpyobject = SwigPyObject_TypeOnce();

  SwigPyObject_stype = SWIG_MangledTypeQuery("_p_SwigPyObject");
  assert(SwigPyObject_stype);
  cd = (SwigPyClientData*) SwigPyObject_stype->clientdata;
  if (!cd) {
    SwigPyObject_stype->clientdata = &SwigPyObject_clientdata;
    SwigPyObject_clientdata.pytype = swigpyobject;
  } else if (swigpyobject->tp_basicsize != cd->pytype->tp_basicsize) {
    PyErr_SetString(PyExc_RuntimeError, "Import error: attempted to load two incompatible swig-generated modules.");
# if PY_VERSION_HEX >= 0x03000000
    return NULL;
# else
    return;
# endif
  }

  /* All objects have a 'this' attribute */
  this_descr = PyDescr_NewGetSet(SwigPyObject_type(), &this_getset_def);
  (void)this_descr;

  /* All objects have a 'thisown' attribute */
  thisown_descr = PyDescr_NewGetSet(SwigPyObject_type(), &thisown_getset_def);
  (void)thisown_descr;

  public_interface = PyList_New(0);
  public_symbol = 0;
  (void)public_symbol;

  PyDict_SetItemString(md, "__all__", public_interface);
  Py_DECREF(public_interface);
  for (i = 0; SwigMethods[i].ml_name != NULL; ++i)
    SwigPyBuiltin_AddPublicSymbol(public_interface, SwigMethods[i].ml_name);
  for (i = 0; swig_const_table[i].name != 0; ++i)
    SwigPyBuiltin_AddPublicSymbol(public_interface, swig_const_table[i].name);
#endif

  SWIG_InstallConstants(d,swig_const_table);
%}

