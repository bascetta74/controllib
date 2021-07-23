#include "MatlabDataArray.hpp"
#include "MatlabEngine.hpp"
#include <iostream>
#include <functional>
#include <algorithm>
#include <Eigen/Dense>

#include "discrete_ss.h"

#define NUM_TEST        1000
#define STATE_ERR_THD   1
#define OUTPUT_ERR_THD  1


template <typename UnaryFunctionT /*= std::less<double>*/ >
int count_all(const std::vector<double>& vec, UnaryFunctionT func )
{
    return std::count_if( vec.begin(), vec.end(), func );
}
bool is_nan (double d) { return std::isnan(d); }
bool is_inf (double d) { return std::isinf(d); }


int main() {
    using namespace matlab::engine;

    // Initialize tests
    std::vector<double> test_state_error(NUM_TEST, 0.0);
    std::vector<double> test_output_error(NUM_TEST, 0.0);

    // Start MATLAB engine synchronously
    std::vector<String> optionVec;
    optionVec.push_back(u"-nodesktop -nosplash");
    std::unique_ptr<MATLABEngine> matlabPtr = startMATLAB(optionVec);

    // Execute tests
    std::cout << "Executing " << NUM_TEST << " tests on discrete_ss_ti class" << std::endl;
    for (auto k=0; k<NUM_TEST; k++) {
        // Simulate system in Matlab
        matlabPtr->eval(u"test_discrete_ss_ti;");

        // Get system data from Matlab
        matlab::data::TypedArray<double> n = matlabPtr->getVariable(u"n");
        matlab::data::TypedArray<double> m = matlabPtr->getVariable(u"m");
        matlab::data::TypedArray<double> p = matlabPtr->getVariable(u"p");

        Eigen::MatrixXd A = Eigen::MatrixXd::Zero((int)n[0],(int)n[0]);
        matlab::data::TypedArray<double> m_A = matlabPtr->getVariable(u"A");
        for (auto i=0; i<(int)n[0]; i++) {
            for (auto j=0; j<(int)n[0]; j++) {
                A(i,j) = m_A[i][j];
            }
        }

        Eigen::MatrixXd B = Eigen::MatrixXd::Zero((int)n[0],(int)m[0]);
        matlab::data::TypedArray<double> m_B = matlabPtr->getVariable(u"B");
        for (auto i=0; i<(int)n[0]; i++) {
            for (auto j=0; j<(int)m[0]; j++) {
                B(i,j) = m_B[i][j];
            }
        }

        Eigen::MatrixXd C = Eigen::MatrixXd::Zero((int)p[0],(int)n[0]);
        matlab::data::TypedArray<double> m_C = matlabPtr->getVariable(u"C");
        for (auto i=0; i<(int)p[0]; i++) {
            for (auto j=0; j<(int)n[0]; j++) {
                C(i,j) = m_C[i][j];
            }
        }

        Eigen::MatrixXd D = Eigen::MatrixXd::Zero((int)p[0],(int)m[0]);
        matlab::data::TypedArray<double> m_D = matlabPtr->getVariable(u"D");
        for (auto i=0; i<(int)p[0]; i++) {
            for (auto j=0; j<(int)m[0]; j++) {
                D(i,j) = m_D[i][j];
            }
        }

        Eigen::VectorXd initial_state = Eigen::VectorXd::Zero((int)n[0]);
        matlab::data::TypedArray<double> m_initial_state = matlabPtr->getVariable(u"initial_state");
        for (auto i=0; i<(int)n[0]; i++) {
            initial_state(i) = m_initial_state[i];
        }

        matlab::data::TypedArray<double> m_in = matlabPtr->getVariable(u"in");
        matlab::data::ArrayDimensions m_in_size = m_in.getDimensions();
        Eigen::MatrixXd input = Eigen::MatrixXd::Zero((int)m[0],m_in_size.at(0));
        for (auto i=0; i<m_in_size.at(0); i++) {
            for (auto j=0; j<(int)m[0]; j++) {
                input(j,i) = m_in[i][j];
            }
        }

        matlab::data::TypedArray<double> m_output = matlabPtr->getVariable(u"output");
        matlab::data::TypedArray<double> m_state  = matlabPtr->getVariable(u"state");

        // Simulate system in C++ and compare
        discrete_ss ssd(A, B, C, D, initial_state);

        Eigen::VectorXd output, state;
        std::vector<double> state_error, output_error;

        bool nan_flag = false;
        for (auto i=0; i<input.cols(); i++) {
            ssd.evaluate(input.col(i));
            ssd.get_state(state);
            ssd.get_output(output);

            double state_error_norm = 0.0;
            for (auto ss=0; ss<(int)n[0]; ss++) {
                if (std::isnan(m_state[i][ss])) {
                    nan_flag = true;
                    std::cout << "Test " << k << ": at time " << i << " Matlab state is NaN!" << std::endl;
                }
                if (std::isnan(state(ss))) {
                    nan_flag = true;
                    std::cout << "Test " << k << ": at time " << i << " C++ state is NaN!" << std::endl;
                }

                if (nan_flag) {
                    break;
                }
                else {
                    state_error_norm += std::sqrt(std::pow((state(ss)-m_state[i][ss])/m_state[i][ss],2))*(100.0/input.cols());
                }
            }

            double output_error_norm = 0.0;
            for (auto out=0; out<(int)p[0]; out++) {
                if (std::isnan(m_output[i][out])) {
                    nan_flag = true;
                    std::cout << "Test " << k << ": at time " << i << " Matlab output is NaN!" << std::endl;
                }
                if (std::isnan(output(out))) {
                    nan_flag = true;
                    std::cout << "Test " << k << ": at time " << i << " C++ output is NaN!" << std::endl;
                }

                if (nan_flag) {
                    break;
                }
                else {
                    output_error_norm += std::sqrt(std::pow((output(out)-m_output[i][out])/m_output[i][out],2))*(100.0/input.cols());
                }
            }

            if (nan_flag) {
                break;
            }
            else {
                state_error.push_back(state_error_norm);
                output_error.push_back(output_error_norm);
            }
        }

        // Skip this test in case of NaN or record the state and output errors
        if (nan_flag) {
            std::cout << "Test " << k << " aborted, due to NaN values!" << std::endl << std::endl;

            test_state_error.at(k)  = 0.0;
            test_output_error.at(k) = 0.0;
        }
        else {
            test_state_error.at(k)  = *std::max_element(state_error.begin(), state_error.end());
            test_output_error.at(k) = *std::max_element(output_error.begin(), output_error.end());

            // In case the error exceeds the threshold, save in a mat file all the data
            if ((test_state_error.at(k)>STATE_ERR_THD) || (test_output_error.at(k)>OUTPUT_ERR_THD)) {
                std::string matlabLine;
                matlabLine = "save('test_discrete_ss_ti_" + std::to_string(k+1) + ".mat','n','m','p','A','B','C','D','initial_state','t','in','state','output');";
                matlabPtr->eval(convertUTF8StringToUTF16String(matlabLine));
            }
        }
    }
    std::cout << "Tests completed, plotting results" << std::endl;

    // Check NaN and Inf values
    if ((count_all(test_state_error, is_inf)>0) || (count_all(test_output_error, is_inf)>0)) {
        std::cout << "Test aborted, there are Inf values in the errors" << std::endl;
    }

    // Plot test results
    matlab::data::ArrayFactory factory;
    matlab::data::TypedArray<double> m_state_error = factory.createArray({NUM_TEST,1}, test_state_error.begin(), test_state_error.end());
    matlab::data::TypedArray<double> m_output_error = factory.createArray({NUM_TEST,1}, test_output_error.begin(), test_output_error.end());
    matlabPtr->eval(u"clear all; close all;");
    matlabPtr->setVariable(u"state_error", std::move(m_state_error));
    matlabPtr->setVariable(u"output_error", std::move(m_output_error));
    matlabPtr->eval(u"figure('visible', 'off'),bar(1:1:length(state_error),state_error),grid,xlabel('Test number'),ylabel('Percentage error')");
    matlabPtr->eval(u"print -djpeg test_discrete_ss_ti_state_error.jpg");
    matlabPtr->eval(u"figure('visible', 'off'),bar(1:1:length(output_error),output_error),grid,xlabel('Test number'),ylabel('Percentage error')");
    matlabPtr->eval(u"print -djpeg test_discrete_ss_ti_output_error.jpg");

    // Terminate MATLAB session
    matlab::engine::terminateEngineClient();

	return 0;
}
