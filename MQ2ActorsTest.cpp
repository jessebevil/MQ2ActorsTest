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
			//There's only the above 4 response status. So we shouldn't be here.
			//Simply here in the event the number of Enums changes and one isn't
			//accounted for.
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
		addr.Character = pLocalPlayer->Name;
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
	ActorTestDropbox.Remove();
}

PLUGIN_API void InitializePlugin() {
	//DebugSpewAlways("MQ2ActorsTest::Initializing version %f", MQ2Version);

	// Examples:
	// AddCommand("/mycommand", MyCommand);
	// AddXMLFile("MQUI_MyXMLFile.xml");
	// AddMQ2Data("mytlo", MyTLOData);
}

/**
 * @fn OnWriteChatColor
 *
 * This is called each time WriteChatColor is called (whether by MQ2Main or by any
 * plugin).  This can be considered the "when outputting text from MQ" callback.
 *
 * This ignores filters on display, so if they are needed either implement them in
 * this section or see @ref OnIncomingChat where filters are already handled.
 *
 * If CEverQuest::dsp_chat is not called, and events are required, they'll need to
 * be implemented here as well.  Otherwise, see @ref OnIncomingChat where that is
 * already handled.
 *
 * For a list of Color values, see the constants for USERCOLOR_.  The default is
 * USERCOLOR_DEFAULT.
 *
 * @param Line const char* - The line that was passed to WriteChatColor
 * @param Color int - The type of chat text this is to be sent as
 * @param Filter int - (default 0)
 */
PLUGIN_API void OnWriteChatColor(const char* Line, int Color, int Filter)
{
	// DebugSpewAlways("MQ2ActorsTest::OnWriteChatColor(%s, %d, %d)", Line, Color, Filter);
}

/**
 * @fn OnIncomingChat
 *
 * This is called each time a line of chat is shown.  It occurs after MQ filters
 * and chat events have been handled.  If you need to know when MQ2 has sent chat,
 * consider using @ref OnWriteChatColor instead.
 *
 * For a list of Color values, see the constants for USERCOLOR_. The default is
 * USERCOLOR_DEFAULT.
 *
 * @param Line const char* - The line of text that was shown
 * @param Color int - The type of chat text this was sent as
 *
 * @return bool - Whether to filter this chat from display
 */
PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color)
{
	// DebugSpewAlways("MQ2ActorsTest::OnIncomingChat(%s, %d)", Line, Color);
	return false;
}

/**
 * @fn OnAddSpawn
 *
 * This is called each time a spawn is added to a zone (ie, something spawns). It is
 * also called for each existing spawn when a plugin first initializes.
 *
 * When zoning, this is called for all spawns in the zone after @ref OnEndZone is
 * called and before @ref OnZoned is called.
 *
 * @param pNewSpawn PSPAWNINFO - The spawn that was added
 */
PLUGIN_API void OnAddSpawn(PSPAWNINFO pNewSpawn)
{
	// DebugSpewAlways("MQ2ActorsTest::OnAddSpawn(%s)", pNewSpawn->Name);
}

/**
 * @fn OnRemoveSpawn
 *
 * This is called each time a spawn is removed from a zone (ie, something despawns
 * or is killed).  It is NOT called when a plugin shuts down.
 *
 * When zoning, this is called for all spawns in the zone after @ref OnBeginZone is
 * called.
 *
 * @param pSpawn PSPAWNINFO - The spawn that was removed
 */
PLUGIN_API void OnRemoveSpawn(PSPAWNINFO pSpawn)
{
	// DebugSpewAlways("MQ2ActorsTest::OnRemoveSpawn(%s)", pSpawn->Name);
}

/**
 * @fn OnAddGroundItem
 *
 * This is called each time a ground item is added to a zone (ie, something spawns).
 * It is also called for each existing ground item when a plugin first initializes.
 *
 * When zoning, this is called for all ground items in the zone after @ref OnEndZone
 * is called and before @ref OnZoned is called.
 *
 * @param pNewGroundItem PGROUNDITEM - The ground item that was added
 */
PLUGIN_API void OnAddGroundItem(PGROUNDITEM pNewGroundItem)
{
	// DebugSpewAlways("MQ2ActorsTest::OnAddGroundItem(%d)", pNewGroundItem->DropID);
}

