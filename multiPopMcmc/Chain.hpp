#ifndef CHAIN_HPP
#define CHAIN_HPP

#include <array>
#include <ostream>
#include <iostream>

#include "Cluster.hpp"
#include "Matrix.hpp"
#include "McmcApplication.hpp"
#include "MpiMcmcApplication.hpp"

using std::ostream;
using std::cout;
using std::endl;
using std::cerr;

template <class T>
class Chain : public McmcApplication
{
  private:
    MVatrix<double, NPARAMS> params;
    std::array<double, NPARAMS> priorVar;

    T clust;

    ostream &fout;
    ostream &starParams;

    double logPostCurr = -std::numeric_limits<double>::infinity();

    std::vector<double> starData;

  public:
    Chain(uint32_t seed, std::array<double, NPARAMS> priorVar, T clust, ostream &fout, ostream &starParams)
        : McmcApplication(seed), priorVar(priorVar), clust(clust), fout(fout), starParams(starParams)
    {}

    void reset()
    {
        resetRatio();

        for ( auto &p : params )
        {
            p.clear();
        }
    }

    void run(std::function<T(T)> propose, std::function<std::tuple<double, std::vector<double>>(T&)> logPost, std::function<void(const T&)> checkPriors, int iters, int thin = 1)
    {
        for (int iteration = 0; iteration < iters * thin; iteration++)
        {
            double logPostProp = 0.0;
            std::vector<double> logPostStarData;

            auto propClust = propose (clust);

            try
            {
                checkPriors(propClust);

                auto lpOut      = logPost (propClust);
		logPostProp     = std::get<0>(lpOut);
		logPostStarData = std::get<1>(lpOut);
            }
            catch(InvalidCluster &e)
            {
                logPostProp = -std::numeric_limits<double>::infinity();
            }

            /* accept/reject */
            if (acceptClustMarg (logPostCurr, logPostProp))
            {
                clust = propClust;
                logPostCurr = logPostProp;
                starData = logPostStarData;
            }

            /* save draws to estimate covariance matrix for more efficient Metropolis */
            for (int p = 0; p < NPARAMS; p++)
            {
                if (p == YYA)
                    params.at(p).push_back(clust.clust[0].yyy);
                else if (p == YYB)
                    params.at(p).push_back(clust.clust[1].yyy);
                else if (p == LAMBDA)
                    params.at(p).push_back(clust.lambda);
                else if (priorVar[p] > EPSILON)
                {
                    params.at(p).push_back(clust.clust[0].getParam(p));
                }
            }

            if (iteration % thin == 0)
            {
                for (int p = 0; p < NPARAMS; p++)
                {
                    if (priorVar.at(p) > EPS || p == FEH || p == MOD || p == ABS)
                    {
                        fout << base::utility::format << clust.clust[0].getParam(p) << ' ';
                    }
                }

                fout << base::utility::format << clust.clust[0].yyy << ' '
                     << base::utility::format << clust.clust[1].yyy << ' '
                     << base::utility::format << clust.lambda << ' ';


                fout << base::utility::format << logPostCurr << endl;
            }

	    for (auto a : starData)
	    {
                starParams << base::utility::format << a;
	    }

            starParams << std::endl;
        }
    }

    Matrix<double, NPARAMS, NPARAMS> makeCholeskyDecomp() const
    {
        double cov;
        int nParamsUsed = 0;

        Matrix<double, NPARAMS, NPARAMS> propMatrix;

        for (int p = 0; p < NPARAMS; p++)
        {
            if (priorVar.at(p) > EPSILON || p == YYA || p == YYB || p == LAMBDA)
            {
                nParamsUsed++;
            }
        }

        /* compute Cholesky decomposition of covariance matrix */
        int h, k;
        gsl_matrix *covMat = gsl_matrix_alloc (nParamsUsed, nParamsUsed);

        h = 0;

        double cholScale = 1000;    /* for numerical stability */

        for (int i = 0; i < NPARAMS; i++)
        {
            if (priorVar.at(i) > EPSILON || i == YYA || i == YYB || i == LAMBDA)
            {
                k = 0;
                for (int j = 0; j < NPARAMS; j++)
                {
                    if (priorVar.at(j) > EPSILON || j == YYA || j == YYB || j == LAMBDA)
                    {
                        cov = gsl_stats_covariance (params.at(i).data(), 1, params.at(j).data(), 1, params.at(i).size());
                        gsl_matrix_set (covMat, h, k, cov * cholScale * cholScale); /* for numerical stability? */

                        if (h != k)
                        {
                            gsl_matrix_set (covMat, k, h, cov * cholScale * cholScale);
                        }

                        k++;
                    }
                }
                h++;
            }
        }

        // for (int k = 0; k < params.at(0).size(); ++k)
        // {
        //     for (int i = 0; i < NPARAMS; i++)
        //     {
        //         if (priorVar.at(i) > EPSILON || i == YYA || i == YYB || i == LAMBDA)
        //         {
        //             cout << base::utility::format << params.at(i).at(k) << " ";
        //         }
        //     }

        //     cout << '\n';
        // }

        // cout << endl;

        // for (int i = 0; i < nParamsUsed; i++)
        // {
        //     for (int j = 0; j < nParamsUsed; j++)
        //     {
        //         cout << base::utility::format << gsl_matrix_get (covMat, i, j) << " ";
        //     }
        //     cout << endl;
        // }

        /* Cholesky decomposition */
        gsl_linalg_cholesky_decomp (covMat);

        /* compute proposal matrix from Cholesky factor */

        /* Gelman, Roberts, Gilks scale */
        double GRGscale = 2.38 / sqrt(nParamsUsed);

        h = 0;
        for (int i = 0; i < NPARAMS; i++)
        {
            if (priorVar.at(i) > EPSILON || i == YYA || i == YYB || i == LAMBDA)
            {
                k = 0;
                for (int j = 0; j < NPARAMS; j++)
                {
                    if (priorVar.at(j) > EPSILON || j == YYA || j == YYB || j == LAMBDA)
                    {
                        if (j <= i)
                        {
                            propMatrix.at(i).at(j) = GRGscale * gsl_matrix_get (covMat, h, k) / cholScale;
                        }
                        else
                        {
                            propMatrix.at(i).at(j) = 0.0;
                        }
                        k++;
                    }
                    else
                    {
                        propMatrix.at(i).at(j) = 0.0;
                    }
                }
                h++;
            }
            else
            {
                for (int j = 0; j < NPARAMS; j++)
                {
                    propMatrix.at(i).at(j) = 0.0;
                }
            }
        }

        return propMatrix;
    }

    T getCluster() const { return clust; };
};

#endif
