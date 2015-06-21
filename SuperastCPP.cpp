#include "./SuperastCPP.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <map>
#include <cassert>
#include <iostream>
#include <ostream>
#include <fstream>

// Our document
rapidjson::Document document;
rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

// MAP TRANSLATIONS
const std::map<std::string,std::string> UNARY_OP_MAPPING {
    {"!", "not"},
    {"-", "neg"},
    {"+", "pos"},
};

const std::map<std::string,std::string> BINARY_OP_MAPPING {
    {"||", "or"},
    {"&&", "and"},
};

const std::map<std::string,std::string> VECTOR_TYPE_MAPPING {
    {"float", "double"},
    {"char", "string"},
};

// MACRO FOR TRAVERSING
#define TRY_TO(CALL_EXPR)                                                      \
do {                                                                           \
  if (!getDerived().CALL_EXPR)                                                 \
    return false;                                                              \
} while (0)

// Custom category for command-line option
static llvm::cl::OptionCategory SuperastCPPCategory("superast-cpp options");

// Output configuration
const std::string PRINT_NAME = "operator<<";
const std::string READ_NAME = "operator>>";
const std::string VECTOR_POS_NAME = "operator[]";
const std::string PRINT_FLAG_TYPE = "basic_ostream<char, struct std::char_traits"
    "<char> > &(basic_ostream<char, struct std::char_traits<char> > &)";
const std::string PRINT_TYPE = "ostream";
const std::string READ_TYPE = "istream";
const std::string VECTOR_TYPE = "class std::vector<";
const std::string STRING_TYPE = "class std::basic_string<char>";

// CONSTRUCTOR
SuperastCPP::SuperastCPP(clang::ASTContext *context)
    : context(context), 
      currentId(0),
      sonValue(),
      iofunctionStarted(false) {
}

/*******************
 * STATEMENTS
 ******************/
bool SuperastCPP::TraverseStmt(clang::Stmt* S) {
  // If null, or not in main file, skip.
  if (!S || !context->getSourceManager().isInMainFile(S->getLocStart())) {
    return true;
  }
  // Default call. Will call all other Traverses.
  return RecursiveASTVisitor::TraverseStmt(S);
}


  // IF STATEMENT 
bool SuperastCPP::TraverseIfStmt(clang::IfStmt* ifs) {
  rapidjson::Value ifValue(rapidjson::kObjectType);
  addId(ifValue);
  addPos(ifValue, ifs);
  ifValue.AddMember("type", "conditional", allocator);

  // Check if the condition contains a var declaration
  if (ifs->getConditionVariable()) {
    ifValue.AddMember("condition", createMessageValue(ifs->getConditionVariable(), "error",
        "condition-variable",
        "Variable declarations are not allowed in if conditions"),
        allocator);
  }
  else {
    // condition part
    TRY_TO(TraverseStmt(ifs->getCond()));

    rapidjson::Value conditionValue;
    conditionValue = sonValue;
    ifValue.AddMember("condition", conditionValue, allocator);
  }

  // then part
  TRY_TO(TraverseStmt(ifs->getThen()));
  // Now sonValue contains the arrayValue
  ensureSonIsArray();
  rapidjson::Value blockValue = createBlockValue(sonValue);
  ifValue.AddMember("then", blockValue, allocator);

  // if has else, print
  if (ifs->getElse()) {
    TRY_TO(TraverseStmt(ifs->getElse()));
    // sonValue contains the arrayValue
    ensureSonIsArray();
    blockValue = createBlockValue(sonValue);
    ifValue.AddMember("else", blockValue, allocator);
  }

  sonValue = ifValue;
  return true;
}

// RETURN STATEMENT
bool SuperastCPP::TraverseReturnStmt(clang::ReturnStmt* ret) {
  rapidjson::Value returnValue(rapidjson::kObjectType);
  addId(returnValue);
  addPos(returnValue, ret);
  
  TRY_TO(TraverseStmt(ret->getRetValue()));

  returnValue.AddMember("type", "return", allocator);
  returnValue.AddMember("expression", sonValue, allocator);

  sonValue = returnValue;
  return true;
}


// WHILE STMT
bool SuperastCPP::TraverseWhileStmt(clang::WhileStmt* whileStmt) {

  rapidjson::Value whileValue(rapidjson::kObjectType);
  addId(whileValue);
  addPos(whileValue, whileStmt);
  whileValue.AddMember("type", "while", allocator);

  // Check if the condition of the while contains a var declaration
  if (whileStmt->getConditionVariable()) {
    whileValue.AddMember("condition", createMessageValue(whileStmt->getConditionVariable(),
        "error", "condition-variable", 
        "Variable declarations are not allowed in while conditions"),
        allocator);
  }
  else {
    // Get condition
    TRY_TO(TraverseStmt(whileStmt->getCond()));
    whileValue.AddMember("condition", sonValue, allocator);
  }

  // Get the body
  TRY_TO(TraverseStmt(whileStmt->getBody()));
  // Now sonValue contains the arrayValue
  ensureSonIsArray();
  rapidjson::Value blockValue = createBlockValue(sonValue);
  whileValue.AddMember("block", blockValue, allocator);

  sonValue = whileValue;
  return true;
}



