/** \file MPLFrame.h
    \brief Interface for MPLFrame class.
    \author Tom Stephens, HEASARC/GSSC
*/
#ifndef st_graph_MPLFrame_h
#define st_graph_MPLFrame_h

#include <list>
#include <string>

#include "st_graph/IFrame.h"

// Forward declaration of the Python object type, so that clients need not include Python.h.
typedef struct _object PyObject;

namespace st_graph {

  class IEventReceiver;

  /** \class MPLFrame
      \brief Base class frame for all graphical frames implemented with Tk and matplotlib.

      Each MPLFrame owns one wrapper object from the st_graph_tk Python module. The wrapper holds the Tk widget
      and its geometry; events from the widget are sent back to the MPLFrame through a callback created by
      createCallback(), and are dispatched to the frame's IEventReceiver by handleEvent().
  */
  class MPLFrame : public IFrame {
      typedef std::list<MPLFrame *> FrameCont;

    public:
      /// \brief Return the invisible frame which is the parent of all main frames.
      static MPLFrame * ancestor();

      /** \brief Construct a MPLFrame which wraps the given Python frame.
          \param parent The parent frame in which to embed the constructed frame.
          \param receiver The event receiver which will process events from the frame.
          \param frame Already constructed st_graph_tk wrapper object, or 0 if it will be supplied later
                 with setPythonFrame(). The MPLFrame takes ownership of this reference.
          \param delete_parent Flag indicating frame owns (and should delete) parent.
      */
      MPLFrame(IFrame * parent, IEventReceiver * receiver, PyObject * frame, bool delete_parent = false);

      /** \brief Construct a top level MPLFrame which wraps the given Python frame.
          \param receiver The event receiver which will process events from the frame.
          \param frame Already constructed st_graph_tk wrapper object, or 0 if it will be supplied later
                 with setPythonFrame(). The MPLFrame takes ownership of this reference.
          \param delete_parent Flag indicating frame owns (and should delete) parent.
      */
      MPLFrame(IEventReceiver * receiver, PyObject * frame, bool delete_parent = false);

      virtual ~MPLFrame();

      virtual std::string getName() const { return m_name; }

      virtual void setName(const std::string & name) { m_name = name; }

      virtual void display();

      virtual void unDisplay();

      virtual void reset() {}

      /** \brief Get flag indicating whether frame is hidden.
      */
      virtual bool isHidden() const;

      /** \brief Set flag indicating whether frame should be hidden. If hidden, calls to display() will have no effect.
          \param hidden If true, frame will not be displayed if display() is called.
      */
      virtual void setHidden(bool hidden = true);

      /** \brief Add the given (sub) frame to this container frame.
          \param frame The frame being added.
      */
      virtual void addFrame(IFrame * frame);

      /** \brief Remove the given (sub) frame to this container frame. If the frame is not currently in the container,
                 no harm done.
          \param frame The frame being removed.
      */
      virtual void removeFrame(IFrame * frame);

      virtual void getSubframes(std::list<st_graph::IFrame *> & frame_cont);

      /** \brief Get a string describing the state of the widget. The possible values of the state string depend
                 on the exact type of widget being represented by the IFrame.
                 For buttons, possible states are "up" and "down".
                 For text entry frames, the state gives the text the user has currently entered.
      */
      virtual const std::string & getState() const;

      /** \brief Change the internal state of the widget using the state description given by the argument.
                 Valid values of the state string depend on the exact type of widget being represented by the IFrame.
                 For buttons, possible states are "up" and "down".
                 For text entry frames, the state gives the text the user has currently entered.
          \param state The new state of widget being set.
      */
      virtual void setState(const std::string & state);

      /** \brief Set the tool tip text for the widget. Causes tool tips to appear/disappear automatically.
          \param text The text to display in the tool tip.
      */
      virtual void setToolTipText(const std::string & text);

      virtual void setNaturalSize();

      /** \brief Lay out the subframes of this frame. If the frame has an event receiver, its layout method is
                 called. Otherwise containers stack their subframes vertically.
          \param force_layout Ignored; present for compatibility with RootFrame.
      */
      virtual void layout(bool force_layout = false);

      virtual long getHCenter() const;

      virtual void setHCenter(long center);

      virtual long getVCenter() const;

      virtual void setVCenter(long center);

      virtual long getL() const;

      /** \brief Set the X position of the left edge of the frame.
          \param l The new position of the left edge.
      */
      virtual void setL(long l);

      virtual long getR() const;

      /** \brief Set the X position of the right edge of the frame.
          \param r The new position of the right edge.
      */
      virtual void setR(long r);

      virtual long getT() const;

      /** \brief Set the Y position of the top edge of the frame.
          \param t The new position of the top edge.
      */
      virtual void setT(long t);

      virtual long getB() const;

      /** \brief Set the Y position of the bottom edge of the frame.
          \param b The new position of the bottom edge.
      */
      virtual void setB(long b);

      virtual long getWidth() const;

      virtual void setWidth(long width);

      virtual long getHeight() const;

      virtual void setHeight(long height);

      virtual long getMinimumWidth() const;

      virtual void setMinimumWidth(long width);

      virtual long getMinimumHeight() const;

      virtual void setMinimumHeight(long height);

      /// \brief Get the st_graph_tk wrapper object for this frame (borrowed reference).
      virtual PyObject * getPythonFrame();

      /// \brief Replace the st_graph_tk wrapper object for this frame. Takes ownership of the reference.
      virtual void setPythonFrame(PyObject * frame);

      virtual IEventReceiver * getReceiver();

      /// \brief Flag this frame as one which contains and lays out other frames.
      virtual void setContainer(bool container = true);

      /** \brief Create a Python callable which, when called with an event name and arguments, invokes handleEvent
                 on this frame. Returns a new reference.
      */
      virtual PyObject * createCallback();

      /** \brief Handle an event sent by the Python wrapper object.
          \param event The name of the event, e.g. "clicked", "modified", "close", "layout", "rightClicked".
          \param args The complete Python argument tuple, whose first item is the event name.
      */
      virtual void handleEvent(const std::string & event, PyObject * args);

    protected:
      /// \brief Get geometry (position relative to the parent frame, and size) from the Python wrapper.
      void getGeometry(long & x, long & y, long & width, long & height) const;

      /// \brief Set the position of this frame relative to its parent, clamping at 0.
      void setRelativeX(long x);
      void setRelativeY(long y);

      /// \brief Default layout used by containers without an event receiver: stack subframes vertically.
      void stackSubframes();

      mutable std::string m_state;
      std::string m_name;
      FrameCont m_subframes;
      MPLFrame * m_parent;
      PyObject * m_frame;
      IEventReceiver * m_receiver;
      bool m_delete_parent;
      long m_minimum_width;
      long m_minimum_height;
      bool m_hidden;
      bool m_container;
      unsigned long m_id;

    private:
      MPLFrame();
  };

}

#endif
