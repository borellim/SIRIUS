// Copyright (c) 2013-2016 Anton Kozhevnikov, Thomas Schulthess
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

/** \file k_point.h
 *   
 *  \brief Contains definition and partial implementation of sirius::K_point class.
 */

#ifndef __K_POINT_H__
#define __K_POINT_H__

#include "periodic_function.h"
#include "matching_coefficients.h"
#include "Beta_projectors/beta_projectors.h"
#include "wave_functions.hpp"

namespace sirius
{

/// K-point related variables and methods.
/** \image html wf_storage.png "Wave-function storage"
 *  \image html fv_eigen_vectors.png "First-variational eigen vectors" */
class K_point
{
    private:

        /// Simulation context.
        Simulation_context& ctx_;
        
        /// Unit cell object.
        Unit_cell const& unit_cell_;

        /// Weight of k-point.
        double weight_;

        /// Fractional k-point coordinates.
        vector3d<double> vk_;
        
        /// List of G-vectors with |G+k| < cutoff.
        Gvec gkvec_;

        /// First-variational eigen values
        std::vector<double> fv_eigen_values_;

        /// First-variational eigen vectors, distributed over 2D BLACS grid.
        dmatrix<double_complex> fv_eigen_vectors_;

        /// First-variational eigen vectors, distributed in slabs.
        std::unique_ptr<wave_functions> fv_eigen_vectors_slab_;

        std::unique_ptr<wave_functions> singular_components_;
        
        /// Second-variational eigen vectors.
        /** Second-variational eigen-vectors are stored as one or two \f$ N_{fv} \times N_{fv} \f$ matrices in
         *  case of non-magnetic or collinear magnetic case or as a single \f$ 2 N_{fv} \times 2 N_{fv} \f$ 
         *  matrix in case of general non-collinear magnetism. */
        dmatrix<double_complex> sv_eigen_vectors_[2];

        /// Full-diagonalization eigen vectors.
        mdarray<double_complex, 2> fd_eigen_vectors_;

        /// First-variational states.
        std::unique_ptr<wave_functions> fv_states_{nullptr};

        /// Two-component (spinor) wave functions describing the bands.
        std::unique_ptr<Wave_functions> spinor_wave_functions_{nullptr};

        /// Band occupation numbers.
        std::vector<double> band_occupancies_;

        /// Band energies.
        std::vector<double> band_energies_; 

        std::unique_ptr<Matching_coefficients> alm_coeffs_row_{nullptr};

        std::unique_ptr<Matching_coefficients> alm_coeffs_col_{nullptr};

        std::unique_ptr<Matching_coefficients> alm_coeffs_loc_{nullptr};

        std::vector<int> igk_row_;

        std::vector<int> igk_col_;

        std::vector<int> igk_loc_;

        /// Number of G+k vectors distributed along rows of MPI grid
        int num_gkvec_row_{0};
        
        /// Number of G+k vectors distributed along columns of MPI grid
        int num_gkvec_col_{0};
        
        /// Offset of the local fraction of G+k vectors in the global index.
        int gkvec_offset_{0};

        /// Basis descriptors distributed between rows of the 2D MPI grid.
        /** This is a local array. Only MPI ranks belonging to the same column have identical copies of this array. */
        std::vector<lo_basis_descriptor> lo_basis_descriptors_row_;
        
        /// Basis descriptors distributed between columns of the 2D MPI grid.
        /** This is a local array. Only MPI ranks belonging to the same row have identical copies of this array. */
        std::vector<lo_basis_descriptor> lo_basis_descriptors_col_;

        /// List of columns of the Hamiltonian and overlap matrix lo block (local index) for a given atom.
        std::vector<std::vector<int>> atom_lo_cols_;

        /// list of rows of the Hamiltonian and overlap matrix lo block (local index) for a given atom
        std::vector<std::vector<int>> atom_lo_rows_;

        /// Imaginary unit to the power of l.
        std::vector<double_complex> zil_;

        /// Mapping between lm and l.
        mdarray<int, 1> l_by_lm_;

        /// column rank of the processors of ScaLAPACK/ELPA diagonalization grid
        int rank_col_;

        /// number of processors along columns of the diagonalization grid
        int num_ranks_col_;

        int rank_row_;

        int num_ranks_row_;

        std::unique_ptr<Beta_projectors> beta_projectors_{nullptr};
       
        /// Preconditioner matrix for Chebyshev solver.  
        mdarray<double_complex, 3> p_mtrx_;

