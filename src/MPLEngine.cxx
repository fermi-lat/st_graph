/** \file MPLEngine.cxx
    \brief Implementation of class which encapsulates the matplotlib graphics implementation.
    \author Tom Stephens, HEASARC/GSSC
*/
#include "EmbedPython.h"

#include <cctype>
#include <stdexcept>

#include "st_graph/IEventReceiver.h"
#include "st_graph/IFrame.h"
#include "st_graph/MPLFrame.h"

#include "MPLEngine.h"
#include "MPLPlot.h"
#include "MPLPlotFrame.h"
#include "MPLTabFolder.h"

namespace {

  // Receiver which terminates the application -- sensible default behavior for a main frame.
  class DefaultReceiver : public st_graph::IEventReceiver {
    public:
      DefaultReceiver(): m_engine(st_graph::Engine::instance()), m_exit_on_close(true) {}

      // General behavior when closing window is to hide the window.
      virtual void closeWindow(st_graph::IFrame * f) {
        if (0 != f) f->unDisplay();
        if (m_exit_on_close) m_engine.stop();
      }

      void setExitOnClose(bool exit_on_close) { m_exit_on_close = exit_on_close; }

    private:
      st_graph::Engine & m_engine;
      bool m_exit_on_close;
  };

  DefaultReceiver * s_default_receiver = 0;

  // Get the MPL frame of a parent frame, checking that it is valid.
  st_graph::MPLFrame * getParent(st_graph::IFrame * parent, const std::string & method) {
    st_graph::MPLFrame * mpl_frame = dynamic_cast<st_graph::MPLFrame *>(parent);
    if (0 == mpl_frame || 0 == mpl_frame->getPythonFrame())
      throw std::logic_error("MPLEngine::" + method + " was passed an invalid parent frame pointer");
    return mpl_frame;
  }

  // Create a widget using the named class from the st_graph_tk module. The class is constructed with the parent's
  // Python object, a text string and the callback which reports events to the new frame.
  st_graph::IFrame * createWidget(st_graph::IFrame * parent, st_graph::IEventReceiver * receiver,
    const std::string & method, const char * python_class, const std::string & text, const std::string & name,
    bool container = false) {
    using namespace st_graph;
    MPLFrame * mpl_parent = getParent(parent, method);

    MPLFrame * frame = new MPLFrame(mpl_parent, receiver, 0);
    try {
      PyRef callback(frame->createCallback());
      frame->setPythonFrame(EP_CallHelper(python_class, "(OsO)", mpl_parent->getPythonFrame(), text.c_str(),
        callback.get()));
    } catch (...) {
      delete frame;
      throw;
    }
    frame->setName(name);
    frame->setContainer(container);
    return frame;
  }

}

namespace st_graph {

  MPLEngine::MPLEngine(): m_init_succeeded(false) {
    try {
      // Start Python, import the helper module and create the Tk root window. This fails if no display is available,
      // in which case all methods which create graphical objects will throw.
      EP_Init();
      MPLFrame::ancestor();
      m_init_succeeded = true;
    } catch (const std::exception &) {
      m_init_succeeded = false;
    }
  }

  void MPLEngine::run() {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::run: graphical environment not initialized");

    // Display all frames currently linked to the top-level frame.
    MPLFrame::ancestor()->display();

    // Hide all frames which need to be hidden at the outset.
    hideHidden(MPLFrame::ancestor());

    // Run the Tk event loop to handle the graphical displays. This returns when stop() is called.
    PyRef result(EP_CallHelper("mainloop", "()"));
  }

  void MPLEngine::stop() {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::stop: graphical environment not initialized");

    // Hide all frames currently linked to the top-level frame.
    MPLFrame::ancestor()->unDisplay();

    // Cause the event loop started by run() to return.
    PyRef result(EP_CallHelper("quit", "()"));
  }

  IFrame * MPLEngine::createMainFrame(IEventReceiver * receiver, unsigned int width, unsigned int height,
    const std::string & title) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createMainFrame: graphical environment not initialized");

    // If client did not supply a receiver, use the default one.
    if (0 == receiver) {
      if (0 == s_default_receiver) s_default_receiver = new DefaultReceiver;
      receiver = s_default_receiver;
    }

    // Create the IFrame which refers to the window.
    MPLFrame * frame = new MPLFrame(receiver, 0);
    try {
      PyRef callback(frame->createCallback());
      frame->setPythonFrame(EP_CallHelper("MainFrame", "(OiisO)", MPLFrame::ancestor()->getPythonFrame(), int(width),
        int(height), title.c_str(), callback.get()));
    } catch (...) {
      delete frame;
      throw;
    }