// FOR STMT
bool SuperastCPP::TraverseForStmt(clang::ForStmt* forStmt) {
  rapidjson::Value forValue(rapidjson::kObjectType);
  addId(forValue);
  addPos(forValue, forStmt);
  forValue.AddMember("type", "for", allocator);
  
  // Init
  TRY_TO(TraverseStmt(forStmt->getInit()));
  // This could be a compound statement
  if (sonValue.IsArray()) {
    sonValue = createMessageValue(forStmt, "error", "compoundStmt", 
        "Compound Statements are not allowed in for loop init");
  }
  forValue.AddMember("init", sonValue, allocator);

  // Cond
  TRY_TO(TraverseStmt(forStmt->getCond()));
  forValue.AddMember("condition", sonValue, allocator);

  // Post
  TRY_TO(TraverseStmt(forStmt->getInc()));
  forValue.AddMember("post", sonValue, allocator);

  // Get the body
  TRY_TO(TraverseStmt(forStmt->getBody()));
  ensureSonIsArray();
  rapidjson::Value blockValue = createBlockValue(sonValue);
  forValue.AddMember("block", blockValue, allocator);

  sonValue = forValue;
  return true;
}

bool SuperastCPP::TraverseDoStmt(clang::DoStmt* doStmt) {
  sonValue = createMessageValue(doStmt, "error", "do/while statement",
      "Do/While statements are not allowed");
  return true;
}

// EXPRESIONS: Actually not used
bool SuperastCPP::TraverseExpr(clang::Expr* expr) {
  std::cerr << " In expression" << std::endl;
  if (clang::UnaryOperator* uop = clang::dyn_cast<clang::UnaryOperator>(expr)) {
    return TraverseUnaryOperator(uop);
  }
  if (clang::BinaryOperator* bop = clang::dyn_cast<clang::BinaryOperator>(expr)) {
    return TraverseBinaryOperator(bop);
  }
  return true;
}

// COMPOUND STATEMENTS, A block of statements in clangAST
bool SuperastCPP::TraverseCompoundStmt(clang::CompoundStmt* compoundStmt) {
  rapidjson::Value arrayValue(rapidjson::kArrayType);
  // Traverse each statement and append it to the array
  for (clang::Stmt* stmt : compoundStmt->body()) {
    TRY_TO(TraverseStmt(stmt));
    if (sonValue.IsArray()) {
      addElemsToArray(arrayValue, sonValue);
    }
    else {
      arrayValue.PushBack(sonValue, allocator);
    }
  }
  sonValue = arrayValue;
  return true;
}

// UNARY OPERATOR
bool SuperastCPP::TraverseUnaryOperator(clang::UnaryOperator* uop) {

  std::string opString = uop->getOpcodeStr(uop->getOpcode());
  auto it = UNARY_OP_MAPPING.find(opString);
  if (it != UNARY_OP_MAPPING.end()) opString = it->second;

  if (opString == "++" || opString == "--") {
    if (uop->isPrefix(uop->getOpcode())) opString += "_";
    else opString = "_" + opString;
  }

  TRY_TO(TraverseStmt(uop->getSubExpr()));

  rapidjson::Value unaryOpValue(rapidjson::kObjectType);
  addId(unaryOpValue);
  addPos(unaryOpValue, uop);

  unaryOpValue.AddMember("type",
                         rapidjson::Value().SetString(opString.c_str(),
                                                      opString.size(),
                                                      allocator),
                         allocator);
  unaryOpValue.AddMember("expression", sonValue, allocator);

  sonValue = unaryOpValue;
  return true;
}


// BINARY OPERATOR
bool SuperastCPP::TraverseBinaryOperator(clang::BinaryOperator* bop) {
  // Save current status
  std::string opString = bop->getOpcodeStr();
  auto it = BINARY_OP_MAPPING.find(opString);
  if (it != BINARY_OP_MAPPING.end()) opString = it->second;

  // Comma operator, give a WARNING
  if (opString == ",") {
    sonValue = createMessageValue(bop, "warning", "comma operator",
        "We recommend not using the comma operator!");
    return true;
  }

  rapidjson::Value leftValue;
  TRY_TO(TraverseStmt(bop->getLHS()));
  leftValue = sonValue;

  rapidjson::Value rightValue;
  TRY_TO(TraverseStmt(bop->getRHS()));
  rightValue = sonValue;

  rapidjson::Value binOpValue = createBinOpValue(opString, leftValue, rightValue);
  addPos(binOpValue, bop);

  sonValue = binOpValue;
  return true;
}

// Dispatch to binary operator
bool SuperastCPP::TraverseCompoundAssignOperator(clang::CompoundAssignOperator* caop) {
  return TraverseBinaryOperator(caop);
}

