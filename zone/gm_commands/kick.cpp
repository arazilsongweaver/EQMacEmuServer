#include "../client.h"
#include "../worldserver.h"
extern WorldServer worldserver;

void command_kick(Client *c, const Seperator *sep){
	const char* name;
	if (sep->arg[1])                // arg specified
	{
		name = sep->arg[1];
	}
	else if (c->GetTarget()) {               // have target
		name = c->GetTarget()->GetName();
	}
	else
	{
		c->Message(Chat::White, "Usage: #kick [Character Name]");
		return;
	}

	auto client = entity_list.GetClientByName(name);

	if (!worldserver.Connected())
		c->Message(Chat::White, "The world server is currently disconnected.");
	else {
		auto pack = new ServerPacket(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
		ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*)pack->pBuffer;
		strcpy(skp->adminname, c->GetName());
		strcpy(skp->name, name);
		skp->adminrank = c->Admin();
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}

	if (client) {
		if (client->Admin() <= c->Admin()) {
			client->Message_StringID(Chat::Red, StringID::BEEN_KICKED);
			client->WorldKick();
			c->Message(Chat::White, 
				fmt::format(
				"{} has been kicked from the server.",
				name
				).c_str()
			);
		}
	}
}

