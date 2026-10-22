/** \file EmbedPython.cpp
    \brief Helper functions for calling the embedded Python interpreter from st_graph.
    \author Tom Stephens, HEASARC/GSSC
*/
#include "EmbedPython.h"

#include <cstdarg>
#include <stdexcept>

namespace {
  using namespace st_graph;

  // Number of PythonFpeGuard objects currently in scope, and the floating point environment of the application
  // which was in effect when the outermost guard was created.
  int s_guard_depth = 0;
  std::fenv_t s_application_env;

  // Build an argument tuple from a Py_BuildValue format, making sure the result is always a tuple.
  PyObject * buildArgs(const char * format, va_list args) {
    PyObject * built = Py_VaBuildValue(format, args);
    if (0 == built) return 0;
    if (PyTuple_Check(built)) return built;
    PyObject * tuple = PyTuple_Pack(1, built);
    Py_DECREF(built);
    return tuple;
  }

  PyObject * callMethod(PyObject * obj, const char * method, const char * format, va_list args) {
    if (0 == obj) throw std::logic_error(std::string("st_graph: cannot call ") + method + " on a null Python object");
    PythonFpeGuard guard;
    PyRef func(PyObject_GetAttrString(obj, method));
    if (0 == func.get())
      throw std::runtime_error(std::string("st_graph: cannot find Python method ") + method + ": " + EP_ErrorMessage());
    PyRef py_args(buildArgs(format, args));
    if (0 == py_args.get())
      throw std::runtime_error(std::string("st_graph: cannot build arguments for ") + method + ": " + EP_ErrorMessage());
    PyObject * result = PyObject_CallObject(func.get(), py_args.get());
    if (0 == result)
      throw std::runtime_error(std::string("st_graph: Python call to ") + method + " failed: " + EP_ErrorMessage());
    return result;
  }
}

namespace st_graph {

  PythonFpeGuard::PythonFpeGuard() {
    if (0 == s_guard_depth) std::fegetenv(&s_application_env);
    ++s_guard_depth;
    // Save the current environment, clear the exception flags and switch to non-stop (no traps) mode.
    std::feholdexcept(&m_env);
  }

  PythonFpeGuard::~PythonFpeGuard() {
    // Discard exceptions raised by Python code, which are expected, before restoring traps; otherwise restoring
    // the environment could raise them.
    std::feclearexcept(FE_ALL_EXCEPT);
    std::fesetenv(&m_env);
    --s_guard_depth;
  }

  ApplicationFpeScope::ApplicationFpeScope(): m_env(), m_depth(s_guard_depth) {
    if (0 == m_depth) return;
    std::fegetenv(&m_env);
    // Nested guards created by application code must save the application's environment, not this one.
    s_guard_depth = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    std::fesetenv(&s_application_env);
  }

  ApplicationFpeScope::~ApplicationFpeScope() {
    if (0 == m_depth) return;
    std::feclearexcept(FE_ALL_EXCEPT);
    std::fesetenv(&m_env);
    s_guard_depth = m_depth;
  }

  void EP_Init() {
    PythonFpeGuard guard;
    if (!Py_IsInitialized()) {
      // Do not install Python's signal handlers; the host application owns signal handling.
      Py_InitializeEx(0);
    }
    // tkinter needs sys.argv, which is not set up when Python is embedded.
    if (0 == PySys_GetObject("argv")) {
      PyRef argv(Py_BuildValue("[s]", "st_graph"));
      if (0 != argv.get()) PySys_SetObject("argv", argv.get());
    }
  }

  PyObject * EP_Helper() {
    static PyObject * s_helper = 0;
    if (0 == s_helper) {
      EP_Init();
      PythonFpeGuard guard;
      s_helper = PyImport_ImportModule("st_graph_tk");
      if (0 == s_helper)
        throw std::runtime_error("st_graph: cannot import Python module st_graph_tk: " + EP_ErrorMessage());
    }
    return s_helper;
  }

  PyObject * EP_CallHelper(const char * function, const char * format, ...) {
    PyObject * helper = EP_Helper();
    va_list args;
    va_start(args, format);
    try {
      PyObject * result = callMethod(helper, function, format, args);
      va_end(args);
      return result;
    } catch (...) {
      va_end(args);
      throw;
    }
  }

  PyObject * EP_CallMethod(PyObject * obj, const char * method, const char * format, ...) {
    va_list args;
    va_start(args, format);
    try {
      PyObject * result = callMethod(obj, method, format, args);
      va_end(args);
      return result;
    } catch (...) {
      va_end(args);
      throw;
    }
  }

  void EP_CallMethodNoThrow(PyObject * obj, const char * method) {
    if (0 == obj || !Py_IsInitialized()) return;
    PythonFpeGuard guard;
    PyObject * result = PyObject_CallMethod(obj, method, 0);
    if (0 == result) PyErr_Clear();
    else Py_DECREF(result);
  }

  PyObject * EP_FromVector(const std::vector<double> & vec) {
    PyObject * list = PyList_New(vec.size());
    if (0 == list) throw std::runtime_error("st_graph: cannot create Python list: " + EP_ErrorMessage());
    for (std::vector<double>::size_type index = 0; index != vec.size(); ++index) {
      // PyList_SetItem steals the reference to the new float.
      PyList_SetItem(list, index, PyFloat_FromDouble(vec[index]));
    }
    return list;
  }

  std::string EP_AsString(PyObject * obj) {
    const char * text = (0 != obj && PyUnicode_Check(obj)) ? PyUnicode_AsUTF8AndSize(obj, 0) : 0;
    if (0 == text) {
      PyErr_Clear();
      throw std::runtime_error("st_graph: Python object is not a string");
    }
    return text;
  }

  std::string EP_ErrorMessage() {
    if (0 == PyErr_Occurred()) return "unknown error";
    PythonFpeGuard guard;
    PyObject * type = 0;
    PyObject * value = 0;
    PyObject * traceback = 0;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);
    std::string msg;
    if (0 != type) {
      PyRef name(PyObject_GetAttrString(type, "__name__"));
      if (0 != name.get() && PyUnicode_Check(name.get())) msg = PyUnicode_AsUTF8AndSize(name.get(), 0);
    }
    if (0 != value) {
      PyRef text(PyObject_Str(value));
      if (0 != text.get() && PyUnicode_Check(text.get()))
        msg += std::string(msg.empty() ? "" : ": ") + PyUnicode_AsUTF8AndSize(text.get(), 0);
    }
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    PyErr_Clear();
    return msg.empty() ? std::string("unknown error") : msg;
  }

}
