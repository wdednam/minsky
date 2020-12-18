/*
  @copyright Steve Keen 2020
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
#include "itemTab.h"
#include "godleyTableWindow.h"
#include "latexMarkup.h"
#include "group.h"
#include <pango.h>
#include "minsky_epilogue.h"
#include "minsky.h"
#include "equations.h"
using namespace std;
using namespace MathDAG;
using ecolab::cairo::Surface;
using ecolab::Pango;
using ecolab::cairo::CairoSave;

namespace minsky
{
	
   void ItemTab::markEdited()
   {
     minsky::minsky().markEdited();
   }
   
   void ItemTab::insertRow(unsigned row)
   {
     if (row<=data.size())
       {
         data.insert(data.begin()+row, vector<string>(cols()));
         markEdited();
       }
   }
   
   void ItemTab::insertCol(unsigned col)
   {
     for (unsigned row=0; row<data.size(); ++row)
       {
         for (size_t i=data[row].size(); i<col; ++i)
           data[row].insert(data[row].end(), "");
         data[row].insert(data[row].begin()+col, "");
       }
     markEdited();
   }   
	

  void ItemTab::populateItemVector() {
    itemVector.clear();	
    minsky().canvas.model->recursiveDo(&GroupItems::items,
                                       [&](Items&, Items::iterator i) {                                 
                                         if (itemSelector(*i)) 
                                           {		                                 
                                             itemVector.emplace_back(*i);
                                             if (auto p=(*i)->plotWidgetCast()) itemCoords.emplace(make_pair(*i,make_pair(p->x(),p->y()))); 
                                             if (auto g=dynamic_cast<GodleyIcon*>(i->get())) itemCoords.emplace(make_pair(*i,make_pair(g->x(),g->y())));
                                           }
                                         return false;
                                       });   	
  }
  
  int ItemTab::colX(double x) const
  { 
    if (itemVector.empty() || colLeftMargin.empty()) return -1;
    size_t c=-1;
    for (auto& i: colLeftMargin)
      {
        auto p=std::upper_bound(i.second.begin(), i.second.end(), (x-offsx));
        c=p-i.second.begin();
      }
    if (c<0) c=-1; // out of bounds, invalidate
    return c;
  }

  int ItemTab::rowY(double y) const
  {
    if (itemVector.empty() || rowTopMargin.empty()) return -1;     
    auto p=std::upper_bound(rowTopMargin.begin(), rowTopMargin.end(), (y-offsy));
    size_t r=p-rowTopMargin.begin(); 
    if (r<0) r=-1; // out of bounds, invalidate
    return r;
  }
  
  void ItemTab::moveTo(float x, float y)
  {   
    if (itemFocus) {    
      xItem=x;	                               
      yItem=y;
      assert(abs(x-xItem)<1 && abs(y-yItem)<1);
    }
  }  
  
  ItemTab::ClickType ItemTab::clickType(double x, double y) const
  {
    int c=colX(x), r=rowY(y);

    if (c>=0 && c<int(colLeftMargin.size())&& r>=0 && r<int(itemVector.size()))
      return internal;
        
    if (itemFocus) return internal;   
  
    return background;
  }
      
  void ItemTab::mouseDownCommon(float x, float y)
  {
    if (itemFocus=itemAt(x,y))
      switch (clickType(x,y))
        {
        case internal:
          moveOffsX=x-itemCoords[itemFocus].first;
          moveOffsY=y-itemCoords[itemFocus].second;
          break;
        case background:
          itemFocus.reset();
          break;
        default:
          break;  
        }
  }  
  
  void ItemTab::mouseUp(float x, float y)
  {
    mouseMove(x,y);
    itemFocus.reset();
  }
  
  void ItemTab::mouseMove(float x, float y)
  {
	    
    try
      {
        if (itemFocus)
          switch (clickType(x,y))
            {
            case internal:
              moveTo(x-moveOffsX,y-moveOffsY);
              requestRedraw();
              return;
            default:
              break;
            }
      }
    catch (...) {/* absorb any exceptions, as they're not useful here */}  
  }
  
  void ItemTab::displayDelayedTooltip(float x, float y)
  {
    if (auto item=itemAt(x,y))
      {
        item->displayDelayedTooltip(x,y);
        requestRedraw();
      }
  } 
  
  ItemPtr ItemTab::itemAt(float x, float y)
  {
    ItemPtr item;                    
    auto minD=numeric_limits<float>::max();
    for (auto& i: itemCoords)
      {
        float xx=(i.second).first+offsx, yy=(i.second).second+offsy;  
        float d=sqr(xx-x)+sqr(yy-y);
        float z=i.first->zoomFactor();
        float w=0.5*i.first->iWidth()*z,h=0.5*i.first->iHeight()*z;
        // improve grabbing of Godley tables on the tab.
        if (auto g=dynamic_pointer_cast<GodleyIcon>(i.first))
          {
            GodleyTableWindow godley(g);
            ecolab::cairo::Surface surf
              (cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL));			
            try
              {
                godley.draw(surf.cairo());
              }
            catch (const std::exception& e) 
              {cerr<<"illegal exception caught in draw(): "<<e.what()<<endl;}
            catch (...) {cerr<<"illegal exception caught in draw()";}
            w=0.5*godley.colLeftMargin[godley.colLeftMargin.size()-1];
            h=0.5*(godley.godleyIcon->table.rows())*godley.rowHeight;
            xx+=w;
            yy+=h;
            d=sqr(xx-x)+sqr(yy-y);
          }
        if (d<minD && fabs(xx-x)<w && fabs(yy-y)<h)
          {
            minD=d;
            item=i.first;
          }
      }
    
    return item;
  }
  
  void ItemTab::togglePlotDisplay() const      
  {
    if (auto p=itemFocus->plotWidgetCast()) p->togglePlotTabDisplay();
    else return;
  }
  
  namespace
  {
 
    std::string definition(const VariableBase& v)
    {
      SystemOfEquations system(cminsky());	  
      ostringstream o;
	
      auto varDAG=system.getNodeFromVar(v);
    
      if (v.type()!=VariableType::parameter)
        {
          if (varDAG && varDAG->rhs && varDAG->type!=VariableType::constant && varDAG->type!=VariableType::integral)
            o << varDAG->rhs->matlab();
	  else return system.getDefFromIntVar(v).str();
        }
          
      return o.str();	  
    }
  }  
	
  void ItemTab::draw(cairo_t* cairo)
  {   
    try
      {	
      		
        if (!itemVector.empty())
          {
            float x0, y0=1.5*rowHeight;//+pango.height();	
            double w=0,h=0,h_prev,lh; 
            colLeftMargin.clear();                
            rowTopMargin.clear();
            int iC=0;                
            for (auto& it: itemVector)
              {
                if (auto v=it->variableCast())
                  {
                    auto value=v->vValue();
                    auto rank=value->hypercube().rank();
                    auto dims=value->hypercube().dims();                
                    Pango pango(cairo);      
                    x0=0.0;
                    float x=x0, y=y0;
                    pango.setMarkup("9999");
                    if (rank==0)
                      { 
                        varAttribVals.clear();
                        varAttribVals.push_back(v->name());
                        varAttribVals.push_back(definition(*v));                    
                        varAttribVals.push_back(v->init());
                        varAttribVals.push_back(it->tooltip);
                        varAttribVals.push_back(it->detailedText);
                        varAttribVals.push_back(to_string(v->sliderStep));
                        varAttribVals.push_back(to_string(v->sliderMin));
                        varAttribVals.push_back(to_string(v->sliderMax));
                        varAttribVals.push_back(to_string(v->value()));
                    
                        for (auto& i:varAttrib) 
                          {
                            cairo_move_to(cairo,x,y-1.5*rowHeight);                    
                            pango.setMarkup(i);
                            pango.show();                  
                            colWidth=std::max(colWidth,5+pango.width());  
                            x+=colWidth;	
                            colLeftMargin[iC].push_back(x);                        				    
                          }
                        x=0;
                        for (auto& i : varAttribVals)
                          {
                            cairo_move_to(cairo,x,y-0.5*rowHeight);                    
                            pango.setMarkup(latexToPango(i));
                            pango.show();                    
                            colWidth=std::max(colWidth,5+pango.width());
                            x+=colWidth;		
                          }
                        x=x0;                      
                        h_prev=h;
                        w=0;h=0;      
                        cairo_get_current_point (cairo,&w,&h);   
                        if (h<h_prev) h+=h_prev;                                                                         
                        // draw grid
                        {
				      		
                          cairo::CairoSave cs(cairo);
                          cairo_set_source_rgba(cairo,0,0,0,0.2);
                          for (y=y0-1.5*rowHeight; y<h+rowHeight; y+=2*rowHeight)
                            {
                              cairo_rectangle(cairo,x0,y,w+colWidth,rowHeight);
                              cairo_fill(cairo);
                            }

                        }
                        { // draw vertical grid lines
                          cairo::CairoSave cs(cairo);
                          cairo_set_source_rgba(cairo,0,0,0,0.5);
                          for (x=x0; x<w+colWidth; x+=colWidth)
                            {
                              cairo_move_to(cairo,x,y-2*rowHeight);
                              cairo_line_to(cairo,x,y+0.5*rowHeight);
                              cairo_stroke(cairo);
                            }
                        }                                            
                        { // draw horizontal grid line
                          cairo::CairoSave cs(cairo);
                          cairo_set_source_rgba(cairo,0,0,0,0.5);
                          cairo_move_to(cairo,x0,y0-0.5*rowHeight);
                          cairo_line_to(cairo,w+colWidth,y0-0.5*rowHeight);
                          cairo_stroke(cairo);
                        }                                  
                        cairo::CairoSave cs(cairo);
                        // make sure rectangle has right height
                        cairo_rectangle(cairo,x0,y0-1.5*rowHeight,w+colWidth,y-y0+2*rowHeight);    
                        rowTopMargin.push_back(y);
                        cairo_stroke(cairo);                          	          
                        cairo_clip(cairo);	                               
                      }
                    else if (rank==1)
                      {
                        cairo_move_to(cairo,x,y-1.5*rowHeight);
                        pango.setMarkup(latexToPango(value->name)+":");
                        pango.show();                                  
                        string format=value->hypercube().xvectors[0].dimension.units;
                        for (auto& i: value->hypercube().xvectors[0])
                          {
                            cairo_move_to(cairo,x,y);
                            pango.setText(trimWS(str(i,format)));
                            pango.show();
                            y+=rowHeight;
                            colWidth=std::max(colWidth,5+pango.width());
                          }
                        insertCol(size_t(rank));                            
                        colLeftMargin[iC].push_back(x);                         
                        y=y0;
                        lh=0;                        
                        for (size_t j=0; j<dims[0]; ++j)
                          lh+=rowHeight;                    
                        { // draw vertical grid line
                          cairo::CairoSave cs(cairo);
                          cairo_set_source_rgba(cairo,0,0,0,0.5);
                          cairo_move_to(cairo,colWidth-2.5,y0);
                          cairo_line_to(cairo,colWidth-2.5,y0+lh);
                          cairo_stroke(cairo);
                        }                                       
                        x+=colWidth;
                        for (size_t i=0; i<value->size(); ++i)
                          {
                            if (!value->index().empty())
                              y=y0+value->index()[i]*rowHeight;
                            cairo_move_to(cairo,x,y);
                            auto v=value->value(i);
                            insertRow(i+1);                            
                            if (!std::isnan(v))
                              {
                                pango.setMarkup(str(v));
                                pango.show();
                              }
                            y+=rowHeight;
                          }
                        resize(value->size(),size_t(rank));                             
                        h_prev=h;
                        w=0;h=0;      
                        cairo_get_current_point (cairo,&w,&h);   
                        if (h<h_prev) h+=h_prev;                                                                        
                        // draw grid
                        {
                          cairo::CairoSave cs(cairo);
                          cairo_set_source_rgba(cairo,0,0,0,0.2);
                          for (y=y0+rowHeight; y<h+rowHeight; y+=2*rowHeight)
                            {
                              cairo_rectangle(cairo,0.0,y,w+colWidth,rowHeight);
                              cairo_fill(cairo);
                            }
                        }
                        cairo::CairoSave cs(cairo);
                        float rectHeight=0;
                        // make sure rectangle has right height
                        if ((value->size()&1)!=0) rectHeight= y-y0;
                        else rectHeight=y-y0-rowHeight;                    
                        cairo_rectangle(cairo,0.0,y0,w+colWidth,rectHeight);    
                        cairo_stroke(cairo);                          
                        cairo_clip(cairo);             
                        rowTopMargin.push_back(y);                    
                        colLeftMargin[iC].push_back(x);                   
                        y0=h+3.1*rowHeight;                 
                      }
                    else
                      { 
                        cairo_move_to(cairo,x,y-1.5*rowHeight);
                        pango.setMarkup(latexToPango(value->name)+":");
                        pango.show();                
                        size_t labelDim1=0, labelDim2=1; 					    
                        string vName;
                        if (v->type()==VariableType::parameter)
                          for (size_t k=0; k<rank; k++)  
                            {
                              vName=static_cast<string>(value->hypercube().xvectors[k].name);
                              if (v->getDimLabelsPicked().first==vName) labelDim1=k;
                              if (v->getDimLabelsPicked().second==vName) labelDim2=k;
                              else if (v->getDimLabelsPicked().second=="") labelDim2=labelDim1+1;
                            }
						
                        if ((labelDim1&1)==0) y+=rowHeight; // allow room for header row
                        string format=value->hypercube().xvectors[labelDim1].dimension.units;
                        for (auto& i: value->hypercube().xvectors[labelDim1])
                          {
                            cairo_move_to(cairo,x,y);
                            pango.setText(trimWS(str(i,format)));
                            pango.show();
                            y+=rowHeight;
                            colWidth=std::max(colWidth,5+pango.width());
                          }                                             
                        y=y0;  
                        x+=colWidth;
                        lh=0;                 
                        for (size_t j=0; j<dims[labelDim1]; ++j)
                          lh+=rowHeight;                         
                        format=value->hypercube().xvectors[labelDim2].timeFormat();
                        for (size_t i=0; i<dims[labelDim2]; ++i)
                          {
                            y=y0;
                            cairo_move_to(cairo,x,y);
                            pango.setText(trimWS(str(value->hypercube().xvectors[labelDim2][i],format)));
                            pango.show();
                            { // draw vertical grid line
                              cairo::CairoSave cs(cairo);
                              cairo_set_source_rgba(cairo,0,0,0,0.5);
                              cairo_move_to(cairo,x-2.5,y0);
                              cairo_line_to(cairo,x-2.5,y0+lh+1.1*rowHeight);
                              cairo_stroke(cairo);
                            }
                            colWidth=std::max(colWidth, 5+pango.width());
                            for (size_t j=0; j<dims[labelDim1]; ++j)
                              {
                                y+=rowHeight;
                                if (y>2e09) break;
                                cairo_move_to(cairo,x,y);
                                auto v=value->atHCIndex(j+i*dims[labelDim1]);
                                if (!std::isnan(v))
                                  {
                                    pango.setText(str(v));
                                    pango.show();
                                  }
                                colWidth=std::max(colWidth, pango.width());
                              }
                            colLeftMargin[iC].push_back(x);                     
                            x+=colWidth;
                            if (x>2e09) break;
                          }      
                        h_prev=h;
                        w=0;h=0;      
                        cairo_get_current_point (cairo,&w,&h);   
                        if (h<h_prev) h+=h_prev;                                                                         
                        // draw grid
                        {
				      		
                          cairo::CairoSave cs(cairo);
                          cairo_set_source_rgba(cairo,0,0,0,0.2);
                          for (y=y0+rowHeight; y<h+rowHeight; y+=2*rowHeight)
                            {
                              cairo_rectangle(cairo,x0,y,w+colWidth,rowHeight);
                              cairo_fill(cairo);
                            }
                        }
                        { // draw horizontal grid line
                          cairo::CairoSave cs(cairo);
                          cairo_set_source_rgba(cairo,0,0,0,0.5);
                          cairo_move_to(cairo,x0,y0+1.1*rowHeight);
                          cairo_line_to(cairo,w+colWidth,y0+1.1*rowHeight);
                          cairo_stroke(cairo);
                        }                         
                        cairo::CairoSave cs(cairo);
                        float rectHeight=0;
                        // make sure rectangle has right height
                        if ((labelDim1&1)==0) rectHeight= y-y0;
                        else rectHeight=y-y0-rowHeight;
                        cairo_rectangle(cairo,x0,y0,w+colWidth,rectHeight);    
                        cairo_stroke(cairo);                          	        
                        cairo_clip(cairo);	
                        rowTopMargin.push_back(y);    	        
                        x+=0.25*colWidth;      
                        y=y0;                	
			
					
                      }               
                    if (rank>0) y0=h+4.1*rowHeight;
                    else y0+=4.1*rowHeight;   
                    iC++;
                    
                    // indicate cell mouse is hovering over
                    if ((hoverRow>0 || hoverCol>0) &&                                
                        size_t(hoverRow)<rows() &&
                        size_t(hoverCol)<cols())
                      {
                        CairoSave cs(cairo);
                        cairo_rectangle(cairo,
                                        colLeftMargin[iC-1][hoverCol],hoverRow*rowHeight+topTableOffset,
                                        colLeftMargin[iC-1][hoverCol+1]-colLeftMargin[iC-1][hoverCol],rowHeight);
                        cairo_set_line_width(cairo,1);
                        cairo_stroke(cairo);
                      }
                        
                    // indicate selected cells
                    {
                      CairoSave cs(cairo);
                      if (selectedRow==0 || (selectedRow>=int(scrollRowStart) && selectedRow<int(rows())))
                        {
                          size_t i=0, j=0;
                          if (selectedRow>=int(scrollRowStart)) j=selectedRow-scrollRowStart+1;
                          double y1=j*rowHeight+topTableOffset;
	                
                          if (selectedCol==0 || /* selecting individual cell */
                                   (selectedCol>=int(scrollColStart) && selectedCol<int(cols())))   
                            {
                              if ((selectedRow>1 || selectedRow <0) || selectedCol!=0) // can't select flows/stockVars or Initial Conditions labels
                                {
                                  if (selectedCol>=int(scrollColStart)) i=selectedCol-scrollColStart+1;
                                  double x1=colLeftMargin[iC-1][i];
                                  cairo_set_source_rgba(cairo,1,1,1,1);
                                  cairo_rectangle(cairo,x1,y1,colLeftMargin[iC-1][i+1]-x1,rowHeight);
                                  cairo_fill(cairo);
                                  pango.setMarkup(defang(cell(selectedRow,selectedCol)));
                                  cairo_set_source_rgba(cairo,0,0,0,1);
                                  cairo_move_to(cairo,x1,y1);
                                  pango.show();
	                			  
                                  // show insertion cursor
                                  cairo_move_to(cairo,x1+pango.idxToPos(insertIdx),y1);
                                  cairo_rel_line_to(cairo,0,rowHeight);
                                  cairo_set_line_width(cairo,1);
                                  cairo_stroke(cairo);
                                  if (motionRow>0 && motionCol>0)
                                    highlightCell(cairo,motionRow,motionCol);
                                  if (selectIdx!=insertIdx)
                                    {
                                      // indicate some text has been selected
                                      cairo_rectangle(cairo,x1+pango.idxToPos(insertIdx),y1,
                                                      pango.idxToPos(selectIdx)-pango.idxToPos(insertIdx),rowHeight);
                                      cairo_set_source_rgba(cairo,0.5,0.5,0.5,0.5);
                                      cairo_fill(cairo);
                                   }
                                }
                            }
                        }
                    }                                  
                  }
                else if (auto p=it->plotWidgetCast())
                  {
                    cairo::CairoSave cs(cairo);   
                    if (it==itemFocus) {
                      cairo_translate(cairo,xItem,yItem);  		    				   
                      itemCoords.erase(itemFocus);   
                      itemCoords.emplace(make_pair(itemFocus,make_pair(xItem,yItem)));
                    } else cairo_translate(cairo,itemCoords[it].first,itemCoords[it].second);      
                    p->draw(cairo);
                  }
                else if (auto g=dynamic_pointer_cast<GodleyIcon>(it))
                  {
                    cairo::CairoSave cs(cairo);   
                    if (it==itemFocus) {
                      cairo_translate(cairo,xItem,yItem);  		    				   
                      itemCoords.erase(itemFocus);   
                      itemCoords.emplace(make_pair(itemFocus,make_pair(xItem,yItem)));         
                    } else cairo_translate(cairo,itemCoords[it].first,itemCoords[it].second);
                    GodleyTableWindow godley(g);
                    godley.disableButtons();
                    godley.displayValues=true;
                    godley.draw(cairo);
                    
                    // draw title
                    if (!g->table.title.empty())
                      {
                        CairoSave cs(cairo);
                        Pango pango(cairo);
                        pango.setMarkup("<b>"+latexToPango(g->table.title)+"</b>");
                        pango.setFontSize(12);
                        cairo_move_to(cairo,0.5*godley.colLeftMargin[godley.colLeftMargin.size()-1],godley.topTableOffset-2*godley.rowHeight);
                        pango.show();
                      }   
                       
                  }			   
              }              
          }
      }
    catch (...) {throw;/* exception most likely invalid variable value */}
  }

  namespace
  {    
    struct CroppedPango: public Pango
    {
      cairo_t* cairo;
      double w, x=0, y=0;
      CroppedPango(cairo_t* cairo, double width): Pango(cairo), cairo(cairo), w(width) {}
      void setxy(double xx, double yy) {x=xx; y=yy;}
      void show() {
        CairoSave cs(cairo);
        cairo_rectangle(cairo,x,y,w,height());
        cairo_clip(cairo);
        cairo_move_to(cairo,x,y);
        Pango::show();
      }
    };
  }

  void ItemTab::redraw(int, int, int width, int height)
  {
    if (surface.get()) {
      cairo_t* cairo=surface->cairo();  
      CroppedPango pango(cairo, colWidth);
      rowHeight=15;
      pango.setFontSize(5.0*rowHeight);
	    
      if (!minsky().canvas.model->empty()) {	  
        populateItemVector();			               
        cairo_translate(cairo,offsx,offsy); 
        draw(cairo); 
        ecolab::cairo::Surface surf
          (cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL));            
        draw(surf.cairo());      
        m_width=surf.width();
        m_height=surf.height();
        //cout << m_width << " " << m_height << " " << offsx << " " << offsy << " " << rows() << " " << cols() << " " << hoverRow << " " << hoverCol << endl;
        //for (size_t i=0; i< colLeftMargin.size(); i++) cout << " " << colLeftMargin[i];
        //cout << endl;
        //if (((fabs(offsx)>1.5*m_width) || (offsx<0 && fabs(offsx)>600)) || ((fabs(offsy)>1.5*m_height) || (offsy<0 && fabs(offsy)>800))) cairo_translate(surf.cairo(),0.0,0.0);            
      }     
    }
  }
  
  int ItemTab::textIdx(double x) const
  {
    cairo::Surface surf(cairo_recording_surface_create(CAIRO_CONTENT_COLOR,NULL));
    Pango pango(surf.cairo());
    if (selectedRow>=0 && size_t(selectedRow)<rows() &&
        selectedCol>=0 && size_t(selectedCol)<cols() && (selectedRow!=1 || selectedCol!=0)) // No text index needed for a cell that is immutable. For ticket 1064
      {
        auto& str=cell(selectedRow,selectedCol);
        pango.setMarkup(defang(str));
        int j=0;
        if (selectedCol>=int(scrollColStart)) j=selectedCol-scrollColStart+1;
        x-=colLeftMargin.at(selectedRow)[j]+2;
        return x>0 && str.length()>0?pango.posToIdx(x)+1: 0;  
      }
    return 0;
  }

