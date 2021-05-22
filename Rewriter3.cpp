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
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

static const llvm::StringRef MAGIC_TYPE = "___VOLATILE_BLOCK_MARKER";

static std::vector<std::string> strings; // Yes, yes, bad, but... I'm done passing references through those three (or more) classes.

class FunctionMarkerVisitor : public clang::RecursiveASTVisitor<FunctionMarkerVisitor> {
public:
	FunctionMarkerVisitor(
		std::set<std::pair<std::string, unsigned int>>& positionsInsertedAt,
		clang::SourceManager& sourceManager,
		std::map<std::string, clang::tooling::Replacements>& replacements
	) : positionsInsertedAt(positionsInsertedAt)
	  , sourceManager(sourceManager)
	  , replacements(replacements)
	{}

	bool TraverseIfStmt(clang::IfStmt* ifStmt) {
		{
			clang::Expr* conditionExpr = ifStmt->getCond();
			clang::CStyleCastExpr* castExpr = llvm::dyn_cast<clang::CStyleCastExpr>(conditionExpr);
			if (castExpr != nullptr && castExpr->getTypeAsWritten().getAsString() == MAGIC_TYPE) {
				volatilizationRequested = true;
			}
		}

		return clang::RecursiveASTVisitor<FunctionMarkerVisitor>::TraverseIfStmt(ifStmt);
	}

	bool TraverseOptimizeNoneAttr(clang::OptimizeNoneAttr* optimizeNoneAttr) {
		alreadyVolatilized = true;
		return
			clang::RecursiveASTVisitor<FunctionMarkerVisitor>::TraverseOptimizeNoneAttr(optimizeNoneAttr);
	}

	bool TraverseFunctionDecl(clang::FunctionDecl* functionDecl) {
		if (!functionDecl->doesThisDeclarationHaveABody()) {
			return clang::RecursiveASTVisitor<FunctionMarkerVisitor>::TraverseFunctionDecl(functionDecl);
		}
		volatilizationRequested = false;
		alreadyVolatilized = false;
		bool returnValue =
			clang::RecursiveASTVisitor<FunctionMarkerVisitor>::TraverseFunctionDecl(functionDecl);
		if (volatilizationRequested && !alreadyVolatilized) {
			clang::SourceLocation beginLocation = functionDecl->getBeginLoc();
			std::pair<clang::FileID, unsigned int> decomposedBeginLocation =
				sourceManager.getDecomposedExpansionLoc(beginLocation);
			llvm::StringRef fileNameRef = sourceManager.getFileManager().getCanonicalName(
				sourceManager.getFileEntryForID(decomposedBeginLocation.first)
			);
			strings.push_back(std::string(fileNameRef));
			{
				std::pair<std::string, unsigned int> position(
					strings.back(),
					decomposedBeginLocation.second
				);
				if (positionsInsertedAt.find(position) != positionsInsertedAt.end()) {
					return returnValue;
				}
				positionsInsertedAt.insert(position);
			}
			llvm::Error error = replacements[strings.back()].add(
				clang::tooling::Replacement(
					strings.back(),
					decomposedBeginLocation.second,
					0,
					"__attribute__((optnone)) "
				)
			);
			llvm::outs() << functionDecl->getNameInfo().getAsString() << '@';
			llvm::outs() << fileNameRef << ':' << decomposedBeginLocation.second << '\n';
			if (error) {
				llvm::errs() << "Failed to insert a replacement. Location: " << fileNameRef << "+";
				llvm::errs() << decomposedBeginLocation.second << ". " << llvm::toString(std::move(error));
				llvm::errs() << '\n';
			}
		}
		return returnValue;
	}

private:
	std::set<std::pair<std::string, unsigned int>>& positionsInsertedAt;
	clang::SourceManager& sourceManager;
	std::map<std::string, clang::tooling::Replacements>& replacements;
	bool volatilizationRequested;
	bool alreadyVolatilized;
};

