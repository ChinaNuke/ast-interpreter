//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool
//--------------===//
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
  /// Which are either integer or addresses (also represented using an Integer
  /// value)

  // 区别：Decl 是不能被 Visit 遍历的，而 Stmt 是可以被遍历的。
  // 而对于指针类型（数组、指针），在大部分情况下我们是使用其地址所指向的值的，对于要使用
  // 地址的情况，我们在获取时可以明确知道我们要用地址，所以在这里单独增加一个
  // map 来存储 这类地址。
  std::map<Decl *, int64_t> mVars;
  std::map<Stmt *, int64_t> mExprs;
  std::map<Stmt *, int64_t *> mPtrs;
  /// The current stmt
  int64_t returnValue; // 保存当前栈帧的返回值，只考虑整数
public:
  StackFrame() : mVars(), mExprs() {}

  void bindDecl(Decl *decl, int64_t val) { mVars[decl] = val; }

  bool hasDecl(Decl *decl) { return (mVars.find(decl) != mVars.end()); }

  int64_t getDeclVal(Decl *decl) {
    assert(mVars.find(decl) != mVars.end());
    return mVars.find(decl)->second;
  }

  void bindStmt(Stmt *stmt, int64_t val) { mExprs[stmt] = val; }

  bool hasStmt(Stmt *stmt) { return (mExprs.find(stmt) != mExprs.end()); }

  int64_t getStmtVal(Stmt *stmt) {
#ifndef DEBUG
    assert(mExprs.find(stmt) != mExprs.end());
#else
    if (mExprs.find(stmt) == mExprs.end()) {
      llvm::errs() << "[DEBUG] Can not find statement: \n";
      stmt->dump();
      assert(false);
    }
#endif
    return mExprs[stmt];
  }

  void bindPtr(Stmt *stmt, int64_t *val) { mPtrs[stmt] = val; }

  int64_t *getPtr(Stmt *stmt) {
    assert(mPtrs.find(stmt) != mPtrs.end());
    return mPtrs[stmt];
  }

  void setReturnValue(int64_t value) { returnValue = value; }

  int64_t getReturnValue() { return returnValue; }
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

  FunctionDecl *mFree; /// Declartions to the built-in functions
  FunctionDecl *mMalloc;
  FunctionDecl *mInput;
  FunctionDecl *mOutput;
  FunctionDecl *mEntry;

  std::map<Decl *, int64_t> gVars; // 全局变量

