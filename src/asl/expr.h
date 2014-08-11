/*
 A C++ interface to AMPL expressions.

 Copyright (C) 2012 AMPL Optimization Inc

 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted,
 provided that the above copyright notice appear in all copies and that
 both that the copyright notice and this permission notice and warranty
 disclaimer appear in supporting documentation.

 The author and AMPL Optimization Inc disclaim all warranties with
 regard to this software, including all implied warranties of
 merchantability and fitness.  In no event shall the author be liable
 for any special, indirect or consequential damages or any damages
 whatsoever resulting from loss of use, data or profits, whether in an
 action of contract, negligence or other tortious action, arising out
 of or in connection with the use or performance of this software.

 Author: Victor Zverovich
 */

#ifndef MP_EXPR_H_
#define MP_EXPR_H_

#ifdef HAVE_UNORDERED_MAP
# include <unordered_map>
#endif

#include <cassert>
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "solvers/nlp.h"
}

// Undefine ASL macros because they often clash with names used in solver
// libraries.
#undef solve_code
#undef var_name
#undef con_name
#undef filename
#undef ampl_vbtol
#undef strtod
#ifndef ASL_PRESERVE_DEFINES
# undef Char
# undef A_colstarts
# undef A_rownos
# undef A_vals
# undef Cgrad
# undef Fortran
# undef LUrhs
# undef LUv
# undef Lastx
# undef Ograd
# undef Urhsx
# undef Uvx
# undef X0
# undef adjoints
# undef adjoints_nv1
# undef amax
# undef ampl_options
# undef amplflag
# undef archan
# undef awchan
# undef binary_nl
# undef c_cexp1st
# undef c_vars
# undef co_index
# undef comb
# undef combc
# undef comc
# undef comc1
# undef como
# undef como1
# undef cv_index
# undef cvar
# undef err_jmp
# undef err_jmp1
# undef fhash
# undef funcs
# undef funcsfirst
# undef funcslast
# undef havepi0
# undef havex0
# undef lnc
# undef maxcolnamelen
# undef maxrownamelen
# undef n_cc
# undef n_con
# undef n_conjac
# undef n_obj
# undef n_var
# undef nbv
# undef nclcon
# undef ncom0
# undef ncom1
# undef nderps
# undef need_nl
# undef n_eqn
# undef nfunc
# undef niv
# undef nlc
# undef nlcc
# undef nlnc
# undef nlo
# undef n_lcon
# undef nlogv
# undef nlvb
# undef nlvbi
# undef nlvc
# undef nlvci
# undef nlvo
# undef nlvoi
# undef nranges
# undef nwv
# undef nzc
# undef nzjac
# undef nzo
# undef o_cexp1st
# undef o_vars
# undef obj_no
# undef objtype
# undef pi0
# undef plterms
# undef real
# undef return_nofile
# undef size_expr_n
# undef skip_int_derivs
# undef sputinfo
# undef stub_end
# undef want_deriv
# undef want_xpi0
# undef x0kind
# undef x0len
# undef xscanf
# undef zaC
# undef zac
# undef zao
# undef zerograds
#endif  // ASL_PRESERVE_DEFINES

#include "mp/error.h"
#include "mp/problem-base.h"

namespace mp {
class Expr;
class NumericExpr;
class LogicalExpr;
}

#ifdef HAVE_UNORDERED_MAP
namespace std {
template <>
struct hash<mp::NumericExpr> {
  std::size_t operator()(ampl::NumericExpr e) const;
};
template <>
struct hash<mp::LogicalExpr> {
  std::size_t operator()(ampl::LogicalExpr e) const;
};
}
#endif

namespace mp {

namespace internal {

class ASLBuilder;

// Returns true if the non-null expression e is of type ExprT.
template <typename ExprT>
bool Is(Expr e);
}

// Specialize Is<ExprT> for the class ExprClass corresponding to a single
// expression kind with the specified operation code.
#define AMPL_SPECIALIZE_IS(ExprClass, code) \
namespace internal { \
template <> \
inline bool Is<ExprClass>(Expr e) { \
  return e.opcode() == code; \
} \
}

// An expression.
// An Expr object represents a reference to an expression so
// it is cheap to construct and pass by value. A type safe way to
// process expressions of different types is by using ExprVisitor.
class Expr {
 private:
  // Returns the kind of this expression.
  expr::Kind kind() const { return expr::kind(opcode()); }

  void True() const {}
  typedef void (Expr::*SafeBool)() const;

  template <typename Impl, typename Result, typename LResult>
  friend class ExprVisitor;

  template <typename Impl, typename Result, typename LResult>
  friend class ExprConverter;

  friend class ExprBuilder;
  friend class internal::ASLBuilder;
  friend class Problem;

  template <typename ExprT>
  static ExprT Create(Expr e) {
    assert(!e || internal::Is<ExprT>(e));
    ExprT expr;
    expr.expr_ = e.expr_;
    return expr;
  }

  // Constructs an Expr object representing a reference to an AMPL
  // expression e. Only a minimal check is performed when assertions are
  // enabled to make sure that the opcode is within the valid range.
  explicit Expr(::expr *e) : expr_(e) {
    assert(!expr_ || (kind() >= expr::EXPR_START && kind() <= expr::EXPR_END));
  }

 protected:
  ::expr *expr_;

  // Creates an expression object from a raw expr pointer.
  // For safety reason expression classes don't provide constructors
  // taking raw pointers and this method should be used instead.
  template <typename ExprT>
  static ExprT Create(::expr *e) { return Create<ExprT>(Expr(e)); }

