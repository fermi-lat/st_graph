/** \file RootPlotFrame.h
    \brief Interface for RootPlotFrame class.
    \author James Peachey, HEASARC/GSSC
*/
#ifndef st_graph_RootPlotFrame_h
#define st_graph_RootPlotFrame_h

#include <string>

#include "RootFrame.h"

#include "st_graph/IFrame.h"

class TGMainFrame;
class TMultiGraph;
class TRootEmbeddedCanvas;

namespace st_graph {

  class RootEngine;
  class RootFrame;

  /** \class RootPlotFrame
      \brief Interface for base class frame for all graphical frames.
  */
  class RootPlotFrame : public RootFrame {
    public:
      RootPlotFrame(IFrame * parent, const std::string & title, unsigned int width, unsigned int height);

      /// \brief Destruct the frame.
      virtual ~RootPlotFrame();

      virtual void display();

      /// \brief Get the underlying Root graphical object. Not part of the API.
      virtual TMultiGraph * getMultiGraph();

      const std::string & getTitle() const;

    private:
      std::string m_title;
      TRootEmbeddedCanvas * m_canvas;
      TMultiGraph * m_multi_graph;
  };

}

#endif
