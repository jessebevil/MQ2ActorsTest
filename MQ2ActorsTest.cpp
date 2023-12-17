// MQ2ActorsTest.cpp : Defines the entry point for the DLL application.

#include <mq/Plugin.h>
#include "mq/api/ActorAPI.h"//This is the only file you should need to send/receive std::string

PreSetup("MQ2ActorsTest");
PLUGIN_VERSION(0.1);

//global. Need access to Remove and to AddActor
//Locations like SetGameState, ShutdownPlugin, OnPulse, and Callbacks
postoffice::DropboxAPI ActorTestDropbox;

// no way to see if ActorTestDropbox is valid for removal....so keep track?
bool gINIT_DONE = false;

//using ReceiveCallbackAPI = std::function<void(const std::shared_ptr<Message>&)>;
//The "using" specifies this should be a function with a return type of void, and parameter of const std::shared_ptr<postoffice::Message>& message
void ActorTestReceiveCallback(const std::shared_ptr<postoffice::Message>& message) {
	if (message && message->Payload) {
		//Right now we're only sending std::string, so we know it's a string and we'll just write it out.
		//Your plugin should determine what was sent, and then read it appropriately. Writing is just showing it was received.
		WriteChatf(message->Payload->c_str());

		//PONG - Not needed, we're just verifying we got it.
		//This would populate in ActorTestResponseCallback.
		//message is the message we just received
		//received is an std::string we're sending to them
		//and the int value at the end is a custom response status.
		//You can enumerate this based on logic from the message you received here.
		//In this case we're using 0. This number should be a non-negative value.
		std::string received = "received";
		ActorTestDropbox.PostReply(message, received, 0);
	}
}


//using ResponseCallbackAPI = std::function<void(int, const std::shared_ptr<Message>&)>;
void ActorTestResponseCallback(int responsestatus, const std::shared_ptr<postoffice::Message>& message) {
	//While this is the only place ResponseStatus is used, you MUST cast either the enum to an int or
	//the incoming int (currently called responsestatus) to a ResponseStatus to compare the two.
	//According to dannuic you need to be explicit in your use of this which is why it requires a cast.
	//It would be better if the API ResponseCallback's first param was a ResponseStatus to avoid this
	//additional step. But dannuic says this is an intentional requirement.
	//While it's technically undefined behavior to cast to ResponseStatus as it's a int8_t. It should
	//Only ever be one of the 4 enums as a result. So assumptions are made. I don't like it, but it's
	//not up to me.
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
			WriteChatf("\arResponseStatus: %d", responsestatus);
			break;
	}
	// WriteChatf("ResponseCallback was triggered");
}

PLUGIN_API void OnPulse() {
	//Because we're using pLocalPlayer->Name, let's make sure we're in game and pLocalPlayer exists
	if (GetGameState() != GAMESTATE_INGAME || !pLocalPlayer)
		return;

	static postoffice::Address addr;
	if (!gINIT_DONE) {
		// Do these need to be unique?
		addr.Mailbox = "Mailbox";
		addr.Character = pLocalPlayer->Name;//this means just send to ourselves. Not ideal.
		// Is this intended for the game server? Probably
		// addr.Server
		ActorTestDropbox = postoffice::AddActor("Mailbox", ActorTestReceiveCallback);
		gINIT_DONE = true;
	}

	static int pulsedelay = 1000;// This is pretty high value. Takes like 10 seconds? Don't need to be that fast for testing.
	static int pulse = 1000;
	if (gINIT_DONE && pulsedelay < pulse++) {
		std::string pointless = "somedata";
		// send "somedata" as an std::string. You can send any string this way, and OnPulse
		// is probably not the normal place you would send it, however we're just using this to show
		// sending information from one client to another.
		ActorTestDropbox.Post(addr, pointless, ActorTestResponseCallback);
		pulse = 0;
	}
}

PLUGIN_API void SetGameState(int GameState) {
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