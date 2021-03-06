// Copyright (c) 2013-2017 Anton Kozhevnikov, Thomas Schulthess
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that 
// the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the 
//    following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
//    and the following disclaimer in the documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED 
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/** \file radial_integrals.h
 *
 *  \brief Representation of various radial integrals.
 */

#ifndef __RADIAL_INTEGRALS_H__
#define __RADIAL_INTEGRALS_H__

#include "Unit_cell/unit_cell.h"
#include "sbessel.h"

namespace sirius {

/// Base class for all kinds of radial integrals.
template <int N>
class Radial_integrals_base
{
  protected:
    /// Unit cell.
    Unit_cell const& unit_cell_;
    
    /// Linear grid of q-points on which the interpolation of radial integrals is done.
    Radial_grid<double> grid_q_;
   
    /// Array with integrals.
    mdarray<Spline<double>, N> values_;
    
  public:
    /// Constructor.
    Radial_integrals_base(Unit_cell const& unit_cell__, double qmax__, int np__)
        : unit_cell_(unit_cell__)
    {
        grid_q_ = Radial_grid_lin<double>(static_cast<int>(np__ * qmax__), 0, qmax__);
    }
    
    /// Get starting index iq and delta dq for the q-point on the linear grid.
    /** The following condition is satisfied: q = grid_q[iq] + dq */
    inline std::pair<int, double> iqdq(double q__) const
    {
        std::pair<int, double> result;
        /* find index of q-point */
        result.first = static_cast<int>((grid_q_.num_points() - 1) * q__ / grid_q_.last());
        /* delta q = q - q_i */
        result.second = q__ - grid_q_[result.first];
        return std::move(result);
    }
};

/// Radial integrals of the augmentation operator.
template <bool jl_deriv>
class Radial_integrals_aug: public Radial_integrals_base<3>
{
  private:
    void generate()
    {
        PROFILE("sirius::Radial_integrals|aug");

        splindex<block> spl_q(grid_q_.num_points(), unit_cell_.comm().size(), unit_cell_.comm().rank());

        /* interpolate <j_{l_n}(q*x) | Q_{xi,xi'}^{l}(x) > with splines */
        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            auto& atom_type = unit_cell_.atom_type(iat);

            if (!atom_type.pp_desc().augment) {
                continue;
            }

            /* number of radial beta-functions */
            int nbrf = atom_type.mt_radial_basis_size();
            /* maximum l of beta-projectors */
            int lmax_beta = atom_type.indexr().lmax();

            for (int l = 0; l <= 2 * lmax_beta; l++) {
                for (int idx = 0; idx < nbrf * (nbrf + 1) / 2; idx++) {
                    values_(idx, l, iat) = Spline<double>(grid_q_);
                }
            }

            #pragma omp parallel for
            for (int iq_loc = 0; iq_loc < spl_q.local_size(); iq_loc++) {
                int iq = spl_q[iq_loc];

                Spherical_Bessel_functions jl(2 * lmax_beta, atom_type.radial_grid(), grid_q_[iq]);

                for (int l3 = 0; l3 <= 2 * lmax_beta; l3++) {
                    for (int idxrf2 = 0; idxrf2 < nbrf; idxrf2++) {
                        int l2 = atom_type.indexr(idxrf2).l;
                        for (int idxrf1 = 0; idxrf1 <= idxrf2; idxrf1++) {
                            int l1 = atom_type.indexr(idxrf1).l;

                            int idx = idxrf2 * (idxrf2 + 1) / 2 + idxrf1;
                            
                            if (l3 >= std::abs(l1 - l2) && l3 <= (l1 + l2) && (l1 + l2 + l3) % 2 == 0) {
                                if (jl_deriv) {
                                    auto s = jl.deriv_q(l3);
                                    values_(idx, l3, iat)[iq] = sirius::inner(s, atom_type.q_rf(idx, l3), 0,
                                                                              atom_type.num_mt_points());
                                } else {
                                    values_(idx, l3, iat)[iq] = sirius::inner(jl[l3], atom_type.q_rf(idx, l3), 0,
                                                                              atom_type.num_mt_points());
                                }
                            }
                        }
                    }
                }
            }
            for (int l = 0; l <= 2 * lmax_beta; l++) {
                for (int idx = 0; idx < nbrf * (nbrf + 1) / 2; idx++) {
                    unit_cell_.comm().allgather(&values_(idx, l, iat)[0], spl_q.global_offset(), spl_q.local_size());
                }
            }

            #pragma omp parallel for
            for (int l = 0; l <= 2 * lmax_beta; l++) {
                for (int idx = 0; idx < nbrf * (nbrf + 1) / 2; idx++) {
                    values_(idx, l, iat).interpolate();
                }
            }
        }
    }

