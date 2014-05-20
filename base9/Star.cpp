#include <cassert>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "Cluster.hpp"
#include "ifmr.hpp"
#include "Model.hpp"
#include "Star.hpp"

#include <iostream>

using std::ifstream;
using std::string;
using std::stringstream;
using std::vector;

vector<double> Star::getMags (const Cluster &clust, const Model &evoModels) const
{
    // Masses greater than 100Mo should never occur
    assert (mass <= 100.0);

    switch(getStatus(clust))
    {
        case MSRG: // for main seq or giant star
            return evoModels.mainSequenceEvol->msRgbEvol(mass);

        case WD:   // for white dwarf
            return wdEvol (clust, evoModels);

        default:   // For brown dwarfs, neutron stars, black hole remnants, and 0-mass secondaries
            vector<double> mags;
            mags.resize(evoModels.absCoeffs.size(), 99.999);

            return mags;
    }
}

StarStatus Star::getStatus(const Cluster &clust) const
{
    if (mass <= 0.0001)
    {                           // for non-existent secondaries
        return DNE;
    }
    else if (mass <= clust.AGBt_zmass)
    {                           // for main seq or giant star
        return MSRG;
    }
    else if (mass <= clust.getM_wd_up())
    {                           // for white dwarf
        return WD;
    }
    else if (mass <= 100.)
    {                           // for neutron star or black hole remnant
        return NSBH;
    }
    else
    {                           // for everything else
        return DNE;
    }
}

vector<double> Star::wdEvol (const Cluster &clust, const Model &evoModels) const
{
    double thisWDMass = intlFinalMassReln (clust, evoModels, mass);

    auto precLogAge = evoModels.mainSequenceEvol->wdPrecLogAge(clust.feh, mass, clust.yyy);

    //get temperature from WD cooling models (returns 0.0 if there is an error(or does it??))
    auto teffRadiusPair = evoModels.WDcooling->wdMassToTeffAndRadius (clust.age, clust.carbonicity, precLogAge, thisWDMass);

    double logTeff = teffRadiusPair.first;
    // double thisWDLogRadius = teffRadiusPair.second;
    
    auto mags = evoModels.WDAtmosphere->teffToMags (logTeff, thisWDMass, wdType);

    return mags;
}


// Returns actual current mass (i.e. not zams_mass)
double Star::wdMassNow(const Cluster &clust, const Model &evoModels) const
{
    if (mass <= clust.AGBt_zmass)
    {                           // for main seq or giant star
        return mass;
    }
    else if (mass <= clust.getM_wd_up())
    {                           // for white dwarf
        return intlFinalMassReln (clust, evoModels, mass);
    }
    else
    {
        return 0.0;
    }
}


double Star::wdLogTeff(const Cluster &clust, const Model &evoModels) const
{
    double thisWDMass = intlFinalMassReln (clust, evoModels, mass);

    auto precLogAge = evoModels.mainSequenceEvol->wdPrecLogAge(clust.feh, mass, clust.yyy);

    auto teffRadiusPair = evoModels.WDcooling->wdMassToTeffAndRadius (clust.age, clust.carbonicity, precLogAge, thisWDMass);

    return teffRadiusPair.first;
}


double Star::getLtau(const Cluster &clust, const Model &evoModels) const
{
    return evoModels.mainSequenceEvol->wdPrecLogAge(clust.feh, mass, clust.yyy);
}

double StellarSystem::getMassRatio() const
{
    return secondary.mass / primary.mass;
}

void StellarSystem::setMassRatio(double r)
{
    // Secondary stars should always be between 0 and the primary's mass.
    assert ( r >= 0.0 );
    assert ( r <= 1.0 );

    secondary.mass = primary.mass * r;
}

void StellarSystem::readCMD(const string &s, int filters)
{
    int status;
    double tempSigma, massRatio;
    string starID;

    stringstream in(s);  

    in >> starID;

    for (int i = 0; i < filters; i++)
    {
        double t;
        in >> t;

        obsPhot.push_back(t);
    }

    for (int i = 0; i < filters; i++)
    {
        in >> tempSigma;

        variance.push_back(tempSigma * fabs (tempSigma));
        // The fabs() keeps the sign of the variance the same as that input by the user for sigma
        // Negative sigma (variance) is used to signal "don't count this band for this star"
    }

    in >> primary.mass
       >> massRatio
       >> status
       >> clustStarPriorDens
       >> useDuringBurnIn;

    if ((status == 1)   // MSRGB
     || (status == 3)   // WD
     || (status == 4)   // NSBH
     || (status == 5))  // BD
    {
        observedStatus = static_cast<StarStatus>(status);
    }
    else
        observedStatus = DNE;

    setMassRatio(massRatio);
}


vector<double> StellarSystem::deriveCombinedMags (const Cluster &clust, const Model &evoModels) const
{
    auto primaryMags = primary.getMags(clust, evoModels);
    auto secondaryMags = secondary.getMags(clust, evoModels);

    assert(primaryMags.size() == secondaryMags.size());
    assert(evoModels.absCoeffs.size() == primaryMags.size());

    vector<double> combinedMags;

    // can now derive combined mags
    if (secondaryMags.front() < 99.)
    {                           // if there is a secondary star
        double flux = 0.0;

        for (size_t f = 0; f < primaryMags.size(); ++f)
        {
            flux = exp10((primaryMags.at(f) / -2.5));    // add up the fluxes of the primary
            flux += exp10((secondaryMags.at(f) / -2.5)); // and the secondary
            combinedMags.push_back(-2.5 * log10 (flux));    // (these 3 lines .at(used to?) take 5% of run time for N large)
            // if primary mag = 99.999, then this works
        }
    }  // to make the combined mag = secondary mag
    else
    {
        for (size_t f = 0; f < primaryMags.size(); ++f)
            combinedMags.push_back(primaryMags.at(f));
    }

    assert(combinedMags.size() == primaryMags.size());

    for (size_t f = 0; f < combinedMags.size(); ++f)
    {
        combinedMags.at(f) += clust.mod;
        combinedMags.at(f) += (evoModels.absCoeffs.at(f) - 1.0) * clust.abs;       // add A_.at(u-k) (standard defn of modulus already includes Av)
    }

    return combinedMags;
}
