#include "nixt/Flake.h"

using namespace nix;

namespace {

const char *FlakeCompat = R"(
src:

let

  lockFilePath = src + "/flake.lock";

  lockFile = builtins.fromJSON (builtins.readFile lockFilePath);

  fetchTree =
    info:
    if info.type == "github" then
      { outPath =
          fetchTarball
            ({ url = "https://api.${info.host or "github.com"}/repos/${info.owner}/${info.repo}/tarball/${info.rev}"; }
             // (if info ? narHash then { sha256 = info.narHash; } else {})
            );
        rev = info.rev;
        shortRev = builtins.substring 0 7 info.rev;
        lastModified = info.lastModified;
        lastModifiedDate = formatSecondsSinceEpoch info.lastModified;
        narHash = info.narHash;
      }
    else if info.type == "git" then
      { outPath =
          builtins.fetchGit
            ({ url = info.url; }
             // (if info ? rev then { inherit (info) rev; } else {})
             // (if info ? ref then { inherit (info) ref; } else {})
             // (if info ? submodules then { inherit (info) submodules; } else {})
            );
        lastModified = info.lastModified;
        lastModifiedDate = formatSecondsSinceEpoch info.lastModified;
        narHash = info.narHash;
      } // (if info ? rev then {
        rev = info.rev;
        shortRev = builtins.substring 0 7 info.rev;
      } else {
      })
    else if info.type == "path" then
      { outPath = builtins.path { path = info.path; };
        narHash = info.narHash;
      }
    else if info.type == "tarball" then
      { outPath =
          fetchTarball
            ({ inherit (info) url; }
             // (if info ? narHash then { sha256 = info.narHash; } else {})
            );
      }
    else if info.type == "gitlab" then
      { inherit (info) rev narHash lastModified;
        outPath =
          fetchTarball
            ({ url = "https://${info.host or "gitlab.com"}/api/v4/projects/${info.owner}%2F${info.repo}/repository/archive.tar.gz?sha=${info.rev}"; }
             // (if info ? narHash then { sha256 = info.narHash; } else {})
            );
        shortRev = builtins.substring 0 7 info.rev;
      }
    else if info.type == "sourcehut" then
      { inherit (info) rev narHash lastModified;
        outPath =
          fetchTarball
            ({ url = "https://${info.host or "git.sr.ht"}/${info.owner}/${info.repo}/archive/${info.rev}.tar.gz"; }
             // (if info ? narHash then { sha256 = info.narHash; } else {})
            );
        shortRev = builtins.substring 0 7 info.rev;
      }
    else
      # FIXME: add Mercurial, tarball inputs.
      throw "flake input has unsupported input type '${info.type}'";

  callFlake4 = flakeSrc: locks:
    let
      flake = import (flakeSrc + "/flake.nix");

      inputs = builtins.mapAttrs (n: v:
        if v.flake or true
        then callFlake4 (fetchTree (v.locked // v.info)) v.inputs
        else fetchTree (v.locked // v.info)) locks;

      outputs = flakeSrc // (flake.outputs (inputs // {self = outputs;}));
    in
      assert flake.edition == 201909;
      outputs;

  callLocklessFlake = flakeSrc:
    let
      flake = import (flakeSrc + "/flake.nix");
      outputs = flakeSrc // (flake.outputs ({ self = outputs; }));
    in outputs;

  rootSrc = let
    addOutPath = src: { outPath = src; };
  in
    { lastModified = 0; lastModifiedDate = formatSecondsSinceEpoch 0; }
      // (addOutPath src);

  # Format number of seconds in the Unix epoch as %Y%m%d%H%M%S.
  formatSecondsSinceEpoch = t:
    let
      rem = x: y: x - x / y * y;
      days = t / 86400;
      secondsInDay = rem t 86400;
      hours = secondsInDay / 3600;
      minutes = (rem secondsInDay 3600) / 60;
      seconds = rem t 60;

      # Courtesy of https://stackoverflow.com/a/32158604.
      z = days + 719468;
      era = (if z >= 0 then z else z - 146096) / 146097;
      doe = z - era * 146097;
      yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
      y = yoe + era * 400;
      doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
      mp = (5 * doy + 2) / 153;
      d = doy - (153 * mp + 2) / 5 + 1;
      m = mp + (if mp < 10 then 3 else -9);
      y' = y + (if m <= 2 then 1 else 0);

      pad = s: if builtins.stringLength s < 2 then "0" + s else s;
    in "${toString y'}${pad (toString m)}${pad (toString d)}${pad (toString hours)}${pad (toString minutes)}${pad (toString seconds)}";

  allNodes =
    builtins.mapAttrs
      (key: node:
        let
          sourceInfo =
            if key == lockFile.root
            then rootSrc
            else fetchTree (node.info or {} // removeAttrs node.locked ["dir"]);

          subdir = if key == lockFile.root then "" else node.locked.dir or "";

          outPath = sourceInfo + ((if subdir == "" then "" else "/") + subdir);

          flake = import (outPath + "/flake.nix");

          inputs = builtins.mapAttrs
            (inputName: inputSpec: allNodes.${resolveInput inputSpec})
            (node.inputs or {});

          # Resolve a input spec into a node name. An input spec is
          # either a node name, or a 'follows' path from the root
          # node.
          resolveInput = inputSpec:
              if builtins.isList inputSpec
              then getInputByPath lockFile.root inputSpec
              else inputSpec;

          # Follow an input path (e.g. ["dwarffs" "nixpkgs"]) from the
          # root node, returning the final node.
          getInputByPath = nodeName: path:
            if path == []
            then nodeName
            else
              getInputByPath
                # Since this could be a 'follows' input, call resolveInput.
                (resolveInput lockFile.nodes.${nodeName}.inputs.${builtins.head path})
                (builtins.tail path);

          outputs = flake.outputs (inputs // { self = result; });

          result =
            outputs
            # We add the sourceInfo attribute for its metadata, as they are
            # relevant metadata for the flake. However, the outPath of the
            # sourceInfo does not necessarily match the outPath of the flake,
            # as the flake may be in a subdirectory of a source.
            # This is shadowed in the next //
            // sourceInfo
            // {
              # This shadows the sourceInfo.outPath
              inherit outPath;

              inherit inputs; inherit outputs; inherit sourceInfo; _type = "flake";
            };

        in
          if node.flake or true then
            assert builtins.isFunction flake.outputs;
            result
          else
            sourceInfo
      )
      lockFile.nodes;

  result =
    if !(builtins.pathExists lockFilePath)
    then callLocklessFlake rootSrc
    else if lockFile.version == 4
    then callFlake4 rootSrc (lockFile.inputs)
    else if lockFile.version >= 5 && lockFile.version <= 7
    then allNodes.${lockFile.root}
    else throw "lock file '${lockFilePath}' has unsupported version ${toString lockFile.version}";

in
  result
)";

} // namespace

void nixt::callDirtyFlake(EvalState &State, std::string_view Src,
                          nix::Value &VRes) {

  nix::Value *VSrc = State.allocValue();
  VSrc->mkPath(State.rootPath(nix::CanonPath(Src, nix::CanonPath::fromCwd())));

  auto *VFlakeCompat = State.allocValue();

  nix::Expr *EFlakeCompat = State.parseExprFromString(
      FlakeCompat, State.rootPath(nix::CanonPath::fromCwd()));
  State.eval(EFlakeCompat, *VFlakeCompat);

  State.callFunction(*VFlakeCompat, *VSrc, VRes, noPos);
}
