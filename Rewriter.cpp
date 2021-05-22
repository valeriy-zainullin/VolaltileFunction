#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <sys/resource.h>

static const llvm::StringRef MAGIC_TYPE = "___VOLATILE_BLOCK_MARKER";

class BlockVolatilizingVisitor : public clang::RecursiveASTVisitor<BlockVolatilizingVisitor> {
public:
	BlockVolatilizingVisitor(
		llvm::StringRef inputFile,
		clang::SourceManager& sourceManager,
		std::map<std::string, clang::tooling::Replacements>& replacements,
		std::vector<std::string>& replacementContainer
	) : inputFile(inputFile)
		, sourceManager(sourceManager)
		, replacements(replacements)
		, replacementContainer(replacementContainer) {}

	bool TraverseUnaryExprOrTypeTraitExpr(clang::UnaryExprOrTypeTraitExpr* unaryExprOrTypeTraitExpr) {
		return true;
	}

	bool TraverseDeclRefExpr(clang::DeclRefExpr* declRefExpr) {
		clang::NamedDecl* namedDecl = declRefExpr->getFoundDecl();
		clang::VarDecl* varDecl = llvm::dyn_cast<clang::VarDecl>(namedDecl);
		if (varDecl == nullptr) {
			return clang::RecursiveASTVisitor<BlockVolatilizingVisitor>::TraverseDeclRefExpr(declRefExpr);
		}

		bool arrayType = false;
		std::string ptrType = varDecl->getType().getAsString();
		static const std::regex ARRAY_TYPE_REGEX("(.*) *\\[[0-9][0-9]*\\] *");
		static const std::regex VOLATILE_IN_BACK_REGEX("(.*)  *volatile *");
		static const std::regex VOLATILE_IN_FRONT_REGEX(" *volatile  *(.*)");
		static const std::regex PTR_TYPE_REGEX("(.*) *\\*");
		std::cmatch match;
		if (std::regex_match(ptrType.c_str(), match, ARRAY_TYPE_REGEX)) {
			arrayType = true;

			ptrType = std::string(match[1]);
			unsigned int ptrDepth = 1;

			bool markAsVolatile = true;
			if (
				std::regex_match(ptrType.c_str(), match, VOLATILE_IN_BACK_REGEX) ||
				(
					!std::regex_match(ptrType.c_str(), match, PTR_TYPE_REGEX) &&
					std::regex_match(ptrType.c_str(), match, VOLATILE_IN_FRONT_REGEX)
				)
			) {
				markAsVolatile = false;
			}

			while (std::regex_match(ptrType.c_str(), match, ARRAY_TYPE_REGEX)) {
				ptrType = std::string(match[1]);
				ptrDepth += 1;
			}

			for (unsigned int i = 0; i < ptrDepth - 1; ++i) {
				ptrType += "*";
			}

			if (markAsVolatile) {
				ptrType += " volatile ";
			}
			ptrType += "*";
		} else {
			ptrType += " volatile *";
		}

		clang::SourceLocation beginLocation = declRefExpr->getBeginLoc();
		std::string varName = declRefExpr->getNameInfo().getAsString();
		assert(varName.size() <= std::numeric_limits<unsigned int>::max());
		unsigned int length = static_cast<unsigned int>(varName.size());
		assert(length != 0);

		std::pair<clang::FileID, unsigned int> decomposedBeginLocation =
			sourceManager.getDecomposedExpansionLoc(sourceManager.getSpellingLoc(beginLocation));

		// std::lock_guard<std::mutex> lock(OUTPUT_STREAM_MUTEX);
		// llvm::outs() << decomposedBeginLocation.first.getHashValue() << ", " << decomposedEndLocation.first.getHashValue() << ", " << sourceManager.getMainFileID().getHashValue() << ".\n";

		// std::lock_guard<std::mutex> lock(OUTPUT_STREAM_MUTEX);
		// llvm::outs() << "(" << decomposedBeginLocation.second << ", " << length << ").\n";

		if (decomposedBeginLocation.first != sourceManager.getMainFileID()) {
			return clang::RecursiveASTVisitor<BlockVolatilizingVisitor>::TraverseDeclRefExpr(declRefExpr);
		}

		if (!arrayType) {
			replacementContainer.push_back("(* (" + ptrType + ") (&" + varName + "))");
		} else {
			replacementContainer.push_back("((" + ptrType + ") " + varName + ")");
		}
		llvm::Error error = replacements[inputFile].add(
			clang::tooling::Replacement(
				inputFile,
				decomposedBeginLocation.second,
				length,
				replacementContainer.back()
			)
		);
		if (error) {
			llvm::errs() << "Failed to insert a replacement. Location: " << inputFile << ":";
			llvm::errs() << decomposedBeginLocation.second << "..";
			llvm::errs() << decomposedBeginLocation.second + length - 1 << ".\n";
		}

		return true;
	}

private:
	std::string inputFile;
	clang::SourceManager& sourceManager;
	std::map<std::string, clang::tooling::Replacements>& replacements;
	std::vector<std::string>& replacementContainer;
};

