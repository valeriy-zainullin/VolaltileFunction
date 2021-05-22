cmake_minimum_required(VERSION 3.13.4)

project(VolatilizeBlocks)

set(CMAKE_CXX_STANDARD 17)

add_executable(Rewriter3 Rewriter3.cpp)
target_include_directories(Rewriter3 PRIVATE ${LLVM_ROOT}/include)
target_link_directories(Rewriter3 PRIVATE ${LLVM_ROOT}/lib)
target_link_libraries(Rewriter3 PRIVATE
	clangFrontend
	clangParse
	LLVMMC
	LLVMMCParser
	LLVMBitstreamReader
	LLVMProfileData
	clangSerialization
	clangSema
	clangEdit
	clangLex
	clangAnalysis
	LLVMFrontendOpenMP
	clangAST
	clangBasic
	LLVMCore
	LLVMBinaryFormat
	LLVMRemarks
	clangTooling
	LLVMOption
	clangDriver
	clangToolingCore
	clangRewrite
	clangFormat
	clangToolingInclusions
	LLVMSupport
	LLVMDemangle
	pthread
	ncurses
	z
)

add_executable(Runner Runner.cpp)
target_include_directories(Runner PRIVATE ${LLVM_ROOT}/include)
target_link_directories(Runner PRIVATE ${LLVM_ROOT}/lib)
target_link_libraries(Runner PRIVATE
	clangFrontend
	clangParse
	LLVMMC
	LLVMMCParser
	LLVMBitstreamReader
	LLVMProfileData
	clangSerialization
	clangSema
	clangEdit
	clangLex
	clangAnalysis
	LLVMFrontendOpenMP
	clangAST
	clangBasic
	LLVMCore
	LLVMBinaryFormat
	LLVMRemarks
	clangTooling
	LLVMOption
	clangDriver
	clangToolingCore
	clangRewrite
	clangFormat
	clangToolingInclusions
	LLVMSupport
	LLVMDemangle
	pthread
	ncurses
	z
)
