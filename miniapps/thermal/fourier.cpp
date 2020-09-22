// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.
//
//            -----------------------------------------------------
//                     Fourier Miniapp:  Thermal Diffusion
//            -----------------------------------------------------
//
// This miniapp solves a time dependent heat equation.
//

#include "fourier_solver.hpp"
#include <cassert>
#include <memory>
#include <iostream>
#include <fstream>

using namespace std;
using namespace mfem;
using namespace mfem::common;
using namespace mfem::thermal;

void display_banner(ostream & os);

static int prob_ = 1;
static int gamma_ = 10;
static double alpha_ = NAN;
static double chi_max_ratio_ = 1.0;
static double chi_min_ratio_ = 1.0;

double QFunc(const Vector &x, double t)
{
   switch (prob_)
   {
      case 1:
      {
         return 2.0 * M_PI * M_PI * sin(M_PI * x[0]) * sin(M_PI * x[1]);
      }
      case 2:
      case 4:
      {
         double a = 0.4;
         double b = 0.8;

         double r = pow(x[0] / a, 2) + pow(x[1] / b, 2);
         double e = exp(-0.25 * t * M_PI * M_PI / (a * b) );

         if ( r == 0.0 )
            return 0.25 * M_PI * M_PI *
                   ( (1.0 - e) * ( pow(a, -2) + pow(b, -2) ) + e / (a * b));

         return ( M_PI / r ) *
                ( 0.25 * M_PI * pow(a * b, -4) *
                  ( pow(b * b * x[0],2) + pow(a * a * x[1], 2) +
                    (a - b) * (b * pow(b * x[0], 2) - a * pow(a*x[1],2)) * e) *
                  cos(0.5 * M_PI * sqrt(r)) +
                  0.5 * pow(a * b, -2) * (x * x) * (1.0 - e) *
                  sin(0.5 * M_PI * sqrt(r)) / sqrt(r)
                );

      }
      case 3:
      {
         double cx = cos(M_PI * (x[0]-0.5));
         double cy = cos(M_PI * (x[1]-0.5));
         double c2x = cos(2.0 * M_PI * (x[0]-0.5));
         double s2x = sin(2.0 * M_PI * (x[0]-0.5));
         double c2y = cos(2.0 * M_PI * (x[1]-0.5));
         double s2y = sin(2.0 * M_PI * (x[1]-0.5));
         double c2a = cos(2.0 * alpha_);
         double s2a = sin(2.0 * alpha_);
         double ccg = 0.5 * M_PI * M_PI * gamma_ * pow(cx * cy, gamma_ - 2);
         double perp = 1.0 * gamma_ * (c2x * c2y - 1.0) + c2x + c2y + 2.0;
         double para = 0.5 * (gamma_ * (c2x * c2y - s2a * s2x * s2y - 1.0) +
                              (gamma_ - 1.0) * c2a * (c2x - c2y) +
                              c2x + c2y + 2.0);
         return ccg * (1.0 * perp + (chi_max_ratio_ - 1.0) * para);
      }
   default:
     return 0.0;
   }
}

//static double chi_ratio_ = 1.0;

double TFunc(const Vector &x, double t)
{
   switch (prob_)
   {
      case 1:
      {
         double e = exp(-2.0 * M_PI * M_PI * t);
         return sin(M_PI * x[0]) * sin(M_PI * x[1]) * (1.0 - e);
      }
      case 2:
      {
         double a = 0.4;
         double b = 0.8;

         double r = pow(x[0] / a, 2) + pow(x[1] / b, 2);
         double e = exp(-0.25 * t * M_PI * M_PI / (a * b) );

         return cos(0.5 * M_PI * sqrt(r)) * (1.0 - e);
      }
      case 3:
         return pow(sin(M_PI * x[0]) * sin(M_PI * x[1]), gamma_);
      case 4:
      {
         double a = 0.4;
         double b = 0.8;

         double r = pow(x[0] / a, 2) + pow(x[1] / b, 2);
         double rs = pow(x[0] - 0.5 * a, 2) + pow(x[1] - 0.5 * b, 2);
         return cos(0.5 * M_PI * sqrt(r)) + 0.5 * exp(-400.0 * rs);
      }
   default:
     return 0.0;
   }
}

