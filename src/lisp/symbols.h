#ifndef SYMBOLS_H
#define SYMBOLS_H

/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Sam Hocevar.
 */

#include <array>

struct func
{
    char const *name;
    short min_args;
    short max_args;
};

enum class SysFunc
{
    Print,
    Car,
    Cdr,
    Length,
    List,
    Cons,
    Quote,
    Eq,
    Plus,
    Minus,
    If,
    Setf,
    SymbolList,
    Assoc,
    Null,
    Acons,
    Pairlis,
    Let,
    Defun,
    Atom,
    Not,
    And,
    Or,
    Progn,
    Equal,
    Concatenate,
    CharCode,
    CodeChar,
    Times,
    Slash,
    Cond,
    Select,
    Function,
    Mapcar,
    Funcall,
    GreaterThan,
    LessThan,
    TmpSpace,
    PermSpace,
    SymbolName,
    Trace,
    Untrace,
    Digstr,
    CompileFile,
    Abs,
    Min,
    Max,
    GreaterOrEqual,
    LessOrEqual,
    Backquote,
    Comma,
    Nth,
    ResizeTmp,
    ResizePerm,
    Cos,
    Sin,
    Atan2,
    Enum,
    Quit,
    Eval,
    Break,
    Mod,
    WriteProfile,
    Setq,
    For,
    OpenFile,
    Load,
    BitAnd,
    BitOr,
    BitXor,
    MakeArray,
    Aref,
    If1Progn,
    If2Progn,
    If12Progn,
    Eq0,
    Preport,
    Search,
    Elt,
    Listp,
    Numberp,
    Do,
    GarbageCollect,
    Schar,
    Symbolp,
    Num2Str,
    Nconc,
    First,
    Second,
    Third,
    Fourth,
    Fifth,
    Sixth,
    Seventh,
    Eighth,
    Ninth,
    Tenth,
    Substr,
    LocalLoad,
    Count
};

static std::array<func, static_cast<size_t>(SysFunc::Count)> sys_funcs;

inline void initSysFuncs()
{
    sys_funcs[static_cast<size_t>(SysFunc::Print)] = {"print", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Car)] = {"car", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Cdr)] = {"cdr", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Length)] = {"length", 0, -1};
    sys_funcs[static_cast<size_t>(SysFunc::List)] = {"list", 0, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Cons)] = {"cons", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Quote)] = {"quote", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Eq)] = {"eq", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Plus)] = {"+", 0, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Minus)] = {"-", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::If)] = {"if", 2, 3};
    sys_funcs[static_cast<size_t>(SysFunc::Setf)] = {"setf", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::SymbolList)] = {"symbol-list", 0, 0};
    sys_funcs[static_cast<size_t>(SysFunc::Assoc)] = {"assoc", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Null)] = {"null", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Acons)] = {"acons", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Pairlis)] = {"pairlis", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Let)] = {"let", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Defun)] = {"defun", 2, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Atom)] = {"atom", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Not)] = {"not", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::And)] = {"and", -1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Or)] = {"or", -1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Progn)] = {"progn", -1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Equal)] = {"equal", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Concatenate)] = {"concatenate", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::CharCode)] = {"char-code", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::CodeChar)] = {"code-char", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Times)] = {"*", -1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Slash)] = {"/", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Cond)] = {"cond", -1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Select)] = {"select", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Function)] = {"function", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Mapcar)] = {"mapcar", 2, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Funcall)] = {"funcall", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::GreaterThan)] = {">", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::LessThan)] = {"<", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::TmpSpace)] = {"tmp-space", 0, 0};
    sys_funcs[static_cast<size_t>(SysFunc::PermSpace)] = {"perm-space", 0, 0};
    sys_funcs[static_cast<size_t>(SysFunc::SymbolName)] = {"symbol-name", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Trace)] = {"trace", 0, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Untrace)] = {"untrace", 0, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Digstr)] = {"digstr", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::CompileFile)] = {"compile-file", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Abs)] = {"abs", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Min)] = {"min", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Max)] = {"max", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::GreaterOrEqual)] = {">=", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::LessOrEqual)] = {"<=", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Backquote)] = {"backquote", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Comma)] = {"comma", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Nth)] = {"nth", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::ResizeTmp)] = {"resize-tmp", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::ResizePerm)] = {"resize-perm", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Cos)] = {"cos", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Sin)] = {"sin", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Atan2)] = {"atan2", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Enum)] = {"enum", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Quit)] = {"quit", 0, 0};
    sys_funcs[static_cast<size_t>(SysFunc::Eval)] = {"eval", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Break)] = {"break", 0, 0};
    sys_funcs[static_cast<size_t>(SysFunc::Mod)] = {"mod", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::WriteProfile)] = {"write_profile", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Setq)] = {"setq", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::For)] = {"for", 4, -1};
    sys_funcs[static_cast<size_t>(SysFunc::OpenFile)] = {"open_file", 2, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Load)] = {"load", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::BitAnd)] = {"bit-and", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::BitOr)] = {"bit-or", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::BitXor)] = {"bit-xor", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::MakeArray)] = {"make-array", 1, -1};
    sys_funcs[static_cast<size_t>(SysFunc::Aref)] = {"aref", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::If1Progn)] = {"if-1progn", 2, 3};
    sys_funcs[static_cast<size_t>(SysFunc::If2Progn)] = {"if-2progn", 2, 3};
    sys_funcs[static_cast<size_t>(SysFunc::If12Progn)] = {"if-12progn", 2, 3};
    sys_funcs[static_cast<size_t>(SysFunc::Eq0)] = {"eq0", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Preport)] = {"preport", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Search)] = {"search", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Elt)] = {"elt", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Listp)] = {"listp", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Numberp)] = {"numberp", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Do)] = {"do", 2, 3};
    sys_funcs[static_cast<size_t>(SysFunc::GarbageCollect)] = {"gc", 0, 0};
    sys_funcs[static_cast<size_t>(SysFunc::Schar)] = {"schar", 2, 2};
    sys_funcs[static_cast<size_t>(SysFunc::Symbolp)] = {"symbolp", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Num2Str)] = {"num2str", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Nconc)] = {"nconc", 2, -1};
    sys_funcs[static_cast<size_t>(SysFunc::First)] = {"first", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Second)] = {"second", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Third)] = {"third", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Fourth)] = {"fourth", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Fifth)] = {"fifth", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Sixth)] = {"sixth", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Seventh)] = {"seventh", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Eighth)] = {"eighth", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Ninth)] = {"ninth", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Tenth)] = {"tenth", 1, 1};
    sys_funcs[static_cast<size_t>(SysFunc::Substr)] = {"substr", 3, 3};
    sys_funcs[static_cast<size_t>(SysFunc::LocalLoad)] = {"local_load", 1, 1};
}

static_assert(sys_funcs.size() == static_cast<size_t>(SysFunc::Count),
              "Mismatch between sys_funcs array and SysFunc enum size.");

#endif // SYMBOLS_H