class ASTConsumer : public clang::ASTConsumer {
public:
	ASTConsumer(
		std::set<std::pair<std::string, unsigned int>>& positionsInsertedTo,
		clang::SourceManager& sourceManager,
		std::map<std::string, clang::tooling::Replacements>& replacements
	) : functionMarkerVisitor(positionsInsertedTo, sourceManager, replacements) {}

	void HandleTranslationUnit(clang::ASTContext& context) override {
		functionMarkerVisitor.TraverseDecl(context.getTranslationUnitDecl());
	}

private:
	FunctionMarkerVisitor functionMarkerVisitor;
};

/*
class NullASTConsumer : public clang::ASTConsumer {
public:
	void HandleTranslationUnit(clang::ASTContext& context) override {}
};
*/

class ASTFrontendAction : public clang::ASTFrontendAction {
public:
	ASTFrontendAction(
		std::set<std::pair<std::string, unsigned int>>& positionsInsertedAt,
		// std::set<std::string>& processedFiles,
		std::map<std::string, clang::tooling::Replacements>& replacements
	) : positionsInsertedAt(positionsInsertedAt)
	  //, processedFiles(processedFiles)
	  , replacements(replacements)
	{}

	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
		clang::CompilerInstance& compilerInstance,
		llvm::StringRef inputFile
	) {
		/*
		std::string inputFileAsStr(inputFile);
		if (processedFiles.find(inputFileAsStr) != processedFiles.end()) {
			return std::make_unique<NullASTConsumer>();
		}
		processedFiles.insert(inputFileAsStr);
		*/

		llvm::outs() << inputFile << '\n';
		return std::make_unique<ASTConsumer>(
			positionsInsertedAt,
			compilerInstance.getSourceManager(),
			replacements
		);
	}

private:
	std::set<std::pair<std::string, unsigned int>>& positionsInsertedAt;
	// std::set<std::string>& processedFiles;
	std::map<std::string, clang::tooling::Replacements>& replacements;
};

class FrontendActionFactory : public clang::tooling::FrontendActionFactory {
public:
	FrontendActionFactory(
		clang::tooling::RefactoringTool& refactoringTool,
		std::set<std::pair<std::string, unsigned int>>& positionsInsertedAt//,
		//std::set<std::string>& processedFiles
	) : positionsInsertedAt(positionsInsertedAt)
	  //, processedFiles(processedFiles)
	  , replacements(refactoringTool.getReplacements())
	{}

	std::unique_ptr<clang::FrontendAction> create() override {
		return std::make_unique<ASTFrontendAction>(positionsInsertedAt, /*processedFiles, */replacements);
	}

private:
	std::set<std::pair<std::string, unsigned int>>& positionsInsertedAt;
	//std::set<std::string>& processedFiles;
	std::map<std::string, clang::tooling::Replacements>& replacements;
};

std::vector<std::string> selectCFiles(const std::vector<std::string>& originalFileList) {
	std::vector<std::string> result;
	for (const std::string& file: originalFileList) {
		if (file.size() >= 2 && file[file.size() - 2] == '.' && file[file.size() - 1] == 'c') {
			result.push_back(file);
		}
	}
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

int main(int argc, const char** argv) {
	if (argc != 3) {//(argc != 2) {//argc < 2) {
		llvm::errs() << "Usage: " << argv[0];
		llvm::errs() << " <build directory with compilation database for clang>\n";
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

	/*
	std::vector<std::string> files(argv + 2, argv + argc);
	files = selectCFiles(files);
	*/
	// std::vector<std::string> files = selectCFiles(std::vector<std::string>(argv + 2, argv + argc));
	std::vector<std::string> files = {argv[2]};
	std::set<std::pair<std::string, unsigned int>> positionsInsertedAt;
	// std::set<std::string> processedFiles;
	clang::tooling::RefactoringTool refactoringTool(*compilationDatabasePtr, files);
	return refactoringTool.runAndSave(
		std::make_unique<FrontendActionFactory>(
			refactoringTool,
			positionsInsertedAt//,
			// processedFiles
		).get()
	);
}
