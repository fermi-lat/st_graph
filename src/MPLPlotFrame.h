/** \file MPLPlotFrame.h
    \brief Interface for MPLPlotFrame class.
    \author Tom Stephens, HEASARC/GSSC
*/
#ifndef st_graph_MPLPlotFrame_h
#define st_graph_MPLPlotFrame_h

#include <list>
#include <string>
#include <vector>

#include "st_graph/Axis.h"
#include "st_graph/IPlot.h"
#include "st_graph/MPLFrame.h"

namespace st_graph {

  class IFrame;
  class IPlot;
  class ISequence;
  class MPLPlot;

  /** \class MPLPlotFrame
      \brief A MPL frame which is suitable for displaying plots. The frame holds a matplotlib figure with a
             navigation toolbar. For 2d plots, the mouse may be used to drag markers, to zoom the X axis
             (left-drag; double-click restores the range), and to send rightClicked events to the receiver.
  */
  class MPLPlotFrame : public MPLFrame {
    public:
      /** \brief Construct a frame connected to the given parent, with the given properties.
          \param parent The parent frame.
          \param title The title to display on the frame.
          \param width The width of the frame in pixels.
          \param height The height of the frame in pixels.
          \param delete_parent Flag indicating frame owns (and should delete) parent.
      */
      MPLPlotFrame(IFrame * parent, const std::string & title, unsigned int width, unsigned int height,
        bool delete_parent = false);

      virtual ~MPLPlotFrame();

      virtual void display();

      virtual void unDisplay();

      virtual void reset();

      /** \brief Add the given plot to the frame.
          \param plot The plot to add.
      */
      virtual void addPlot(IPlot * plot);

      /** \brief Remove the given plot from the frame.
          \param plot The plot to remove. If not already in the frame, no harm done.
      */
      virtual void removePlot(IPlot * plot);

      /** \brief Draw a marker which was just added to one of the plots in this frame.
          \param plot The plot which owns the marker.
          \param index The index of the marker in the plot's marker container.
      */
      virtual void addMarker(MPLPlot * plot, std::vector<Marker>::size_type index);

      /** \brief Get the title of the frame.
      */
      const std::string & getTitle() const;

      std::vector<Axis> & getAxes();

      const std::vector<Axis> & getAxes() const;

      /// \brief Handle events from the plot: markers being dragged, in addition to the standard events.
      virtual void handleEvent(const std::string & event, PyObject * args);

    protected:
      /** \brief Internal helper method which correctly displays 2d plots.
      */
      virtual void display2d();

      /** \brief Internal helper method which correctly displays 3d plots.
      */
      virtual void display3d();

      /** \brief Draw one marker in the current axes.
          \param plot_index The position of the plot in this frame's container of plots.
          \param marker_index The position of the marker in the plot's container of markers.
          \param marker The marker.
      */
      void drawMarker(long plot_index, long marker_index, const Marker & marker);

      /** \brief Generate matplotlib color string for the specified color.
          \param color The Color enum value of the color desired
      */
      std::string getColorString(int color) const;

    private:
      std::vector<Axis> m_axes;
      std::list<MPLPlot *> m_plots;
      std::string m_title;
      unsigned int m_dimensionality;
      bool m_drawn;
  };

}

#endif
