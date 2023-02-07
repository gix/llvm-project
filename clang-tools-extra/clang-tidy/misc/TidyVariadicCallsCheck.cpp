//===--- TidyVariadicCallsCheck.cpp - clang-tidy --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TidyVariadicCallsCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

namespace clang {
namespace tidy {
namespace misc {

using namespace clang::ast_matchers;

static bool isCStringType(QualType QT) {
  auto RT = dyn_cast_or_null<const RecordType>(QT.getTypePtrOrNull());
  if (!RT)
    return false;

  auto RD = dyn_cast<const CXXRecordDecl>(RT->getDecl());
  if (!RD)
    return false;

  if (RD->getName() != "CStringT")
    return false;

  auto NS = cast<NamespaceDecl>(RD->getEnclosingNamespaceContext());
  if (NS->getName() != "ATL")
    return false;

  return true;
}

static const Expr *skipTemporaryBindingsCtorAndNoOpCasts(const Expr *E) {
  if (const MaterializeTemporaryExpr *M = dyn_cast<MaterializeTemporaryExpr>(E))
    E = M->getSubExpr();

  while (const ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E)) {
    if (ICE->getCastKind() == CK_NoOp)
      E = ICE->getSubExpr();
    else
      break;
  }

  while (const CXXBindTemporaryExpr *BE = dyn_cast<CXXBindTemporaryExpr>(E))
    E = BE->getSubExpr();

  if (const CXXConstructExpr *CE = dyn_cast<CXXConstructExpr>(E)) {
    if (CE->getNumArgs() == 1 && !isa<CXXTemporaryObjectExpr>(E))
      E = CE->getArg(0);
  }

  while (const ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E)) {
    if (ICE->getCastKind() == CK_NoOp)
      E = ICE->getSubExpr();
    else
      break;
  }

  return E;
}

static bool needsParens(const Expr *E) {
  if (auto Bind = dyn_cast<CXXBindTemporaryExpr>(E))
    return needsParens(Bind->getSubExpr());

  if (auto Op = dyn_cast<CXXOperatorCallExpr>(E)) {
    switch (Op->getOperator()) {
    case OO_New:
    case OO_Delete: // Fall-through on purpose.
    case OO_Array_New:
    case OO_Array_Delete:
    case OO_ArrowStar:
    case OO_Arrow:
    case OO_Call:
    case OO_Subscript:
      return false;
    default:
      return true;
    }
  }

  if (isa<CallExpr>(E) || isa<CXXMemberCallExpr>(E))
    return false;
  if (isa<DeclRefExpr>(E))
    return false;
  if (isa<ArraySubscriptExpr>(E))
    return false;
  if (isa<ParenExpr>(E))
    return false;
  if (isa<MemberExpr>(E))
    return false;

  return true;
}

static bool isOrderedSourceRange(const SourceManager &SM, SourceRange R) {
  std::pair<FileID, unsigned> DB = SM.getDecomposedLoc(R.getBegin());
  std::pair<FileID, unsigned> DE = SM.getDecomposedLoc(R.getEnd());

  std::pair<bool, bool> result = SM.isInTheSameTranslationUnit(DB, DE);
  return result.second;
}

void TidyVariadicCallsCheck::registerMatchers(MatchFinder *Finder) {
  clang::ast_matchers::StatementMatcher const &call =
      callExpr(callee(functionDecl(isVariadic()))).bind("call");
  Finder->addMatcher(call, this);
}

void TidyVariadicCallsCheck::check(const MatchFinder::MatchResult &Result) {
  const auto &SM = *Result.SourceManager;
  const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call");
  const clang::Decl *Callee = cast<clang::FunctionDecl>(Call->getCalleeDecl());

  size_t const nonVariadicArgCount =
      cast<clang::FunctionDecl>(Callee)->param_size();
  if (nonVariadicArgCount > Call->getNumArgs())
    return;

  llvm::SmallVector<const Expr *> Args(Call->arg_begin() + nonVariadicArgCount,
                                       Call->arg_end());

  for (const Expr *Arg : Args) {
    clang::QualType ArgType = Arg->getType();
    ArgType.removeLocalFastQualifiers();
    ArgType = ArgType.getDesugaredType(*Result.Context);

    if (isCStringType(ArgType)) {
      const Expr *E = skipTemporaryBindingsCtorAndNoOpCasts(Arg);

      SourceRange R = E->getSourceRange();
      R.setBegin(SM.getSpellingLoc(R.getBegin()));
      R.setEnd(Lexer::getLocForEndOfToken(SM.getSpellingLoc(R.getEnd()), 0, SM,
                                          Result.Context->getLangOpts()));

      if (!isOrderedSourceRange(SM, R))
        continue; // Usually a macro, skip it since don't know the real end.

      if (needsParens(E)) {
        diag(R.getBegin(), "CString passed as vararg")
            << FixItHint::CreateInsertion(R.getBegin(), "(")
            << FixItHint::CreateInsertion(R.getEnd(), ").GetString()");
      } else {
        diag(R.getBegin(), "CString passed as vararg")
            << FixItHint::CreateInsertion(R.getEnd(), ".GetString()");
      }
    }
  }
}

} // namespace misc
} // namespace tidy
} // namespace clang