        Communicator const& comm_;

        /// Communicator between(!!) rows.
        Communicator const& comm_row_;

        /// Communicator between(!!) columns.
        Communicator const& comm_col_;

        inline void generate_gklo_basis();

        /// Test orthonormalization of first-variational states.
        inline void test_fv_states();

    public:

        /// Constructor
        K_point(Simulation_context& ctx__,
                double* vk__,
                double weight__)
            : ctx_(ctx__)
            , unit_cell_(ctx_.unit_cell())
            , weight_(weight__)
            , comm_(ctx_.blacs_grid().comm())
            , comm_row_(ctx_.blacs_grid().comm_row())
            , comm_col_(ctx_.blacs_grid().comm_col())
        {
            PROFILE("sirius::K_point::K_point");

            for (int x = 0; x < 3; x++) {
                vk_[x] = vk__[x];
            }
            
            band_occupancies_ = std::vector<double>(ctx_.num_bands(), 1);
            band_energies_    = std::vector<double>(ctx_.num_bands(), 0);
            
            num_ranks_row_ = comm_row_.size();
            num_ranks_col_ = comm_col_.size();
            
            rank_row_ = comm_row_.rank();
            rank_col_ = comm_col_.rank();
        }

        /// Find G+k vectors within the cutoff.
        inline void generate_gkvec(double gk_cutoff__)
        {
            PROFILE("sirius::K_point::generate_gkvec");

            if (ctx_.full_potential() && (gk_cutoff__ * unit_cell_.max_mt_radius() > ctx_.lmax_apw())) {
                std::stringstream s;
                s << "G+k cutoff (" << gk_cutoff__ << ") is too large for a given lmax (" 
                  << ctx_.lmax_apw() << ") and a maximum MT radius (" << unit_cell_.max_mt_radius() << ")" << std::endl
                  << "suggested minimum value for lmax : " << int(gk_cutoff__ * unit_cell_.max_mt_radius()) + 1;
                WARNING(s);
            }

            if (gk_cutoff__ * 2 > ctx_.pw_cutoff()) {
                std::stringstream s;
                s << "G+k cutoff is too large for a given plane-wave cutoff" << std::endl
                  << "  pw cutoff : " << ctx_.pw_cutoff() << std::endl
                  << "  doubled G+k cutoff : " << gk_cutoff__ * 2;
                TERMINATE(s);
            }

            /* create G+k vectors; communicator of the coarse FFT grid is used because wave-functions will be transformed 
             * only on the coarse grid; G+k-vectors will be distributed between MPI ranks assigned to the k-point */
            gkvec_ = Gvec(vk_, ctx_.unit_cell().reciprocal_lattice_vectors(), gk_cutoff__, comm(), ctx_.comm_fft_coarse(),
                          ctx_.gamma_point());
            
            gkvec_offset_ = gkvec_.gvec_offset(comm_.rank());
        }

        /// Initialize the k-point related arrays and data.
        inline void initialize();

        /// Generate first-variational states from eigen-vectors.
        /** First-variational states are obtained from the first-variational eigen-vectors and 
         *  LAPW matching coefficients.
         *
         *  APW part:
         *  \f[
         *      \psi_{\xi j}^{\bf k} = \sum_{{\bf G}} Z_{{\bf G} j}^{\bf k} * A_{\xi}({\bf G+k})
         *  \f]
         */
        void generate_fv_states();

        #ifdef __GPU
        void generate_fv_states_aw_mt_gpu();
        #endif

        /// Generate two-component spinor wave functions 
        inline void generate_spinor_wave_functions();

        //Periodic_function<double_complex>* spinor_wave_function_component(int lmax, int ispn, int j);

        void save(int id);

        void load(HDF5_tree h5in, int id);

        //== void save_wave_functions(int id);

        //== void load_wave_functions(int id);

        void get_fv_eigen_vectors(mdarray<double_complex, 2>& fv_evec);
        
        void get_sv_eigen_vectors(mdarray<double_complex, 2>& sv_evec);
        
        /// Test orthonormalization of spinor wave-functions
        void test_spinor_wave_functions(int use_fft);

