/*
  @copyright Steve Keen 2020
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

#include <classdesc.h>
#include "minskyTensorOps.h"
#include "minsky.h"
#include "ravelWrap.h"
#include "minsky_epilogue.h"
static const int ravelVersion=2;

#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace civita;
namespace classdesc
{
  // postpone factory definition to TensorOpFactory()
  template <> Factory<ITensor, minsky::OperationType::Type>::Factory() {}
}


namespace minsky
{
  using namespace classdesc;

  TensorOpFactory tensorOpFactory;

  struct DerivativeNotDefined: public std::exception
  {
    const char* what() const throw() {return "Derivative not defined";}
  };
  
  struct TimeOp: public ITensor
  {
    size_t size() const override {return 1;}
    vector<size_t> index() const override {return {};}
    double operator[](size_t) const override {return EvalOpBase::t;}
    Timestamp timestamp() const override {return {};}
  };
  
  // Default template calls the regular legacy double function
  template <OperationType::Type op> struct MinskyTensorOp: public civita::ElementWiseOp, public DerivativeMixin
  {
    EvalOp<op> eo;
    MinskyTensorOp(): ElementWiseOp([this](double x){return eo.evaluate(x);}) {}
    void setArguments(const std::vector<TensorPtr>& a,const std::string&,double) override
    {if (!a.empty()) setArgument(a[0],{},0);}
    double dFlow(size_t ti, size_t fi) const override {
      auto deriv=dynamic_cast<DerivativeMixin*>(arg.get());
      if (!deriv) throw DerivativeNotDefined();
      if (double df=deriv->dFlow(ti,fi))
        return eo.d1((*arg)[ti])*df;
      return 0;
    }
    double dStock(size_t ti, size_t si) const override {
      auto deriv=dynamic_cast<DerivativeMixin*>(arg.get());
      if (!deriv) throw DerivativeNotDefined();
      if (double ds=deriv->dStock(ti,si))
        return eo.d1((*arg)[ti])*ds;
      return 0;
    }
  };

  template <OperationType::Type op> struct TensorBinOp: civita::BinOp, public DerivativeMixin
  {
    EvalOp<op> eo;
    TensorBinOp(): BinOp([this](double x,double y){return eo.evaluate(x,y);}) {}
    virtual void setArguments(const std::vector<TensorPtr>& a1, const std::vector<TensorPtr>& a2) override
    {
      civita::BinOp::setArguments
        (a1.empty()? TensorPtr(): a1[0],
         a2.empty()?  TensorPtr(): a2[0]
         );
    }
    double dFlow(size_t ti, size_t fi) const override {
      auto deriv1=dynamic_cast<DerivativeMixin*>(arg1.get());
      auto deriv2=dynamic_cast<DerivativeMixin*>(arg2.get());
      if (!deriv1 || !deriv2) throw DerivativeNotDefined();
      double r=0;
      if (double df=deriv1->dFlow(ti,fi))
        r += eo.d1((*arg1)[ti])*df;
      if (double df=deriv2->dFlow(ti,fi))
        r += eo.d2((*arg2)[ti])*df;
      return r;
    }
    double dStock(size_t ti, size_t si) const override {
      auto deriv1=dynamic_cast<DerivativeMixin*>(arg1.get());
      auto deriv2=dynamic_cast<DerivativeMixin*>(arg2.get());
      if (!deriv1 || !deriv2) throw DerivativeNotDefined();
      double r=0;
      if (double ds=deriv1->dStock(ti,si))
        r += eo.d1((*arg1)[ti])*ds;
      if (double ds=deriv2->dStock(ti,si))
        r += eo.d2((*arg2)[ti])*ds;
      return r;
    }
  };

  template <OperationType::Type op> struct AccumArgs;

  template <> struct AccumArgs<OperationType::add>: public civita::ReduceArguments
  {
    AccumArgs(): civita::ReduceArguments([](double& x,double y){x+=y;},0) {}
  };
  template <> struct AccumArgs<OperationType::subtract>: public AccumArgs<OperationType::add> {};

  template <> struct AccumArgs<OperationType::multiply>: public civita::ReduceArguments
  {
    AccumArgs(): civita::ReduceArguments([](double& x,double y){x*=y;},1) {}
  };
  template <> struct AccumArgs<OperationType::divide>: public AccumArgs<OperationType::multiply> {};

  template <> struct AccumArgs<OperationType::min>: public civita::ReduceArguments
  {
    AccumArgs(): civita::ReduceArguments([](double& x,double y){if (y<x) x=y;},std::numeric_limits<double>::max()) {}
  };
  template <> struct AccumArgs<OperationType::max>: public civita::ReduceArguments
  {
    AccumArgs(): civita::ReduceArguments([](double& x,double y){if (y>x) x=y;},-std::numeric_limits<double>::max()) {}
  };

  template <> struct AccumArgs<OperationType::and_>: public civita::ReduceArguments
  {
    AccumArgs(): civita::ReduceArguments([](double& x,double y){x*=(y>0.5);},1) {}
  };
  template <> struct AccumArgs<OperationType::or_>: public civita::ReduceArguments
  {
    AccumArgs(): civita::ReduceArguments([](double& x,double y){if (y>0.5) x=1;},0) {}
  };

  
  
  template <OperationType::Type op> struct MultiWireBinOp: public TensorBinOp<op>
  {
    virtual void setArguments(const std::vector<TensorPtr>& a1,
                              const std::vector<TensorPtr>& a2)
    {
      auto pa1=make_shared<AccumArgs<op>>(), pa2=make_shared<AccumArgs<op>>();
      pa1->setArguments(a1,{},0); pa2->setArguments(a2,{},0);
      civita::BinOp::setArguments(pa1, pa2);
    }
  };
   
//  template <minsky::OperationType::Type T>
//  struct ReductionTraits
//  {
//    /// x op= y
//    static void accum(double& x, double y);
//    static const double init;
//  };

  template <OperationType::Type op> struct GeneralTensorOp;
  
  //struct RavelTensor;
                                                                                      
  namespace
  {
    template <int I, int J>
    struct is_equal {const static bool value=I==J;};
  }

  //register factory functions for all binary ops
  template <template<OperationType::Type> class T, int op, int to>
  typename classdesc::enable_if<Not<is_equal<op, to>>, void>::T
  registerOps(TensorOpFactory& tensorOpFactory)
  {
    tensorOpFactory.registerType<T<OperationType::Type(op)>>(OperationType::Type(op));
    registerOps<T, op+1, to>(tensorOpFactory);
  }

  template <template<OperationType::Type> class T, int op, int to>
  typename classdesc::enable_if<is_equal<op, to>, void>::T
  registerOps(TensorOpFactory& tensorOpFactory)
  {}  //terminates recursion

  TensorOpFactory::TensorOpFactory()
  {
    registerType<TimeOp>(OperationType::time);
    //registerType<RavelTensor>(OperationType::ravel);
    registerOps<MultiWireBinOp, OperationType::add, OperationType::log>(*this);
    registerOps<TensorBinOp, OperationType::log, OperationType::copy>(*this);
    registerOps<MinskyTensorOp, OperationType::copy, OperationType::sum>(*this);
    registerOps<GeneralTensorOp, OperationType::sum, OperationType::numOps>(*this);
  }
                                                                                    
  template <>
  class GeneralTensorOp<OperationType::sum>: public civita::ReductionOp
  {
  public:
    GeneralTensorOp(): civita::ReductionOp([](double& x, double y,size_t){x+=y;},0){}
  };
  template <>
  class GeneralTensorOp<OperationType::product>: public civita::ReductionOp
  {
  public:
    GeneralTensorOp(): civita::ReductionOp([](double& x, double y,size_t){x*=y;},1){}
  };
  template <>
  class GeneralTensorOp<OperationType::infimum>: public civita::ReductionOp
  {
  public:
    GeneralTensorOp(): civita::ReductionOp([](double& x, double y,size_t){if (y<x) x=y;},std::numeric_limits<double>::max()){}
   };
  template <>
  class GeneralTensorOp<OperationType::supremum>: public civita::ReductionOp
  {
  public:
    GeneralTensorOp(): civita::ReductionOp([](double& x, double y,size_t){if (y>x) x=y;},-std::numeric_limits<double>::max()){}
   };
  template <>
  class GeneralTensorOp<OperationType::any>: public civita::ReductionOp
  {
  public:
    GeneralTensorOp(): civita::ReductionOp([](double& x, double y,size_t){if (y>0.5) x=1;},0){}
   };
  template <>
  class GeneralTensorOp<OperationType::all>: public civita::ReductionOp
  {
  public:
    GeneralTensorOp(): civita::ReductionOp([](double& x, double y,size_t){x*=(y>0.5);},1){}
   };

  template <>
  class GeneralTensorOp<OperationType::runningSum>: public civita::Scan
  {
  public:
    GeneralTensorOp(): civita::Scan([](double& x,double y,size_t){x+=y;}) {}
  };

  template <>
  class GeneralTensorOp<OperationType::runningProduct>: public civita::Scan
  {
  public:
    GeneralTensorOp(): civita::Scan([](double& x,double y,size_t){x*=y;}) {}
  };
  
  template <>
  class GeneralTensorOp<OperationType::difference>: public civita::Scan
  {
    ssize_t delta;
  public:
    GeneralTensorOp(): civita::Scan
                       ([this](double& x,double y,size_t i)
                        {
                          ssize_t t=ssize_t(i)-delta;
                          if (t>=0 && t<ssize_t(arg->size()))
                            x = y-arg->atHCIndex(t);
                        }) {}
    void setArgument(const TensorPtr& a,const std::string& s,double d) override {
      civita::Scan::setArgument(a,s,d);
      delta=d;
      // determine offset in hypercube space
      auto dims=arg->hypercube().dims();
      if (dimension<dims.size())
        for (size_t i=0; i<dimension; ++i)
          delta*=dims[i];
    }
  };
  
  template <>
  class GeneralTensorOp<OperationType::innerProduct>: public civita::CachedTensorOp
  {
    std::shared_ptr<ITensor> arg1, arg2;
    void computeTensor() const override {//TODO
      throw runtime_error("inner product not yet implemented");
    }
    Timestamp timestamp() const override {return max(arg1->timestamp(), arg2->timestamp());}
  };

  template <>
  class GeneralTensorOp<OperationType::outerProduct>: public civita::CachedTensorOp
  {
    std::shared_ptr<ITensor> arg1, arg2;
    void computeTensor() const override {//TODO
      throw runtime_error("outer product not yet implemented");
    }
    Timestamp timestamp() const override {return max(arg1->timestamp(), arg2->timestamp());}
  };

  template <>
  class GeneralTensorOp<OperationType::index>: public civita::CachedTensorOp
  {
    std::shared_ptr<ITensor> arg;
    void computeTensor() const override {
      size_t i=0, j=0;
      for (; i<arg->size(); ++i)
        if ((*arg)[i]>0.5)
          cachedResult[j++]=i;
      for (; j<cachedResult.size(); ++j)
        cachedResult[j]=nan("");
    }
    void setArgument(const TensorPtr& a, const string&,double) override {
      arg=a; cachedResult.index(a->index()); cachedResult.hypercube(a->hypercube());
    }
    
    Timestamp timestamp() const override {return arg->timestamp();}
  };

  template <>
  class GeneralTensorOp<OperationType::gather>: public civita::CachedTensorOp
  {
    std::shared_ptr<ITensor> arg1, arg2;
    void computeTensor() const override
    {
      for (size_t i=0; i<arg2->size(); ++i)
        {
          auto idx=(*arg2)[i];
          if (isfinite(idx))
            {
              if (idx>=0)
                {
                  if (idx==arg1->size()-1)
                    cachedResult[i]=(*arg1)[idx];
                  else if (idx<arg1->size()-1)
                    {
                      double s=idx-floor(idx);
                      cachedResult[i]=(1-s)*(*arg1)[idx]+s*(*arg1)[idx+1];
                    }
                }
              else if (idx>-1)
                cachedResult[i]=(*arg1)[0];
              else
                cachedResult[i]=nan("");
            }
          else
            cachedResult[i]=nan("");
        }              
    }
    Timestamp timestamp() const override {return max(arg1->timestamp(), arg2->timestamp());}
    void setArguments(const TensorPtr& a1, const TensorPtr& a2) override {
      arg1=a1; arg2=a2;
      cachedResult.index(arg2->index());
      cachedResult.hypercube(arg2->hypercube());
    }
      
  };

  template <>
  class GeneralTensorOp<OperationType::supIndex>: public civita::ReductionOp
  {
    double maxValue; // scratch register for holding current max
  public:
    GeneralTensorOp(): civita::ReductionOp
                       ([this](double& r,double x,size_t i){
                          if (i==0 || x>maxValue) {
                            maxValue=x;
                            r=i;
                          }
                        },0) {}
  };
  
  template <>
  class GeneralTensorOp<OperationType::infIndex>: public civita::ReductionOp
  {
    double minValue; // scratch register for holding current min
  public:
    GeneralTensorOp(): civita::ReductionOp
                       ([this](double& r,double x,size_t i){
                          if (i==0 || x<minValue) {
                            minValue=x;
                            r=i;
                          }
                        },0) {}
  };
  
  class SwitchTensor: public ITensor
  {
    size_t m_size=1;
    vector<size_t> m_index;
    vector<TensorPtr> args;
    size_t hcIndex(size_t i) const {
      if (m_index.empty()) return i;
      if (i>=m_index.size()) return numeric_limits<size_t>::max();
      return m_index[i];
    }
  public:
    void setArguments(const std::vector<TensorPtr>& a,const std::string& axis={},double argv=0) override {
      args=a;
      if (args.size()<2)
        hypercube(Hypercube());
      else
        hypercube(args[1]->hypercube());
//      if (!args.empty() && args[0]->rank()!=0)
//        // TODO: feature ticket #36 - extend to conformant selector arg
//        throw runtime_error("tensor value selectors not yet supported");
      
      set<size_t> indices; // collect the union of argument indices
      for (auto& i: args)
        {
          auto ai=i->index();
          indices.insert(ai.begin(), ai.end());
          if (i->size()>1)
            {
              if (m_size==1)
                m_size=i->size();
              else if (m_size!=i->size())
                // TODO - should we check and throw on nonconformat hypercubes?
                throw runtime_error("noconformant tensor arguments in switch");
            }
        }
      m_index.clear();
      m_index.insert(m_index.end(),indices.begin(), indices.end());
      if (!m_index.empty()) m_size=m_index.size();
    }
    vector<size_t> index() const override {return m_index;}
    size_t size() const override {return m_size;}
    Timestamp timestamp() const override {
      Timestamp t;
      for (auto& i: args)
        {
          auto tt=i->timestamp();
          if (tt>t) t=tt;
        }
      return t;
    }
    double operator[](size_t i) const override {
      if (args.size()<2) return nan("");

      double selector=0;
      if (args[0])
        {
          if (args[0]->rank()==0) // scalar selector, so broadcast
            selector = (*args[0])[0];
          else
            selector = args[0]->atHCIndex(hcIndex(i));
        }
      ssize_t idx = selector+1.5; // selector selects between args 1..n
      
      if (idx>0 && idx<int(args.size()))
        {
          if (args[idx]->rank()==0)
            return (*args[idx])[0];
          else
            return args[idx]->atHCIndex(hcIndex(i));
        }
      return nan("");
    }
  };

// Pulled the C interface out of ravelWrap.cc and put it here in the hope all that is required to use tensor interface are correct hypercube indices  
//  namespace
//  {
//    struct InvalidSym {
//      const string symbol;
//      InvalidSym(const string& s): symbol(s) {}
//    };
//
//    struct RavelDataSpec
//    {
//      int nRowAxes=-1; ///< No. rows describing axes
//      int nColAxes=-1; ///< No. cols describing axes
//      int nCommentLines=-1; ///< No. comment header lines
//      char separator=','; ///< field separator character
//    };
//
//    typedef RavelState::HandleState::ReductionOp ReductionOp;
//    typedef RavelState::HandleState::HandleSort HandleSort;
//    
//    struct RavelHandleState
//    {
//      double x=0,y=0; ///< handle tip coordinates (only angle important, not length)
//      size_t sliceIndex=0, sliceMin=0, sliceMax=0;
//      bool collapsed=false, displayFilterCaliper=false;
//      ReductionOp reductionOp=RavelState::HandleState::sum;
//      HandleSort order=RavelState::HandleState::none;
//      RavelHandleState() {}
//      // NB: sliceIndex, sliceMin, sliceMax need to be dealt with separately
//      RavelHandleState(const RavelState::HandleState& s):
//        x(s.x), y(s.y), collapsed(s.collapsed), displayFilterCaliper(s.displayFilterCaliper),
//        reductionOp(s.reductionOp), order(s.order) {}
//      operator RavelState::HandleState() {
//        RavelState::HandleState r;
//        r.x=x; r.y=y;
//        r.collapsed=collapsed; r.displayFilterCaliper=displayFilterCaliper;
//        r.reductionOp=reductionOp; r.order=order;
//        return r;
//      }
//    };
//    
//#ifdef WIN32
//    typedef HINSTANCE libHandle;
//    libHandle loadLibrary(const string& lib)
//    {return LoadLibraryA((lib+".dll").c_str());}
//
//    FARPROC WINAPI dlsym(HMODULE lib, const char* name)
//    {return GetProcAddress(lib,name);}
//
//    void dlclose(HINSTANCE) {}
//
//    const string dlerror() {
//      char msg[1024];
//      FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,nullptr,GetLastError(),0,msg,sizeof(msg),nullptr);
//      return msg;
//    }
//#else
//    typedef void* libHandle;
//    libHandle loadLibrary(const string& lib)
//    {return dlopen((lib+".so").c_str(),RTLD_NOW);}
//#endif
//  
//    //#define ASG_FN_PTR(f,lib) asgFnPointer(f,lib,#f)
//
//    struct RavelLib
//    {
//      libHandle lib;
//      string errorMsg;
//      string versionFound="unavailable";
//      RavelLib(): lib(loadLibrary("libravel"))
//      {
//        if (!lib)
//          {
//            errorMsg=dlerror();
//            return;
//          }
//      
//        auto version=(const char* (*)())dlsym(lib,"ravel_version");
//        auto capi_version=(int (*)())dlsym(lib,"ravel_capi_version");
//        if (version) versionFound=version();
//        if (!version || !capi_version || ravelVersion!=capi_version())
//          { // incompatible API
//            errorMsg="Incompatible libravel dynamic library found";
//            dlclose(lib);
//            lib=nullptr;
//          }
//      }
//      ~RavelLib() {
//        if (lib)
//          dlclose(lib);
//        lib=nullptr;
//      }
//      template <class F>
//      void asgFnPointer(F& f, const char* name)
//      {
//        if (lib)
//          {
//            f=(F)dlsym(lib,name);
//            if (!f)
//              {
//                errorMsg=dlerror();
//                errorMsg+="\nLooking for ";
//                errorMsg+=name;
//                errorMsg+="\nProbably libravel dynamic library is too old";
//                dlclose(lib);
//                lib=nullptr;
//              }
//          }
//      }
//    };
//
//    RavelLib ravelLib;
//
//    template <class... T> struct RavelFn;
//    
//    template <class R>
//    struct RavelFn<R>
//    {
//      R (*f)()=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      R operator()() {return f? f(): R();}
//    };
//    template <>
//    struct RavelFn<void>
//    {
//      void (*f)()=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      void operator()() {if (f) f();}
//    };
//    template <class R, class A>
//    struct RavelFn<R,A>
//    {
//      R (*f)(A)=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      R operator()(A a) {return f? f(a): R{};}
//    };
//    template <class A>
//    struct RavelFn<void,A>
//    {
//      void (*f)(A)=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      void operator()(A a) {if (f) f(a);}
//    };
//    template <class R, class A0, class A1>
//    struct RavelFn<R,A0,A1>
//    {
//      R (*f)(A0,A1)=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      R operator()(A0 a0, A1 a1) {return f? f(a0,a1): R{};}
//    };
//    template <class A0, class A1>
//    struct RavelFn<void,A0,A1>
//    {
//      void (*f)(A0,A1)=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      void operator()(A0 a0, A1 a1) {if (f) f(a0,a1);}
//    };
//    template <class R, class A0, class A1, class A2>
//    struct RavelFn<R,A0,A1,A2>
//    {
//      R (*f)(A0,A1,A2)=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      R operator()(A0 a0, A1 a1,A2 a2) {return f? f(a0,a1,a2): R{};}
//    };
//    template <class A0, class A1, class A2>
//    struct RavelFn<void,A0,A1,A2>
//    {
//      void (*f)(A0,A1,A2)=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      void operator()(A0 a0, A1 a1, A2 a2) {if (f) f(a0,a1,a2);}
//    };
//    template <class R, class A0, class A1, class A2, class A3>
//    struct RavelFn<R,A0,A1,A2,A3>
//    {
//      R (*f)(A0,A1,A2,A3)=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      R operator()(A0 a0, A1 a1,A2 a2,A3 a3) {return f? f(a0,a1,a2,a3): R{};}
//    };
//    template <class A0, class A1, class A2,class A3>
//    struct RavelFn<void,A0,A1,A2,A3>
//    {
//      void (*f)(A0,A1,A2,A3)=nullptr;
//      RavelFn(const char*name, libHandle lib) {ravelLib.asgFnPointer(f,name);}
//      void operator()(A0 a0, A1 a1, A2 a2,A3 a3) {if (f) f(a0,a1,a2,a3);}
//    };
//    
//#define DEFFN(f,...) RavelFn<__VA_ARGS__> f(#f,ravelLib.lib);
//    
//    DEFFN(ravel_lastErr, const char*);
//    DEFFN(ravel_version, const char*);
//    DEFFN(ravel_clear, void, Ravel::RavelImpl*);
//    DEFFN(ravel_rank, size_t, Ravel::RavelImpl*);
//    DEFFN(ravel_outputHandleIds, void, Ravel::RavelImpl*, size_t*);
//    DEFFN(ravel_addHandle, void, Ravel::RavelImpl*, const char*, size_t, const char**);
//    DEFFN(ravel_numHandles, unsigned, Ravel::RavelImpl*);
//    DEFFN(ravel_handleDescription, const char*, Ravel::RavelImpl*, int);
//    DEFFN(ravel_numSliceLabels, size_t, Ravel::RavelImpl*, size_t);
//    DEFFN(ravel_sliceLabels, void, Ravel::RavelImpl*, size_t, const char**);
//    DEFFN(ravel_displayFilterCaliper, void, Ravel::RavelImpl*, size_t, bool);
//    DEFFN(ravel_orderLabels, void, Ravel::RavelImpl*, size_t,HandleSort);
//    DEFFN(ravel_getHandleState, void, const Ravel::RavelImpl*, size_t, RavelHandleState*);
//    
//    DEFFN(ravelDC_loadData, void, Ravel::DataCube*, const Ravel::RavelImpl*, const double*);
//    DEFFN(ravelDC_hyperSlice, int, Ravel::DataCube*, Ravel::RavelImpl*, size_t*, double**);
//
//    inline double sqr(double x) {return x*x;} 
//  }  

  class RavelTensor: public civita::DimensionedArgCachedOp
  {
    const Ravel& ravel;
    TensorPtr arg;
    //friend struct DataCube;
    //friend struct RavelImpl;
    void computeTensor() const override  
    {
	 //RavelState initState;	
	 
    const_cast<Ravel&>(ravel).loadDataCubeFromVariable(*arg);      
      
//    if (ravel.ravel && ravel.dataCube)
//      {
		// Load input tensor pointer  
		  
        // this ensure that handles are restored correctly after loading a .mky file. 
//        RavelState state=initState.empty()? const_cast<Ravel&>(ravel).getState(): initState;
//        initState.clear();
//        ravel_clear(ravel.ravel);                         // THINK IT CAN BE USED AS IS
//        for (auto& i: arg->hypercube().xvectors)
//          {
//            vector<string> ss;
//            for (auto& j: i) ss.push_back(str(j));
//            // clear the format if time so that data will reload correctly
//            if (i.dimension.type==Dimension::time)
//              const_cast<Ravel&>(ravel).axisDimensions[i.name]=Dimension(Dimension::time,"");          // CHECK IF NEEDS TO BE REFACTORED.: CAN BE DONE INSIDE RAVELWRAP.CC
//            vector<const char*> sl;
//            for (auto& j: ss)
//              sl.push_back(j.c_str());
//            ravel_addHandle(ravel.ravel, i.name.c_str(), i.size(), &sl[0]);              // CHECK IF NEEDS TO BE REFACTORED
//            size_t h=ravel_numHandles(ravel.ravel)-1;                                    // CHECK IF NEEDS TO BE REFACTORED
//            ravel_displayFilterCaliper(ravel.ravel,h,false);                             // CHECK IF NEEDS TO BE REFACTORED   
//            // set forward sort order
//            ravel_orderLabels(ravel.ravel,h,RavelState::HandleState::forward);            // PROBABLY REQUIRES REFACTOR 
//          }
//        if (state.empty())
//          const_cast<Ravel&>(ravel).setRank(arg->hypercube().rank());
//#ifndef NDEBUG
//        if (state.empty())
//          {
//            auto d=arg->hypercube().dims();
//            assert(d.size()==ravel_rank(ravel.ravel));
//            vector<size_t> outputHandles(d.size());
//            ravel_outputHandleIds(ravel.ravel,&outputHandles[0]);                      // PROBABLY NEEDS TO BE REFACTORED
//            for (size_t i=0; i<d.size(); ++i)
//              assert(d[i]==ravel_numSliceLabels(ravel.ravel,outputHandles[i]));         // PROBABLY NEEDS TO BE REFACTORED
//          }
//#endif
//        vector<double> tmp(arg->size());
//        for (size_t i=0; i<arg->size(); ++i) tmp[i]=dynamic_cast<ITensorVal&>(*arg)[i];
//           ravelDC_loadData(ravel.dataCube, ravel.ravel, &tmp[0]);                           // PROBABLY NEEDS TO BE REFACTORED
//        const_cast<Ravel&>(ravel).applyState(state);                                       // PROBABLY NEEDS TO BE REFACTORED: CAN BE DONE INSIDE RAVELWRAP.CC
//          
//     
//        // Populate output tensor pointer
//        vector<size_t> dims(ravel_rank(ravel.ravel));
//        double* temp=nullptr;
//        ravelDC_hyperSlice(ravel.dataCube, ravel.ravel, &dims[0], &temp);              // WILL NEED REFACTORING
////        try
////            {
////              ravel.dataCube->hyperSlice(ravel.dataCube->slice,*ravel.ravel);           // WILL NEED REFACTORING, dataCube->slice is a rawData object inherited from struct AxisSlice, SizeAndStride is like C++ gslice object
////              if (ravel.ravel->rank()==ravel.dataCube->slice.rank())
////                {
////                  if (ravel.dataCube->slice.size()>0)
////                    *data=&ravel.dataCube->slice[0];
////                  for (size_t i=0; i<ravel.dataCube->slice.rank(); ++i)
////                    dims[i]=ravel.dataCube->slice.dim(i);
////                  return true;
////                }
////            }
//        if (dims.empty() || dims[0]==0)
//          {
//            cachedResult.hypercube({});
//            if (dims.empty() && temp) cachedResult[0]=temp[0];
//            return; // do nothing if ravel data is empty
//          }
//        if (&temp)
//          {
//            vector<size_t> outHandles(dims.size());
//            ravel_outputHandleIds(ravel.ravel, &outHandles[0]);                              // WILL NEED REFACTORING
//            Hypercube hc; 
//            auto& xv=hc.xvectors;
//            for (size_t j=0; j<outHandles.size(); ++j)
//              {
//                auto h=outHandles[j];
//                vector<const char*> labels(ravel_numSliceLabels(ravel.ravel,h));
//                assert(ravel_numSliceLabels(ravel.ravel,h)==dims[j]);                          // CHECK IF NEEDS TO BE REFACTORED       
//                ravel_sliceLabels(ravel.ravel,h,&labels[0]);                               // WILL NEED REFACTORING
//                assert(all_of(labels.begin(), labels.end(),
//                              [](const char* i){return bool(i);}));
//                xv.emplace_back
//                  (ravel_handleDescription(ravel.ravel,h));                                   // CHECK IF NEEDS TO BE REFACTORED                                           
//                auto dim=const_cast<Ravel&>(ravel).axisDimensions.find(xv.back().name);
//                if (dim!=const_cast<Ravel&>(ravel).axisDimensions.end())
//                  xv.back().dimension=dim->second;
//                else
//                  {
//                    auto dim=cminsky().dimensions.find(xv.back().name);
//                    if (dim!=cminsky().dimensions.end())
//                      xv.back().dimension=dim->second;
//                  }
//                // else otherwise dimension is a string (default type)
//                for (size_t i=0; i<labels.size(); ++i)
//                  xv.back().push_back(labels[i]);
//              }
//            cachedResult.hypercube(move(hc));
//            assert(vector<unsigned>(dims.begin(), dims.end())==cachedResult.hypercube().dims());
//
//            for (size_t i=0; i< cachedResult.size(); ++i)
//              *(cachedResult.begin()+i)=temp[i];
//          }
//        else
//          throw error(ravel_lastErr());
//      }
     //cachedResult.hypercube({}); // ensure scalar data space allocated
     
      ravel.loadDataFromSlice(cachedResult);  
//     if (dimension<rank())
//      {
//        auto argDims=arg->hypercube().dims();
//        size_t stride=1;
//        for (size_t j=0; j<dimension; ++j)
//          stride*=argDims[j];
//        if (argVal>=1 && argVal<argDims[dimension])
//          // argVal is interpreted as the binning window. -ve argVal ignored
//          for (size_t i=0; i<hypercube().numElements(); i+=stride*argDims[dimension])
//            for (size_t j=0; j<stride; ++j)
//              for (size_t j1=0; j1<argDims[dimension]*stride; j1+=stride)
//                {
//                  size_t k=i+j+max(ssize_t(0), ssize_t(j1-ssize_t(argVal-1)*stride));
//                  cachedResult[i+j+j1]=arg->atHCIndex(i+j+j1);
//                  for (; k<i+j+j1; k+=stride)
//                    {
//                      f(cachedResult[i+j+j1], arg->atHCIndex(k), k);
//                    }
//              }
//        else // scan over whole dimension
//          for (size_t i=0; i<hypercube().numElements(); i+=stride*argDims[dimension])
//            for (size_t j=0; j<stride; ++j)
//              {
//                cachedResult[i+j]=arg->atHCIndex(i+j);
//                for (size_t k=i+j+stride; k<i+j+stride*argDims[dimension]; k+=stride)
//                  {
//                    cachedResult[k] = cachedResult[k-stride];
//                    f(cachedResult[k], arg->atHCIndex(k), k);
//                  }
//              }
//          }
//    else
//      {
//        cachedResult[0]=arg->atHCIndex(0);
//        for (size_t i=1; i<hypercube().numElements(); ++i)
//          {
//            cachedResult[i]=cachedResult[i-1];
//            f(cachedResult[i], arg->atHCIndex(i), i);
//          }
//      }

      size_t i=0, j=0;
      for (; i<arg->size(); ++i)
        if ((*arg)[i]>0.5)
          cachedResult[j++]+=i;
      for (; j<cachedResult.size(); ++j)
        cachedResult[j]=nan("");

      m_timestamp = Timestamp::clock::now();
    }    
    CLASSDESC_ACCESS(Ravel);
  public:
    RavelTensor(const Ravel& ravel): ravel(ravel) {}
    //std::function<void(double&,double,size_t)> f;
    //template <class F>
    //RavelTensor(F f, const TensorPtr& arg={}, const std::string& dimName="", double av=0): f(f) 
    //{RavelTensor::setArgument(arg,dimName,av);}      

    void setArgument(const TensorPtr& a,const std::string&,double) override {arg=a;		
  	cachedResult.index(arg->index()); cachedResult.hypercube(arg->hypercube());}
    Timestamp timestamp() const override {return arg? arg->timestamp(): Timestamp();}
  };
       
  std::shared_ptr<ITensor> TensorOpFactory::create
  (const Item& it, const TensorsFromPort& tfp)
  {
    if (auto ravel=dynamic_cast<const Ravel*>(&it))
	    {
	      auto r=make_shared<RavelTensor>(*ravel);
	      r->setArguments(tfp.tensorsFromPorts(it.ports));
	      return r;
	    }
    if (auto op=it.operationCast())
      try
        {
          TensorPtr r{create(op->type())};
          switch (op->ports.size())
            {
            case 2:
              r->setArguments(tfp.tensorsFromPort(*op->ports[1]),op->axis,op->arg);
            break;
            case 3:
              r->setArguments(tfp.tensorsFromPort(*op->ports[1]), tfp.tensorsFromPort(*op->ports[2]));
              break;
		    }
          return r;
        }
      catch (const InvalidType&)
        {return {};}
    else if (auto v=it.variableCast())
      return make_shared<TensorVarVal>(*v->vValue(), tfp.ev);
    else if (auto sw=dynamic_cast<const SwitchIcon*>(&it))
      {
        auto r=make_shared<SwitchTensor>();
        r->setArguments(tfp.tensorsFromPorts(it.ports));
        return r;
      } // Original code
    //else if (auto ravel=dynamic_cast<const Ravel*>(&it))
    //  {
    //    auto r=make_shared<RavelTensor>(*ravel);
    //    r->setArguments(tfp.tensorsFromPorts(it.ports));
    //    return r;
    //  }
    return {};
  }

  vector<TensorPtr> TensorsFromPort::tensorsFromPort(const Port& p) const
  {
    if (!p.input()) return {};
    vector<TensorPtr> r;
    for (auto w: p.wires())
      {
        Item& item=w->from()->item();
        if (auto o=item.operationCast())
          {
            if (o->type()==OperationType::differentiate)
              {
                // check if we're differentiating a scalar or tensor
                // expression, and throw accordingly
                auto rhs=tensorsFromPort(*o->ports[1]);
                if (rhs.empty() || rhs[0]->size()==1)
                  throw FallBackToScalar();
                else
                  // TODO - implement symbolic differentiation of
                  // tensor operations
                  throw std::runtime_error("Tensor derivative not implemented");
              }
          }
        r.push_back(tensorOpFactory.create(item, *this));
        assert(r.back());
      }
    return r;
  }


  vector<TensorPtr> TensorsFromPort::tensorsFromPorts(const vector<shared_ptr<Port>>& ports) const
  {
    vector<TensorPtr> r;
    for (auto& p: ports)
      if (p->input())
        {
          auto tensorArgs=tensorsFromPort(*p);
          r.insert(r.end(), tensorArgs.begin(), tensorArgs.end());
        }
    return r;
  }

  TensorEval::TensorEval(VariableValue& v, const shared_ptr<EvalCommon>& ev): result(v, ev)
  {
    if (auto var=cminsky().definingVar(v.valueId()))
      if (var->lhs())
        {
          rhs=TensorsFromPort(ev).tensorsFromPort(*var->ports[1])[0];
          result.hypercube(rhs->hypercube());
          result.index(rhs->index());
          v=result;
        }
  }
  
  TensorEval::TensorEval(const VariableValue& dest, const VariableValue& src):
    result(dest,make_shared<EvalCommon>())
  {
    result.index(src.index());
    result.hypercube(src.hypercube());
    Operation<OperationType::copy> tmp;
    auto copy=dynamic_pointer_cast<ITensor>(tensorOpFactory.create(tmp));
    copy->setArgument(make_shared<TensorVarVal>(src,result.ev));
    rhs=move(copy);
    assert(result.size()==rhs->size());
  }   

  void TensorEval::eval(double fv[], const double sv[])
  {
    if (rhs)
      {
        assert(result.idx()>=0);
        assert(result.size()==rhs->size());
        result.ev->update(fv, sv);
        for (size_t i=0; i<rhs->size(); ++i)
          {
            result[i]=(*rhs)[i];
            assert(!finite(result[i]) || fv[result.idx()+i]==(*rhs)[i]);
            //            cout << "i="<<i<<"idx="<<result.idx()<<" set to "<< (*rhs)[i] << " should be "<<fv[result.idx()]<<endl;
          }
      }
  }
   
  void TensorEval::deriv(double df[], const double ds[],
                         const double sv[], const double fv[])
  {
    if (result.idx()<0) return;
    if (rhs)
      {
        result.ev->update(const_cast<double*>(fv), sv);
        if (auto deriv=dynamic_cast<DerivativeMixin*>(rhs.get()))
          for (size_t i=0; i<rhs->size(); ++i)
            {
              df[result.idx()+i]=0;
              for (int j=0; j<result.idx(); ++j)
                df[result.idx()+i] += df[j]*deriv->dFlow(i,j);
              // skip self variables
              for (size_t j=result.idx()+result.size(); j<ValueVector::flowVars.size(); ++j)
                df[result.idx()+i] += df[j]*deriv->dFlow(i,j);
              for (size_t j=0; j<ValueVector::stockVars.size(); ++j)
                df[result.idx()+i] += ds[j]*deriv->dStock(i,j);
            }
      }
  }
}
