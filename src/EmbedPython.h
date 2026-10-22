/** \file EmbedPython.h
    \brief Helper functions for calling the embedded Python interpreter from st_graph.
    \author Tom Stephens, HEASARC/GSSC
*/
#ifndef st_graph_EmbedPython_h
#define st_graph_EmbedPython_h

// Python.h must be included before any standard headers.
// Use only the Python stable ABI. The build normally defines this; keep a fallback for standalone builds.
#ifndef Py_LIMITED_API
#define Py_LIMITED_API 0x03080000
#endif
#include <Python.h>

#include <cfenv>
#include <string>
#include <vector>

namespace st_graph {

  /** \class PythonFpeGuard
      \brief Disables floating point exception traps while Python code runs, restoring the previous floating point
             environment when the guard goes out of scope. Guards may be nested.

      On Linux, Science Tools applications are built with TRAP_FPE, so their main() calls feenableexcept and any
      invalid operation, division by zero or overflow raises SIGFPE. numpy, matplotlib and Tk rely on IEEE
      arithmetic producing inf and nan quietly, and so crash under those traps. Every call into Python must
      therefore be made with a PythonFpeGuard in scope.
  */
  class PythonFpeGuard {
    public:
      PythonFpeGuard();
      ~PythonFpeGuard();

    private:
      PythonFpeGuard(const PythonFpeGuard &);
      PythonFpeGuard & operator =(const PythonFpeGuard &);

      std::fenv_t m_env;
  };

  /** \class ApplicationFpeScope
      \brief Restores the application's own floating point environment (including any traps it enabled) while
             C++ code is called back from Python, for example by a button click. This keeps TRAP_FPE effective
             for application code which runs from inside the GUI event loop.
  */
  class ApplicationFpeScope {
    public:
      ApplicationFpeScope();
      ~ApplicationFpeScope();

    private:
      ApplicationFpeScope(const ApplicationFpeScope &);
      ApplicationFpeScope & operator =(const ApplicationFpeScope &);

      std::fenv_t m_env;
      int m_depth;
  };

  /** \class PyRef
      \brief Owning reference to a Python object, which is released when the PyRef goes out of scope.
  */
  class PyRef {
    public:
      /// \brief Take ownership of a new reference.
      explicit PyRef(PyObject * obj = 0): m_obj(obj) {}

      PyRef(const PyRef & other): m_obj(other.m_obj) { Py_XINCREF(m_obj); }

      // Releasing a reference may run Python code (finalizers), so it needs the floating point guard.
      ~PyRef() { if (0 != m_obj) { PythonFpeGuard guard; Py_DECREF(m_obj); } }

      PyRef & operator =(const PyRef & other) {
        Py_XINCREF(other.m_obj);
        if (0 != m_obj) { PythonFpeGuard guard; Py_DECREF(m_obj); }
        m_obj = other.m_obj;
        return *this;
      }

      PyObject * get() const { return m_obj; }

      /// \brief Give up ownership of the reference, returning it to the caller.
      PyObject * release() { PyObject * obj = m_obj; m_obj = 0; return obj; }

    private:
      PyObject * m_obj;
  };

  /// \brief Start the Python interpreter if it is not already running.
  void EP_Init();

  /// \brief Return the st_graph_tk helper module (borrowed reference). Throws if it cannot be imported.
  PyObject * EP_Helper();

  /** \brief Call a function in the st_graph_tk helper module. Returns a new reference; throws std::runtime_error
             if the call fails.
      \param function The name of the function.
      \param format Py_BuildValue format describing the arguments, for example "(si)".
  */
  PyObject * EP_CallHelper(const char * function, const char * format, ...);

  /** \brief Call a method of a Python object. Returns a new reference; throws std::runtime_error if the call fails.
      \param obj The object.
      \param method The name of the method.
      \param format Py_BuildValue format describing the arguments, for example "(si)".
  */
  PyObject * EP_CallMethod(PyObject * obj, const char * method, const char * format, ...);

  /// \brief Call a method which takes no arguments, ignoring any errors. Suitable for use in destructors.
  void EP_CallMethodNoThrow(PyObject * obj, const char * method);

  /// \brief Convert a vector of doubles to a Python list (new reference).
  PyObject * EP_FromVector(const std::vector<double> & vec);

  /// \brief Convert a Python string to a std::string. Throws if the object is not a string.
  std::string EP_AsString(PyObject * obj);

  /// \brief Fetch, clear and describe the current Python error, if any.
  std::string EP_ErrorMessage();

}

#endif