        /// Get the number of occupied bands for each spin channel.
        int num_occupied_bands(int ispn__ = -1)
        {
            if (ctx_.num_mag_dims() == 3) {
                for (int j = ctx_.num_bands() - 1; j >= 0; j--) {
                    if (std::abs(band_occupancy(j) * weight()) > 1e-14) {
                        return j + 1;
                    }
                }
            }

            if (!(ispn__ == 0 || ispn__ == 1)) {
                TERMINATE("wrong spin channel");
            }

            for (int i = ctx_.num_fv_states() - 1; i >= 0; i--) {
                int j = i + ispn__ * ctx_.num_fv_states();
                if (std::abs(band_occupancy(j) * weight()) > 1e-14) {
                    return i + 1;
                }
            }
            
            return 0;
        }

        /// Total number of G+k vectors within the cutoff distance
        inline int num_gkvec() const
        {
            return gkvec_.num_gvec();
        }

        /// Total number of muffin-tin and plane-wave expansion coefficients for the wave-functions.
        /** APW+lo basis \f$ \varphi_{\mu {\bf k}}({\bf r}) = \{ \varphi_{\bf G+k}({\bf r}),
         *  \varphi_{j{\bf k}}({\bf r}) \} \f$ is used to expand first-variational wave-functions:
         *
         *  \f[
         *      \psi_{i{\bf k}}({\bf r}) = \sum_{\mu} c_{\mu i}^{\bf k} \varphi_{\mu \bf k}({\bf r}) = 
         *      \sum_{{\bf G}}c_{{\bf G} i}^{\bf k} \varphi_{\bf G+k}({\bf r}) + 
         *      \sum_{j}c_{j i}^{\bf k}\varphi_{j{\bf k}}({\bf r})
         *  \f]
         *
         *  Inside muffin-tins the expansion is converted into the following form:
         *  \f[
         *      \psi_{i {\bf k}}({\bf r})= \begin{array}{ll} 
         *      \displaystyle \sum_{L} \sum_{\lambda=1}^{N_{\ell}^{\alpha}} 
         *      F_{L \lambda}^{i {\bf k},\alpha}f_{\ell \lambda}^{\alpha}(r) 
         *      Y_{\ell m}(\hat {\bf r}) & {\bf r} \in MT_{\alpha} \end{array}
         *  \f]
         *
         *  Thus, the total number of coefficients representing a wave-funstion is equal
         *  to the number of muffin-tin basis functions of the form \f$ f_{\ell \lambda}^{\alpha}(r) 
         *  Y_{\ell m}(\hat {\bf r}) \f$ plust the number of G+k plane waves. */ 
        //inline int wf_size() const // TODO: better name for this
        //{
        //    if (ctx_.full_potential()) {
        //        return unit_cell_.mt_basis_size() + num_gkvec();
        //    } else {
        //        return num_gkvec();
        //    }
        //}

        inline void get_band_occupancies(double* band_occupancies) const
        {
            assert(static_cast<int>(band_occupancies_.size()) == ctx_.num_bands());
            
            std::memcpy(band_occupancies, &band_occupancies_[0], ctx_.num_bands() * sizeof(double));
        }

        inline void set_band_occupancies(double* band_occupancies)
        {
            band_occupancies_.resize(ctx_.num_bands());
            std::memcpy(&band_occupancies_[0], band_occupancies, ctx_.num_bands() * sizeof(double));
        }

        inline void get_band_energies(double* band_energies) const
        {
            assert(static_cast<int>(band_energies_.size()) == ctx_.num_bands());
            std::memcpy(band_energies, &band_energies_[0], ctx_.num_bands() * sizeof(double));
        }

        inline void set_band_energies(double* band_energies)
        {
            band_energies_.resize(ctx_.num_bands()); 
            std::memcpy(&band_energies_[0], band_energies, ctx_.num_bands() * sizeof(double));
        }

        inline double band_occupancy(int j) const
        {
            return band_occupancies_[j];
        }

        inline double& band_occupancy(int j)
        {
            return band_occupancies_[j];
        }
        
        inline double band_energy(int j) const
        {
            return band_energies_[j];
        }

        inline double& band_energy(int j__)
        {
            return band_energies_[j__];
        }

        inline double fv_eigen_value(int i) const
        {
            return fv_eigen_values_[i];
        }

        void set_fv_eigen_values(double* eval)
        {
            std::memcpy(&fv_eigen_values_[0], eval, ctx_.num_fv_states() * sizeof(double));
        }
        
        inline double weight() const
        {
            return weight_;
        }

        inline wave_functions& fv_states()
        {
            return *fv_states_;
        }

        inline wave_functions& spinor_wave_functions(int ispn__)
        {
            return spinor_wave_functions_->component(ispn__);
        }

        inline Wave_functions& spinor_wave_functions()
        {
            return *spinor_wave_functions_;
        }