bool SuperastCPP::TraverseCXXOperatorCallExpr(clang::CXXOperatorCallExpr* operatorCallExpr) {
  // IF THERE IS A PRINT
  auto decl = llvm::dyn_cast<clang::FunctionDecl>(operatorCallExpr->getCalleeDecl());
  if (!decl) {
    std::cerr << "Unknown function in CXXOperatorCallExpr" << std::endl;
    return true;
  }
  const std::string functionName = decl->getNameInfo().getAsString();
  if (functionName == PRINT_NAME || functionName == READ_NAME) {
    bool isFirst = false;
    if (!iofunctionStarted) {
      isFirst = true;
      iofunctionStarted = true;
    }

    rapidjson::Value arrayValue(rapidjson::kArrayType);
    for (unsigned i = 0; i < operatorCallExpr->getNumArgs(); ++i) {
      TRY_TO(TraverseStmt(operatorCallExpr->getArg(i)));
      if (i == 0) {
        // If sonValue is not an array, is because it reached the begin of
        // call, which is the cout/cerr/stream class
        if (sonValue.IsArray()) {
          arrayValue = sonValue;
        }
      }
      else {
        arrayValue.PushBack(sonValue, allocator);
      }
    }
    
    if (!isFirst) {
      sonValue = arrayValue;
    }
    else {
      iofunctionStarted = false;

      rapidjson::Value functionValue(rapidjson::kObjectType);
      addId(functionValue);
      addPos(functionValue, operatorCallExpr);
      functionValue.AddMember("type", "function-call", allocator);
      if (functionName == PRINT_NAME) 
        functionValue.AddMember("name", "print", allocator);
      else functionValue.AddMember("name", "read", allocator);
      functionValue.AddMember("arguments", arrayValue, allocator);
      sonValue = functionValue;
    }
  }
  else if (functionName == VECTOR_POS_NAME) {
    // Operator []
    TRY_TO(TraverseStmt(operatorCallExpr->getArg(0)));
    rapidjson::Value leftValue(rapidjson::kObjectType);
    leftValue = sonValue;
    TRY_TO(TraverseStmt(operatorCallExpr->getArg(1)));
    rapidjson::Value rightValue(rapidjson::kObjectType);
    rightValue = sonValue;
    sonValue = createBinOpValue("[]", leftValue, rightValue);
  }
  else {
    std::cerr << "Operator call not defined: " << functionName << std::endl;
  }
    return true;
}

// CXX CLASS MEMBER CALL, with dot operator ('.')
bool SuperastCPP::TraverseCXXMemberCallExpr(
    clang::CXXMemberCallExpr* memberCall) {
  // Method Decl
  clang::CXXMethodDecl* methodDecl = memberCall->getMethodDecl();
  // Implicit obj
  clang::Expr* objectExpr = memberCall->getImplicitObjectArgument();
  // If this is an implicitCast, get the subExpr
  if (clang::ImplicitCastExpr* implicitExpr = 
      llvm::dyn_cast<clang::ImplicitCastExpr>(objectExpr)) {
    objectExpr = implicitExpr->getSubExpr();
  }

  const std::string& methodName = methodDecl->getNameInfo().getAsString();
  //const clang::Type* type = methodDecl->getReturnType().getTypePtr();

  rapidjson::Value rightValue(rapidjson::kObjectType);
  addId(rightValue);
  addPos(rightValue, memberCall);
  rightValue.AddMember("type", "function-call", allocator);
  rightValue.AddMember("name",
                      rapidjson::Value().SetString(methodName.c_str(),
                                                   methodName.size(),
                                                   allocator),
                      allocator);

  // For each argument
  rapidjson::Value arrayValue(rapidjson::kArrayType);
  for (auto arg : memberCall->arguments()) {
    TRY_TO(TraverseStmt(arg));
    arrayValue.PushBack(sonValue, allocator);
  }
  rightValue.AddMember("arguments", arrayValue, allocator);

  // Uncomment to add the type
  //rightValue.AddMember("return-type", createTypeValue(type), allocator);
  
  TRY_TO(TraverseStmt(objectExpr));
  // Object from which the method is called
  rapidjson::Value memberCallValue = createBinOpValue(".", sonValue, rightValue);
  addPos(memberCallValue, memberCall);

  sonValue = memberCallValue;
  return true;
}


// MEMBER EXPR. Access to a member of a struct or class.
// Translate to binary operator with operator []
bool SuperastCPP::TraverseMemberExpr(clang::MemberExpr* memberExpr) {
  // Accessing a member. Translate into binary operator []
  // Save current status
  const std::string opString = "[]";

  rapidjson::Value leftValue;
  TRY_TO(TraverseStmt(memberExpr->getBase()));
  leftValue = sonValue;

  // Build right value from Declaration
  rapidjson::Value rightValue;
  TRY_TO(TraverseDecl(memberExpr->getMemberDecl()));
  rightValue = sonValue;

  // Instead of data-type, just a type 'string'
  rightValue.AddMember("type", "string", allocator);
  rightValue.EraseMember(rightValue.FindMember("data-type"));
  // The id as 'value', not 'name'
  rightValue.AddMember("value", rightValue["name"], allocator);
  rightValue.EraseMember(rightValue.FindMember("name"));
      
  rapidjson::Value memberExprValue = createBinOpValue(opString, leftValue, rightValue);
  addPos(memberExprValue, memberExpr);

  sonValue = memberExprValue;
  return true;
}