    frame->setName("main");
    frame->setContainer();
    return frame;
  }

  IPlot * MPLEngine::createPlot(const std::string & title, unsigned int width, unsigned int height, const std::string & style,
    const ISequence & x, const ISequence & y) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createPlot: graphical environment not initialized");

    // Create parent main frame.
    IFrame * mf = createMainFrame(0, width, height, title);

    // Create frame to hold plot. This frame owns and will delete its parent.
    IFrame * pf = new MPLPlotFrame(mf, title, width, height, true);

    // Create plot. This plot owns and will delete its parent.
    return new MPLPlot(pf, style, x, y, true);
  }

  IPlot * MPLEngine::createPlot(const std::string & title, unsigned int width, unsigned int height, const std::string & style,
    const ISequence & x, const ISequence & y, const std::vector<std::vector<double> > & z) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createPlot: graphical environment not initialized");

    // Create parent main frame.
    IFrame * mf = createMainFrame(0, width, height, title);

    // Create frame to hold plot. This frame owns and will delete its parent.
    IFrame * pf = new MPLPlotFrame(mf, title, width, height, true);

    // Create plot. This plot owns and will delete its parent.
    return new MPLPlot(pf, style, x, y, z, true);
  }

  IPlot * MPLEngine::createPlot(IFrame * parent, const std::string & style, const ISequence & x, const ISequence & y) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createPlot: graphical environment not initialized");

    return new MPLPlot(parent, style, x, y);
  }

  IPlot * MPLEngine::createPlot(IFrame * parent, const std::string & style, const ISequence & x, const ISequence & y,
    const std::vector<std::vector<double> > & z) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createPlot: graphical environment not initialized");

    return new MPLPlot(parent, style, x, y, z);
  }

  IFrame * MPLEngine::createPlotFrame(IFrame * parent, const std::string & title, unsigned int width, unsigned int height) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createPlotFrame: graphical environment not initialized");

    return new MPLPlotFrame(parent, title, width, height);
  }

  IFrame * MPLEngine::createButton(IFrame * parent, IEventReceiver * receiver, const std::string & style,
    const std::string & label) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createButton: graphical environment not initialized");

    // Make style check case insensitive.
    std::string lc_style = style;
    for (std::string::iterator itor = lc_style.begin(); itor != lc_style.end(); ++itor) *itor = tolower(*itor);

    const char * python_class = 0;
    if (std::string::npos != lc_style.find("check")) {
      python_class = "CheckButton";
    } else if (std::string::npos != lc_style.find("text")) {
      python_class = "Button";
    } else {
      throw std::logic_error("MPLEngine::createButton cannot create a button with style " + style);
    }

    return createWidget(parent, receiver, "createButton", python_class, label, "button " + label);
  }

  IFrame * MPLEngine::createLabel(IFrame * parent, IEventReceiver * receiver, const std::string & text) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createLabel: graphical environment not initialized");

    return createWidget(parent, receiver, "createLabel", "Label", text, "label " + text);
  }

  IFrame * MPLEngine::createTextEntry(IFrame * parent, IEventReceiver * receiver, const std::string & content) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createTextEntry: graphical environment not initialized");

    return createWidget(parent, receiver, "createTextEntry", "TextEntry", content, "text entry " + content);
  }

  IFrame * MPLEngine::createComposite(IFrame * parent, IEventReceiver * receiver) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createComposite: graphical environment not initialized");

    std::string name = "composite frame inside " + (0 != parent ? parent->getName() : std::string());
    return createWidget(parent, receiver, "createComposite", "Composite", "", name, true);
  }

  IFrame * MPLEngine::createGroupFrame(IFrame * parent, IEventReceiver * receiver, const std::string & label) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createGroupFrame: graphical environment not initialized");

    return createWidget(parent, receiver, "createGroupFrame", "GroupFrame", label, "group frame " + label, true);
  }

  ITabFolder * MPLEngine::createTabFolder(IFrame * parent, IEventReceiver * receiver) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::createTabFolder: graphical environment not initialized");

    return new MPLTabFolder(getParent(parent, "createTabFolder"), receiver);
  }

  std::string MPLEngine::fileDialog(IFrame * parent, const std::string & initial_file_name, const std::string & style) {
    if (!m_init_succeeded) throw std::runtime_error("MPLEngine::fileDialog: graphical environment not initialized");

    std::string dir;
    std::string file;

    // Split input file name into directory and file.
    std::string::size_type delim_pos = initial_file_name.find_last_of("/\\");
    if (std::string::npos == delim_pos) {
      // No delimiter: assume directory only was supplied.
      dir = initial_file_name;
    } else {
      // Delimiter found: split into before/after delimiter.
      dir = initial_file_name.substr(0, delim_pos);
      file = initial_file_name.substr(delim_pos + 1, std::string::npos);
    }

    MPLFrame * mpl_parent = dynamic_cast<MPLFrame *>(parent);
    PyObject * py_parent = (0 != mpl_parent && 0 != mpl_parent->getPythonFrame()) ? mpl_parent->getPythonFrame() : Py_None;

    // Display the dialog box. This runs an inner event loop until the user closes the dialog box one way or another.
    PyRef result(EP_CallHelper("file_dialog", "(Osss)", py_parent, dir.c_str(), file.c_str(),
      ("open" == style ? "open" : "save")));

    // If the dialog was canceled, return the original file name.
    std::string file_name = EP_AsString(result.get());
    if (file_name.empty()) file_name = initial_file_name;

    return file_name;
  }

  void MPLEngine::setDefaultExitOnClose(bool exit_on_close) {
    if (0 == s_default_receiver) s_default_receiver = new DefaultReceiver;
    s_default_receiver->setExitOnClose(exit_on_close);
  }

  void MPLEngine::hideHidden(IFrame * frame) {
    std::list<IFrame *> subframes;
    frame->getSubframes(subframes);
    for (std::list<IFrame *>::iterator itor = subframes.begin(); itor != subframes.end(); ++itor) {
      hideHidden(*itor);
    }
    if (frame->isHidden()) frame->unDisplay(); // Will hide all children too.
  }

}
