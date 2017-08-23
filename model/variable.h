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
#ifndef VARIABLE_H
#define VARIABLE_H

#include "slider.h"
#include "str.h"

#include <ecolab.h>
#include <arrays.h>

#include <vector>
#include <map>
// override EcoLab's default CLASSDESC_ACCESS macro
#include "classdesc_access.h"

#include "polyBase.h"
#include <polyPackBase.h>
#include "variableType.h"
#include "item.h"
#include <accessor.h>
#include <cairo/cairo.h>

namespace minsky
{
  class VariablePtr;
  struct SchemaHelper;

  template <class T, class G, class S>
  ecolab::Accessor<T,G,S> makeAccessor(G g,S s) {
    return ecolab::Accessor<T,G,S>(g,s);
  }
  
  class VariableBase: virtual public classdesc::PolyPackBase,
                      public Item, public Slider, 
                      public VariableType
  {
  public:
    typedef VariableType::Type Type;
  protected:
 
    friend struct minsky::SchemaHelper;

  private:
    CLASSDESC_ACCESS(VariableBase);
    std::string m_name; 

  protected:
    void addPorts();
    
  public:
    ///factory method
    static VariableBase* create(Type type); 

    virtual size_t numPorts() const=0;
    //std::string classType() const override {return "VariableBase";}
    virtual Type type() const=0;

    
    /// @{ variable displayed name
    virtual std::string _name() const;
    virtual std::string _name(const std::string& nm);
    ecolab::Accessor<std::string> name {
      [this]() {return _name();},
        [this](const std::string& s){return _name(s);}};
    /// @}

    //virtual Type type() const override=0;
    bool ioVar() const override;

    /// unqualified portion of name
    //virtual std::string uqName() const {return m_name;}

    /// ensure an associated variableValue exists
    void ensureValueExists() const;

    /// string used to link to the VariableValue associated with this
    virtual std::string valueId() const;

    /// zoom by \a factor, scaling all widget's coordinates, using (\a
    /// xOrigin, \a yOrigin) as the origin of the zoom transformation
    //   void zoom(float xOrigin, float yOrigin,float factor);
    void setZoom(float factor) {zoomFactor=factor;}

    /// @{ the initial value of this variable
    std::string _init() const; /// < return initial value for this variable
    std::string _init(const std::string&); /// < set the initial value for this variable
    ecolab::Accessor<std::string> init {
      [this]() {return _init();},
        [this](const std::string& s){return _init(s);}};
    /// @}
    
    /// @{ current value associated with this variable
    double _value(double);
    double _value() const;  
    ecolab::Accessor<double> value {
      [this]() {return _value();},
        [this](double x){return _value(x);}};
    /// @}

    /// sets variable value (or init value)
    void sliderSet(double x) {init(str(x)); value(x);}
    /// initialise slider bounds when slider first opened
    void initSliderBounds();
    void adjustSliderBounds();

    /// variable is on left hand side of flow calculation
    bool lhs() const {return type()==flow || type()==tempFlow;} 
    /// variable is temporary
    bool temp() const {return type()==tempFlow || type()==undefined;}
    virtual VariableBase* clone() const override=0;
    bool isStock() const {return type()==stock || type()==integral;}

    VariableBase() {}
    VariableBase(const VariableBase& x): Item(x), Slider(x), m_name(x.m_name) {}
    VariableBase& operator=(const VariableBase& x) {
      Item::operator=(x);
      Slider::operator=(x);
      m_name=x.m_name;
      return *this;
    }
    virtual ~VariableBase();

    /// adds inPort for integral case (not relevant elsewhere) if one
    /// not allocated, removes it if one allocated
    // virtual void toggleInPort();

    /** draws the icon onto the given cairo context 
        @return cairo path of icon outline
    */
    void draw(cairo_t*) const override;

    bool inputWired() const;
  };

  template <VariableType::Type T>
  class Variable: public ItemT<Variable<T>, VariableBase>,
                  public classdesc::PolyPack<Variable<T> >
  {
  public:
    typedef VariableBase::Type Type;
    Type type() const override {return T;}
    size_t numPorts() const override;

    Variable(const Variable& x): VariableBase(x) {this->addPorts();}
    Variable& operator=(const Variable& x) {
      VariableBase::operator=(x);
      this->addPorts();
      return *this;
    }
    Variable(const std::string& name="") {this->name(name); this->addPorts();}
//    Variable* clone() const override {
//      return new Variable(*this);}
//    std::string classType() const override 
//    {return "Variable<"+typeName(T)+">";}
//    void TCL_obj(classdesc::TCL_obj_t& t, const classdesc::string& d) override
//    {::TCL_obj(t,d,*this);}
  };

  struct VarConstant: public Variable<VariableType::constant>
  {
    int id;
    static int nextId;
    VarConstant(): id(nextId++) {ensureValueExists();}
    std::string valueId() const override {return "constant:"+str(id);}
    std::string _name() const override {return init();}
    std::string _name(const std::string& nm) override {ensureValueExists(); return _name();}
    VarConstant* clone() const override {return new VarConstant(*this);}
    std::string classType() const override {return "VarConstant";}
  
  };

  class VariablePtr: 
    public classdesc::shared_ptr<VariableBase>
  {
    typedef classdesc::shared_ptr<VariableBase> PtrBase;
  public:
    virtual int id() const {return -1;}
    VariablePtr(VariableBase::Type type=VariableBase::undefined, 
                const std::string& name=""): 
      PtrBase(VariableBase::create(type)) {get()->name(name);}
    template <class P>
    VariablePtr(P* var): PtrBase(dynamic_cast<VariableBase*>(var)) 
    {
      // check for incorrect type assignment
      assert(!var || *this);
    }
    VariablePtr(const PtrBase& x): PtrBase(x) {}
    VariablePtr(const VariableBase& x): PtrBase(x.clone()) {}
    /// changes type of variable to \a type
    void retype(VariableBase::Type type);
    VariablePtr(const ItemPtr& x): 
      PtrBase(std::dynamic_pointer_cast<VariableBase>(x)) {}
    /// make variable's type consistent with the type of the valueId
    void makeConsistentWithValue();
  };

}
#include "variable.cd"
#endif