// DECL REF EXPR. Contains the Identifiers, but also cin/count/endl/cerr
bool SuperastCPP::TraverseDeclRefExpr(clang::DeclRefExpr* declRefExpr) {
  const std::string& name = declRefExpr->getNameInfo().getAsString();
  const std::string& typeName = declRefExpr->getType().getAsString();

  // Cout / Cin / Cerr / Unused flag
  if (typeName == PRINT_TYPE || typeName == READ_TYPE || 
      (typeName == PRINT_FLAG_TYPE && name != "endl")) {
    return true;
  }

  rapidjson::Value identifierValue;

  if (typeName == PRINT_FLAG_TYPE) {
    identifierValue = rapidjson::Value(rapidjson::kObjectType);
    addId(identifierValue);
    addPos(identifierValue, declRefExpr);
    if (name == "endl") {
      identifierValue.AddMember("type", "string", allocator);
      identifierValue.AddMember("value", "\n", allocator);
    }
  }
  else {
    identifierValue = createIdentifierValue(name);
    addId(identifierValue);
    addPos(identifierValue, declRefExpr);
  }

  sonValue = identifierValue;
  return true;
}

// INTEGER LITERAL
bool SuperastCPP::TraverseIntegerLiteral(clang::IntegerLiteral* lit) {
  int64_t value = lit->getValue().getSExtValue();
  rapidjson::Value integerValue = createIntegerValue(value);
  addId(integerValue);
  addPos(integerValue, lit);

  sonValue = integerValue;
  return true;
}

// FLOAT
// FLOATING LITERAL
bool SuperastCPP::TraverseFloatingLiteral(clang::FloatingLiteral* lit) {
  double value = lit->getValueAsApproximateDouble();
  rapidjson::Value floatingValue = createFloatingValue(value);
  addId(floatingValue);
  addPos(floatingValue, lit);

  sonValue = floatingValue;;
  return true;
}

// Character literal
bool SuperastCPP::TraverseCharacterLiteral(clang::CharacterLiteral* lit) {
  std::string value = std::string(1,char(lit->getValue()));
  rapidjson::Value stringValue = createStringValue(value);
  addId(stringValue);
  addPos(stringValue, lit);

  sonValue = stringValue;;
  return true;
}

// STRING LITERAL
bool SuperastCPP::TraverseStringLiteral(clang::StringLiteral* lit) {
  std::string value = lit->getString().str();
  rapidjson::Value stringValue = createStringValue(value);
  addId(stringValue);
  addPos(stringValue, lit);

  sonValue = stringValue;;
  return true;
}

// BOOL LITERAL
bool SuperastCPP::TraverseCXXBoolLiteralExpr(clang::CXXBoolLiteralExpr* lit) {
  bool value = lit->getValue();
  rapidjson::Value boolValue = createBoolValue(value);
  addId(boolValue);
  addPos(boolValue, lit);

  sonValue = boolValue;;
  return true;
}

bool SuperastCPP::TraverseFunctionDecl(clang::FunctionDecl* functionDecl) {

  const std::string functionName = functionDecl->getNameInfo().getAsString();

  rapidjson::Value functionValue(rapidjson::kObjectType);
  addId(functionValue);
  addPos(functionValue, functionDecl);
  
  functionValue.AddMember("type", "function-declaration", allocator);
  // Add the name
  functionValue.AddMember(
      "name", 
      rapidjson::Value().SetString(functionName.c_str(), 
                                   functionName.size(),
                                   allocator), 
      allocator);

  // Add the return type
  clang::QualType qualType = functionDecl->getCallResultType().getNonLValueExprType(*context);
  functionValue.AddMember("return-type", createTypeValue((qualType.getTypePtr())), allocator);

  // Array of parameters
  rapidjson::Value parametersValue(rapidjson::kArrayType);

  // Traverse parameters
  for (unsigned int i = 0; i < functionDecl->param_size(); ++i) {
    TRY_TO(TraverseDecl(functionDecl->getParamDecl(i)));

    // Add the parameter to array
    parametersValue.PushBack(sonValue, allocator);
  }

  // Add parameters to functionValue
  functionValue.AddMember("parameters", parametersValue, allocator);
  
  // If this is a function definition, traverse definition.
  if (functionDecl->isThisDeclarationADefinition()) {
    TRY_TO(TraverseStmt(functionDecl->getBody()));
    ensureSonIsArray();
    rapidjson::Value blockValue = createBlockValue(sonValue);
    functionValue.AddMember("block", blockValue, allocator);
  }
  else {
    // If not definition, add empty block
    rapidjson::Value emptyArray(rapidjson::kArrayType);
    functionValue.AddMember("block", createBlockValue(emptyArray), allocator);
  }

  // END BODY TRAVERSE
  sonValue = functionValue;
  return true;
}