  public:
    Radial_integrals_aug(Unit_cell const& unit_cell__, double qmax__, int np__)
        : Radial_integrals_base<3>(unit_cell__, qmax__, np__)
    {
        int nmax = unit_cell_.max_mt_radial_basis_size();
        int lmax = unit_cell_.lmax(); 

        values_ = mdarray<Spline<double>, 3>(nmax * (nmax + 1) / 2, 2 * lmax + 1, unit_cell_.num_atom_types());

        generate();
    }

    inline double value(int idx__, int l__, int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        return values_(idx__, l__, iat__)(idx.first, idx.second);
    }

    inline mdarray<double, 2> values(int iat__, double q__) const
    {
        auto idx = iqdq(q__);

        auto& atom_type = unit_cell_.atom_type(iat__);
        int lmax = atom_type.indexr().lmax();
        int nbrf = atom_type.mt_radial_basis_size();

        mdarray<double, 2> val(nbrf * (nbrf + 1) / 2, 2 * lmax + 1);
        for (int l = 0; l <= 2 * lmax; l++) {
            for (int i = 0; i < nbrf * (nbrf + 1) / 2; i++) {
                val(i, l) = values_(i, l, iat__)(idx.first, idx.second);
            }
        }
        return std::move(val);
    }
};

class Radial_integrals_rho_pseudo: public Radial_integrals_base<1>
{
  private:
    void generate()
    {
        PROFILE("sirius::Radial_integrals|rho_pseudo");

        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            auto& atom_type = unit_cell_.atom_type(iat);
            values_(iat) = Spline<double>(grid_q_);

            Spline<double> rho(atom_type.radial_grid());
            for (int ir = 0; ir < atom_type.num_mt_points(); ir++) {
                rho[ir] = atom_type.pp_desc().total_charge_density[ir];
            }
            rho.interpolate();

            #pragma omp parallel for
            for (int iq = 0; iq < grid_q_.num_points(); iq++) {
                Spherical_Bessel_functions jl(0, atom_type.radial_grid(), grid_q_[iq]);

                values_(iat)[iq] = sirius::inner(jl[0], rho, 0, atom_type.num_mt_points()) / fourpi;
            }
            values_(iat).interpolate();
        }
    }

  public:
    Radial_integrals_rho_pseudo(Unit_cell const& unit_cell__, double qmax__, int np__)
        : Radial_integrals_base<1>(unit_cell__, qmax__, np__)
    {
        values_ = mdarray<Spline<double>, 1>(unit_cell_.num_atom_types());
        generate();
    }

    inline double value(int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        return values_(iat__)(idx.first, idx.second);
    }
};

