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

#include "godleyExport.h"
#include "variableManager.h"
#include "flowCoef.h"
#include "latexMarkup.h"
#include <ecolab_epilogue.h>

using namespace std;

namespace minsky
{
  namespace
  {
    string fcStr(const FlowCoef& fc)
    {
      auto nm=VariableManager::uqName(fc.name);
      if (fc.coef==1)
        return nm;
      else if (fc.coef==-1)
        return "-"+nm;
      else
        return str(fc.coef)+nm;
    }

  // trim enclosing <i> tags
    string trim(string x)
    {
      if (x.substr(0,3)=="<i>")
        x=x.substr(3);
      if (x.substr(x.length()-4)=="</i>")
        return x.substr(0,x.length()-4);
      else
        return x;
    }
}

  void exportToCSV(std::ostream& s, const GodleyTable& g)
  {
    s<<'"'<<g.getCell(0,0)<<'"';
    for (unsigned i=1; i<g.cols(); ++i)
      s<<",\""<<trim(latexToPango(VariableManager::uqName(g.getCell(0,i))))<<'"';
    s<<'\n';
    for (unsigned r=1; r<g.rows(); ++r)
      {
        s<<g.getCell(r,0);
        for (unsigned c=1; c<g.cols(); ++c)
          s<<",\""<<trim(latexToPango(fcStr(FlowCoef(g.getCell(r,c)))))<<'"';
        s<<'\n';
      }
  }

  void exportToLaTeX(std::ostream& f, const GodleyTable& g)
  {
    f<<"\\documentclass{article}\n\\begin{document}\n";
    f<<"\\begin{tabular}{|c|";
    for (unsigned i=1; i<g.cols(); ++i)
      f<<'c';
    f<<"|}\n\\hline\n";
    f<<"Flows $\\downarrow$ / Stock Variables $\\rightarrow$";
    for (unsigned i=1; i<g.cols(); ++i)
      f<<"&$"<<VariableManager::uqName(g.getCell(0,i))<<'$';
    f<<"\\\\\\hline\n";
    for (unsigned r=1; r<g.rows(); ++r)
      {
        f<<g.getCell(r,0);
        for (unsigned c=1; c<g.cols(); ++c)
          f<<"&$"<<fcStr(FlowCoef(g.getCell(r,c)))<<'$';
        f<<"\\\\\n";
      }
    f<<"\\hline\n\\end{tabular}\n";
    f<<"\\end{document}\n";
  }
}
