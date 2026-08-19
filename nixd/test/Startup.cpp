#include "nixd/Controller/Startup.h"
#include "nixd/Support/JSON.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

using namespace nixd;
using namespace lspserver;

namespace {

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    char Pattern[] = "/private/tmp/codex-nixd-startup-XXXXXX";
    char *Created = mkdtemp(Pattern);
    EXPECT_NE(Created, nullptr);
    if (Created)
      Path = Created;
  }

  ~TemporaryDirectory() {
    std::error_code EC;
    std::filesystem::remove_all(Path, EC);
  }

  std::filesystem::path path() const { return Path; }

private:
  std::filesystem::path Path;
};

void writeFile(const std::filesystem::path &Path, llvm::StringRef Contents) {
  std::ofstream Stream(Path);
  ASSERT_TRUE(Stream.good());
  Stream << Contents.str();
  ASSERT_TRUE(Stream.good());
}

URIForFile fileURI(const std::filesystem::path &Path) {
  return URIForFile::canonicalize(Path.string(), "");
}

WorkspaceFolder workspaceFolder(const std::filesystem::path &Path,
                                std::string Name = "workspace") {
  return {.uri = fileURI(Path), .name = std::move(Name)};
}

Configuration baseConfiguration() {
  Configuration Base = defaultConfiguration();
  Base.formatting.command = {"launch-format"};
  Base.nixpkgs.expr = "launch-nixpkgs";
  return Base;
}

CommandLineConfiguration commandLine(bool EnableProjectConfig = true,
                                     bool ConfigSpecified = false) {
  return {
      .baseConfiguration = baseConfiguration(),
      .configSpecified = ConfigSpecified,
      .enableProjectConfig = EnableProjectConfig,
  };
}

void writeProject(const std::filesystem::path &Root, llvm::StringRef Marker) {
  writeFile(Root / ".nixd.json", (R"json({"formatting":{"command":[")json" +
                                  Marker + R"json("]}})json")
                                     .str());
}

InitializeParams parseInitialize(llvm::json::Value Value) {
  InitializeParams Params;
  llvm::json::Path::Root Path;
  EXPECT_TRUE(lspserver::fromJSON(Value, Params, Path));
  return Params;
}

TEST(InitializeParams, ParsesStandardWorkspaceFolders) {
  const auto Value = parse(R"json({
    "processId": 7,
    "capabilities": {},
    "workspaceFolders": [
      {"uri": "file:///tmp/one", "name": "one"},
      {"uri": "file:///tmp/two", "name": "two"}
    ]
  })json");
  InitializeParams Params;
  llvm::json::Path::Root Path;

  ASSERT_TRUE(lspserver::fromJSON(Value, Params, Path))
      << llvm::toString(Path.getError());
  ASSERT_TRUE(Params.workspaceFolders);
  ASSERT_EQ(Params.workspaceFolders->size(), 2U);
  EXPECT_EQ((*Params.workspaceFolders)[0].name, "one");
  EXPECT_EQ((*Params.workspaceFolders)[0].uri.file(), "/tmp/one");
  EXPECT_EQ((*Params.workspaceFolders)[1].name, "two");
  EXPECT_EQ((*Params.workspaceFolders)[1].uri.file(), "/tmp/two");
}

TEST(StartupSelection, MultipleWorkspaceFoldersRefuseProjectLoadingFirst) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto RootURI = Temp.path() / "root-uri";
  const auto Other = Temp.path() / "other";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(RootURI);
  std::filesystem::create_directories(Other);
  writeProject(RootURI, "root-uri-format");

  InitializeParams Params;
  Params.rootUri = fileURI(RootURI);
  Params.workspaceFolders =
      std::vector{workspaceFolder(RootURI), workspaceFolder(Other)};

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"launch-format"}));
  EXPECT_EQ(Selected.executionCWD, Launch);
  ASSERT_TRUE(Selected.warning);
  EXPECT_NE(Selected.warning->find("multiple workspace folders"),
            std::string::npos);
  EXPECT_NE(Selected.warning->find("2"), std::string::npos);
}

