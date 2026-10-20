#include "../client.h"

void command_showfilters(Client *c, const Seperator *sep) {
	if (!c) {
		return;
	}

	c->Message(Chat::Yellow, "Server filtering: %s", c->IsServerFilterEnabled() ? "On" : "Off");
	c->Message(Chat::Yellow, "Stored category modes: 0 Hide, 1 Show, 2 Group, 3 Self");
	for (int i = FilterDamageShields; i < _FilterCount; ++i) {
		const auto filter = static_cast<eqFilterType>(i);
		if (filter != FilterNone) {
			c->Message(Chat::Yellow, "Stored filter (%i) = %i", i, (int)c->GetFilterSetting(filter));
		}
	}
}

