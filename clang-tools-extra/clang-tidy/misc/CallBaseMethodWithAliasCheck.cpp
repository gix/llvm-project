//===--- CallBaseMethodWithAliasCheck.cpp - clang-tidy --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CallBaseMethodWithAliasCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

#define DEBUG_TYPE "EasilySwappableParametersCheck"

namespace clang {
namespace tidy {
namespace misc {

using namespace clang::ast_matchers;

static bool isDirectParentOf(const CXXRecordDecl &Parent,
                             const CXXRecordDecl &ThisClass) {
  if (Parent.getCanonicalDecl() == ThisClass.getCanonicalDecl())
    return true;
  const CXXRecordDecl *ParentCanonicalDecl = Parent.getCanonicalDecl();
  return llvm::any_of(ThisClass.bases(), [=](const CXXBaseSpecifier &Base) {
    auto *BaseDecl = Base.getType()->getAsCXXRecordDecl();
    assert(BaseDecl);
    return ParentCanonicalDecl == BaseDecl->getCanonicalDecl();
  });
}

const TypeAliasDecl *findBaseAlias(const CXXRecordDecl *RD) {

  for (Decl *D : RD->decls()) {
    if (const auto *TAD = dyn_cast<TypeAliasDecl>(D)) {
      if (TAD->getName() == "base") {
        return TAD;
      }
    }
  }
  return nullptr;
}

SourceLocation findLocationBeforeToken(SourceLocation Loc, tok::TokenKind TKind,
                                       const SourceManager &SM,
                                       const LangOptions &LangOpts,
                                       bool SkipTrailingWhitespaceAndNewLine) {
  std::optional<Token> Tok = Lexer::findNextToken(Loc, SM, LangOpts);
  if (!Tok || Tok->isNot(TKind))
    return {};
  return Tok->getLocation();
}

void CallBaseMethodWithAliasCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      traverse(TK_AsIs,
               cxxMemberCallExpr(
                   callee(memberExpr(
                              hasDescendant(
                                  implicitCastExpr(
                                      hasImplicitDestinationType(pointsTo(
                                          type(anything()).bind("castToType"))),
                                      hasSourceExpression(cxxThisExpr(hasType(
                                          type(anything()).bind("thisType")))))
                                      .bind("implicitCast")))
                              .bind("member")))
                   .bind("call")),
      this);
}

void CallBaseMethodWithAliasCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto &SM = *Result.SourceManager;
  const auto &Context = *Result.Context;
  const auto &LO = Context.getLangOpts();
  const auto *Call = Result.Nodes.getNodeAs<CXXMemberCallExpr>("call");
  const SourceRange R = Call->getSourceRange();

  const auto *Member = Result.Nodes.getNodeAs<MemberExpr>("member");
  assert(Member);

  if (llvm::DebugFlag) {
    llvm::dbgs() << "\n---------- Matched call ----------\n";
    llvm::dbgs() << "Loc: ";
    Member->getSourceRange().dump(SM);
    llvm::dbgs() << "\n";
    Member->dumpPretty(Context);
    llvm::dbgs() << "\n";
    Member->dumpColor();
    llvm::dbgs() << "\n";
  }

  if (!Member->getQualifier()) {
    LLVM_DEBUG(llvm::dbgs() << "Skipping: no qualifier\n");
    return;
  }

  const auto *ThisTypePtr = Result.Nodes.getNodeAs<PointerType>("thisType");
  const auto *CastToTypePtr = Result.Nodes.getNodeAs<Type>("castToType");
  if (!ThisTypePtr || !CastToTypePtr) {
    LLVM_DEBUG(llvm::dbgs() << "Skipping: null TypePtrs\n");
    return;
  }

  const auto *ThisType = ThisTypePtr->getPointeeCXXRecordDecl();
  const auto *CastToType = CastToTypePtr->getAsCXXRecordDecl();
  if (!ThisType || !CastToType) {
    LLVM_DEBUG(llvm::dbgs() << "Skipping: null CXX record decls\n");
    return;
  }

  LLVM_DEBUG(llvm::dbgs() << "ThisType:   " << ThisType->getNameAsString()
                          << "\n");
  LLVM_DEBUG(llvm::dbgs() << "CastToType: " << CastToType->getNameAsString()
                          << "\n");

  if (!isDirectParentOf(*CastToType, *ThisType)) {
    diag(R.getBegin(),
         "Calling method via non-direct base class. This may accidentally skip "
         "a method defined in a direct base class");
    return;
  }

  const auto *ImplicitCast =
      Result.Nodes.getNodeAs<ImplicitCastExpr>("implicitCast");
  if (ImplicitCast->getCastKind() != CK_UncheckedDerivedToBase) {
    LLVM_DEBUG(llvm::dbgs()
               << "Skipping: cast is not UncheckedDerivedToBase (is: "
               << ImplicitCast->getCastKindName() << ")\n");
    return;
  }

  LLVM_DEBUG(llvm::dbgs() << "ICE type: "
                          << ImplicitCast->getType().getAsString() << "\n"
                          << ImplicitCast->getType());

  const PointerType *PT = ImplicitCast->getType()->getAs<PointerType>();
  if (!PT) {
    LLVM_DEBUG(llvm::dbgs() << "Skipping: ICE type is not a pointer type\n");
    return;
  }

  const TypedefType *TT = PT->getPointeeType()->getAs<TypedefType>();
  if (TT && TT->getDecl()->getName() == "base")
    return; // OK, call via base::

  if (llvm::DebugFlag) {
    llvm::dbgs() << (TT ? "is typedef\n" : "not typedef\n");
    if (TT)
      TT->getDecl()->dumpColor();
  }

  const TypeAliasDecl *BaseAlias = findBaseAlias(ThisType);
  if (BaseAlias) {
    CXXRecordDecl *RD = BaseAlias->getUnderlyingType()->getAsCXXRecordDecl();
    if (RD != CastToType) {
      diag(BaseAlias->getBeginLoc(),
           "'base' type alias found, but for different class");
      BaseAlias = nullptr;
    }
  }

  auto DB =
      diag(R.getBegin(), "Base method called without using 'base' type alias");

  SourceLocation TokenLoc = findLocationBeforeToken(
      Member->getBeginLoc(), tok::coloncolon, SM, LO, true);
  if (TokenLoc.isValid()) {
    DB << FixItHint::CreateReplacement(
        CharSourceRange::getCharRange(Member->getBeginLoc(), TokenLoc), "base");
  }

  // Try to add the 'base' type alias unless one is already present.
  if (!BaseAlias) {
    // Only works if there is a single base.
    if (ThisType->getNumBases() + ThisType->getNumVBases() == 1) {
      QualType BaseType;
      if (ThisType->getNumBases() == 1)
        BaseType = ThisType->bases_begin()->getType();
      else
        BaseType = ThisType->vbases_begin()->getType();

      SourceLocation TokenLoc = ThisType->getBraceRange().getBegin();
      if (TokenLoc.isValid()) {
        TokenLoc = TokenLoc.getLocWithOffset(1);
        llvm::SmallString<64> typeAliasString("\nusing base = ");
        typeAliasString += BaseType.getAsString();
        typeAliasString += ";\n";
        DB << FixItHint::CreateInsertion(TokenLoc, typeAliasString);
      }
    } else {
      diag(R.getBegin(),
           "No fix-it hint for base-alias due to multiple bases.\n");
      LLVM_DEBUG(llvm::dbgs()
                 << "No fix-it hint for base-alias due to multiple bases.\n");
    }
  }
}

} // namespace misc
} // namespace tidy
} // namespace clang
