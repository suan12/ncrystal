////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  This file is part of NCrystal (see https://mctools.github.io/ncrystal/)   //
//                                                                            //
//  Copyright 2015-2019 NCrystal developers                                   //
//                                                                            //
//  Licensed under the Apache License, Version 2.0 (the "License");           //
//  you may not use this file except in compliance with the License.          //
//  You may obtain a copy of the License at                                   //
//                                                                            //
//      http://www.apache.org/licenses/LICENSE-2.0                            //
//                                                                            //
//  Unless required by applicable law or agreed to in writing, software       //
//  distributed under the License is distributed on an "AS IS" BASIS,         //
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  //
//  See the License for the specific language governing permissions and       //
//  limitations under the License.                                            //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "NCBkgdPhonDebyeXS.hh"
#include "NCrystal/NCInfo.hh"
#include "NCMath.hh"
#include "NCPhononDebye.hh"
#include "NCNeutronSCL.hh"
#include <algorithm>
#include <cstdlib>
#include <iostream>

NCrystal::BkgdPhonDebyeXS::BkgdPhonDebyeXS(double kt, bool extrapolate_from_peak)
  : m_en_inel(logspace(-5,log10(ncmin(1,log(std::numeric_limits<double>::max())*kt*2)), 200)),
    m_xs_inel(m_en_inel.size(),0.), m_saturated_xs(-1.0),
    m_k(0.), m_k2(0.0), m_extrapolate_from_peak(extrapolate_from_peak)
{
}

void NCrystal::BkgdPhonDebyeXS::setSaturatedXSAndInit(double xs)
{
  nc_assert_always(m_saturated_xs==-1.0&&xs>=0.0);
  m_saturated_xs = xs;
  if (m_extrapolate_from_peak)
    cropIncompleteHighEBins();
  double lambda_edge = ekin2wl(m_en_inel.back());
  double xs_edge = m_xs_inel.back();
  nc_assert(lambda_edge>0);
  //see the implementation of getXS() for an explanation and usage of these constants:
  m_k = (xs_edge-m_saturated_xs)/(lambda_edge*lambda_edge);
  m_k2 = m_xs_inel.front() / ekin2wl( m_en_inel.front() );
  size_t n = m_xs_inel.size();
  m_k3.reserve(n);
  m_k3.push_back(std::make_pair(0.,0.));
  for (size_t u = 1; u < n; ++u) {
    size_t l = u-1;
    double Eu = m_en_inel.at(u);
    double Xu = m_xs_inel.at(u);
    double El = m_en_inel.at(l);
    double Xl = m_xs_inel.at(l);
    double dd = (Xu-Xl)/(Eu-El);
    m_k3.push_back(std::make_pair(Xl-El*dd,dd));
  }
  std::vector<double>().swap(m_xs_inel);//release m_xs_inel memory since no longer needed.
}

void NCrystal::BkgdPhonDebyeXS::cropIncompleteHighEBins()
{
  //Shorten energy/cross-section vectors to the point where the phonon spectrum
  //begins to drop off due to insufficient terms in the expansion (querying
  //beyond the vector triggers extrapolation towards the saturated/free
  //cross-section at 0Aa).:
  std::vector<double> & v_e  = m_en_inel;
  std::vector<double> & v_xs = m_xs_inel;
  nc_assert(v_e.size()==v_xs.size()&&v_xs.size()>1);
  size_t n = v_e.size()-1;
  while (n) {
    double delta_xs = v_xs.at(n)-v_xs.at(n-1);
    double delta_wl = ekin2wl(v_e.at(n)) - ekin2wl(v_e.at(n-1));
    double dxsdwl = delta_xs/delta_wl;
    if ( dxsdwl <= 0 && n+1 == v_e.size() )
      break;//XS decreasing with wl at edge - don't perform truncation at all in this case.
    if (dxsdwl < 0.075*m_saturated_xs)//XS no longer increasing rapidly, stop here ( 0.075 barn/Aa is a hand-tuned value).
      break;
    --n;
  }
  if (!n)
    NCRYSTAL_THROW(CalcError,"Cross sections from phonon expansion keeps increasing rapidly over all wavelengths");
  //perform truncation:
  v_e.resize(n);
  v_xs.resize(n);
#if __cplusplus >= 201103L
  v_e.shrink_to_fit();
  v_xs.shrink_to_fit();
#endif

  const bool verbose = (std::getenv("NCRYSTAL_DEBUGSCATTER") ? true : false);
  if (verbose)
    std::cout<<"NCrystal::NCBkgdPhonDebye multi-phonon cross-section extrapolated below "
             <<ekin2wl(v_e.back()) <<" Aa"<<std::endl;
}


