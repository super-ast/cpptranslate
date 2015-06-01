#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/AST/StmtIterator.h"

// RapidJson library for JSON
#include "rapidjson/document.h"


// ******************************
// Functions for visiting the AST
// ******************************
class SuperastCPP
    : public clang::RecursiveASTVisitor<SuperastCPP> {
public:
  explicit SuperastCPP(clang::ASTContext *context);

  // STATEMENTS
  bool TraverseStmt(clang::Stmt* S);

  bool TraverseIfStmt(clang::IfStmt* ifs);
  bool TraverseReturnStmt(clang::ReturnStmt* ret);
  bool TraverseWhileStmt(clang::WhileStmt* whileStmt);
  bool TraverseForStmt(clang::ForStmt* forStmt);

  // Do while statement
  bool TraverseDoStmt(clang::DoStmt* doStmt);

  bool TraverseExpr(clang::Expr* expr); // Traverse expr, forward to other classes
  bool TraverseCompoundStmt(clang::CompoundStmt* compoundStmt);

  // MACRO FOR DISPATCHING ALL UNARY OPERATORS
#define UNARYOP_LIST()                                                         \
    OPERATOR(PostInc) OPERATOR(PostDec) OPERATOR(PreInc) OPERATOR(PreDec)      \
    OPERATOR(AddrOf) OPERATOR(Deref) OPERATOR(Plus) OPERATOR(Minus)            \
    OPERATOR(Not) OPERATOR(LNot) OPERATOR(Real) OPERATOR(Imag)                 \
    OPERATOR(Extension)

#define OPERATOR(Name)                                                         \
    bool TraverseUnary##Name(clang::UnaryOperator* uop) {                      \
      return TraverseUnaryOperator(uop);                                       \
    }

  UNARYOP_LIST()

#undef OPERATOR
#undef UNARYOP_LIST


  // MACRO FOR DISPATCHING ALL BINARY OPERATORS
#define BINOP_LIST()                                                           \
    OPERATOR(PtrMemD) OPERATOR(PtrMemI) OPERATOR(Mul) OPERATOR(Div)            \
    OPERATOR(Rem) OPERATOR(Add) OPERATOR(Sub) OPERATOR(Shl) OPERATOR(Shr)      \
    OPERATOR(LT) OPERATOR(GT) OPERATOR(LE) OPERATOR(GE) OPERATOR(EQ)           \
    OPERATOR(NE) OPERATOR(And) OPERATOR(Xor) OPERATOR(Or) OPERATOR(LAnd)       \
    OPERATOR(LOr) OPERATOR(Assign) OPERATOR(Comma)

#define OPERATOR(Name)                                                         \
  bool TraverseBin##Name(clang::BinaryOperator* bop) {                         \
    return TraverseBinaryOperator(bop);                                        \
  }

  BINOP_LIST()

#undef OPERATOR
#undef BINOP_LIST

  // // MACRO FOR DISPATCHING ALL COMPOUND ASSIGNMENT OPERATORS 
#define CAO_LIST()                                                             \
    OPERATOR(Mul) OPERATOR(Div) OPERATOR(Rem) OPERATOR(Add) OPERATOR(Sub)      \
    OPERATOR(Shl) OPERATOR(Shr) OPERATOR(And) OPERATOR(Or) OPERATOR(Xor)

#define OPERATOR(Name)                                                         \
  bool TraverseBin##Name##Assign(clang::CompoundAssignOperator* caop) {        \
    return TraverseCompoundAssignOperator(caop);                               \
  }

  CAO_LIST()

