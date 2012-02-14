/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "sqp_internal.hpp"
#include "casadi/stl_vector_tools.hpp"
#include "casadi/matrix/sparsity_tools.hpp"
#include "casadi/matrix/matrix_tools.hpp"
#include "casadi/fx/sx_function.hpp"
#include "casadi/sx/sx_tools.hpp"
#include "casadi/casadi_calculus.hpp"
/*#include "interfaces/qpoases/qpoases_solver.hpp"*/
#include <ctime>
#include <iomanip>

using namespace std;
namespace CasADi{

SQPInternal::SQPInternal(const FX& F, const FX& G, const FX& H, const FX& J) : NLPSolverInternal(F,G,H,J){
  casadi_warning("The SQP method is under development");
  addOption("qp_solver",         OT_QPSOLVER,   GenericType(), "The QP solver to be used by the SQP method");
  addOption("qp_solver_options", OT_DICTIONARY, GenericType(), "Options to be passed to the QP solver");
  addOption("maxiter",           OT_INTEGER,    100,           "Maximum number of SQP iterations");
  addOption("maxiter_ls",        OT_INTEGER,    100,           "Maximum number of linesearch iterations");
  addOption("toldx",             OT_REAL   ,    1e-12,         "Stopping criterion for the stepsize");
  addOption("tolgl",             OT_REAL   ,    1e-12,         "Stopping criterion for the Lagrangian gradient");
  addOption("sigma",             OT_REAL   ,    1.0,           "Linesearch parameter");
  addOption("rho",               OT_REAL   ,    0.5,           "Linesearch parameter");
  addOption("mu_safety",         OT_REAL   ,    1.1,           "Safety factor for linesearch mu");
  addOption("eta",               OT_REAL   ,    0.0001,        "Linesearch parameter: See Nocedal 3.4");
  addOption("tau",               OT_REAL   ,    0.2,           "Linesearch parameter");
  addOption("hessian_approximation", OT_STRING, "BFGS",        "BFGS|exact");
  
  // Monitors
  addOption("monitor",      OT_STRINGVECTOR, GenericType(),  "", "eval_f|eval_g|eval_jac_g|eval_grad_f|eval_h|qp", true);
}


SQPInternal::~SQPInternal(){
}

void SQPInternal::init(){
  // Call the init method of the base class
  NLPSolverInternal::init();
    
  // Read options
  maxiter_ = getOption("maxiter");
  maxiter_ls_ = getOption("maxiter_ls");
  toldx_ = getOption("toldx");
  tolgl_ = getOption("tolgl");
  sigma_ = getOption("sigma");
  rho_ = getOption("rho");
  mu_safety_ = getOption("mu_safety");
  eta_ = getOption("eta");
  tau_ = getOption("tau");
  
  // QP solver
  int n = input(NLP_X_INIT).size();
  
  // Allocate a QP solver
  CRSSparsity H_sparsity = sp_dense(n,n);
  CRSSparsity A_sparsity = J_.isNull() ? CRSSparsity(0,n,false) : J_.output().sparsity();

  QPSolverCreator qp_solver_creator = getOption("qp_solver");
  qp_solver_ = qp_solver_creator(H_sparsity,A_sparsity);
  
  // Set options if provided
  if(hasSetOption("qp_solver_options")){
    Dictionary qp_solver_options = getOption("qp_solver_options");
    qp_solver_.setOption(qp_solver_options);
  }

  qp_solver_.init();
  
#if 0  
  // Assume SXFunction for now
  SXFunction F = shared_cast<SXFunction>(F_);
  SXFunction G = shared_cast<SXFunction>(G_);
  
  // Get the expressions
  SXMatrix x = F.inputSX();
  SXMatrix f = F.outputSX();
  SXMatrix g = G.outputSX();
#endif
  
/*  cout << "x = " << x << endl;
  cout << "f = " << f << endl;
  cout << "g = " << g << endl;*/
  
  // Derivatives of lifted variables
  // SXMatrix zdot = ssym("zdot",z.size());
    
  // Lagrange multipliers
  //SXMatrix mux = ssym("mux",x.size1());
/*  SXMatrix mug = ssym("mug",g.size1());

  // Gradient of the Lagrangian
  SXMatrix lgrad = gradient(f - inner_prod(mug,g),x);

  // Combined Lagrangian gradient and constraint function
  enum ZInput{Z_X,Z_MUG,Z_NUM_IN};
  enum ZOutput{Z_L,Z_MUG,Z_NUM_IN};
  Z = SXFunction([u,d,mux,mug],[z,f1,f2])*/
  
  
/*  vector<SXMatrix> z_in(2);
  z_in[0] = x;
  z_in[1] = mug;
  vector<SXMatrix> z_out(3);
  z_in[0] = lgrad;
  z_in[1] = g;
  SXFunction z(z_in,z_out);
  Z.init()

  # Matrix A and B in lifted Newton
  A = Z.jac(0,0)
  B1 = Z.jac(0,1)
  B2 = Z.jac(0,2)

  
  SXFunction z(z_in,lgrad);
  lfcn.init();*/
  
  if (getOption("hessian_approximation")=="exact" && H_.isNull()) {
    casadi_error("SQPInternal::evaluate: you set option 'hessian_approximation' to 'exact', but no hessian was supplied. Suggest using 'generate_hessian' option.");
  }
  
}

void SQPInternal::evaluate(int nfdir, int nadir){
  casadi_assert(nfdir==0 && nadir==0);
  
  checkInitialBounds();
    
  // Set the static parameter
  if (parametric_) {
    if (!F_.isNull()) F_.setInput(input(NLP_P),F_.getNumInputs()-1);
    if (!G_.isNull()) G_.setInput(input(NLP_P),G_.getNumInputs()-1);
    if (!H_.isNull()) H_.setInput(input(NLP_P),H_.getNumInputs()-1);
    if (!J_.isNull()) J_.setInput(input(NLP_P),J_.getNumInputs()-1);
  }
  
  // Initial guess
  DMatrix x = input(NLP_X_INIT);

  // Current cost;
  double fk;
  
  // current 'mu' in the T1 merit function
  double merit_mu = 0;  

  // Get dimensions
  int m = G_.isNull() ? 0 : G_.output().size(); // Number of equality constraints
  int n = x.size();  // Number of variables

  // Initial guess for the lagrange multipliers
  DMatrix lambda_k(m,1,0);
  DMatrix lambda_x_k(n,1,0);

  // Initial guess for the Hessian
  DMatrix Bk = DMatrix::eye(n);
  makeDense(Bk);
  
  if (getOption("hessian_approximation")=="exact") {
    int n_hess_in = H_.getNumInputs() - (parametric_ ? 1 : 0);
    H_.setInput(x);
    if(n_hess_in>1){
      H_.setInput(lambda_k, n_hess_in==4? 2 : 1);
      H_.setInput(1, n_hess_in==4? 3 : 2);
    }
    H_.evaluate();
    DMatrix Bk = H_.output();
    
  }

  if (monitored("eval_h")) {
    cout << "(pre) B = " << endl;
    Bk.printSparse();
  }
    
  // No bounds on the control
  double inf = numeric_limits<double>::infinity();
  qp_solver_.input(QP_LBX).setAll(-inf);
  qp_solver_.input(QP_UBX).setAll( inf);

  // Header
  cout << " iter     objective    nls           dx         gradL      eq viol" << endl;
  int k = 0;

  while(true){
    DMatrix gk;
    DMatrix Jgk = DMatrix::zeros(0,n);
    
    if (!G_.isNull()) {
      // Evaluate the constraint function
      G_.setInput(x);
      G_.evaluate();
      gk = G_.output();
      
      if (monitored("eval_g")) {
        cout << "(main loop) x = " << G_.input().data() << endl;
        cout << "(main loop) G = " << endl;
        G_.output().printSparse();
      }
      
      // Evaluate the Jacobian
      J_.setInput(x);
      J_.evaluate();
      Jgk = J_.output();

      if (monitored("eval_jac_g")) {
        cout << "(main loop) x = " << J_.input().data() << endl;
        cout << "(main loop) J = " << endl;
        J_.output().printSparse();
      }
    }
    
    // Evaluate the gradient of the objective function
    F_.setInput(x);
    F_.setAdjSeed(1.0);
    F_.evaluate(0,1);
    fk = F_.output().at(0);
    DMatrix gfk = F_.adjSens();
    
    
    if (monitored("eval_f")) {
      cout << "(main loop) x = " << F_.input().data() << endl;
      cout << "(main loop) F = " << endl;
      F_.output().printSparse();
    }
    
    if (monitored("eval_grad_f")) {
      cout << "(main loop) x = " << F_.input().data() << endl;
      cout << "(main loop) gradF = " << endl;
      gfk.printSparse();
    }
    
    // Pass data to QP solver
    qp_solver_.setInput(Bk,QP_H);
    qp_solver_.setInput(gfk,QP_G);
    qp_solver_.setInput(Jgk,QP_A);
      
    if (!G_.isNull()) {
      qp_solver_.setInput(-gk+input(NLP_LBG),QP_LBA);
      qp_solver_.setInput(-gk+input(NLP_UBG),QP_UBA);
    }

    qp_solver_.setInput(-x+input(NLP_LBX),QP_LBX);
    qp_solver_.setInput(-x+input(NLP_UBX),QP_UBX);
    
    if (monitored("qp")) {
      cout << "(main loop) QP_H = " << endl;
      qp_solver_.input(QP_H).printDense();
      cout << "(main loop) QP_A = " << endl;
      qp_solver_.input(QP_A).printDense();
      cout << "(main loop) QP_G = " << endl;
      qp_solver_.input(QP_G).printDense();
      cout << "(main loop) QP_LBA = " << endl;
      qp_solver_.input(QP_LBA).printDense();
      cout << "(main loop) QP_UBA = " << endl;
      qp_solver_.input(QP_UBA).printDense();
      cout << "(main loop) QP_LBX = " << endl;
      qp_solver_.input(QP_LBX).printDense();
      cout << "(main loop) QP_UBX = " << endl;
      qp_solver_.input(QP_UBX).printDense();
    }

    // Solve the QP subproblem
    qp_solver_.evaluate();

    // Get the optimal solution
    DMatrix p = qp_solver_.output(QP_PRIMAL);
    
    // Get the dual solution for the inequalities
    DMatrix lambda_hat = qp_solver_.output(QP_DUAL_A);
    
    // Get the dual solution for the bounds
    DMatrix lambda_x_hat = qp_solver_.output(QP_DUAL_X);
    
    // Get the gradient of the Lagrangian
    DMatrix gradL = F_.adjSens() - (G_.isNull() ? 0 : mul(trans(Jgk),lambda_hat)) - lambda_x_hat;
    
    // Pass adjoint seeds to g
    //gfcn.setAdjSeed(lambda_hat);
    //gfcn.evaluate(0,1);

    // Do a line search along p
    double mu = merit_mu;
    
    // 1-norm of the feasability violations
    double feasviol = G_.isNull() ? 0 : sumRows(fabs(gk)).at(0);

    // Use a quadratic model of T1 to get a lower bound on mu (eq. 18.36 in Nocedal)
    double mu_lb = ((inner_prod(gfk,p) + sigma_/2.0*mul(trans(p),mul(Bk,p)))/(1.-rho_)/feasviol).at(0);

    // Increase mu if it is below the lower bound
    if(mu < mu_lb){
      mu = mu_lb*mu_safety_;
    }

    // Calculate T1 at x (18.27 in Nocedal)
    double T1 = fk + mu*feasviol;

    // Calculate the directional derivative of T1 at x (cf. 18.29 in Nocedal)
    double DT1 = (inner_prod(gfk,p) - (G_.isNull() ? 0 : mu*sumRows(fabs(gk)))).at(0);
    
    int lsiter = 0;
    double alpha = 1;
    while(true){
      // Evaluate prospective x
      DMatrix x_new = x+alpha*p;
      F_.setInput(x_new);
      F_.evaluate();
      DMatrix fk_new = DMatrix(F_.output());

      if (monitored("eval_f")) {
        cout << "(armillo loop) x = " << F_.input().data() << endl;
        cout << "(armillo loop) F = " << endl;
        F_.output().printSparse();
      }
    
      DMatrix feasviol_new;
        
      if (!G_.isNull()) {
        // Evaluate gk, hk and get 1-norm of the feasability violations
        G_.setInput(x_new);
        G_.evaluate();
        DMatrix gk_new = G_.output();
        feasviol_new = sumRows(fabs(gk_new));

        if (monitored("eval_g")) {
          cout << "(armillo loop) x = " << G_.input().data() << endl;
          cout << "(armillo loop) G = " << endl;
          G_.output().printSparse();
        }
      }
    
      // New T1 function
      DMatrix T1_new = fk_new + ( G_.isNull() ? 0 : mu*feasviol_new );

      // Check Armijo condition, SQP version (18.28 in Nocedal)
      if(T1_new.at(0) <= (T1 + eta_*alpha*DT1)){
        break;
      }

      // Backtrace
      alpha = alpha*tau_;
      
      // Go to next iteration
      lsiter = lsiter+1;
      if(lsiter >= maxiter_ls_){
        throw CasadiException("linesearch failed!");
      }
    }

    // Step size
    double tk = alpha;

    // Calculate the new step
    DMatrix dx = p*tk;
    x = x + dx;
    lambda_k = tk*lambda_hat + (1-tk)*lambda_k;
    lambda_x_k = tk*lambda_x_hat + (1-tk)*lambda_x_k;
    k = k+1;

    // Gather and print iteration information
    double normdx = norm_2(dx).at(0); // step size
    double normgradL = norm_2(gradL).at(0); // size of the Lagrangian gradient
    double eq_viol = G_.isNull() ? 0 : sumRows(fabs(gk)).at(0); // constraint violation
    string ineq_viol = "nan"; // sumRows(max(0,-hk)); % inequality constraint violation

    if (!callback_.isNull()) {
      callback_.input(NLP_X_OPT).set(x);
      callback_.input(NLP_COST).set(fk);
      callback_->stats_["iter"] = k;
      callback_->stats_["lsiter"] = lsiter;
      callback_->stats_["normdx"] = normdx;
      callback_->stats_["normgradL"] = normgradL;
      callback_->stats_["eq_viol"] = eq_viol;
      callback_.evaluate();
      if (callback_.output(0).at(0)) {
        cout << "Stop requested by user." << endl;
        break;
      }
    }    
    
    cout << setw(5) << k << setw(15) << fk << setw(5) << lsiter << setw(15) << normdx << setw(15) << normgradL << setw(15) << eq_viol << endl;

    // Check convergence on dx
    if(normdx < toldx_){
      cout << "Convergence (small dx)" << endl;
      break;
    } else if(normgradL < tolgl_){
      cout << "Convergence (small gradL)" << endl;
      break;
    }
      
    if (!G_.isNull()) {
      // Evaluate the constraint function
      G_.setInput(x);
      G_.evaluate();
      gk = G_.output();

      if (monitored("eval_g")) {
        cout << "(main loop-post) x = " << G_.input().data() << endl;
        cout << "(main loop-post) G = " << endl;
        G_.output().printSparse();
      }
      
      // Evaluate the Jacobian
      J_.setInput(x);
      J_.evaluate();
      Jgk = J_.output();

      if (monitored("eval_jac_g")) {
        cout << "(main loop-post) x = " << J_.input().data() << endl;
        cout << "(main loop-post) J = " << endl;
        J_.output().printSparse();
      }
    }
    
    // Evaluate the gradient of the objective function
    F_.setInput(x);
    F_.setAdjSeed(1.0);
    F_.evaluate(0,1);
    fk = DMatrix(F_.output()).at(0);
    gfk = F_.adjSens();

    if (monitored("eval_f")) {
      cout << "(main loop-post) x = " << F_.input().data() << endl;
      cout << "(main loop-post) F = " << endl;
      F_.output().printSparse();
    }
    
    if (monitored("eval_grad_f")) {
      cout << "(main loop-post) x = " << F_.input().data() << endl;
      cout << "(main loop-post) gradF = " << endl;
      gfk.printSparse();
    }
    
    // Check if maximum number of iterations reached
    if(k >= maxiter_){
      cout << "Maximum number of SQP iterations reached!" << endl;
      break;
    }
    
    if (getOption("hessian_approximation")=="exact") {
      int n_hess_in = H_.getNumInputs() - (parametric_ ? 1 : 0);
      H_.setInput(x);
      if(n_hess_in>1){
        H_.setInput(lambda_k, n_hess_in==4? 2 : 1);
        H_.setInput(1, n_hess_in==4? 3 : 2);
      }
      H_.evaluate();
      Bk = H_.output();
    }

    if (getOption("hessian_approximation")=="BFGS") {
      // Complete the damped BFGS update (Procedure 18.2 in Nocedal)
      DMatrix gradL_new = gfk - ( G_.isNull() ? 0 : mul(trans(Jgk),lambda_k) ) - lambda_x_k;
      DMatrix yk = gradL_new - gradL;
      DMatrix Bdx = mul(Bk,dx);
      DMatrix dxBdx = mul(trans(dx),Bdx);
      DMatrix ydx = inner_prod(dx,yk);
      DMatrix thetak;
      if(ydx.at(0) >= 0.2*dxBdx.at(0)){
        thetak = 1.;
      } else {
        thetak = 1 - 0.8*dxBdx/(dxBdx - ydx);
      }
      DMatrix rk = thetak*yk + (1-thetak)*Bdx; // rk replaces yk to assure Bk pos.def.
      
      Bk = Bk - outer_prod(Bdx,Bdx)/dxBdx + outer_prod(rk,rk)/ inner_prod(rk,dx);
    }
    
    if (monitored("eval_h")) {
      cout << "(main loop-post) B = " << endl;
      Bk.printSparse();
    }
    
  }
  cout << "SQP algorithm terminated after " << (k-1) << " iterations" << endl;
  
  output(NLP_COST).set(fk);
  output(NLP_X_OPT).set(x);
}

} // namespace CasADi
