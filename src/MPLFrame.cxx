/** \file MPLFrame.cxx
    \brief Implementation of MPLFrame class.
    \author Tom Stephens, HEASARC/GSSC
*/
#include "EmbedPython.h"

#include <algorithm>
#include <map>
#include <stdexcept>

#include "st_graph/IEventReceiver.h"
#include "st_graph/MPLFrame.h"

namespace {
  using st_graph::MPLFrame;

  // Frames which currently exist, indexed by a unique id. Events which arrive for an id not in this map are ignored.
  // Ids rather than pointers identify frames so that a stale callback can never reach a new frame which happens
  // to occupy the same address. The map is never deleted, so it outlives frames destroyed during static destruction.
  std::map<unsigned long, MPLFrame *> & liveFrames() {
    static std::map<unsigned long, MPLFrame *> * s_live_frames = new std::map<unsigned long, MPLFrame *>;
    return *s_live_frames;
  }

  unsigned long nextFrameId() {
    static unsigned long s_next_id = 0;
    return ++s_next_id;
  }

  // Function called by Python wrappers to report an event. The self argument is the id of the frame.
  PyObject * dispatchEvent(PyObject * self, PyObject * args) {
    unsigned long id = PyLong_AsUnsignedLong(self);
    if (0 != PyErr_Occurred()) return 0;
    std::map<unsigned long, MPLFrame *>::iterator found = liveFrames().find(id);
    if (liveFrames().end() == found) Py_RETURN_NONE;
    MPLFrame * frame = found->second;

    if (!PyTuple_Check(args) || PyTuple_Size(args) < 1 || !PyUnicode_Check(PyTuple_GetItem(args, 0))) {
      PyErr_SetString(PyExc_TypeError, "st_graph event callback requires an event name");
      return 0;
    }
    std::string event = PyUnicode_AsUTF8AndSize(PyTuple_GetItem(args, 0), 0);

    try {
      // Application code runs here, so give it back its own floating point environment (and traps).
      st_graph::ApplicationFpeScope fpe_scope;
      frame->handleEvent(event, args);
    } catch (const std::exception & x) {
      if (0 == PyErr_Occurred()) PyErr_SetString(PyExc_RuntimeError, x.what());
      return 0;
    } catch (...) {
      if (0 == PyErr_Occurred()) PyErr_SetString(PyExc_RuntimeError, "unknown C++ exception in st_graph event handler");
      return 0;
    }
    if (0 != PyErr_Occurred()) return 0;
    Py_RETURN_NONE;
  }

  PyMethodDef s_dispatch_def = { "st_graph_dispatch", dispatchEvent, METH_VARARGS,
    "Forward a GUI event to an st_graph::MPLFrame." };
}

namespace st_graph {

  MPLFrame * MPLFrame::ancestor() {
    static MPLFrame s_ancestor;
    // Create the Python object (and with it the Tk root window) on first use. If this fails, e.g. because no
    // display is available, an exception is thrown and creation will be attempted again next time.
    if (0 == s_ancestor.m_frame) s_ancestor.m_frame = EP_CallHelper("Ancestor", "()");
    return &s_ancestor;
  }

  MPLFrame::MPLFrame(IFrame * parent, IEventReceiver * receiver, PyObject * frame, bool delete_parent): m_state(), m_name(),
    m_subframes(), m_parent(0), m_frame(frame), m_receiver(receiver), m_delete_parent(delete_parent), m_minimum_width(0),
    m_minimum_height(0), m_hidden(false), m_container(false), m_id(nextFrameId()) {
    // Make sure the parent is a MPL parent.
    m_parent = dynamic_cast<MPLFrame *>(parent);
    if (0 == m_parent)
      throw std::logic_error("MPLFrame constructor was passed a parent IFrame which is not a MPLFrame");

    m_parent->addFrame(this);
    liveFrames()[m_id] = this;
  }

  MPLFrame::MPLFrame(IEventReceiver * receiver, PyObject * frame, bool delete_parent): m_state(), m_name(), m_subframes(),
    m_parent(MPLFrame::ancestor()), m_frame(frame), m_receiver(receiver), m_delete_parent(delete_parent),
    m_minimum_width(0), m_minimum_height(0), m_hidden(false), m_container(false), m_id(nextFrameId()) {
    m_parent->addFrame(this);
    liveFrames()[m_id] = this;
  }

  MPLFrame::~MPLFrame() {
    // Note: This appears more complicated than necessary, but be careful changing it. Under some circumstances,
    // a MPLFrame needs to delete its parent, but the parent will always attempt to delete the MPLFrame in the
    // process. Thus it is important to ensure the child is detached at the right times to prevent deleting
    // the parent and/or the child twice.

    // Stop handling events for this frame.
    liveFrames().erase(m_id);

    // Save pointer to parent in case delete parent was selected.
    MPLFrame * parent = m_parent;

    // Break all connections between this and its parent.
    if (0 != m_parent) m_parent->removeFrame(this);

    // Delete children.
    while (!m_subframes.empty()) {
      // Find last child.
      std::list<MPLFrame *>::iterator itor = --m_subframes.end();
      // Break links between this and the child.
      removeFrame(*itor);
      // Delete the child.
      delete *itor;
    }

    // Destroy the Python wrapper and the widget it holds.
    if (0 != m_frame && Py_IsInitialized()) {
      EP_CallMethodNoThrow(m_frame, "destroy");
      PythonFpeGuard guard;
      Py_DECREF(m_frame);
    }
    m_frame = 0;

    // In special case where the frame owns its parent frame, delete that as well.
    if (m_delete_parent) delete parent;
  }