TEST(StartupSelection, RootPrecedenceSelectsExactlyOneSource) {
  struct Case {
    bool RootURI;
    bool RootPath;
    bool WorkspaceFolder;
    const char *Expected;
  };
  const Case Cases[] = {
      {.RootURI = true,
       .RootPath = true,
       .WorkspaceFolder = true,
       .Expected = "root-uri"},
      {.RootURI = false,
       .RootPath = true,
       .WorkspaceFolder = true,
       .Expected = "root-path"},
      {.RootURI = false,
       .RootPath = false,
       .WorkspaceFolder = true,
       .Expected = "workspace-folder"},
      {.RootURI = false,
       .RootPath = false,
       .WorkspaceFolder = false,
       .Expected = "launch"},
  };

  for (const Case &TestCase : Cases) {
    TemporaryDirectory Temp;
    const auto Launch = Temp.path() / "launch";
    const auto RootURI = Temp.path() / "root-uri";
    const auto RootPath = Temp.path() / "root-path";
    const auto Folder = Temp.path() / "workspace-folder";
    for (const auto &Path : {Launch, RootURI, RootPath, Folder}) {
      std::filesystem::create_directories(Path);
      writeProject(Path, Path.filename().string());
    }

    InitializeParams Params;
    if (TestCase.RootURI)
      Params.rootUri = fileURI(RootURI);
    if (TestCase.RootPath)
      Params.rootPath = RootPath.string();
    if (TestCase.WorkspaceFolder)
      Params.workspaceFolders = std::vector{workspaceFolder(Folder)};

    const StartupSelection Selected =
        selectStartup(commandLine(), Params, Launch);

    EXPECT_EQ(Selected.baseConfiguration.formatting.command,
              std::vector<std::string>({TestCase.Expected}));
    EXPECT_EQ(Selected.executionCWD, Temp.path() / TestCase.Expected);
    EXPECT_FALSE(Selected.warning);
  }
}

TEST(StartupSelection, EmptyRootPathUsesTheSoleWorkspaceFolder) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Folder = Temp.path() / "folder";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(Folder);
  writeProject(Folder, "folder-format");

  InitializeParams Params;
  Params.rootPath = "";
  Params.workspaceFolders = std::vector{workspaceFolder(Folder)};

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"folder-format"}));
  EXPECT_EQ(Selected.executionCWD, Folder);
  EXPECT_FALSE(Selected.warning);
}

TEST(StartupSelection, ValidRootURIIgnoresMalformedWorkspaceFolders) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Root = Temp.path() / "root-uri";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(Root);
  writeProject(Root, "root-uri-format");

  const InitializeParams Params = parseInitialize(llvm::json::Object{
      {"capabilities", llvm::json::Object{}},
      {"rootUri", fileURI(Root).uri()},
      {"workspaceFolders", 17},
  });

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"root-uri-format"}));
  EXPECT_EQ(Selected.executionCWD, Root);
  EXPECT_FALSE(Selected.warning);
}

TEST(StartupSelection, ValidRootURIIgnoresMalformedRootPath) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Root = Temp.path() / "root-uri";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(Root);
  writeProject(Root, "root-uri-format");

  const InitializeParams Params = parseInitialize(llvm::json::Object{
      {"capabilities", llvm::json::Object{}},
      {"rootUri", fileURI(Root).uri()},
      {"rootPath", true},
  });

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"root-uri-format"}));
  EXPECT_EQ(Selected.executionCWD, Root);
  EXPECT_FALSE(Selected.warning);
}

TEST(StartupSelection, ValidRootPathIgnoresMalformedWorkspaceFolders) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Root = Temp.path() / "root-path";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(Root);
  writeProject(Root, "root-path-format");

  const InitializeParams Params = parseInitialize(llvm::json::Object{
      {"capabilities", llvm::json::Object{}},
      {"rootPath", Root.string()},
      {"workspaceFolders", 17},
  });

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"root-path-format"}));
  EXPECT_EQ(Selected.executionCWD, Root);
  EXPECT_FALSE(Selected.warning);
}

TEST(StartupSelection,
     RawMultipleWorkspaceFoldersRefuseBeforeRootURIDespiteMalformedEntry) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Root = Temp.path() / "root-uri";
  const auto Folder = Temp.path() / "folder";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(Root);
  std::filesystem::create_directories(Folder);
  writeProject(Root, "must-not-load");

  const InitializeParams Params = parseInitialize(llvm::json::Object{
      {"capabilities", llvm::json::Object{}},
      {"rootUri", fileURI(Root).uri()},
      {"workspaceFolders",
       llvm::json::Array{
           llvm::json::Object{{"uri", fileURI(Folder).uri()},
                              {"name", "valid"}},
           llvm::json::Object{{"uri", 42}, {"name", "malformed"}},
       }},
  });

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"launch-format"}));
  EXPECT_EQ(Selected.executionCWD, Launch);
  ASSERT_TRUE(Selected.warning);
  EXPECT_NE(Selected.warning->find("multiple workspace folders"),
            std::string::npos);
  EXPECT_NE(Selected.warning->find("2"), std::string::npos);
}

