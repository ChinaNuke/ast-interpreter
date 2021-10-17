//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//
#include <stdio.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

class StackFrame {
   /// StackFrame maps Variable Declaration to Value
   /// Which are either integer or addresses (also represented using an Integer value)
   std::map<Decl*, int> mVars;
   std::map<Stmt*, int> mExprs;
   /// The current stmt
   Stmt * mPC;
   int returnValue; // 保存当前栈帧的返回值，只考虑整数
public:
   StackFrame() : mVars(), mExprs(), mPC() {
   }

   void bindDecl(Decl* decl, int val) {
      mVars[decl] = val;
   }    
   bool hasDecl(Decl * decl) {
	   return (mVars.find(decl) != mVars.end());
   }
   int getDeclVal(Decl * decl) {
      assert (mVars.find(decl) != mVars.end());
      return mVars.find(decl)->second;
   }
   void bindStmt(Stmt * stmt, int val) {
	   mExprs[stmt] = val;
   }
   int getStmtVal(Stmt * stmt) {
	   assert (mExprs.find(stmt) != mExprs.end());
	   return mExprs[stmt];
   }
   void setPC(Stmt * stmt) {
	   mPC = stmt;
   }
   Stmt * getPC() {
	   return mPC;
   }
   void setReturnValue(int value) {
	   returnValue = value;
   }
   int getReturnValue() {
	   return returnValue;
   }
};

/// Heap maps address to a value
/*
class Heap {
public:
   int Malloc(int size) ;
   void Free (int addr) ;
   void Update(int addr, int val) ;
   int get(int addr);
};
*/

class Environment {
   std::vector<StackFrame> mStack;

   FunctionDecl * mFree;				/// Declartions to the built-in functions
   FunctionDecl * mMalloc;
   FunctionDecl * mInput;
   FunctionDecl * mOutput;

   FunctionDecl * mEntry;
public:
   /// Get the declartions to the built-in functions
   Environment() : mStack(), mFree(NULL), mMalloc(NULL), mInput(NULL), mOutput(NULL), mEntry(NULL) {
   }


   /// Initialize the Environment
   void init(TranslationUnitDecl * unit) {
	   for (TranslationUnitDecl::decl_iterator i = unit->decls_begin(), e = unit->decls_end(); i != e; ++ i) {
		   if (FunctionDecl * fdecl = dyn_cast<FunctionDecl>(*i) ) {
			   // 获取源代码中的函数声明的引用，比如 extern void PRINT(int);
			   if (fdecl->getName().equals("FREE")) mFree = fdecl;
			   else if (fdecl->getName().equals("MALLOC")) mMalloc = fdecl;
			   else if (fdecl->getName().equals("GET")) mInput = fdecl;
			   else if (fdecl->getName().equals("PRINT")) mOutput = fdecl;
			   else if (fdecl->getName().equals("main")) mEntry = fdecl;
		   }
	   }
	   mStack.push_back(StackFrame()); // 可以看做是入口的栈帧？
   }

   FunctionDecl * getEntry() {
	   return mEntry;
   }

   /// 供外部调用
   int getExprValue(Expr *expr) {
	   return mStack.back().getStmtVal(expr);
   }
   
   /// 把 IntegerLiteral 和 CharacterLiteral 这类常量也保存到栈帧
   void literal(Expr *expr) {
	   if (IntegerLiteral *literal = dyn_cast<IntegerLiteral>(expr)) {
		   // clang/AST/Expr.h: class APIIntStorage
		   mStack.back().bindStmt(expr, literal->getValue().getSExtValue());
	   } else if (CharacterLiteral *literal = dyn_cast<CharacterLiteral>(expr)) {
		   // 这块尚未验证正确性
		   mStack.back().bindStmt(expr, literal->getValue());
	   }
   }

   void binop(BinaryOperator *bop) {

	   typedef BinaryOperatorKind Opcode;
	   Expr * left = bop->getLHS(); // Left Hand Side
	   Expr * right = bop->getRHS();
	   int result = 0; // 保存当前二元表达式的计算结果

	   // 赋值运算：=, *=, /=, %=, +=, -=, ...
	   // 算数和逻辑运算：+, -, *, /, %, <<, >>, &, ^, |

	   if (bop->isAssignmentOp()) {
		   /// TODO: right 是数组元素的情况
		   /// TODO: 是否要考虑诸如 +=, *= /=, -=, &=, |= 之类的赋值操作？
		   int val = mStack.back().getStmtVal(right);
		   mStack.back().bindStmt(left, val); // 右值赋给左值
		   if (DeclRefExpr * declexpr = dyn_cast<DeclRefExpr>(left)) {
			   Decl * decl = declexpr->getFoundDecl();
			   mStack.back().bindDecl(decl, val);
		   }
		   result = val;
	   } else {
		   // 比较操作、算数运算和逻辑运算
		   // Opcodes 在 clang/AST/OperationKinds.def 中定义，
	   	   // 而 clang/AST/OperationKinds.h 中定义了转换规则（即在定义中添加对应的BO_、UO_ 等）
		   Opcode opc = bop->getOpcode();
		   int leftValue = mStack.back().getStmtVal(left);
		   int rightValue = mStack.back().getStmtVal(right);
		   
		   switch (opc) {
		   default:
		      llvm::errs() << "Unhandled binary operator.";
		   case BO_Add:	result = leftValue +  rightValue; break;
		   case BO_Sub: result = leftValue -  rightValue; break;
		   case BO_Mul: result = leftValue *  rightValue; break;
		   case BO_Div: result = leftValue /  rightValue; break;
		   case BO_EQ:  result = leftValue == rightValue; break;
		   case BO_NE:  result = leftValue != rightValue; break;
		   case BO_LT:  result = leftValue <  rightValue; break;
		   case BO_GT:  result = leftValue >  rightValue; break;
		   case BO_LE:  result = leftValue <= rightValue; break;
		   case BO_GE:  result = leftValue >= rightValue; break;
		   }
	   }

	   // 保存此二元表达式的值到栈帧
	   mStack.back().bindStmt(bop, result);

   }