  void MPLFrame::display() {
    if (m_hidden) {
      unDisplay();
      return;
    }
    for (std::list<MPLFrame *>::iterator itor = m_subframes.begin(); itor != m_subframes.end(); ++itor)
      (*itor)->display();
    if (0 != m_frame) {
      if (m_container) layout();
      PyRef result(EP_CallMethod(m_frame, "show", "()"));
    }
  }

  void MPLFrame::unDisplay() {
    if (0 != m_frame) PyRef result(EP_CallMethod(m_frame, "hide", "()"));
    for (std::list<MPLFrame *>::reverse_iterator itor = m_subframes.rbegin(); itor != m_subframes.rend(); ++itor)
      (*itor)->unDisplay();
  }

  bool MPLFrame::isHidden() const { return m_hidden; }

  void MPLFrame::setHidden(bool hidden) { m_hidden = hidden; }

  void MPLFrame::addFrame(IFrame * frame) {
    // Make certain this frame is not added to itself.
    if (this == frame) return;

    // Make certain MPL frame is not added more than once.
    if (m_subframes.end() == std::find(m_subframes.begin(), m_subframes.end(), frame)) {
      MPLFrame * f = dynamic_cast<MPLFrame *>(frame);
      if (0 == f) throw std::logic_error("MPLFrame::addFrame was passed an invalid child frame");

      // Note that Tk widgets are linked to their parent widgets when they are created, so there is nothing
      // to do here with the Python objects.

      // Connect this parent to the child.
      f->m_parent = this;

      // Connect child to this parent.
      m_subframes.push_back(f);
    }
  }

  void MPLFrame::removeFrame(IFrame * frame) {
    std::list<MPLFrame *>::iterator itor = std::find(m_subframes.begin(), m_subframes.end(), frame);
    if (m_subframes.end() != itor) {
      // Disconnect child from parent.
      m_subframes.erase(itor);
      MPLFrame * f = dynamic_cast<MPLFrame *>(frame);
      if (0 == f) throw std::logic_error("MPLFrame::removeFrame was passed an invalid child frame");

      // Disconnect parent from child.
      f->m_parent = 0;
    }
  }

  void MPLFrame::getSubframes(std::list<IFrame *> & frame_cont) {
    frame_cont.assign(m_subframes.begin(), m_subframes.end());
  }

  const std::string & MPLFrame::getState() const {
    m_state.clear();
    if (0 != m_frame) {
      PyRef state(EP_CallMethod(m_frame, "get_state", "()"));
      m_state = EP_AsString(state.get());
    }
    return m_state;
  }

  void MPLFrame::setState(const std::string & state) {
    if (0 != m_frame) PyRef result(EP_CallMethod(m_frame, "set_state", "(s)", state.c_str()));
  }

  void MPLFrame::setToolTipText(const std::string & text) {
    if (0 != m_frame) PyRef result(EP_CallMethod(m_frame, "set_tooltip", "(s)", text.c_str()));
  }

  void MPLFrame::setNaturalSize() {
    if (0 != m_frame) PyRef result(EP_CallMethod(m_frame, "set_natural_size", "()"));
  }

  void MPLFrame::layout(bool) {
    if (0 != m_receiver) m_receiver->layout(this);
    else if (m_container) stackSubframes();
  }

  long MPLFrame::getHCenter() const {
    // Computation designed to minimize round-off error.
    // center = left + .5 * width = .5 * (2. * left + width) = (left + left + width) / 2
    long left = getL();
    return (left + left + getWidth()) / 2;
  }

  void MPLFrame::setHCenter(long center) {
    // left = center - .5 * width = .5 * (2. * center - width) = (center + center - width) / 2
    setL((center + center - getWidth()) / 2);
  }

  long MPLFrame::getVCenter() const {
    long top = getT();
    return (top + top + getHeight()) / 2;
  }

  void MPLFrame::setVCenter(long center) {
    setT((center + center - getHeight()) / 2);
  }

  long MPLFrame::getL() const {
    long x, y, width, height;
    getGeometry(x, y, width, height);
    if (0 != m_parent) x += m_parent->getL(); // Convert to absolute coordinates.
    return x;
  }

  void MPLFrame::setL(long l) {
    if (0 != m_parent) l -= m_parent->getL(); // Convert to relative coordinates.
    setRelativeX(l);
  }

  long MPLFrame::getR() const {
    return getL() + getWidth();
  }

  void MPLFrame::setR(long r) {
    setL(r - getWidth());
  }

