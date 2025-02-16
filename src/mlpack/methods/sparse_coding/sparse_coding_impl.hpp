/**
 * @file sparse_coding_impl.hpp
 * @author Nishant Mehta
 *
 * Implementation of Sparse Coding with Dictionary Learning using l1 (LASSO) or
 * l1+l2 (Elastic Net) regularization.
 */
#ifndef __MLPACK_METHODS_SPARSE_CODING_SPARSE_CODING_IMPL_HPP
#define __MLPACK_METHODS_SPARSE_CODING_SPARSE_CODING_IMPL_HPP

// In case it hasn't already been included.
#include "sparse_coding.hpp"

namespace mlpack {
namespace sparse_coding {

template<typename DictionaryInitializer>
SparseCoding<DictionaryInitializer>::SparseCoding(const arma::mat& data,
                                                  const size_t atoms,
                                                  const double lambda1,
                                                  const double lambda2) :
    atoms(atoms),
    data(data),
    codes(atoms, data.n_cols),
    lambda1(lambda1),
    lambda2(lambda2)
{
  // Initialize the dictionary.
  DictionaryInitializer::Initialize(data, atoms, dictionary);
}

template<typename DictionaryInitializer>
void SparseCoding<DictionaryInitializer>::Encode(const size_t maxIterations,
                                                 const double objTolerance,
                                                 const double newtonTolerance)
{
  Timer::Start("sparse_coding");

  double lastObjVal = DBL_MAX;

  // Take the initial coding step, which has to happen before entering the main
  // optimization loop.
  Log::Info << "Initial Coding Step." << std::endl;

  OptimizeCode();
  arma::uvec adjacencies = find(codes);

  Log::Info << "  Sparsity level: " << 100.0 * ((double) (adjacencies.n_elem))
      / ((double) (atoms * data.n_cols)) << "%." << std::endl;
  Log::Info << "  Objective value: " << Objective() << "." << std::endl;

  for (size_t t = 1; t != maxIterations; ++t)
  {
    // Print current iteration, and maximum number of iterations (if it isn't
    // 0).
    Log::Info << "Iteration " << t;
    if (maxIterations != 0)
      Log::Info << " of " << maxIterations;
    Log::Info << "." << std::endl;

    // First step: optimize the dictionary.
    Log::Info << "Performing dictionary step... " << std::endl;
    OptimizeDictionary(adjacencies, newtonTolerance);
    Log::Info << "  Objective value: " << Objective() << "." << std::endl;

    // Second step: perform the coding.
    Log::Info << "Performing coding step..." << std::endl;
    OptimizeCode();
    // Get the indices of all the nonzero elements in the codes.
    adjacencies = find(codes);
    Log::Info << "  Sparsity level: " << 100.0 * ((double) (adjacencies.n_elem))
        / ((double) (atoms * data.n_cols)) << "%." << std::endl;

    // Find the new objective value and improvement so we can check for
    // convergence.
    double curObjVal = Objective();
    double improvement = lastObjVal - curObjVal;
    Log::Info << "  Objective value: " << curObjVal << " (improvement "
        << std::scientific << improvement << ")." << std::endl;

    // Have we converged?
    if (improvement < objTolerance)
    {
      Log::Info << "Converged within tolerance " << objTolerance << ".\n";
      break;
    }

    lastObjVal = curObjVal;
  }

  Timer::Stop("sparse_coding");
}

template<typename DictionaryInitializer>
void SparseCoding<DictionaryInitializer>::OptimizeCode()
{
  // When using the Cholesky version of LARS, this is correct even if
  // lambda2 > 0.
  arma::mat matGram = trans(dictionary) * dictionary;

  for (size_t i = 0; i < data.n_cols; ++i)
  {
    // Report progress.
    if ((i % 100) == 0)
      Log::Debug << "Optimization at point " << i << "." << std::endl;

    bool useCholesky = true;
    regression::LARS lars(useCholesky, matGram, lambda1, lambda2);

    // Create an alias of the code (using the same memory), and then LARS will
    // place the result directly into that; then we will not need to have an
    // extra copy.
    arma::vec code = codes.unsafe_col(i);
    lars.Regress(dictionary, data.unsafe_col(i), code, false);
  }
}

// Dictionary step for optimization.
template<typename DictionaryInitializer>
double SparseCoding<DictionaryInitializer>::OptimizeDictionary(
    const arma::uvec& adjacencies,
    const double newtonTolerance,
    const size_t maxIterations)
{
  // Count the number of atomic neighbors for each point x^i.
  arma::uvec neighborCounts = arma::zeros<arma::uvec>(data.n_cols, 1);

  if (adjacencies.n_elem > 0)
  {
    // This gets the column index.  Intentional integer division.
    size_t curPointInd = (size_t) (adjacencies(0) / atoms);

    size_t nextColIndex = (curPointInd + 1) * atoms;
    for (size_t l = 1; l < adjacencies.n_elem; ++l)
    {
      // If l no longer refers to an element in this column, advance the column
      // number accordingly.
      if (adjacencies(l) >= nextColIndex)
      {
        curPointInd = (size_t) (adjacencies(l) / atoms);
        nextColIndex = (curPointInd + 1) * atoms;
      }

      ++neighborCounts(curPointInd);
    }
  }

  // Handle the case of inactive atoms (atoms not used in the given coding).
  std::vector<size_t> inactiveAtoms;

  for (size_t j = 0; j < atoms; ++j)
  {
    if (arma::accu(codes.row(j) != 0) == 0)
      inactiveAtoms.push_back(j);
  }

  const size_t nInactiveAtoms = inactiveAtoms.size();
  const size_t nActiveAtoms = atoms - nInactiveAtoms;

  // Efficient construction of Z restricted to active atoms.
  arma::mat matActiveZ;
  if (nInactiveAtoms > 0)
  {
    math::RemoveRows(codes, inactiveAtoms, matActiveZ);
  }

  if (nInactiveAtoms > 0)
  {
    Log::Warn << "There are " << nInactiveAtoms
        << " inactive atoms. They will be re-initialized randomly.\n";
  }

  Log::Debug << "Solving Dual via Newton's Method.\n";

  // Solve using Newton's method in the dual - note that the final dot
  // multiplication with inv(A) seems to be unavoidable. Although more
  // expensive, the code written this way (we use solve()) should be more
  // numerically stable than just using inv(A) for everything.
  arma::vec dualVars = arma::zeros<arma::vec>(nActiveAtoms);

  //vec dualVars = 1e-14 * ones<vec>(nActiveAtoms);

  // Method used by feature sign code - fails miserably here.  Perhaps the
  // MATLAB optimizer fmincon does something clever?
  //vec dualVars = 10.0 * randu(nActiveAtoms, 1);

  //vec dualVars = diagvec(solve(dictionary, data * trans(codes))
  //    - codes * trans(codes));
  //for (size_t i = 0; i < dualVars.n_elem; i++)
  //  if (dualVars(i) < 0)
  //    dualVars(i) = 0;

  bool converged = false;

  // If we have any inactive atoms, we must construct these differently.
  arma::mat codesXT;
  arma::mat codesZT;

  if (inactiveAtoms.empty())
  {
    codesXT = codes * trans(data);
    codesZT = codes * trans(codes);
  }
  else
  {
    codesXT = matActiveZ * trans(data);
    codesZT = matActiveZ * trans(matActiveZ);
  }

  double normGradient = 0;
  double improvement = 0;
  for (size_t t = 1; (t != maxIterations) && !converged; ++t)
  {
    arma::mat A = codesZT + diagmat(dualVars);

    arma::mat matAInvZXT = solve(A, codesXT);

    arma::vec gradient = -arma::sum(arma::square(matAInvZXT), 1);
    gradient += 1;

    arma::mat hessian = -(-2 * (matAInvZXT * trans(matAInvZXT)) % inv(A));

    arma::vec searchDirection = -solve(hessian, gradient);
    //printf("%e\n", norm(searchDirection, 2));

    // Armijo line search.
    const double c = 1e-4;
    double alpha = 1.0;
    const double rho = 0.9;
    double sufficientDecrease = c * dot(gradient, searchDirection);

    // A maxIterations parameter for the Armijo line search may be a good idea,
    // but it doesn't seem to be causing any problems for now.
    while (true)
    {
      // Calculate objective.
      double sumDualVars = arma::sum(dualVars);
      double fOld = -(-trace(trans(codesXT) * matAInvZXT) - sumDualVars);
      double fNew = -(-trace(trans(codesXT) * solve(codesZT +
          diagmat(dualVars + alpha * searchDirection), codesXT)) -
          (sumDualVars + alpha * arma::sum(searchDirection)));

      if (fNew <= fOld + alpha * sufficientDecrease)
      {
        searchDirection = alpha * searchDirection;
        improvement = fOld - fNew;
        break;
      }

      alpha *= rho;
    }

    // Take step and print useful information.
    dualVars += searchDirection;
    normGradient = arma::norm(gradient, 2);
    Log::Debug << "Newton Method iteration " << t << ":" << std::endl;
    Log::Debug << "  Gradient norm: " << std::scientific << normGradient
        << "." << std::endl;
    Log::Debug << "  Improvement: " << std::scientific << improvement << ".\n";

    if (normGradient < newtonTolerance)
      converged = true;
  }

  if (inactiveAtoms.empty())
  {
    // Directly update dictionary.
    dictionary = trans(solve(codesZT + diagmat(dualVars), codesXT));
  }
  else
  {
    arma::mat activeDictionary = trans(solve(codesZT +
        diagmat(dualVars), codesXT));

    // Update all atoms.
    size_t currentInactiveIndex = 0;
    for (size_t i = 0; i < atoms; ++i)
    {
      if (inactiveAtoms[currentInactiveIndex] == i)
      {
        // This atom is inactive.  Reinitialize it randomly.
        dictionary.col(i) = (data.col(math::RandInt(data.n_cols)) +
                             data.col(math::RandInt(data.n_cols)) +
                             data.col(math::RandInt(data.n_cols)));

        dictionary.col(i) /= arma::norm(dictionary.col(i), 2);

        // Increment inactive index counter.
        ++currentInactiveIndex;
      }
      else
      {
        // Update estimate.
        dictionary.col(i) = activeDictionary.col(i - currentInactiveIndex);
      }
    }
  }

  return normGradient;
}

// Project each atom of the dictionary back into the unit ball (if necessary).
template<typename DictionaryInitializer>
void SparseCoding<DictionaryInitializer>::ProjectDictionary()
{
  for (size_t j = 0; j < atoms; j++)
  {
    double atomNorm = arma::norm(dictionary.col(j), 2);
    if (atomNorm > 1)
    {
      Log::Info << "Norm of atom " << j << " exceeds 1 (" << std::scientific
          << atomNorm << ").  Shrinking...\n";
      dictionary.col(j) /= atomNorm;
    }
  }
}

// Compute the objective function.
template<typename DictionaryInitializer>
double SparseCoding<DictionaryInitializer>::Objective() const
{
  double l11NormZ = arma::sum(arma::sum(arma::abs(codes)));
  double froNormResidual = arma::norm(data - (dictionary * codes), "fro");

  if (lambda2 > 0)
  {
    double froNormZ = arma::norm(codes, "fro");
    return 0.5 * (std::pow(froNormResidual, 2.0) + (lambda2 *
        std::pow(froNormZ, 2.0))) + (lambda1 * l11NormZ);
  }
  else // It can be simpler.
  {
    return 0.5 * std::pow(froNormResidual, 2.0) + lambda1 * l11NormZ;
  }
}

template<typename DictionaryInitializer>
std::string SparseCoding<DictionaryInitializer>::ToString() const
{
  std::ostringstream convert;
  convert << "Sparse Coding  [" << this << "]" << std::endl;
  convert << "  Data: " << data.n_rows << "x" ;
  convert <<  data.n_cols << std::endl;
  convert << "  Atoms: " << atoms << std::endl;
  convert << "  Lambda 1: " << lambda1 << std::endl;
  convert << "  Lambda 2: " << lambda2 << std::endl;
  return convert.str();
}

} // namespace sparse_coding
} // namespace mlpack

#endif
