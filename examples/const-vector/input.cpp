#include <vector>
using namespace std;
void f(vector<int> v) {}
void f2(vector<vector<int> > v) {}
void f3(const vector<vector<int> > v) {}
void f4(const vector<int> v) {}
void f5(const vector<vector<int> >& v) {}
int main() {
  vector<int> a(4);
  const vector<int> b(5);
  vector<vector<int> > c(4, vector<int>(2));

}