/**
 * @fn OnRemoveGroundItem
 *
 * This is called each time a ground item is removed from a zone (ie, something
 * despawns or is picked up).  It is NOT called when a plugin shuts down.
 *
 * When zoning, this is called for all ground items in the zone after
 * @ref OnBeginZone is called.
 *
 * @param pGroundItem PGROUNDITEM - The ground item that was removed
 */
PLUGIN_API void OnRemoveGroundItem(PGROUNDITEM pGroundItem)
{
	// DebugSpewAlways("MQ2ActorsTest::OnRemoveGroundItem(%d)", pGroundItem->DropID);
}

/**
 * @fn OnBeginZone
 *
 * This is called just after entering a zone line and as the loading screen appears.
 */
PLUGIN_API void OnBeginZone()
{
	// DebugSpewAlways("MQ2ActorsTest::OnBeginZone()");
}

/**
 * @fn OnEndZone
 *
 * This is called just after the loading screen, but prior to the zone being fully
 * loaded.
 *
 * This should occur before @ref OnAddSpawn and @ref OnAddGroundItem are called. It
 * always occurs before @ref OnZoned is called.
 */
PLUGIN_API void OnEndZone()
{
	// DebugSpewAlways("MQ2ActorsTest::OnEndZone()");
}

/**
 * @fn OnZoned
 *
 * This is called after entering a new zone and the zone is considered "loaded."
 *
 * It occurs after @ref OnEndZone @ref OnAddSpawn and @ref OnAddGroundItem have
 * been called.
 */
PLUGIN_API void OnZoned()
{
	// DebugSpewAlways("MQ2ActorsTest::OnZoned()");
}

/**
 * @fn OnUpdateImGui
 *
 * This is called each time that the ImGui Overlay is rendered. Use this to render
 * and update plugin specific widgets.
 *
 * Because this happens extremely frequently, it is recommended to move any actual
 * work to a separate call and use this only for updating the display.
 */
PLUGIN_API void OnUpdateImGui()
{
/*
	if (GetGameState() == GAMESTATE_INGAME)
	{
		if (ShowMQ2ActorsTestWindow)
		{
			if (ImGui::Begin("MQ2ActorsTest", &ShowMQ2ActorsTestWindow, ImGuiWindowFlags_MenuBar))
			{
				if (ImGui::BeginMenuBar())
				{
					ImGui::Text("MQ2ActorsTest is loaded!");
					ImGui::EndMenuBar();
				}
			}
			ImGui::End();
		}
	}
*/
}

/**
 * @fn OnMacroStart
 *
 * This is called each time a macro starts (ex: /mac somemacro.mac), prior to
 * launching the macro.
 *
 * @param Name const char* - The name of the macro that was launched
 */
PLUGIN_API void OnMacroStart(const char* Name)
{
	// DebugSpewAlways("MQ2ActorsTest::OnMacroStart(%s)", Name);
}

/**
 * @fn OnMacroStop
 *
 * This is called each time a macro stops (ex: /endmac), after the macro has ended.
 *
 * @param Name const char* - The name of the macro that was stopped.
 */
PLUGIN_API void OnMacroStop(const char* Name)
{
	// DebugSpewAlways("MQ2ActorsTest::OnMacroStop(%s)", Name);
}

/**
 * @fn OnLoadPlugin
 *
 * This is called each time a plugin is loaded (ex: /plugin someplugin), after the
 * plugin has been loaded and any associated -AutoExec.cfg file has been launched.
 * This means it will be executed after the plugin's @ref InitializePlugin callback.
 *
 * This is also called when THIS plugin is loaded, but initialization tasks should
 * still be done in @ref InitializePlugin.
 *
 * @param Name const char* - The name of the plugin that was loaded
 */
PLUGIN_API void OnLoadPlugin(const char* Name)
{
	// DebugSpewAlways("MQ2ActorsTest::OnLoadPlugin(%s)", Name);
}

/**
 * @fn OnUnloadPlugin
 *
 * This is called each time a plugin is unloaded (ex: /plugin someplugin unload),
 * just prior to the plugin unloading.  This means it will be executed prior to that
 * plugin's @ref ShutdownPlugin callback.
 *
 * This is also called when THIS plugin is unloaded, but shutdown tasks should still
 * be done in @ref ShutdownPlugin.
 *
 * @param Name const char* - The name of the plugin that is to be unloaded
 */
PLUGIN_API void OnUnloadPlugin(const char* Name)
{
	// DebugSpewAlways("MQ2ActorsTest::OnUnloadPlugin(%s)", Name);
}