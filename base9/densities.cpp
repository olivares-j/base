#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "constants.hpp"
#include "evolve.hpp"
#include "structures.hpp"
#include "densities.hpp"

constexpr double sqr(double a)
{
    return a * a;
}

static_assert(M_PI >= 3.14, "M_PI is defined and and at least 3.14");
static_assert(M_PI <  4.00, "M_PI is defined and less than 4.0");

static double logMassNorm = 0.0;
static int calcMassNorm = 0;

extern double filterPriorMin[FILTS], filterPriorMax[FILTS];
extern double ageLimit[2];

double logPriorMass (Star *pStar, Cluster *pCluster)
// Compute log prior density
{
    const double mf_sigma = 0.67729, mf_mu = -1.02;
    const double loglog10 = log (log (10));

    double mass1, log_m1, logPrior = 0.0;

    if (pStar->status[0] == BD)
        return 0.0;

    // Calculate the mass normalization factor once so that we don't have to
    // calculate it every time we want the mass prior
    if (!calcMassNorm)
    {
        double p, q, c;
        double tup, tlow;

        p = mf_mu + mf_sigma * mf_sigma * log (10);
        tup = (log10 (pCluster->M_wd_up) - p) / (mf_sigma);
        tlow = (-1 - p) / mf_sigma;
        q = exp (-(mf_mu * mf_mu - p * p) / (2 * mf_sigma * mf_sigma));
        c = 1 / (q * mf_sigma * sqrt (2 * M_PI) * (Phi (tup) - Phi (tlow)));

        logMassNorm = log (c);
        calcMassNorm = 1;
    }

    mass1 = getMass1 (pStar, pCluster);

    if (mass1 > 0.1 && mass1 <= pCluster->M_wd_up)
    {
        if ((*pStar).isFieldStar)
        {
            logPrior = logTDens ((*pStar).U, (*pStar).meanU, (*pStar).varU, DOF);
            if ((*pStar).status[0] != WD)
                logPrior += logTDens ((*pStar).massRatio, (*pStar).meanMassRatio, (*pStar).varMassRatio, DOF);
            return logPrior;
        }
        else
        {
            log_m1 = log10 (mass1);
            logPrior = logMassNorm + -0.5 * sqr (log_m1 - mf_mu) / (sqr (mf_sigma)) - log (mass1) - loglog10;
            return logPrior;
        }
    }
    else
        return -HUGE_VAL;
}

// Compute log prior density for cluster properties
double logPriorClust (Cluster *pCluster)
{
    if (getParameter (pCluster, AGE) < ageLimit[0])
        return -HUGE_VAL;               // these are possible, we just don't have models for them YET
    else if (getParameter (pCluster, AGE) > ageLimit[1])
        return -HUGE_VAL;               // appropriate for the MS/RGB models but not the WDs
    else if (pCluster->parameter[IFMR_SLOPE] < 0.0)
        return -HUGE_VAL;
    if (pCluster->evoModels.IFMR == 11)
    {
        if (pCluster->parameter[IFMR_QUADCOEF] < 0.0)
            return -HUGE_VAL;
    }

    // enforce monotonicity in IFMR
    if (pCluster->evoModels.IFMR == 10)
    {
        double massLower = 0.15;
        double massUpper = pCluster->M_wd_up;
        double massShift = 3.0;
        double angle = atan (pCluster->parameter[IFMR_SLOPE]);
        double aa = cos (angle) * (1 + pCluster->parameter[IFMR_SLOPE] * pCluster->parameter[IFMR_SLOPE]);
        double xLower = aa * (massLower - massShift);
        double xUpper = aa * (massUpper - massShift);

        double dydx_xLower = pCluster->parameter[IFMR_QUADCOEF] * (xLower - xUpper);
        double dydx_xUpper = -dydx_xLower;

        double slopeLower = tan (angle + atan (dydx_xLower));
        double slopeUpper = tan (angle + atan (dydx_xUpper));

        // if IFMR is decreasing at either endpoint, reject
        if (slopeLower < 0.0 || slopeUpper < 0.0)
            return -HUGE_VAL;
    }

    double prior = 0.0;

    if (getParameter (pCluster, ABS) < 0.0)
        return -HUGE_VAL;
    if (pCluster->priorVar[FEH] > EPSILON)
        prior += (-0.5) * sqr (getParameter (pCluster, FEH) - pCluster->priorMean[FEH]) / pCluster->priorVar[FEH];
    if (pCluster->priorVar[MOD] > EPSILON)
        prior += (-0.5) * sqr (getParameter (pCluster, MOD) - pCluster->priorMean[MOD]) / pCluster->priorVar[MOD];
    if (pCluster->priorVar[ABS] > EPSILON)
        prior += (-0.5) * sqr (getParameter (pCluster, ABS) - pCluster->priorMean[ABS]) / pCluster->priorVar[ABS];
    if (pCluster->priorVar[YYY] > EPSILON)
        prior += (-0.5) * sqr (getParameter (pCluster, YYY) - pCluster->priorMean[YYY]) / pCluster->priorVar[YYY];

    return prior;
}