/***************
 * DECLARATIONS
 **************/
bool SuperastCPP::TraverseDecl(clang::Decl* D) {
  // Check if is a TranslationUnitDecl, entry point not in main.
  if (clang::dyn_cast<clang::TranslationUnitDecl>(D)) {
    RecursiveASTVisitor::TraverseDecl(D);
  }
  // Do not analyze the source not in main file.
  if (!D || !context->getSourceManager().isInMainFile(D->getLocStart())) {
    return true;
  }
  // Default call. This will call each TraverseCLASSNAME.
  return RecursiveASTVisitor::TraverseDecl(D);
}

// The AST entry point. Here begins everything.
bool SuperastCPP::TraverseTranslationUnitDecl(
    clang::TranslationUnitDecl* unitDecl) {
  // Create the block object at root of DOM
  document.SetObject();

  // Set the pointer. Statements from root will be added here
  addId(document);
  document.AddMember("statements", rapidjson::kArrayType, allocator);

  for (auto declaration : unitDecl->decls()) {
    TRY_TO(TraverseDecl(declaration));
    if (!sonValue.IsNull()) {
      if (sonValue.IsArray()) {
        addElemsToArray(document["statements"], sonValue);
      }
      else {
        document["statements"].PushBack(sonValue, allocator);
      }
    }
  }
  return true;
}

// Traverse VAR DECL
bool SuperastCPP::TraverseVarDecl(clang::VarDecl* var) {
  const std::string varName = var->getName().str();

  rapidjson::Value varValue(rapidjson::kObjectType);
  addId(varValue);
  addPos(varValue, var);

  // Variable Declaration if it is not a parameter of a function
  if (!clang::dyn_cast<clang::ParmVarDecl>(var)) {
    varValue.AddMember("type", "variable-declaration", allocator);
  }
  
  varValue.AddMember("name", 
                     rapidjson::Value().SetString(varName.c_str(), 
                                                  varName.size(), 
                                                  allocator), 
                     allocator);

  // NonLValueExpr to remove LValueExpression for references. 
  // There are more options
  clang::QualType qualType = var->getType().getNonLValueExprType(*context);
  const clang::Type* type = qualType.getTypePtr();
  bool isConst = var->getType().isConstQualified() | qualType.isConstQualified();

  //std::cerr << " type: " << type->getCanonicalTypeInternal().getAsString();
  varValue.AddMember("data-type", createTypeValue(type), allocator);
  varValue.AddMember("is-reference", var->getType()->isReferenceType(), allocator);
  varValue.AddMember("is-const", isConst, allocator);
  

  if (var->hasInit() && !type->isStructureType() && 
      !isSTLVectorType(type->getCanonicalTypeInternal())) {
    rapidjson::Value exprValue(rapidjson::kObjectType);
    TRY_TO(TraverseStmt(var->getInit()));

    clang::VarDecl::InitializationStyle initStyle = var->getInitStyle();
    switch (initStyle) {
      case clang::VarDecl::CInit:
      case clang::VarDecl::CallInit:
        // We don't distinguis between these two initializations
        if (!sonValue.IsNull()) {
          varValue.AddMember("init", sonValue, allocator);
        }
        // Call style initializer (int x(1))
        break;
      case clang::VarDecl::ListInit:
        // C++11 Initializer list. Not supported by us.
        break;
    }
  }

  sonValue = varValue;
  return true;
}

bool SuperastCPP::TraverseParmVarDecl(clang::ParmVarDecl* parmVarDecl) {
  return TraverseVarDecl(parmVarDecl);
}

bool SuperastCPP::TraverseFieldDecl(clang::FieldDecl* fieldDecl) {

  const std::string varName = fieldDecl->getName().str();

  rapidjson::Value varValue(rapidjson::kObjectType);
  addId(varValue);
  addPos(varValue, fieldDecl);
  varValue.AddMember("name", 
                     rapidjson::Value().SetString(varName.c_str(), 
                                                  varName.size(), 
                                                  allocator), 
                     allocator);

  clang::QualType qualType = fieldDecl->getType().getNonLValueExprType(*context);
  varValue.AddMember("data-type", createTypeValue((qualType.getTypePtr())), allocator);

  // Will be an array in struct decl. Otherwise, just a call.
  if (sonValue.IsArray()) {
    sonValue.PushBack(varValue, allocator);
  }
  else {
    sonValue = varValue;
  }

  return true;
}

