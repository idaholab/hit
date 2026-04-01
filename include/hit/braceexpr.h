#pragma once

#include "hit/parse.h"

#include <string>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <variant>

namespace hit
{

inline std::string
errormsg(std::string /*fname*/, Node * /*n*/)
{
  return "";
}

template <typename T, typename... Args>
std::string
errormsg(std::string fname, Node * n, T arg, Args... args)
{
  std::stringstream ss;
  if (n && fname.size() > 0)
    ss << fname << ":" << n->line() << "." << n->column() << ": ";
  else if (fname.size() > 0)
    ss << fname << ":0: ";
  ss << arg;
  ss << errormsg("", nullptr, args...);
  return ss.str();
}

inline std::string
errormsg(Node * /*n*/)
{
  return "";
}

template <typename T, typename... Args>
std::string
errormsg(Node * n, T arg, Args... args)
{
  std::stringstream ss;
  if (n)
    ss << n->filename() << ":" << n->line() << "." << n->column() << ": ";
  ss << arg;
  ss << errormsg(nullptr, args...);
  return ss.str();
}

class BraceNode
{
public:
  /// Render this node and its children as an indented debug string.
  std::string str(int indent = 0);

  /// Append a new child node and return it for in-place population during parsing.
  BraceNode & append();

  /// Access the nested brace nodes collected beneath this node.
  inline std::vector<BraceNode> & list() { return _list; }

  /// Access the literal text stored for this node between nested brace expressions.
  inline std::string & val() { return _val; }

  /// Access the byte offset of this node within the original brace expression.
  inline size_t & offset() { return _offset; }

  /// Access the span length covered by this node in the original input.
  inline size_t & len() { return _len; }

private:
  /// Byte offset where this parsed node begins in the source expression.
  size_t _offset;

  /// Number of characters covered by this node in the source expression.
  size_t _len;

  /// Literal text content associated with this node.
  std::string _val;

  /// Child nodes representing nested brace expressions.
  std::vector<BraceNode> _list;
};

/// Parse a brace node beginning at \p start and store the resulting subtree in \p n.
size_t parseBraceNode(const std::string & input, size_t start, BraceNode & n);

class BraceExpr;

/// One segment of a parsed template, either literal text or a nested brace expression.
using BracePart = std::variant<std::string, std::shared_ptr<BraceExpr>>;

class BraceTemplate
{
public:
  /// Access the ordered sequence of literal and nested-expression parts in this template.
  inline std::vector<BracePart> & parts() { return _parts; }

  /// Access the ordered sequence of literal and nested-expression parts in this template.
  inline const std::vector<BracePart> & parts() const { return _parts; }

  /// Determine whether this template consists of a single brace expression and no literal text.
  bool pureExpression() const;

  /// Determine whether any literal text remains alongside embedded expressions.
  bool hasLiteralText() const;

  /// Count the number of embedded brace expressions stored in this template.
  std::size_t expressionCount() const;

private:
  /// Template fragments preserved in source order for later evaluation.
  std::vector<BracePart> _parts;
};

class BraceArg
{
public:
  /// Access the parsed template that supplies this argument's value.
  inline BraceTemplate & value() { return _value; }

  /// Access the parsed template that supplies this argument's value.
  inline const BraceTemplate & value() const { return _value; }

private:
  /// Argument payload, which may itself contain nested brace expressions.
  BraceTemplate _value;
};

class BraceExpr
{
public:
  /// Access the argument list parsed from the current brace expression.
  inline std::vector<BraceArg> & args() { return _args; }

  /// Access the argument list parsed from the current brace expression.
  inline const std::vector<BraceArg> & args() const { return _args; }

  /// Access the 1-based source line where this expression begins.
  inline std::size_t & line() { return _line; }

  /// Access the 1-based source line where this expression begins.
  inline const std::size_t & line() const { return _line; }

  /// Access the 1-based source column where this expression begins.
  inline std::size_t & column() { return _column; }

  /// Access the 1-based source column where this expression begins.
  inline const std::size_t & column() const { return _column; }

  /// Access the byte position of this expression within the original field value.
  inline std::size_t & position() { return _position; }

  /// Access the byte position of this expression within the original field value.
  inline const std::size_t & position() const { return _position; }

  /// Access the number of characters occupied by this expression in the source text.
  inline std::size_t & length() { return _length; }

  /// Access the number of characters occupied by this expression in the source text.
  inline const std::size_t & length() const { return _length; }

  /// Access the original expression text, including braces, for diagnostics.
  inline std::string & text() { return _text; }

  /// Access the original expression text, including braces, for diagnostics.
  inline const std::string & text() const { return _text; }

private:
  /// Parsed arguments passed to the evaluator named by the first argument.
  std::vector<BraceArg> _args;

  /// 1-based line number where this expression starts in the source field.
  std::size_t _line = 1;

  /// 1-based column number where this expression starts in the source field.
  std::size_t _column = 1;

  /// Byte offset of the opening brace relative to the source field value.
  std::size_t _position = 0;

