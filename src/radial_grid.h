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

/** \file radial_grid.h
 *
 *  \brief Contains declaraion and partial implementation of sirius::Radial_grid class.
 */

#ifndef __RADIAL_GRID_H__
#define __RADIAL_GRID_H__

namespace sirius {

/// Types of radial grid.
enum radial_grid_t
{
    linear_grid = 0,

    exponential_grid = 1,

    pow2_grid = 2,

    pow3_grid = 3,

    scaled_pow_grid = 4,

    lin_exp_grid = 5
};

/// Base class for radial grids.
template <typename T>
class Radial_grid
{
  protected:
    /// Radial grid points.
    mdarray<T, 1> x_;

    /// Inverse values of radial grid points.
    mdarray<T, 1> x_inv_;
    
    /// Radial grid points difference.
    /** \f$ dx_{i} = x_{i+1} - x_{i} \f$ */
    mdarray<T, 1> dx_;

    /// Name of the grid type.
    std::string name_;

    void init(int num_points__)
    {
        /* set x^{-1} */
        for (int i = 0; i < num_points__; i++) {
            x_inv_(i) = (x_(i) == 0) ? 0 : 1.0 / x_(i);
        }
        
        /* set dx */
        for (int i = 0; i < num_points__ - 1; i++) {
            dx_(i) = x_(i + 1) - x_(i);
        }
    }

    Radial_grid(Radial_grid<T> const& src__) = delete;

    Radial_grid& operator=(Radial_grid<T> const& src__) = delete;

  public:

    Radial_grid()
    {
    }

    Radial_grid(int num_points__)
    {
        x_     = mdarray<T, 1>(num_points__);
        x_inv_ = mdarray<T, 1>(num_points__);
        dx_    = mdarray<T, 1>(num_points__ - 1);
    }

    Radial_grid(Radial_grid<T>&& src__)
    {
        x_     = std::move(src__.x_);
        dx_    = std::move(src__.dx_);
        x_inv_ = std::move(src__.x_inv_);
        name_  = src__.name_;
    }

    Radial_grid<T>& operator=(Radial_grid<T>&& src__)
    {
        if (this != &src__) {
            x_     = std::move(src__.x_);
            dx_    = std::move(src__.dx_);
            x_inv_ = std::move(src__.x_inv_);
            name_  = src__.name_;
        }
        return *this;
    }

    int index_of(T x__) const
    {
        if (x__ < first() || x__ > last()) {
            return -1;
        }
        int i0 = 0;
        int i1 = num_points() - 1;

        while (i1 - i0 > 1) {
            int i = (i1 + i0) >> 1;
            if (x__ >= x_[i0] && x__ < x_[i]) {
                i1 = i;
            } else {
                i0 = i;
            }
        }
        return i0;
    }

    /// Number of grid points.
    inline int num_points() const
    {
        return static_cast<int>(x_.size());
    }

    /// First point of the grid.
    inline T first() const
    {
        return x_(0);
    }

    /// Last point of the grid.
    inline T last() const
    {
        return x_(num_points() - 1);
    }

    /// Return \f$ x_{i} \f$.
    inline T operator[](const int i) const
    {
        assert(i < (int)x_.size());
        return x_(i);
    }
    
    /// Return \f$ dx_{i} \f$.
    inline T dx(const int i) const
    {
        assert(i < (int)dx_.size());
        return dx_(i);
    }
    
    /// Return \f$ x_{i}^{-1} \f$.
    inline T x_inv(const int i) const
    {
        assert(i < (int)x_inv_.size());
        return x_inv_(i);
    }

    inline std::string const& name() const
    {
        return name_;
    }

    void copy_to_device()
    {
        x_.allocate(memory_t::device);
        x_.template copy<memory_t::host, memory_t::device>();
        dx_.allocate(memory_t::device);
        dx_.template copy<memory_t::host, memory_t::device>();
    }

    mdarray<T, 1> const& x() const
    {
        return x_;
    }

    mdarray<T, 1> const& dx() const
    {
        return dx_;
    }

    Radial_grid<T> segment(int num_points__) const
    {
        assert(num_points__ >= 0 && num_points__ <= (int)x_.size());
        Radial_grid<T> r;
        r.name_  = name_ + " (segment)";
        r.x_     = mdarray<T, 1>(num_points__);
        r.dx_    = mdarray<T, 1>(num_points__ - 1);
        r.x_inv_ = mdarray<T, 1>(num_points__);

        std::memcpy(&r.x_(0), &x_(0), num_points__ * sizeof(T));
        std::memcpy(&r.dx_(0), &dx_(0), (num_points__ - 1) * sizeof(T));
        std::memcpy(&r.x_inv_(0), &x_inv_(0), num_points__ * sizeof(T));

        return std::move(r);
    }