template <bool jl_deriv>
class Radial_integrals_rho_core_pseudo: public Radial_integrals_base<1>
{
  private:
    void generate()
    {
        PROFILE("sirius::Radial_integrals|rho_core_pseudo");

        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            auto& atom_type = unit_cell_.atom_type(iat);
            values_(iat) = Spline<double>(grid_q_);

            Spline<double> ps_core(atom_type.radial_grid());
            for (int ir = 0; ir < atom_type.num_mt_points(); ir++) {
                ps_core[ir] = atom_type.pp_desc().core_charge_density[ir];
            }
            ps_core.interpolate();

            #pragma omp parallel for
            for (int iq = 0; iq < grid_q_.num_points(); iq++) {
                Spherical_Bessel_functions jl(0, atom_type.radial_grid(), grid_q_[iq]);

                if (jl_deriv) {
                    auto s = jl.deriv_q(0);
                    values_(iat)[iq] = sirius::inner(s, ps_core, 2, atom_type.num_mt_points());
                } else {
                    values_(iat)[iq] = sirius::inner(jl[0], ps_core, 2, atom_type.num_mt_points());
                }
            }
            values_(iat).interpolate();
        }
    }

  public:
    Radial_integrals_rho_core_pseudo(Unit_cell const& unit_cell__, double qmax__, int np__)
        : Radial_integrals_base<1>(unit_cell__, qmax__, np__)
    {
        values_ = mdarray<Spline<double>, 1>(unit_cell_.num_atom_types());
        generate();
    }

    inline double value(int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        return values_(iat__)(idx.first, idx.second);
    }
};

template <bool jl_deriv>
class Radial_integrals_beta: public Radial_integrals_base<2>
{
  private:
    void generate()
    {
        PROFILE("sirius::Radial_integrals|beta");
    
        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            auto& atom_type = unit_cell_.atom_type(iat);
            int nrb = atom_type.mt_radial_basis_size();

            for (int idxrf = 0; idxrf < nrb; idxrf++) {
                values_(idxrf, iat) = Spline<double>(grid_q_);
            }
    
            #pragma omp parallel for
            for (int iq = 0; iq < grid_q_.num_points(); iq++) {
                Spherical_Bessel_functions jl(unit_cell_.lmax(), atom_type.radial_grid(), grid_q_[iq]);
                for (int idxrf = 0; idxrf < nrb; idxrf++) {
                    int l  = atom_type.indexr(idxrf).l;
                    int nr = atom_type.pp_desc().num_beta_radial_points[idxrf];
                    /* compute \int j_l(q * r) beta_l(r) r^2 dr or \int d (j_l(q*r) / dq) beta_l(r) r^2  */
                    /* remeber that beta(r) are defined as miltiplied by r */
                    if (jl_deriv) {
                        auto s = jl.deriv_q(l);
                        values_(idxrf, iat)[iq] = sirius::inner(s, atom_type.beta_rf(idxrf), 1, nr);
                    } else {
                        values_(idxrf, iat)[iq] = sirius::inner(jl[l], atom_type.beta_rf(idxrf), 1, nr);
                    }
                }
            }

            for (int idxrf = 0; idxrf < nrb; idxrf++) {
                values_(idxrf, iat).interpolate();
            }
        }
    }

  public:
    Radial_integrals_beta(Unit_cell const& unit_cell__, double qmax__, int np__)
        : Radial_integrals_base<2>(unit_cell__, qmax__, np__)
    {
        /* create space for <j_l(qr)|beta> or <d j_l(qr) / dq|beta> radial integrals */
        values_ = mdarray<Spline<double>, 2>(unit_cell_.max_mt_radial_basis_size(), unit_cell_.num_atom_types());
        generate();
    }

    inline double value(int idxrf__, int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        return values_(idxrf__, iat__)(idx.first, idx.second);
    }

    inline mdarray<double, 1> values(int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        auto& atom_type = unit_cell_.atom_type(iat__);
        mdarray<double, 1> val(atom_type.mt_radial_basis_size());
        for (int i = 0; i < atom_type.mt_radial_basis_size(); i++) {
            val(i) = values_(i, iat__)(idx.first, idx.second);
        }
        return std::move(val);
    }
};

class Radial_integrals_beta_jl: public Radial_integrals_base<3>
{
  private:
    int lmax_;

