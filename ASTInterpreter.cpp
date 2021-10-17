//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//

// 可以参考 https://clang.llvm.org/docs/RAVFrontendAction.html 去理解这段代码

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

#include "Environment.h"

class InterpreterVisitor : 
   public EvaluatedExprVisitor<InterpreterVisitor> {
public:
   explicit InterpreterVisitor(const ASTContext &context, Environment * env)
   : EvaluatedExprVisitor(context), mEnv(env) {}
   virtual ~InterpreterVisitor() {}

   virtual void VisitIntegerLiteral (IntegerLiteral * literal) {
      mEnv->literal(literal);
   }
   virtual void VisitBinaryOperator (BinaryOperator * bop) {
	   VisitStmt(bop);
	   mEnv->binop(bop);
   }
   virtual void VisitUnaryOperator (UnaryOperator * uop) {
      VisitStmt(uop);
      mEnv->unaryop(uop);
   }
   virtual void VisitDeclRefExpr(DeclRefExpr * expr) {
	   VisitStmt(expr);
	   mEnv->declref(expr);
   }
   virtual void VisitCastExpr(CastExpr * expr) {
	   VisitStmt(expr);
	   mEnv->cast(expr);
   }
   virtual void VisitCallExpr(CallExpr * call) {
	   VisitStmt(call); // 主要作用是计算函数参数

      // 内建函数不需要进行后续的处理
	   if (mEnv->builtinfunc(call)) {
         return;
      }

      // 创建新栈帧并进行参数绑定
      mEnv->enterfunc(call);

      // 遍历执行函数体
      VisitStmt(call->getDirectCallee()->getBody());

      // 弹出栈帧并进行返回值绑定
      mEnv->exitfunc(call);
   }
   virtual void VisitReturnStmt(ReturnStmt * ret) {
      // 计算返回值，但不在此处进行返回操作，因为有的函数可能不含有 ReturnStmt.

      /// TODO: 考虑 main 函数返回的特殊情况:FunctionDecl isMain()
      /// TODO: 考虑没有返回语句的情况

      VisitStmt(ret);

      // clang/AST/Stmt.h: class ReturnStmt
      mEnv->retstmt(ret->getRetValue());
   }
   virtual void VisitDeclStmt(DeclStmt * declstmt) {
      VisitStmt(declstmt);
	   mEnv->decl(declstmt);
   }
   virtual void VisitIfStmt(IfStmt * ifstmt) {
      // clang/AST/Stmt.h: class IfStmt

      Expr * cond = ifstmt->getCond();

      // 此处不能用 VisitStmt() 因为它只会取出参数的所有子节点进行遍历而忽略当前节点本身
      // 比如对于一个 BinaryOperator，使用 VisitStmt() 会跳过 VisitBinaryOperator() 的执行
      // 语句的 body 部分也可能是一个简单的 BinaryOperator，所以也必须用 Visit()
      // 详见：clang/AST/EvaluatedExprVisitor.h: void VisitStmt(PTR(Stmt) S)
      Visit(cond);

      // 根据 cond 判断的结果只去 Visit 需要执行的子树
      if (mEnv->getExprValue(cond)) {
         Visit(ifstmt->getThen());
      } else {
         // 需要手动处理没有 Else 分支的情况
         if (Stmt * elseStmt = ifstmt->getElse()) {
            Visit(elseStmt);
         }
      }
   }
   virtual void VisitWhileStmt(WhileStmt * whilestmt) {
      // clang/AST/Stmt.h: class WhileStmt

      Expr * cond = whilestmt->getCond();
      Stmt * body = whilestmt->getBody();

      Visit(cond);

      // 每次循环都要重新 evaluate 一下 condition 的值，以更新 StackFrame 中保存的结果
      while (mEnv->getExprValue(cond)) {
         Visit(body);
         Visit(cond);
      }
   }
   virtual void VisitForStmt(ForStmt * forstmt) {
      // clang/AST/Stmt.h: class ForStmt

      Stmt * init = forstmt->getInit();
      Expr * cond = forstmt->getCond();
      Expr * inc = forstmt->getInc();
      Stmt * body = forstmt->getBody();

      Visit(init);
      Visit(cond);

      // 每次循环都要重新 evaluate 一下 condition 的值，以更新 StackFrame 中保存的结果
      while (mEnv->getExprValue(cond)) {
         Visit(body);
         Visit(inc);
         Visit(cond);
      }
   }
private:
   Environment * mEnv;
};

class InterpreterConsumer : public ASTConsumer {
public:
   explicit InterpreterConsumer(const ASTContext& context) : mEnv(),
   	   mVisitor(context, &mEnv) {
   }
   virtual ~InterpreterConsumer() {}

   virtual void HandleTranslationUnit(clang::ASTContext &Context) {
	   TranslationUnitDecl * decl = Context.getTranslationUnitDecl();

      /// 遍历全局变量声明以计算出它们的值，这些值保存在临时的初始栈帧中
      /// TODO: 跟 mEnv->init() 函数里的循环有些重复，可以考虑优化一下
      for (TranslationUnitDecl::decl_iterator i = decl->decls_begin(), e = decl->decls_end(); i != e; ++ i) {
         if (VarDecl *vdecl = dyn_cast<VarDecl>(*i)) {
            if (vdecl->hasInit()) {
               mVisitor.Visit(vdecl->getInit());
            }
         }
      }

	   mEnv.init(decl);

	   FunctionDecl * entry = mEnv.getEntry();
	   mVisitor.VisitStmt(entry->getBody());
  }
private:
   Environment mEnv;
   InterpreterVisitor mVisitor;
};

class InterpreterClassAction : public ASTFrontendAction {
public: 
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
    return std::unique_ptr<clang::ASTConsumer>(
        new InterpreterConsumer(Compiler.getASTContext()));
  }
};

/// Usage: ./ast-interpreter "$(cat ../tests/test00.c)"
int main (int argc, char ** argv) {
   if (argc > 1) {
      clang::tooling::runToolOnCode(std::unique_ptr<clang::FrontendAction>(new InterpreterClassAction), argv[1]);
   }
}
