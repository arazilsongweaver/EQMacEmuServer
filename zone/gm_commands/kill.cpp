#include "../client.h"

void command_kill(Client *c, const Seperator *sep)
{

	Mob* t = nullptr;
	uint16 entity_id = 0;
	const char* npc_name;
        if (sep->arg[1][0] != 0)                // arg specified
        {
		// Number = Entity ID
		if (sep->IsNumber(1)) {
			entity_id = static_cast<uint16>(Strings::ToUnsignedInt(sep->arg[1]));
			t = entity_list.GetMob(entity_id);
			npc_name = t->GetName();
		}
		// String = NPC Name
		else {
			npc_name = sep->arg[1];
			t = entity_list.GetMob(npc_name);
			if(!t) {
				c->Message(Chat::Red, fmt::format("ERROR: {} not found.", npc_name).c_str());
				return;
			}
		}
        }
        else if (c->GetTarget()) {               // have target
                t = c->GetTarget();
		npc_name = t->GetName();
	}
        else
        {
                c->Message(Chat::White, "Usage: #kill [name|entityid] - Kills your current target or name or entity ID if specified.");
                return;
        }

        if (!t || !npc_name)
                return;

	if (!t->IsClient() || t->CastToClient()->Admin() <= c->Admin()) {
		c->Message(Chat::Red, fmt::format("Killing {}.", npc_name).c_str());
		if(t->IsClient()) {
			t->CastToClient()->GMKill();
		} else {
			t->Kill();
		}
	}
}

