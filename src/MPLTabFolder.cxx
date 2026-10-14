/** \file MPLTabFolder.cxx
    \brief Implementation for MPLTabFolder class.
    \author Tom Stephens, HEASARC/GSSC
*/
#include "EmbedPython.h"

#include <utility>

#include "MPLTabFolder.h"

#include "st_graph/Engine.h"
#include "st_graph/MPLFrame.h"

namespace st_graph {

  MPLTabFolder::MPLTabFolder(MPLFrame * parent, IEventReceiver * receiver): m_tab(), m_frame(0), m_receiver(receiver) {
    // The frame is owned by the parent, which will delete it.
    m_frame = new MPLFrame(parent, 0, 0);
    m_frame->setName("tab folder");
    try {
      PyRef callback(m_frame->createCallback());
      m_frame->setPythonFrame(EP_CallHelper("TabFolder", "(OO)", parent->getPythonFrame(), callback.get()));
    } catch (...) {
      delete m_frame;
      throw;
    }
  }

  MPLTabFolder::~MPLTabFolder() {}

  IFrame * MPLTabFolder::addTab(const std::string & label) {
    IFrame * frame = getTab(label);

    if (0 == frame) {
      // Create a frame for the new tabbed sheet; it is owned by the tab folder frame.
      MPLFrame * tab_frame = new MPLFrame(m_frame, m_receiver, 0);
      tab_frame->setName("tab " + label);
      tab_frame->setContainer();
      try {
        PyRef callback(tab_frame->createCallback());
        tab_frame->setPythonFrame(EP_CallMethod(m_frame->getPythonFrame(), "add_page", "(sO)", label.c_str(),
          callback.get()));
      } catch (...) {
        delete tab_frame;
        throw;
      }

      // Keep track of tabs so that they can be obtained again later.
      m_tab.insert(std::make_pair(label, tab_frame));

      frame = tab_frame;
    }

    return frame;
  }

  IFrame * MPLTabFolder::getTab(const std::string & label) {
    IFrame * frame = 0;
    std::map<std::string, IFrame *>::iterator itor = m_tab.find(label);
    if (m_tab.end() != itor) frame = itor->second;
    return frame;
  }

  IFrame * MPLTabFolder::getFrame() { return m_frame; }

  std::string MPLTabFolder::getSelected() const {
    PyRef selected(EP_CallMethod(m_frame->getPythonFrame(), "selected", "()"));
    return EP_AsString(selected.get());
  }

  void MPLTabFolder::select(IFrame * tab) {
    // Make sure this tab belongs to this folder.
    for (std::map<std::string, IFrame *>::iterator itor = m_tab.begin(); itor != m_tab.end(); ++itor) {
      if (itor->second == tab) {
        MPLFrame * tab_frame = dynamic_cast<MPLFrame *>(tab);
        if (0 != tab_frame)
          PyRef result(EP_CallMethod(m_frame->getPythonFrame(), "select_page", "(O)", tab_frame->getPythonFrame()));
        break;
      }
    }
  }

  void MPLTabFolder::getTabCont(std::map<std::string, IFrame *> & tab_cont) {
    tab_cont = m_tab;
  }

}
