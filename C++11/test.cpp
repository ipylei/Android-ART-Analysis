#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <vector>

using namespace std;

int main()
{

    vector<int> aIntVector = {1, 2, 3, 4, 5, 6}; // 21
    // accumulate将累加指定范围的元素。同时指定了一个初值100。最终sum100的值是91
    auto sum100 = accumulate(aIntVector.begin(), aIntVector.end(), 100);
    std::cout << sum100 << std::endl;

    auto sum1000 = accumulate(aIntVector.begin(), aIntVector.end(), 0, [](int a, int b)
                              { return a + b + 1000; });

    std::cout << sum1000;
    return 0;
}