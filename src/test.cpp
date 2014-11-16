#include <vector>

#include <cstdio>

class test {
public:
    test();
};

test::test()
{
    int a = 1, b = 2;
    std::vector<int*> test;
    test.push_back(&a);
    test.push_back(&b);
    for(auto t : test) std::printf("%d\n", *t);
}

// int main()
// {
//     test blah;
// }
