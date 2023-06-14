/*
  @copyright Steve Keen 2023
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

#ifndef PHILLIPS_DIAGRAM_H
#define PHILLIPS_DIAGRAM_H
#include "classdesc_access.h"
#include "renderNativeWindow.h"
#include "variable.h"
#include "wire.h"

#include <vector>
#include <map>

namespace minsky
{
  using FlowVar=Variable<VariableType::flow>;
  using StockVar=Variable<VariableType::stock>;
  class PhillipsFlow: public Wire
  {
  public:
    PhillipsFlow()=default;
    PhillipsFlow(const std::string& name, const std::weak_ptr<Port>& from, const std::weak_ptr<Port>& to):
      Wire(from,to), flow(name) {}
    static std::map<Units, double> maxFlow;
    FlowVar flow;
    double coeficient=1;
    void draw(cairo_t*);
  };

  
  class PhillipsStock: public StockVar
  {
  public:
    PhillipsStock()=default;
    PhillipsStock(const StockVar& x): StockVar(x) {addPorts();}
    std::size_t numPorts() const override {return 2;}
    void draw(cairo_t* cairo) const override;
  };
  
  class PhillipsDiagram: public RenderNativeWindow
  {
    bool redraw(int, int, int width, int height) override;
    CLASSDESC_ACCESS(PhillipsDiagram);
  public:
    std::map<std::string, PhillipsStock> stocks;
    std::vector<PhillipsFlow> flows;
    static std::map<Units, double> maxStock;
    void requestRedraw() {if (surface.get()) surface->requestRedraw();}
    /// populate phillips diagram from Godley tables in model
    void init() override;
    void updateMaxValues();
  };
}

#include "phillipsDiagram.cd"
#endif