void dTFunc(const Vector &x, double t, Vector &dT)
{
   dT.SetSize(x.Size());
   dT = 0.0;

   switch (prob_)
   {
      case 1:
      {
         double e = exp(-2.0 * M_PI * M_PI * t);
         dT[0] = M_PI * cos(M_PI * x[0]) * sin(M_PI * x[1]);
         dT[1] = M_PI * sin(M_PI * x[0]) * cos(M_PI * x[1]);
         dT *= (1.0 - e);
      }
      break;
      case 2:
      {
         double a = 0.4;
         double b = 0.8;

         double r = pow(x[0] / a, 2) + pow(x[1] / b, 2);
         double r_2 = sqrt(r);
         double sr = sin(0.5 * M_PI * r_2);
         double e = exp(-0.25 * t * M_PI * M_PI / (a * b) );

         dT[0] = -0.5 * M_PI * x[0] * sr / ( a * a * r_2 );
         dT[1] = -0.5 * M_PI * x[1] * sr / ( b * b * r_2 );
         dT *= (1.0 - e);
      }
      break;
      case 3:
      {
         double cx = cos(M_PI * x[0]);
         double cy = cos(M_PI * x[1]);
         double sx = sin(M_PI * x[0]);
         double sy = sin(M_PI * x[1]);
         // T = pow(sin(M_PI * x[0]) * sin(M_PI * x[1]), gamma_);
         dT[0] = cx * sy;
         dT[1] = sx * cy;
         dT *= M_PI * gamma_ * pow(sx * sy, gamma_ - 1);
      }
      break;
      case 4:
      {
         double a = 0.4;
         double b = 0.8;

         double r = pow(x[0] / a, 2) + pow(x[1] / b, 2);
         double rs = pow(x[0] - 0.5 * a, 2) + pow(x[1] - 0.5 * b, 2);
         double ers = exp(-400.0 * rs);

         double r_2 = sqrt(r);
         double sr = sin(0.5 * M_PI * r_2);

         // T = cos(0.5 * M_PI * sqrt(r)) + 0.5 * exp(-400.0 * rs);

         dT[0] = -0.5 * M_PI * x[0] * sr / ( a * a * r_2 );
         dT[1] = -0.5 * M_PI * x[1] * sr / ( b * b * r_2 );

         dT[0] -= 400.0 * (x[0] - 0.5 * a) * ers;
         dT[1] -= 400.0 * (x[1] - 0.5 * b) * ers;

      }
      break;
   }
}

void ChiFunc(const Vector &x, DenseMatrix &M)
{
   M.SetSize(2);

   switch (prob_)
   {
      case 1:
      {
         double cx = cos(M_PI * x[0]);
         double cy = cos(M_PI * x[1]);
         double sx = sin(M_PI * x[0]);
         double sy = sin(M_PI * x[1]);

         double den = cx * cx * sy * sy + sx * sx * cy * cy;

         M(0,0) = chi_max_ratio_ * sx * sx * cy * cy + sy * sy * cx * cx;
         M(1,1) = chi_max_ratio_ * sy * sy * cx * cx + sx * sx * cy * cy;

         M(0,1) = (1.0 - chi_max_ratio_) * cx * cy * sx * sy;
         M(1,0) = M(0,1);

         M *= 1.0 / den;
      }
      break;
      case 2:
      case 4:
      {
         double a = 0.4;
         double b = 0.8;

         double den = pow(b * b * x[0], 2) + pow(a * a * x[1], 2);

         M(0,0) = chi_max_ratio_ * pow(a * a * x[1], 2) + pow(b * b * x[0], 2);
         M(1,1) = chi_max_ratio_ * pow(b * b * x[0], 2) + pow(a * a * x[1], 2);

         M(0,1) = (1.0 - chi_max_ratio_) * pow(a * b, 2) * x[0] * x[1];
         M(1,0) = M(0,1);

         M *= 1.0 / den;
      }
      break;
      case 3:
      {
         double ca = cos(alpha_);
         double sa = sin(alpha_);

         M(0,0) = 1.0 + (chi_max_ratio_ - 1.0) * ca * ca;
         M(1,1) = 1.0 + (chi_max_ratio_ - 1.0) * sa * sa;

         M(0,1) = (chi_max_ratio_ - 1.0) * ca * sa;
         M(1,0) = (chi_max_ratio_ - 1.0) * ca * sa;
      }
      break;
   }
}