#undef OPERATOR
#undef CAO_LIST

  bool TraverseUnaryOperator(clang::UnaryOperator* uop);
  bool TraverseBinaryOperator(clang::BinaryOperator* bop);
  bool TraverseCompoundAssignOperator(clang::CompoundAssignOperator* caop);
  bool TraverseCXXOperatorCallExpr(clang::CXXOperatorCallExpr* operatorCallExpr);
  bool TraverseCXXMemberCallExpr(clang::CXXMemberCallExpr* memberCall);
  bool TraverseMemberExpr(clang::MemberExpr* memberExpr);

  bool TraverseDeclRefExpr(clang::DeclRefExpr* declRefExpr);

  bool TraverseIntegerLiteral(clang::IntegerLiteral* lit);
  bool TraverseFloatingLiteral(clang::FloatingLiteral* lit);
  bool TraverseCharacterLiteral(clang::CharacterLiteral* lit);
  bool TraverseStringLiteral(clang::StringLiteral* lit);
  bool TraverseCXXBoolLiteralExpr(clang::CXXBoolLiteralExpr* lit);

  bool TraverseCallExpr(clang::CallExpr* call);
  bool TraverseDeclStmt(clang::DeclStmt* declStmt);

  // NOT SUPPORTED
  bool TraverseBreakStmt(clang::BreakStmt* breakStmt);
  bool TraverseLabelStmt(clang::LabelStmt* labelStmt);
  bool TraverseGotoStmt(clang::GotoStmt* gotoStmt);


  // DECLARATIONS
  bool TraverseDecl(clang::Decl* D);
  
  bool TraverseTranslationUnitDecl(clang::TranslationUnitDecl* unitDecl);
  bool TraverseFunctionDecl(clang::FunctionDecl* functionDecl);
  bool TraverseVarDecl(clang::VarDecl* var);
  // Forward paramVarDecl to varDecl
  bool TraverseParmVarDecl(clang::ParmVarDecl* parmVarDecl);

  // STRUCT/UNION/CLASS DECLARATION
  bool TraverseFieldDecl(clang::FieldDecl* fieldDecl);
  bool TraverseCXXRecordDecl(clang::CXXRecordDecl* cxxRecordDecl);
  bool TraverseCXXMethodDecl(clang::CXXMethodDecl* D);

private:
  // Adds an id to the object and increments currentId;
  void addId(rapidjson::Value& object);
  void addLine(rapidjson::Value& object, clang::Stmt* stmt);
  void addLine(rapidjson::Value& object, clang::Decl* decl);
  void addCol(rapidjson::Value& object, clang::Stmt* stmt);
  void addCol(rapidjson::Value& object, clang::Decl* decl);
  void addPos(rapidjson::Value& object, clang::Stmt* stmt);
  void addPos(rapidjson::Value& object, clang::Decl* decl);

  // If son is not an array, will be an array containing the former one
  void ensureSonIsArray();
  void addElemsToArray(rapidjson::Value& parentValue,
      rapidjson::Value& elemsValue);

  rapidjson::Value createBlockValue(rapidjson::Value& arrayValue);
  rapidjson::Value createTypeValue(const std::string& type);
  rapidjson::Value createTypeValue(const clang::Type* type);
  rapidjson::Value createBinOpValue(const std::string& opcode, 
      rapidjson::Value& leftValue, rapidjson::Value& rightValue);
  rapidjson::Value createIntegerValue(const int64_t value);
  rapidjson::Value createFloatingValue(const double value);
  rapidjson::Value createStringValue(const std::string& value);
  rapidjson::Value createBoolValue(const bool value);
  rapidjson::Value createIdentifierValue(const std::string& name);
  bool isSTLVectorType(const clang::QualType qualType);
  // Get vector type from string
  rapidjson::Value createVectorValue(const clang::QualType qualType);
  // Error and Warning
  rapidjson::Value createMessageValue(clang::Stmt* stmt, const std::string& type,
      const std::string& value, const std::string& description);
  rapidjson::Value createMessageValue(clang::Decl* decl, const std::string& type,
      const std::string& value, const std::string& description);

  clang::ASTContext *context;
  unsigned currentId;
  rapidjson::Value sonValue; // Each call will return this
  bool iofunctionStarted; // If it is an already started chain of print function
};


// ******************************
// Interface to read from the AST
// ******************************
class SuperastCPPConsumer : public clang::ASTConsumer {
public:
  explicit SuperastCPPConsumer(clang::ASTContext *context)
    : Visitor(context) {}

  virtual void HandleTranslationUnit(clang::ASTContext &context) {
    // TODO change this to skip all headers and start at begin of main file
    Visitor.TraverseDecl(context.getTranslationUnitDecl());
  }
private:
  SuperastCPP Visitor;
};



// ************************************************************
// Interface to execute specific actions as part of compilation
// ************************************************************
class SuperastCPPAction : public clang::ASTFrontendAction {
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &Compiler, llvm::StringRef) {
    return std::unique_ptr<clang::ASTConsumer>(
        new SuperastCPPConsumer(&Compiler.getASTContext()));
  }
};

