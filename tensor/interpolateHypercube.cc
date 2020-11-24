/*
  @copyright Russell Standish 2020
  @author Russell Standish
  This file is part of Civita.

  Civita is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Civita is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Civita.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "interpolateHypercube.h"
#include <ecolab_epilogue.h>

namespace civita
{

  vector<size_t> InterpolateHC::splitAndRotate(size_t hcIndex) const
  {
    vector<size_t> r(rank());
    auto& h=hypercube();
    for (size_t dim=0; dim<rank(); ++dim)
      r[rotation[dim]] = (hcIndex/strides[dim]) % h.xvectors[dim].size();
    return r;
  }

  
  void InterpolateHC::setArgument(const TensorPtr& a, const string&,double)
  {
    arg=a;
    if (rank()!=arg->rank())
      throw runtime_error("Rank of interpolated tensor doesnt match its argument");
    // reorder hypercube for type and name
    interimHC.xvectors.clear();
    size_t stride=1;
    auto& targetHC=hypercube().xvectors;
    rotation.clear();
    rotation.resize(rank(), rank());
    // construct interim hypercube and its rotation permutation
    for (size_t i=0; i<rank(); ++i)
      {
        auto& src=arg->hypercube().xvectors[i];
        if (src.name.empty())
          {
            auto& dst=targetHC[i];
            if (!dst.name.empty() || src.dimension.type!=dst.dimension.type ||
                src.dimension.units!=dst.dimension.units) //TODO handle conversion between different units
              throw runtime_error("mismatch between unnamed dimensions");
            interimHC.xvectors.push_back(dst);
            rotation[i]=i;
          }
        else // find matching names dimension
          {
            auto dst=find_if(targetHC.begin(), targetHC.end(),
                             [&](const XVector& i){return i.name==src.name;});
            if (dst==targetHC.end())
              throw runtime_error("no matching dimension found");
            interimHC.xvectors.push_back(*dst);
            rotation[dst-targetHC.begin()]=i;
          }
        strides.push_back(stride);
        stride*=targetHC[i].size();
      }
#ifndef NDEBUG
    for (auto& i: rotation) assert(i<rank()); // check that no indices have been doubly assigned.
    // Now we're sure rotation is a permutation
#endif
    if (index().empty())
      for (size_t i=0; i<size(); ++i)
        weightedIndices.push_back(bodyCentredNeighbourhood(i));
    else
      for (auto i: index())
        weightedIndices.push_back(bodyCentredNeighbourhood(i));
  }
  
  double InterpolateHC::operator[](size_t idx) const
  {
    assert(arg->size()==weightedIndices.size());
    if (idx<weightedIndices.size())
      {
        if (weightedIndices[idx].empty()) return nan("");
        double r=0;
        for (auto& i: weightedIndices[idx])
          r+=i.weight * (*arg)[i.index];
        return r;
      }
    return nan("");
  }

  InterpolateHC::WeightedIndexVector InterpolateHC::bodyCentredNeighbourhood(size_t destIdx) const
  {
    // note this agorithm is limited in rank (typically 32 dims on 32bit machine, or 64 dims on 64bit)
    if (rank()>sizeof(size_t)*8)
      throw runtime_error("Ranks > "+to_string(sizeof(size_t)*8)+" not supported");
    size_t numNeighbours=size_t(1)<<rank();
    vector<size_t> iIdx=splitAndRotate(destIdx);
    auto& argHC=arg->hypercube();
    // loop over the nearest neighbours in argument hypercube space of
    // this point in interimHypercube space
    double sumWeight=0;
    WeightedIndexVector r;
    size_t idx=0;
    auto argIndexVector=arg->index();
    // multivariate interpolation - eg see Abramowitz & Stegun 25.2.66
    for (size_t nbr=0; nbr<numNeighbours; ++nbr)
      {
        size_t argIdx=0;
        double weight=1;
        for (size_t dim=0, stride=1; dim<rank(); ++dim, stride*=argHC.xvectors[dim].size())
          {
            auto& x=argHC.xvectors[dim];
            auto v=interimHC.xvectors[dim][iIdx[dim]];
            auto lesserIt=std::lower_bound(x.begin(), x.end(), v, AnyLess());
            boost::any lesser, greater;
            if (lesserIt==x.end())
              {
                --lesserIt;
                lesser=x.back();
              }
            else
              lesser=*lesserIt;
            if (lesserIt+1==x.end())
              greater=x.back();
            else
              greater=*(lesserIt+1);

            bool greaterSide = nbr&(size_t(1)<<dim); // on higher side of hypercube
            double d=diff(greater,lesser);
            if (d==0 && greaterSide) goto nextNeighbour; // already taken lesserVal at weight 1.

            idx += (lesserIt-x.begin()+greaterSide)*stride;
            if (d>0)
              weight *= greaterSide? diff(v,lesser): d-diff(v,lesser);
          }
        if (index().empty())
          {
            r.emplace_back(WeightedIndex{idx,weight});
            sumWeight+=weight;
          }
        else
          {
            auto indexV=index();
            if (binary_search(indexV.begin(), indexV.end(), idx))
              {
                r.emplace_back(WeightedIndex{idx,weight});
                sumWeight+=weight;
              }
          }
      nextNeighbour:;
      }

    // normalise weights
    for (auto& i: r) i.weight/=sumWeight;
    return r;
  }

}