        inline wave_functions& singular_components()
        {
            return *singular_components_;
        }

        inline vector3d<double> vk() const
        {
            return vk_;
        }

        /// Basis size of LAPW+lo method.
        inline int gklo_basis_size() const
        {
            return static_cast<int>(num_gkvec() + unit_cell_.mt_lo_basis_size());
        }

        /// Local number of G+k vectors in case of flat distribution.
        inline int num_gkvec_loc() const
        {
            return gkvec_.gvec_count(comm_.rank());
        }

        /// Return global index of G+k vector.
        inline int idxgk(int igkloc__) const
        {
            return gkvec_offset_ + igkloc__;
        }
        
        /// Local number of G+k vectors for each MPI rank in the row of the 2D MPI grid.
        inline int num_gkvec_row() const
        {
            return num_gkvec_row_;
        }

        /// Local number of local orbitals for each MPI rank in the row of the 2D MPI grid.
        inline int num_lo_row() const
        {
            return static_cast<int>(lo_basis_descriptors_row_.size());
        }

        /// Local number of basis functions for each MPI rank in the row of the 2D MPI grid.
        inline int gklo_basis_size_row() const
        {
            return num_gkvec_row() + num_lo_row();
        }

        /// Local number of G+k vectors for each MPI rank in the column of the 2D MPI grid.
        inline int num_gkvec_col() const
        {
            return num_gkvec_col_;
        }
        
        /// Local number of local orbitals for each MPI rank in the column of the 2D MPI grid.
        inline int num_lo_col() const
        {
            return static_cast<int>(lo_basis_descriptors_col_.size());
        }

        /// Local number of basis functions for each MPI rank in the column of the 2D MPI grid.
        inline int gklo_basis_size_col() const
        {
            return num_gkvec_col() + num_lo_col();
        }

        inline lo_basis_descriptor const& lo_basis_descriptor_col(int idx) const
        {
            assert(idx >=0 && idx < (int)lo_basis_descriptors_col_.size());
            return lo_basis_descriptors_col_[idx];
        }
        
        inline lo_basis_descriptor const& lo_basis_descriptor_row(int idx) const
        {
            assert(idx >= 0 && idx < (int)lo_basis_descriptors_row_.size());
            return lo_basis_descriptors_row_[idx];
        }

        inline int igk_loc(int idx__) const
        {
            return igk_loc_[idx__];
        }

        inline int igk_row(int idx__) const
        {
            return igk_row_[idx__];
        }

        inline int igk_col(int idx__) const
        {
            return igk_col_[idx__];
        }

        inline int num_ranks_row() const
        {
            return num_ranks_row_;
        }
        
        inline int rank_row() const
        {
            return rank_row_;
        }
        
        inline int num_ranks_col() const
        {
            return num_ranks_col_;
        }
        
        inline int rank_col() const
        {
            return rank_col_;
        }
       
        /// Number of MPI ranks for a given k-point
        inline int num_ranks() const
        {
            return comm_.size();
        }

        inline int rank() const
        {
            return comm_.rank();
        }

        /// Return number of lo columns for a given atom
        inline int num_atom_lo_cols(int ia) const
        {
            return (int)atom_lo_cols_[ia].size();
        }

        /// Return local index (for the current MPI rank) of a column for a given atom and column index within an atom
        inline int lo_col(int ia, int i) const
        {
            return atom_lo_cols_[ia][i];
        }
        
        /// Return number of lo rows for a given atom
        inline int num_atom_lo_rows(int ia) const
        {
            return (int)atom_lo_rows_[ia].size();
        }

        /// Return local index (for the current MPI rank) of a row for a given atom and row index within an atom
        inline int lo_row(int ia, int i) const
        {
            return atom_lo_rows_[ia][i];
        }

        inline dmatrix<double_complex>& fv_eigen_vectors()
        {
            return fv_eigen_vectors_;
        }

        inline wave_functions& fv_eigen_vectors_slab()
        {
            return *fv_eigen_vectors_slab_;
        }
        
        inline dmatrix<double_complex>& sv_eigen_vectors(int ispn)
        {
            return sv_eigen_vectors_[ispn];
        }
        
        inline mdarray<double_complex, 2>& fd_eigen_vectors()
        {
            return fd_eigen_vectors_;
        }

        void bypass_sv()
        {
            std::memcpy(&band_energies_[0], &fv_eigen_values_[0], ctx_.num_fv_states() * sizeof(double));
        }

