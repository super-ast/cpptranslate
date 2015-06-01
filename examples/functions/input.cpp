int ans() {
  int x;
  int a,b,c;
  a=b=c=42;
  return a=b=c=42;
}

bool bf(int x, bool& b) {
  b = false;
  return (x + ans()) == 42;
}

bool cf(const int x, const int& y) {
  return x == y;
}

void vf() {
  vf();
}

int main() {
  bool b = true;
  bool c = bf(0, b);
  const int x = ans();
  c = cf(x, x);
}
