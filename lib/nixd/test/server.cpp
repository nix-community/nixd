#include "nixd/Server.h"
#include "lspserver/Protocol.h"

#include <gtest/gtest.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>

#include <nix/eval.hh>
#include <nix/shared.hh>

#include <filesystem>
#include <memory>

class ServerTest {
  std::unique_ptr<nixd::Server> Server;
  std::string O;
  llvm::raw_string_ostream Out;

public:
  /// Do not exceed the lifetime of the object
  nixd::Server &getServer() { return *Server; }
  llvm::StringRef getBuffer() { return O; }

  ServerTest() : Out(O) {
    nix::initNix();
    nix::initGC();

    Server = std::make_unique<nixd::Server>(
        std::make_unique<lspserver::InboundPort>(),
        std::make_unique<lspserver::OutboundPort>(Out));
  }
};

static const char *ParseErrorNix =
    R"(
{
  a = 1;
  foo

)";

static lspserver::URIForFile getDummyUri() {
  auto CurrentAbsolute =
      std::filesystem::absolute(std::filesystem::path(".")).string();
  return lspserver::URIForFile::canonicalize(CurrentAbsolute, CurrentAbsolute);
}

TEST(Server, Diagnostic) {
  ServerTest S;
  auto &Server = S.getServer();
  Server.publishStandloneDiagnostic(getDummyUri(), ParseErrorNix, 1);
  llvm::StringRef SRO = S.getBuffer();
  ASSERT_TRUE(SRO.contains(
      R"("message":"syntax error, unexpected end of file, expecting '.' or '='")"));
  ASSERT_TRUE(SRO.contains(R"("method":"textDocument/publishDiagnostics")"));
  ASSERT_TRUE(SRO.contains(R"({"end":{"character":5,"line":3})"));
  ASSERT_TRUE(SRO.starts_with("Content-Length:"));
}

static const char *ParseCorretNix =
    R"(
{
  a = 1;
  b = 2;
}
)";

TEST(Server, DiagnosticCorrect) {
  ServerTest S;
  auto &Server = S.getServer();
  Server.publishStandloneDiagnostic(getDummyUri(), ParseCorretNix, 1);
  llvm::StringRef SRO = S.getBuffer();
  ASSERT_FALSE(SRO.contains("message"));
  ASSERT_TRUE(SRO.contains(R"("diagnostics":[])"));
  ASSERT_TRUE(SRO.starts_with("Content-Length:"));
}

static const char *SemaUndefineVariableNix =
    R"(
# "foo" is undefined
with whatEverUndefined; [

]
)";

TEST(Server, DiagnosticSemaUndefineVariable) {
  ServerTest S;
  auto &Server = S.getServer();
  Server.publishStandloneDiagnostic(getDummyUri(), SemaUndefineVariableNix, 1);
  llvm::StringRef SRO = S.getBuffer();
  std::cout << SRO.str() << "\n";
  ASSERT_TRUE(SRO.contains("undefined variable"));
  // TODO: Strip ANSI colors here, or report upstream
  //   ASSERT_TRUE(SRO.contains(R"("whatEverUndefined)"));
  ASSERT_TRUE(SRO.starts_with("Content-Length:"));
}
