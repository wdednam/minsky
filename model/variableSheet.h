/*
  @copyright Steve Keen 2019
  @author Russell Standish
  @author Wynand Dednam
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

#ifndef VARIABLESHEET_H
#define VARIABLESHEET_H
#include <parVarSheet.h>

namespace minsky
{
	 
  class VariableSheet: public ParVarSheet
  {	  
  public:
    bool variableSelector(ItemPtr i) override {if (auto v=i->variableCast()) return v->type()!=VariableType::parameter && v->attachedToDefiningVar(); return false;} 
    //ClickType clickType(double x, double y) override
    //{
    //  int c=colX(x), r=rowY(y);
    //  
    //  if (r==0)
    //    return row0;      
    //  if (c==0)
    //    return col0;  
    //
    //  if (c>0 && c<int(cols()))
    //    if (r>0 && r<int(rows()))
    //      return internal;
    //
    //  return background;
    //}     
  };
  
}
#include "variableSheet.cd"
#endif
