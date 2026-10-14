/** \file MPLPlotFrame.cxx
    \brief Implementation for MPLPlotFrame class.
    \author Tom Stephens, HEASARC/GSSC
*/
#include "EmbedPython.h"

#include <algorithm>
#include <list>
#include <stdexcept>

#include "MPLPlot.h"
#include "MPLPlotFrame.h"

#include "st_graph/IEventReceiver.h"
#include "st_graph/Sequence.h"

namespace st_graph {

  MPLPlotFrame::MPLPlotFrame(IFrame * parent, const std::string & title, unsigned int width, unsigned int height,
    bool delete_parent): MPLFrame(parent, 0, 0, delete_parent), m_axes(3), m_plots(), m_title(title), m_dimensionality(0),
    m_drawn(false) {

    // Send event messages back to parent.
    m_receiver = m_parent->getReceiver();

    // Hook together MPL primitives.
    PyObject * parent_frame = m_parent->getPythonFrame();
    if (0 == parent_frame)
      throw std::logic_error("MPLPlotFrame constructor was passed a parent frame which cannot contain other MPL frames");

    PyRef callback(createCallback());
    setPythonFrame(EP_CallHelper("PlotFrame", "(OiiO)", parent_frame, int(width), int(height), callback.get()));
  }

  MPLPlotFrame::~MPLPlotFrame() {
    // Note: This appears more complicated than necessary, but be careful changing it. Under some circumstances,
    // a MPLPlot needs to delete its parent, but the parent will always attempt to delete the MPLPlot in the
    // process. Thus it is important to ensure the child is detached at the right times to prevent deleting
    // the parent and/or the child twice.
    try {
      reset();
    } catch (...) {
      // Do not allow exceptions to escape from the destructor.
    }
  }

  void MPLPlotFrame::display() {
    MPLFrame::display();
    if (m_hidden || 0 == m_frame) return;

    m_drawn = false;
    PyRef result(EP_CallMethod(m_frame, "clear", "()"));

    // Display plot correctly for the current dimensionality.
    if (m_dimensionality == 2) display2d();
    else if (m_dimensionality == 3) display3d();

    if (0 != m_dimensionality) {
      // Set titles and log/linear scaling.
      result = PyRef(EP_CallMethod(m_frame, "finish", "(ssssii)", m_title.c_str(), m_axes[0].getTitle().c_str(),
        m_axes[1].getTitle().c_str(), m_axes[2].getTitle().c_str(), int(Axis::eLog == m_axes[0].getScaleMode()),
        int(Axis::eLog == m_axes[1].getScaleMode())));

      // Get labels/markers from IPlots.
      long plot_index = 0;
      for (std::list<MPLPlot *>::iterator itor = m_plots.begin(); itor != m_plots.end(); ++itor, ++plot_index) {
        // Loop over plots, displaying each one's labels.
        std::vector<Marker> & marker((*itor)->getMarkers());
        for (std::vector<Marker>::size_type index = 0; index != marker.size(); ++index) {
          drawMarker(plot_index, long(index), marker[index]);
        }
      }
      m_drawn = true;
    }

    // Force complete update of the display.
    result = PyRef(EP_CallMethod(m_frame, "draw", "()"));
  }

  void MPLPlotFrame::unDisplay() {
    MPLFrame::unDisplay();
  }

  void MPLPlotFrame::reset() {
    // Delete children.
    while (!m_plots.empty()) {
      // Find last child.
      std::list<MPLPlot *>::iterator itor = --m_plots.end();

      // Get pointer to the plot.
      MPLPlot * plot = *itor;

      // Break links between this and the child plot.
      removePlot(*itor);

      // Delete the child plot.
      delete plot;
    }

    // Clear the figure.
    m_drawn = false;
    if (0 != m_frame && Py_IsInitialized()) EP_CallMethodNoThrow(m_frame, "clear");

    unDisplay();
  }

  void MPLPlotFrame::addPlot(IPlot * plot) {
    MPLPlot * mpl_plot = dynamic_cast<MPLPlot *>(plot);
    if (0 == mpl_plot) throw std::logic_error("MPLPlotFrame::addPlot cannot add a non-MPL plot");

    if (m_plots.empty()) m_dimensionality = mpl_plot->getDimensionality();
    else if (m_dimensionality != mpl_plot->getDimensionality())
      throw std::logic_error("MPLPlotFrame::addPlot cannot overlay plots with different numbers of dimensions");
    else if (m_dimensionality > 2)
      throw std::logic_error("MPLPlotFrame::addPlot cannot overlay 3d plots");

    // Make certain plot is not added more than once.
    if (m_plots.end() == std::find(m_plots.begin(), m_plots.end(), mpl_plot)) {
      m_plots.push_back(mpl_plot);
      mpl_plot->setParent(this);
    }
  }

  void MPLPlotFrame::removePlot(IPlot * plot) {
    std::list<MPLPlot *>::iterator itor = std::find(m_plots.begin(), m_plots.end(), plot);
    if (m_plots.end() != itor) {
      MPLPlot * mpl_plot = dynamic_cast<MPLPlot *>(plot);
      if (0 != mpl_plot) mpl_plot->setParent(0);
      m_plots.erase(itor);
    }
  }

  void MPLPlotFrame::addMarker(MPLPlot * plot, std::vector<Marker>::size_type index) {
    // If the plot is not currently drawn, the marker will be drawn the next time it is displayed.
    if (!m_drawn || 0 == m_frame) return;

    std::list<MPLPlot *>::iterator itor = std::find(m_plots.begin(), m_plots.end(), plot);
    if (m_plots.end() == itor) return;
    long plot_index = std::distance(m_plots.begin(), itor);

    std::vector<Marker> & marker(plot->getMarkers());
    if (index >= marker.size()) return;
    drawMarker(plot_index, long(index), marker[index]);

    // Force update of the display.
    PyRef result(EP_CallMethod(m_frame, "draw", "()"));
  }