  /// Number of characters spanning the full brace expression.
  std::size_t _length = 0;

  /// Original source text for this expression, preserved for error reporting.
  std::string _text;
};

struct BraceParseError
{
  /// 1-based source line where parsing failed.
  std::size_t line = 1;

  /// 1-based source column where parsing failed.
  std::size_t column = 1;

  /// Human-readable description of the parse failure.
  std::string message;
};

/// Parse an input string into a brace-expression template with nested-expression structure preserved.
BraceTemplate parseBraceTemplate(const std::string & input, BraceParseError * error = nullptr);
class BraceExpander;

struct EvalResult
{
  /// Final text produced by evaluating the requested brace expression.
  std::string value;

  /// Field kind associated with the resolved value, when one is available.
  Field::Kind kind = Field::Kind::None;

  /// Fully qualified field paths consumed while producing this result.
  std::vector<std::string> used;
};

class Evaler
{
public:
  /// Virtual destructor for evaluator implementations used through the base interface.
  virtual ~Evaler() {}

  /// Evaluate a brace expression invocation against field \p n with already-expanded \p args.
  virtual EvalResult
  eval(Field * n, const std::vector<std::string> & args, BraceExpander & exp) = 0;
};

class EnvEvaler : public Evaler
{
public:
  /// Resolve an environment-variable substitution request.
  virtual EvalResult
  eval(Field * n, const std::vector<std::string> & args, BraceExpander & exp) override;
};

class RawEvaler : public Evaler
{
public:
  /// Return the unexpanded value of a referenced field.
  virtual EvalResult
  eval(Field * n, const std::vector<std::string> & args, BraceExpander & exp) override;
};

class ReplaceEvaler : public Evaler
{
public:
  /// Perform string replacement on the evaluated input arguments.
  virtual EvalResult
  eval(Field * n, const std::vector<std::string> & args, BraceExpander & exp) override;
};

class BraceExpander : public Walker
{
public:
  /// Construct an expander with the built-in replacement evaluator available.
  BraceExpander() {}

  /// Register an evaluator implementation under the function name used in brace expressions.
  void registerEvaler(const std::string & name, Evaler & ev);

  /// Expand brace expressions for each visited field while traversing a HIT tree.
  virtual void walk(const std::string & /*fullpath*/, const std::string & /*nodepath*/, Node * n);

  /// Expand all brace expressions found in \p input relative to field \p n.
  std::string expand(Field * n, const std::string & input);

  /// Locate the field referenced by \p path relative to \p n and optionally record the normalized path.
  Field *
  findReferencedField(Field * n, const std::string & path, std::string * used_path = nullptr);

  /// Resolve a field once, caching the result and detecting dependency cycles.
  const EvalResult & resolve(Field * n);

  /// Build an error message against the currently active expression span.
  ErrorMessage currentExpressionErrorMessage(Field * n, const std::string & message) const;

  /// Build an error object against the currently active expression span.
  Error currentExpressionError(Field * n, const std::string & message) const;

  /// Field paths referenced while expanding the current input.
  std::vector<std::string> used;

  /// Errors collected during tree traversal and expression evaluation.
  std::vector<ErrorMessage> errors;

private:
  enum class ResolveState
  {
    Unvisited, /// Field has not yet been evaluated during this expansion pass.
    Resolving, /// Field evaluation is in progress and should trigger cycle detection if revisited.
    Resolved   /// Field evaluation completed and the cached result is valid.
  };

  struct ResolveEntry
  {
    /// Current resolution state used to detect recursion through brace dependencies.
    ResolveState state = ResolveState::Unvisited;

    /// Cached expansion result for the corresponding field.
    EvalResult result;
  };

  /// Evaluate a parsed template by concatenating its literal and expression parts.
  EvalResult evaluate(Field * n, const BraceTemplate & expr);

  /// Evaluate a single brace expression invocation.
  EvalResult evaluate(Field * n, const BraceExpr & expr);

  /// Convert a parse failure into a HIT error associated with field \p n.
  Error syntaxError(Field * n, const BraceParseError & error) const;

  /// Build an error object covering the source span of a parsed brace expression.
  Error expressionError(Field * n, const BraceExpr & expr, const std::string & message) const;

  /// Build an error message covering the source span of a parsed brace expression.
  ErrorMessage
  expressionErrorMessage(Field * n, const BraceExpr & expr, const std::string & message) const;

  /// Registered evaluator implementations keyed by brace-expression function name.
  std::map<std::string, Evaler *> _evalers;

  /// Per-field cached results and recursion state for dependency-aware resolution.
  std::map<Field *, ResolveEntry> _resolved;

  /// Stack of fields currently being resolved to report dependency cycles clearly.
  std::vector<Field *> _resolving_stack;

  /// Expression currently being evaluated, used to anchor contextual diagnostics.
  const BraceExpr * _active_expr = nullptr;

  /// Built-in evaluator that backs `${replace ...}` expressions.
  ReplaceEvaler _replace;
};

} // namespace hit