  long MPLFrame::getT() const {
    long x, y, width, height;
    getGeometry(x, y, width, height);
    if (0 != m_parent) y += m_parent->getT(); // Convert to absolute coordinates.
    return y;
  }

  void MPLFrame::setT(long t) {
    if (0 != m_parent) t -= m_parent->getT(); // Convert to relative coordinates.
    setRelativeY(t);
  }

  long MPLFrame::getB() const {
    return getT() + getHeight();
  }

  void MPLFrame::setB(long b) {
    setT(b - getHeight());
  }

  long MPLFrame::getWidth() const {
    long x, y, width, height;
    getGeometry(x, y, width, height);
    return width;
  }

  void MPLFrame::setWidth(long width) {
    width = (m_minimum_width < width) ? width : m_minimum_width;
    if (0 != m_frame) PyRef result(EP_CallMethod(m_frame, "set_width", "(l)", width));
  }

  long MPLFrame::getHeight() const {
    long x, y, width, height;
    getGeometry(x, y, width, height);
    return height;
  }

  void MPLFrame::setHeight(long height) {
    height = (m_minimum_height < height) ? height : m_minimum_height;
    if (0 != m_frame) PyRef result(EP_CallMethod(m_frame, "set_height", "(l)", height));
  }

  long MPLFrame::getMinimumWidth() const { return m_minimum_width; }

  void MPLFrame::setMinimumWidth(long width) { m_minimum_width = width > 0 ? width : 0; }

  long MPLFrame::getMinimumHeight() const { return m_minimum_height; }

  void MPLFrame::setMinimumHeight(long height) { m_minimum_height = height > 0 ? height : 0; }

  PyObject * MPLFrame::getPythonFrame() { return m_frame; }

  void MPLFrame::setPythonFrame(PyObject * frame) {
    if (0 != m_frame) {
      EP_CallMethodNoThrow(m_frame, "destroy");
      PythonFpeGuard guard;
      Py_DECREF(m_frame);
    }
    m_frame = frame;
  }

  IEventReceiver * MPLFrame::getReceiver() { return m_receiver; }

  void MPLFrame::setContainer(bool container) { m_container = container; }

  PyObject * MPLFrame::createCallback() {
    PyRef id(PyLong_FromUnsignedLong(m_id));
    if (0 == id.get()) throw std::runtime_error("MPLFrame::createCallback: " + EP_ErrorMessage());
    PyObject * callback = PyCFunction_New(&s_dispatch_def, id.get());
    if (0 == callback) throw std::runtime_error("MPLFrame::createCallback: " + EP_ErrorMessage());
    return callback;
  }

  void MPLFrame::handleEvent(const std::string & event, PyObject * args) {
    if ("layout" == event) {
      layout();
    } else if (0 == m_receiver) {
      return;
    } else if ("clicked" == event) {
      m_receiver->clicked(this);
    } else if ("close" == event) {
      m_receiver->closeWindow(this);
    } else if ("modified" == event) {
      const char * name = 0;
      const char * text = 0;
      if (!PyArg_ParseTuple(args, "ss", &name, &text)) return;
      m_receiver->modified(this, text);
    } else if ("rightClicked" == event) {
      const char * name = 0;
      double x = 0.;
      double y = 0.;
      if (!PyArg_ParseTuple(args, "sdd", &name, &x, &y)) return;
      m_receiver->rightClicked(this, x, y);
    }
  }

  void MPLFrame::getGeometry(long & x, long & y, long & width, long & height) const {
    x = y = width = height = 0;
    if (0 == m_frame) return;
    PyRef geometry(EP_CallMethod(m_frame, "geometry", "()"));
    if (!PyArg_ParseTuple(geometry.get(), "llll", &x, &y, &width, &height))
      throw std::runtime_error("MPLFrame::getGeometry: " + EP_ErrorMessage());
  }

  void MPLFrame::setRelativeX(long x) {
    if (0 != m_frame) PyRef result(EP_CallMethod(m_frame, "set_x", "(l)", x > 0 ? x : 0));
  }

  void MPLFrame::setRelativeY(long y) {
    if (0 != m_frame) PyRef result(EP_CallMethod(m_frame, "set_y", "(l)", y > 0 ? y : 0));
  }

  void MPLFrame::stackSubframes() {
    long top = 0;
    long width = 0;
    for (FrameCont::iterator itor = m_subframes.begin(); itor != m_subframes.end(); ++itor) {
      if ((*itor)->isHidden()) continue;
      (*itor)->setRelativeX(0);
      (*itor)->setRelativeY(top);
      top += (*itor)->getHeight();
      width = std::max(width, (*itor)->getWidth());
    }
    if (0 != top) {
      setWidth(width);
      setHeight(top);
    }
  }

  MPLFrame::MPLFrame(): m_state(), m_name("ancestor"), m_subframes(), m_parent(0), m_frame(0), m_receiver(0),
    m_delete_parent(false), m_minimum_width(0), m_minimum_height(0), m_hidden(false), m_container(false), m_id(0) {}

}
