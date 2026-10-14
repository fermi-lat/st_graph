/**
    \mainpage st_graph package

    \author  James Peachey peachey@lheamail.gsfc.nasa.gov

    \section intro Introduction
    The st_graph package provides rudimentary plotting capabilities
    and simple GUI widgets. The graphics engine (MPLEngine) is
    implemented with matplotlib and Tk, through an embedded Python
    interpreter; the Python side lives in the st_graph_tk module.
    The original Root implementation (RootEngine and related classes)
    remains in the source tree but is no longer built.

    \section info Information For Clients
    Basically two steps are needed to produce a plot using st_graph.
    First, a graphics engine which encapsulates the main application
    event loop is created. This engine also functions as an abstract
    factory for the creation of plots.

    In 2d plots, markers may be dragged with the mouse (the new positions
    are available from IPlot::getMarkers), dragging with the left button
    zooms the X axis (double-click restores it), and the right button
    sends IEventReceiver::rightClicked with the data coordinates.

    If no display is available the engine cannot be initialized, and
    methods which create graphical objects throw std::runtime_error.
    For automated tests, setting ST_GRAPH_AUTO_CLOSE_MS makes each
    Engine::run() return after that many milliseconds, and setting
    ST_GRAPH_SNAPSHOT_DIR saves a PNG image of every plot drawn.

    \subsection sample Sample Code
    Note this example uses an older class (PlotHist) which is deprecated and
    will soon be removed! A better example is coming.
\verbatim

  // Create local reference to engine singleton. This engine is an abstract factory for graphical objects.
  Engine & engine(Engine::instance());

  // Create a histogram plot, size 900 x 600, with the given bin definitions.
  // intervals is a container of start/stop values for the histogram.
  PlotHist * plot_hist_1 = engine.createPlotHist1D("Plot 1", 900, 600, intervals);

  // Fill plot with sinusoids.
  for (int ii = 0; ii < num_intervals; ++ii) {
    plot_hist_1->set(ii, sine_wave[ii]);
  }

  // Display all graphical objects.
  engine.run();

\endverbatim

    The class Engine is the abstract factory for plots. It is used
    to create a 1-d histogram plot, an object of a class named
    PlotHist. This plot is then populated with
    data. The display is created when Engine::run() is called.
    The above example is excerpted from src/test/test_st_graph.cxx.
    When the test code is run, it displays several plots of
    sinusoidal curves.

*/