    //uint64_t hash() const
    //{
    //    uint64_t h = Utils::hash(&x_(0), x_.size() * sizeof(double));
    //    h += Utils::hash(&dx_(0), dx_.size() * sizeof(double), h);
    //    h += Utils::hash(&x_inv_(0), x_inv_.size() * sizeof(double), h);
    //    return h;
    //}
};

template<typename T>
class Radial_grid_pow: public Radial_grid<T>
{
  public:
    Radial_grid_pow(int num_points__, T rmin__, T rmax__, double p__)
        : Radial_grid<T>(num_points__)
    {
        for (int i = 0; i < num_points__; i++) {
            T t = static_cast<T>(i) / (num_points__ - 1);
            this->x_[i] = rmin__ + (rmax__ - rmin__) * std::pow(t, p__);
        }
        this->init(num_points__);
        std::stringstream s;
        s << p__;
        this->name_ = "power" + s.str();
    }
};

template<typename T>
class Radial_grid_lin: public Radial_grid_pow<T>
{
  public:
    Radial_grid_lin(int num_points__, T rmin__, T rmax__)
        : Radial_grid_pow<T>(num_points__, rmin__, rmax__, 1.0)
    {
        this->name_ = "linear";
    }
};

template<typename T>
class Radial_grid_exp: public Radial_grid<T>
{
  public:
    Radial_grid_exp(int num_points__, T rmin__, T rmax__, double p__ = 1.0)
        : Radial_grid<T>(num_points__)
    {
        /* x_i = x_min * (x_max/x_min) ^ (i / (N - 1)) */
        for (int i = 0; i < num_points__; i++) {
            T t = static_cast<T>(i) / (num_points__ - 1);
            this->x_[i] = rmin__ * std::pow(rmax__ / rmin__, std::pow(t, p__));
        }
        this->init(num_points__);
        this->name_ = "exponential";
    }
};

template<typename T>
class Radial_grid_lin_exp: public Radial_grid<T>
{
  public:
    Radial_grid_lin_exp(int num_points__, T rmin__, T rmax__)
        : Radial_grid<T>(num_points__)
    {
        /* x_i = x_min + (x_max - x_min) * A(t)
         * A(0) = 0; A(1) = 1
         * A(t) ~ b * t + Exp[t^a] - 1 */
        T alpha{6.0};
        T beta = 1e-6 * num_points__ / (rmax__ - rmin__);
        T A = 1.0 / (std::exp(static_cast<T>(1)) + beta - static_cast<T>(1));
        for (int i = 0; i < num_points__; i++) {
            T t = static_cast<T>(i) / (num_points__ - 1);
            this->x_[i] = rmin__ + (rmax__ - rmin__) * (beta * t + std::exp(std::pow(t, alpha)) - static_cast<T>(1)) * A;
        }
        this->init(num_points__);
        this->name_ = "linear_exponential";
    }
};

/// External radial grid provided as a list of points.
template<typename T>
class Radial_grid_ext: public Radial_grid<T>
{
  public:
    Radial_grid_ext(int num_points__, T const* data__)
        : Radial_grid<T>(num_points__)
    {
        for (int i = 0; i < num_points__; i++) {
            this->x_[i] = data__[i];
        }
        this->init(num_points__);
        this->name_ = "external";
    }
};

template<typename T>
Radial_grid<T> Radial_grid_factory(radial_grid_t grid_type__,
                                   int           num_points__,
                                   T             rmin__,
                                   T             rmax__,
                                   double        p__ = 1.0)
{
    Radial_grid<T> rgrid;

    switch (grid_type__) {
        case linear_grid: {
            rgrid = Radial_grid_lin<T>(num_points__, rmin__, rmax__);
            break;
        }
        case exponential_grid: {
            rgrid = Radial_grid_exp<T>(num_points__, rmin__, rmax__, p__);
            break;
        }
        case pow2_grid: {
            rgrid = Radial_grid_pow<T>(num_points__, rmin__, rmax__, 2.0);
            break;
        }
        case pow3_grid: {
            rgrid = Radial_grid_pow<T>(num_points__, rmin__, rmax__, 3.0);
            break;
        }
        case lin_exp_grid: {
            rgrid = Radial_grid_lin_exp<T>(num_points__, rmin__, rmax__);
            break;
        }
        default: {
            TERMINATE("wrong radial grid type");
        }
    }
    return std::move(rgrid);
};

}

#endif // __RADIAL_GRID_H__