void bbTFunc(const Vector &x, DenseMatrix &M)
{
   M.SetSize(2);

   switch (prob_)
   {
      case 1:
      {
         double cx = cos(M_PI * x[0]);
         double cy = cos(M_PI * x[1]);
         double sx = sin(M_PI * x[0]);
         double sy = sin(M_PI * x[1]);

         double den = cx * cx * sy * sy + sx * sx * cy * cy;

         M(0,0) = sx * sx * cy * cy;
         M(1,1) = sy * sy * cx * cx;

         M(0,1) =  -1.0 * cx * cy * sx * sy;
         M(1,0) = M(0,1);

         M *= 1.0 / den;
      }
      break;
      case 2:
      case 4:
      {
         double a = 0.4;
         double b = 0.8;

         double den = pow(b * b * x[0], 2) + pow(a * a * x[1], 2);

         M(0,0) = pow(a * a * x[1], 2);
         M(1,1) = pow(b * b * x[0], 2);

         M(0,1) = -1.0 * pow(a * b, 2) * x[0] * x[1];
         M(1,0) = M(0,1);

         M *= 1.0 / den;
      }
      break;
      case 3:
      {
         double ca = cos(alpha_);
         double sa = sin(alpha_);

         M(0,0) = ca * ca;
         M(1,1) = sa * sa;

         M(0,1) = ca * sa;
         M(1,0) = ca * sa;
      }
      break;
   }
}

class ChiGridFuncCoef : public MatrixCoefficient
{
private:
   GridFunction * T_;

public:
   ChiGridFuncCoef(GridFunction & T) : MatrixCoefficient(2), T_(&T) {}

   void Eval(DenseMatrix &K, ElementTransformation &T,
             const IntegrationPoint &ip);
};

void qFunc(const Vector &x, double t, Vector &q)
{
   DenseMatrix Chi(x.Size());
   Vector dT(x.Size());

   dTFunc(x, t, dT);
   ChiFunc(x, Chi);

   Chi.Mult(dT, q);
   q *= -1.0;
}

long int factorial(unsigned int n)
{
   long int fact = 1;
   for (unsigned int i=2; i<=n; i++)
   {
      fact *= i;
   }
   return fact;
}

// Returns the Gamma(n) function for a positive integer n
long int gamma(unsigned int n)
{
   assert(n > 0);
   return factorial(n-1);
}

// Returns Gamma(n+1/2) for a positive integer n
double gamma1_2(unsigned int n)
{
   return sqrt(M_PI) * factorial(2*n) / (pow(4, n) * factorial(n));
}

double TNorm()
{
   switch (prob_)
   {
      case 1:
         return 0.5;
      case 2:
         return (gamma1_2((unsigned int)gamma_) /
                 gamma((unsigned int)gamma_+1)) / sqrt(M_PI);
      default:
         return 0.0;
   }
}

double qPerpNorm()
{
   switch (prob_)
   {
      case 1:
         return M_PI * M_SQRT1_2 * chi_max_ratio_;
      case 3:
         return sqrt(M_PI * gamma_) * M_SQRT1_2 *
                sqrt(gamma1_2((unsigned int)gamma_-1) *
                     gamma1_2((unsigned int)gamma_)) /
                sqrt(gamma((unsigned int)gamma_) * gamma((unsigned int)gamma_+1));
      default:
        return 0.0;
   }
}

double qParaNorm()
{
   switch (prob_)
   {
      case 1:
         return 0.0;
      case 3:
         return chi_max_ratio_ * qPerpNorm();
      default:
        return 0.0;
   }
}

