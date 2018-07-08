// VolEsti

// Copyright (c) 2012-2017 Vissarion Fisikopoulos

// VolEsti is free software: you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at
// your option) any later version.
//
// VolEsti is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// See the file COPYING.LESSER for the text of the GNU Lesser General
// Public License.  If you did not receive this file along with HeaDDaCHe,
// see <http://www.gnu.org/licenses/>.

#ifndef VOLUME_H
#define VOLUME_H

#include <iterator>
#include <fstream>
#include <vector>
#include <list>
#include <algorithm>
#include <math.h>
#include "cartesian_geom/cartesian_kernel.h"
#include "random.hpp"
#include "random/uniform_int.hpp"
#include "random/normal_distribution.hpp"
#include "random/uniform_real_distribution.hpp"
//#include "boost/random.hpp"
//#include <boost/random/uniform_int.hpp>
//#include <boost/random/normal_distribution.hpp>
//#include <boost/random/uniform_real_distribution.hpp>


typedef double                      NT;
typedef Cartesian<NT> 	      Kernel; 
typedef Kernel::Point								Point;
typedef boost::mt19937 RNGType; ///< mersenne twister generator


//structs with variables and random generators
struct vars{
public:
    vars( int m,
          int n,
          int walk_steps,
          int n_threads,
          const double err,
          double error,
          const int lw,
          double up,
          const int L,
          RNGType &rng,
          //std::uniform_real_distribution<NT> urdist,
          //std::uniform_real_distribution<NT> urdist1,
          boost::random::uniform_real_distribution<>(urdist),
          boost::random::uniform_real_distribution<> urdist1,
          bool verbose,
          bool rand_only,
          bool round,
          bool NN,
          bool birk,
          bool coordinate
          ) :
        m(m), n(n), walk_steps(walk_steps), n_threads(n_threads), err(err), error(error),
        lw(lw), up(up), L(L), rng(rng),
        urdist(urdist), urdist1(urdist1) , verbose(verbose), rand_only(rand_only), round(round),
        NN(NN),birk(birk),coordinate(coordinate){};

    int m;
    int n;
    int walk_steps;
    int n_threads;
    const double err;
    double error;
    const int lw;
    double up;
    const int L;
    RNGType &rng;
    //std::uniform_real_distribution<NT> urdist;
    //std::uniform_real_distribution<NT> urdist1;
    boost::random::uniform_real_distribution<>(urdist);
    boost::random::uniform_real_distribution<> urdist1;
    bool verbose;
    bool rand_only;
    bool round;
    bool NN;
    bool birk;
    bool coordinate;
};


#include "run_headers/solve_lp.h"
#include "khach2.h"
#include "convex_bodies/ellipsoids.h"
#include "convex_bodies/polytopes.h"
#include "convex_bodies/ballintersectconvex.h"
#include "samplers.h"
#include "rounding.h"
#include "gaussian_samplers.h"
#include "annealing/gaussian_annealing.h"
#include "misc.h"
#include "linear_extensions.h"


