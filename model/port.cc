/*
  @copyright Steve Keen 2015
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "port.h"
#include "item.h"
#include "wire.h"
#include "group.h"
#include <ecolab_epilogue.h>
using namespace std;

namespace minsky
{

  float Port::x() const 
  {
    return m_x+item.x();
  }

  float Port::y() const 
  {
    return m_y+item.y();
  }

  GroupPtr Port::group() const
  {
    return item.group.lock();
  }

  void Port::eraseWire(Wire* w) 
  {
    for (auto i=wires.begin(); i!=wires.end(); ++i)
      if (*i==w) 
        {
          wires.erase(i);
          break;
        }
  }

  Port::~Port()
  {
    // destruction of this port must also destroy all attached wires

    // copy out list of wire ids to prevent a snarl up from ~wires()
    set<Wire*> wiresToDelete(wires.begin(), wires.end());
    wires.clear();

    /// wires could be anywhere, so we need to walk the whole heirachy
    if (auto g=item.group.lock())
      g->globalGroup().recursiveDo(&Group::wires, [&](Wires& wires, Wires::iterator& i) {
        if (wiresToDelete.erase(i->get()))
          wires.erase(i);
        return wiresToDelete.empty();
      });
  }

}