int main(int argc, char *argv[])
{
   // 1. Initialize MPI.
   MPI_Session mpi(argc, argv);
   int myid = mpi.WorldRank();

   // print the cool banner
   if (mpi.Root()) { display_banner(cout); }

   // 2. Parse command-line options.
   int n = -1;
   int order = 1;
   int irOrder = -1;
   int el_type = Element::QUADRILATERAL;
   int ode_solver_type = 1;
   int vis_steps = 1;
   double dt = 0.5;
   double t_final = 5.0;
   double tol = 1e-4;
   const char *basename = "Fourier";
   const char *mesh_file = "";
   bool zero_start = true;
   bool static_cond = false;
   bool gfprint = true;
   bool visit = true;
   bool visualization = true;

   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&n, "-n", "--num-elems-1d",
                  "Number of elements in x and y directions.  "
                  "Total number of elements is n^2.");
   args.AddOption(&prob_, "-p", "--problem",
                  "Specify problem type: 1 - Square, 2 - Ellipse, 3 - van Es.");
   args.AddOption(&order, "-o", "--order",
                  "Finite element order (polynomial degree) or -1 for"
                  " isoparametric space.");
   args.AddOption(&irOrder, "-iro", "--int-rule-order",
                  "Integration Rule Order.");
   args.AddOption(&alpha_, "-alpha", "--constant-angle",
                  "Angle for constant B field (in degrees)");
   args.AddOption(&gamma_, "-gamma", "--exponent",
                  "Exponent used in problem 2");
   args.AddOption(&chi_max_ratio_, "-chi-max", "--chi-max-ratio",
                  "Ratio of chi_max_parallel/chi_perp.");
   args.AddOption(&chi_min_ratio_, "-chi-min", "--chi-min-ratio",
                  "Ratio of chi_min_parallel/chi_perp.");
   args.AddOption(&dt, "-dt", "--time-step",
                  "Time step.");
   args.AddOption(&t_final, "-tf", "--final-time",
                  "Final Time.");
   args.AddOption(&tol, "-tol", "--tolerance",
                  "Tolerance used to determine convergence to steady state.");
   args.AddOption(&el_type, "-e", "--element-type",
                  "Element type: 2-Triangle, 3-Quadrilateral.");
   args.AddOption(&ode_solver_type, "-s", "--ode-solver",
                  "ODE solver: 1 - Backward Euler, 2 - SDIRK2, 3 - SDIRK3\n\t."
                  "\t   22 - Mid-Point, 23 - SDIRK23, 34 - SDIRK34.");
   args.AddOption(&zero_start, "-z", "--zero-start", "-no-z",
                  "--no-zero-start",
                  "Initial guess of zero or exact solution.");
   args.AddOption(&static_cond, "-sc", "--static-condensation", "-no-sc",
                  "--no-static-condensation", "Enable static condensation.");
   args.AddOption(&gfprint, "-print", "--print","-no-print","--no-print",
                  "Print results (grid functions) to disk.");
   args.AddOption(&visit, "-visit", "--visit", "-no-visit", "--no-visit",
                  "Enable or disable VisIt visualization.");
   args.AddOption(&vis_steps, "-vs", "--visualization-steps",
                  "Visualize every n-th timestep.");
   args.AddOption(&basename, "-k", "--outputfilename",
                  "Name of the visit dump files");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.Parse();
   if (!args.Good())
   {
      if (myid == 0)
      {
         args.PrintUsage(cout);
      }
      MPI_Finalize();
      return 1;
   }
   if (myid == 0)
   {
      args.PrintOptions(cout);
   }

   if (irOrder < 0)
   {
      irOrder = std::max(4, 2 * order - 2);
   }

   if (isnan(alpha_))
   {
      alpha_ = 0.0;
   }
   else
   {
      alpha_ *= M_PI / 180.0;
   }

   // 3. Construct a (serial) mesh of the given size on all processors.  We
   //    can handle triangular and quadrilateral surface meshes with the
   //    same code.
   Mesh *mesh = (n > 0) ?
                new Mesh(n, n, (Element::Type)el_type, true) :
                new Mesh(mesh_file, 1, 1);
   int dim = mesh->Dimension();

   // 4. This step is no longer needed

   // 5. Define a parallel mesh by a partitioning of the serial mesh. Once the
   //    parallel mesh is defined, the serial mesh can be deleted.
   ParMesh *pmesh = new ParMesh(MPI_COMM_WORLD, *mesh);
   delete mesh;

   // 7. Determine the list of true (i.e. parallel conforming) essential
   //    boundary dofs. In this example, the boundary conditions are defined
   //    by marking all the boundary attributes from the mesh as essential
   //    (Dirichlet) and converting them to a list of true dofs.
   Array<int> ess_tdof_list;
   Array<int> ess_bdr(0);
   if (pmesh->bdr_attributes.Size())
   {
      ess_bdr.SetSize(pmesh->bdr_attributes.Max());
      ess_bdr = 1;
   }

   // The following is required for mesh refinement
   // mesh->EnsureNCMesh();

   // 6. Define the ODE solver used for time integration. Several implicit
   //    methods are available, including singly diagonal implicit Runge-Kutta
   //    (SDIRK).
   ODESolver *ode_solver;
   switch (ode_solver_type)
   {
      // Implicit L-stable methods
      case 1:  ode_solver = new BackwardEulerSolver; break;
      case 2:  ode_solver = new SDIRK23Solver(2); break;
      case 3:  ode_solver = new SDIRK33Solver; break;
      // Implicit A-stable methods (not L-stable)
      case 22: ode_solver = new ImplicitMidpointSolver; break;
      case 23: ode_solver = new SDIRK23Solver; break;
      case 34: ode_solver = new SDIRK34Solver; break;
      default:
         if (mpi.Root())
         {
            cout << "Unknown ODE solver type: " << ode_solver_type << '\n';
         }
         delete mesh;
         return 3;
   }

   // 12. Define the parallel finite element spaces. We use:
   //
   //     H(curl) for electric field,
   //     H(div) for magnetic flux,
   //     H(div) for thermal flux,
   //     H(grad)/H1 for electrostatic potential,
   //     L2 for temperature

   // L2 contains discontinuous "cell-center" finite elements, type 2 is
   // "positive"
   L2_FECollection L2FEC0(0, dim);
   L2_FECollection L2FEC(order-1, dim);

   // RT contains Raviart-Thomas "face-centered" vector finite elements with
   // continuous normal component.
   RT_FECollection HDivFEC(order-1, dim);

   // ND contains Nedelec "edge-centered" vector finite elements with
   // continuous tangential component.
   ND_FECollection HCurlFEC(order, dim);

   // H1 contains continuous "node-centered" Lagrange finite elements.
   H1_FECollection HGradFEC(order, dim);

   ParFiniteElementSpace   L2FESpace0(pmesh, &L2FEC0);
   ParFiniteElementSpace    L2FESpace(pmesh, &L2FEC);
   ParFiniteElementSpace  HDivFESpace(pmesh, &HDivFEC);
   ParFiniteElementSpace HCurlFESpace(pmesh, &HCurlFEC);
   ParFiniteElementSpace HGradFESpace(pmesh, &HGradFEC);

   // The terminology is TrueVSize is the unique (non-redundant) number of dofs
   // HYPRE_Int glob_size_l2 = L2FESpace.GlobalTrueVSize();
   // HYPRE_Int glob_size_rt = HDivFESpace.GlobalTrueVSize();
   HYPRE_Int glob_size_h1 = HGradFESpace.GlobalTrueVSize();

   if (mpi.Root())
   {
      cout << "Number of Temperature unknowns:       " << glob_size_h1 << endl;
   }

   // int Vsize_l2 = L2FESpace.GetVSize();
   // int Vsize_rt = HDivFESpace.GetVSize();
   // int Vsize_h1 = HGradFESpace.GetVSize();

   // grid functions E, B, T, F, P, and w which is the Joule heating
   ParGridFunction q(&HCurlFESpace);
   ParGridFunction qPara(&HCurlFESpace);
   ParGridFunction qPerp(&HCurlFESpace);
   ParGridFunction Q(&L2FESpace);
   ParGridFunction T1(&HGradFESpace);
   ParGridFunction T0(&HGradFESpace);
   ParGridFunction dT(&HGradFESpace);
   ParGridFunction errorq(&L2FESpace0);
   ParGridFunction errorqPara(&L2FESpace0);
   ParGridFunction errorqPerp(&L2FESpace0);
   ParGridFunction errorT(&L2FESpace0);
   T0 = 0.0;
   T1 = 0.0;
   dT = 1.0;

   // 13. Get the boundary conditions, set up the exact solution grid functions
   //     These VectorCoefficients have an Eval function.  Note that e_exact and
   //     b_exact in this case are exact analytical solutions, taking a 3-vector
   //     point as input and returning a 3-vector field
   FunctionCoefficient TCoef(TFunc);
   VectorFunctionCoefficient qCoef(2, qFunc);

   Vector zeroVec(dim); zeroVec = 0.0;
   ConstantCoefficient zeroCoef(0.0);
   VectorConstantCoefficient zeroVecCoef(zeroVec);

   IdentityMatrixCoefficient ICoef(2);
   MatrixFunctionCoefficient bbTCoef(2, bbTFunc);
   MatrixSumCoefficient ImbbTCoef(bbTCoef, ICoef, -1.0);

   MatVecCoefficient qParaCoef(bbTCoef, qCoef);
   MatVecCoefficient qPerpCoef(ImbbTCoef, qCoef);

   ConstantCoefficient SpecificHeatCoef(1.0);
   MatrixFunctionCoefficient ConductionCoef(2, ChiFunc);
   FunctionCoefficient HeatSourceCoef(QFunc);

   Q.ProjectCoefficient(HeatSourceCoef);

   if (!zero_start)
   {
      T1.ProjectCoefficient(TCoef);
      q.ProjectCoefficient(qCoef);
   }

   T1.GridFunction::ComputeElementL2Errors(TCoef, errorT);
   q.GridFunction::ComputeElementL2Errors(qCoef, errorq);
   qPara.GridFunction::ComputeElementL2Errors(qParaCoef, errorqPara);
   qPerp.GridFunction::ComputeElementL2Errors(qPerpCoef, errorqPerp);

   ParBilinearForm m1(&HCurlFESpace);
   m1.AddDomainIntegrator(new VectorFEMassIntegrator);
   m1.Assemble();

   ParMixedBilinearForm gPara(&HGradFESpace, &HCurlFESpace);
   gPara.AddDomainIntegrator(new MixedVectorGradientIntegrator(bbTCoef));
   gPara.Assemble();

   ParMixedBilinearForm gPerp(&HGradFESpace, &HCurlFESpace);
   gPerp.AddDomainIntegrator(new MixedVectorGradientIntegrator(ImbbTCoef));
   gPerp.Assemble();

   HypreParMatrix M1C;
   Vector RHS1(HCurlFESpace.GetTrueVSize()), X1(HCurlFESpace.GetTrueVSize());

   Array<int> ess_tdof_list_q(0);
   // Array<int> ess_bdr_q;
   // HCurlFESpace.GetEssentialTrueDofs(ess_bdr_q, ess_tdof_list_q);

   m1.FormSystemMatrix(ess_tdof_list_q, M1C);

   HypreDiagScale Precond(M1C);
   HyprePCG M1Inv(M1C);
   M1Inv.SetTol(1e-12);
   M1Inv.SetMaxIter(200);
   M1Inv.SetPrintLevel(0);
   M1Inv.SetPreconditioner(Precond);

   // 14. Initialize the Diffusion operator, the GLVis visualization and print
   //     the initial energies.
   DiffusionTDO oper(HGradFESpace,
		     zeroCoef, ess_bdr,
		     SpecificHeatCoef, false,
		     ConductionCoef, false,
		     HeatSourceCoef, false);

   // This function initializes all the fields to zero or some provided IC
   // oper.Init(F);

   socketstream vis_Q;
   socketstream vis_q, vis_errq;
   socketstream vis_qPara, vis_errqPara;
   socketstream vis_qPerp, vis_errqPerp;
   socketstream vis_T, vis_errT;
   char vishost[] = "localhost";
   int  visport   = 19916;
   if (visualization)
   {
      // Make sure all ranks have sent their 'v' solution before initiating
      // another set of GLVis connections (one from each rank):
      MPI_Barrier(pmesh->GetComm());

      vis_Q.precision(8);
      vis_T.precision(8);
      vis_errT.precision(8);
      vis_q.precision(8);
      vis_errq.precision(8);
      vis_qPara.precision(8);
      vis_errqPara.precision(8);
      vis_qPerp.precision(8);
      vis_errqPerp.precision(8);

      int Wx = 0, Wy = 0; // window position
      int Ww = 280, Wh = 280; // window size
      int offx = Ww+10, offy = Wh+45; // window offsets

      VisualizeField(vis_Q, vishost, visport,
		     Q, "Heat Source", Wx, Wy, Ww, Wh);

      Wy += offy;
      // miniapps::VisualizeField(vis_U, vishost, visport,
      //                       U1, "Energy", Wx, Wy, Ww, Wh);

      Wx += offx;
      Wy -= offy;
      VisualizeField(vis_T, vishost, visport,
		     T1, "Temperature", Wx, Wy, Ww, Wh);

      Wy += offy;
      VisualizeField(vis_errT, vishost, visport,
		     errorT, "Error in T", Wx, Wy, Ww, Wh);

      Wx += offx;
      Wy -= offy;
      VisualizeField(vis_q, vishost, visport,
		     q, "Heat Flux", Wx, Wy, Ww, Wh);

      Wy += offy;
      VisualizeField(vis_errq, vishost, visport,
		     errorq, "Error in q", Wx, Wy, Ww, Wh);

      Wx += offx;
      Wy -= offy;
      VisualizeField(vis_qPara, vishost, visport,
                               qPara, "Parallel Heat Flux", Wx, Wy, Ww, Wh);

      Wy += offy;
      VisualizeField(vis_errqPara, vishost, visport,
                               errorqPara, "Error in q para", Wx, Wy, Ww, Wh);

      Wx += offx;
      Wy -= offy;
      VisualizeField(vis_qPerp, vishost, visport,
                               qPerp, "Perpendicular Heat Flux", Wx, Wy, Ww, Wh);

      Wy += offy;
      VisualizeField(vis_errqPerp, vishost, visport,
                               errorqPerp, "Error in q perp", Wx, Wy, Ww, Wh);
   }
   // VisIt visualization
   VisItDataCollection visit_dc(basename, pmesh);
   if ( visit )
   {
      visit_dc.RegisterField("Q", &Q);
      visit_dc.RegisterField("q", &q);
      visit_dc.RegisterField("qPara", &qPara);
      visit_dc.RegisterField("qPerp", &qPerp);
      visit_dc.RegisterField("T", &T1);

      visit_dc.RegisterField("L2 Error T", &errorT);
      visit_dc.RegisterField("L2 Error q", &errorq);
      visit_dc.RegisterField("L2 Error q para", &errorqPara);
      visit_dc.RegisterField("L2 Error q perp", &errorqPerp);

      visit_dc.SetCycle(0);
      visit_dc.SetTime(0.0);
      visit_dc.Save();
   }

   // 15. Perform time-integration (looping over the time iterations, ti, with a
   //     time-step dt). The object oper is the MagneticDiffusionOperator which
   //     has a Mult() method and an ImplicitSolve() method which are used by
   //     the time integrators.
   ode_solver->Init(oper);
   double t = 0.0;

   bool last_step = false;
   for (int ti = 1; !last_step; ti++)
   {
      if (t + dt >= t_final - dt/2)
      {
         if (myid == 0)
         {
            cout << "Final Time Reached" << endl;
         }
         last_step = true;
      }

      // F is the vector of dofs, t is the current time, and dt is the time step
      // to advance.
      T0 = T1;
      ode_solver->Step(T1, t, dt);

      add(1.0, T1, -1.0, T0, dT);

      double maxT    = T1.ComputeMaxError(zeroCoef);
      double maxDiff = dT.ComputeMaxError(zeroCoef);

      if ( !last_step )
      {
         if ( maxT == 0.0 )
         {
            last_step = (maxDiff < tol) ? true:false;
         }
         else if ( maxDiff/maxT < tol )
         {
            last_step = true;
         }
         if (last_step && myid == 0)
         {
            cout << "Converged to Steady State" << endl;
         }
      }
      /*
      if (debug == 1)
      {
         oper.Debug(basename,t);
      }
      */
      gPara.Mult(T1, qPara);
      gPerp.Mult(T1, qPerp);

      qPara.ParallelAssemble(RHS1);
      X1 = 0.0;
      M1Inv.Mult(RHS1, X1);
      qPara.Distribute(X1);
      qPara *= -chi_max_ratio_;

      qPerp.ParallelAssemble(RHS1);
      X1 = 0.0;
      M1Inv.Mult(RHS1, X1);
      qPerp.Distribute(X1);
      qPerp *= -1.0;

      q = qPara;
      q += qPerp;

      if (gfprint)
      {
         ostringstream q_name, T_name, mesh_name;
         q_name << basename << "_" << setfill('0') << setw(6) << t << "_"
                << "q." << setfill('0') << setw(6) << myid;
         T_name << basename << "_" << setfill('0') << setw(6) << t << "_"
                << "T." << setfill('0') << setw(6) << myid;
         mesh_name << basename << "_" << setfill('0') << setw(6) << t << "_"
                   << "mesh." << setfill('0') << setw(6) << myid;

         ofstream mesh_ofs(mesh_name.str().c_str());
         mesh_ofs.precision(8);
         pmesh->Print(mesh_ofs);
         mesh_ofs.close();

         ofstream q_ofs(q_name.str().c_str());
         q_ofs.precision(8);
         q.Save(q_ofs);
         q_ofs.close();

         ofstream T_ofs(T_name.str().c_str());
         T_ofs.precision(8);
         T1.Save(T_ofs);
         T_ofs.close();
      }

      if (last_step || (ti % vis_steps) == 0)
      {
         // Make sure all ranks have sent their 'v' solution before initiating
         // another set of GLVis connections (one from each rank):
         MPI_Barrier(pmesh->GetComm());

         if (visualization)
         {
            int Wx = 0, Wy = 0; // window position
            int Ww = 350, Wh = 350; // window size
            int offx = Ww+10, offy = Wh+45; // window offsets

            VisualizeField(vis_q, vishost, visport,
                                     q, "Heat Flux", Wx, Wy, Ww, Wh);

            VisualizeField(vis_qPara, vishost, visport,
                                     qPara, "Parallel Heat Flux",
                                     Wx, Wy, Ww, Wh);

            VisualizeField(vis_qPerp, vishost, visport,
                                     qPerp, "Perpendicular Heat Flux",
                                     Wx, Wy, Ww, Wh);

            // Wx += offx;
            // VisualizeField(vis_U, vishost, visport,
            //                       U1, "Energy", Wx, Wy, Ww, Wh);

            // Wx -= offx;
            // Wy += offy;
            VisualizeField(vis_T, vishost, visport,
                                     T1, "Temperature", Wx, Wy, Ww, Wh);

            // Wx += offx;
            VisualizeField(vis_errT, vishost, visport,
                                     errorT, "Error in T", Wx, Wy, Ww, Wh);

            // Wx += offx;
            VisualizeField(vis_errq, vishost, visport,
                                     errorq, "Error in q", Wx, Wy, Ww, Wh);

            VisualizeField(vis_errqPara, vishost, visport,
                                     errorqPara, "Error in q para",
                                     Wx, Wy, Ww, Wh);

            VisualizeField(vis_errqPerp, vishost, visport,
                                     errorqPerp, "Error in q perp",
                                     Wx, Wy, Ww, Wh);
         }

         if (visit)
         {
            visit_dc.SetCycle(ti);
            visit_dc.SetTime(t);
            visit_dc.Save();
         }
      }
   }
   if (visualization)
   {
      vis_Q.close();
      vis_q.close();
      vis_T.close();
      vis_errT.close();
      vis_errq.close();
      vis_errqPara.close();
      vis_errqPerp.close();
   }

   double loc_T_max = T1.Normlinf();
   double T_max = -1.0;
   MPI_Allreduce(&loc_T_max, &T_max, 1, MPI_DOUBLE, MPI_MAX,
                 MPI_COMM_WORLD);
   double err1 = T1.ComputeL2Error(TCoef);
   if (myid == 0)
   {
      cout << "L2 Error of Solution: " << err1 << endl;
      cout << "Maximum Temperature: " << T_max << endl;
      cout << "| chi_eff - 1 | = " << fabs(1.0/T_max - 1) << endl;
   }

   // 16. Free the used memory.
   delete ode_solver;
   delete pmesh;

   return 0;
}

void display_banner(ostream & os)
{
   os << "___________                 .__              " << endl
      << "\\_   _____/___  __ _________|__| ___________ " << endl
      << " |    __)/  _ \\|  |  \\_  __ \\  |/ __ \\_  __ \\" << endl
      << " |    | (  <_> )  |  /|  | \\/  \\  ___/|  | \\/" << endl
      << " \\__  |  \\____/|____/ |__|  |__|\\___  >__|   " << endl
      << "    \\/                              \\/       " << endl
      << flush;
}