bool SuperastCPP::TraverseCXXRecordDecl(clang::CXXRecordDecl* cxxRecordDecl) {
  // cxxRecordDecls can be checked with isCLike, isImplicit, isStruct, POD, Trivial.
  if (cxxRecordDecl->isImplicit()) return true;
  const bool isValid = true;
  bool returnValue = true;

  rapidjson::Value structValue(rapidjson::kObjectType);
  addId(structValue);
  addPos(structValue, cxxRecordDecl);

  if (!isValid) {
    structValue.AddMember("type", "invalid", allocator);
    structValue = createMessageValue(cxxRecordDecl, "error", "Struct decl", 
      "Invalid struct declaration");
  }
  else {
    const std::string structName = cxxRecordDecl->getNameAsString();
    structValue.AddMember("type", "struct-declaration", allocator);
    structValue.AddMember("name",
                          rapidjson::Value().SetString(structName.c_str(),
                                                       structName.size(),
                                                       allocator),
                          allocator);

    sonValue = rapidjson::Value(rapidjson::kArrayType);
    returnValue = RecursiveASTVisitor::TraverseCXXRecordDecl(cxxRecordDecl);
    structValue.AddMember("attributes", sonValue, allocator);
  }

  sonValue = structValue;
  return returnValue;
}

// Methods declaration. Not supported
bool SuperastCPP::TraverseCXXMethodDecl(clang::CXXMethodDecl* D) {
  // If null, just call the default
  if (!D || !context->getSourceManager().isInMainFile(D->getLocStart())) {
    return RecursiveASTVisitor::TraverseCXXMethodDecl(D);
  }

  // Just ignore the method decl
  //assert(sonValue.IsArray());
  //const std::string declString = D->getNameAsString();
  //sonValue.PushBack(createMessageValue(D, "error", "method decl",
      //"Error declaring method '" + declString + "'. No method decl allowed"),
      //allocator);
  return true;
}


// FUNCTION CALL EXPRESSION. Take care of PRINT/READ functions
bool SuperastCPP::TraverseCallExpr(clang::CallExpr* call) {
  // Get call declaration
  auto decl = llvm::dyn_cast<clang::FunctionDecl>(call->getCalleeDecl());
  if (!decl) {
    std::cerr << "UNKNOWN FUNCTION_CALL\n";
    return true;
  }

  const std::string functionName = decl->getNameInfo().getAsString();

  rapidjson::Value functionCallValue(rapidjson::kObjectType);
  addId(functionCallValue);
  addPos(functionCallValue, call);
  functionCallValue.AddMember("type", "function-call", allocator);
  rapidjson::Value nameValue(functionName.c_str(), 
                             functionName.size(), 
                             allocator);
  functionCallValue.AddMember("name", nameValue, allocator);

  rapidjson::Value argumentsValue(rapidjson::kArrayType);
  for (auto arg : call->arguments()) {
    TRY_TO(TraverseStmt(arg));
    argumentsValue.PushBack(sonValue, allocator);
  }

  functionCallValue.AddMember("arguments", argumentsValue, allocator);
  
  sonValue = functionCallValue;
  return true;
}


// DECLSTMT Allows mix of decl and statements
bool SuperastCPP::TraverseDeclStmt(clang::DeclStmt* declStmt) {
  if (declStmt->isSingleDecl()) {
    TRY_TO(TraverseDecl(declStmt->getSingleDecl()));
  }
  else {
    // Group of  declStmt
    rapidjson::Value arrayValue(rapidjson::kArrayType);
    const clang::DeclGroupRef declGroup = declStmt->getDeclGroup();
    for (auto iterator = declGroup.begin(); iterator != declGroup.end(); ++iterator) {
      TRY_TO(TraverseDecl(*iterator));
      arrayValue.PushBack(sonValue, allocator);
    }
    sonValue = arrayValue;
  }
  return true;
}


bool SuperastCPP::TraverseBreakStmt(clang::BreakStmt* breakStmt) {
  sonValue = createMessageValue(breakStmt, "error", "break statement", 
      "Breaks are not allowed");
  return true;
}

bool SuperastCPP::TraverseLabelStmt(clang::LabelStmt* gotoStmt) {
  sonValue = createMessageValue(gotoStmt, "error", "label",
      "Labels are not allowed");
  return true;
}

bool SuperastCPP::TraverseGotoStmt(clang::GotoStmt* gotoStmt) {
  sonValue = createMessageValue(gotoStmt, "error", "goto",
      "Goto is not allowed");
  return true;
}

void SuperastCPP::addId(rapidjson::Value& object) {
  object.AddMember("id", currentId++, allocator);
}

void SuperastCPP::addLine(rapidjson::Value& object, clang::Stmt* stmt) {
  clang::FullSourceLoc loc = context->getFullLoc(stmt->getLocStart());
  int lineNumber = -1;
  if (loc.isValid()) lineNumber = loc.getSpellingLineNumber();
  object.AddMember("line", lineNumber, allocator);
}

void SuperastCPP::addLine(rapidjson::Value& object, clang::Decl* decl) {
  clang::FullSourceLoc loc = context->getFullLoc(decl->getLocStart());
  int lineNumber = -1;
  if (loc.isValid()) lineNumber = loc.getSpellingLineNumber();
  object.AddMember("line", lineNumber, allocator);
}

