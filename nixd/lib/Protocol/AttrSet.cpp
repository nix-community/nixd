#include "nixd/Protocol/AttrSet.h"

using namespace nixd;
using namespace llvm::json;

Value nixd::toJSON(const PackageDescription &Params) {
  return Object{
      {"Name", Params.Name},
      {"PName", Params.PName},
      {"Version", Params.Version},
      {"Description", Params.Description},
      {"LongDescription", Params.LongDescription},
      {"Position", Params.Position},
      {"Homepage", Params.Homepage},
  };
}

bool nixd::fromJSON(const llvm::json::Value &Params, PackageDescription &R,
                    llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O                                              //
         && O.map("Name", R.Name)                       //
         && O.map("PName", R.PName)                     //
         && O.map("Version", R.Version)                 //
         && O.map("Description", R.Description)         //
         && O.map("LongDescription", R.LongDescription) //
         && O.map("Position", R.Position)               //
         && O.map("Homepage", R.Homepage)               //
      ;
}

Value nixd::toJSON(const AttrPathCompleteParams &Params) {
  return Object{{"Scope", Params.Scope}, {"Prefix", Params.Prefix}};
}
bool nixd::fromJSON(const llvm::json::Value &Params, AttrPathCompleteParams &R,
                    llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O                            //
         && O.map("Scope", R.Scope)   //
         && O.map("Prefix", R.Prefix) //
      ;
}