double logLikelihood (int numFilts, Star *pStar)
// Computes log likelihood
{
    int i;
    double likelihood = 0.0;

    for (i = 0; i < numFilts; i++)
    {
        if ((*pStar).isFieldStar)
        {
            if (filterPriorMin[i] <= pStar->obsPhot[i] && pStar->obsPhot[i] <= filterPriorMax[i])
                likelihood -= log (filterPriorMax[i] - filterPriorMin[i]);
            else
            {
                likelihood = -HUGE_VAL;
                return likelihood;
            }
        }
        else
        {
            if (pStar->variance[i] > 1e-9)
                likelihood -= 0.5 * (log (2 * M_PI * pStar->variance[i]) + (sqr (pStar->photometry[i] - pStar->obsPhot[i]) / pStar->variance[i]));
        }
    }
    return likelihood;
}

double tLogLikelihood (int numFilts, Star *pStar)
// Computes log likelihood
{
    int i;
    double likelihood = 0.0;
    double dof = 3.0;
    double quadratic_sum = 0.0;

    if ((*pStar).isFieldStar)
    {
        for (i = 0; i < numFilts; i++)
        {
            if (filterPriorMin[i] <= pStar->obsPhot[i] && pStar->obsPhot[i] <= filterPriorMax[i])
                likelihood -= log (filterPriorMax[i] - filterPriorMin[i]);
            else
            {
                likelihood = -HUGE_VAL;
                return likelihood;
            }
        }
    }
    else
    {
        for (i = 0; i < numFilts; i++)
        {
            if (pStar->variance[i] > 1e-9)
            {
                quadratic_sum += sqr (pStar->photometry[i] - pStar->obsPhot[i]) / pStar->variance[i];
                likelihood -= 0.5 * (log (M_PI * pStar->variance[i]));
            }
        }
        likelihood += lgamma (0.5 * (dof + (double) numFilts)) - lgamma (0.5 * dof);
        likelihood -= 0.5 * (dof + (double) numFilts) * log (1 + quadratic_sum);
    }
    return likelihood;
}

double scaledLogLike (int numFilts, Star *pStar, double varScale)
// Computes log likelihood
{
    int i;
    double likelihood = 0.0;

    for (i = 0; i < numFilts; i++)
    {
        if ((*pStar).isFieldStar)
        {
            if (filterPriorMin[i] <= pStar->obsPhot[i] && pStar->obsPhot[i] <= filterPriorMax[i])
                likelihood -= log (filterPriorMax[i] - filterPriorMin[i]);
            else
            {
                likelihood = -HUGE_VAL;
                return likelihood;
            }
        }
        else
        {
            if (pStar->variance[i] > 1e-9)
            {
                likelihood -= 0.5 * (log (2 * M_PI * varScale * pStar->variance[i]) + (sqr (pStar->photometry[i] - pStar->obsPhot[i]) / (varScale * pStar->variance[i])));
            }

        }
    }
    return likelihood;
}


double logPost1Star (Star *pStar, Cluster *pCluster)
// Compute posterior density for 1 star:
{
    double likelihood = 0.0, logPrior = 0.0;

    logPrior = logPriorMass (pStar, pCluster);

    if (fabs (logPrior + HUGE_VAL) < EPSILON)
        return (logPrior);

    likelihood = scaledLogLike (pCluster->evoModels.numFilts, pStar, pCluster->varScale);

    if (fabs (likelihood + HUGE_VAL) < EPSILON)
        return (likelihood);

    return (logPrior + likelihood);
}


// computes normal distribution Phi(x) (integral from -Inf to x of normal density)
// taken from: http://www.jstatsoft.org/v11/i04/v11i04.pdf
double Phi (double x)
{
    long double s = x, t = 0, b = x, q = x * x, i = 1;

    while (s != t)
        s = (t = s) + (b *= q / (i += 2));

    return 0.5 + s * exp (-0.5 * q - 0.91893853320467274178L);
}

// do not call this routine with nu = 2, which wouldn't make much sense anyway
double logTDens (double x, double mean, double var, double nu)
{
    double logp = 0;
    double s;

    s = sqrt (DOF / (var * (DOF - 2)));

    logp = log (s) + GAMMA6 - 3.5 * log (1 + pow (s * (x - mean), 2) / DOF);

    return logp;
}