template<typename T>
class BlockFindingVisitor : public clang::RecursiveASTVisitor<BlockFindingVisitor<T>> {
public:
	BlockFindingVisitor(T&& blockHandler)
		: blockHandler(std::move(blockHandler)) {}

	bool TraverseIfStmt(clang::IfStmt* ifStmt) {
		clang::Expr* conditionExpr = ifStmt->getCond();
		clang::CStyleCastExpr* castExpr = llvm::dyn_cast<clang::CStyleCastExpr>(conditionExpr);
		if (castExpr != nullptr && castExpr->getTypeAsWritten().getAsString() == MAGIC_TYPE) {
			return blockHandler.TraverseIfStmt(ifStmt);
		}

		return clang::RecursiveASTVisitor<BlockFindingVisitor<T>>::TraverseIfStmt(ifStmt);
	}

private:
	T blockHandler;
};

class ASTConsumer : public clang::ASTConsumer {
public:
	ASTConsumer(
		llvm::StringRef inputFile,
		clang::SourceManager& sourceManager,
		std::map<std::string, clang::tooling::Replacements>& replacements,
		std::vector<std::string>& replacementContainer
	) : blockFindingVisitor(
				BlockVolatilizingVisitor(inputFile, sourceManager, replacements, replacementContainer)
			) {}

	void HandleTranslationUnit(clang::ASTContext& context) override {
		blockFindingVisitor.TraverseDecl(context.getTranslationUnitDecl());
	}

private:
	BlockFindingVisitor<BlockVolatilizingVisitor> blockFindingVisitor;
};

class ASTFrontendAction : public clang::ASTFrontendAction {
public:
	ASTFrontendAction(
		std::map<std::string, clang::tooling::Replacements>& replacements,
		std::vector<std::string>& replacementContainer
	) : replacements(replacements)
		, replacementContainer(replacementContainer) {}

	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
		clang::CompilerInstance& compilerInstance,
		llvm::StringRef inputFile
	) {
		llvm::outs() << inputFile << '\n';
		return std::make_unique<ASTConsumer>(
			inputFile,
			compilerInstance.getSourceManager(),
			replacements,
			replacementContainer
		);
	}

private:
	std::map<std::string, clang::tooling::Replacements>& replacements;
	std::vector<std::string>& replacementContainer;
};

class FrontendActionFactory : public clang::tooling::FrontendActionFactory {
public:
	FrontendActionFactory(
		clang::tooling::RefactoringTool& refactoringTool,
		std::vector<std::string>& replacementContainer
	) : replacements(refactoringTool.getReplacements())
		, replacementContainer(replacementContainer) {}

	std::unique_ptr<clang::FrontendAction> create() override {
		return std::make_unique<ASTFrontendAction>(replacements, replacementContainer);
	}

private:
	std::map<std::string, clang::tooling::Replacements>& replacements;
	std::vector<std::string>& replacementContainer;
};

int main(int argc, const char** argv) {
	if (argc < 2) {
		llvm::errs() << "Usage: " << argv[0] << " <build directory with compilation database for clang> [files...]\n";
		return 1;
	}

	std::string compilationDatabaseLoadingErrorMessage;
	std::unique_ptr<clang::tooling::CompilationDatabase> compilationDatabasePtr =
		clang::tooling::CompilationDatabase::loadFromDirectory(
			argv[1],
			compilationDatabaseLoadingErrorMessage
		);
	if (!compilationDatabasePtr) {
		llvm::errs() << compilationDatabaseLoadingErrorMessage;
		return 2;
	}

	static const rlim_t CURRENT_STACK_SIZE = 256 * 1024 * 1024;
	static const rlim_t MAX_STACK_SIZE = static_cast<rlim_t>(2) * 1024 * 1024 * 1024;
	struct rlimit limit;
	limit.rlim_cur = CURRENT_STACK_SIZE;
	limit.rlim_max = MAX_STACK_SIZE;
	int queryResult = setrlimit(RLIMIT_STACK, &limit);
	if (queryResult != 0) {
		return 3;
	}

	std::vector<std::string> replacementContainer;
	clang::tooling::RefactoringTool refactoringTool(*compilationDatabasePtr, files);
	return refactoringTool.runAndSave(
		std::make_unique<FrontendActionFactory>(refactoringTool, replacementContainer).get()
	);
}