  // An expression proxy used for implementing operator-> in iterators.
  template <typename ExprT>
  class Proxy {
   private:
    ExprT expr_;

   public:
    explicit Proxy(::expr *e) : expr_(Create<ExprT>(e)) {}

    const ExprT *operator->() const { return &expr_; }
  };

  // An expression array iterator.
  template <typename ExprT>
  class ArrayIterator :
    public std::iterator<std::forward_iterator_tag, ExprT> {
   private:
    ::expr *const *ptr_;

   public:
    explicit ArrayIterator(::expr *const *p = 0) : ptr_(p) {}

    ExprT operator*() const { return Create<ExprT>(*ptr_); }

    Proxy<ExprT> operator->() const {
      return Proxy<ExprT>(*ptr_);
    }

    ArrayIterator &operator++() {
      ++ptr_;
      return *this;
    }

    ArrayIterator operator++(int ) {
      ArrayIterator it(*this);
      ++ptr_;
      return it;
    }

    bool operator==(ArrayIterator other) const { return ptr_ == other.ptr_; }
    bool operator!=(ArrayIterator other) const { return ptr_ != other.ptr_; }
  };

 public:
  // Constructs an Expr object representing a null reference to an AMPL
  // expression. The only operation permitted for such expression is
  // copying, assignment and check whether it is null using operator SafeBool.
  Expr() : expr_() {}

  // Returns a value convertible to bool that can be used in conditions but not
  // in comparisons and evaluates to "true" if this expression is not null
  // and "false" otherwise.
  // Example:
  //   void foo(Expr e) {
  //     if (e) {
  //       // Do something if e is not null.
  //     }
  //   }
  operator SafeBool() const { return expr_ ? &Expr::True : 0; }

  // Returns the operation code (opcode) of this expression.
  // The opcodes are defined in opcode.hd.
  int opcode() const {
    return static_cast<int>(reinterpret_cast<std::size_t>(expr_->op));
  }

  // Returns the function name or operator for this expression as a
  // string. Expressions with different opcodes can have identical
  // strings. For example, OPPOW, OP1POW and OPCPOW all use the
  // same operator "^".
  const char *opstr() const {
    assert(opcode() >= 0 && opcode() < N_OPS);
    return expr::Info::INFO[opcode()].str;
  }

  int precedence() const {
    assert(opcode() >= 0 && opcode() < N_OPS);
    return expr::Info::INFO[opcode()].precedence;
  }

  bool operator==(Expr other) const { return expr_ == other.expr_; }
  bool operator!=(Expr other) const { return expr_ != other.expr_; }

  template <typename ExprT>
  friend bool internal::Is(Expr e);

  template <typename ExprT>
  friend ExprT Cast(Expr e);

  // Recursively compares two expressions and returns true if they are equal.
  friend bool Equal(Expr e1, Expr e2);
};

// Casts an expression to type ExprT. Returns a null expression if the cast
// is not possible.
template <typename ExprT>
ExprT Cast(Expr e) {
  return internal::Is<ExprT>(e) ? Expr::Create<ExprT>(e) : ExprT();
}

namespace internal {
template <typename ExprT>
inline bool Is(Expr e) {
  return e.kind() == ExprT::KIND;
}

template <>
inline bool Is<Expr>(Expr e) {
  return e.kind() >= expr::EXPR_START && e.kind() <= expr::EXPR_END;
}
}

// A numeric expression.
class NumericExpr : public Expr {
 public:
  NumericExpr() {}
};

namespace internal {
template <>
inline bool Is<NumericExpr>(Expr e) {
  expr::Kind k = e.kind();
  return k >= expr::NUMERIC_START && k <= expr::NUMERIC_END;
}
}

// A logical or constraint expression.
class LogicalExpr : public Expr {
 public:
  LogicalExpr() {}
};

namespace internal {
template <>
inline bool Is<LogicalExpr>(Expr e) {
  expr::Kind k = e.kind();
  return k >= expr::LOGICAL_START && k <= expr::LOGICAL_END;
}
}

// A numeric constant.
// Examples: 42, -1.23e-4
class NumericConstant : public NumericExpr {
 public:
  NumericConstant() {}

  // Returns the value of this number.
  double value() const { return reinterpret_cast<expr_n*>(expr_)->v; }
};

AMPL_SPECIALIZE_IS(NumericConstant, OPNUM)

// A reference to a variable.
// Example: x
class Variable : public NumericExpr {
 public:
  Variable() {}

  // Returns the index of the referenced variable.
  int index() const { return expr_->a; }
};

AMPL_SPECIALIZE_IS(Variable, OPVARVAL)

template <expr::Kind K, typename Base>
class BasicUnaryExpr : public Base {
 public:
  static const expr::Kind KIND = K;

  BasicUnaryExpr() {}

  // Returns the argument of this expression.
  Base arg() const { return Expr::Create<Base>(this->expr_->L.e); }
};

// A unary numeric expression.
// Examples: -x, sin(x), where x is a variable.
typedef BasicUnaryExpr<expr::UNARY, NumericExpr> UnaryExpr;

// A binary expression.
// Base: base expression class.
// Arg: argument expression class.
template <expr::Kind K, typename Base, typename Arg = Base>
class BasicBinaryExpr : public Base {
 public:
  static const expr::Kind KIND = K;

  BasicBinaryExpr() {}

  // Returns the left-hand side (the first argument) of this expression.
  Arg lhs() const { return Expr::Create<Arg>(this->expr_->L.e); }

