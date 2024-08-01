/*
 * Copyright (c) liblcf authors
 * This file is released under the MIT License
 * http://opensource.org/licenses/MIT
 */

#include <fstream>
#include <memory>
#include <sstream>
#include <variant>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include "lcf/ldb/reader.h"
#include "lcf/lmt/reader.h"
#include "lcf/lmu/reader.h"
#include "lcf/lsd/reader.h"
#include "lcf/reader_util.h"
#include "lcf/dbstring.h"
#include "lcf/rpg/map.h"
#include "lcf/rpg/music.h"
#include "lcf/rpg/treemap.h"
#include "lcf/rpg/mapinfo.h"
#include "lcf/rpg/database.h"
#include "lcf/dbarray.h"
#include "lcf/enum_tags.h"
#include "lcf/rpg/animationtiming.h"
#include "lcf/rpg/battlecommands.h"
#include "lcf/rpg/battleranimation.h"
#include "lcf/rpg/battleranimationitemskill.h"
#include "lcf/rpg/battleranimationpose.h"
#include "lcf/rpg/commonevent.h"
#include "lcf/rpg/encounter.h"
#include "lcf/rpg/enemyaction.h"
#include "lcf/rpg/equipment.h"
#include "lcf/rpg/event.h"
#include "lcf/rpg/eventcommand.h"
#include "lcf/rpg/eventpage.h"
#include "lcf/rpg/eventpagecondition.h"
#include "lcf/rpg/movecommand.h"
#include "lcf/rpg/parameters.h"
#include "lcf/rpg/rect.h"
#include "lcf/rpg/saveactor.h"
#include "lcf/rpg/savecommonevent.h"
#include "lcf/rpg/saveeasyrpgdata.h"
#include "lcf/rpg/saveeasyrpgwindow.h"
#include "lcf/rpg/saveeventexecstate.h"
#include "lcf/rpg/saveinventory.h"
#include "lcf/rpg/savepanorama.h"
#include "lcf/rpg/savepartylocation.h"
#include "lcf/rpg/savepicture.h"
#include "lcf/rpg/savescreen.h"
#include "lcf/rpg/savesystem.h"
#include "lcf/rpg/savetarget.h"
#include "lcf/rpg/savetitle.h"
#include "lcf/rpg/savevehiclelocation.h"
#include "lcf/rpg/trooppagecondition.h"
#include "lcf/rpg/variable.h"
#include "lcf/string_view.h"
#include "reader_struct_impl.h"

namespace py = pybind11;
namespace rpg = lcf::rpg;

using namespace pybind11::literals;

using LcfObject = std::variant<
  std::unique_ptr<rpg::Map>,
  std::unique_ptr<rpg::TreeMap>,
  std::unique_ptr<rpg::Save>,
  std::unique_ptr<rpg::Database>,
  py::none
>;

template <class ...Structs>
void DefineStructs(py::class_<Structs>&... classes) {
  ([&] {
    lcf::Struct<Structs>::ApplyTo(classes);
  } (), ...);
}

template <class ...Structs>
void DefineStructs(py::module_& m) {
  ([&] {
    using Self = lcf::Struct<Structs>;
    auto clazz = py::class_<Structs>(m, Self::name);
    Self::ApplyTo(clazz);
  } (), ...);
}

inline bool StringIsAscii(lcf::StringView s) {
  return std::all_of(s.begin(), s.end(), [](char c) {
    return isascii(static_cast<int>(c));
  });
}

static std::string parsed_encoding {};

