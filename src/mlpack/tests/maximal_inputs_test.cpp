/**
 * @file MaximalInputs.cpp
 * @author Ngap Wei Tham
 *
 * Test the MaximalInputs and ColumnsToBlocks functions.
 */
#include <mlpack/core.hpp>
#include <mlpack/core/math/columns_to_blocks.hpp>
#include <mlpack/methods/sparse_autoencoder/maximal_inputs.hpp>

#include <boost/test/unit_test.hpp>
#include "old_boost_test_definitions.hpp"

using namespace mlpack;

arma::mat CreateMaximalInput()
{
  arma::mat w1(2, 4);
  w1<<0<<1<<2<<3<<arma::endr
    <<4<<5<<6<<7;

  arma::mat input(5,5);
  input.submat(0, 0, 1, 3) = w1;

  arma::mat maximalInputs;
  mlpack::nn::MaximalInputs(input, maximalInputs);

  return maximalInputs;
}

void TestResults(arma::mat const &actualResult, arma::mat const &expectResult)
{
  BOOST_REQUIRE(expectResult.n_rows == actualResult.n_rows &&
                expectResult.n_cols == actualResult.n_cols);

  for(size_t i = 0; i != expectResult.n_elem; ++i)
  {
    BOOST_REQUIRE_CLOSE(expectResult[i], actualResult[i], 1e-2);
  }
}

BOOST_AUTO_TEST_SUITE(MaximalInputsTest);

BOOST_AUTO_TEST_CASE(ColumnToBlocksEvaluate)
{   
  arma::mat output;  
  mlpack::math::ColumnsToBlocks ctb(1,2);
  ctb.Transform(CreateMaximalInput(), output);

  arma::mat matlabResults;
  matlabResults<<-1<<-1<<-1<<-1<<-1<<-1<<-1<<arma::endr
               <<-1<<-1<<-0.42857<<-1<<0.14286<<0.71429<<-1<<arma::endr
               <<-1<<-0.71429<<-0.14286<<-1.00000<<0.42857<<1<<-1<<arma::endr
               <<-1<<-1<<-1<<-1<<-1<<-1<<-1;

  TestResults(output, matlabResults);
}

BOOST_AUTO_TEST_CASE(ColumnToBlocksChangeBlockSize)
{
  arma::mat output;
  mlpack::math::ColumnsToBlocks ctb(1,2);
  ctb.BlockWidth(4);
  ctb.BlockHeight(1);
  ctb.BufValue(-3);
  ctb.Transform(CreateMaximalInput(), output);

  arma::mat matlabResults;
  matlabResults<<-3<<-3<<-3<<-3<<-3
               <<-3<<-3<<-3<<-3<<-3<<-3<<arma::endr
               <<-3<<-1<<-0.71429<<-0.42857<<-0.14286
               <<-3<<0.14286<<0.42857<<0.71429<<1<<-3<<arma::endr
               <<-3<<-3<<-3<<-3<<-3<<-3<<-3<<-3<<-3<<-3<<-3<<arma::endr;

  TestResults(output, matlabResults);
}

BOOST_AUTO_TEST_SUITE_END();