//  void ItemTab::mouseDown(double x, double y)
//  {
//     // catch exception, as the intention here is to allow the user to fix a problem
//     try {update();}
//     catch (...) {}
//     selectedCol=colX(x);
//     selectedRow=rowY(y);
//     if (selectedRow>=0 && selectedRow<int(rows()) &&
//         selectedCol>=0 && selectedCol<int(cols()) && (selectedRow!=1 || selectedCol!=0)) // Cannot save text in cell(1,0). For ticket 1064
//        {
//          selectIdx=insertIdx = textIdx(x);
//          savedText=cell(selectedRow, selectedCol);
//        }
//     else
//       selectIdx=insertIdx=0;
//  }

//  void ItemTab::mouseUp(double x, double y)
//  {
//    int c=colX(x), r=rowY(y);
//    motionRow=motionCol=-1;
//    // Cannot swap cell(1,0) with another. For ticket 1064. Also cannot move cells outside an existing Godley table to create new rows or columns. For ticket 1066. 
//    //if ((selectedCol==0 && selectedRow==1) || (c==0 && r==1) || size_t(selectedRow)>=(godleyIcon->table.rows()) || size_t(r)>=(godleyIcon->table.rows()) || size_t(c)>=(godleyIcon->table.cols()) || size_t(selectedCol)>=(godleyIcon->table.cols()))
//    //  return;  
//    //else if (selectedRow==0)
//    //  {  
//	//	// Disallow moving flow labels column and prevent columns from moving when import stockvar dropdown button is pressed in empty column. For tickets 1053/1064/1066
//    //    if (c>0 && size_t(c)<godleyIcon->table.cols() && selectedCol>0 && size_t(selectedCol)<godleyIcon->table.cols() && c!=selectedCol && !(colLeftMargin[c+1]-x < pulldownHot)) 
//    //      godleyIcon->table.moveCol(selectedCol,c-selectedCol);
//    //  }
//    //else if (r>0 && selectedCol==0)
//    //  {
//    //    if (r!=selectedRow && !godleyIcon->table.initialConditionRow(selectedRow) && !godleyIcon->table.initialConditionRow(r))  // Cannot move Intitial Conditions row. For ticket 1064.
//    //      godleyIcon->table.moveRow(selectedRow,r-selectedRow);
//    //  } 
//    //else if ((c!=selectedCol || r!=selectedRow) && c>0 && r>0)
//    //  {
//    //    swap(godleyIcon->table.cell(selectedRow,selectedCol), godleyIcon->table.cell(r,c));
//    //    selectedCol=c;
//    //    selectedRow=r;
//    //  }
//    //else 
//    if (selectIdx!=insertIdx)
//      copy();
//  }