public:
  /// Get the declartions to the built-in functions
  Environment()
      : mStack(), mFree(NULL), mMalloc(NULL), mInput(NULL), mOutput(NULL),
        mEntry(NULL) {
    mStack.push_back(StackFrame()); // 初始栈帧，用于临时存储计算的全局变量值
  }

  /// Initialize the Environment
  void init(TranslationUnitDecl *unit) {
    for (TranslationUnitDecl::decl_iterator i = unit->decls_begin(),
                                            e = unit->decls_end();
         i != e; ++i) {
      if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(*i)) {
        // 获取源代码中的函数声明的引用，比如 extern void PRINT(int);
        if (fdecl->getName().equals("FREE"))
          mFree = fdecl;
        else if (fdecl->getName().equals("MALLOC"))
          mMalloc = fdecl;
        else if (fdecl->getName().equals("GET"))
          mInput = fdecl;
        else if (fdecl->getName().equals("PRINT"))
          mOutput = fdecl;
        else if (fdecl->getName().equals("main"))
          mEntry = fdecl;
      } else if (VarDecl *vdecl = dyn_cast<VarDecl>(*i)) {
        // 保存全局变量
        Stmt *initStmt = vdecl->getInit();
        if (mStack.back().hasStmt(initStmt)) {
          gVars[vdecl] = mStack.back().getStmtVal(initStmt);
        } else {
          gVars[vdecl] = 0; // 未初始化的全局变量默认为 0
        }
      }
    }
    mStack.pop_back(); // 清除初始的临时栈帧，后面不会再用到
    mStack.push_back(StackFrame()); // 入口函数 main 的栈帧
  }

  FunctionDecl *getEntry() { return mEntry; }

  /// 供外部调用
  int64_t getExprValue(Expr *expr) { return mStack.back().getStmtVal(expr); }

  /// 先考虑单纯的数组
  void array(ArraySubscriptExpr *arraysubscript) {
    // clang/AST/Expr.h: class ArraySubscriptExpr
    // getBase() 获得的就是一个指向数组声明的 DeclRefExpr，getIdx()
    // 获得的是索引， 可能是一个 IntegerLiteral 或者 DeclRefExpr 之类的对象。
    Expr *base = arraysubscript->getBase();
    Expr *index = arraysubscript->getIdx();

    // 假定数组元素都声明为 int64 类型，暂不考虑其他类型的数组
    int64_t *basePtr = (int64_t *)mStack.back().getStmtVal(base);
    int64_t indexVal = mStack.back().getStmtVal(index);

    mStack.back().bindPtr(arraysubscript, basePtr + indexVal);
    mStack.back().bindStmt(arraysubscript, *(basePtr + indexVal));
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

  void ueot(UnaryExprOrTypeTraitExpr *ueotexpr) {
    // 比较草率的实现，后面可以再完善
    UnaryExprOrTypeTrait kind = ueotexpr->getKind();
    int64_t result = 0;
    switch (kind) {
    default:
      llvm::errs() << "Unhandled UEOT.";
      break;
    case UETT_SizeOf:
      result = 8; // int64_t 和 int64_t * 两种类型
      break;
    }
    mStack.back().bindStmt(ueotexpr, result);
  }

  void paren(ParenExpr *parenexpr) {
    mStack.back().bindStmt(parenexpr,
                           mStack.back().getStmtVal(parenexpr->getSubExpr()));
  }

  void binop(BinaryOperator *bop) {

    typedef BinaryOperatorKind Opcode;
    Expr *left = bop->getLHS(); // Left Hand Side
    Expr *right = bop->getRHS();
    int64_t result = 0; // 保存当前二元表达式的计算结果

    // 赋值运算：=, *=, /=, %=, +=, -=, ...
    // 算数和逻辑运算：+, -, *, /, %, <<, >>, &, ^, |

    int64_t rightValue = mStack.back().getStmtVal(right);

    if (bop->isAssignmentOp()) {

      /// TODO: 是否要考虑诸如 +=, *= /=, -=, &=, |= 之类的赋值操作？

      // 这一块实际上可以理解为左值是不同类型时的结果保存操作
      if (DeclRefExpr *declref = dyn_cast<DeclRefExpr>(left)) {
        // 不需要把 DeclRefExpr 自身的值保存到栈帧，因为在 VisitDeclRefExpr 时
        // 会直接去查找 FoundDecl。
        Decl *decl = declref->getFoundDecl();
        mStack.back().bindDecl(decl, rightValue);
      } else if (isa<ArraySubscriptExpr>(left)) {
        int64_t *ptr = mStack.back().getPtr(left);
        *ptr = rightValue;
      } else if (UnaryOperator *uop = dyn_cast<UnaryOperator>(left)) {
        // 暂时没想到什么优雅的写法合并到上面去
        assert(uop->getOpcode() == UO_Deref);
        int64_t *ptr = mStack.back().getPtr(left);
        *ptr = rightValue;
#ifndef DEBUG
      }
#else
      } else {
        llvm::errs() << "Can not find assignment operation: \n";
        bop->dump();
      }
#endif
      result = rightValue;

    } else {

      // 比较操作、算数运算和逻辑运算
      // Opcodes 在 clang/AST/OperationKinds.def 中定义，
      // 而 clang/AST/OperationKinds.h
      // 中定义了转换规则（即在定义中添加对应的BO_、UO_ 等）
      Opcode opc = bop->getOpcode();

      int64_t leftValue = mStack.back().getStmtVal(left);

      // *(a + 2)
      if (left->getType()->isPointerType() &&
          right->getType()->isIntegerType()) {
        assert(opc == BO_Add || opc == BO_Sub);
        rightValue *= sizeof(int64_t);
      } else if (left->getType()->isIntegerType() &&
                 right->getType()->isPointerType()) {
        assert(opc == BO_Add || opc == BO_Sub);
        leftValue *= sizeof(int64_t);
      }

      switch (opc) {
      default:
        llvm::errs() << "Unhandled binary operator.";
      case BO_Add:
        result = leftValue + rightValue;
        break;
      case BO_Sub:
        result = leftValue - rightValue;
        break;
      case BO_Mul:
        result = leftValue * rightValue;
        break;
      case BO_Div:
        result = leftValue / rightValue;
        break;
      case BO_EQ:
        result = leftValue == rightValue;
        break;
      case BO_NE:
        result = leftValue != rightValue;
        break;
      case BO_LT:
        result = leftValue < rightValue;
        break;
      case BO_GT:
        result = leftValue > rightValue;
        break;
      case BO_LE:
        result = leftValue <= rightValue;
        break;
      case BO_GE:
        result = leftValue >= rightValue;
        break;
      }
    }

    // 保存此二元表达式的值到栈帧
    mStack.back().bindStmt(bop, result);
  }

  void unaryop(UnaryOperator *uop) {

    typedef UnaryOperatorKind Opcode;
    int64_t result = 0;

    // 算数运算：+, -, ~, !
    // 自增自减：++, --（分前缀和后缀）
    // 地址操作：&, *

    Opcode opc = uop->getOpcode();
    int64_t value = mStack.back().getStmtVal(uop->getSubExpr());

    switch (opc) {
    default:
      llvm::errs() << "Unhandled unary operator.";
    case UO_Plus:
      result = value;
      break;
    case UO_Minus:
      result = -value;
      break;
    case UO_Not:
      result = ~value;
      break;
    case UO_LNot:
      result = !value;
      break;
    case UO_Deref:
      // Deref 不是 ArithmeticOp !
      mStack.back().bindPtr(uop, (int64_t *)value);
      result = *(int64_t *)value;
      break;
    }

    // 保存此一元表达式的值到栈帧
    mStack.back().bindStmt(uop, result);
  }

  void decl(DeclStmt *declstmt) {
    for (DeclStmt::decl_iterator it = declstmt->decl_begin(),
                                 ie = declstmt->decl_end();
         it != ie; ++it) {
      Decl *decl = *it; // 不需要用 dyn_cast 因为一定会成功
      if (VarDecl *vardecl = dyn_cast<VarDecl>(decl)) {
        // 支持 int a = 10; 这样的简单声明和 int a[3]; 这样的数组声明
        QualType type = vardecl->getType();

        if (type->isIntegerType() || type->isPointerType()) {
          // int a; int a = 1; int *a; int *a = MALLOC(10); 四种情况
          if (vardecl->hasInit()) {
            mStack.back().bindDecl(
                vardecl, mStack.back().getStmtVal(vardecl->getInit()));
          } else {
            mStack.back().bindDecl(vardecl, 0); // 新定义的变量初始化为 0
          }
        } else if (type->isArrayType()) {
          // 暂时不考虑带初始化的数组声明的情况
          const ConstantArrayType *array =
              dyn_cast<ConstantArrayType>(type.getTypePtr());
          int64_t size = array->getSize().getSExtValue();
          int64_t *arrayStorage = new int64_t[size];
          for (int64_t i = 0; i < size; i++) {
            arrayStorage[i] = 0;
          }
          mStack.back().bindDecl(vardecl, (int64_t)arrayStorage);
#ifndef DEBUG
        }
#else
        } else {
          llvm::errs() << "Unhandled decl type: \n";
          declstmt->dump();
          type->dump();
        }
#endif
      }
    }
  }

  /// 为 DeclRefExpr 复制一份对应 DeclStmt 的值，以便直接引用
  void declref(DeclRefExpr *declref) {
    mStack.back().setPC(declref);
    QualType type = declref->getType();
    if (type->isIntegerType() || type->isArrayType() || type->isPointerType()) {
      Decl *decl = declref->getFoundDecl();
      int64_t val;
      // 优先从当前栈帧中查找，找不到再查找全局变量
      if (mStack.back().hasDecl(decl)) {
        val = mStack.back().getDeclVal(decl);
      } else {
        assert(gVars.find(decl) != gVars.end());
        val = gVars[decl];
      }
      mStack.back().bindStmt(declref, val);
#ifndef DEBUG
    }
#else
    } else if (!type->isFunctionProtoType()) {
      llvm::errs() << "[DEBUG] Unhandled declref type: \n";
      declref->dump();
      type->dump();
    }
