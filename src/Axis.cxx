/** \file Axis.cxx
    \brief Implementation of Axis class.
    \author James Peachey, HEASARC/GSSC
*/
#include "st_graph/Axis.h"

namespace st_graph {

  Axis::Axis(): m_title(), m_log_scale(false) {}

  auto Axis::getTitle() const -> const std::string & { return m_title; }

  void Axis::setTitle(const std::string & title) { m_title = title; }

  auto Axis::getScaleMode() const -> int { return m_log_scale ? eLog : eLinear; }

  void Axis::setScaleMode(int scale_mode) { m_log_scale = eLog == scale_mode ? true : false; }

}
