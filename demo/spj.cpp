#include <iostream>
#include <fstream>

#define TRUE 0
#define FALSE 1
#define ERR 2

using namespace std;

int main(int argc, char * argv[])
{
    if (argc < 4) {
        cout << "args error" << endl;
        return ERR;
    }
    ifstream std_in(argv[1]);
    ifstream std_out(argv[2]);
    ifstream user_out(argv[3]);

    /* special judge start */

    int n;
    user_out >> n;
    if (n < 128) {
        cout << "太小了" << endl;
        return FALSE;
    }
    else if (n > 256) {
        cout << "太大了" << endl;
        return FALSE;
    }

    /* special judge end */

    return TRUE;
}