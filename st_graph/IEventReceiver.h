/** \file IEventReceiver.h
    \brief Declaration of IEventReceiver class.
    \authors Lawrence Brown, HEASARC/GSSC
             James Peachey, HEASARC/GSSC
*/
#ifndef st_graph_IEventReceiver_h
#define st_graph_IEventReceiver_h
namespace st_graph {

  class IFrame;

  /** \class IEventReceiver
      \brief Generic interface which handles events generated by a graphical object. Each event type calls
             the corresponding method, which are all no-ops in the base class. The methods are not abstract
             so that derived client classes may handle only the events they wish to handle.
  */
  class IEventReceiver {
    public:
      virtual ~IEventReceiver() {}

      /** \brief Handle mouse click. Parameter is the frame which was clicked.
      */
      virtual void clicked(IFrame *) {}
  };

}
#endif