template <class T>
NT volume(T &P,
                  vars &var,  // constans for volume
                  vars &var2, // constants for optimization in case of MinkSums
                  std::pair<Point,double> CheBall)  //Chebychev ball
{
    typedef BallIntersectPolytope<T>        BallPoly;



    bool round = var.round;
    bool print = var.verbose;
    bool rand_only = var.rand_only;
    int n = var.n;
    int rnum = var.m;
    int walk_len = var.walk_steps;
    int n_threads = var.n_threads;
    const double err = var.err;
    RNGType &rng = var.rng;
    // Rotation: only for test with skinny polytopes and rounding
    //std::cout<<"Rotate="<<rotate(P)<<std::endl;
    //rotate(P);

    //0. Rounding of the polytope if round=true
    Point c=CheBall.first;
    NT radius=CheBall.second;
    double round_value=1;
    if(round){
        if(print) std::cout<<"\nRounding.."<<std::endl;
        double tstart1 = (double)clock()/(double)CLOCKS_PER_SEC;
        round_value = rounding_min_ellipsoid(P,c,radius,var);
        double tstop1 = (double)clock()/(double)CLOCKS_PER_SEC;
        if(print) std::cout << "Rounding time = " << tstop1 - tstart1 << std::endl;
        std::pair<Point,NT> res=solveLP(P.get_matrix(), P.dimension());
        c=res.first; radius=res.second;
    }

    //1. Get the Chebychev ball (largest inscribed ball) with center and radius

    rnum=rnum/n_threads;
    NT vol=0;

    // Perform the procedure for a number of threads and then take the average
    //#pragma omp for ordered schedule(dynamic)
    for(int t=0; t<n_threads; t++){
        // 2. Generate the first random point in P
        // Perform random walk on random point in the Chebychev ball
        if(print) std::cout<<"\nGenerate the first random point in P"<<std::endl;
        Random_points_on_sphere_d<Point> gen (n, radius);
        Point p = gen.sample_point(rng);
        p=p+c;
        std::list<Point> randPoints; //ds for storing rand points
        //use a large walk length e.g. 1000
        rand_point_generator(P, p, 1, 50*n, randPoints, var);
        double tstart2 = (double)clock()/(double)CLOCKS_PER_SEC;
        // 3. Sample "rnum" points from P
        if(print) std::cout<<"\nCompute "<<rnum<<" random points in P"<<std::endl;
        rand_point_generator(P, p, rnum-1, walk_len, randPoints, var);
        double tstop2 = (double)clock()/(double)CLOCKS_PER_SEC;
        if(print) std::cout << "First random points construction time = " << tstop2 - tstart2 << std::endl;

        // 4.  Construct the sequence of balls
        // 4a. compute the radius of the largest ball
        double current_dist, max_dist=NT(0);
        for(std::list<Point>::iterator pit=randPoints.begin(); pit!=randPoints.end(); ++pit){
            current_dist=(*pit-c).squared_length();
            if(current_dist>max_dist){
                max_dist=current_dist;
            }
        }
        max_dist=std::sqrt(max_dist);
        if(print) std::cout<<"\nFurthest distance from Chebychev point= "<<max_dist<<std::endl;

        //
        // 4b. Number of balls
        int nb1 = n * (std::log(radius)/std::log(2.0));
        //std::cout<<"nb1 = "<< n * (std::log(radius)/std::log(2.0))<<" nb1INT= "<<nb1<<" nb1stdfloor = "<<std::floor(n * (std::log(radius)/std::log(2.0)))<<std::endl;
        int nb2 = std::ceil(n * (std::log(max_dist)/std::log(2.0)));
        
        if(print) std::cout<<"\nConstructing the sequence of balls"<<std::endl;

        std::vector<Ball> balls;
        
        for(int i=nb1; i<=nb2; ++i){

            if(i==nb1){
                balls.push_back(Ball(c,radius*radius));
            }else{
                balls.push_back(Ball(c,std::pow(std::pow(2.0,NT(i)/NT(n)),2)));
            }

        }
        assert(!balls.empty());
        if (print) std::cout<<"---------"<<std::endl;

        // 5. Estimate Vol(P)

        NT telescopic_prod=NT(1);

        std::vector<Ball>::iterator bit2=balls.end();
        bit2--;

        while(bit2!=balls.begin()){

            //each step starts with some random points in PBLarge stored in list "randPoints"
            //these points have been generated in a previous step

            BallPoly PBLarge(P,*bit2);
            --bit2;
            BallPoly PBSmall(P,*bit2);

            if(print)
                std::cout<<"("<<balls.end()-bit2<<"/"<<balls.end()-balls.begin()<<") Ball ratio radius="
                        <<PBLarge.second().radius()<<","<<PBSmall.second().radius()<<std::endl;

            // choose a point in PBLarge to be used to generate more rand points
            Point p_gen = *randPoints.begin();

            // num of points in PBSmall and PBLarge
            int nump_PBSmall = 0;
            int nump_PBLarge = randPoints.size();

            if(print) std::cout<<"Points in PBLarge="<<randPoints.size()
                              <<std::endl;

            //keep the points in randPoints that fall in PBSmall
            std::list<Point>::iterator rpit=randPoints.begin();
            while(rpit!=randPoints.end()){
                if (PBSmall.second().is_in(*rpit) == 0){//not in
                    rpit=randPoints.erase(rpit);
                } else {
                    ++nump_PBSmall;
                    ++rpit;
                }
            }

            if(print) std::cout<<"Points in PBSmall="<<randPoints.size()
                              <<"\nRatio= "<<NT(nump_PBLarge)/NT(nump_PBSmall)
                             <<std::endl;

            if(print) std::cout<<"Generate "<<rnum-nump_PBLarge<<  " more "
                              <<std::endl;

            //generate more random points in PBLarge to have "rnum" in total
            rand_point_generator(PBLarge,p_gen,rnum-nump_PBLarge,walk_len,randPoints,PBSmall,nump_PBSmall,var);

            telescopic_prod *= NT(rnum)/NT(nump_PBSmall);
            if(print) std::cout<<nump_PBSmall<<"/"<<rnum<<" = "<<NT(rnum)/nump_PBSmall
                              <<"\ncurrent_vol="<<telescopic_prod
                             <<"\n="<<telescopic_prod
                            <<"\n--------------------------"<<std::endl;

            //don't continue in pairs of balls that are almost inside P, i.e. ratio ~= 2
        }
        
        if(print) std::cout<<"rand points = "<<rnum<<std::endl;
        if(print) std::cout<<"walk len = "<<walk_len<<std::endl;
        vol = (std::pow(M_PI,n/2.0)*(std::pow(balls[0].radius(), n) ) ) / (tgamma(n/2.0+1));
        vol=vol*telescopic_prod;
        if(print) std::cout<<"round_value: "<<round_value<<std::endl;
        vol=round_value*vol;
        if(print) std::cout<<"volume computed: "<<vol<<std::endl;

    }

    return vol;
}