void SuperastCPP::addCol(rapidjson::Value& object, clang::Stmt* stmt) {
  clang::FullSourceLoc loc = context->getFullLoc(stmt->getLocStart());
  int colNumber = -1;
  if (loc.isValid()) colNumber = loc.getSpellingColumnNumber();
  object.AddMember("column", colNumber, allocator);
}

void SuperastCPP::addCol(rapidjson::Value& object, clang::Decl* decl) {
  clang::FullSourceLoc loc = context->getFullLoc(decl->getLocStart());
  int colNumber = -1;
  if (loc.isValid()) colNumber = loc.getSpellingColumnNumber();
  object.AddMember("column", colNumber, allocator);
}

void SuperastCPP::addPos(rapidjson::Value& object, clang::Stmt* stmt) {
  addLine(object, stmt);
  addCol(object, stmt);
}

void SuperastCPP::addPos(rapidjson::Value& object, clang::Decl* decl) {
  addLine(object, decl);
  addCol(object, decl);
}

void SuperastCPP::ensureSonIsArray() {
  if (!sonValue.IsArray()) {
    rapidjson::Value arrayValue(rapidjson::kArrayType);
    if (!sonValue.IsNull()) {
      arrayValue.PushBack(sonValue, allocator);
    }
    sonValue = arrayValue;
  }
}

void SuperastCPP::addElemsToArray(rapidjson::Value& parentValue,
    rapidjson::Value& elemsValue) {
  assert(parentValue.IsArray());
  assert(elemsValue.IsArray());
  for (unsigned i = 0; i < elemsValue.Size(); ++i) {
    parentValue.PushBack(elemsValue[i], allocator);
  }
}

rapidjson::Value SuperastCPP::createBlockValue(rapidjson::Value& arrayValue) {
  rapidjson::Value blockValue(rapidjson::kObjectType);
  addId(blockValue);
  blockValue.AddMember("statements", arrayValue, allocator);
  return blockValue;
}

// Returns a rapidjson Value with the type
rapidjson::Value SuperastCPP::createTypeValue(const std::string& type) {
  rapidjson::Value typeValue(rapidjson::kObjectType);
  rapidjson::Value nameValue(type.c_str(), type.size(), allocator);
  addId(typeValue);
  typeValue.AddMember("name", nameValue, allocator);
  return typeValue;
}

// TYPE VALUE
rapidjson::Value SuperastCPP::createTypeValue(const clang::Type* type) {
  // Check if it is vector
  
  if (isSTLVectorType(type->getCanonicalTypeInternal()))
    return createVectorValue(type->getCanonicalTypeInternal());
  if (type->isBooleanType()) return createTypeValue("bool");
  if (type->isIntegerType()) return createTypeValue("int");
  if (type->isFloatingType()) return createTypeValue("double");
  if (type->isVoidType()) return createTypeValue("void");
  if (type->isAnyCharacterType()) return createTypeValue("string");
  // std::string
  if (type->getCanonicalTypeInternal().getAsString() == STRING_TYPE)
    return createTypeValue("string");
  if (type->isStructureType()) {
    return createTypeValue(type->
                           getAsStructureType()->
                           getAsCXXRecordDecl()->
                           getNameAsString());
  }

  return createTypeValue("Unknown: (" + std::string(type->getTypeClassName()) + ")");
}

// BINARY OPERATION VALUE
rapidjson::Value SuperastCPP::createBinOpValue(const std::string& opcode, 
                                  rapidjson::Value& leftValue,
                                  rapidjson::Value& rightValue) {
  rapidjson::Value binOpValue(rapidjson::kObjectType);
  addId(binOpValue);
  binOpValue.AddMember("type",
                       rapidjson::Value().SetString(opcode.c_str(),
                                                    opcode.size(),
                                                    allocator),
                       allocator);
  binOpValue.AddMember("left", leftValue, allocator);
  binOpValue.AddMember("right", rightValue, allocator);

  return binOpValue;
}

// INTEGER LITERAL VALUE
rapidjson::Value SuperastCPP::createIntegerValue(const int64_t value) {
  rapidjson::Value integerValue(rapidjson::kObjectType);
  integerValue.AddMember("type", "int", allocator);
  integerValue.AddMember("value", value, allocator);

  return integerValue;
}

// FLOATING LITERAL VALUE
rapidjson::Value SuperastCPP::createFloatingValue(const double value) {
  rapidjson::Value floatingValue(rapidjson::kObjectType);
  floatingValue.AddMember("type", "double", allocator);
  floatingValue.AddMember("value", value, allocator);

  return floatingValue;
}

//STRING LITERAL VALUE
rapidjson::Value SuperastCPP::createStringValue(const std::string& value) {
  rapidjson::Value stringValue(rapidjson::kObjectType);
  stringValue.AddMember("type", "string", allocator);
  stringValue.AddMember("value", 
                        rapidjson::Value().SetString(value.c_str(),
                                                     value.size(),
                                                     allocator),
                        allocator);
  return stringValue;
}

