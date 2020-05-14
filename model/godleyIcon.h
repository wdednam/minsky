/*
  @copyright Steve Keen 2012
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
#ifndef GODLEYICON_H
#define GODLEYICON_H

#include "variable.h"
#include "godleyTable.h"
#include "zoom.h"
#include "intrusiveMap.h"
#include "classdesc_access.h"
#include "SVGItem.h"
#include "group.h"

#include <map>
#include <cairo.h>
#include <cairo_base.h>

namespace minsky
{
  class GodleyTableEditor;

  class GodleyIcon: public ItemT<GodleyIcon>
  {
    /// for placement of bank icon within complex
    float flowMargin=0, stockMargin=0, iconWidth=100, iconHeight=100;
    /// icon scale is adjusted when Godley icon is resized
    float m_iconScale=1;
    CLASSDESC_ACCESS(GodleyIcon);
    friend class SchemaHelper;

    /// support godley edit window on canvas
    struct CopiableUniquePtr: public std::unique_ptr<GodleyTableEditor>
    {
      // make this copiable, but do nothing on copying
      CopiableUniquePtr();
      ~CopiableUniquePtr();
      CopiableUniquePtr(const CopiableUniquePtr&);
      CopiableUniquePtr& operator=(const CopiableUniquePtr&) {return *this;}
    };
    CopiableUniquePtr editor;

    void updateBB() {
      auto wasSelected=selected;
      selected=true; // ensure bounding box is set to the entire icon
      bb.update(*this); 
      selected=wasSelected;
    }
  public:
    static SVGRenderer svgRenderer;
    
    ~GodleyIcon() {removeControlledItems();}

    /// indicate whether icon is in editor mode or icon mode
    bool editorMode() const {return editor.get();}
    void toggleEditorMode();

    /// enable/disable drawing buttons in table on canvas display
    bool buttonDisplay() const;
    void toggleButtons(); 

    bool variableDisplay=true;
    void toggleVariableDisplay() {variableDisplay=!variableDisplay;}
    
    /// width of Godley icon in screen coordinates
    float gWidth() const {return leftMargin()+iconWidth*iconScale()*zoomFactor();}
    /// height of Godley icon in screen coordinates
    float gHeight() const {return bottomMargin()+iconHeight*iconScale()*zoomFactor();}
    /// scale icon until it's height matches \a h 
    void scaleIconForHeight(float h) {update(); m_iconScale*=h/gHeight();}

    /// left margin of bank icon with Godley icon
    float leftMargin() const {return variableDisplay? flowMargin*iconScale()*zoomFactor(): 0;}
    /// bottom margin of bank icon with Godley icon
    float bottomMargin() const {return variableDisplay? stockMargin*iconScale()*zoomFactor(): 0;}

    /// icon scale is adjusted when Godley icon is resized
    float iconScale() const {return m_iconScale;}
    
    /// helper for schema1
    double schema1ZoomFactor() const; 
    
    void resize(const LassoBox&) override;
    void removeControlledItems() const override;
 
    /// set cell(row,col) with contents val
    void setCell(int row, int col, const string& val);
    /// delete row before \a row
    void deleteRow(unsigned row);
    /// move the contents of cell at (srcRow, srcCol) to (destRow, destCol).
    void moveCell(int srcRow, int srcCol, int destRow, int destCol);
    /// flows, along with multipliers, appearing in \a col
    std::map<string,double> flowSignature(int col) const;

    typedef std::vector<VariablePtr> Variables;
    const Variables& flowVars() const {return m_flowVars;}
    const Variables& stockVars() const {return m_stockVars;}
    GodleyTable table;
    /// updates the variable lists with the Godley table
    void update();
    
    GodleyIcon* clone() const override {
      auto r=new GodleyIcon(*this);
      // create a dummy group for the cloned Godley icon to live in while the update takes place. for tickets 1180-1183. 
      if (auto g=r->group.lock())
      {
	   GroupPtr dummy(new Group);
       for (auto& i: g->items)
       {		
          dummy->addItem(i);			  
          assert(!i->ioVar());
       }
	   for (auto& it: dummy->items) 
	     if (auto godley=dynamic_cast<GodleyIcon*>(it.get())) {
	        godley->update();
	        r=godley;
	     }
	   dummy->clear(); 
	  }
	  else r->update();
      return r;
    }      

    /// returns the variable if point (x,y) is within a
    /// variable icon, null otherwise, indicating that the Godley table
    /// has been selected.
    ItemPtr select(float x, float y) const override;
    ClickType::Type clickType(float x, float y) override;

    /// draw icon to \a context
    void draw(cairo_t* context) const override;

    /// returns valueid for variable reference in table
    // TODO: this should be refactored to a more central location
    std::string valueId(const std::string& x) const {
      return VariableValue::valueId(group.lock(), x);
    }
    /// performs dimensional analysis on stock var column \a stockName
    /// @param check indicates whether a consistency check is applied
    Units stockVarUnits(const std::string& stockName, bool check) const;

    void setCurrency(const std::string& currency) 
    {for (auto& i: m_stockVars) i->setUnits(currency);}
      
    void insertControlled(Selection& selection) override;
  private:
    void updateVars(Variables& vars, 
                    const vector<string>& varNames, 
                    VariableBase::Type varType);
    /// move contained variables to correct locations within icon
    void positionVariables() const;
    Variables m_flowVars, m_stockVars;
  };
}

#ifdef CLASSDESC
// omit these, because weak/shared pointers cause problems, and its
// not needed anyway
#pragma omit pack minsky::GodleyIcon
#pragma omit unpack minsky::GodleyIcon
#endif
namespace classdesc_access
{
  template <> struct access_pack<minsky::GodleyIcon>: 
    public classdesc::NullDescriptor<classdesc::pack_t> {};
  template <> struct access_unpack<minsky::GodleyIcon>: 
    public classdesc::NullDescriptor<classdesc::unpack_t> {};
}

#include "godleyIcon.cd"
#include "godleyIcon.xcd"
#endif