#endif
  }

  void cast(CastExpr *castexpr) {
    mStack.back().setPC(castexpr);
    QualType type = castexpr->getType();

    // 这里的 PointerType 包含了数组引用和函数调用的两种情况，
    // 但实际上不需要处理函数调用的情况，因为在 enterfunc 里直接获取 callee
    // 定义了。
    if (type->isIntegerType() ||
        (type->isPointerType() && !type->isFunctionPointerType())) {
      Expr *expr = castexpr->getSubExpr();
      int64_t val = mStack.back().getStmtVal(expr);
      mStack.back().bindStmt(castexpr, val);
#ifndef DEBUG
    }
#else
    } else if (!type->isFunctionPointerType()) {
      llvm::errs() << "[DEBUG] Unhandled cast type: \n";
      castexpr->dump();
      type->dump();
    }
#endif
  }

  /// 将返回值保存到栈帧
  void retstmt(Expr *retexpr) {
    int64_t retval = mStack.back().getStmtVal(retexpr);
    mStack.back().setReturnValue(retval);
  }

  /// 创建新栈帧以及进行参数绑定
  void enterfunc(CallExpr *callexpr) {
    FunctionDecl *callee = callexpr->getDirectCallee();
    int paramCount = callee->getNumParams();
    assert(paramCount == callexpr->getNumArgs());

    StackFrame newFrame = StackFrame();

    for (int i = 0; i < paramCount; i++) {
      newFrame.bindDecl(callee->getParamDecl(i),
                        mStack.back().getStmtVal(callexpr->getArg(i)));
    }

    mStack.push_back(newFrame);
  }

  /// 弹出栈帧以及进行返回值绑定
  void exitfunc(CallExpr *callexpr) {
    int64_t returnValue = mStack.back().getReturnValue();
    mStack.pop_back();
    mStack.back().bindStmt(callexpr, returnValue);
  }

  /// 返回值表示是否为内建函数
  bool builtinfunc(CallExpr *callexpr) {
    mStack.back().setPC(callexpr); // PC有啥用？
    int64_t val = 0;
    FunctionDecl *callee = callexpr->getDirectCallee();
    if (callee == mInput) {
      llvm::errs() << "Please Input an Integer Value : ";
      scanf("%ld", &val);
      mStack.back().bindStmt(callexpr, val);
      return true;
    } else if (callee == mOutput) {
      /// TODO: 测试输出字符串常量的情况，比如 PRINT("hello")
      Expr *decl = callexpr->getArg(0);
      val = mStack.back().getStmtVal(decl);
      llvm::errs() << val;
      mStack.back().bindStmt(callexpr, 0);
      return true;
    } else if (callee == mMalloc) {
      int64_t size = mStack.back().getStmtVal(callexpr->getArg(0));
      mStack.back().bindStmt(callexpr, (int64_t)malloc(size));
      return true;
    } else if (callee == mFree) {
      int64_t *ptr = (int64_t *)mStack.back().getStmtVal(callexpr->getArg(0));
      free(ptr);
      return true;
    } else {
      /// You could add your code here for Function call Return
      return false;
    }
  }
};