// BOOL LITERAL VALUE
rapidjson::Value SuperastCPP::createBoolValue(const bool value) {
  rapidjson::Value boolValue(rapidjson::kObjectType);
  boolValue.AddMember("type", "bool", allocator);
  boolValue.AddMember("value", value, allocator);

  return boolValue;
}

// IDENTIFIER VALUE
rapidjson::Value SuperastCPP::createIdentifierValue(const std::string& name) {
  rapidjson::Value idValue(rapidjson::kObjectType);
  idValue.AddMember("type", "identifier", allocator);
  idValue.AddMember("value", 
                    rapidjson::Value().SetString(name.c_str(),
                                                 name.size(),
                                                 allocator),
                    allocator);
  return idValue;
}

// IF IT IS A VECTOR TYPE
bool SuperastCPP::isSTLVectorType(const clang::QualType qualType) {
  const std::string& typeName = qualType.getAsString();
  return typeName.find(VECTOR_TYPE) != std::string::npos;
}

// GET VECTOR TYPE FROM STRING
rapidjson::Value SuperastCPP::createVectorValue(const clang::QualType qualType) {
  assert(isSTLVectorType(qualType));

  //rapidjson::Value vectorValue(rapidjson::kObjectType);
  //vectorValue.AddMember("id", currentId++, allocator);
  const std::string& typeName = qualType.getAsString();

  std::size_t actPos = 0;
  bool finished = false;
  unsigned depth = 0;
  std::string innerMostType;
  while (!finished) {
    // find next vector or comma
    std::size_t vectorPos = typeName.find(VECTOR_TYPE, actPos);
    std::size_t commaPos = typeName.find(",", actPos);

    std::string type = "vector";
    if (commaPos < vectorPos) {
      finished = true;
      innerMostType = typeName.substr(actPos, commaPos-actPos);
      auto it = VECTOR_TYPE_MAPPING.find(type);
      if (it != VECTOR_TYPE_MAPPING.end()) innerMostType = it->second;
    }
    else {
      actPos = vectorPos + VECTOR_TYPE.size();
      ++depth;
    }
  }

  rapidjson::Value actTypeValue(rapidjson::kObjectType);
  addId(actTypeValue);
  actTypeValue.AddMember("name", 
                         rapidjson::Value().SetString(innerMostType.c_str(),
                                                      innerMostType.size(),
                                                      allocator),
                         allocator);

  for (unsigned i = 0; i < depth; ++i) {
    rapidjson::Value aux(rapidjson::kObjectType);
    addId(aux);
    aux.AddMember("name", "vector", allocator);
    aux.AddMember("data-type", actTypeValue, allocator);
    actTypeValue = aux;
  }
  
  return actTypeValue;
}

rapidjson::Value SuperastCPP::createMessageValue(clang::Stmt* stmt, const std::string& type,
    const std::string& value, const std::string& description) {
  
  rapidjson::Value object(rapidjson::kObjectType);
  addId(object);
  addPos(object, stmt);
  object.AddMember("type",
      rapidjson::Value().SetString(type.c_str(), type.size(), allocator),
      allocator);
  object.AddMember("value",
      rapidjson::Value().SetString(value.c_str(), value.size(), allocator),
      allocator);
  object.AddMember("description",
      rapidjson::Value().SetString(description.c_str(), description.size(), allocator),
      allocator);

  return object;
}

rapidjson::Value SuperastCPP::createMessageValue(clang::Decl* decl, const std::string& type,
    const std::string& value, const std::string& description) {
  
  rapidjson::Value object(rapidjson::kObjectType);
  addId(object);
  addPos(object, decl);
  object.AddMember("type",
      rapidjson::Value().SetString(type.c_str(), type.size(), allocator),
      allocator);
  object.AddMember("value",
      rapidjson::Value().SetString(value.c_str(), value.size(), allocator),
      allocator);
  object.AddMember("description",
      rapidjson::Value().SetString(description.c_str(), description.size(), allocator),
      allocator);

  return object;
}

/***************************
 * END SuperastCPP methods
 ***************************/

// dump all the json document in a pretty format
std::ostream& dumpJsonDocument(std::ostream& os, rapidjson::Document& doc) {
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);
  // Output DOM
  return os << buffer.GetString() << std::endl;
}

// Main function
int main(int argc, const char **argv) {
  clang::tooling::CommonOptionsParser OptionsParser(argc, argv, SuperastCPPCategory);
  clang::tooling::ClangTool Tool(OptionsParser.getCompilations(),
                                 OptionsParser.getSourcePathList());

  // Run the recursive visitor
  int returnValue = Tool.run(
      clang::tooling::newFrontendActionFactory<SuperastCPPAction>().get());

  // Dump the json to stdin
  dumpJsonDocument(std::cout, document);

  // Return
  return returnValue;
}