  // Returns the right-hand side (the second argument) of this expression.
  Arg rhs() const { return Expr::Create<Arg>(this->expr_->R.e); }
};

// A binary numeric expression.
// Examples: x / y, atan2(x, y), where x and y are variables.
typedef BasicBinaryExpr<expr::BINARY, NumericExpr> BinaryExpr;

template <typename Base>
class BasicIfExpr : public Base {
 public:
  BasicIfExpr() {}

  LogicalExpr condition() const {
    return Expr::Create<LogicalExpr>(
        reinterpret_cast<expr_if*>(this->expr_)->e);
  }

  Base true_expr() const {
    return Expr::Create<Base>(reinterpret_cast<expr_if*>(this->expr_)->T);
  }

  Base false_expr() const {
    return Expr::Create<Base>(reinterpret_cast<expr_if*>(this->expr_)->F);
  }
};

// An if-then-else expression.
// Example: if x != 0 then y else z, where x, y and z are variables.
typedef BasicIfExpr<NumericExpr> IfExpr;
AMPL_SPECIALIZE_IS(IfExpr, OPIFnl)

// A piecewise-linear expression.
// Example: <<0; -1, 1>> x, where x is a variable.
class PiecewiseLinearExpr : public NumericExpr {
 public:
  PiecewiseLinearExpr() {}

  // Returns the number of breakpoints in this term.
  int num_breakpoints() const {
    return num_slopes() - 1;
  }

  // Returns the number of slopes in this term.
  int num_slopes() const {
    assert(expr_->L.p->n >= 1);
    return expr_->L.p->n;
  }

  // Returns a breakpoint with the specified index.
  double breakpoint(int index) const {
    assert(index >= 0 && index < num_breakpoints());
    return expr_->L.p->bs[2 * index + 1];
  }

  // Returns a slope with the specified index.
  double slope(int index) const {
    assert(index >= 0 && index < num_slopes());
    return expr_->L.p->bs[2 * index];
  }

  int var_index() const {
    return reinterpret_cast<expr_v*>(expr_->R.e)->a;
  }
};

AMPL_SPECIALIZE_IS(PiecewiseLinearExpr, OPPLTERM)

class Function {
 private:
  func_info *fi_;

  friend class CallExpr;
  friend class internal::ASLBuilder;

  explicit Function(func_info *fi) : fi_(fi) {}

  void True() const {}
  typedef void (Function::*SafeBool)() const;

 public:
  Function() : fi_(0) {}

  // Returns the function name.
  const char *name() const { return fi_->name; }

  // Returns the number of arguments.
  int num_args() const { return fi_->nargs; }

  // Returns a value convertible to bool that can be used in conditions but not
  // in comparisons and evaluates to "true" if this function is not null
  // and "false" otherwise.
  // Example:
  //   void foo(Function f) {
  //     if (f) {
  //       // Do something if e is not null.
  //     }
  //   }
  operator SafeBool() const { return fi_ ? &Function::True : 0; }

  bool operator==(const Function &other) const { return fi_ == other.fi_; }
  bool operator!=(const Function &other) const { return fi_ != other.fi_; }
};

// A function call expression.
// Example: f(x), where f is a function and x is a variable.
class CallExpr : public NumericExpr {
 public:
  CallExpr() {}

  Function function() const {
    return Function(reinterpret_cast<expr_f*>(expr_)->fi);
  }

  int num_args() const { return reinterpret_cast<expr_f*>(expr_)->al->n; }

  Expr operator[](int index) {
    assert(index >= 0 && index < num_args());
    return Create<Expr>(reinterpret_cast<expr_f*>(expr_)->args[index]);
  }

  // An argument iterator.
  typedef ArrayIterator<Expr> iterator;

  iterator begin() const {
    return iterator(reinterpret_cast<expr_f*>(expr_)->args);
  }

  iterator end() const {
    return iterator(reinterpret_cast<expr_f*>(expr_)->args + num_args());
  }
};

AMPL_SPECIALIZE_IS(CallExpr, OPFUNCALL)

// A numeric expression with a variable number of arguments.
// The min and max functions always have at least one argument.
// Example: min{i in I} x[i], where I is a set and x is a variable.
class VarArgExpr : public NumericExpr {
 private:
  static const de END;

 public:
  static const expr::Kind KIND = expr::VARARG;

  VarArgExpr() {}

  // An argument iterator.
  class iterator :
    public std::iterator<std::forward_iterator_tag, NumericExpr> {
   private:
    const de *de_;

    friend class VarArgExpr;

    explicit iterator(const de *d) : de_(d) {}

   public:
    iterator() : de_(&END) {}

    NumericExpr operator*() const { return Create<NumericExpr>(de_->e); }

    Proxy<NumericExpr> operator->() const {
      return Proxy<NumericExpr>(de_->e);
    }

    iterator &operator++() {
      ++de_;
      return *this;
    }

    iterator operator++(int ) {
      iterator it(*this);
      ++de_;
      return it;
    }

    bool operator==(iterator other) const { return de_->e == other.de_->e; }
    bool operator!=(iterator other) const { return de_->e != other.de_->e; }
  };

  iterator begin() const {
    return iterator(reinterpret_cast<expr_va*>(expr_)->L.d);
  }

  iterator end() const {
    return iterator();
  }
};

template <expr::Kind>
struct ExprInfo;

template <typename BaseT = void, typename ArgT = BaseT>
struct BasicExprInfo {
  typedef BaseT Base;
  typedef ArgT Arg;
};

template <expr::Kind K>
class BasicIteratedExpr : public ExprInfo<K>::Base {
 public:
  static const expr::Kind KIND = K;

  typedef typename ExprInfo<K>::Arg Arg;

  BasicIteratedExpr() {}

  // Returns the number of arguments.
  int num_args() const {
    return static_cast<int>(this->expr_->R.ep - this->expr_->L.ep);
  }