//  void ItemTab::mouseMoveB1(double x, double y)
//  {
//    motionCol=colX(x), motionRow=rowY(y);
//    if (motionCol==selectedCol && motionRow==selectedRow)
//      selectIdx=textIdx(x);
//  }

//  void ItemTab::mouseMove(double x, double y)
//  {
//    hoverRow=hoverCol=-1;
//    switch (clickType(x,y))
//      {
//      case background:
//        break;
//      default:
//        hoverRow=rowY(y);
//        hoverCol=colX(x);
//        break;
//      }  
//  }

  inline constexpr char control(char x) {return x-'`';}
  
  void ItemTab::keyPress(int keySym, const std::string& utf8)
  {

    if (selectedCol>=0 && selectedRow>=0 && selectedCol<int(cols()) &&
        selectedRow<int(rows()) && (selectedCol!=0 || selectedRow!=1)) // Cell (1,0) is off-limits. For ticket 1064
          {			  	  
            auto& str=cell(selectedRow,selectedCol);
            if (utf8.length() && (keySym<0x7f || (0xffaa <= keySym && keySym <= 0xffbf)))  // Enable numeric keypad key presses. For ticket 1136
              // all printing and control characters have keysym
              // <0x80. But some keys (eg tab, backspace and escape
              // are mapped to control characters
              if (unsigned(utf8[0])>=' ' && utf8[0]!=0x7f)
                {
                  delSelection();
                  if (insertIdx>=str.length()) insertIdx=str.length();
                  str.insert(insertIdx,utf8);
                  selectIdx=insertIdx+=utf8.length();
                }
              else
                {
                  switch (utf8[0]) // process control characters
                    {
                    case control('x'):
                      cut();
                      break;
                    case control('c'):
                      copy();
                      break;
                    case control('v'):
                      paste();
                      break;
                    case control('h'): case 0x7f:
                      handleDelete();
                      break;
                    }
                }
            else
              {
              switch (keySym)
                {
                case 0xff08: // backspace
		          handleBackspace();
		          break;
		        case 0xffff:  // delete
                  handleDelete();
                  break;
                case 0xff1b: // escape
                  if (selectedRow>=0 && size_t(rows()) &&
                      selectedCol>=0 && size_t(cols()))
                    cell(selectedRow, selectedCol)=savedText;
                  selectedRow=selectedCol=-1;
                  break;
                case 0xff0d: //return
                case 0xff8d: //enter added for ticket 1122                            
                  update();
                  selectedRow=selectedCol=-1;                  
                  break;     
                case 0xff51: //left arrow
                  if (insertIdx>0) insertIdx--;
                  else navigateLeft();
                  break;
                case 0xff53: //right arrow
                  if (insertIdx<str.length()) insertIdx++;
                  else navigateRight();
                  break;
                case 0xff09: // tab
                  navigateRight();
                  break;
                case 0xfe20: // back tab
                  navigateLeft();
                  break;
                case 0xff54: // down
                  navigateDown();
                  break;
                case 0xff52: // up
                  navigateUp();
                  break;
                default:
                  return; // key not handled, just return without resetting selection
                }
                selectIdx=insertIdx;
              }
          }  
    else // nothing selected
      {
        // if one of the navigation keys pressed, move to the first/last etc cell
        switch (keySym)
          {
          case 0xff09: case 0xff53: // tab, right
            selectedRow=0; selectedCol=1; break;
          case 0xfe20: // back tab
            selectedRow=rows()-1; selectedCol=cols()-1; break;
          case 0xff51: //left arrow
            selectedRow=0; selectedCol=cols()-1; break;
          case 0xff54: // down
            selectedRow=2; selectedCol=0; break;           // Start from second row because Initial Conditions cell (1,0) can no longer be selected. For ticket 1064
          case 0xff52: // up
            selectedRow=rows()-1; selectedCol=0; break;
          default:
            return; // early return, no need to redraw
          }
      }
  }

  void ItemTab::delSelection()
  {
    if (insertIdx!=selectIdx)
      {
        auto& str=cell(selectedRow,selectedCol);
        str.erase(min(insertIdx,selectIdx),abs(int(insertIdx)-int(selectIdx))); 
        selectIdx=insertIdx=min(insertIdx,selectIdx);   
      }
  }

  void ItemTab::handleBackspace()
    {
      assert(selectedRow>=0 && selectedCol>=0);
      assert(unsigned(selectedRow)<rows());
      assert(unsigned(selectedCol)<cols());
      auto& str=cell(selectedRow,selectedCol);
      if (insertIdx!=selectIdx)
        delSelection();
      else if (insertIdx>0 && insertIdx<=str.length())
        str.erase(--insertIdx,1);
      selectIdx=insertIdx;
    }

  void ItemTab::handleDelete()
    {
      assert(selectedRow>=0 && selectedCol>=0);
      assert(unsigned(selectedRow)<rows());
      assert(unsigned(selectedCol)<cols());
      auto& str=cell(selectedRow,selectedCol); 
      if (insertIdx!=selectIdx)
        delSelection();
      else if (insertIdx>=0 && insertIdx<str.length())
        str.erase(insertIdx,1);
      selectIdx=insertIdx;
    }

  void ItemTab::cut()
  {
    copy();
    if (selectedCol>=0 && selectedRow>=0 && selectedCol<int(cols()) &&
        selectedRow<int(rows()))
      {	  
         if (selectIdx==insertIdx)
           // delete entire cell
           cell(selectedRow,selectedCol).clear();
         else
           delSelection();
      }
  }
  
  void ItemTab::copy()
  {
    if (selectedCol>=0 && selectedRow>=0 && selectedCol<int(cols()) &&
        selectedRow<int(rows()))
      {	  
         auto& str=cell(selectedRow,selectedCol);
         if (selectIdx!=insertIdx)
           cminsky().putClipboard
             (str.substr(min(selectIdx,insertIdx), abs(int(selectIdx)-int(insertIdx))));
         else
           cminsky().putClipboard(str);  
      }
  }

  void ItemTab::paste()
  {
    if (selectedCol>=0 && selectedRow>=0 && selectedCol<int(cols()) &&
        selectedRow<int(rows()))
      {
	     delSelection();
         auto& str=cell(selectedRow,selectedCol); 
         auto stringToInsert=cminsky().getClipboard();
         // only insert first line
         auto p=stringToInsert.find('\n');
         if (p!=string::npos)
           stringToInsert=stringToInsert.substr(0,p-1);
         str.insert(insertIdx,stringToInsert);
         selectIdx=insertIdx+=stringToInsert.length();
      }
  }
  
  void ItemTab::highlightCell(cairo_t* cairo, unsigned row, unsigned col)
  {
    if (row<scrollRowStart || col<scrollColStart) return;
    double x=colLeftMargin[row][col-scrollColStart+1];
    double width=colLeftMargin[row][col-scrollColStart+2]-x;
    double y=(row-scrollRowStart+1)*rowHeight+topTableOffset;
    cairo_rectangle(cairo,x,y,width,rowHeight);
    cairo_set_source_rgba(cairo,1,1,1,0.5);
    cairo_fill(cairo);
  }

  void ItemTab::pushHistory(ItemPtr i)
  {
    while (history.size()>maxHistory) history.pop_front();
    // Perform deep comparison of Godley tables in history to avoid spurious noAssetClass columns from arising during undo. For ticket 1118.
    if (history.empty() || !(history.back()==i)) {
      history.push_back(i);
    }
    historyPtr=history.size();
  }
      
  void ItemTab::undo(int changes, ItemPtr i)
  { 
    if (historyPtr==history.size())
      pushHistory(i);
    historyPtr-=changes;
    if (historyPtr > 0 && historyPtr <= history.size())
      {
        auto& d=history[historyPtr-1];
        // Perform deep comparison of Godley tables in history to avoid spurious noAssetClass columns from arising during undo. For ticket 1118.
        if (!i->variableCast()->vValue()) return; // should not happen
		i=d; 
      }
  }
  
  void ItemTab::update()
  {
    // if the contents of the cell are cleared, set the cell to "0". For #1181
    //if (!savedText.empty() && cell(selectedRow,selectedCol).empty())
    //      cell(selectedRow,selectedCol)="0";

    requestRedraw();
  }  
  
  
  
  void ItemTab::checkCell00()
  {
    if (selectedCol==0 && (selectedRow==0 || selectedRow ==1))
      // (0,0) cell not editable
      {
        selectedCol=-1;
        selectedRow=-1;
      }         
  }
  
    void ItemTab::navigateRight()
    {
      if (selectedCol>=0)
        {
          selectedCol++;
          insertIdx=0;
          if (selectedCol>=int(cols()))
            {
              if (selectedRow>0) selectedCol=0;   // Minor fix: Make sure tabbing and right arrow traverse all editable cells.
              else selectedCol=1;
              navigateDown();
            }
          checkCell00();
        }
    }
  
    void ItemTab::navigateLeft()
    {
      if (selectedCol>=0)
        {
          selectedCol--;
          insertIdx=0;
          if (selectedCol<0)
            {
              selectedCol=cols()-1;
              navigateUp();
            }
          checkCell00();
        }
    }

    void ItemTab::navigateUp()
    {
      if (selectedRow>=0)
        selectedRow=(selectedRow-1)%rows();
      checkCell00();
    }
  
    void ItemTab::navigateDown()
    {
      if (selectedRow>=0)
        selectedRow=(selectedRow+1)%rows();
      checkCell00();
    }             

}
