// MQ2ActorsTest.cpp : Defines the entry point for the DLL application.

#include <mq/Plugin.h>
#include "mq/api/ActorAPI.h"//This is the only file you should need to send/receive std::string

PreSetup("MQ2ActorsTest");
PLUGIN_VERSION(0.1);

//global. Need access to Remove and to AddActor
//Locations like SetGameState, ShutdownPlugin, OnPulse, and Callbacks
postoffice::DropboxAPI ActorTestDropbox;
std::queue<std::pair<std::string, std::string>> qBuffsToGive;

// no way to see if ActorTestDropbox is valid for removal....so keep track?
bool gINIT_DONE = false;
bool gGroupChanged = true;

//Need a timer to allow any changes in group to finish or we don't get our group correctly.
static uint64_t VerifyGroupTimer = 0;

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

	}//We didn't need the broadcasted buff.
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
			//Group member wasn't connected at the time the message was sent
			WriteChatf("\arRouting Failed to \ap%s", address.Character->c_str());
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
				std::string sender = address.Character->c_str();
				std::string buff = message->Payload->c_str();
				WriteChatf("%s wants the buff %s", sender.c_str(), buff.c_str());
				qBuffsToGive.emplace(std::make_pair(sender, buff));
				return;
			}

			//unhandled responsestatus
			WriteChatf("\ayUnhandled ResponseStatus: \at%d\ax - Did you make a mistake?", responsestatus);

			//Payload output to MQ Console.
			if (message && message->Payload) {
				WriteChatf("Unhandled ResponseStatus payload %s", message->Payload->c_str());
			}

			break;
	}
}

PLUGIN_API void OnPulse() {
	//Because we're using pLocalPlayer->Name, let's make sure we're in game and pLocalPlayer exists
	if (GetGameState() != GAMESTATE_INGAME || !pLocalPlayer)
		return;

	static std::string mybuff;

	if (!gINIT_DONE) {
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

	//Proper main loop
	if (gINIT_DONE) {
		if (!ci_equals(mybuff, "NULL")) {
			static int pulsedelay = 1000;
			static int pulse = pulsedelay;//start the pulse at the pulsedelay so that it fires immediately.
			if (pulsedelay < pulse++) {
				//Let my group know what buffs we can do.
				if (pLocalPC->pGroupInfo) {
					for (int i = 0; i < MAX_GROUP_SIZE; i++) {//not skipping myself for testing purposes.
						CGroupMember* pMember = pLocalPC->Group->GetGroupMember(i);
						if (!pMember)
							break;

						//Setup a mailbox address for each member.
						postoffice::Address memberaddress;
						memberaddress.Mailbox = "Mailbox";
						memberaddress.Character = pMember->GetName();

						//create a lamda that matches the required callback signature, but include the addr of the group member [addr]
						//then call a modified function that includes the addr in the list of paramaters.
						//This gives us access to more than just the PID of who responded.
						ActorTestDropbox.Post(memberaddress, mybuff, [memberaddress](int response, const std::shared_ptr<postoffice::Message>& message) {
							ActorTestResponseCallback(memberaddress, response, message);
							});
					}
				}

				//Handle Xtargets
				if (ExtendedTargetList* xtl = pLocalPC->pXTargetMgr) {
					for (int i = 0; i < xtl->XTargetSlots.Count; i++) {
						ExtendedTargetSlot xts = xtl->XTargetSlots[i];
						if (xts.xTargetType && xts.XTargetSlotStatus && xts.xTargetType == XTARGET_SPECIFIC_PC) {
							PlayerClient* thisXTargetMember = GetSpawnByID(xts.SpawnID);
							if (!thisXTargetMember)
								continue;

							//Setup a mailbox address for each member.
							postoffice::Address memberaddress;
							memberaddress.Mailbox = "Mailbox";
							memberaddress.Character = thisXTargetMember->Name;

							//create a lamda that matches the required callback signature, but include the addr of the group member [addr]
							//then call a modified function that includes the addr in the list of paramaters.
							//This gives us access to more than just the PID of who responded.
							ActorTestDropbox.Post(memberaddress, mybuff, [memberaddress](int response, const std::shared_ptr<postoffice::Message>& message) {
								ActorTestResponseCallback(memberaddress, response, message);
							});
						}
					}
				}

				//Handle raid members.
				if (pRaid && pRaid->RaidMemberCount) {
					for (int j = 0; j < 72; j++) {
						if (!pRaid->locations[j])
							continue;

						PlayerClient* thisRaidMember = GetSpawnByName(pRaid->raidMembers[j].Name);
						if (!thisRaidMember)
							continue;

						//Setup a mailbox address for each member.
						postoffice::Address memberaddress;
						memberaddress.Mailbox = "Mailbox";
						memberaddress.Character = thisRaidMember->Name;

						//create a lamda that matches the required callback signature, but include the addr of the group member [addr]
						//then call a modified function that includes the addr in the list of paramaters.
						//This gives us access to more than just the PID of who responded.
						ActorTestDropbox.Post(memberaddress, mybuff, [memberaddress](int response, const std::shared_ptr<postoffice::Message>& message) {
							ActorTestResponseCallback(memberaddress, response, message);
						});

					}
				}

				pulse = 0;
			}

			//Do buffs in the queue then pop it out.
			static int buffdelay = 500;
			static int buffpulse = buffdelay;//start the buffpulse at the buffdelay so that it fires immediately.
			if (!qBuffsToGive.empty() && buffdelay < buffpulse++ /*&& Im not in combat/moving/casting/dead/etc*/) {

				//TODO: Add logic to do a buff. Pretend for now.
				WriteChatf("\ayBuffing \at%s\ax with \ap%s", qBuffsToGive.front().first.c_str(), qBuffsToGive.front().second.c_str());
				qBuffsToGive.pop();
			}
		}
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
	//Don't need to set gINIT_DONE here because we're closing the entire plugin.
}