  Arg operator[](int index) const {
    assert(index >= 0 && index < num_args());
    return Expr::Create<Arg>(this->expr_->L.ep[index]);
  }

  typedef Expr::ArrayIterator<Arg> iterator;

  iterator begin() const { return iterator(this->expr_->L.ep); }
  iterator end() const { return iterator(this->expr_->R.ep); }
};

template <>
struct ExprInfo<expr::SUM> : BasicExprInfo<NumericExpr> {};

// A sum expression.
// Example: sum{i in I} x[i], where I is a set and x is a variable.
typedef BasicIteratedExpr<expr::SUM> SumExpr;
AMPL_SPECIALIZE_IS(SumExpr, OPSUMLIST)

template <>
struct ExprInfo<expr::COUNT> : BasicExprInfo<NumericExpr, LogicalExpr> {};

// A count expression.
// Example: count{i in I} (x[i] >= 0), where I is a set and x is a variable.
typedef BasicIteratedExpr<expr::COUNT> CountExpr;
AMPL_SPECIALIZE_IS(CountExpr, OPCOUNT)

template <>
struct ExprInfo<expr::NUMBEROF> : BasicExprInfo<NumericExpr> {};

// A numberof expression.
// Example: numberof 42 in ({i in I} x[i]),
// where I is a set and x is a variable.
typedef BasicIteratedExpr<expr::NUMBEROF> NumberOfExpr;
AMPL_SPECIALIZE_IS(NumberOfExpr, OPNUMBEROF)

// A logical constant.
// Examples: 0, 1
class LogicalConstant : public LogicalExpr {
 public:
  LogicalConstant() {}

  // Returns the value of this constant.
  bool value() const { return reinterpret_cast<expr_n*>(expr_)->v != 0; }
};

AMPL_SPECIALIZE_IS(LogicalConstant, OPNUM)

// A logical NOT expression.
// Example: not a, where a is a logical expression.
typedef BasicUnaryExpr<expr::NOT, LogicalExpr> NotExpr;
AMPL_SPECIALIZE_IS(NotExpr, OPNOT)

// A binary logical expression.
// Examples: a || b, a && b, where a and b are logical expressions.
typedef BasicBinaryExpr<expr::BINARY_LOGICAL, LogicalExpr> BinaryLogicalExpr;

// A relational expression.
// Examples: x < y, x != y, where x and y are variables.
typedef BasicBinaryExpr<
    expr::RELATIONAL, LogicalExpr, NumericExpr> RelationalExpr;

// A logical count expression.
// Examples: atleast 1 (x < y, x != y), where x and y are variables.
class LogicalCountExpr : public LogicalExpr {
 public:
  static const expr::Kind KIND = expr::LOGICAL_COUNT;

  LogicalCountExpr() {}

  // Returns the left-hand side (the first argument) of this expression.
  NumericExpr lhs() const { return Create<NumericExpr>(expr_->L.e); }

  // Returns the right-hand side (the second argument) of this expression.
  CountExpr rhs() const { return Create<CountExpr>(expr_->R.e); }
};

// An implication expression.
// Example: a ==> b else c, where a, b and c are logical expressions.
typedef BasicIfExpr<LogicalExpr> ImplicationExpr;
AMPL_SPECIALIZE_IS(ImplicationExpr, OPIMPELSE)

template <>
struct ExprInfo<expr::ITERATED_LOGICAL> : BasicExprInfo<LogicalExpr> {};

// An iterated logical expression.
// Example: exists{i in I} x[i] >= 0, where I is a set and x is a variable.
typedef BasicIteratedExpr<expr::ITERATED_LOGICAL> IteratedLogicalExpr;

template <>
struct ExprInfo<expr::ALLDIFF> : BasicExprInfo<LogicalExpr, NumericExpr> {};

// An alldiff expression.
// Example: alldiff{i in I} x[i], where I is a set and x is a variable.
typedef BasicIteratedExpr<expr::ALLDIFF> AllDiffExpr;
AMPL_SPECIALIZE_IS(AllDiffExpr, OPALLDIFF)

class StringLiteral : public Expr {
 public:
  StringLiteral() {}

  const char *value() const { return reinterpret_cast<expr_h*>(expr_)->sym; }
};

AMPL_SPECIALIZE_IS(StringLiteral, OPHOL)

// Returns true iff e is a zero constant.
inline bool IsZero(NumericExpr e) {
  NumericConstant c = Cast<NumericConstant>(e);
  return c && c.value() == 0;
}

// An exception that is thrown when an ASL expression not supported
// by the solver is encountered.
class UnsupportedExprError : public Error {
 private:
  explicit UnsupportedExprError(fmt::StringRef message) : Error(message) {}

 public:
  static UnsupportedExprError CreateFromMessage(fmt::StringRef message) {
    return UnsupportedExprError(message);
  }

