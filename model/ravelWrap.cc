/*
  @copyright Steve Keen 2018
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

#include "ravelWrap.h"
#include "selection.h"
#include "dimension.h"
#include "minskyTensorOps.h"
#include "minsky.h"
#include "minsky_epilogue.h"

#include <string>
#include <cmath>
using namespace std;

#include "cairoRenderer.h"

namespace minsky
{
  unsigned RavelLockGroup::nextColour=1;

  namespace
  {
    struct InvalidSym {
      const string symbol;
      InvalidSym(const string& s): symbol(s) {}
    };

    struct RavelDataSpec
    {
      int nRowAxes=-1; ///< No. rows describing axes
      int nColAxes=-1; ///< No. cols describing axes
      int nCommentLines=-1; ///< No. comment header lines
      char separator=','; ///< field separator character
    };

    typedef ravel::Op::ReductionOp ReductionOp;
    typedef ravel::HandleSort::Order HandleSort;
    
    inline double sqr(double x) {return x*x;} 
  }

  Ravel::Ravel()
  {
    if (!*this)
      {
        tooltip="https://ravelation.hpcoders.com.au";
        detailedText=lastError();
      }
  }

  void Ravel::draw(cairo_t* cairo) const
  {
    double  z=zoomFactor(), r=1.1*z*radius();
    ports[0]->moveTo(x()+1.1*r, y());
    ports[1]->moveTo(x()-1.1*r, y());
    if (mouseFocus)
      {
        drawPorts(cairo);
        displayTooltip(cairo,tooltip.empty()? explanation: tooltip);
        // Resize handles always visible on mousefocus. For ticket 92.
        drawResizeHandles(cairo);
      }
    //if (onResizeHandles) drawResizeHandles(cairo);
    cairo_rectangle(cairo,-r,-r,2*r,2*r);
    cairo_rectangle(cairo,-1.1*r,-1.1*r,2.2*r,2.2*r);
    cairo_stroke_preserve(cairo);
    if (onBorder || lockGroup)
      { // shadow the border when mouse is over it
        cairo::CairoSave cs(cairo);
        cairo::Colour c{1,1,1,0};
        if (lockGroup)
          c=palette[ lockGroup->colour() % paletteSz ];
        c.r*=0.5; c.g*=0.5; c.b*=0.5;
        c.a=onBorder? 0.5:0.3;
        cairo_set_source_rgba(cairo,c.r,c.g,c.b,c.a);
        cairo_set_fill_rule(cairo,CAIRO_FILL_RULE_EVEN_ODD);
        cairo_fill_preserve(cairo);
      }
    
    cairo_clip(cairo);

    {
      cairo::CairoSave cs(cairo);
      cairo_rectangle(cairo,-r,-r,2*r,2*r);
      cairo_clip(cairo);
      cairo_scale(cairo,z,z);
      ravel::CairoRenderer cr(cairo);
      render(cr);
    }        
    if (selected) drawSelected(cairo);
  }

  void Ravel::resize(const LassoBox& b)
  {
    rescale(0.5*std::max(fabs(b.x0-b.x1),fabs(b.y0-b.y1))/(1.21*zoomFactor()));
    moveTo(0.5*(b.x0+b.x1), 0.5*(b.y0+b.y1));
    bb.update(*this);
  }

  ClickType::Type Ravel::clickType(float xx, float yy)
  {
    double z=zoomFactor();
    for (auto& p: ports)
      if (hypot(xx-p->x(), yy-p->y()) < portRadius*z)
        return ClickType::onPort;
    double r=1.1*z*radius();
    double R=1.1*r;
    double dx=xx-x(), dy=yy-y();
    if (onResizeHandle(xx,yy))
      return ClickType::onResize;         
    if (std::abs(xx-x())>R || std::abs(yy-y())>R)
      return ClickType::outside;    
    if (std::abs(dx)<=r && std::abs(dy)<=r)
      return ClickType::onRavel;
    return ClickType::onItem;
  }

  void Ravel::onMouseDown(float xx, float yy)
  {
    double invZ=1/zoomFactor();
    ravel::Ravel::onMouseDown((xx-x())*invZ,(yy-y())*invZ);
  }
  
  void Ravel::onMouseUp(float xx, float yy)
  {
    double invZ=1/zoomFactor();
    ravel::Ravel::onMouseUp((xx-x())*invZ,(yy-y())*invZ);
  }
  bool Ravel::onMouseMotion(float xx, float yy)
  {
    double invZ=1/zoomFactor();
    return ravel::Ravel::onMouseMotion((xx-x())*invZ,(yy-y())*invZ);
  }
  
  bool Ravel::onMouseOver(float xx, float yy)
  {
    double invZ=1/zoomFactor();
    return ravel::Ravel::onMouseOver((xx-x())*invZ,(yy-y())*invZ);
  }

//  void Ravel::loadFile(const string& fileName)
//  {
//    m_filename=fileName;
//    if (dataCube && ravel)
//      {
//        if (!ravelDC_openFile(dataCube, fileName.c_str(), RavelDataSpec()))
//          throw error(ravel_lastErr());
//        else if (!ravelDC_initRavel(dataCube,ravel))
//          throw error(ravel_lastErr());
//        for (size_t i=0; i<ravel_numHandles(ravel); ++i)
//          {
//            ravel_displayFilterCaliper(ravel,i,false);
//            setHandleSortOrder(HandleState::forward,i);
//          }
//        setRank(ravel_numHandles(ravel));
//      }
//  }

  Hypercube Ravel::hypercube() const
  {
    auto outHandles=outputHandleIds();
    Hypercube hc;
    auto& xv=hc.xvectors;
    for (auto h: outHandles)
      {
        auto labels=sliceLabels(h);
        xv.emplace_back(handleDescription(h));
        auto dim=axisDimensions.find(xv.back().name);
        if (dim!=axisDimensions.end())
          xv.back().dimension=dim->second;
        else
          {
            auto dim=cminsky().dimensions.find(xv.back().name);
            if (dim!=cminsky().dimensions.end())
              xv.back().dimension=dim->second;
          }
        // else otherwise dimension is a string (default type)
        for (auto i: labels)
          xv.back().push_back(i);
      }
    return hc;
  }
  
  void Ravel::populateHypercube(const Hypercube& hc)
  {
    auto state=initState.empty()? getState(): initState;
    initState.clear();
    clear();
    for (auto& i: hc.xvectors)
      {
        vector<string> ss;
        for (auto& j: i) ss.push_back(str(j,i.dimension.units));
        addHandle(i.name, ss);
        size_t h=numHandles()-1;
        ravel::Ravel::displayFilterCaliper(h,false);
      }
    if (state.empty())
      setRank(hc.rank());
    else
      applyState(state);
#ifndef NDEBUG
    if (state.empty())
      {
        auto d=hc.dims();
        assert(d.size()==rank());
        auto outputHandles=outputHandleIds();
        for (size_t i=0; i<d.size(); ++i)
          assert(d[i]==numSliceLabels(outputHandles[i]));
      }
#endif
  }

  
  unsigned Ravel::maxRank() const
  {
    return numHandles();
  }

  void Ravel::setRank(unsigned rank)
  {
    vector<size_t> ids;
    for (size_t i=0; i<rank; ++i) ids.push_back(i);
    setOutputHandleIds(ids);
  }
  
  void Ravel::adjustSlicer(int n)
  {
    ravel::Ravel::adjustSlicer(n);
    broadcastStateToLockGroup();
  }

  bool Ravel::handleArrows(int dir, bool modifier)
  {
    adjustSlicer(dir);
    if (modifier)
      minsky().reset();
    return true;
  }

  
  bool Ravel::displayFilterCaliper() const
  {
    int h=selectedHandle();
    if (h>=0)
      {
        auto state=getHandleState(h);
        return state.displayFilterCaliper;
      }
    else
      return false;
  }
    
  bool Ravel::setDisplayFilterCaliper(bool x)
  {
    int h=selectedHandle();
    if (h>=0)
      ravel::Ravel::displayFilterCaliper(h,x);
    return x;
  }

  vector<string> Ravel::allSliceLabels() const
  {
    return ravel::Ravel::allSliceLabels(selectedHandle(),ravel::HandleSort::forward);
  }
  
  vector<string> Ravel::allSliceLabelsAxis(int axis) const
  {
    return ravel::Ravel::allSliceLabels(axis,ravel::HandleSort::forward);
  }  

//  vector<string> Ravel::allSliceLabelsImpl(int axis, HandleSort order) const
//  {
//    if (axis>=0 && axis<int(numHandles()))
//      {
//        assert(order!=HandleState::custom); //custom makes no sense here
//        // grab the labels in sorted order, or forward order if a custom order is applied
//        auto state=getHandleState(axis);
//        // grab the ordering in case its custom
//        auto customOrdering=currentPermutation(axis);
//
//        // set the order and reset calipers to full range
//        const_cast<ravel::Ravel*>(static_cast<const ravel::Ravel*>(this))->displayFilterCaliper(axis,false);
//        auto sl=sliceLabels(axis);
//        setHandleState(axis,state); // return things to how they were
//        if (state.order==ravel::HandleSort::custom)
//          applyCustomPermutation(axis,customOrdering);
//        return sl;
//      }
//    return {};
//  }

  vector<string> Ravel::pickedSliceLabels() const
  {return sliceLabels(selectedHandle());}

  void Ravel::pickSliceLabels(int axis, const vector<string>& pick) 
  {
    if (axis>=0 && axis<int(numHandles()))
      {
        // stash previous handle sort order
        auto state=getHandleState(axis);
        if (state.order!=ravel::HandleSort::custom)
          previousOrder=state.order;

        if (pick.size()>=numSliceLabels(axis))
          {
            // if all labels are selected, revert ordering to previous
            setHandleSortOrder(previousOrder, axis);
            return;
          }
        
        auto allLabels=ravel::Ravel::allSliceLabels(axis, ravel::HandleSort::none);
        map<string,size_t> idxMap; // map index positions
        for (size_t i=0; i<allLabels.size(); ++i)
          idxMap[allLabels[i]]=i;
        vector<size_t> customOrder;
        for (auto& i: pick)
          {
            auto j=idxMap.find(i);
            if (j!=idxMap.end())
              customOrder.push_back(j->second);
          }
        assert(!customOrder.empty());
        applyCustomPermutation(axis,customOrder);
      }
  }

  Dimension Ravel::dimension(int handle) const
  {
    Dimension dim;
    auto dimitr=cminsky().dimensions.find(handleDescription(handle));
    if (dimitr!=cminsky().dimensions.end())
      dim=dimitr->second;
    return dim;
  }
  
  ravel::HandleSort::Order Ravel::sortOrder() const
  {
    int h=selectedHandle();
    if (h>=0)
      {
        auto state=getHandleState(h);
        return state.order;
      }
    else
      return ravel::HandleSort::none;
  }
 
  ravel::HandleSort::Order Ravel::setSortOrder(ravel::HandleSort::Order x)
  {
    setHandleSortOrder(x, selectedHandle());
    return x;
  }

  ravel::HandleSort::Order Ravel::setHandleSortOrder(ravel::HandleSort::Order order, int handle)
  {
    if (handle>=0)
      {
        Dimension dim=dimension(handle);
        orderLabels(handle,order,ravel::toEnum<ravel::HandleSort::OrderType>(dim.type),dim.units);
      }
    return order;
  }

  bool Ravel::handleSortableByValue() const
  {
    if (rank()!=1) return false;
    auto ids=outputHandleIds();
    return size_t(selectedHandle())==ids[0];
  }

  
  string Ravel::description() const
  {
    return handleDescription(selectedHandle());
  }
  
  void Ravel::setDescription(const string& description)
  {
    setHandleDescription(selectedHandle(),description);
  }

  Dimension::Type Ravel::dimensionType() const
  {
    auto descr=description();
    auto i=axisDimensions.find(descr);
    if (i!=axisDimensions.end())
      return i->second.type;
    else
      {
        auto i=cminsky().dimensions.find(descr);
        if (i!=cminsky().dimensions.end())
          return i->second.type;
        else
          return Dimension::string;
      }
  }
  
  std::string Ravel::dimensionUnitsFormat() const
  {
    auto descr=description();
    if (descr.empty()) return "";
    auto i=axisDimensions.find(descr);
    if (i!=axisDimensions.end())
      return i->second.units;
    i=cminsky().dimensions.find(descr);
    if (i!=cminsky().dimensions.end())
      return i->second.units;
    return "";
  }
      
  /// @throw if type does not match global dimension type
  void Ravel::setDimension(Dimension::Type type,const std::string& units)
  {
    auto descr=description();
    if (descr.empty()) return;
    auto i=cminsky().dimensions.find(descr);
    Dimension d{type,units};
    if (i!=cminsky().dimensions.end())
      {
        if (type!=i->second.type)
          throw error("type mismatch with global dimension");
      }
    else
      {
        minsky().dimensions[descr]=d;
        minsky().imposeDimensions();
      }
    axisDimensions[descr]=d;
  }

  
//  RavelState Ravel::getState() const
//  {
//    RavelState state;
//        state.radius=ravel_radius(ravel);
//        for (size_t i=0; i<ravel_numHandles(ravel); ++i)
//          {
//            RavelHandleState hs;
//            ravel_getHandleState(ravel, i, &hs);
//            auto& s=state.handleStates[ravel_handleDescription(ravel,i)];
//            s=hs;
//            vector<const char*> sliceLabels(ravel_numSliceLabels(ravel,i));
//            if (!sliceLabels.empty())
//              {
//                ravel_sliceLabels(ravel,i,&sliceLabels[0]);
//                s.minLabel=sliceLabels.front();
//                s.maxLabel=sliceLabels.back();
//                if (hs.sliceIndex<sliceLabels.size())
//                  s.sliceLabel=sliceLabels[hs.sliceIndex];
//                if (s.order==HandleState::custom)
//                  {
//                    s.customOrder={sliceLabels.begin(),sliceLabels.end()};
//                  }
//              }
//          }
//        vector<size_t> ids(ravel_rank(ravel));
//        ravel_outputHandleIds(ravel,&ids[0]);
//        for (size_t i=0; i<ids.size(); ++i)
//          state.outputHandles.push_back(ravel_handleDescription(ravel,ids[i]));
//        state.sortByValue=sortByValue;
//    return state;
//  }

//  /// apply the \a state to the Ravel, leaving data, slicelabels etc unchanged
//  void Ravel::applyState(const RavelState& state)
//  {
//    if (ravel)
//      {
//        sortByValue=state.sortByValue;
//        ravel_rescale(ravel,state.radius);
//
//        vector<size_t> ids;
//        for (auto& outName: state.outputHandles)
//          {
//            for (size_t handle=0; handle<ravel_numHandles(ravel); ++handle)
//              if (ravel_handleDescription(ravel,handle)==outName)
//                ids.push_back(handle);
//          }
//        ravel_setOutputHandleIds(ravel,ids.size(),&ids[0]);
//
//        for (size_t i=0; i<ravel_numHandles(ravel); ++i)
//          {
//            string name=ravel_handleDescription(ravel,i);
//            auto hs=state.handleStates.find(name);
//            if (hs!=state.handleStates.end())
//              {
//                RavelHandleState state(hs->second);
//                ravel_setHandleState(ravel,i,&state);
//                setHandleSortOrder(state.order,i);
//                if (hs->second.order==HandleState::custom)
//                  pickSliceLabels(i,hs->second.customOrder);
//                else if (state.displayFilterCaliper)
//                  ravel_setCalipers(ravel,i,hs->second.minLabel.c_str(),
//                                    hs->second.maxLabel.c_str());
//                ravel_setSlicer(ravel,i,hs->second.sliceLabel.c_str());
//              }
//          }
//        ravel_redistributeHandles(ravel);
//      }
//  }

  void Ravel::exportAsCSV(const string& filename) const
  {
    VariableValue v(VariableType::flow);
    TensorsFromPort tp(make_shared<EvalCommon>());
    tp.ev->update(ValueVector::flowVars.data(), ValueVector::flowVars.size(), ValueVector::stockVars.data());
    v=*tensorOpFactory.create(*this, tp);
    // TODO: add some comment lines, such as source of data
    v.exportAsCSV(filename, ravel::Ravel::description());
  }

  Units Ravel::units(bool check) const
  {
    Units inputUnits=ports[1]->units(check);
    if (inputUnits.empty()) return inputUnits;
    size_t multiplier=1;
    // at this stage, gross up exponents by the handle size of each
    // reduced by product handles
    for (size_t h=0; h<numHandles(); ++h)
      {
        auto state=getHandleState(h);
        if (state.collapsed && state.reductionOp==ravel::Op::prod)
          multiplier*=numSliceLabels(h);
      }
    if (multiplier>1)
      for (auto& u: inputUnits)
        u.second*=multiplier;
    return inputUnits;
  }
  
  void Ravel::displayDelayedTooltip(float xx, float yy)
  {
    if (rank()==0)
      explanation="load CSV data from\ncontext menu";
    else
      {
        explanation=explain(xx-x(),yy-y());
        // line break every 5 words
        int spCnt=0;
        for (auto& c: explanation)
          if (isspace(c) && ++spCnt % 5 == 0)
            c='\n';
      }
  }
    
//  string Minsky::ravelVersion() const
//  {
//    return ravelLib.versionFound+
//      ((ravelLib.lib || ravelLib.versionFound=="unavailable")?
//        "":" but incompatible");
//  }

  void Ravel::leaveLockGroup()
  {
    if (lockGroup)
      lockGroup->removeFromGroup(*this);
    lockGroup.reset();
  }

  void Ravel::broadcastStateToLockGroup() const
  {
    if (lockGroup)
      {
        auto state=getState();
        for (auto& rr: lockGroup->ravels)
          if (auto r=rr.lock())
            if (r.get()!=this)
              r->applyState(state/*,true*/);
      }
  }
 
  void RavelLockGroup::removeFromGroup(const Ravel& ravel)
  {
    vector<weak_ptr<Ravel>> newRavelList;
    for (auto& i: ravels)
      {
        auto r=i.lock();
        if (r && r.get()!=&ravel)
          newRavelList.push_back(move(i));
      }
    ravels.swap(newRavelList);
    if (ravels.size()==1)
      if (auto r=ravels[0].lock())
        r->lockGroup.reset(); // this may delete this, so should be last
  }
 
}

  