PYBIND11_MODULE(pylcf, m) {
  m.doc() = R"###(
Thin Python wrapper around liblcf.

Encoding
========
pylcf takes a similar approach to the EasyRPG Player on how to automatically
determine the suitable encoding for strings. By reading the database file first
before any other file, conventionally named RPG_RT.ldb, pylcf will try to
guess the encoding and reuse it for later reads.

If the encoding of a game is known ahead of time, it is advisable to set the
encoding with :ref:`pylcf.encoding` before reading. For Japanese games, general
usage will usually only require the "shift_jis" encoding. One can also set the
encoding to "auto" so that the next read of a database will guess the
encoding again.
)###";

  py::class_<rpg::EventCommand> eventcmd(m, "EventCommand",
    "A mapping between command codes and tags is available on ``EventCommand.tag``.");
  eventcmd
    .def_readwrite("code", &rpg::EventCommand::code,
      "An integer code representing the command. A description of the command\n"
      "can be acquired with ``EventCommand.tag[code]``.")
    .def_readwrite("indent", &rpg::EventCommand::indent)
    .def_readwrite("string", &rpg::EventCommand::string,
      "The string component of this command, for example the description of a\n"
      "Comment, the file path for Picture commands etc.")
    .def_readwrite("parameters", &rpg::EventCommand::parameters)
    .def("__repr__", [](const rpg::EventCommand& cmd) {
      std::ostringstream out;
      out << std::string(cmd.indent * 2, ' ');

      const auto evcode = static_cast<rpg::EventCommand::Code>(cmd.code);
      switch (evcode) {
        case rpg::EventCommand::Code::Comment:
        case rpg::EventCommand::Code::Comment_2:
          out << "# " << cmd.string;
          break;
        case rpg::EventCommand::Code::ConditionalBranch:
          out << "If";
          // Shortened version of Player logic
          switch (cmd.parameters[0]) {
            case 0: out << " Switch(" << cmd.parameters[1] << ") = " << 1 - cmd.parameters[2]; break;
            case 1:
              out << " Variable(" << cmd.parameters[1] << ")";
              switch (cmd.parameters[4]) {
                case 0: out << " = "; break;
                case 1: out << " >= "; break;
                case 2: out << " <= "; break;
                case 3: out << " > "; break;
                case 4: out << " < "; break;
                case 5: out << " != "; break;
              }
              if (cmd.parameters[2]) { // variable
                out << "Variable(" << cmd.parameters[3] << ")";
              } else { // value
                out << cmd.parameters[3];
              }
              break;
            default:
              for (auto &i : cmd.parameters) {
                out << " " << i;
              }
              break;
          }
          break;
        case rpg::EventCommand::Code::ControlVars:
        case rpg::EventCommand::Code::ControlSwitches:
          if (evcode == rpg::EventCommand::Code::ControlVars) {
            out << "Set Variable(";
          } else {
            out << "Set Switch(";
          }
          switch (cmd.parameters[0]) {
            case 0: out << cmd.parameters[1]; break; // single
            case 1: out << cmd.parameters[1] << ".." << cmd.parameters[2]; break; // range
            case 2: out << "at Variable(" << cmd.parameters[1] << ")"; break; // indirect
            default: out << "..."; break;
          }
          switch (cmd.parameters[3]) {
            case 0: out << ") = "; break;
            case 1: out << ") += "; break;
            case 2: out << ") -= "; break;
            case 3: out << ") *= "; break;
            case 4: out << ") /= "; break;
            case 5: out << ") %= "; break;
            case 6: out << ") |= "; break;
            case 7: out << ") &= "; break;
            case 8: out << ") ^= "; break;
            case 9: out << ") <<= "; break;
            case 10: out << ") >>= "; break;
          }
          if (evcode == rpg::EventCommand::Code::ControlVars) {
            switch (cmd.parameters[4]) {
              case 0: out << cmd.parameters[5]; break;
              case 1: out << "Variable(" << cmd.parameters[5] << ")"; break;
              case 2: out << "Variable(at Variable(" << cmd.parameters[5] << "))"; break;
              default: out << "..."; break;
            }
          } else if (cmd.parameters[3] < 2) {
            out << 1 - cmd.parameters[3];
          } else {
            out << "toggle";
          }
          break;
        default:
          out << rpg::EventCommand::kCodeTags.tag(evcode);
					for (auto &i : cmd.parameters) {
						out << " " << i;
					}
          if (!cmd.string.empty()) {
            out << " \"" << cmd.string << "\"";
          }
          break;
      }
      return out.str();
    });

  {
    py::dict eventcmdtag {};
    for (auto &item : rpg::EventCommand::kCodeTags) {
      eventcmdtag[item.name] = item.value;
      eventcmdtag[py::int_(item.value)] = item.name;
    }
    m.attr("EventCommand").attr("tag") = eventcmdtag;
  }

  py::class_<rpg::MoveCommand>(m, "MoveCommand")
    .def_readwrite("command_id", &rpg::MoveCommand::command_id)
    .def_readwrite("parameter_string", &rpg::MoveCommand::parameter_string)
    .def_readwrite("parameter_a", &rpg::MoveCommand::parameter_a)
    .def_readwrite("parameter_b", &rpg::MoveCommand::parameter_b)
    .def_readwrite("parameter_c", &rpg::MoveCommand::parameter_c);

  py::class_<rpg::Rect>(m, "Rect")
    .def_readwrite("t", &rpg::Rect::t)
    .def_readwrite("l", &rpg::Rect::l)
    .def_readwrite("b", &rpg::Rect::b)
    .def_readwrite("r", &rpg::Rect::r);

  py::class_<rpg::Parameters>(m, "Parameters")
    .def_readwrite("attack", &rpg::Parameters::attack)
    .def_readwrite("defense", &rpg::Parameters::defense)
    .def_readwrite("maxhp", &rpg::Parameters::maxhp)
    .def_readwrite("maxsp", &rpg::Parameters::maxsp)
    .def_readwrite("spirit", &rpg::Parameters::spirit)
    .def_readwrite("agility", &rpg::Parameters::agility);

  py::class_<rpg::Equipment>(m, "Equipment")
    .def_readwrite("weapon_id", &rpg::Equipment::weapon_id)
    .def_readwrite("shield_id", &rpg::Equipment::shield_id)
    .def_readwrite("armor_id", &rpg::Equipment::armor_id)
    .def_readwrite("helmet_id", &rpg::Equipment::helmet_id)
    .def_readwrite("accessory_id", &rpg::Equipment::accessory_id);

  py::class_<rpg::EventPageCondition::Flags>(m, "EventPageConditionFlags")
    .def_readwrite("switch_a", &rpg::EventPageCondition::Flags::switch_a)
    .def_readwrite("switch_b", &rpg::EventPageCondition::Flags::switch_b)
    .def_readwrite("variable", &rpg::EventPageCondition::Flags::variable)
    .def_readwrite("item", &rpg::EventPageCondition::Flags::item)
    .def_readwrite("actor", &rpg::EventPageCondition::Flags::actor)
    .def_readwrite("timer", &rpg::EventPageCondition::Flags::timer)
    .def_readwrite("timer2", &rpg::EventPageCondition::Flags::timer2)
    .def("__repr__", [](const rpg::EventPageCondition::Flags& flags) {
      std::ostringstream out;
      out << "<pylcf.EventPageConditionFlags";
      if (flags.switch_a) out << " switch_a";
      if (flags.switch_b) out << " switch_b";
      if (flags.variable) out << " variable";
      if (flags.item) out << " item";
      if (flags.actor) out << " actor";
      if (flags.timer) out << " timer";
      if (flags.timer2) out << " timer2";
      out << ">";
      return out.str();
    });

  DefineStructs<
    // Leaf structs
    rpg::Encounter, rpg::EventPageCondition, rpg::Music, rpg::MapInfo,
      rpg::Learning, rpg::Sound, rpg::BattlerAnimationItemSkill,
      rpg::EnemyAction, rpg::Start, rpg::Attribute, rpg::BattleCommand,
      rpg::AnimationCellData, rpg::Variable, rpg::Switch, rpg::MoveRoute, 
    // Structs depending on at least 1 child struct
    rpg::Item, rpg::Skill, rpg::Class, rpg::Actor, rpg::Enemy, rpg::Terrain,
      rpg::BattleCommands, rpg::EventPage, rpg::Event,
    // Uncategorized
    rpg::TroopMember, rpg::TroopPageCondition, rpg::TroopPage, rpg::Troop,
    rpg::SaveActor, rpg::SaveTitle, rpg::SaveSystem, rpg::SaveScreen,
      rpg::SavePicture, rpg::SavePartyLocation, rpg::SaveVehicleLocation,
      rpg::SaveInventory, rpg::SaveTarget, rpg::SaveEventExecFrame,
      rpg::SaveEventExecState, rpg::SaveMapEvent, rpg::SaveMapInfo,
      rpg::SavePanorama, rpg::SaveCommonEvent, rpg::SaveEasyRpgText,
      rpg::SaveEasyRpgWindow, rpg::SaveEasyRpgData, rpg::Save,
    rpg::State, rpg::Terms,
    rpg::System, rpg::Chipset,
    rpg::AnimationFrame, rpg::AnimationTiming, rpg::Animation,
    rpg::Map, rpg::BattlerAnimationPose, rpg::BattlerAnimationWeapon, rpg::BattlerAnimation,
    rpg::CommonEvent, rpg::Database>(m);

  // ================= Class definitions end =================

  m.def("encoding", [&](std::string encoding) {
    if (encoding.empty()) {
      return parsed_encoding;
    }
    return parsed_encoding = encoding;
  }, "encoding"_a = "", "Gets or sets the current encoding used by pylcf.");

  py::class_<rpg::TreeMap>(m, "TreeMap")
    .def_readwrite("maps", &rpg::TreeMap::maps)
    .def_readwrite("tree_order", &rpg::TreeMap::tree_order, "A list of map indices in which the maps are laid out.")
    .def_readwrite("active_node", &rpg::TreeMap::active_node)
    .def_readwrite("start", &rpg::TreeMap::start);

  m.def("read_lcf", [&](std::variant<int, std::string> path_or_id, std::string encoding) -> LcfObject {
    std::string path;
    if (int *id = std::get_if<int>(&path_or_id)) {
      std::ostringstream stream;
      stream << "Map" << std::setw(4) << std::setfill('0') << *id << ".lmu";
      path = stream.str();
    } else if (std::string *full_path = std::get_if<std::string>(&path_or_id)) {
      path = *full_path;
    }
    std::ifstream file(path);

    char buffer[128] {0};
    file.seekg(1, std::ios::beg);
    file.read(buffer, 10);
    std::string header(buffer);

    if (encoding.empty() || encoding == "auto") {
      encoding = parsed_encoding;
    }

    if (header == "LcfDataBas") {
      auto db = lcf::LDB_Reader::Load(path, encoding);
      if (encoding.empty() || encoding == "auto") {
        // Perform a limited version of the heuristic used by Player.
        auto candidates = lcf::ReaderUtil::DetectEncodings(*db);
        for (std::string &candidate : candidates) {
          const auto &system_name = db->system.system_name;
          if (!StringIsAscii(system_name)) {
            auto system_path = std::filesystem::canonical(path);
            system_path = system_path.parent_path() / "System" / lcf::ReaderUtil::Recode(system_name, candidate);
            if (std::filesystem::exists(system_path)) {
              encoding = parsed_encoding = system_path.string();
              break;
            }
          }
        }
        if (!candidates.empty() && encoding.empty()) {
          encoding = parsed_encoding = candidates.front();
        } else if (encoding.empty()) {
          encoding = lcf::ReaderUtil::GetLocaleEncoding();
        }
        // Reparse a new copy of the database, hopefully with the correct encoding.
        db = lcf::LDB_Reader::Load(path, encoding);
      }
      return db;
    }
    if (encoding.empty()) {
      encoding = lcf::ReaderUtil::GetLocaleEncoding();
    }
    if (header == "LcfMapTree") {
      return lcf::LMT_Reader::Load(path, encoding);
    } else if (header == "LcfSaveDat") {
      return lcf::LSD_Reader::Load(path, encoding);
    } else if (header == "LcfMapUnit") {
      return lcf::LMU_Reader::Load(path, encoding);
    } else {
      py::print(path, "is not supported");
      return py::none();
    }
  }, "path_or_id"_a, "encoding"_a = "", R"###(
Reads an LCF-compatible file.

:param path_or_id: A path to an .ldb, .lmu, .lmt or .lsd file. If an integer,
  it is understood as "MapXXXX.lmu".
:param encoding: The desired encoding to parse this file with. If empty, an
  encoding previously determined by parsing an .ldb file will be used if exists,
  or the system's active locale otherwise.
)###");

  m.def("read_ldb", [](std::string path, std::string encoding) -> std::variant<std::unique_ptr<rpg::Database>, py::none> {
    auto db = lcf::LDB_Reader::Load(path, encoding);
    if (encoding.empty() || encoding == "auto") {
      // Perform a limited version of the heuristic used by Player.
      auto candidates = lcf::ReaderUtil::DetectEncodings(*db);
      for (std::string &candidate : candidates) {
        const auto &system_name = db->system.system_name;
        if (!StringIsAscii(system_name)) {
          auto system_path = std::filesystem::canonical(path);
          system_path = system_path.parent_path() / "System" / lcf::ReaderUtil::Recode(system_name, candidate);
          if (std::filesystem::exists(system_path)) {
            encoding = parsed_encoding = system_path.string();
            break;
          }
        }
      }
      if (!candidates.empty() && encoding.empty()) {
        encoding = parsed_encoding = candidates.front();
      } else if (encoding.empty()) {
        encoding = lcf::ReaderUtil::GetLocaleEncoding();
      }
      // Reparse a new copy of the database, hopefully with the correct encoding.
      db = lcf::LDB_Reader::Load(path, encoding);
    }
    if (!db) return py::none();
    return db;
  }, "path"_a = "RPG_RT.ldb", "encoding"_a = "");

  m.def("read_lmt", [](std::string path, std::string encoding) -> std::variant<std::unique_ptr<rpg::TreeMap>, py::none> {
    if (encoding.empty() || encoding == "auto") encoding = parsed_encoding;
    auto data = lcf::LMT_Reader::Load(path, encoding);
    if (!data) return py::none();
    return data;
  }, "path"_a = "RPG_RT.lmt", "encoding"_a = "");

  m.def("read_lmu", [](std::variant<int, std::string> path_or_id, std::string encoding) -> std::variant<std::unique_ptr<rpg::Map>, py::none> {
    if (encoding.empty() || encoding == "auto") encoding = parsed_encoding;
    std::string path;
    if (int *id = std::get_if<int>(&path_or_id)) {
      std::ostringstream stream;
      stream << "Map" << std::setw(4) << std::setfill('0') << *id << ".lmu";
      path = stream.str();
    } else if (std::string *full_path = std::get_if<std::string>(&path_or_id)) {
      path = *full_path;
    }
    auto data = lcf::LMU_Reader::Load(path, encoding);
    if (!data) return py::none();
    return data;
  }, "path_or_id"_a, "encoding"_a = "");
}
