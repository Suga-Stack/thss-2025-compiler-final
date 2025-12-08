#include <iostream>
#include <fstream>
#include <memory>

#include "CodeGenVisitor.h"

#include "antlr4-runtime.h"
#include "SysYLexer.h"
#include "SysYParser.h"

int main(int argc, const char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: ./compiler <input-file> <output-file>\n";
    return 1;
  }

  const char *infile = argv[1];
  const char *outfile = argv[2];

  std::ifstream ifs(infile);
  if (!ifs) {
    std::cerr << "Cannot open input file: " << infile << std::endl;
    return 1;
  }
  std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  antlr4::ANTLRInputStream input(code);
  SysYLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  SysYParser parser(&tokens);

  antlr4::tree::ParseTree *tree = parser.compUnit();

  CodeGenVisitor codegen;
  codegen.run(dynamic_cast<antlr4::ParserRuleContext*>(tree));

  Module *m = codegen.getModule();
  if (!m) {
    std::cerr << "Codegen failed, no module produced\n";
    return 1;
  }

  std::ofstream ofs(outfile);
  if (!ofs) {
    std::cerr << "Cannot open output file: " << outfile << std::endl;
    return 1;
  }
  ofs << m->toString();
  std::cout << "Wrote IR to " << outfile << std::endl;
  return 0;
}