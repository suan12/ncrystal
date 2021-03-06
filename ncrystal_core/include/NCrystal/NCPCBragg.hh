#ifndef NCrystal_PCBragg_hh
#define NCrystal_PCBragg_hh

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

#include "NCrystal/NCScatterIsotropic.hh"
#include "NCrystal/NCInfo.hh"
#include <vector>
#include <utility>

namespace NCrystal {

  class PlaneProvider;

  class NCRYSTAL_API PCBragg : public ScatterIsotropic {
  public:

    //Calculates Bragg diffraction in a polycrystalline/powdered material.

    //Constructor:
    PCBragg( const Info* );

    //Alternative constructor which use plane provider for source of
    //planes. This is not particularly efficient in general for PCBragg, since
    //it likely incurs the overhead of normal-creation, which is not actually
    //needed. It exists mainly for specialised usage (such at putting some
    //unimportant planes from a single crystal into a PCBragg instance as a
    //tradeoff to gain faster simulations). Will *not* assume ownership of plane
    //provider.
    PCBragg( const StructureInfo& , PlaneProvider * );

    //Specialised constructors taking (dspacing,fsquared*multiplicity) pairs
    //(will sort passed vector, hence it is non-const). Either needs structure
    //info, or just v0*n_atoms, unit cell volume in Aa^3 and number atoms per
    //unit cell:
    PCBragg( const StructureInfo&, std::vector<std::pair<double,double> >& );
    PCBragg( double v0_times_natoms, std::vector<std::pair<double,double> >& );

    //The cross-section (in barns):
    virtual double crossSectionNonOriented(double ekin) const;

    //There is a maximum wavelength at which Bragg diffraction is possible,
    //so ekin_low will be set to reflect this (ekin_high will be set to infinity):
    virtual void domain(double& ekin_low, double& ekin_high) const;

    //Generate scatterings according to Bragg diffraction. This is elastic
    //scattering and will always result in delta_ekin=0:
    virtual void generateScatteringNonOriented( double ekin,
                                                double& angle, double& delta_ekin ) const;

    virtual void generateScattering( double ekin, const double (&neutron_direction)[3],
                                     double (&resulting_neutron_direction)[3], double& delta_ekin ) const;

  protected:
    virtual ~PCBragg();
    double genScatterMu(RandomBase*, double ekin) const;
    std::size_t findLastValidPlaneIdx(double ekin) const;
    double m_threshold;
    std::vector<double> m_2dE;
    std::vector<double> m_fdm_commul;
    void init( const StructureInfo&, std::vector<std::pair<double,double> >& );
    void init( double v0_times_natoms, std::vector<std::pair<double,double> >& );
  };

}

#endif