  static UnsupportedExprError CreateFromExprString(fmt::StringRef expr) {
    return UnsupportedExprError(
        std::string("unsupported expression: ") + expr.c_str());
  }
};

namespace internal {
std::string FormatOpCode(Expr e);
}

// An exception that is thrown when an invalid numeric expression
// is encountered.
class InvalidNumericExprError : public Error {
 public:
  explicit InvalidNumericExprError(NumericExpr e) :
    Error("invalid numeric expression: " + internal::FormatOpCode(e)) {}
};

// An exception that is thrown when an invalid logical or constraint
// expression is encountered.
class InvalidLogicalExprError : public Error {
 public:
  explicit InvalidLogicalExprError(LogicalExpr e) :
    Error("invalid logical expression: " + internal::FormatOpCode(e)) {}
};

#define AMPL_DISPATCH(call) static_cast<Impl*>(this)->call

// An expression visitor.
// To use ExprVisitor define a subclass that implements some or all of the
// Visit* methods with the same signatures as the methods in ExprVisitor,
// for example, VisitDiv(BinaryExpr).
// Specify the subclass name as the Impl template parameter. Then calling
// ExprVisitor::Visit for some expression will dispatch to a Visit* method
// specific to the expression type. For example, if the expression is
// a division then VisitDiv(BinaryExpr) method of a subclass will be called.
// If the subclass doesn't contain a method with this signature, then
// a corresponding method of ExprVisitor will be called.
//
// Example:
//  class MyExprVisitor : public ExprVisitor<MyExprVisitor, double, void> {
//   public:
//    double VisitPlus(BinaryExpr e) { return Visit(e.lhs()) + Visit(e.rhs()); }
//    double VisitConstant(NumericConstant n) { return n.value(); }
//  };
//
// ExprVisitor uses the curiously recurring template pattern:
// http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
template <typename Impl, typename Result, typename LResult = Result>
class ExprVisitor {
 public:
  Result Visit(NumericExpr e);
  LResult Visit(LogicalExpr e);

  Result VisitUnhandledNumericExpr(NumericExpr e) {
    throw UnsupportedExprError::CreateFromExprString(e.opstr());
  }

  LResult VisitUnhandledLogicalExpr(LogicalExpr e) {
    throw UnsupportedExprError::CreateFromExprString(e.opstr());
  }

  Result VisitInvalidNumericExpr(NumericExpr e) {
    throw InvalidNumericExprError(e);
  }

  LResult VisitInvalidLogicalExpr(LogicalExpr e) {
    throw InvalidLogicalExprError(e);
  }

