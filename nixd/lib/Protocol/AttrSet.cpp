#include "nixd/Protocol/AttrSet.h"

using namespace nixd;
using namespace llvm::json;

Value nixd::toJSON(const OptionType &Params) {
  return Object{
      {"Description", Params.Description},
      {"Name", Params.Name},
  };
}

bool nixd::fromJSON(const Value &Params, OptionType &R, Path P) {
  ObjectMapper O(Params, P);
  return O                                              //
         && O.mapOptional("Description", R.Description) //
         && O.mapOptional("Name", R.Name);
}

Value nixd::toJSON(const OptionDescription &Params) {
  return Object{
      {"Description", Params.Description},
      {"Declarations", Params.Declarations},
      {"Definitions", Params.Definitions},
      {"Example", Params.Example},
      {"Type", Params.Type},
  };
}
bool nixd::fromJSON(const Value &Params, OptionDescription &R, Path P) {
  ObjectMapper O(Params, P);
  return O                                                //
         && O.mapOptional("Description", R.Description)   //
         && O.mapOptional("Declarations", R.Declarations) //
         && O.mapOptional("Definitions", R.Definitions)   //
         && O.mapOptional("Example", R.Example)           //
         && O.mapOptional("Type", R.Type)                 //
      ;
}

Value nixd::toJSON(const OptionField &Params) {
  return Object{
      {"Name", Params.Name},
      {"Description", Params.Description},
  };
}
bool nixd::fromJSON(const Value &Params, OptionField &R, Path P) {
  ObjectMapper O(Params, P);
  return O                                              //
         && O.mapOptional("Description", R.Description) //
         && O.mapOptional("Name", R.Name)               //
      ;
}

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

Value nixd::toJSON(const ValueMeta &Params) {
  return Object{
      {"Type", Params.Type},
      {"Location", Params.Location},
  };
}

bool nixd::fromJSON(const llvm::json::Value &Params, ValueMeta &R,
                    llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O                                        //
         && O.map("Type", R.Type)                 //
         && O.mapOptional("Location", R.Location) //
      ;
}

Value nixd::toJSON(const AttrPathInfoResponse &Params) {
  return Object{
      {"Meta", Params.Meta},
      {"PackageDesc", Params.PackageDesc},
  };
}

bool nixd::fromJSON(const llvm::json::Value &Params, AttrPathInfoResponse &R,
                    llvm::json::Path P) {
  ObjectMapper O(Params, P);
  return O                                              //
         && O.map("Meta", R.Meta)                       //
         && O.mapOptional("PackageDesc", R.PackageDesc) //
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

llvm::json::Value nixd::toJSON(const BuiltinDescription &Params) {
  return Object{
      {"arity", Params.Arity},
      {"doc", Params.Doc},
  };
}
bool nixd::fromJSON(const llvm::json::Value &Params, BuiltinDescription &R,
                    llvm::json::Path P) {

  ObjectMapper O(Params, P);
  return O                          //
         && O.map("arity", R.Arity) //
         && O.map("doc", R.Doc)     //
      ;
}
