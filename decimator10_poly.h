#pragma once
#include <vector>

class Decimator10Poly {
public:
    Decimator10Poly(const std::vector<double>& coeffs = {}, int D = 10);

    // 输入一块 in，输出 append 到 out
    void processBlock(const std::vector<double>& in, std::vector<double>& out);

    void reset();

private:
    int m_D;
    std::vector<double> m_h;
    std::vector<double> m_state;
};
