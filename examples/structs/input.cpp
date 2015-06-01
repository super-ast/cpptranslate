struct myStruct {
  int intMember1;
  int intMember2;
  char charMember3;
  float floatMember5;
  double doubleMember6;
  long long longlongMember7;
  unsigned long long unsignedlonglongMember8;
};

  
int main() {
  myStruct ms;
  ms.intMember1 = 1;
  ms.intMember2 = ms.intMember1 + 1;
  ms.charMember3 = '5';
  ms.floatMember5 = 2.343f;
  ms.floatMember5 = 2.343;
  ms.doubleMember6 = 1;
  ms.doubleMember6 = 1.0;
  ms.doubleMember6 = 1.0f;
  ms.longlongMember7 = 1;
  ms.longlongMember7 = 1LL;
  ms.unsignedlonglongMember8 = 100000000000000000ull;
}