    void generate()
    {
        PROFILE("sirius::Radial_integrals|beta_jl");
    
        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            auto& atom_type = unit_cell_.atom_type(iat);
            int nrb = atom_type.mt_radial_basis_size();

            for (int idxrf = 0; idxrf < nrb; idxrf++) {
                for (int l1 = 0; l1 <= lmax_; l1++) {
                    values_(idxrf, l1, iat) = Spline<double>(grid_q_);
                }
            }

            #pragma omp parallel for
            for (int iq = 0; iq < grid_q_.num_points(); iq++) {
                Spherical_Bessel_functions jl(lmax_, atom_type.radial_grid(), grid_q_[iq]);
                for (int idxrf = 0; idxrf < nrb; idxrf++) {
                    int nr = atom_type.pp_desc().num_beta_radial_points[idxrf];
                    for (int l1 = 0; l1 <= lmax_; l1++) {
                        /* compute \int j_{l'}(q * r) beta_l(r) r^2 * r * dr */
                        /* remeber that beta(r) are defined as miltiplied by r */
                        values_(idxrf, l1, iat)[iq] = sirius::inner(jl[l1], atom_type.beta_rf(idxrf), 2, nr);
                    }
                }
            }

            for (int idxrf = 0; idxrf < nrb; idxrf++) {
                for (int l1 = 0; l1 <= lmax_; l1++) {
                    values_(idxrf, l1, iat).interpolate();
                }
            }
        }
    }

  public:
    Radial_integrals_beta_jl(Unit_cell const& unit_cell__, double qmax__, int np__)
        : Radial_integrals_base<3>(unit_cell__, qmax__, np__)
    {
        lmax_ = unit_cell__.lmax() + 2;
        /* create space for <j_l(qr)|beta> radial integrals */
        values_ = mdarray<Spline<double>, 3>(unit_cell_.max_mt_radial_basis_size(), lmax_ + 1, unit_cell_.num_atom_types());
        generate();
    }

    inline double value(int idxrf__, int l__, int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        return values_(idxrf__, l__, iat__)(idx.first, idx.second);
    }
};

/// Radial integrals for the step function of the LAPW method.
/** Radial integrals have the following expression:
 *  \f[
 *      \Theta(\alpha, G) = \int_{0}^{R_{\alpha}} \frac{\sin(Gr)}{Gr} r^2 dr = 
 *          \left\{ \begin{array}{ll} \displaystyle R_{\alpha}^3 / 3 & G=0 \\
 *          \Big( \sin(GR_{\alpha}) - GR_{\alpha}\cos(GR_{\alpha}) \Big) / G^3 & G \ne 0 \end{array} \right.
 *  \f]
 */
class Radial_integrals_theta: public Radial_integrals_base<1>
{
  private:
    void generate()
    {
        PROFILE("sirius::Radial_integrals|theta");

        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            auto& atom_type = unit_cell_.atom_type(iat);
            auto R = atom_type.mt_radius();
            values_(iat) = Spline<double>(grid_q_);

            #pragma omp parallel for
            for (int iq = 0; iq < grid_q_.num_points(); iq++) {
                if (iq == 0) {
                    values_(iat)[iq] = std::pow(R, 3) / 3.0;
                } else {
                    double g = grid_q_[iq];
                    values_(iat)[iq] = (std::sin(g * R) - g * R * std::cos(g * R)) / std::pow(g, 3);
                }
            }
            values_(iat).interpolate();
        }
    }

  public:
    Radial_integrals_theta(Unit_cell const& unit_cell__, double qmax__, int np__)
        : Radial_integrals_base<1>(unit_cell__, qmax__, np__)
    {
        values_ = mdarray<Spline<double>, 1>(unit_cell_.num_atom_types());
        generate();
    }

    inline double value(int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        return values_(iat__)(idx.first, idx.second);
    }
};

