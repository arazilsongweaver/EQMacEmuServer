#include "../client.h"
#include "../worldserver.h"
extern WorldServer worldserver;

void command_kick(Client *c, const Seperator *sep){
	Mob* t = nullptr;
	const char* name;

	// Use Argument (if specified)
	if (sep->arg[1][0] != 0) {
		name = sep->arg[1];
		t = entity_list.GetMob(name);

	}
	// Use Target (if available)
	else if (c->GetTarget()) {
		t = c->GetTarget();
		if (t) {
			name = t->GetName();
		}
	}
	// Unknown target -> print help text
	else
	{
		c->Message(Chat::White, "Usage: #kick [Character Name]");
		return;
	}

	// Target does not exist in the area -> send to world
	if (!t) {	
		if (!worldserver.Connected()) {
			c->Message(Chat::Red, "ERROR: The world server is currently disconnected.");
			return;
		}
		else {
			auto pack = new ServerPacket(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
			ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*)pack->pBuffer;
			strcpy(skp->adminname, c->GetName());
			strcpy(skp->name, name);
			skp->adminrank = c->Admin();
			worldserver.SendPacket(pack);
			safe_delete(pack);
			c->Message(Chat::Red, fmt::format("Remote kicking {}.", name).c_str());
		}
	}
	// Target exists and is a Client -> WorldKick() them
	else if (t->IsClient()) {
		if (t->CastToClient()->Admin() <= c->Admin()) {
			t->CastToClient()->WorldKick();
			c->Message(Chat::Red, fmt::format("Kicking {}.", name).c_str());
		}
	}
	// Target exists and is not a Client -> print error message
	else {
		c->Message(Chat::Red, fmt::format("ERROR: {} is not a player character.", name).c_str());
	}
}
