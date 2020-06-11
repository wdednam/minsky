/*
  @copyright Steve Keen 2016
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

#ifndef INTEGRAL_H
#define INTEGRAL_H
#include "operation.h"
#include "variableValue.h"

namespace minsky
{
  /// An integral is an additional stock variable, that integrates its flow variable
  struct Integral
  {
    VariableValue stock;
    VariableValue input;
    IntOp* operation; //< reference to the interal operation object
    Integral(VariableValue input=VariableValue()):
      stock(VariableBase::integral), input(input), operation(NULL) {}
  };

}

#endif