        inline Gvec const& gkvec() const
        {
            return gkvec_;
        }

        inline Matching_coefficients const& alm_coeffs_row()
        {
            return *alm_coeffs_row_;
        }

        inline Matching_coefficients const& alm_coeffs_col()
        {
            return *alm_coeffs_col_;
        }

        inline Matching_coefficients const& alm_coeffs_loc() const
        {
            return *alm_coeffs_loc_;
        }

        inline Communicator const& comm() const
        {
            return comm_;
        }

        inline Communicator const& comm_row() const
        {
            return comm_row_;
        }

        inline Communicator const& comm_col() const
        {
            return comm_col_;
        }

        inline double_complex p_mtrx(int xi1, int xi2, int iat) const
        {
            return p_mtrx_(xi1, xi2, iat);
        }

        inline mdarray<double_complex, 3>& p_mtrx()
        {
            return p_mtrx_;
        }

        Beta_projectors& beta_projectors()
        {
            assert(beta_projectors_ != nullptr);
            return *beta_projectors_;
        }
};

#include "K_point/generate_fv_states.hpp"
#include "K_point/generate_spinor_wave_functions.hpp"
#include "K_point/generate_gklo_basis.hpp"
#include "K_point/initialize.hpp"
#include "K_point/k_point.hpp"
#include "K_point/test_fv_states.hpp"

}

/** \page basis Basis functions for Kohn-Sham wave-functions expansion
 *   
 *  \section basis1 LAPW+lo basis
 *
 *  LAPW+lo basis consists of two different sets of functions: LAPW functions \f$ \varphi_{{\bf G+k}} \f$ defined over 
 *  entire unit cell:
 *  \f[
 *      \varphi_{{\bf G+k}}({\bf r}) = \left\{ \begin{array}{ll}
 *      \displaystyle \sum_{L} \sum_{\nu=1}^{O_{\ell}^{\alpha}} a_{L\nu}^{\alpha}({\bf G+k})u_{\ell \nu}^{\alpha}(r) 
 *      Y_{\ell m}(\hat {\bf r}) & {\bf r} \in {\rm MT} \alpha \\
 *      \displaystyle \frac{1}{\sqrt  \Omega} e^{i({\bf G+k}){\bf r}} & {\bf r} \in {\rm I} \end{array} \right.
 *  \f]  
 *  and Bloch sums of local orbitals defined inside muffin-tin spheres only:
 *  \f[
 *      \begin{array}{ll} \displaystyle \varphi_{j{\bf k}}({\bf r})=\sum_{{\bf T}} e^{i{\bf kT}} 
 *      \varphi_{j}({\bf r - T}) & {\rm {\bf r} \in MT} \end{array}
 *  \f]
 *  Each local orbital is composed of radial and angular parts:
 *  \f[
 *      \varphi_{j}({\bf r}) = \phi_{\ell_j}^{\zeta_j,\alpha_j}(r) Y_{\ell_j m_j}(\hat {\bf r})
 *  \f]
 *  Radial part of local orbital is defined as a linear combination of radial functions (minimum two radial functions 
 *  are required) such that local orbital vanishes at the sphere boundary:
 *  \f[
 *      \phi_{\ell}^{\zeta, \alpha}(r) = \sum_{p}\gamma_{p}^{\zeta,\alpha} u_{\ell \nu_p}^{\alpha}(r)  
 *  \f]
 *  
 *  Arbitrary number of local orbitals can be introduced for each angular quantum number (this is highlighted by
 *  the index \f$ \zeta \f$).
 *
 *  Radial functions are m-th order (with zero-order being a function itself) energy derivatives of the radial 
 *  Schrödinger equation:
 *  \f[
 *      u_{\ell \nu}^{\alpha}(r) = \frac{\partial^{m_{\nu}}}{\partial^{m_{\nu}}E}u_{\ell}^{\alpha}(r,E)\Big|_{E=E_{\nu}}
 *  \f]
 */

/** \page data_dist K-point data distribution
 *
 *  \section data_dist1 "Panel" and "full" data storage
 *
 *  We have to deal with big arrays (matching coefficients, eigen vectors, wave functions, etc.) which may not fit
 *  into the memory of a single node. For some operations we need a "panel" distribution of data, where each 
 *  MPI rank gets a local panel of block-cyclic distributed matrix. This way of storing data is necessary for the
 *  distributed PBLAS and ScaLAPACK operations.  
 *
 */


#endif // __K_POINT_H__