double NCrystal::BkgdPhonDebyeXS::getXS(const double& kiEn) const
{
  std::vector<double>::const_iterator upper = std::upper_bound (m_en_inel.begin(), m_en_inel.end(), kiEn);

  if(upper == m_en_inel.end())
  {
    if (!m_extrapolate_from_peak)
      return 0.0;
    //interpolate xs between m_en_inel.back() and 0 Angstrom. In this region,
    //the contribution from Bragg diffraction will tend to zero as lambda^2 -
    //this is assuming we are at wavelengths low enough that all significant
    //Bragg edges already contribute (which we should hopefully be if our phonon
    //expansion had enough terms). Since we should approach a flat total
    //scattering cross-section, the phonon background cross-section should
    //consequently increase as m_saturated_xs+k*lambda^2 for some (usually
    //negative) constant k, which we fix by requiring continuity at lambda_edge
    //(see the implementation of setSaturatedXSAndInit for how we calculate k):
    const double lambda = ekin2wl(kiEn);//TODO for NC2: work directly on energies.
    return m_saturated_xs + m_k * lambda * lambda;
  }
  else if (upper == m_en_inel.begin() )
  {
    //Extrapolate to low energies using single-phonon 1/v law (m_k2 fixed by
    //continuity condition):
    const double lambda = ekin2wl(kiEn);//TODO for NC2: work directly on energies.
    return m_k2 * lambda;
  }
  else
  {
    //inside calculated range = interpolate in bins
    const std::pair<double,double>& interp_consts = m_k3[upper-m_en_inel.begin()];
    return interp_consts.first + interp_consts.second * kiEn;
  }
}

void NCrystal::BkgdPhonDebyeXS::accumInelastic(const std::vector<double>& xs, double frac)
{
  nc_assert_always(m_saturated_xs==-1.0);
  nc_assert_always(xs.size()==m_en_inel.size());

  for(unsigned i=0;i<xs.size();i++)
    m_xs_inel[i] += xs[i]*frac;
}

NCrystal::RCHolder<const NCrystal::BkgdPhonDebyeXS> NCrystal::createBkgdPhonDebyeXS(const NCrystal::Info* ci,
                                                                                    unsigned nphonon,
                                                                                    bool include_phonzeroinco,
                                                                                    bool only_phonzeroinco,
                                                                                    bool extrapolate_from_peak )
{
  nc_assert_always(ci);
  if (!ci->hasAtomInfo())
    NCRYSTAL_THROW(MissingInfo,"Passed Info object lacks AtomInfo information.");
  if (!ci->hasTemperature())
    NCRYSTAL_THROW(MissingInfo,"Passed Info object lacks Temperature information.");
  if (!ci->hasXSectFree())
    NCRYSTAL_THROW(MissingInfo,"Passed Info object lacks XSectFree information.");
  if (!ci->hasPerElementDebyeTemperature()&&!ci->hasDebyeTemperature())
    NCRYSTAL_THROW(MissingInfo,"Passed Info object lacks Debye temperature information.");

  if (!include_phonzeroinco&&only_phonzeroinco)
    NCRYSTAL_THROW(BadInput,"Do not set include_phonzeroinco=false with only_phonzeroinco=true");
  int pzi_flag = include_phonzeroinco ? ( only_phonzeroinco ? 2 : 1 ) : 0;
  if (only_phonzeroinco)
    nphonon=1;
  const double kT = ci->getTemperature() * constant_boltzmann;
  BkgdPhonDebyeXS * res = new BkgdPhonDebyeXS(kT,extrapolate_from_peak);
  RCGuard guard(res);

  std::vector<double> xs;

  const NeutronSCL *nscl = NeutronSCL::instance();

  //Deduce natoms/cell from AtomInfo rather than StructureInfo, since we don't
  //otherwise depend on the latter here:
  unsigned ntotatoms = 0;
  for (AtomList::const_iterator it = ci->atomInfoBegin(); it!=ci->atomInfoEnd(); ++it)
    ntotatoms += it->number_per_unit_cell;

  nc_assert_always(ntotatoms>0);

  for (AtomList::const_iterator it = ci->atomInfoBegin(); it!=ci->atomInfoEnd(); ++it) {
    const std::string& element_name = nscl->getAtomName(it->atomic_number);
    if (element_name.empty())
      NCRYSTAL_THROW2(BadInput,"Does not have data for element number "<<it->atomic_number);

    double debye_temp = ci->hasPerElementDebyeTemperature() ? it->debye_temp : ci->getDebyeTemperature();
    bool auto_select(nphonon==0);
    //phonon inelastic:

    //TODO for NC2: For consistency, do we want to be able to pass in MSD from
    //AtomInfo if available, and use those rather than the ones calculated
    //internally in PhononDebye? For .ncmat files it is anyway calculated in the
    //same manner, but this is not the case for .nxs files.
    PhononDebye inel( debye_temp*constant_boltzmann, kT, element_name, nphonon, pzi_flag );

    const bool verbose = (std::getenv("NCRYSTAL_DEBUGSCATTER") ? true : false);
    if (auto_select&&verbose)
      std::cout<<"NCrystal::NCBkgdPhonDebye model automatically selected nphonon level "<< inel.getMaxPhononNum()<<std::endl;

    nc_assert(res);
    inel.doit(res->getEnergyVector(), xs);
    res->accumInelastic( xs, double(it->number_per_unit_cell) / ntotatoms );
  }

  res->setSaturatedXSAndInit(ci->getXSectFree());

  return RCHolder<const BkgdPhonDebyeXS>(res);
}
