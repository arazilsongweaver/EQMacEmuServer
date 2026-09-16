#include "../client.h"

void command_linkdead(Client *c, const Seperator *sep)
{
	if (sep->argnum < 1)
	{
		c->Message(Chat::White, "Usage: #linkdead [Character Name]");
		return;
	}

	Client *client = entity_list.GetClientByName(sep->arg[1]);
	if (client == nullptr)
	{
		c->Message(Chat::White, "Character %s is not connected to this zone.", sep->arg[1]);
		return;
	}

	if (client->Admin() > c->Admin())
	{
		c->Message(Chat::White, "Your status is not high enough to make %s linkdead.", client->GetName());
		return;
	}

	if (!client->Connected())
	{
		c->Message(Chat::White, "%s is not in the connected state.", client->GetName());
		return;
	}

	client->Save();
	client->LinkDead();

	if (client != c)
		c->Message(Chat::White, "%s is now linkdead.", client->GetName());
}