   void unaryop(UnaryOperator *uop) {

	   typedef UnaryOperatorKind Opcode;
	   int result = 0;

	   // 算数运算：+, -, ~, !
	   // 自增自减：++, --（分前缀和后缀）
	   // 地址操作：&, *

	   if (uop->isArithmeticOp()) {
		   Opcode opc = uop->getOpcode();
		   int value = mStack.back().getStmtVal(uop->getSubExpr());

		   switch (opc) {
		   default:
		      llvm::errs() << "Unhandled unary operator.";
		   case UO_Plus:  result = value; break;
		   case UO_Minus: result = -value; break;
		   case UO_Not:   result = ~value; break;
		   case UO_LNot:  result = !value; break;
		   }
	   }

	   // 保存此一元表达式的值到栈帧
	   mStack.back().bindStmt(uop, result);
   }

   void decl(DeclStmt * declstmt) {
	   for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end();
			   it != ie; ++ it) {
		   Decl * decl = *it; // 不需要用 dyn_cast 因为一定会成功
		   if (VarDecl * vardecl = dyn_cast<VarDecl>(decl)) {
			   // 支持 int a = 10; 这样的声明
			   if (vardecl->hasInit()) {
				   mStack.back().bindDecl(vardecl, mStack.back().getStmtVal(vardecl->getInit()));
			   } else {
				   mStack.back().bindDecl(vardecl, 0); // 新定义的变量初始化为 0
			   }
		   }
	   }
   }

   /// 为 DeclRefExpr 复制一份对应 DeclStmt 的值，以便直接引用
   void declref(DeclRefExpr * declref) {
	   mStack.back().setPC(declref);
	   if (declref->getType()->isIntegerType()) {
		   Decl* decl = declref->getFoundDecl();

		   int val = mStack.back().getDeclVal(decl);
		   mStack.back().bindStmt(declref, val);
	   }
   }

   void cast(CastExpr * castexpr) {
	   mStack.back().setPC(castexpr);
	   if (castexpr->getType()->isIntegerType()) {
		   Expr * expr = castexpr->getSubExpr();
		   int val = mStack.back().getStmtVal(expr); // 这里就可能用到 DeclRefExpr 的值
		   mStack.back().bindStmt(castexpr, val );
	   }
   }

   /// 将返回值保存到栈帧
   void retstmt(Expr * retexpr) {
	   mStack.back().setReturnValue(mStack.back().getStmtVal(retexpr));
   }

   /// 创建新栈帧以及进行参数绑定
   void enterfunc(CallExpr * callexpr) {
	   FunctionDecl *callee = callexpr->getDirectCallee();
	   int paramCount = callee->getNumParams();
	   assert(paramCount == callexpr->getNumArgs());

	   StackFrame newFrame = StackFrame();
	   
	   for (int i = 0; i < paramCount; i++) {
		   newFrame.bindDecl(
			   callee->getParamDecl(i),
			   mStack.back().getStmtVal(callexpr->getArg(i))
		   );
	   }

	   mStack.push_back(newFrame);
   }

   /// 弹出栈帧以及进行返回值绑定
   void exitfunc(CallExpr * callexpr) {
	   int returnValue = mStack.back().getReturnValue();
	   mStack.pop_back();
	   mStack.back().bindStmt(callexpr, returnValue);
   }

   /// 返回值表示是否为内建函数
   bool builtinfunc(CallExpr * callexpr) {
	   mStack.back().setPC(callexpr); // PC有啥用？
	   int val = 0;
	   FunctionDecl * callee = callexpr->getDirectCallee();
	   if (callee == mInput) {
		  llvm::errs() << "Please Input an Integer Value : ";
		  scanf("%d", &val);

		  mStack.back().bindStmt(callexpr, val);
		  return true;
	   } else if (callee == mOutput) {
		   /// TODO: 测试输出字符串常量的情况，比如 PRINT("hello")
		   Expr * decl = callexpr->getArg(0);
		   val = mStack.back().getStmtVal(decl);
		   llvm::errs() << val;
		   return true;
	   } else {
		   /// You could add your code here for Function call Return
		   return false;
	   }
   }
};