template <bool jl_deriv>
class Radial_integrals_vloc: public Radial_integrals_base<1>
{
  private:
    void generate()
    {
        PROFILE("sirius::Radial_integrals|vloc");

        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            auto& atom_type = unit_cell_.atom_type(iat);
            values_(iat) = Spline<double>(grid_q_);

            int np = atom_type.radial_grid().index_of(10);
            if (np == -1) {
                np = atom_type.num_mt_points();
            }
            auto rg = atom_type.radial_grid().segment(np);

            #pragma omp parallel for
            for (int iq = 0; iq < grid_q_.num_points(); iq++) {
                Spline<double> s(rg);
                double g = grid_q_[iq];

                if (jl_deriv) { /* integral with derivative of j0(q*r) over q */
                    for (int ir = 0; ir < rg.num_points(); ir++) {
                        double x = rg[ir];
                        s[ir] = (x * atom_type.pp_desc().vloc[ir] + atom_type.zn() * gsl_sf_erf(x)) * (std::sin(g * x) - g * x * std::cos(g * x));
                    }
                } else { /* integral with j0(q*r) */
                    if (iq == 0) {
                        for (int ir = 0; ir < rg.num_points(); ir++) {
                            double x = rg[ir];
                            s[ir] = (x * atom_type.pp_desc().vloc[ir] + atom_type.zn()) * x;
                        }
                    } else {
                        for (int ir = 0; ir < rg.num_points(); ir++) {
                            double x = rg[ir];
                            s[ir] = (x * atom_type.pp_desc().vloc[ir] + atom_type.zn() * gsl_sf_erf(x)) * std::sin(g * x);
                        }
                    }
                }
                values_(iat)[iq] = s.interpolate().integrate(0);
            }
            values_(iat).interpolate();
        }
    }
  
  public:
    Radial_integrals_vloc(Unit_cell const& unit_cell__, double qmax__, int np__)
        : Radial_integrals_base<1>(unit_cell__, qmax__, np__)
    {
        values_ = mdarray<Spline<double>, 1>(unit_cell_.num_atom_types());
        generate();
    }

    inline double value(int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        if (std::abs(q__) < 1e-12) {
            if (jl_deriv) {
                return 0;
            } else {
                return values_(iat__)[0];
            }
        } else {
            auto& atom_type = unit_cell_.atom_type(iat__);
            auto q2 = std::pow(q__, 2);
            if (jl_deriv) {
                return values_(iat__)(idx.first, idx.second) / q2 / q__ - atom_type.zn() * std::exp(-q2 / 4) * (4 + q2) / 2 / q2 / q2;
            } else {
                return values_(iat__)(idx.first, idx.second) / q__ - atom_type.zn() * std::exp(-q2 / 4) / q2;
            }
        }
    }
};

class Radial_integrals_rho_free_atom: public Radial_integrals_base<1>
{
  private:
    void generate()
    {
        PROFILE("sirius::Radial_integrals|rho_free_atom");

        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            auto& atom_type = unit_cell_.atom_type(iat);
            values_(iat) = Spline<double>(grid_q_);

            #pragma omp parallel for
            for (int iq = 0; iq < grid_q_.num_points(); iq++) {
                double g = grid_q_[iq];
                Spline<double> s(unit_cell_.atom_type(iat).free_atom_radial_grid());
                if (iq == 0) {
                    for (int ir = 0; ir < s.num_points(); ir++) {
                        s[ir] = atom_type.free_atom_density(ir);
                    }
                    values_(iat)[iq] = s.interpolate().integrate(2);
                } else {
                    for (int ir = 0; ir < s.num_points(); ir++) {
                        s[ir] = atom_type.free_atom_density(ir) * std::sin(g * atom_type.free_atom_radial_grid(ir)) / g;
                    }
                    values_(iat)[iq] = s.interpolate().integrate(1);
                }
            }
            values_(iat).interpolate();
        }
    }

  public:
    Radial_integrals_rho_free_atom(Unit_cell const& unit_cell__, double qmax__, int np__)
        : Radial_integrals_base<1>(unit_cell__, qmax__, np__)
    {
        values_ = mdarray<Spline<double>, 1>(unit_cell_.num_atom_types());
        generate();
    }

    inline double value(int iat__, double q__) const
    {
        auto idx = iqdq(q__);
        return values_(iat__)(idx.first, idx.second);
    }
};

} // namespace

#endif // __RADIAL_INTEGRALS_H__


