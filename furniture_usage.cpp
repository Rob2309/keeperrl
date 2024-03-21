#include "stdafx.h"
#include "furniture_usage.h"
#include "furniture.h"
#include "creature_factory.h"
#include "furniture_type.h"
#include "tribe.h"
#include "furniture_factory.h"
#include "creature.h"
#include "player_message.h"
#include "creature_group.h"
#include "game.h"
#include "event_listener.h"
#include "item.h"
#include "effect.h"
#include "lasting_effect.h"
#include "creature_name.h"
#include "level.h"
#include "sound.h"
#include "body.h"
#include "creature_attributes.h"
#include "gender.h"
#include "collective.h"
#include "territory.h"
#include "game_event.h"
#include "effect.h"
#include "name_generator.h"
#include "content_factory.h"
#include "item_list.h"
#include "effect_type.h"
#include "item_types.h"

struct ChestInfo {
  FurnitureType openedType;
  struct CreatureInfo {
    optional<CreatureGroup> creature;
    int creatureChance;
    int numCreatures;
    string msgCreature;
  };
  optional<CreatureInfo> creatureInfo;
  struct ItemInfo {
    ItemListId items;
    string msgItem;
  };
  optional<ItemInfo> itemInfo;
};

static void usePortal(Position pos, Creature* c) {
  c->you(MsgType::ENTER_PORTAL, "");
  if (auto otherPos = pos.getOtherPortal()) {
    if (otherPos->isSameLevel(pos)) {
      for (auto f : otherPos->getFurniture())
        if (f->hasUsageType(BuiltinUsageId::PORTAL)) {
          if (pos.canMoveCreature(*otherPos)) {
            pos.moveCreature(*otherPos, true);
            return;
          }
          for (Position v : otherPos->neighbors8(Random))
            if (pos.canMoveCreature(v)) {
              pos.moveCreature(v, true);
              return;
            }
        }
    } else {
      if (auto link = pos.getLandingLink()) {
        c->getLevel()->changeLevel(*link, c);
        pos.getGame()->addEvent(EventInfo::FX{pos, FXName::TELEPORT_OUT});
        pos.getGame()->addEvent(EventInfo::FX{*otherPos, FXName::TELEPORT_IN});
        return;
      }
    }
  }
  c->privateMessage("The portal is inactive. Create another one to open a connection.");
}

void FurnitureUsage::handle(FurnitureUsageType type, Position pos, const Furniture* furniture, Creature* c) {
  CHECK(c != nullptr);
  type.visit([&] (BuiltinUsageId id) {
    switch (id) {
      case BuiltinUsageId::KEEPER_BOARD:
        c->getGame()->handleMessageBoard(pos, c);
        break;
      case BuiltinUsageId::TIE_UP:
        c->addEffect(LastingEffect::TIED_UP, 100_visible);
        break;
      case BuiltinUsageId::TRAIN:
        pos.addSound(SoundId("MISSED_ATTACK"));
        break;
      case BuiltinUsageId::PORTAL:
        usePortal(pos, c);
        break;
      case BuiltinUsageId::DEMON_RITUAL:
      case BuiltinUsageId::STUDY:
      case BuiltinUsageId::ARCHERY_RANGE:
        break;
    }
  },
  [&](const UsageEffect& effect) {
    effect.effect.apply(c->getPosition(), c);
  });
}

bool FurnitureUsage::canHandle(FurnitureUsageType type, const Creature* c) {
  if (auto id = type.getReferenceMaybe<BuiltinUsageId>())
    switch (*id) {
      case BuiltinUsageId::KEEPER_BOARD:
        return c->getBody().isHumanoid();
      default:
        return true;
    }
  return true;
}

string FurnitureUsage::getUsageQuestion(FurnitureUsageType type, string furnitureName) {
  return type.visit(
      [&] (BuiltinUsageId id) {
        switch (id) {
          case BuiltinUsageId::KEEPER_BOARD: return "view " + furnitureName;
          case BuiltinUsageId::PORTAL: return "enter " + furnitureName;
          default:
            return "use " + furnitureName;
        }
      },
      [&](const UsageEffect& e) {
        return e.usageVerb;
      }
  );
}
