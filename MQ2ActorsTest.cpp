// MQ2ActorsTest.cpp : Defines the entry point for the DLL application.

#include <mq/Plugin.h>
#include "mq/api/ActorAPI.h"//This is the only file you should need to send/receive std::string

PreSetup("MQ2ActorsTest");
PLUGIN_VERSION(0.1);

//global. Need access to Remove and to AddActor
//Locations like SetGameState, ShutdownPlugin, OnPulse, and Callbacks
postoffice::DropboxAPI ActorTestDropbox;
std::vector<postoffice::Address> gvGroupMembers;
std::queue<std::pair<std::string, std::string>> qBuffsToGive;

// no way to see if ActorTestDropbox is valid for removal....so keep track?
bool gINIT_DONE = false;
bool WeNeedTheBuff(std::optional<std::string> buff) {
	//Actually check this later. For now, just don't say true for "NULL"
	if (buff && !ci_equals(buff->c_str(), "NULL")) {
		return true;
	}

	return false;
}

void ActorTestReceiveCallback(const std::shared_ptr<postoffice::Message>& message) {
	if (message && message->Payload) {
		//pretend we check all our buffs for stacking, and now we want one of the broadcasted buffs from the person that sent it.
		if (WeNeedTheBuff(message->Payload)) {//TODO: Add logic to check this.
			std::string buffIWant = message->Payload->c_str();
			ActorTestDropbox.PostReply(message, buffIWant, 1);
		}
		WriteChatf("%s", message->Payload->c_str());
	}//We didn't need the broadcasted buff.
	WriteChatf("ReceiveCallback used");
}

void ActorTestResponseCallback(const postoffice::Address& address, int responsestatus, const std::shared_ptr<postoffice::Message>& message) {

	postoffice::ResponseStatus rsResponse = static_cast<postoffice::ResponseStatus>(responsestatus);
	switch (rsResponse) {
		case postoffice::ResponseStatus::ConnectionClosed:
			WriteChatf("\arConnection closed");
			break;
		case postoffice::ResponseStatus::NoConnection:
			WriteChatf("\arNo connection");
			break;
		case postoffice::ResponseStatus::RoutingFailed:
			WriteChatf("\arRouting Failed");
			break;
		case postoffice::ResponseStatus::AmbiguousRecipient:
			WriteChatf("\arAmbiguous Recipient");
			break;
		default:
			//There's only the above 4 response status built in. So if we are here. It's a PostReply
			//If it's a PostReply we should use our own ResponseStatus values. They should be >= 0
			//WriteChatf("\ayResponseStatus: %d", responsestatus);
			//1 - They wanted the buff.
			if (responsestatus == 1) {
				//can't do this, message->Sender->Character is always empty.
				std::string sender = address.Character->c_str();
				std::string buff = message->Payload->c_str();
				WriteChatf("%s wants the buff %s", sender.c_str(), buff.c_str());
				qBuffsToGive.push(std::make_pair(sender, buff));
				WriteChatf("%s wants %s", sender.c_str(), message->Payload->c_str());
				return;
			}

			//unhandled responsestatus
			WriteChatf("\ayResponseStatus: %d", responsestatus);

			//Payload output to MQ Console.
			if (message && message->Payload) {
				WriteChatf("%s", message->Payload->c_str());
			}

			break;
	}
	WriteChatf("Response callback used");
}

PLUGIN_API void OnPulse() {
	//Because we're using pLocalPlayer->Name, let's make sure we're in game and pLocalPlayer exists
	if (GetGameState() != GAMESTATE_INGAME || !pLocalPlayer)
		return;

	if (!GetCharInfo()->pGroupInfo)
		return;

	static postoffice::Address addr;
	static std::string mybuff;
	if (!gINIT_DONE) {
		//Store an address for everyone currently in the group at load time. can adjust this later.
		//Like when someone joins or leaves the group.
		for (int i = 0; i < MAX_GROUP_SIZE; i++) {//not skipping myself for testing purposes.
			CGroupMember* pMember = GetCharInfo()->Group->GetGroupMember(i);
			if (!pMember)
				break;

			postoffice::Address memberaddress;
			memberaddress.Mailbox = "Mailbox";
			memberaddress.Character = pMember->GetName();
			gvGroupMembers.push_back(memberaddress);
		}

		int myclass = pLocalPlayer->GetClass();
		switch (myclass) {
			case Cleric:
				mybuff = "Temperance";
				break;
			case Shaman:
				mybuff = "Swift Like the Wind";
				break;
			case Enchanter:
				mybuff = "Aanya's Quickening";
				break;
			default:
				mybuff = "NULL";
				break;
		}

		ActorTestDropbox = postoffice::AddActor("Mailbox", ActorTestReceiveCallback);
		gINIT_DONE = true;
	}

	static int pulsedelay = 3000;
	static int pulse = 3000;

	//Proper main loop
	if (gINIT_DONE && pulsedelay < pulse++) {
		WriteChatf("Pulse Occured");
		if (!ci_equals(mybuff, "NULL")) {
			//Let my group know what buffs we can do.
			for (postoffice::Address& addr : gvGroupMembers) {
				WriteChatf("Sent buffs to group member: %s", addr.Character->c_str());
				//create a lamda that matches the required callback signature, but include the addr of the group member [addr]
				//then call a modified function that includes the addr in the list of paramaters.
				//This gives us access to the recipient on the response.
				ActorTestDropbox.Post(addr, mybuff, [addr](int response, const std::shared_ptr<postoffice::Message>& message) {
					ActorTestResponseCallback(addr, response, message);
				});
			}

			//Do buffs in the queue then pop it out.
			if (!qBuffsToGive.empty() /*&& Im not in combat/moving/casting/dead/etc*/) {

				//TODO: Add logic to do a buff.
				qBuffsToGive.pop();
			}
		}

		pulse = 0;
	}
}

PLUGIN_API void SetGameState(int GameState) {//Brain says this isn't needed, and just ignore the messages
	if (GameState != GAMESTATE_INGAME && gINIT_DONE) {
		ActorTestDropbox.Remove();
		gINIT_DONE = false;
	}
}

PLUGIN_API void ShutdownPlugin() {
	//Sometimes crashes if you Remove() as it's doing the callback or processing a sent message.
	//This specifically occured when I was sending a post every pulse of OnPulse and unload the plugin.
	//Probably not going to happen if you throttle how often you send posts.
	ActorTestDropbox.Remove();
}