  Result VisitNumericConstant(NumericConstant c) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(c));
  }

  Result VisitVariable(Variable v) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(v));
  }

  // Visits a unary expression or a function taking one argument.
  Result VisitUnary(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  Result VisitUnaryMinus(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitPow2(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitFloor(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitCeil(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitAbs(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitTanh(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitTan(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitSqrt(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitSinh(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitSin(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitLog10(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitLog(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitExp(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitCosh(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitCos(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitAtanh(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitAtan(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitAsinh(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitAsin(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitAcosh(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  Result VisitAcos(UnaryExpr e) {
    return AMPL_DISPATCH(VisitUnary(e));
  }

  // Visits a binary expression or a function taking two arguments.
  Result VisitBinary(BinaryExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  Result VisitPlus(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitMinus(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitMult(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitDiv(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitRem(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitPow(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitPowConstExp(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitPowConstBase(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitNumericLess(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitIntDiv(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  // Visits a function taking two arguments.
  Result VisitBinaryFunc(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinary(e));
  }

  Result VisitAtan2(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinaryFunc(e));
  }

  Result VisitPrecision(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinaryFunc(e));
  }

  Result VisitRound(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinaryFunc(e));
  }

  Result VisitTrunc(BinaryExpr e) {
    return AMPL_DISPATCH(VisitBinaryFunc(e));
  }

  Result VisitIf(IfExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  Result VisitPiecewiseLinear(PiecewiseLinearExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  Result VisitCall(CallExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  Result VisitVarArg(VarArgExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  Result VisitMin(VarArgExpr e) {
    return AMPL_DISPATCH(VisitVarArg(e));
  }

  Result VisitMax(VarArgExpr e) {
    return AMPL_DISPATCH(VisitVarArg(e));
  }

   Result VisitSum(SumExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  Result VisitCount(CountExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  Result VisitNumberOf(NumberOfExpr e) {
    return AMPL_DISPATCH(VisitUnhandledNumericExpr(e));
  }

  LResult VisitLogicalConstant(LogicalConstant c) {
    return AMPL_DISPATCH(VisitUnhandledLogicalExpr(c));
  }

  LResult VisitNot(NotExpr e) {
    return AMPL_DISPATCH(VisitUnhandledLogicalExpr(e));
  }

  LResult VisitBinaryLogical(BinaryLogicalExpr e) {
    return AMPL_DISPATCH(VisitUnhandledLogicalExpr(e));
  }

  LResult VisitOr(BinaryLogicalExpr e) {
    return AMPL_DISPATCH(VisitBinaryLogical(e));
  }

  LResult VisitAnd(BinaryLogicalExpr e) {
    return AMPL_DISPATCH(VisitBinaryLogical(e));
  }

  LResult VisitIff(BinaryLogicalExpr e) {
    return AMPL_DISPATCH(VisitBinaryLogical(e));
  }

  LResult VisitRelational(RelationalExpr e) {
    return AMPL_DISPATCH(VisitUnhandledLogicalExpr(e));
  }

  LResult VisitLess(RelationalExpr e) {
    return AMPL_DISPATCH(VisitRelational(e));
  }

  LResult VisitLessEqual(RelationalExpr e) {
    return AMPL_DISPATCH(VisitRelational(e));
  }

  LResult VisitEqual(RelationalExpr e) {
    return AMPL_DISPATCH(VisitRelational(e));
  }

  LResult VisitGreaterEqual(RelationalExpr e) {
    return AMPL_DISPATCH(VisitRelational(e));
  }

  LResult VisitGreater(RelationalExpr e) {
    return AMPL_DISPATCH(VisitRelational(e));
  }

  LResult VisitNotEqual(RelationalExpr e) {
    return AMPL_DISPATCH(VisitRelational(e));
  }

  LResult VisitLogicalCount(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitUnhandledLogicalExpr(e));
  }

  LResult VisitAtLeast(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitLogicalCount(e));
  }

  LResult VisitAtMost(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitLogicalCount(e));
  }

  LResult VisitExactly(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitLogicalCount(e));
  }

  LResult VisitNotAtLeast(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitLogicalCount(e));
  }

  LResult VisitNotAtMost(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitLogicalCount(e));
  }

  LResult VisitNotExactly(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitLogicalCount(e));
  }

  LResult VisitImplication(ImplicationExpr e) {
    return AMPL_DISPATCH(VisitUnhandledLogicalExpr(e));
  }

  LResult VisitIteratedLogical(IteratedLogicalExpr e) {
    return AMPL_DISPATCH(VisitUnhandledLogicalExpr(e));
  }

  LResult VisitForAll(IteratedLogicalExpr e) {
    return AMPL_DISPATCH(VisitIteratedLogical(e));
  }

  LResult VisitExists(IteratedLogicalExpr e) {
    return AMPL_DISPATCH(VisitIteratedLogical(e));
  }

  LResult VisitAllDiff(AllDiffExpr e) {
    return AMPL_DISPATCH(VisitUnhandledLogicalExpr(e));
  }
};

template <typename Impl, typename Result, typename LResult>
Result ExprVisitor<Impl, Result, LResult>::Visit(NumericExpr e) {
  // All expressions except OPNUMBEROFs, OPIFSYM, OPFUNCALL and OPHOL
  // are supported.
  switch (e.opcode()) {
  case OPPLUS:
    return AMPL_DISPATCH(VisitPlus(Expr::Create<BinaryExpr>(e)));
  case OPMINUS:
    return AMPL_DISPATCH(VisitMinus(Expr::Create<BinaryExpr>(e)));
  case OPMULT:
    return AMPL_DISPATCH(VisitMult(Expr::Create<BinaryExpr>(e)));
  case OPDIV:
    return AMPL_DISPATCH(VisitDiv(Expr::Create<BinaryExpr>(e)));
  case OPREM:
    return AMPL_DISPATCH(VisitRem(Expr::Create<BinaryExpr>(e)));
  case OPPOW:
    return AMPL_DISPATCH(VisitPow(Expr::Create<BinaryExpr>(e)));
  case OPLESS:
    return AMPL_DISPATCH(VisitNumericLess(Expr::Create<BinaryExpr>(e)));
  case MINLIST:
    return AMPL_DISPATCH(VisitMin(Expr::Create<VarArgExpr>(e)));
  case MAXLIST:
    return AMPL_DISPATCH(VisitMax(Expr::Create<VarArgExpr>(e)));
  case FLOOR:
    return AMPL_DISPATCH(VisitFloor(Expr::Create<UnaryExpr>(e)));
  case CEIL:
    return AMPL_DISPATCH(VisitCeil(Expr::Create<UnaryExpr>(e)));
  case ABS:
    return AMPL_DISPATCH(VisitAbs(Expr::Create<UnaryExpr>(e)));
  case OPUMINUS:
    return AMPL_DISPATCH(VisitUnaryMinus(Expr::Create<UnaryExpr>(e)));
  case OPIFnl:
    return AMPL_DISPATCH(VisitIf(Expr::Create<IfExpr>(e)));
  case OP_tanh:
    return AMPL_DISPATCH(VisitTanh(Expr::Create<UnaryExpr>(e)));
  case OP_tan:
    return AMPL_DISPATCH(VisitTan(Expr::Create<UnaryExpr>(e)));
  case OP_sqrt:
    return AMPL_DISPATCH(VisitSqrt(Expr::Create<UnaryExpr>(e)));
  case OP_sinh:
    return AMPL_DISPATCH(VisitSinh(Expr::Create<UnaryExpr>(e)));
  case OP_sin:
    return AMPL_DISPATCH(VisitSin(Expr::Create<UnaryExpr>(e)));
  case OP_log10:
    return AMPL_DISPATCH(VisitLog10(Expr::Create<UnaryExpr>(e)));
  case OP_log:
    return AMPL_DISPATCH(VisitLog(Expr::Create<UnaryExpr>(e)));
  case OP_exp:
    return AMPL_DISPATCH(VisitExp(Expr::Create<UnaryExpr>(e)));
  case OP_cosh:
    return AMPL_DISPATCH(VisitCosh(Expr::Create<UnaryExpr>(e)));
  case OP_cos:
    return AMPL_DISPATCH(VisitCos(Expr::Create<UnaryExpr>(e)));
  case OP_atanh:
    return AMPL_DISPATCH(VisitAtanh(Expr::Create<UnaryExpr>(e)));
  case OP_atan2:
    return AMPL_DISPATCH(VisitAtan2(Expr::Create<BinaryExpr>(e)));
  case OP_atan:
    return AMPL_DISPATCH(VisitAtan(Expr::Create<UnaryExpr>(e)));
  case OP_asinh:
    return AMPL_DISPATCH(VisitAsinh(Expr::Create<UnaryExpr>(e)));
  case OP_asin:
    return AMPL_DISPATCH(VisitAsin(Expr::Create<UnaryExpr>(e)));
  case OP_acosh:
    return AMPL_DISPATCH(VisitAcosh(Expr::Create<UnaryExpr>(e)));
  case OP_acos:
    return AMPL_DISPATCH(VisitAcos(Expr::Create<UnaryExpr>(e)));
  case OPSUMLIST:
    return AMPL_DISPATCH(VisitSum(Expr::Create<SumExpr>(e)));
  case OPintDIV:
    return AMPL_DISPATCH(VisitIntDiv(Expr::Create<BinaryExpr>(e)));
  case OPprecision:
    return AMPL_DISPATCH(VisitPrecision(Expr::Create<BinaryExpr>(e)));
  case OPround:
    return AMPL_DISPATCH(VisitRound(Expr::Create<BinaryExpr>(e)));
  case OPtrunc:
    return AMPL_DISPATCH(VisitTrunc(Expr::Create<BinaryExpr>(e)));
  case OPCOUNT:
    return AMPL_DISPATCH(VisitCount(Expr::Create<CountExpr>(e)));
  case OPNUMBEROF:
    return AMPL_DISPATCH(VisitNumberOf(Expr::Create<NumberOfExpr>(e)));
  case OPPLTERM:
    return AMPL_DISPATCH(VisitPiecewiseLinear(
        Expr::Create<PiecewiseLinearExpr>(e)));
  case OP1POW:
    return AMPL_DISPATCH(VisitPowConstExp(Expr::Create<BinaryExpr>(e)));
  case OP2POW:
    return AMPL_DISPATCH(VisitPow2(Expr::Create<UnaryExpr>(e)));
  case OPCPOW:
    return AMPL_DISPATCH(VisitPowConstBase(Expr::Create<BinaryExpr>(e)));
  case OPFUNCALL:
    return AMPL_DISPATCH(VisitCall(Expr::Create<CallExpr>(e)));
  case OPNUM:
    return AMPL_DISPATCH(VisitNumericConstant(
        Expr::Create<NumericConstant>(e)));
  case OPVARVAL:
    return AMPL_DISPATCH(VisitVariable(Expr::Create<Variable>(e)));
  default:
    // Normally this branch shouldn't be executed.
    return AMPL_DISPATCH(VisitInvalidNumericExpr(e));
  }
}

template <typename Impl, typename Result, typename LResult>
LResult ExprVisitor<Impl, Result, LResult>::Visit(LogicalExpr e) {
  switch (e.opcode()) {
  case OPOR:
    return AMPL_DISPATCH(VisitOr(Expr::Create<BinaryLogicalExpr>(e)));
  case OPAND:
    return AMPL_DISPATCH(VisitAnd(Expr::Create<BinaryLogicalExpr>(e)));
  case LT:
    return AMPL_DISPATCH(VisitLess(Expr::Create<RelationalExpr>(e)));
  case LE:
    return AMPL_DISPATCH(VisitLessEqual(Expr::Create<RelationalExpr>(e)));
  case EQ:
    return AMPL_DISPATCH(VisitEqual(Expr::Create<RelationalExpr>(e)));
  case GE:
    return AMPL_DISPATCH(VisitGreaterEqual(Expr::Create<RelationalExpr>(e)));
  case GT:
    return AMPL_DISPATCH(VisitGreater(Expr::Create<RelationalExpr>(e)));
  case NE:
    return AMPL_DISPATCH(VisitNotEqual(Expr::Create<RelationalExpr>(e)));
  case OPNOT:
    return AMPL_DISPATCH(VisitNot(Expr::Create<NotExpr>(e)));
  case OPATLEAST:
    return AMPL_DISPATCH(VisitAtLeast(Expr::Create<LogicalCountExpr>(e)));
  case OPATMOST:
    return AMPL_DISPATCH(VisitAtMost(Expr::Create<LogicalCountExpr>(e)));
  case OPEXACTLY:
    return AMPL_DISPATCH(VisitExactly(Expr::Create<LogicalCountExpr>(e)));
  case OPNOTATLEAST:
    return AMPL_DISPATCH(VisitNotAtLeast(Expr::Create<LogicalCountExpr>(e)));
  case OPNOTATMOST:
    return AMPL_DISPATCH(VisitNotAtMost(Expr::Create<LogicalCountExpr>(e)));
  case OPNOTEXACTLY:
    return AMPL_DISPATCH(VisitNotExactly(Expr::Create<LogicalCountExpr>(e)));
  case ANDLIST:
    return AMPL_DISPATCH(VisitForAll(Expr::Create<IteratedLogicalExpr>(e)));
  case ORLIST:
    return AMPL_DISPATCH(VisitExists(Expr::Create<IteratedLogicalExpr>(e)));
  case OPIMPELSE:
    return AMPL_DISPATCH(VisitImplication(Expr::Create<ImplicationExpr>(e)));
  case OP_IFF:
    return AMPL_DISPATCH(VisitIff(Expr::Create<BinaryLogicalExpr>(e)));
  case OPALLDIFF:
    return AMPL_DISPATCH(VisitAllDiff(Expr::Create<AllDiffExpr>(e)));
  case OPNUM:
    return AMPL_DISPATCH(VisitLogicalConstant(
        Expr::Create<LogicalConstant>(e)));
  default:
    // Normally this branch shouldn't be executed.
    return AMPL_DISPATCH(VisitInvalidLogicalExpr(e));
  }
}

// Expression converter.
// Converts logical count expressions to corresponding relational expressions.
// For example "atleast" is converted to "<=".
template <typename Impl, typename Result, typename LResult = Result>
class ExprConverter : public ExprVisitor<Impl, Result, LResult> {
 private:
  std::vector< ::expr> exprs_;

  RelationalExpr Convert(LogicalCountExpr e, int opcode) {
    exprs_.push_back(*e.expr_);
    ::expr *result = &exprs_.back();
    result->op = reinterpret_cast<efunc*>(opcode);
    return Expr::Create<RelationalExpr>(result);
  }

 public:
  LResult VisitAtLeast(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitLessEqual(Convert(e, LE)));
  }
  LResult VisitAtMost(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitGreaterEqual(Convert(e, GE)));
  }
  LResult VisitExactly(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitEqual(Convert(e, EQ)));
  }
  LResult VisitNotAtLeast(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitGreater(Convert(e, GT)));
  }
  LResult VisitNotAtMost(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitLess(Convert(e, LT)));
  }
  LResult VisitNotExactly(LogicalCountExpr e) {
    return AMPL_DISPATCH(VisitNotEqual(Convert(e, NE)));
  }
};

#undef AMPL_DISPATCH

template <typename Grad>
class LinearExpr;

// A linear term.
template <typename GradT>
class LinearTerm {
 private:
  typedef GradT Grad;
  Grad *grad_;

  friend class LinearExpr< LinearTerm<Grad> >;

  explicit LinearTerm(Grad *g = 0) : grad_(g) {}

 public:
  // Returns the coefficient.
  double coef() const { return grad_->coef; }

  // Returns the variable index.
  int var_index() const { return grad_->varno; }
};

typedef LinearTerm<ograd> LinearObjTerm;
typedef LinearTerm<cgrad> LinearConTerm;

// A linear expression.
template <typename Term>
class LinearExpr {
 private:
  Term first_term_;

  friend class Problem;

  explicit LinearExpr(typename Term::Grad *first_term)
  : first_term_(Term(first_term)) {}

 public:
  LinearExpr() {}

  class iterator : public std::iterator<std::forward_iterator_tag, Term> {
   private:
    Term term_;

   public:
    explicit iterator(Term t) : term_(t) {}

    Term operator*() const { return term_; }
    const Term *operator->() const { return &term_; }

    iterator &operator++() {
      term_ = Term(term_.grad_->next);
      return *this;
    }

    iterator operator++(int ) {
      iterator it(*this);
      term_ = Term(term_.grad_->next);
      return it;
    }

    bool operator==(iterator other) const {
      return term_.grad_ == other.term_.grad_;
    }
    bool operator!=(iterator other) const {
      return term_.grad_ != other.term_.grad_;
    }
  };

  iterator begin() { return iterator(first_term_); }
  iterator end() { return iterator(Term()); }
};

typedef LinearExpr<LinearObjTerm> LinearObjExpr;
typedef LinearExpr<LinearConTerm> LinearConExpr;

template <typename LinearExpr>
void WriteExpr(fmt::Writer &w, LinearExpr linear, NumericExpr nonlinear);

namespace internal {

#ifdef HAVE_UNORDERED_MAP
template <class T>
inline std::size_t HashCombine(std::size_t seed, const T &v) {
  return seed ^ (std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

class HashNumberOfArgs {
 public:
  std::size_t operator()(NumberOfExpr e) const;
};
#endif

class EqualNumberOfArgs {
 public:
  bool operator()(NumberOfExpr lhs, NumberOfExpr rhs) const;
};

template <typename NumberOf>
class MatchNumberOfArgs {
 private:
  NumberOfExpr expr_;

 public:
  explicit MatchNumberOfArgs(NumberOfExpr e) : expr_(e) {}

  // Returns true if the stored expression has the same arguments as the nof's
  // expression.
  bool operator()(const NumberOf &nof) const {
    return EqualNumberOfArgs()(expr_, nof.expr);
  }
};
}  // namespace internal

// A map from numberof expressions with the same argument lists to
// values and corresponding variables.
template <typename Var, typename CreateVar>
class NumberOfMap {
 public:
  typedef std::map<double, Var> ValueMap;

  struct NumberOf {
    NumberOfExpr expr;
    ValueMap values;

    explicit NumberOf(NumberOfExpr e) : expr(e) {}
  };

 private:
  CreateVar create_var_;

#ifdef HAVE_UNORDERED_MAP
  // Map from a numberof expression to an index in numberofs_.
  typedef std::unordered_map<NumberOfExpr, std::size_t,
    internal::HashNumberOfArgs, internal::EqualNumberOfArgs> Map;
  Map map_;
#endif

  std::vector<NumberOf> numberofs_;

 public:
  explicit NumberOfMap(CreateVar cv) : create_var_(cv) {}

  typedef typename std::vector<NumberOf>::const_iterator iterator;

  iterator begin() const {
    return numberofs_.begin();
  }

  iterator end() const {
    return numberofs_.end();
  }

  // Adds a numberof expression with a constant value.
  Var Add(double value, NumberOfExpr e);
};

template <typename Var, typename CreateVar>
Var NumberOfMap<Var, CreateVar>::Add(double value, NumberOfExpr e) {
  assert(Cast<NumericConstant>(e[0]).value() == value);
#ifdef HAVE_UNORDERED_MAP
  std::pair<typename Map::iterator, bool> result =
      map_.insert(typename Map::value_type(e, numberofs_.size()));
  if (result.second)
    numberofs_.push_back(NumberOf(e));
  ValueMap &values = numberofs_[result.first->second].values;
#else
  typename std::vector<NumberOf>::reverse_iterator np =
      std::find_if(numberofs_.rbegin(), numberofs_.rend(),
                   internal::MatchNumberOfArgs<NumberOf>(e));
  if (np == numberofs_.rend()) {
    numberofs_.push_back(NumberOf(e));
    np = numberofs_.rbegin();
  }
  ValueMap &values = np->values;
#endif
  typename ValueMap::iterator i = values.lower_bound(value);
  if (i != values.end() && !values.key_comp()(value, i->first))
    return i->second;
  Var var(create_var_());
  values.insert(i, typename ValueMap::value_type(value, var));
  return var;
}
}  // namespace mp

#endif  // MP_EXPR_H_
