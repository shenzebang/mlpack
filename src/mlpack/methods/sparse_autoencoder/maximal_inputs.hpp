#ifndef __MLPACK_METHODS_NN_MAXIMAL_INPUTS_HPP
#define __MLPACK_METHODS_NN_MAXIMAL_INPUTS_HPP

#include <mlpack/core.hpp>

namespace mlpack {
namespace nn {

/**
 * Maximize the hidden units of the parameters, details are located at
 * http://deeplearning.stanford.edu/wiki/index.php/Visualizing_a_Trained_Autoencoder.
 * This function is based on the implementation(display_network.m) from the exercise of UFLDL.
 * http://deeplearning.stanford.edu/wiki/index.php/Exercise:Sparse_Autoencoder
 * @param params The parameters want to maximize
 * @param output Parameters after maximize
 * @pre 1 : The layout of the parameters should be same as following
 * //          vSize   1
 * //       |        |  |
 * //  hSize|   w1   |b1|
 * //       |________|__|
 * //       |        |  |
 * //  hSize|   w2'  |  |
 * //       |________|__|
 * //      1|   b2'  |  |
 *
 * 2 : Square root of vSize must be interger and bigger than zero
 * @code
 * arma::mat data; // Data matrix.
 * const size_t vSize = 64; // Size of visible layer, depends on the data.
 * const size_t hSize = 25; // Size of hidden layer, depends on requirements.
 *
 *
 * const size_t numBasis = 5; // Parameter required for L-BFGS algorithm.
 * const size_t numIterations = 100; // Maximum number of iterations.
 *
 * // Use an instantiated optimizer for the training.
 * SparseAutoencoderFunction saf(data, vSize, hSize);
 * L_BFGS<SparseAutoencoderFunction> optimizer(saf, numBasis, numIterations);
 * SparseAutoencoder<L_BFGS> encoder2(optimizer);
 *
 * arma::mat maximalInput; //store the features learned by sparse autoencoder
 * mlpack::nn::MaximalInputs(encoder2.Parameters(), maximalInput);
 *
 * arma::mat outputs;
 * const bool scale = true;
 *
 * ColumnsToBlocks ctb(5,5);
 * arma::mat output;
 * ctb.Transform(maximalInput, output);
 * //you can save the output as a pgm, this may help you visualize the training results
 * output.save(fileName, arma::pgm_binary);
 *
 * @endcode
 */
void MaximalInputs(const arma::mat &parameters, arma::mat &output);

/**
 * Normalize the cols with its maximum value if it is not zero
 * @param input the input data want to normalize
 * @param output inputs after normalize
 */
void NormalizeColByMax(const arma::mat &input,
                       arma::mat &output);

} // namespace nn
} // namespace mlpack

#endif