  const std::string & MPLPlotFrame::getTitle() const {
    return m_title;
  }

  std::vector<Axis> & MPLPlotFrame::getAxes() { return m_axes; }

  const std::vector<Axis> & MPLPlotFrame::getAxes() const { return m_axes; }

  void MPLPlotFrame::handleEvent(const std::string & event, PyObject * args) {
    if ("markerMoved" == event) {
      // User dragged a marker: record its new position so that clients can retrieve it with getMarkers.
      const char * name = 0;
      long plot_index = 0;
      long marker_index = 0;
      double x = 0.;
      double y = 0.;
      if (!PyArg_ParseTuple(args, "slldd", &name, &plot_index, &marker_index, &x, &y)) return;
      if (plot_index < 0 || plot_index >= long(m_plots.size()) || marker_index < 0) return;
      std::list<MPLPlot *>::iterator itor = m_plots.begin();
      std::advance(itor, plot_index);
      std::vector<Marker> & marker((*itor)->getMarkers());
      if (marker_index < long(marker.size())) {
        marker[marker_index].m_x = x;
        marker[marker_index].m_y = y;
      }
    } else {
      MPLFrame::handleEvent(event, args);
    }
  }

  void MPLPlotFrame::display2d() {
    PyRef result(EP_CallMethod(m_frame, "begin", "(i)", 2));

    for (std::list<MPLPlot *>::iterator itor = m_plots.begin(); itor != m_plots.end(); ++itor) {

      // Get numeric sequences from data.
      const std::vector<const ISequence *> sequences((*itor)->getSequences());

      // Unpack the sequences: first dimension is the x axis, second is the y.
      const ISequence * x = sequences.at(0);
      const ISequence * y = sequences.at(1);

      std::string color = getColorString((*itor)->getLineColor());
      std::string line_style = (*itor)->getLineStyle();

      // Depending on the style, create appropriate MPL plot object.
      if ((*itor)->getStyle() == "hist") {
        // Interpret x as a set of intervals, and y as the value in each interval.
        std::vector<double> x_low;
        std::vector<double> x_high;
        std::vector<double> y_value;
        x->getIntervals(x_low, x_high);
        y->getValues(y_value);

        result = PyRef(EP_CallMethod(m_frame, "plot_hist", "(NNNss)", EP_FromVector(x_low), EP_FromVector(x_high),
          EP_FromVector(y_value), color.c_str(), line_style.c_str()));
      } else {
        // Interpret x and y as points with error bars.
        std::vector<double> x_pts;
        std::vector<double> x_low_err;
        std::vector<double> x_high_err;
        std::vector<double> y_pts;
        std::vector<double> y_low_err;
        std::vector<double> y_high_err;
        x->getValues(x_pts);
        x->getSpreads(x_low_err, x_high_err);
        y->getValues(y_pts);
        y->getSpreads(y_low_err, y_high_err);

        result = PyRef(EP_CallMethod(m_frame, "plot_scatter", "(NNNNNNsss)", EP_FromVector(x_pts),
          EP_FromVector(x_low_err), EP_FromVector(x_high_err), EP_FromVector(y_pts), EP_FromVector(y_low_err),
          EP_FromVector(y_high_err), color.c_str(), line_style.c_str(), (*itor)->getCurveType().c_str()));
      }
    }
  }

  void MPLPlotFrame::display3d() {
    if (m_plots.empty()) return;
    std::list<MPLPlot *>::iterator itor = m_plots.begin();

    // Get numeric sequences from data.
    const std::vector<const ISequence *> sequences((*itor)->getSequences());

    // Unpack the sequences: first dimension is the x axis, second is the y.
    const ISequence * x = sequences.at(0);
    const ISequence * y = sequences.at(1);

    // Get data being plotted.
    const std::vector<std::vector<double> > & z((*itor)->getZData());

    typedef std::vector<double> Vec_t;
    Vec_t lower;
    Vec_t upper;

    // Bin edges are the lower edges of each interval, plus the upper edge of the last interval.
    x->getIntervals(lower, upper);
    Vec_t x_edges(lower.begin(), lower.begin() + x->size());
    x_edges.push_back(upper[x->size() - 1]);

    y->getIntervals(lower, upper);
    Vec_t y_edges(lower.begin(), lower.begin() + y->size());
    y_edges.push_back(upper[y->size() - 1]);

    // Copy data into a list of lists, z[ix][iy].
    PyRef z_list(PyList_New(0));
    for (std::vector<Vec_t>::size_type ii = 0; ii != x->size(); ++ii) {
      PyRef row(EP_FromVector(z[ii]));
      PyList_Append(z_list.get(), row.get());
    }

    PyRef result(EP_CallMethod(m_frame, "begin", "(i)", 3));
    result = PyRef(EP_CallMethod(m_frame, "plot_lego", "(NNOiii)", EP_FromVector(x_edges), EP_FromVector(y_edges),
      z_list.get(), int(Axis::eLog == m_axes[0].getScaleMode()), int(Axis::eLog == m_axes[1].getScaleMode()),
      int(Axis::eLog == m_axes[2].getScaleMode())));
  }

  void MPLPlotFrame::drawMarker(long plot_index, long marker_index, const Marker & marker) {
    PyRef result(EP_CallMethod(m_frame, "add_marker", "(llddsi)", plot_index, marker_index, marker.m_x, marker.m_y,
      marker.m_text.c_str(), marker.m_color));
  }

  std::string MPLPlotFrame::getColorString(int color) const {
    PyRef name(EP_CallHelper("color_name", "(i)", color));
    return EP_AsString(name.get());
  }

}
