int main() {

  // Var declaration inside while inside x
  while(int x = 0) {
    x = 1;
  }

  // Var declaration inside if
  if (int x = 0) {
    x = 1;
  }

  // Comma operator
  for (int i = 0, j = 0; i < 3; ++i, ++j);

  // Do while
  do {
    int x = 1;
  } while(0);

  // Break
  while (1) {
    break;
  }
  
  // Goto
  goto end;
  end:;
}