template <class T>
NT volume_gaussian_annealing(T &P,
                             vars &var,  // constans for volume
                             std::pair<Point,double> CheBall) {
    NT vol;
    bool round = var.round, converged, done;
    bool print = var.verbose;
    bool rand_only = var.rand_only;
    int n = var.n, steps;
    int rnum = var.m;
    int walk_len = var.walk_steps;
    int n_threads = var.n_threads, min_index, max_index, index, min_steps;
    const double err = var.err;
    NT error = var.error, curr_eps, min_val, max_val, val;
    RNGType &rng = var.rng;
    // Rotation: only for test with skinny polytopes and rounding
    //std::cout<<"Rotate="<<rotate(P)<<std::endl;
    //rotate(P);

    //0. Rounding of the polytope if round=true
    Point c=CheBall.first;
    NT radius=CheBall.second;
    NT round_value=1;
    if(round){
        if(print) std::cout<<"\nRounding.."<<std::endl;
        double tstart1 = (double)clock()/(double)CLOCKS_PER_SEC;
        round_value = rounding_min_ellipsoid(P,c,radius,var);
        double tstop1 = (double)clock()/(double)CLOCKS_PER_SEC;
        if(print) std::cout << "Rounding time = " << tstop1 - tstart1 << std::endl;
        std::pair<Point,NT> res=solveLP(P.get_matrix(), P.dimension());
        c=res.first; radius=res.second;
    }

    //1. Move chebychev center to origin and apply the same shifting to the polytope
    int m=P.num_of_hyperplanes();
    Eigen::MatrixXd A(m,n);
    Eigen::VectorXd b(m);
    Eigen::VectorXd c_e(n);
    for(int i=0; i<n; i++){
        c_e(i)=c[i];
    }
    for(int i=0; i<m; ++i){
        b(i) = P.get_coeff(i,0);
        for(int j=1; j<n+1; ++j){
            A(i,j-1) = P.get_coeff(i,j);
        }
    }
    //Shift polytope
    b = b - A*c_e;
    // Write changesto the polytope!
    for(int i=0; i<m; ++i){
        P.put_coeff(i,0,b(i));
        for(int j=1; j<n+1; ++j){
            P.put_coeff(i,j,A(i,j-1));
        }
    }

    std::vector<NT> a_vals;
    NT ratio = 1.0-1.0/(NT(n));
    NT C=2.0;
    get_annealing_schedule(P, a_vals, error, radius, ratio, C, var);
    int mm = a_vals.size();
    std::vector<NT> fn(m,0), its(mm,0);
    int W = 4*n*n+500;
    std::vector<NT> last_W(W,0);
    vol=std::pow((M_PI/a_vals[0]), NT(n/2.0))*std::abs(round_value);
    vars var2=var;
    var2.coordinate=false;
    std::list<Point> randPoints;

    for(int i=0; i<mm-1; i++){
        //initialize convergence test
        curr_eps = error/std::sqrt(mm);
        converged=false, done=false;
        min_val=-std::pow(10,100);
        max_val=-min_val;
        min_index=W;
        max_index=W;
        index = 0;
        min_steps=0;
        Point p;
        randPoints.clear();

        while(!done || its[i]<min_steps){
            for(int j=0; j<n_threads; j++){
                rand_gaussian_point_generator(P, p, 1, 1, randPoints, a_vals[i], var2);
                its[i] += 1.0;
                fn[i] += eval_exp(p,a_vals[i+1]) / eval_exp(p,a_vals[i]);
                val = fn[i]/its[i];

                last_W[index] = val;
                if(val<=min_val){
                    min_val = val;
                    min_index = index;
                }else if(min_index==index){
                    min_val = *(std::min_element(last_W.begin() , last_W.end() ));
                    min_index = std::distance(last_W.begin(),std::min_element(last_W.begin() , last_W.end() ));
                }

                if(val>=max_val){
                    max_val = val;
                    max_index = index;
                }else if(max_index==index){
                    max_val = *(std::max_element(last_W.begin() , last_W.end() ));
                    max_index = std::distance(last_W.begin(),std::max_element(last_W.begin() , last_W.end() ));
                }

                if( (max_val-min_val)/max_val<=curr_eps/2.0 ){
                    done=true;
                }

                index = index%W+1;
            }
        }
        vol = vol*(fn[i]/its[i]);
    }
    NT sum_of_steps;
    for(std::vector<NT>::iterator it = its.begin(); it != its.end(); ++it) {
        sum_of_steps += *it;
    }
    steps= int(sum_of_steps);

    return vol;
}

#endif