TEST(StartupSelection, InvalidRootURIDoesNotFallThroughToRootPath) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto RootPath = Temp.path() / "root-path";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(RootPath);
  writeProject(RootPath, "must-not-load");

  const auto Value = parse((
      R"json({"capabilities":{},"rootUri":"https://example.test/root","rootPath":")json" +
      RootPath.string() + R"json("})json"));
  InitializeParams Params;
  llvm::json::Path::Root Path;
  ASSERT_TRUE(lspserver::fromJSON(Value, Params, Path));

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"launch-format"}));
  EXPECT_EQ(Selected.executionCWD, Launch);
  ASSERT_TRUE(Selected.warning);
  EXPECT_NE(Selected.warning->find("rootUri"), std::string::npos);
  EXPECT_NE(Selected.warning->find("file"), std::string::npos);
}

TEST(StartupSelection, UnusableSelectedRootDoesNotFallThrough) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Missing = Temp.path() / "missing-root";
  std::filesystem::create_directories(Launch);
  writeProject(Launch, "must-not-load");

  InitializeParams Params;
  Params.rootUri = fileURI(Missing);

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"launch-format"}));
  EXPECT_EQ(Selected.executionCWD, Launch);
  ASSERT_TRUE(Selected.warning);
  EXPECT_NE(Selected.warning->find("rootUri"), std::string::npos);
  EXPECT_NE(Selected.warning->find(Missing.string()), std::string::npos);
}

TEST(StartupSelection, ProjectInspectionRequiresOptInAndNoCLIConfig) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Missing = Temp.path() / "missing-root";
  std::filesystem::create_directories(Launch);

  InitializeParams Params;
  Params.rootUri = fileURI(Missing);

  for (const CommandLineConfiguration &CLI :
       {commandLine(false, false), commandLine(true, true)}) {
    const StartupSelection Selected = selectStartup(CLI, Params, Launch);
    EXPECT_EQ(Selected.baseConfiguration.formatting.command,
              std::vector<std::string>({"launch-format"}));
    EXPECT_EQ(Selected.executionCWD, Launch);
    EXPECT_FALSE(Selected.warning);
  }
}

TEST(StartupSelection, MissingProjectFileIsSilentAndKeepsLaunchCWD) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Root = Temp.path() / "root";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(Root);

  InitializeParams Params;
  Params.rootUri = fileURI(Root);

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"launch-format"}));
  EXPECT_EQ(Selected.executionCWD, Launch);
  EXPECT_FALSE(Selected.warning);
}

TEST(StartupSelection, ValidEmptyProjectFileSelectsAndRebases) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Root = Temp.path() / "root";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(Root);
  writeFile(Root / ".nixd.json", "{}");

  InitializeParams Params;
  Params.rootUri = fileURI(Root);

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"launch-format"}));
  EXPECT_EQ(Selected.executionCWD, Root);
  EXPECT_FALSE(Selected.warning);
}

TEST(StartupSelection, InvalidProjectFileWarnsAndKeepsLaunchBase) {
  TemporaryDirectory Temp;
  const auto Launch = Temp.path() / "launch";
  const auto Root = Temp.path() / "root";
  std::filesystem::create_directories(Launch);
  std::filesystem::create_directories(Root);
  writeFile(Root / ".nixd.json", "{");

  InitializeParams Params;
  Params.rootUri = fileURI(Root);

  const StartupSelection Selected =
      selectStartup(commandLine(), Params, Launch);

  EXPECT_EQ(Selected.baseConfiguration.formatting.command,
            std::vector<std::string>({"launch-format"}));
  EXPECT_EQ(Selected.executionCWD, Launch);
  ASSERT_TRUE(Selected.warning);
  EXPECT_NE(Selected.warning->find((Root / ".nixd.json").string()),
            std::string::npos);
  EXPECT_NE(Selected.warning->find("JSON result cannot be parsed"),
            std::string::npos);
}

} // namespace
