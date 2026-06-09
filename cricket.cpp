/*
 * Copyright 2026, Kris Beazley Cricket@epluribusunix.net
 * All rights reserved. Distributed under the terms of the MIT license.
 */

// Haiku Core Application & System Kit Headers
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Font.h>
#include <OS.h>
#include <SupportKit.h>
#include <Roster.h>
#include <Slider.h> 
#include <Box.h>

// Storage, Path Finder & System File Kits
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <NodeInfo.h>
#include <Path.h>
#include <FilePanel.h>

// Vector Graphics & Interface Icons Elements
#include <IconUtils.h>   
#include "icons.h"
#include <Bitmap.h>   
#include <TranslationUtils.h> 
#include <MessageRunner.h>  

// Interface Controls & Layout Managers
#include <Button.h>
#include <TextControl.h>
#include <TextView.h>
#include <ListView.h>
#include <OutlineListView.h>
#include <ScrollView.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <LayoutBuilder.h>
#include <SplitView.h>
#include <Alert.h>
#include <CheckBox.h>
#include <MenuField.h>
#include <Cursor.h>  

// Data Types, Sockets & Networking Kit Headers
#include <String.h>
#include <NetworkAddress.h>
#include <SecureSocket.h>
#include <Socket.h>

// Standard C/C++ STL & POSIX Includes
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <map>
#include "nlohmann/json.hpp"



namespace AppInfo {
    static const char* const VERSION_STRING = "Cricket IRC Client v.0.0.17 (Haiku OS)";
}

using json = nlohmann::json;

struct ServerConfig {
    std::string name;
    std::string host;
    uint16 port;
    std::string nick;
    std::string altNick; 
    std::string altNick2; 
    std::string pass;
    std::vector<std::string> autojoin; 
    bool autoConnect; 
    bool autoReconnect;
    bool hideStatusMessages = false;
    bool enableEmoticons;    
    std::string backgroundImagePath; 
    int32 backgroundOpacity; 
    
    int32 serverListFontSize = 12;
    int32 chatLogFontSize = 12;
    int32 userListFontSize = 12;
    bool useCustomDrawFunction = true; 
    bool debugEnable = false; 

    // Per-server Chat Logging Option (Defaults to false)
    bool logChatsToFile = false; 
};


struct Config {
    bool debugEnable = false;
    std::vector<ServerConfig> servers;
    std::vector<ServerConfig> customServers; 
    int32 serverListFontSize = 12;
    int32 chatLogFontSize = 12;
    int32 userListFontSize = 12;
    std::string quitMessage = "App Quit: " + std::string(AppInfo::VERSION_STRING); 
    std::string awayMessage = "I am away from my computer right now."; 
    bool useCustomDrawFunction = true; // Acts as fallback standard for new profiles
} cfg;


int selectedConfig = 0;

void ensure_config_dir() {
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket");
        create_directory(path.Path(), 0755);
    }
}

void save_config() {
    ensure_config_dir();
    json j;
    j["debugEnable"] = cfg.debugEnable;
    j["serverListFontSize"] = cfg.serverListFontSize;
    j["chatLogFontSize"] = cfg.chatLogFontSize;
    j["userListFontSize"] = cfg.userListFontSize;
    j["quitMessage"] = cfg.quitMessage;
    j["awayMessage"] = cfg.awayMessage;
    j["useCustomDrawFunction"] = cfg.useCustomDrawFunction;
    
    // 1. Save standard/default servers
    json serverArray = json::array();
    for (const auto& srv : cfg.servers) {
        json s;        
        s["name"] = srv.name;
        s["host"] = srv.host;
        s["port"] = srv.port;
        s["nick"] = srv.nick;
        s["altNick"] = srv.altNick;
        s["altNick2"] = srv.altNick2; 
        s["pass"] = srv.pass;
        s["autoConnect"] = srv.autoConnect; 
        s["autoReconnect"] = srv.autoReconnect;
        s["hideStatusMessages"] = srv.hideStatusMessages; 
        s["background_image"] = srv.backgroundImagePath; 
        s["bg_opacity"] = srv.backgroundOpacity; 
        s["enable_emoticons"] = srv.enableEmoticons; 
        s["useCustomDrawFunction"] = srv.useCustomDrawFunction;
	    s["logChatsToFile"] = srv.logChatsToFile;

        // Per-server font size settings
        s["serverListFontSize"] = srv.serverListFontSize;
        s["chatLogFontSize"] = srv.chatLogFontSize;
        s["userListFontSize"] = srv.userListFontSize;

        json ajArray = json::array();
        for (const auto& chan : srv.autojoin) {
            ajArray.push_back(chan);
        }
        s["autojoin"] = ajArray;
        serverArray.push_back(s);
    }
    j["servers"] = serverArray;

    // 2. Save custom servers into their own isolated JSON array
    json customServerArray = json::array();
    for (const auto& srv : cfg.customServers) {
        json s;        
        s["name"] = srv.name;
        s["host"] = srv.host;
        s["port"] = srv.port;
        s["nick"] = srv.nick;        
        s["altNick"] = srv.altNick;
        s["altNick2"] = srv.altNick2; 
        s["pass"] = srv.pass;
        s["autoConnect"] = srv.autoConnect; 
        s["autoReconnect"] = srv.autoReconnect;
        s["hideStatusMessages"] = srv.hideStatusMessages; 
        s["background_image"] = srv.backgroundImagePath;
        s["bg_opacity"] = srv.backgroundOpacity; 
        s["enable_emoticons"] = srv.enableEmoticons;  
        s["useCustomDrawFunction"] = srv.useCustomDrawFunction;
 	    s["logChatsToFile"] = srv.logChatsToFile;
 	     
        // Per-server font size settings
        s["serverListFontSize"] = srv.serverListFontSize;
        s["chatLogFontSize"] = srv.chatLogFontSize;
        s["userListFontSize"] = srv.userListFontSize;

        json ajArray = json::array();
        for (const auto& chan : srv.autojoin) {
            ajArray.push_back(chan);
        }
        s["autojoin"] = ajArray;
        customServerArray.push_back(s);
    }
    j["custom_servers"] = customServerArray;

    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket/cricketConfig.txt");
        std::ofstream outfile(path.Path());
        if (outfile.is_open()) {
            outfile << j.dump(4);
            outfile.close();
        }
    }
}



void load_config() {
    ensure_config_dir();
    cfg.debugEnable = false;
    cfg.useCustomDrawFunction = true;

    cfg.servers.clear(); 
    cfg.customServers.clear();
	
    BPath path;
    bool mustSaveDefaults = false;

    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket/cricketConfig.txt");
        std::ifstream infile(path.Path());        
        if (infile.is_open()) {
            try {
                json j = json::parse(infile);
                cfg.quitMessage = j.value("quitMessage", "App Quit: " + std::string(AppInfo::VERSION_STRING));
                cfg.awayMessage = j.value("awayMessage", "I am away from my computer right now.");
                cfg.debugEnable = j.value("debugEnable", false);             
                cfg.serverListFontSize = j.value("serverListFontSize", (int32)12);
                cfg.chatLogFontSize    = j.value("chatLogFontSize", (int32)12);
                cfg.userListFontSize   = j.value("userListFontSize", (int32)12);  
                cfg.useCustomDrawFunction = j.value("useCustomDrawFunction", true);
                
                // Parse standard servers array
                if (j.contains("servers") && j["servers"].is_array()) {
                    for (const auto& s : j["servers"]) {
                        ServerConfig srv;
                        srv.name = s.value("name", "Unknown Server");
                        srv.host = s.value("host", "127.0.0.1");
                        srv.port = s.value("port", (uint16)6697);
                        srv.nick = s.value("nick", "HaikuUser");
                        srv.altNick = s.value("altNick", srv.nick + "+");  
                        srv.altNick2 = s.value("altNick2", srv.nick + "__"); 
                        srv.pass = s.value("pass", "");
                        srv.autoReconnect = s.value("autoReconnect", false); 
                        srv.autoConnect = s.value("autoConnect", false); 
                        srv.hideStatusMessages = s.value("hideStatusMessages", false);
                        srv.backgroundImagePath = s.value("background_image", ""); 
                        srv.backgroundOpacity = s.value("bg_opacity", 30); 
                        srv.enableEmoticons = s.value("enable_emoticons", true); 
                        srv.useCustomDrawFunction = s.value("useCustomDrawFunction", cfg.useCustomDrawFunction);
                        srv.logChatsToFile = s.value("logChatsToFile", false);

                        // Parse per-server fonts; falls back to the global configurations parsed above
                        srv.serverListFontSize = s.value("serverListFontSize", cfg.serverListFontSize);
                        srv.chatLogFontSize    = s.value("chatLogFontSize", cfg.chatLogFontSize);
                        srv.userListFontSize   = s.value("userListFontSize", cfg.userListFontSize);

                        if (s.contains("autojoin") && s["autojoin"].is_array()) {
                            for (const auto& chan : s["autojoin"]) {
                                srv.autojoin.push_back(chan.get<std::string>());
                            }
                        }
                        cfg.servers.push_back(srv);
                    }
                }

                // Parse the custom servers array safely
                if (j.contains("custom_servers") && j["custom_servers"].is_array()) {
                    for (const auto& s : j["custom_servers"]) {
                        ServerConfig srv;
                        srv.name = s.value("name", "Custom Server");
                        srv.host = s.value("host", "127.0.0.1");
                        srv.port = s.value("port", (uint16)6697);
                        srv.nick = s.value("nick", "HaikuUser");                        
                        srv.altNick = s.value("altNick", srv.nick + "+");  
                        srv.altNick2 = s.value("altNick2", srv.nick + "__"); 
                        srv.pass = s.value("pass", "");
                        srv.autoReconnect = s.value("autoReconnect", false); 
                        srv.autoConnect = s.value("autoConnect", false); 
                        srv.hideStatusMessages = s.value("hideStatusMessages", false);
                        srv.backgroundImagePath = s.value("background_image", ""); 
                        srv.backgroundOpacity = s.value("bg_opacity", 30); 
                        srv.enableEmoticons = s.value("enable_emoticons", true); 
                        srv.useCustomDrawFunction = s.value("useCustomDrawFunction", cfg.useCustomDrawFunction);
                        srv.logChatsToFile = s.value("logChatsToFile", false);
                        
                        // Parse per-server fonts; falls back to the global configurations parsed above
                        srv.serverListFontSize = s.value("serverListFontSize", cfg.serverListFontSize);
                        srv.chatLogFontSize    = s.value("chatLogFontSize", cfg.chatLogFontSize);
                        srv.userListFontSize   = s.value("userListFontSize", cfg.userListFontSize);

                        if (s.contains("autojoin") && s["autojoin"].is_array()) {
                            for (const auto& chan : s["autojoin"]) {
                                srv.autojoin.push_back(chan.get<std::string>());
                            }
                        }
                        cfg.customServers.push_back(srv);
                    }
                }
            } catch(...) {
                mustSaveDefaults = true;
            }
            infile.close();
        } else {
            mustSaveDefaults = true;
        }
    }

    if (mustSaveDefaults || cfg.servers.empty()) {
        cfg.servers.clear();        
        cfg.customServers.clear(); 
        cfg.useCustomDrawFunction = true; 
		cfg.quitMessage = "App Quit: " + std::string(AppInfo::VERSION_STRING);
		
        srand(static_cast<unsigned int>(real_time_clock_usecs()));
        int randomSuffix = 1000 + (rand() % 9000);
        BString dynamicNick;
        dynamicNick << "HaikuIRCUser" << randomSuffix;

        ServerConfig libera;
        libera.name = "Libera Chat";
        libera.host = "irc.libera.chat";
        libera.port = 6697;
        libera.nick = dynamicNick.String();
        libera.altNick = BString(dynamicNick).Append("+").String(); 
        libera.altNick2 = BString(dynamicNick).Append("__").String();
        libera.pass = "";
        libera.autojoin = {""};
        libera.autoConnect = false;
        libera.autoReconnect = false;
        libera.hideStatusMessages = false;
        libera.backgroundImagePath = ""; 
        libera.backgroundOpacity = 30; 
        libera.enableEmoticons = true; 
        libera.useCustomDrawFunction = cfg.useCustomDrawFunction;
        libera.logChatsToFile = false;
         
        // Fallbacks for the hardcoded defaults
        libera.serverListFontSize = cfg.serverListFontSize;
        libera.chatLogFontSize    = cfg.chatLogFontSize;
        libera.userListFontSize   = cfg.userListFontSize;
        cfg.servers.push_back(libera);
         
        ServerConfig oftc;
        oftc.name = "OFTC";
        oftc.host = "irc.oftc.net";
        oftc.port = 6697;
        oftc.nick = dynamicNick.String();
        oftc.altNick = BString(dynamicNick).Append("+").String(); 
        oftc.altNick2 = BString(dynamicNick).Append("__").String();
        oftc.pass = "";
        oftc.autojoin = {"#haiku"};
        oftc.autoConnect = false;
        oftc.autoReconnect = false;
        oftc.hideStatusMessages = false;
        oftc.backgroundImagePath = ""; 
        oftc.backgroundOpacity = 30; 
        oftc.enableEmoticons = true; 
        oftc.useCustomDrawFunction = cfg.useCustomDrawFunction;
        oftc.logChatsToFile = false;
        
        oftc.serverListFontSize = cfg.serverListFontSize;
        oftc.chatLogFontSize    = cfg.chatLogFontSize;
        oftc.userListFontSize   = cfg.userListFontSize;
        cfg.servers.push_back(oftc);
    }
}




// Define application messages
enum {
    MSG_START_SIRC       = 'strt',
    MSG_SEND_MESSAGE     = 'send',
    MSG_IRC_RECEIVED     = 'recv',
    MSG_CONNECT_LIBERA   = 'cnlb',
    MSG_TOGGLE_AUTOJOIN  = 'tgaj',
    MSG_TOGGLE_AUTOCONNECT = 'tgac',
    MSG_CONNECT_OFTC     = 'cnof',
    MSG_OPEN_CHAN_LIST   = 'opcl',
    MSG_ADD_LIST_ROW     = 'adlr',
    MSG_CONTEXT_CHAN_LIST = 'cxcl',
    MSG_CLEAR_LIST_ROW   = 'cllr',
    MSG_TOGGLE_HIDE_STATUS = 'tghs',
    MSG_CONTEXT_ABOUT      = 'cxab',
    MSG_TOGGLE_AUTORECONNECT = 'tgar',
    MSG_RECONNECT_SERVER = 'rcsv',
    MSG_CONFIG_SAVE = 'cfsv',
    MSG_CONFIG_CANCEL = 'cfcn',
	MSG_CONTEXT_CONFIGURE_SERVER = 'mccs',
	MSG_SAVE_CONFIG_FILE = 'mscf',
	MSG_CONNECT_CUSTOM_SERVER = 'cncs', 
	MSG_ADD_CUSTOM_SERVER_SUBMIT = 'acss',
	MSG_DISCONNECT_SERVER = 'dscr',
	MSG_TOGGLE_CUSTOM_DRAW = 'tgcd',
	MSG_CLEAR_CUSTOM_BUFFER = 'clcb',
	MSG_TOPIC_CHANGED = 'tpch',
	MSG_CONTEXT_SET_AWAY = 'cxaw', 
	MSG_USER_LIST_CONTEXT_CLICK = 'ulcx',
	MSG_CONTEXT_PRIVMSG = 'cxpm',
	MSG_EMOTE_CLICKED = 'emcl',
	MSG_CONTEXT_OP = 'mCOP',
	MSG_CONTEXT_OP_SUBMIT = 'mOPS',  
    MSG_CONTEXT_TIMED_DEOP_TRIGGER = 'mTDO',
    MSG_CONTEXT_DEOP = 'mCDO',
    MSG_CONTEXT_VOICE = 'mCVO',
    MSG_CONTEXT_DEVOICE = 'mCDV',
    MSG_CONTEXT_KICK = 'mCKC',
    MSG_CONTEXT_KICK_SUBMIT = 'mKCS',
    MSG_CONTEXT_SHOW_BANS   = 'mSBN',
    MSG_CONTEXT_UNBAN_SUBMIT = 'mUBS', 
    MSG_TOGGLE_ICON_POPUP = 'tICP',
    MSG_POPUP_WAS_DESTROYED = 'mPWD',

};



// Forward Declarations 
class CricketWindow; 
class ServerTreeItem; 






class UserListItem : public BStringItem {
public:
    UserListItem(const char* nickname, bool isAway = false)
        : BStringItem(nickname), fIsAway(isAway) {}

    void SetAway(bool isAway) { fIsAway = isAway; }
    bool IsAway() const { return fIsAway; }

    // Custom drawing hook to render text dim/lighter when away
    virtual void DrawItem(BView* owner, BRect frame, bool complete = false) override {
        // Clear background row depending on selection state
        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
        } else {
            owner->SetHighColor(ui_color(B_LIST_BACKGROUND_COLOR));
        }
        owner->FillRect(frame);

        // Adjust text coloring based on selection and away status
        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
        } else if (fIsAway) {
            // Lighten the text. Adjust the RGB values (140, 140, 140) to taste!
            rgb_color awayColor = { 140, 140, 140, 255 }; 
            owner->SetHighColor(awayColor);
        } else {
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
        }

        // Standard Haiku font alignment math
        font_height fh;
        owner->GetFontHeight(&fh);
        float textHeight = fh.ascent + fh.descent;
        
        owner->MovePenTo(frame.left + 4, frame.top + fh.ascent + (frame.Height() - textHeight) / 2);
        owner->DrawString(Text());
    }

private:
    bool fIsAway;
};


enum FragmentType {
    FRAG_TEXT,
    FRAG_ICON
};

// 1. Holds individual text slices after word-wrapping processing
struct StyledRunFragment {
    FragmentType type;
    BString      subText;   
    const uint8* iconData;  
    BBitmap*     cachedBitmap; // <-- ADDED: Holds the pre-rendered bitmap asset
    float        width;     
    BFont        font;
    rgb_color    color;

    StyledRunFragment() : type(FRAG_TEXT), iconData(nullptr), cachedBitmap(nullptr), width(0.0f) {}
    
    // <-- ADDED: Guarantees leak-free automatic memory cleanup
    ~StyledRunFragment() {
        delete cachedBitmap; 
    }
};



// 2. Holds the raw message, its raw style runs, and its calculated display rows
struct StyledLine {
    BStringItem* itemNode; // <-- ADDED: Binds this specific text entry to its channel context node
    BString text;
    text_run_array* runs;
    
    // Explicit template parameters handle nested pointer cleanup automatically
    BObjectList<BObjectList<StyledRunFragment, true>, true> wrappedRows;

    // <-- Constructor now maps the item node tracking layout context
    StyledLine(BStringItem* node, BString t, const text_run_array* r) : wrappedRows(2) {
        itemNode = node;
        text = t;
        if (r != nullptr) {
            size_t size = sizeof(text_run_array) + (sizeof(text_run) * (r->count - 1));
            runs = (text_run_array*)malloc(size);
            if (runs != nullptr) {
                memcpy(runs, r, size);
            }
        } else {
            runs = nullptr;
        }
    }
    
    ~StyledLine() {
        free(runs);
        wrappedRows.MakeEmpty();
    }
};


// 3. Clean Interface Declaration Block (Contains NO method bodies)
class CustomChatView : public BView {
public:
    CustomChatView(BRect frame, const char* name, uint32 resizingMode, uint32 flags);
    virtual ~CustomChatView(); 

    virtual void Draw(BRect updateRect);
    virtual void FrameResized(float newWidth, float newHeight);
    virtual void MouseDown(BPoint point);

    void AddStyledLine(BStringItem* itemNode, const BString& text, const text_run_array* runs);
    void ClearAllLines();    
    void SetLineHeight(float height);
    void RecalculateAllLineWraps();
    void SetActiveChannel(BStringItem* activeNode);
    void SetBackgroundImage(const char* filePath);
    void SetBackgroundDimming(int32 level); 
    virtual void MessageReceived(BMessage* message); 
    virtual void MouseMoved(BPoint point, uint32 transit, const BMessage* dragMessage) override;


private:
    void ParseTextAndIcons(const BString& text, const text_run_array* runs, BObjectList<StyledRunFragment, true>* rawFragments);
    void ComputeWrapForLine(StyledLine* line, float maxWidth, BObjectList<StyledRunFragment, true>* rawFragments);
    virtual void ScrollTo(BPoint point);
    void ComputeWrapForLine(StyledLine* line, float maxWidth);
	void UpdateScrollRange(bool scrollToBottom = false);
    BObjectList<StyledLine, true> fLines; 
    BStringItem*                  fActiveChannelNode;
    float                         fLineHeight;
    BBitmap* fBackgroundBitmap;
    int32    fBackgroundDimmingLevel; 
};




// Constructor
// Update Constructor to handle initial default allocation state 
// Constructor
// Update Constructor to handle initial default allocation state 
CustomChatView::CustomChatView(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
    : BView(frame, name, resizingMode, flags | B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
      fLines(20),
      fBackgroundBitmap(nullptr),
      fBackgroundDimmingLevel(30) 
{
    fLineHeight = 16.0; 
    fActiveChannelNode = nullptr;
    
    // SetViewColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
    SetViewColor(B_TRANSPARENT_COLOR);
    SetLowColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
}



void CustomChatView::ParseTextAndIcons(const BString& text, const text_run_array* runs, BObjectList<StyledRunFragment, true>* rawFragments)
{
    // --- NEW: RUNTIME TOGGLE ISOLATION RULE ---
    bool emoticonsEnabled = true;
    
    // Look up whether the active view belongs to a default or user-added server configuration profile
    // Uses the global index layout tracker variable "selectedConfig"
    if (selectedConfig < (int)cfg.servers.size()) {
        emoticonsEnabled = cfg.servers[selectedConfig].enableEmoticons;
    } else {
        int customIdx = selectedConfig - cfg.servers.size();
        if (customIdx >= 0 && customIdx < (int)cfg.customServers.size()) {
            emoticonsEnabled = cfg.customServers[customIdx].enableEmoticons;
        }
    }

    // Fallback: If emoticons are turned off for this server, process the raw string as text and break instantly!
    if (!emoticonsEnabled) {
        int32 runCount = (runs != nullptr && runs->count > 0) ? runs->count : 1;
        BFont baseFont;
        GetFont(&baseFont);

        for (int32 i = 0; i < runCount; i++) {
            int32 startPos = (runs != nullptr) ? runs->runs[i].offset : 0;
            int32 endPos = text.Length();
            BFont runFont = baseFont;
            rgb_color runColor = {255, 255, 255, 255};

            if (runs != nullptr && runs->count > 0) {
                runFont = runs->runs[i].font;
                runColor = runs->runs[i].color;
                if (i + 1 < runs->count) endPos = runs->runs[i+1].offset;
            }

            if (startPos >= text.Length() || endPos <= startPos) continue;

            StyledRunFragment* frag = new StyledRunFragment();
            frag->type = FRAG_TEXT;
            text.CopyInto(frag->subText, startPos, endPos - startPos);
            frag->font = runFont;
            frag->color = runColor;
            frag->width = runFont.StringWidth(frag->subText.String());
            rawFragments->AddItem(frag);
        }
        return; // Exit early safely
    }
    
    
    struct EmoteMap { const char* trigger; const uint8* data; };
    EmoteMap emotes[] = {
        // --- HAPPY / SMILING / LAUGHING ---
        {":)", (const uint8*)kIconSmile},      {":-)", (const uint8*)kIconSmile},
        {"^^", (const uint8*)kIconCheerful},   {"^_^", (const uint8*)kIconCheerful},
        {":D", (const uint8*)kIconLaughing},   {":-D", (const uint8*)kIconLaughing},
        {":d", (const uint8*)kIconLaughing},   {"lol", (const uint8*)kIconLaughing},  
        {"LOL", (const uint8*)kIconLaughing},

        // --- SAD / FROWNING / ANNOYED ---
        {":|", (const uint8*)kIconConfused},   {":-|", (const uint8*)kIconConfused},
        {":/", (const uint8*)kIconConfused},   {":-\\", (const uint8*)kIconConfused},
        {"o_O", (const uint8*)kIconConfused},  {"O_o", (const uint8*)kIconConfused},
        {":(", (const uint8*)kIconFrown},      {":-(", (const uint8*)kIconFrown},
        {">_ <", (const uint8*)kIconAnnoyed},  {">_(", (const uint8*)kIconAnnoyed},
        {">_<", (const uint8*)kIconAnnoyed},

        // --- CRYING / SCONXT ---
        {";'(", (const uint8*)kIconCrying},    {";-'(", (const uint8*)kIconCrying},
        {"T_T", (const uint8*)kIconCrying},    {";_;", (const uint8*)kIconCrying},

        // --- WINKING / PLAYFUL TONGUE ---
        {";)", (const uint8*)kIconWink},       {";-)", (const uint8*)kIconWink},
        {":P", (const uint8*)kIconTongue},     {":p", (const uint8*)kIconTongue},
        {":-P", (const uint8*)kIconTongue},    {":-p", (const uint8*)kIconTongue},

        // --- SHOCK / ASTONISHED / SURPRISED ---
        {":o", (const uint8*)kIconAstonished}, {":O", (const uint8*)kIconAstonished},
        {":-o", (const uint8*)kIconAstonished},{":-O", (const uint8*)kIconAstonished},
        {":0", (const uint8*)kIconAstonished}, {":-0", (const uint8*)kIconAstonished},
        {"O_O", (const uint8*)kIconWideeyed},  {"o_o", (const uint8*)kIconWideeyed},

        // --- COOL / SUNGLASSES ---
        {"8-)", (const uint8*)kIconSunglasses}, {"B)", (const uint8*)kIconSunglasses},
        {"8)", (const uint8*)kIconSunglasses},  {"b)", (const uint8*)kIconSunglasses},

        // --- MISC ROMANTIC / SECRETS ---
        {"<3", (const uint8*)kIconHeart},       {":*", (const uint8*)kIconKiss},       
        {":-*", (const uint8*)kIconKiss},       {":X", (const uint8*)kIconFrown},      
        {":x", (const uint8*)kIconFrown}
    };
    // FIX A: Correctly divide by the size of a single row element slot
    int32 emoteCount = sizeof(emotes) / sizeof(emotes[0]);

    BFont baseFont;
    GetFont(&baseFont);
    
    int32 runCount = (runs != nullptr && runs->count > 0) ? runs->count : 1;
    
    for (int32 i = 0; i < runCount; i++) {
        int32 startPos = 0;
        int32 endPos = text.Length();
        
        BFont runFont = baseFont;
        rgb_color runColor = {255, 255, 255, 255}; 
        
        if (runs != nullptr && runs->count > 0) {
            startPos = runs->runs[i].offset;
            runFont = runs->runs[i].font;
            runColor = runs->runs[i].color;
            
            if (i + 1 < runs->count) {
                endPos = runs->runs[i+1].offset;
            }
        }
        
        if (startPos >= text.Length() || endPos <= startPos) {
            continue;
        }

        BString runText;
        text.CopyInto(runText, startPos, endPos - startPos);
        
        int32 currentPos = 0;
        int32 runLength = runText.Length();
        
        while (currentPos < runLength) {
            int32 nextTrigger = runLength;
            int32 triggerLength = 0;
            const uint8* foundIcon = nullptr;
            
            for (int32 e = 0; e < emoteCount; e++) {
                int32 pos = runText.FindFirst(emotes[e].trigger, currentPos);
                if (pos != B_ERROR && pos < nextTrigger) {
                    
                    bool isValidMatch = true;
                    int32 trigLen = strlen(emotes[e].trigger);
                    
                    // FIX B: Only run the timestamp filter if the trigger begins with a colon!
                    if (emotes[e].trigger[0] == ':' && pos + 1 < runLength) {
                        char nextChar = runText.ByteAt(pos + 1);
                        if (isdigit(nextChar)) {
                            isValidMatch = false;
                        }
                    }

                    // Word boundary isolation filters
                    if (isValidMatch && pos > 0) {
                        char prevChar = runText.ByteAt(pos - 1);
                        if (isalnum(prevChar) || prevChar == '/') {
                            isValidMatch = false;
                        }
                    }
                    if (isValidMatch && (pos + trigLen < runLength)) {
                        char nextChar = runText.ByteAt(pos + trigLen);
                        if (isalnum(nextChar) || nextChar == '/') {
                            isValidMatch = false;
                        }
                    }
                    
                    if (isValidMatch) {
                        nextTrigger = pos;
                        triggerLength = trigLen;
                        foundIcon = emotes[e].data;
                    }
                }
            }
            
            if (foundIcon != nullptr) {
                if (nextTrigger > currentPos) {
                    StyledRunFragment* frag = new StyledRunFragment();
                    frag->type = FRAG_TEXT;
                    runText.CopyInto(frag->subText, currentPos, nextTrigger - currentPos);
                    frag->font = runFont;
                    frag->color = runColor;
                    frag->width = runFont.StringWidth(frag->subText.String());
                    rawFragments->AddItem(frag);
                }
                
                StyledRunFragment* frag = new StyledRunFragment();
                frag->type = FRAG_ICON;
                frag->iconData = foundIcon;
                frag->font = runFont;
                frag->color = runColor;
                frag->width = fLineHeight; 

                BRect renderBounds(0, 0, fLineHeight, fLineHeight);
                frag->cachedBitmap = new BBitmap(renderBounds, B_RGBA32);
                if (BIconUtils::GetVectorIcon(frag->iconData, 8192, frag->cachedBitmap) != B_OK) {
                    delete frag->cachedBitmap;
                    frag->cachedBitmap = nullptr;
                }
                
                rawFragments->AddItem(frag);
                currentPos = nextTrigger + triggerLength;
            } else {
                if (currentPos < runLength) {
                    StyledRunFragment* frag = new StyledRunFragment();
                    frag->type = FRAG_TEXT;
                    runText.CopyInto(frag->subText, currentPos, runLength - currentPos);
                    frag->font = runFont;
                    frag->color = runColor;
                    frag->width = runFont.StringWidth(frag->subText.String());
                    rawFragments->AddItem(frag);
                }
                break;
            }
        }
    }
}


void CustomChatView::SetBackgroundDimming(int32 level)
{
    fBackgroundDimmingLevel = level;
    if (fBackgroundDimmingLevel < 0) fBackgroundDimmingLevel = 0;
    if (fBackgroundDimmingLevel > 100) fBackgroundDimmingLevel = 100;
    Invalidate(); // Refresh canvas
}

void CustomChatView::ClearAllLines() {
    fLines.MakeEmpty();
    Invalidate();
}

void CustomChatView::SetLineHeight(float height) {
    fLineHeight = height;
    Invalidate();
}

void CustomChatView::FrameResized(float newWidth, float newHeight) {
    BView::FrameResized(newWidth, newHeight);
    RecalculateAllLineWraps();
    UpdateScrollRange(false); 
    Invalidate();
}

CustomChatView::~CustomChatView() {
	delete fBackgroundBitmap; 
    fLines.MakeEmpty();
}
// Pipeline Line Addition Point
void CustomChatView::AddStyledLine(BStringItem* itemNode, const BString& text, const text_run_array* runs) {
    if (itemNode == nullptr) return;

    // Create the line tagged to its specific target context
    StyledLine* newLine = new StyledLine(itemNode, text, runs);
    fLines.AddItem(newLine);

    // Only spend CPU cycles compute-wrapping the text layout if it belongs to the visible channel
    if (itemNode == fActiveChannelNode) {
        float maxWidth = Bounds().Width() - 20.0f;
        if (maxWidth <= 50.0f) maxWidth = 50.0f;

        // Build a temporary flat collection of fragments (Text + Emotes mixed)
        BObjectList<StyledRunFragment, true> rawFragments(10);
        ParseTextAndIcons(text, runs, &rawFragments);

        // Compute the wrap based on those parsed tokens
        ComputeWrapForLine(newLine, maxWidth, &rawFragments);
        
        UpdateScrollRange(true); 
        Invalidate();
    }
}


// Update Loop Filter Pass
void CustomChatView::RecalculateAllLineWraps()
{
    float maxWidth = Bounds().Width() - 20.0f;
    if (maxWidth <= 50.0f) maxWidth = 50.0f;

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        
        // Only spend CPU resources layout-wrapping lines for the active view context
        if (line->itemNode == fActiveChannelNode) {
            // Re-parse the text and icons into fresh fragments for wrapping
            BObjectList<StyledRunFragment, true> rawFragments(10);
            ParseTextAndIcons(line->text, line->runs, &rawFragments);

            // Call the updated 3-parameter function signature
            ComputeWrapForLine(line, maxWidth, &rawFragments);
        }
    }
    UpdateScrollRange(false);
    Invalidate();
}


void CustomChatView::UpdateScrollRange(bool scrollToBottom) {
    // 1. Locate the vertical scrollbar attached to our view parent frame container
    BScrollBar* vScroll = ScrollBar(B_VERTICAL);
    if (vScroll == nullptr) return;

    // 2. Count the total row count for the currently active visible room
    int32 totalActiveRows = 0;
    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        if (line != nullptr && line->itemNode == fActiveChannelNode) {
            totalActiveRows += line->wrappedRows.CountItems();
        }
    }

    // 3. Compute structural height limits
    float totalDocumentHeight = (totalActiveRows * fLineHeight) + 20.0f; // includes visual gutters
    float viewHeight = Bounds().Height();

    // 4. Update the scrollbar range bounds properties
    if (totalDocumentHeight <= viewHeight) {
        // Everything fits on one screen; disable the scrollbar
        vScroll->SetRange(0.0f, 0.0f);
        vScroll->SetProportion(1.0f);
    } else {
        // Document overflows viewport; set maximum scroll bound target
        float maxScrollValue = totalDocumentHeight - viewHeight;
        vScroll->SetRange(0.0f, maxScrollValue);
        
        // Set the size ratio of the scrollbar thumb relative to full canvas height
        vScroll->SetProportion(viewHeight / totalDocumentHeight);

        // 5. Fire auto-scroll step if flagged or if the user is trailing closely at the bottom
        if (scrollToBottom) {
            vScroll->SetValue(maxScrollValue);
        }
    }
}


void CustomChatView::ComputeWrapForLine(StyledLine* line, float maxWidth, BObjectList<StyledRunFragment, true>* rawFragments)
{
    if (line == nullptr) 
        return;
        
    line->wrappedRows.MakeEmpty();

    if (maxWidth <= 0 || rawFragments == nullptr || rawFragments->IsEmpty()) 
        return;

    BObjectList<StyledRunFragment, true>* currentRow = new BObjectList<StyledRunFragment, true>(5);
    float currentX = 0.0f;

    while (!rawFragments->IsEmpty()) {
        StyledRunFragment* frag = rawFragments->RemoveItemAt(0);
        
        if (frag->type == FRAG_ICON || currentX + frag->width <= maxWidth) {
            currentRow->AddItem(frag);
            currentX += frag->width;
        } else {
            BFont font = frag->font;
            float availableWidth = maxWidth - currentX;
            int32 charCount = 0;
            int32 textLen = frag->subText.Length();
            const char* rawStr = frag->subText.String();
            
            // High-performance string measurement boundary check using byte lengths directly
            for (int32 c = 1; c <= textLen; c++) {
                if (font.StringWidth(rawStr, c) <= availableWidth) {
                    charCount = c;
                } else {
                    break;
                }
            }
            
            if (charCount > 0) {
                StyledRunFragment* leftPart = new StyledRunFragment();
                leftPart->type = FRAG_TEXT;
                leftPart->font = frag->font;
                leftPart->color = frag->color;
                frag->subText.CopyInto(leftPart->subText, 0, charCount);
                leftPart->width = font.StringWidth(leftPart->subText.String());
                currentRow->AddItem(leftPart);
                
                frag->subText.Remove(0, charCount);
                frag->width = font.StringWidth(frag->subText.String());
            }
            
            line->wrappedRows.AddItem(currentRow);
            
            currentRow = new BObjectList<StyledRunFragment, true>(5);
            currentRow->AddItem(frag);
            currentX = frag->width;
        }
    }
    
    if (!currentRow->IsEmpty()) {
        line->wrappedRows.AddItem(currentRow);
    } else {
        delete currentRow;
    }
}

void CustomChatView::ScrollTo(BPoint point) {
    BView::ScrollTo(point);
    Invalidate(); 
}



// Channel Room Switcher Method
void CustomChatView::SetActiveChannel(BStringItem* activeNode) {
    if (fActiveChannelNode != activeNode) {
        fActiveChannelNode = activeNode;
        // Re-wrap rows because a different chat window layout might require different view space setups
        RecalculateAllLineWraps(); 
        UpdateScrollRange(true); 
        Invalidate();
    }
}

// Intercept System-wide Color/Theme Changes Live
void CustomChatView::MessageReceived(BMessage* message) {
    switch (message->what) {
        case B_MOUSE_WHEEL_CHANGED: {
            float deltaY = 0.0f;
            if (message->FindFloat("be:wheel_delta_y", &deltaY) == B_OK && deltaY != 0.0f) {
                
                BScrollBar* vScrollBar = ScrollBar(B_VERTICAL);
                if (vScrollBar != nullptr) {
                    float currentVal = vScrollBar->Value();
                    
                    // Proportional rapid-scrolling multiplier calculation
                    float acceleratedScrollAmount = deltaY * (fLineHeight * 1.5f);
                    
                    vScrollBar->SetValue(currentVal + acceleratedScrollAmount);

                    // FIX: Force a global redraw of the view layout hierarchy.
                    // This tells the App Server to refresh the background image canvas,
                    // completely eliminating text smearing and tearing when scrolling.
                    Invalidate();
                    return; 
                }
            }
            break;
        }



        case MSG_CLEAR_CUSTOM_BUFFER: {
            // 1. Loop backward and purge only rows matching our current active room context node
            for (int32 i = fLines.CountItems() - 1; i >= 0; i--) {
                StyledLine* line = fLines.ItemAt(i);
                if (line != nullptr && line->itemNode == fActiveChannelNode) {
                    delete fLines.RemoveItemAt(i);
                }
            }

            // 2. Alert the parent main window to completely wipe out the background text string cache maps
            if (Window() != nullptr && fActiveChannelNode != nullptr) {
                BMessage clearCacheMsg('clch'); 
                clearCacheMsg.AddPointer("active_node", fActiveChannelNode); 
                Window()->PostMessage(&clearCacheMsg);
            }

            UpdateScrollRange(false);
            Invalidate();
            break;
        }


        
        
        
        
        

        case B_COLORS_UPDATED: {
            SetViewColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
            SetLowColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
            Invalidate();
            break;
        }
        
        default:
            BView::MessageReceived(message);
            break;
    }
}



void CustomChatView::Draw(BRect updateRect) {
    rgb_color systemBgColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
    rgb_color systemTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);

    // --- BRANCH A: SCALED CUSTOM BACKGROUND REFRESH ---
    if (fBackgroundBitmap != nullptr) {
        SetDrawingMode(B_OP_COPY);
        DrawBitmap(fBackgroundBitmap, Bounds());

        // --- NEW: HIGH-PERFORMANCE DIMMING OVERLAY PASS ---
        if (fBackgroundDimmingLevel > 0) {
            uint8 alphaIntensity = (uint8)((fBackgroundDimmingLevel / 100.0f) * 255.0f);
            rgb_color dimColor = { 0, 0, 0, alphaIntensity };
            
            SetHighColor(dimColor);
            SetDrawingMode(B_OP_ALPHA); 
            FillRect(Bounds());
            SetDrawingMode(B_OP_COPY);  
        }
    } else {
        SetHighColor(systemBgColor); 
        FillRect(updateRect); 
    }


    // --- NEW ELEMENT: INITIAL APP BOOT EMPTY STATE OVERLAY ---
    if (fActiveChannelNode == nullptr) {
        BRect bounds = Bounds();
        float centerX = bounds.Width() / 2.0f;
        float centerY = bounds.Height() / 2.0f;

        // 1. Draw your custom kIconCricket data asset dynamically via BMemoryIO
        float iconDimension = 64.0f;
        BBitmap* customIcon = new BBitmap(BRect(0, 0, iconDimension - 1.0f, iconDimension - 1.0f), B_RGBA32);

        if (customIcon != nullptr && customIcon->InitCheck() == B_OK) {
            status_t err = BIconUtils::GetVectorIcon(kIconCricket, kIconkIconCricketSize, customIcon);
            if (err == B_OK) {
                BPoint iconDrawPoint(centerX - (iconDimension / 2.0f), centerY - (iconDimension / 2.0f) - 45.0f);
                SetDrawingMode(B_OP_ALPHA);
                DrawBitmap(customIcon, iconDrawPoint);
                SetDrawingMode(B_OP_COPY);
            }
        }
        delete customIcon;

        // 2. Draw "Welcome to Cricket IRC Client" Main Heading
        BString welcomeText = "Welcome to Cricket IRC Client";
        BFont titleFont;
        GetFont(&titleFont);
        titleFont.SetSize(15.0f);
        titleFont.SetFace(B_BOLD_FACE);
        SetFont(&titleFont);
        SetHighColor(220, 220, 220, 255); // Brighter text color for prominence

        float titleWidth = titleFont.StringWidth(welcomeText.String());
        font_height tfh;
        titleFont.GetHeight(&tfh);
        
        BPoint titleDrawPoint(centerX - (titleWidth / 2.0f), centerY + 15.0f);
        SetDrawingMode(B_OP_ALPHA);
        DrawString(welcomeText.String(), titleDrawPoint);

        // 3. Draw Subtitle Note About Icons Panel
        BString infoText = "Dum Vivimus Vivamus";
        BFont infoFont;
        GetFont(&infoFont);
        infoFont.SetSize(12.0f); // Muted, smaller font size for layout contrast
        infoFont.SetFace(B_BOLD_FACE | B_ITALIC_FACE);
        SetFont(&infoFont);
        SetHighColor(140, 140, 140, 255); // Softer gray tone

        float infoWidth = infoFont.StringWidth(infoText.String());
        font_height ifh;
        infoFont.GetHeight(&ifh);

        BPoint infoDrawPoint(centerX - (infoWidth / 2.0f), centerY + 40.0f);
        DrawString(infoText.String(), infoDrawPoint);

        // 4. Draw Centered Link Action Prompt String "Join #Haiku"
        BString promptText = "Join #Haiku";
        BFont promptFont;
        GetFont(&promptFont);
        promptFont.SetSize(13.0f);
        promptFont.SetFace(B_BOLD_FACE);
        SetFont(&promptFont);
        SetHighColor(114, 172, 230, 255); // Subtle link blue for visibility contrast

        float stringWidth = promptFont.StringWidth(promptText.String());
        font_height pfh;
        promptFont.GetHeight(&pfh);

        BPoint textDrawPoint(centerX - (stringWidth / 2.0f), centerY + 75.0f);
        DrawString(promptText.String(), textDrawPoint);
        SetDrawingMode(B_OP_COPY);

        return; // Terminate execution layout loops early
    }



    // --- BRANCH B: INLINE CHAT RENDERING ---
    float currentY = 10.0f; 
    font_height fh;
    GetFontHeight(&fh);
    float fontAscent = fh.ascent;

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        if (!line || line->itemNode != fActiveChannelNode) continue;

        for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            if (currentY + fLineHeight >= updateRect.top && currentY <= updateRect.bottom) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    SetFont(&(frag->font));

                    if (frag->type == FRAG_TEXT) {
                        rgb_color renderColor = frag->color;
                        if ((renderColor.red == 255 && renderColor.green == 255 && renderColor.blue == 255) ||
                            (renderColor.red == 0   && renderColor.green == 0   && renderColor.blue == 0)) {
                            renderColor = systemTextColor;
                        }
                        SetHighColor(renderColor);
                        
                        SetDrawingMode(B_OP_ALPHA);
                        DrawString(frag->subText.String(), BPoint(currentX, currentY + fLineHeight - 2.0f));
                        SetDrawingMode(B_OP_COPY);
                        
                        currentX += StringWidth(frag->subText.String());
                    } 
                    else if (frag->type == FRAG_ICON) {
                        BRect iconRect(currentX, 
                                       currentY + fLineHeight - fontAscent - 2.0f, 
                                       currentX + fLineHeight, 
                                       currentY + fLineHeight + fh.descent - 2.0f);
                        
                        if (frag->cachedBitmap != nullptr) {
                            SetDrawingMode(B_OP_ALPHA);
                            DrawBitmap(frag->cachedBitmap, iconRect);
                            SetDrawingMode(B_OP_COPY);
                        }
                        currentX += frag->width + 2.0f;
                    }
                }
            }
            currentY += fLineHeight;
        }
    }
}





// Scoped layout mouse hover tracking engine for dynamic cursor feedback
void CustomChatView::MouseMoved(BPoint point, uint32 transit, const BMessage* dragMessage) {
    // 1. Handle clean exit trajectories right away to prevent stuck cursors
    if (transit == B_EXITED_VIEW) {
        BCursor defaultCursor(B_CURSOR_ID_SYSTEM_DEFAULT);
        SetViewCursor(&defaultCursor);
        BView::MouseMoved(point, transit, dragMessage);
        return;
    }

    // =========================================================================
    // NEW: INITIAL APP BOOT HOVER CURSOR TRACKER CHECK
    // =========================================================================
    if (fActiveChannelNode == nullptr) {
        BRect bounds = Bounds();
        float centerX = bounds.Width() / 2.0f;
        float centerY = bounds.Height() / 2.0f;
        float iconDimension = 64.0f;

        // Map out the exact combined interactive box wrapping both the icon and text fields
        BRect clickTargetBox(
            centerX - 60.0f,
            centerY + 60.0f, // Narrowed precisely over the new Blue Link location
            centerX + 60.0f,
            centerY + 95.0f
        );


        if (clickTargetBox.Contains(point)) {
            BCursor linkCursor(B_CURSOR_ID_FOLLOW_LINK);
            SetViewCursor(&linkCursor);
            BView::MouseMoved(point, transit, dragMessage);
            return;
        }
    }

    bool isHoveringOverLink = false;
    float currentY = 10.0f;

    // 2. Spatial matching mirror layout loop from your MouseDown implementation
    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        
        // --- CHANNEL ISOLATION CHECK ---
        if (!line || line->itemNode != fActiveChannelNode) continue;

        for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            // Check if mouse Y coordinate aligns with this row's bounding box
            if (point.y >= currentY && point.y <= (currentY + fLineHeight)) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    // Measure font segment width dynamically
                    SetFont(&(frag->font));
                    float segmentWidth = StringWidth(frag->subText.String());

                    // Check if segment is a link (Underlined) and X intersects
                    if (frag->font.Face() & B_UNDERSCORE_FACE) {
                        if (point.x >= currentX && point.x <= (currentX + segmentWidth)) {
                            isHoveringOverLink = true;
                            break;
                        }
                    }
                    currentX += segmentWidth;
                }
                break;
            }
            currentY += fLineHeight;
        }
        if (isHoveringOverLink) break;
    }

    // 3. Swap the system cursor icon state seamlessly based on coordinates matching outcome
    if (isHoveringOverLink) {
        BCursor linkCursor(B_CURSOR_ID_FOLLOW_LINK);
        SetViewCursor(&linkCursor);
    } else {
        BCursor defaultCursor(B_CURSOR_ID_SYSTEM_DEFAULT);
        SetViewCursor(&defaultCursor);
    }

    // Call the base class implementation to preserve default system drag/drop pipelines
    BView::MouseMoved(point, transit, dragMessage);
}




// Scoped layout mouse click tracker logic implementation with Channel Filtering Isolation
void CustomChatView::MouseDown(BPoint point) {
    if (Window() == nullptr) return;

    // =========================================================================
    // NEW: INITIAL APP BOOT HOVER CLICK TARGET CHECK
    // =========================================================================
    if (fActiveChannelNode == nullptr) {
        BRect bounds = Bounds();
        float centerX = bounds.Width() / 2.0f;
        float centerY = bounds.Height() / 2.0f;
        float iconDimension = 64.0f;

        // Map out the exact combined interactive box wrapping both the icon and text fields
        BRect clickTargetBox(
            centerX - 60.0f,
            centerY + 60.0f, // Narrowed precisely over the new Blue Link location
            centerX + 60.0f,
            centerY + 95.0f
        );


        if (clickTargetBox.Contains(point)) {
            int32 buttons = 0;
            Window()->CurrentMessage()->FindInt32("buttons", &buttons);
            
            // Only fire the initialization routine on primary left clicks
            if (buttons == B_PRIMARY_MOUSE_BUTTON) {
                BWindow* parentWindow = Window();
                if (parentWindow != nullptr) {
                    BMessage joinNotice(MSG_START_SIRC);
                    parentWindow->PostMessage(&joinNotice);
                }
            }
            return; // Terminate early so context menus don't intercept this empty-state zone
        }
    }
    
    int32 buttons = 0;
    Window()->CurrentMessage()->FindInt32("buttons", &buttons);

    // =========================================================================
    // RIGHT-CLICK CONTEXT MENU: Trigger on secondary mouse clicks
    // =========================================================================
    if (buttons == B_SECONDARY_MOUSE_BUTTON) {
        // Instantiate a standard pop-up context panel asynchronously
        BPopUpMenu* contextMenu = new BPopUpMenu("ChatViewContext", false, false);
        
        // Add the Clear option tied to our unique identifier token constant
        BMenuItem* clearItem = new BMenuItem("Clear Buffer", new BMessage(MSG_CLEAR_CUSTOM_BUFFER));
        contextMenu->AddItem(clearItem);
        
        // Command the menu to target this view context instance specifically
        contextMenu->SetTargetForItems(this);
        
        // Open the menu instantly right where the mouse cursor landed on the screen canvas
        contextMenu->Go(ConvertToScreen(point), true, true, true);
        return;
    }

    // =========================================================================
    // LEFT-CLICK LINK TRACKER: Your existing logic remains perfectly intact
    // =========================================================================
    if (buttons != B_PRIMARY_MOUSE_BUTTON) return;

    float currentY = 10.0f;

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        
        // --- CHANNEL ISOLATION CHECK ---
        // Prevents clicks from registering on hidden lines belonging to background chats
        if (!line || line->itemNode != fActiveChannelNode) continue;

        for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            if (point.y >= currentY && point.y <= (currentY + fLineHeight)) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    SetFont(&(frag->font));
                    float segmentWidth = StringWidth(frag->subText.String());

                    // Check for underline style assignment
                    if (frag->font.Face() & B_UNDERSCORE_FACE) {
                        if (point.x >= currentX && point.x <= (currentX + segmentWidth)) {
                            BString url = frag->subText;
                            url.Trim();
                            char* args = (char*)url.String();
                            be_roster->Launch("text/html", 1, &args);
                            return;
                        }
                    }
                    currentX += segmentWidth;
                }
                return;
            }
            currentY += fLineHeight;
        }
    }
}


void CustomChatView::SetBackgroundImage(const char* filePath)
{
    delete fBackgroundBitmap;
    fBackgroundBitmap = nullptr;

    if (filePath != nullptr) {
        fBackgroundBitmap = BTranslationUtils::GetBitmap(filePath);
    }

    Invalidate();
}



static void LogDebugStream(const char* serverName, const char* direction, const char* rawData, int32 dataLength) {
    // Escape early if global debug flag isn't active
    if (!cfg.debugEnable) return;

    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket/cricket_debug_log.txt");
        
        // Open the file in output-append mode
        std::ofstream logFile(path.Path(), std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            // Generate a matching timestamp prefix for the raw data row
            bigtime_t currentTime = real_time_clock_usecs();
            time_t rawTime = (time_t)(currentTime / 1000000);
            struct tm* timeInfo = localtime(&rawTime);
            
            char timeBuffer[32];
            if (timeInfo != nullptr) {
                strftime(timeBuffer, sizeof(timeBuffer), "[%Y-%m-%d %H:%M:%S] ", timeInfo);
                logFile << timeBuffer;
            }

            // Clean data boundaries to prevent printing binary junk characters
            BString cleanBuffer(rawData, dataLength);
            cleanBuffer.ReplaceAll("\r", "");
            cleanBuffer.ReplaceAll("\n", " "); // Flatten multi-line packets into clean singular rows

            // Write the formatted log row transaction
            logFile << "[" << serverName << "] " << direction << ": " << cleanBuffer.String() << "\n";
            logFile.close();
        }
    }
}



class AddServerWindow : public BWindow {
public:
    AddServerWindow(BWindow* targetWindow) 
        // Replaced hardcoded dimensions with a (0,0,1,1) dummy rect to let B_AUTO_UPDATE_SIZE_LIMITS auto-scale height
        : BWindow(BRect(0, 0, 350, 1), "Add Custom Server", 
                  B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS) {
        
        fTarget = targetWindow;

        // 1. Initialize modern, auto-aligning input layout fields
        fNameField = new BTextControl("name", "Network Name:", "My IRC Server", nullptr);
        fHostField = new BTextControl("host", "Server Host:", "irc.example.com", nullptr);
        fPortField = new BTextControl("port", "Port:", "6697", nullptr);
        fNickField = new BTextControl("nick", "Nickname:", "HaikuUser", nullptr);
        
        // NEW: Instantiate two new alternative nickname fields with clean template defaults
        fAltNickField  = new BTextControl("altnick", "Alt Nick 1:", "HaikuUser+", nullptr);
        fAltNick2Field = new BTextControl("altnick2", "Alt Nick 2:", "HaikuUser__", nullptr);
        
        fPassField = new BTextControl("pass", "Password (Optional):", "", nullptr);
        
        // Hide password characters automatically
        fPassField->TextView()->HideTyping(true);

        // 2. Control Form Buttons
        fCancelButton = new BButton("cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        fSaveButton   = new BButton("save", "Add Server", new BMessage(MSG_ADD_CUSTOM_SERVER_SUBMIT));
        
        // Set the save button as the default highlight action on hitting 'Enter' key
        fSaveButton->MakeDefault(true);

        // 3. Build UI Architecture via Group Layouts
        BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
            .SetInsets(12)
            .AddGrid(5.0f, 5.0f) // Vertically aligns the colons of the text inputs perfectly
                .Add(fNameField->CreateLabelLayoutItem(), 0, 0)
                .Add(fNameField->CreateTextViewLayoutItem(), 1, 0)
                
                .Add(fHostField->CreateLabelLayoutItem(), 0, 1)
                .Add(fHostField->CreateTextViewLayoutItem(), 1, 1)
                
                .Add(fPortField->CreateLabelLayoutItem(), 0, 2)
                .Add(fPortField->CreateTextViewLayoutItem(), 1, 2)
                
                .Add(fNickField->CreateLabelLayoutItem(), 0, 3)
                .Add(fNickField->CreateTextViewLayoutItem(), 1, 3)
                
                // Placed new input fields into their own grid coordinates cleanly
                .Add(fAltNickField->CreateLabelLayoutItem(), 0, 4)
                .Add(fAltNickField->CreateTextViewLayoutItem(), 1, 4)
                
                .Add(fAltNick2Field->CreateLabelLayoutItem(), 0, 5)
                .Add(fAltNick2Field->CreateTextViewLayoutItem(), 1, 5)
                
                // Shifted password down to row 6 to clear the room
                .Add(fPassField->CreateLabelLayoutItem(), 0, 6)
                .Add(fPassField->CreateTextViewLayoutItem(), 1, 6)
            .End()
            .AddGlue() // Pushes inputs up and action control buttons down
            .AddGroup(B_HORIZONTAL, 10)
                .AddGlue() // Right-aligns buttons cleanly
                .Add(fCancelButton)
                .Add(fSaveButton)
            .End();

        // 4. Center this modal dynamically directly over the main application
        CenterIn(targetWindow->Frame());
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
        	
        	
            case MSG_ADD_CUSTOM_SERVER_SUBMIT: {
                // Perform quick sanitization constraints check
                if (strlen(fNameField->Text()) == 0 || strlen(fHostField->Text()) == 0) {
                    return; 
                }

                // Pack everything securely into a payload carrier message
                BMessage reply(MSG_ADD_CUSTOM_SERVER_SUBMIT);
                reply.AddString("name", fNameField->Text());
                reply.AddString("host", fHostField->Text());
                reply.AddInt32("port", atoi(fPortField->Text()));
                reply.AddString("nick", fNickField->Text());
                
                // Pack the alternate fields text strings safely into transmission payload
                reply.AddString("altNick", fAltNickField->Text());
                reply.AddString("altNick2", fAltNick2Field->Text());
                
                reply.AddString("pass", fPassField->Text());

                // Post asynchronous message back to the main UI frame
                fTarget->PostMessage(&reply);
                
                // Close dialog
                PostMessage(B_QUIT_REQUESTED);
                break;
            }
            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

private:
    BWindow*       fTarget;
    BTextControl*  fNameField;
    BTextControl*  fHostField;
    BTextControl*  fPortField;
    BTextControl*  fNickField;
    BTextControl*  fAltNickField; 
    BTextControl*  fAltNick2Field; 
    BTextControl*  fPassField;
    BButton*       fCancelButton;
    BButton*       fSaveButton;
};




// 1. DECLARE HELPER ITEM FIRST: Put ChannelRowItem at the top so the window can see it
class ChannelRowItem : public BStringItem {
public:
    ChannelRowItem(const char* channel, const char* users, const char* topic)
        : BStringItem(channel), fChannel(channel), fUsers(users), fTopic(topic) {
        fRawUserCount = atoi(users); 	
        fUsers << " users";
    }

    BString GetChannelName() const { return fChannel; }
    int32   GetUserCount() const { return fRawUserCount; } 
    
    

    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        owner->PushState();

        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        // Read the true runtime font scale from the parent view context container
        BFont dynamicListFont;
        owner->GetFont(&dynamicListFont);
        owner->SetFont(&dynamicListFont);
        
        font_height fh;
        dynamicListFont.GetHeight(&fh); 
        float baselineY = itemRect.bottom - (fh.descent + fh.leading);

        // Column A: Channel Name (Starts at pixel 10, strictly limited to 140px wide)
        BString truncatedChannel = fChannel;
        owner->TruncateString(&truncatedChannel, B_TRUNCATE_END, 140.0f);
        owner->MovePenTo(itemRect.left + 10, baselineY);
        owner->DrawString(truncatedChannel.String());

        // --- FIXED #1: RESTORE STABLE GREEN COLOR FOR VISIBLE USER LOGS ---
        if (!IsSelected()) {
            // Using explicit custom dark green color code (RGB) to guarantee contrast scannability
            rgb_color forestGreen = {0, 180, 0, 255};
            owner->SetHighColor(forestGreen); 
        }
        
        // Column B: User Count (Starts at pixel 160, strictly limited to 80px wide)
        BString truncatedUsers = fUsers;
        owner->TruncateString(&truncatedUsers, B_TRUNCATE_END, 80.0f);
        owner->MovePenTo(itemRect.left + 160, baselineY);
        owner->DrawString(truncatedUsers.String());

        // --- FIXED #2: ROBUST VISIBLE CLIPPING BOUNDARY TRACKER CALCULATION ---
        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
        } else {
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
        }
        
        // Extract the exact layout width directly via the parent window frame.
        // This is 100% self-contained and avoids incomplete type/header errors.
        float visibleViewportWidth = owner->Bounds().Width();
        if (owner->Window() != nullptr) {
            // Subtract the default horizontal scrollbar width if needed, or fallback gracefully
            visibleViewportWidth = owner->Window()->Frame().Width() - 30.0f;
        }
        
        float remainingTopicWidth = visibleViewportWidth - 275.0f;
        if (remainingTopicWidth > 15.0f) {
            BString truncatedTopic = fTopic;
            owner->TruncateString(&truncatedTopic, B_TRUNCATE_END, remainingTopicWidth);
            owner->MovePenTo(itemRect.left + 250, baselineY);
            owner->DrawString(truncatedTopic.String());
        }



        owner->PopState();
    }

private:
    BString fChannel;
    BString fUsers;
    BString fTopic;
    int32   fRawUserCount;
};



// Sorts items so that the highest user count bubbles up to the top
// Use const void* to match the native BListView API
static int SortChannelsByUsers(const void* first, const void* second) {
    // 1. Cast the generic raw pointers to constant BListItem pointers safely
    const BListItem* itemPtrA = *static_cast<const BListItem* const*>(first);
    const BListItem* itemPtrB = *static_cast<const BListItem* const*>(second);

    // 2. Perform dynamic_cast checks to verify these are indeed ChannelRowItems
    const ChannelRowItem* itemA = dynamic_cast<const ChannelRowItem*>(itemPtrA);
    const ChannelRowItem* itemB = dynamic_cast<const ChannelRowItem*>(itemPtrB);

    if (itemA == nullptr || itemB == nullptr) return 0;

    // Descending order sort (highest count first)
    if (itemA->GetUserCount() > itemB->GetUserCount()) return -1;
    if (itemA->GetUserCount() < itemB->GetUserCount()) return 1;
    return 0;
}




// 2. DECLARE THE WINDOW
class IRCChannelListWindow : public BWindow {
public:
    // Update the constructor signature to accept 4 arguments:
    IRCChannelListWindow(BWindow* owner, BSecureSocket* targetSocket, ServerTreeItem* serverItem, IRCChannelListWindow** tracker) 
        : BWindow(BRect(150, 150, 800, 650), "Network Channel List", 
                  B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS) {

        fOwnerWindow = owner;
        fSocket = targetSocket;
        fServerContext = serverItem; 
        fTracker = tracker; // <-- ADDED: Save the tracking address safely

        fListView = new BListView("chan_list_view");
        fListView->SetInvocationMessage(new BMessage('join'));
        BScrollView* scrollPane = new BScrollView("scroll_list", fListView, 0, false, true);
		

        BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
            .SetInsets(5)
            .Add(scrollPane, 1.0)
            .End();

        if (fSocket != nullptr) {
            fSocket->Write("LIST\r\n", 6);
        }
    }

    // Public getter required by the '322' protocol parser block
    BSecureSocket* GetTargetSocket() const { return fSocket; }

    // Public getter to query the parent network server node if needed
    ServerTreeItem* GetServerContext() const { return fServerContext; }
    
    // Force a complete recalculation of our column items during runtime window stretching
    void FrameResized(float newWidth, float newHeight) override {
        // Call base class implementation first to keep Haiku layout structures aligned
        BWindow::FrameResized(newWidth, newHeight);

        if (fListView != nullptr) {
            // 1. Tell the internal view layout parameters to invalidate their width geometries
            fListView->InvalidateLayout();
            
            // 2. Clear item height/width cache bounds and force every visible row item to call DrawItem()
            fListView->Invalidate();
        }
    }


    void DispatchMessage(BMessage* message, BHandler* handler) override {
        if (message->what == B_MOUSE_DOWN && handler == fListView) {
            int32 buttons;
            BPoint point;
            
            if (message->FindInt32("buttons", &buttons) == B_OK && buttons == B_SECONDARY_MOUSE_BUTTON) {
                message->FindPoint("be:view_where", &point);
                int32 index = fListView->IndexOf(point);
                
                if (index >= 0) {
                    fListView->Select(index);
                    ChannelRowItem* selectedItem = dynamic_cast<ChannelRowItem*>(fListView->ItemAt(index));
                    
                    if (selectedItem != nullptr) {
                        BPopUpMenu* menu = new BPopUpMenu("ChannelActions", false, false);
                        menu->AddItem(new BMenuItem("Join Channel", new BMessage('join')));
                        menu->SetTargetForItems(this);
                        menu->Go(fListView->ConvertToScreen(point), true, true, true);
                        return; 
                    }
                }
            }
        }
        BWindow::DispatchMessage(message, handler);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
 
            case MSG_ADD_LIST_ROW: {
                const char* channelName;
                const char* userCount;
                const char* topic;
        
                if (message->FindString("channel", &channelName) == B_OK &&
                    message->FindString("users", &userCount) == B_OK &&
                    message->FindString("topic", &topic) == B_OK) {
            
                    fListView->AddItem(new ChannelRowItem(channelName, userCount, topic));
                    fListView->SortItems(SortChannelsByUsers); 
                }
                break;
            }

            case 'join': {
                int32 selectedIdx = fListView->CurrentSelection();
                if (selectedIdx >= 0) {
                    ChannelRowItem* selectedItem = dynamic_cast<ChannelRowItem*>(fListView->ItemAt(selectedIdx));
                    if (selectedItem != nullptr && fSocket != nullptr) {
                        BString joinCmd;
                        joinCmd << "JOIN " << selectedItem->GetChannelName() << "\r\n";
                        fSocket->Write(joinCmd.String(), joinCmd.Length());
                        
                        if (fOwnerWindow) fOwnerWindow->Activate(true);
                        PostMessage(B_QUIT_REQUESTED); 
                    }
                }
                break;
            }

            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

    // --- UPDATED: Swapped Quit() for QuitRequested() to clear pointer instantly ---
    bool QuitRequested() override {
        while (fListView->CountItems() > 0) {
            BListItem* item = fListView->RemoveItem((int32)0);
            delete item;
        }

        // Instantly clears fActiveListWindow in the main thread thread-safely
        if (fTracker != nullptr) {
            *fTracker = nullptr;
        }

        return true; 
    }

private:
    BWindow*               fOwnerWindow;
    BListView*             fListView;
    BSecureSocket*         fSocket;
    ServerTreeItem*        fServerContext; 
    IRCChannelListWindow** fTracker; // <-- ADDED: Private variable to hold tracker reference
};



// @ChannelTreeItem
class ChannelTreeItem : public BStringItem {
public:
    // Accept an additional bool isCustom parameter (defaults to false)
    ChannelTreeItem(const char* text, size_t serverIndex, bool isCustom = false) 
        : BStringItem(text), fServerIndex(serverIndex), fIsCustom(isCustom), fHasUnread(false), fAutoJoin(false) {
        
        BString chanName(text);
        
        // Route the initial autojoin check to the correct config vector array
        if (fIsCustom) {
            if (fServerIndex < cfg.customServers.size()) {
                for (const auto& chan : cfg.customServers[fServerIndex].autojoin) {
                    if (chanName.ICompare(chan.c_str()) == 0) {
                        fAutoJoin = true;
                        break;
                    }
                }
            }
        } else {
            if (fServerIndex < cfg.servers.size()) {
                for (const auto& chan : cfg.servers[fServerIndex].autojoin) {
                    if (chanName.ICompare(chan.c_str()) == 0) {
                        fAutoJoin = true;
                        break;
                    }
                }
            }
        }
    }

    void SetUnread(bool unread) { fHasUnread = unread; }
    bool HasUnread() const { return fHasUnread; }
    
    void SetAutoJoin(bool autoJoin) { fAutoJoin = autoJoin; }
    bool IsAutoJoin() const { return fAutoJoin; }
    size_t GetServerIndex() const { return fServerIndex; }
    bool   IsCustom() const { return fIsCustom; } 
   // void SetTopic(const char* topic) { fChannelTopic = topic; }
   //  BString GetTopic() const { return fChannelTopic; }

    // Override the native drawing framework loop
    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        owner->PushState();

        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        BFont dynamicFont;
        owner->GetFont(&dynamicFont);

        if (fHasUnread && !IsSelected()) {
            dynamicFont.SetFace(B_BOLD_FACE);
            owner->SetHighColor(ui_color(B_LINK_TEXT_COLOR)); 
        } else {
            dynamicFont.SetFace(B_REGULAR_FACE);
        }
        owner->SetFont(&dynamicFont);

        font_height fh;
        dynamicFont.GetHeight(&fh);
        float textBaseline = itemRect.top + fh.ascent + (itemRect.Height() - (fh.ascent + fh.descent + fh.leading)) / 2.0f;
        
        owner->MovePenTo(itemRect.left + 20, textBaseline);
        
        BString displayString = Text();
        if (fAutoJoin) displayString << " [A]";
        owner->DrawString(displayString.String());

        owner->PopState();
    }

private:
    size_t fServerIndex;
    bool   fIsCustom; 
    bool   fHasUnread;
    bool   fAutoJoin;
};


class ServerTreeItem : public BStringItem {
public:
    ServerTreeItem(const char* text, size_t configIndex, bool isChannel = false, bool isCustom = false) 
        : BStringItem(text), fConfigIndex(configIndex), fIsChannel(isChannel), fIsCustom(isCustom) {
        
        // Safety bounds check to pull structural configs securely
        if (fIsCustom) {
            if (fConfigIndex < cfg.customServers.size()) {
                fAutoConnect = cfg.customServers[fConfigIndex].autoConnect;
                fAutoReconnect = cfg.customServers[fConfigIndex].autoReconnect;
                fHideStatus = cfg.customServers[fConfigIndex].hideStatusMessages;
            } else {
                fAutoConnect = false;
                fAutoReconnect = false;
            }
        } else {
            if (fConfigIndex < cfg.servers.size()) {
                fAutoConnect = cfg.servers[fConfigIndex].autoConnect;
                fAutoReconnect = cfg.servers[fConfigIndex].autoReconnect;
                fHideStatus = cfg.servers[fConfigIndex].hideStatusMessages;
            } else {
                fAutoConnect = false;
                fAutoReconnect = false;
            }
        }
    }
        
    BString GetHost() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].host.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].host.c_str());
        return BString("");
    }

    uint16 GetPort() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return cfg.customServers[fConfigIndex].port;
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return cfg.servers[fConfigIndex].port;
        return 6697;
    }

    BString GetNick() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].nick.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].nick.c_str());
        return BString("HaikuUser");
    }
    
    BString GetAltNick() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].altNick.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].altNick.c_str());
        return BString(cfg.servers[0].nick.c_str()) << "+";
    }

    BString GetAltNick2() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].altNick2.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].altNick2.c_str());
        return BString(cfg.servers[0].nick.c_str()) << "__";
    }

    BString GetPass() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].pass.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].pass.c_str());
        return BString("");
    }

    std::vector<std::string> GetAutojoin() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return cfg.customServers[fConfigIndex].autojoin;
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return cfg.servers[fConfigIndex].autojoin;
        return std::vector<std::string>();
    }

    size_t GetIndex() const { return fConfigIndex; }
    bool   IsCustom() const { return fIsCustom; }
    bool   IsChannel() const { return fIsChannel; }
    
    bool IsAutoConnect() const { return fAutoConnect; }
    void SetAutoConnect(bool autoConnect) { fAutoConnect = autoConnect; }
    bool IsAutoReconnect() const { return fAutoReconnect; }
    void SetAutoReconnect(bool autoReconnect) { fAutoReconnect = autoReconnect; }
    bool IsHideStatus() const { return fHideStatus; }
    void SetHideStatus(bool hide) { fHideStatus = hide; }
	void SetIndex(size_t idx) { fConfigIndex = idx; }
	
    // DrawItem that centers text and indents channels perfectly at any font size
    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        owner->PushState();
        
        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        BFont dynamicFont;
        owner->GetFont(&dynamicFont);
        owner->SetFont(&dynamicFont);

        font_height fh;
        dynamicFont.GetHeight(&fh);
        float textBaseline = itemRect.top + fh.ascent + (itemRect.Height() - (fh.ascent + fh.descent + fh.leading)) / 2.0f;
        
        float leftPadding = fIsChannel ? 24.0f : 4.0f;
        owner->MovePenTo(itemRect.left + leftPadding, textBaseline);
        
        BString displayString = Text();
        if (!fIsChannel) {
            if (fAutoConnect) displayString << " [AC]";
            if (fAutoReconnect) displayString << " [AR]"; 
        }
        owner->DrawString(displayString.String());

        owner->PopState();
    }

private:
    size_t fConfigIndex;
    bool fAutoConnect;
    bool fAutoReconnect;
    bool fHideStatus = false;
    bool fIsChannel; 
    bool fIsCustom; // <-- Tracks whether this item references custom array configs
};



class ServerConfigWindow : public BWindow {
public:
    virtual ~ServerConfigWindow() {}

    ServerConfigWindow(BWindow* parent, ServerTreeItem* item, size_t serverIdx, bool isCustom = false)
        : BWindow(BRect(0, 0, 450, 600), "Server Properties", B_MODAL_WINDOW_LOOK, 
                  B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS) { // Removed B_NOT_RESIZABLE so users can expand the channel list box comfortably

        fParentWindow = parent;
        fItem = item;
        fServerIdx = serverIdx;
        fIsCustom = isCustom; 

        AddToSubset(parent);

        ServerConfig& srv = GetActiveConfig();
        
        // Deep copy active server configuration autojoin fields onto local tracking array space
        fLocalAutojoinList = srv.autojoin;

        // --- Core Entry Fields ---
        fNickInput = new BTextControl("nick", "Nickname:", srv.nick.c_str(), nullptr);
        fAltNickInput  = new BTextControl("altnick", "Alt Nick 1:", srv.altNick.c_str(), nullptr);
        fAltNick2Input = new BTextControl("altnick2", "Alt Nick 2:", srv.altNick2.c_str(), nullptr);
        
        fPassInput = new BTextControl("pass", "Password:", "", nullptr);
        BTextView* passTextView = fPassInput->TextView();
        if (passTextView != nullptr) {
            passTextView->HideTyping(true);
        }
        fPassInput->SetText(srv.pass.c_str());

        fAwayInput = new BTextControl("awaymsg", "AWAY Message:", cfg.awayMessage.c_str(), nullptr);
        fQuitInput = new BTextControl("quitmsg", "QUIT Message:", cfg.quitMessage.c_str(), nullptr);

        fServerListFontMenu = CreateFontMenu("Server List Font:", srv.serverListFontSize);
        fChatLogFontMenu    = CreateFontMenu("Chat Log Font:", srv.chatLogFontSize);
        fUserListFontMenu   = CreateFontMenu("User List Font:", srv.userListFontSize);

        fBgPathInput = new BTextControl("bg_path", "Wallpaper Image:", srv.backgroundImagePath.c_str(), nullptr);
        fBrowseBgBtn = new BButton("browse_bg", "Browse…", new BMessage('adbg'));
        fFilePanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), nullptr, B_FILE_NODE, false);

        fBgOpacitySlider = new BSlider("bg_opacity_sld", "Wallpaper Dimming Level:", nullptr, 0, 100, B_HORIZONTAL);
        fBgOpacitySlider->SetValue(srv.backgroundOpacity);
        fBgOpacitySlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
        fBgOpacitySlider->SetHashMarkCount(11); 

        // --- Checkbox Options ---
        fAutoConnectCheck = new BCheckBox("autoconnect", "Automatically connect at startup", nullptr);
        fAutoConnectCheck->SetValue(srv.autoConnect ? B_CONTROL_ON : B_CONTROL_OFF);

        fAutoReconnectCheck = new BCheckBox("autoreconnect", "Automatically reconnect on drop", nullptr);
        fAutoReconnectCheck->SetValue(srv.autoReconnect ? B_CONTROL_ON : B_CONTROL_OFF);

        fHideStatusCheck = new BCheckBox("hidestatus", "Hide channel status messages (Joins/Parts/Quits)", nullptr);
        fHideStatusCheck->SetValue(srv.hideStatusMessages ? B_CONTROL_ON : B_CONTROL_OFF);

        fDebugEnableCheck = new BCheckBox("debug", "Enable low-level socket engine logs", nullptr);
        fDebugEnableCheck->SetValue(srv.debugEnable ? B_CONTROL_ON : B_CONTROL_OFF);

        fEnableEmoticonsCheck = new BCheckBox("enable_emotes", "Enable custom inline emoticons for this server", nullptr);
        fEnableEmoticonsCheck->SetValue(srv.enableEmoticons ? B_CONTROL_ON : B_CONTROL_OFF);

        fUseCustomDrawCheck = new BCheckBox("use_custom_draw", "Enable High Performance Draw Engine", nullptr);
        fUseCustomDrawCheck->SetValue(srv.useCustomDrawFunction ? B_CONTROL_ON : B_CONTROL_OFF);

        fLogChatsToFileCheck = new BCheckBox("log_chats", "Log server chats to file", nullptr);
        fLogChatsToFileCheck->SetValue(srv.logChatsToFile ? B_CONTROL_ON : B_CONTROL_OFF);

        // --- NEW: AUTOJOIN CHANNELS INTERFACE COMPONENTS ---
        fAutojoinListView = new BListView("autojoin_list");
        BScrollView* autojoinScroll = new BScrollView("autojoin_scroll", fAutojoinListView, 0, false, true);
        autojoinScroll->SetExplicitMinSize(BSize(150, 100)); // Safeguard clear display box framing

        // Populate the interactive list view using our tracking vector elements
        for (const auto& chan : fLocalAutojoinList) {
            if (!chan.empty()) {
                fAutojoinListView->AddItem(new BStringItem(chan.c_str()));
            }
        }

        fAutojoinInput = new BTextControl("chan_input", "", "", nullptr);
        fAddAutojoinBtn = new BButton("add_chan", "Add", new BMessage('ajad'));
        fRemoveAutojoinBtn = new BButton("rem_chan", "Remove", new BMessage('ajrm'));

        // --- Window Actions ---
        BButton* cancelBtn = new BButton("cancel", "Cancel", new BMessage('cfcn'));
        BButton* saveBtn = new BButton("save", "Save", new BMessage('cfsv'));
        saveBtn->MakeDefault(true);

        // --- Layout Building Architecture ---
        BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
            .SetInsets(15)
            .AddGrid(5.0f, 5.0f)
                .Add(fNickInput->CreateLabelLayoutItem(), 0, 0)
                .Add(fNickInput->CreateTextViewLayoutItem(), 1, 0)
                
                .Add(fAltNickInput->CreateLabelLayoutItem(), 0, 1)
                .Add(fAltNickInput->CreateTextViewLayoutItem(), 1, 1)
                
                .Add(fAltNick2Input->CreateLabelLayoutItem(), 0, 2)
                .Add(fAltNick2Input->CreateTextViewLayoutItem(), 1, 2)
                
                .Add(fPassInput->CreateLabelLayoutItem(), 0, 3)     
                .Add(fPassInput->CreateTextViewLayoutItem(), 1, 3)
            
                .Add(fAwayInput->CreateLabelLayoutItem(), 0, 4)
                .Add(fAwayInput->CreateTextViewLayoutItem(), 1, 4)  
                
                .Add(fQuitInput->CreateLabelLayoutItem(), 0, 5)
                .Add(fQuitInput->CreateTextViewLayoutItem(), 1, 5)         
    
                .Add(fServerListFontMenu->CreateLabelLayoutItem(), 0, 6)
                .Add(fServerListFontMenu->CreateMenuBarLayoutItem(), 1, 6)
                
                .Add(fChatLogFontMenu->CreateLabelLayoutItem(), 0, 7)
                .Add(fChatLogFontMenu->CreateMenuBarLayoutItem(), 1, 7)
                
                .Add(fUserListFontMenu->CreateLabelLayoutItem(), 0, 8)
                .Add(fUserListFontMenu->CreateMenuBarLayoutItem(), 1, 8)

                .Add(fBgPathInput->CreateLabelLayoutItem(), 0, 9)
                .AddGroup(B_HORIZONTAL, 5, 1, 9)
                    .Add(fBgPathInput->CreateTextViewLayoutItem(), 1.0)
                    .Add(fBrowseBgBtn, 0.0)
                .End()
            .End()
            .Add(fBgOpacitySlider) 
            .AddStrut(4)


            // HEADER-SAFE FIX: Uses an explicitly instantiated BBox container 
            // to provide a pristine text title and frame border safely.
            .AddGroup(B_VERTICAL, 5)
                .Add([&]() -> BView* {
                    BBox* autojoinBox = new BBox(B_FANCY_BORDER);
                    autojoinBox->SetLabel("Autojoin Channels");
                    
                    BLayoutBuilder::Group<>(autojoinBox, B_VERTICAL, 5)
                        .SetInsets(10, 20, 10, 10) // Padding adjustments account for the header text title
                        .Add(autojoinScroll, 1.0)
                        .AddGroup(B_HORIZONTAL, 5)
                            .Add(fAutojoinInput, 1.0)
                            .Add(fAddAutojoinBtn, 0.0)
                            .Add(fRemoveAutojoinBtn, 0.0)
                        .End();
                        
                    return autojoinBox;
                }(), 1.0)
            .End()






            
            
            .Add(fAutoConnectCheck)
            .Add(fAutoReconnectCheck)
            .Add(fHideStatusCheck)
            // .Add(fDebugEnableCheck)      
            .Add(fEnableEmoticonsCheck) 
            .Add(fUseCustomDrawCheck)            
            .Add(fLogChatsToFileCheck)
            .AddGlue()
            .AddGroup(B_HORIZONTAL, 10)
                .AddGlue()
                .Add(cancelBtn)
                .Add(saveBtn)
            .End();
            
        if (parent) {
            CenterIn(parent->Frame()); 
        } else {
            CenterOnScreen(); 
        }
    }


    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            
            case 'ajad': {
                BString inputChan = fAutojoinInput->Text();
                inputChan.Trim();
                
                // SAFETY GUARD: Ignore the operation if the field is blank or unchanged from the placeholder hint
                if (inputChan.Length() > 0 && inputChan != "#EnterChannelName") {
                    
                    // Automatically append typical IRC channel prefix notation if missing
                    if (!inputChan.StartsWith("#") && !inputChan.StartsWith("&")) {
                        inputChan.Prepend("#");
                    }
                    
                    // Prevent pushing exact structural duplicate channel entries onto list
                    bool isDuplicate = false;
                    for (int32 i = 0; i < fAutojoinListView->CountItems(); i++) {
                        BStringItem* it = static_cast<BStringItem*>(fAutojoinListView->ItemAt(i));
                        if (it && inputChan.ICompare(it->Text()) == 0) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    
                    if (!isDuplicate) {
                        fAutojoinListView->AddItem(new BStringItem(inputChan.String()));
                        fLocalAutojoinList.push_back(inputChan.String());
                        fAutojoinInput->SetText(""); // Reset text field completely empty
                    }
                }
                break;
            }


            // --- REMOVE SELECTED CHANNEL FROM VECTOR LOOP ---
            case 'ajrm': {
                int32 selectedIdx = fAutojoinListView->CurrentSelection();
                if (selectedIdx >= 0) {
                    BStringItem* item = static_cast<BStringItem*>(fAutojoinListView->RemoveItem(selectedIdx));
                    if (item != nullptr) {
                        BString targetText = item->Text();
                        delete item; // Safe deletion avoids storage heap memory leakage
                        
                        // Erase match out of our local copy tracking vector
                        for (auto it = fLocalAutojoinList.begin(); it != fLocalAutojoinList.end(); ++it) {
                            if (targetText.ICompare(it->c_str()) == 0) {
                                fLocalAutojoinList.erase(it);
                                break;
                            }
                        }
                    }
                }
                break;
            }
        	
        	case 'adbg':
                if (fFilePanel != nullptr) {
                    fFilePanel->Show();
                }
                break;

            case B_REFS_RECEIVED: {
                entry_ref ref;
                if (message->FindRef("refs", &ref) == B_OK) {
                    BEntry entry(&ref, true);
                    BPath path;
                    if (entry.GetPath(&path) == B_OK) {
                        fBgPathInput->SetText(path.Path());
                    }
                }
                break;
            }
        	
            case 'cfcn': {
                // Clear and free item pointers from the local list box view to prevent memory leaking
                while (fAutojoinListView->CountItems() > 0) {
                    delete fAutojoinListView->RemoveItem((int32)0);
                }
            	delete fFilePanel; 
                Quit();
                break;
            }
                
            case 'cfsv': {
                ServerConfig& srv = GetActiveConfig();
                srv.nick = fNickInput->Text();
                srv.altNick  = fAltNickInput->Text();
                srv.altNick2 = fAltNick2Input->Text();

                BString inputPassword = fPassInput->Text();
                if (inputPassword.Length() > 0) {
                    srv.pass = inputPassword.String();
                }         
                
                srv.backgroundImagePath = fBgPathInput->Text();
                srv.backgroundOpacity = fBgOpacitySlider->Value(); 
                
                cfg.awayMessage = fAwayInput->Text();
                cfg.quitMessage = fQuitInput->Text();
                
                BMenuItem* item = fServerListFontMenu->Menu()->FindMarked();
                if (item && item->Message()) srv.serverListFontSize = item->Message()->FindInt32("size");

                item = fChatLogFontMenu->Menu()->FindMarked();
                if (item && item->Message()) srv.chatLogFontSize = item->Message()->FindInt32("size");

                item = fUserListFontMenu->Menu()->FindMarked();
                if (item && item->Message()) srv.userListFontSize = item->Message()->FindInt32("size");

                srv.autoConnect = (fAutoConnectCheck->Value() == B_CONTROL_ON);
                srv.autoReconnect = (fAutoReconnectCheck->Value() == B_CONTROL_ON);
                srv.hideStatusMessages = (fHideStatusCheck->Value() == B_CONTROL_ON);
                srv.debugEnable = (fDebugEnableCheck->Value() == B_CONTROL_ON);
                srv.enableEmoticons = (fEnableEmoticonsCheck->Value() == B_CONTROL_ON);
                srv.useCustomDrawFunction = (fUseCustomDrawCheck->Value() == B_CONTROL_ON);
                srv.logChatsToFile = (fLogChatsToFileCheck->Value() == B_CONTROL_ON);

                // --- NEW: COMMIT LOCAL AUTOJOIN CHANGED VECTOR PERMANENTLY ---
                // If list is empty, we preserve a baseline blank value to prevent connection script stalls
                if (fLocalAutojoinList.empty()) {
                    srv.autojoin = {""};
                } else {
                    srv.autojoin = fLocalAutojoinList;
                }

                // Memory cleanup pass before shutdown
                while (fAutojoinListView->CountItems() > 0) {
                    delete fAutojoinListView->RemoveItem((int32)0);
                }

                BMessage updateNotify('mscf'); 
                if (fParentWindow) fParentWindow->PostMessage(&updateNotify);
                delete fFilePanel; 
                Quit();
                break;
            }
            
            default:
                BWindow::MessageReceived(message);
                break;
        }
    }


private:
    BWindow*            fParentWindow;
    ServerTreeItem*     fItem;
    size_t              fServerIdx;
    bool                fIsCustom; 
	BTextControl* 		fQuitInput;
	BTextControl* 		fAwayInput;
    BTextControl*       fNickInput;
    BTextControl*       fAltNickInput;
    BTextControl*       fAltNick2Input; 
    BTextControl*       fPassInput;
    BCheckBox*          fAutoConnectCheck;
    BCheckBox*          fAutoReconnectCheck;
    BCheckBox*          fDebugEnableCheck;
    BCheckBox*          fHideStatusCheck;
    BMenuField*         fServerListFontMenu;
    BMenuField*         fChatLogFontMenu;
    BMenuField*         fUserListFontMenu;
    BTextControl*       fBgPathInput;
    BButton*            fBrowseBgBtn;
    BFilePanel*         fFilePanel;
    BCheckBox*          fEnableEmoticonsCheck; 
    BCheckBox*          fUseCustomDrawCheck; 
	BSlider*            fBgOpacitySlider; 
	BCheckBox*          fLogChatsToFileCheck; 
	BListView*          fAutojoinListView;
    BTextControl*       fAutojoinInput;
    BButton*            fAddAutojoinBtn;
    BButton*            fRemoveAutojoinBtn;
	std::vector<std::string> fLocalAutojoinList;
	
	
    // NEW: Safe helper function to fetch correct active reference target safely
    ServerConfig& GetActiveConfig() {
        if (fIsCustom && fServerIdx < cfg.customServers.size()) {
            return cfg.customServers[fServerIdx];
        }
        // Fallback or standard hardcoded element reference loop block
        return cfg.servers[fServerIdx];
    }

    BMenuField* CreateFontMenu(const char* label, int32 currentSize) {
        BPopUpMenu* menu = new BPopUpMenu("font_popup");
        int32 sizes[] = {9, 10, 11, 12, 14, 16, 18, 20, 24};
        
        for (int i = 0; i < 9; i++) {
            BString sizeStr;
            sizeStr << sizes[i];
            
            BMessage* msg = new BMessage('fntc');
            msg->AddInt32("size", sizes[i]);
            
            BMenuItem* item = new BMenuItem(sizeStr.String(), msg);
            if (sizes[i] == currentSize) {
                item->SetMarked(true);
            }
            menu->AddItem(item);
        }
        return new BMenuField("font_field", label, menu);
    }
};




// Sorts users by rank (@ on top, then +, then regular users), then alphabetically
static int SortUsersByRank(const void* first, const void* second) {
    const BListItem* itemPtrA = *static_cast<const BListItem* const*>(first);
    const BListItem* itemPtrB = *static_cast<const BListItem* const*>(second);

    const BStringItem* userA = dynamic_cast<const BStringItem*>(itemPtrA);
    const BStringItem* userB = dynamic_cast<const BStringItem*>(itemPtrB);

    if (userA == nullptr || userB == nullptr) return 0;

    BString nameA(userA->Text());
    BString nameB(userB->Text());

    // 1. Assign numeric rank weights (higher numbers = higher priority)
    int32 rankA = 0;
    int32 rankB = 0;

    if (nameA.StartsWith("@")) rankA = 2;
    else if (nameA.StartsWith("+")) rankA = 1;

    if (nameB.StartsWith("@")) rankB = 2;
    else if (nameB.StartsWith("+")) rankB = 1;

    // 2. If ranks are different, sort by rank weight descending
    if (rankA != rankB) {
        return (rankA > rankB) ? -1 : 1;
    }

    // 3. If ranks are identical, strip prefixes and sort alphabetically
    if (rankA > 0) nameA.Remove(0, 1);
    if (rankB > 0) nameB.Remove(0, 1);

    return nameA.ICompare(nameB);
}



// ==================== NATIVE HAIKU ICON POPUP CANVAS ====================
class EmotePopupWindow : public BWindow {
public:
    EmotePopupWindow(BPoint screenPos, BMessenger targetWindowMessenger)
        : BWindow(BRect(0, 0, 100, 50), "", B_NO_BORDER_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL, 
                  B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
          fTargetMessenger(targetWindowMessenger)
    {
        BView* bgPanel = new BView(Bounds(), "popBg", B_FOLLOW_ALL, B_WILL_DRAW);
        bgPanel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        bgPanel->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        AddChild(bgPanel);

        BGridView* grid = new BGridView();
        BGridLayout* gridLayout = grid->GridLayout();
        gridLayout->SetHorizontalSpacing(2.0f);
        gridLayout->SetVerticalSpacing(2.0f);
        gridLayout->SetInsets(4, 4, 4, 4);

        struct EmoteButtonMap { const char* trigger; const uint8* iconData; const char* tooltip; };
        EmoteButtonMap barEmotes[] = {
            {":)", kIconSmile, "Smile"},       {":(", kIconFrown, "Frown"},
            {";)", kIconWink, "Wink"},         {":P", kIconTongue, "Tongue"},
            {"lol", kIconLaughing, "Laughing"},{":O", kIconAstonished, "Shocked"},
            {"<3", kIconHeart, "Heart"},       {":|", kIconConfused, "Confused"}
        };

        for (int32 i = 0; i < 8; i++) {
            BMessage* clickMsg = new BMessage('emcl');
            clickMsg->AddString("trigger", barEmotes[i].trigger);
            clickMsg->AddPointer("popup_window_ref", this);

            BButton* btn = new BButton(barEmotes[i].trigger, "", clickMsg);
            
            BBitmap* btnIcon = new BBitmap(BRect(0, 0, 15, 15), B_RGBA32);
            if (BIconUtils::GetVectorIcon(barEmotes[i].iconData, 1024, btnIcon) == B_OK) {
                btn->SetIcon(btnIcon);
            }
            delete btnIcon;

            btn->SetFlat(true);
            btn->SetToolTip(barEmotes[i].tooltip);
            btn->SetExplicitPreferredSize(BSize(22, 22));
            btn->SetTarget(fTargetMessenger);

            int32 column = i % 4;
            int32 row = i / 4;
            gridLayout->AddView(btn, column, row, 1, 1);
        }

        BButton* closeBtn = new BButton("closeBtn", "[Close]", new BMessage(B_QUIT_REQUESTED));
        closeBtn->SetFlat(true);
        closeBtn->SetTarget(this); 
        
        closeBtn->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        closeBtn->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        
        BFont themeFont;
        bgPanel->GetFont(&themeFont);
        closeBtn->SetFont(&themeFont);
        
        rgb_color panelTextColor = ui_color(B_PANEL_TEXT_COLOR);
        closeBtn->SetHighColor(panelTextColor);

        closeBtn->SetExplicitPreferredSize(BSize(96, 20));
        closeBtn->SetExplicitAlignment(BAlignment(B_ALIGN_CENTER, B_ALIGN_TOP));

        BLayoutBuilder::Group<>(bgPanel, B_VERTICAL, 2)
            .SetInsets(4) 
            .Add(grid)
            .Add(closeBtn)
            .End();
        
        BSize optimalSize = bgPanel->MinSize();
        ResizeTo(optimalSize.width, optimalSize.height);
        MoveTo(screenPos.x, screenPos.y - Bounds().Height() - 5.0f);
    }

    virtual void WindowActivated(bool active) {
        BWindow::WindowActivated(active);
        if (!active) {
            PostMessage(B_QUIT_REQUESTED);
        }
    }

    // CRITICAL BUG FIX: Fires right before the window closes to alert our parent window thread
    virtual void Quit() {
        // Send a notification message thread-safely across the boundary 
        fTargetMessenger.SendMessage(MSG_POPUP_WAS_DESTROYED);
        BWindow::Quit();
    }

private:
    BMessenger fTargetMessenger;
};








class CricketWindow : public BWindow {
public:
    // --- Restore flexible window border decoration masks ---
    CricketWindow() : BWindow(BRect(100, 100, 900, 600), "Cricket IRC Client", 
                        B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS) {
        

        SetFlags(Flags() | B_QUIT_ON_WINDOW_CLOSE);

        //@constructor
        BString windowTitle;
        windowTitle << AppInfo::VERSION_STRING;
        SetTitle(windowTitle.String());
		fActiveIconPopup = nullptr;
		fActiveListWindow = nullptr;

        
        // 1. Setup Channel Tree View (Left Side)
        fChannelTree = new BOutlineListView("channel_tree");
        BScrollView* channelScroll = new BScrollView("scroll_channels", fChannelTree, 0, false, true);
        
        
        
        
        // --- NEW COMPACT ICON DROPDOWN LAUNCHER CONTROLS ---
        fIconToggleButton = new BButton("iconToggleBtn", "Emots ▾", new BMessage(MSG_TOGGLE_ICON_POPUP));
        fIconToggleButton->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
        fIconToggleButton->SetTarget(this);
        // --- END OF GRID SELECTION SETUP ---

       
        // 2. Load Config and Populate Servers Tree ONCE
        load_config(); 

        // Loop A: Populate hardcoded default servers (Libera / OFTC)
        for (size_t i = 0; i < cfg.servers.size(); i++) {
            ServerTreeItem* serverNode = new ServerTreeItem(cfg.servers[i].name.c_str(), i, false, false);
            serverNode->SetHideStatus(cfg.servers[i].hideStatusMessages);
            fChannelTree->AddItem(serverNode);
            
            if (i == 0) fLiberaNode = serverNode;
            if (i == 1) fOftcNode = serverNode;
        }

        // Loop B: Populate user-defined custom servers dynamically
        for (size_t i = 0; i < cfg.customServers.size(); i++) {
            ServerTreeItem* customNode = new ServerTreeItem(cfg.customServers[i].name.c_str(), i, false, true);
            customNode->SetHideStatus(cfg.customServers[i].hideStatusMessages);
            fChannelTree->AddItem(customNode);
        }
        
        // 3. Create Topic View bar Control
        // REMOVED: MakeEditable(false) to allow typing inside the field
        fTopicView = new BTextControl("topic_view", "Topic:", "No topic set.", new BMessage(MSG_TOPIC_CHANGED));
        
        // Ensure that this text field sends its messaging packet back into your main window context
        fTopicView->SetTarget(this);
        
        // 4. Setup Middle & Right Components
        fChatLog = new BTextView("chat_log");
        fChatLog->MakeEditable(false);
        fChatLog->SetStylable(true);

        BScrollView* chatScroll = new BScrollView("scroll_chat", fChatLog, 0, false, true);

        // --- NEW: Custom Draw Engine View Canvas Instantiation ---
        fCustomChatLog = new CustomChatView(BRect(), "custom_draw_view", B_FOLLOW_ALL, B_WILL_DRAW);
        // Note: Wrapping a direct BView inside a scrollview requires explicit scroll implementation.
        // For simple presentation management, we swap whole container view hierarchies.
        BScrollView* customScroll = new BScrollView("scroll_custom", fCustomChatLog, 0, false, true);

        // --- NEW: Layout-Safe Group View Container Setup ---
        // Using BGroupView seamlessly integrates with Layout Builders for runtime swapping
        fChatContainer = new BGroupView(B_VERTICAL, 0);
        
        fUserList = new BListView("user_list");
        
        fUserList->SetSelectionMessage(new BMessage(MSG_USER_LIST_CONTEXT_CLICK));

        BScrollView* userScroll = new BScrollView("scroll_users", fUserList, 0, false, true);

        fInputControl = new BTextControl("input", "", "", new BMessage(MSG_SEND_MESSAGE));


        // Ensure we have at least one server available to read initial sizes from safely
        int32 initialServerListSize = cfg.serverListFontSize;
        int32 initialChatLogSize    = cfg.chatLogFontSize;
        int32 initialUserListSize   = cfg.userListFontSize;

        if (!cfg.servers.empty()) {
            initialServerListSize = cfg.servers[0].serverListFontSize;
            initialChatLogSize    = cfg.servers[0].chatLogFontSize;
            initialUserListSize   = cfg.servers[0].userListFontSize;
        }

        // Apply saved font choices on startup based on the active server's settings
        BFont initialFont;

        fChannelTree->GetFont(&initialFont);
        initialFont.SetSize(initialServerListSize);
        fChannelTree->SetFont(&initialFont, B_FONT_SIZE); // Pass B_FONT_SIZE flag to force invalidate/redraw

        fChatLog->GetFont(&initialFont);
        initialFont.SetSize(initialChatLogSize);
        fChatLog->SetFont(&initialFont, B_FONT_SIZE);

        // Also push text size preferences right down to custom engine tracker
        fCustomChatLog->SetLineHeight(initialChatLogSize + 4.0f); 

        fUserList->GetFont(&initialFont);
        initialFont.SetSize(initialUserListSize);
        fUserList->SetFont(&initialFont, B_FONT_SIZE);
        
        fTopicView->GetFont(&initialFont);
        initialFont.SetSize(initialChatLogSize);
        fTopicView->SetFont(&initialFont, B_FONT_SIZE);
        fTopicView->TextView()->SetFontAndColor(&initialFont, B_FONT_SIZE);

        // ... (Keep Layout Architecture/BSplitView code exactly as it is below) ...


       // 5. Layout Architecture using Adjustable Split Panes
        BSplitView* mainSplitter = new BSplitView(B_HORIZONTAL, 5.0f);
        
        BView* centerStackView = BLayoutBuilder::Group<>(B_VERTICAL, 5)
            .Add(fTopicView, 0.0)         
            .Add(fChatContainer, 1.0)     
            .View();

        mainSplitter->AddChild(channelScroll, 0.16f);  
        mainSplitter->AddChild(centerStackView, 0.73f); 
        mainSplitter->AddChild(userScroll, 0.11f);     

        channelScroll->SetExplicitPreferredSize(BSize(130, B_SIZE_UNLIMITED)); 
        userScroll->SetExplicitPreferredSize(BSize(110, B_SIZE_UNLIMITED));    

        // FIXED BOTTOM TOOLBAR: Using our layout-safe toggle button control
        BLayoutBuilder::Group<>(this, B_VERTICAL, 5)
            .SetInsets(10)
            .Add(mainSplitter, 1.0) 
            .AddGroup(B_HORIZONTAL, 5, 0.0) 
                .Add(fIconToggleButton, 0.0) // FIX: Places the layout-safe toggle button here
                .Add(fInputControl, 1.0)     
            .End();




        // 6. Initialize State Properties
        fChannelTree->SetSelectionMessage(new BMessage('slch'));
        fActiveBufferItem = nullptr;        
        fLiberaThread = -1;
        fOftcThread = -1;
        fLiberaSocket = nullptr;
        fOftcSocket = nullptr;
        fCurrentServerNode = nullptr;
        
        if (!cfg.servers.empty()) {
            fMyNick = cfg.servers[0].nick.c_str();
        } else {
            // High-precision clock backup fallback to generate matching unique IDs anywhere
            srand(static_cast<unsigned int>(real_time_clock_usecs()));
            int randomSuffix = 1000 + (rand() % 9000);
            fMyNick.SetToFormat("HaikuIRCUser%d", randomSuffix);
        }
        
        // 7. Process Automatic Startup Connections instantly
        for (int32 i = 0; i < fChannelTree->CountItems(); i++) {
            ServerTreeItem* srvNode = dynamic_cast<ServerTreeItem*>(fChannelTree->ItemAt(i));
            if (srvNode != nullptr) {
                bool triggerConnect = false;
                size_t idx = srvNode->GetIndex();

                // Check the configuration storage location based on the server node classification type
                if (srvNode->IsCustom()) {
                    if (idx < cfg.customServers.size()) {
                        triggerConnect = cfg.customServers[idx].autoConnect;
                    }
                } else {
                    if (idx < cfg.servers.size()) {
                        triggerConnect = cfg.servers[idx].autoConnect;
                    }
                }

                if (triggerConnect) {
                    ConnectToServer(srvNode);
                }
            }
        }              
    }




	
void Show()
{
    // 1. Allow the base Haiku window engine to map layout sub-containers first
    BWindow::Show();

    // 2. DYNAMIC BACKGROUND INITIALIZATION ON BOOT
    if (fCustomChatLog != nullptr) {
        std::string targetServerName = "";

        // If a server is actively highlighted on launch, grab its name
        if (fCurrentServerNode != nullptr) {
            targetServerName = fCurrentServerNode->Text();
        } 
        // Fallback: If nothing is selected yet, default to the first server in the tree
        else if (fChannelTree != nullptr && fChannelTree->CountItems() > 0) {
            ServerTreeItem* firstServer = dynamic_cast<ServerTreeItem*>(fChannelTree->ItemAt(0));
            if (firstServer != nullptr) {
                targetServerName = firstServer->Text();
            }
        }

        // Apply background matching configurations
        if (!targetServerName.empty()) {
            bool found = false;
            for (size_t i = 0; i < cfg.servers.size(); i++) {
                if (cfg.servers[i].name == targetServerName) {
                    if (!cfg.servers[i].backgroundImagePath.empty()) {
                        fCustomChatLog->SetBackgroundImage(cfg.servers[i].backgroundImagePath.c_str());
                        fCustomChatLog->SetBackgroundDimming(cfg.servers[i].backgroundOpacity);
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (size_t i = 0; i < cfg.customServers.size(); i++) {
                    if (cfg.customServers[i].name == targetServerName) {
                        if (!cfg.customServers[i].backgroundImagePath.empty()) {
                            fCustomChatLog->SetBackgroundImage(cfg.customServers[i].backgroundImagePath.c_str());
                            fCustomChatLog->SetBackgroundDimming(cfg.customServers[i].backgroundOpacity);
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // --- ADD THE LOOKUP BLOCK ---
    size_t activeIdx = (size_t)selectedConfig;
    ServerConfig* activeSrv = nullptr;
    if (activeIdx < cfg.servers.size()) {
        activeSrv = &cfg.servers[activeIdx];
    } else {
        size_t customIdx = activeIdx - cfg.servers.size();
        if (customIdx < cfg.customServers.size()) {
            activeSrv = &cfg.customServers[customIdx];
        }
    }

    // 3. Trigger the high-performance engine swap pass after the initial draw cycle
    bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
	if (drawEngineActive) { 
        BMessage initDrawMsg(MSG_TOGGLE_CUSTOM_DRAW);
        initDrawMsg.AddBool("initial_boot_pass", true);
        this->PostMessage(&initDrawMsg);
    }
    
    if (fCustomChatLog != nullptr && fCurrentServerNode == nullptr) {
        // Force the custom chat view to repaint itself with our default graphic
        fCustomChatLog->Invalidate(); 
    }
    
    
}





    ~CricketWindow() {
        // 1. Prepare the custom sign-off message payload
        BString quitPayload;
        quitPayload << "QUIT :" << cfg.quitMessage.c_str() << "\r\n";

        std::vector<std::pair<thread_id, BSecureSocket*>> shutdownSnapshot;
        
        Lock();
        for (auto const& [serverNode, socketPtr] : fServerSockets) {
            if (socketPtr != nullptr) {
                socketPtr->Write(quitPayload.String(), quitPayload.Length());
                thread_id tid = (fServerThreads.count(serverNode) > 0) ? fServerThreads[serverNode] : -1;
                shutdownSnapshot.push_back({tid, socketPtr});
            }
        }
        Unlock();

        // Give the networking hardware a brief moment to flush the QUIT strings out
        snooze(50000); // 50ms

        // 2. Disconnect all sockets to tell the threads to drop out of their loops
        for (auto const& [tid, socketPtr] : shutdownSnapshot) {
            if (socketPtr != nullptr) {
                socketPtr->Disconnect(); 
            }
        }

        // Give the background worker threads a quick window to wake up and close on their own
        snooze(50000); // 50ms

        // 3. FORCE KILL STUBBORN THREADS: Completely breaks any deadlocks
        for (auto const& [tid, socketPtr] : shutdownSnapshot) {
            if (tid >= 0) {
                // If the thread is still alive and didn't close gracefully, kill it instantly!
                // This stops it from trapping wait_for_thread in a deadlock.
                thread_info info;
                if (get_thread_info(tid, &info) == B_OK) {
                    kill_thread(tid); 
                }
            }
        }

        // 4. Clean up memory collections safely now that threads are guaranteed dead
        for (auto const& [key, listPtr] : fChannelUsers) {
            delete listPtr;
        }

        // 5. Force the application process to instantly terminate and leave the Deskbar
        be_app->PostMessage(B_QUIT_REQUESTED); 
    }



void DisplayBanListDialog(BString channelName)
{
    BRect windowFrame(0, 0, 450, 300);
    BRect screenFrame = Frame();
    windowFrame.OffsetTo(
        screenFrame.left + (screenFrame.Width() - windowFrame.Width()) / 2,
        screenFrame.top + (screenFrame.Height() - windowFrame.Height()) / 2
    );

    BWindow* banWin = new BWindow(windowFrame, (BString("Bans: ") << channelName).String(), 
                                 B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE);

    BView* panel = new BView(banWin->Bounds(), "banPanel", B_FOLLOW_ALL, B_WILL_DRAW);
    panel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    banWin->AddChild(panel);

    BTextView* titleText = new BTextView(BRect(15, 10, 435, 35), "title", BRect(0,0,420,20), B_FOLLOW_ALL, B_WILL_DRAW);
    titleText->SetText((BString("Active ban mask protocols running in ") << channelName << ":").String());
    titleText->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    titleText->MakeEditable(false);
    titleText->MakeSelectable(false);
    panel->AddChild(titleText);

    BListView* banListView = new BListView(BRect(15, 40, 435, 220), "banListWidget", B_SINGLE_SELECTION_LIST);
    
    // ==================== REPOPULATE WITH LIVE HARVEST DATA ====================
    for (int32 i = 0; i < fCurrentBanHarvest.CountItems(); i++) {
        BString* storedMask = fCurrentBanHarvest.ItemAt(i);
        if (storedMask != nullptr) {
            banListView->AddItem(new BStringItem(storedMask->String()));
        }
    }
    
    // Clear out the tracking storage array right away so it is fresh for your next check
    fCurrentBanHarvest.MakeEmpty();
    // ===========================================================================

    panel->AddChild(banListView);

    BButton* closeBtn = new BButton(BRect(245, 260, 330, 285), "close", "Close", new BMessage(B_QUIT_REQUESTED));
    BButton* unbanBtn = new BButton(BRect(340, 260, 435, 285), "unban", "Remove Ban", new BMessage(MSG_CONTEXT_UNBAN_SUBMIT));
    
    closeBtn->SetTarget(banWin);
    unbanBtn->SetTarget(this);

    BMessage* unbanPayload = new BMessage(MSG_CONTEXT_UNBAN_SUBMIT);
    unbanPayload->AddString("target_channel", channelName);
    unbanPayload->AddPointer("window_ref", banWin);
    unbanBtn->SetMessage(unbanPayload);

    panel->AddChild(closeBtn);
    panel->AddChild(unbanBtn);

    banWin->Show();
}







void RebuildActiveChannelBuffer() {
	size_t activeIdx = (size_t)selectedConfig;
    ServerConfig* activeSrv = nullptr;
    if (activeIdx < cfg.servers.size()) {
        activeSrv = &cfg.servers[activeIdx];
    } else {
        size_t customIdx = activeIdx - cfg.servers.size();
        if (customIdx < cfg.customServers.size()) {
            activeSrv = &cfg.customServers[customIdx];
        }
    }
	
	fIsLoadingHistory = true;
    if (fActiveBufferItem == nullptr || !fChatLog || !fCustomChatLog) return;

    // 1. Fetch the raw historical plain-text string from memory cache map
    BString channelHistory = fTextBuffers[fActiveBufferItem];
    if (channelHistory.Length() == 0) return;

    // 2. Clear both layout views completely to prevent duplicated text layering
    fChatLog->SetText("");
    fCustomChatLog->ClearAllLines(); // Canvas is now safely cleared out

    // 3. Ensure the custom log view is pointing at the selected workspace channel room
    bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
	if (drawEngineActive) { 
        fCustomChatLog->SetActiveChannel(fActiveBufferItem);
    }

    // 4. Tokenize the massive historical text block into separate individual text lines
    int32 startSearch = 0;
    while (startSearch < channelHistory.Length()) {
        int32 nextNewline = channelHistory.FindFirst("\n", startSearch);
        if (nextNewline == B_ERROR) break;

        BString singleLine;
        channelHistory.CopyInto(singleLine, startSearch, nextNewline - startSearch);
        
        // Pass the individual line through the isolated formatting engine
        this->RenderLineWithoutCaching(fActiveBufferItem, singleLine);

        startSearch = nextNewline + 1;
    }

    // 5. Force exactly ONE layout redraw pass once the history is fully built out
	if (drawEngineActive) { 
        fCustomChatLog->Invalidate();
    } else {
        fChatLog->ScrollToSelection();
    }
    fIsLoadingHistory = false;
}




void RenderLineWithoutCaching(BStringItem* itemNode, BString text) {
    if (!text.EndsWith("\n")) text << "\n";
    if (!fChatLog || !fCustomChatLog) return;

    // Fetch the context-driven active server config
    size_t activeIdx = (size_t)selectedConfig;
    ServerConfig* activeSrv = nullptr;
    
    if (activeIdx < cfg.servers.size()) {
        activeSrv = &cfg.servers[activeIdx];
    } else {
        size_t customIdx = activeIdx - cfg.servers.size();
        if (customIdx < cfg.customServers.size()) {
            activeSrv = &cfg.customServers[customIdx];
        }
    }

    // Determine the precise baseline font size for this stream layer
    int32 targetChatFontSize = activeSrv ? activeSrv->chatLogFontSize : cfg.chatLogFontSize;

    // Prepare Font Variations using the contextual target scale
    BFont regularChatFont;
    fChatLog->GetFont(&regularChatFont);
    regularChatFont.SetSize(targetChatFontSize);

    BFont boldChatFont = regularChatFont;
    boldChatFont.SetFace(B_BOLD_FACE);

    BFont urlLinkFont = regularChatFont;
    urlLinkFont.SetFace(B_UNDERSCORE_FACE);

    // Define explicit, adaptive colors matching Haiku global UI preferences
    rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);     
    rgb_color hyperlinkColor = {51, 153, 255, 255}; 

    // Find Positions
    int32 openingBracketIdx = text.FindFirst("<");
    int32 closingBracketIdx = text.FindFirst("> ");
    int32 urlStartIdx = text.IFindFirst("http://");
    if (urlStartIdx == B_ERROR) urlStartIdx = text.IFindFirst("https://");

    // 1. Allocate the run array layout cleanly before routing
    int32 maxRuns = 6;
    size_t arraySize = sizeof(text_run_array) + (sizeof(text_run) * (maxRuns - 1));
    text_run_array* runArray = (text_run_array*)malloc(arraySize);
    
    if (runArray != nullptr) {
        int32 runCount = 0;

        // RULE 0: Start of line is always Regular
        runArray->runs[runCount].offset = 0;
        runArray->runs[runCount].font = regularChatFont;
        runArray->runs[runCount].color = textColor;
        runCount++;

        // NICKNAME BOLDING (<Nick>)
        if (openingBracketIdx != B_ERROR && closingBracketIdx != B_ERROR && openingBracketIdx < closingBracketIdx) {
            if (openingBracketIdx > 0) {
                runArray->runs[runCount].offset = openingBracketIdx;
                runArray->runs[runCount].font = boldChatFont;
                runArray->runs[runCount].color = textColor;
                runCount++;
            } else {
                runArray->runs[0].font = boldChatFont;
            }

            // Reset to Regular text right after the closing bracket "> "
            runArray->runs[runCount].offset = closingBracketIdx + 2;
            runArray->runs[runCount].font = regularChatFont;
            runArray->runs[runCount].color = textColor;
            runCount++;
        }

        // URL COLORING (http...)
        if (urlStartIdx != B_ERROR) {
            bool inserted = false;
            for (int32 i = 0; i < runCount; i++) {
                if (runArray->runs[i].offset == urlStartIdx) {
                    runArray->runs[i].font = urlLinkFont;
                    runArray->runs[i].color = hyperlinkColor;
                    inserted = true;
                    break;
                }
            }

            if (!inserted && (runCount == 0 || urlStartIdx > runArray->runs[runCount - 1].offset)) {
                runArray->runs[runCount].offset = urlStartIdx;
                runArray->runs[runCount].font = urlLinkFont;
                runArray->runs[runCount].color = hyperlinkColor;
                runCount++;
            }

            // Reset back to Regular text after the URL
            int32 urlEndIdx = text.FindFirst(" ", urlStartIdx);
            if (urlEndIdx == B_ERROR) urlEndIdx = text.FindFirst("\n", urlStartIdx);
            if (urlEndIdx == B_ERROR) urlEndIdx = text.Length();
            
            // Enforce structural style reset at the link terminal layout boundary 
            if (urlEndIdx <= text.Length() && runCount < maxRuns) {
                runArray->runs[runCount].offset = urlEndIdx;
                runArray->runs[runCount].font = regularChatFont;
                runArray->runs[runCount].color = textColor;
                runCount++;
            }
        }

        runArray->count = runCount;

        // --- Route to the active view layer constraints ---
        bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
		if (drawEngineActive) { 
            // Pass incoming itemNode directly to preserve multi-room isolation routes
            fCustomChatLog->AddStyledLine(itemNode, text, runArray);
        } else {
            int32 textLength = fChatLog->TextLength();
            fChatLog->Select(textLength, textLength);
            fChatLog->Insert(text.String(), runArray);
        }
        
        free(runArray);
    } else {
        // Fallback insertion route if system out of memory
        bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
		if (drawEngineActive) { 
            // Pass incoming itemNode directly here as well
            fCustomChatLog->AddStyledLine(itemNode, text, nullptr);
        } else {
            int32 textLength = fChatLog->TextLength();
            fChatLog->Select(textLength, textLength);
            fChatLog->Insert(text.String());
        }
    }
}






private:
    //@Menus

void ShowContextMenu(BPoint screenPoint, BListItem* item) {
	
	size_t activeIdx = (size_t)selectedConfig;
    ServerConfig* activeSrv = nullptr;
    if (activeIdx < cfg.servers.size()) {
        activeSrv = &cfg.servers[activeIdx];
    } else {
        size_t customIdx = activeIdx - cfg.servers.size();
        if (customIdx < cfg.customServers.size()) {
            activeSrv = &cfg.customServers[customIdx];
        }
    }
	
    if (item == nullptr) return; // Safety check
    
    // 1. Channel menu logic
    ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(item);
    if (chanItem != nullptr) {
        BPopUpMenu* menu = new BPopUpMenu("ChannelOptions", false, false);
        
        BString label = chanItem->IsAutoJoin() ? "Disable Auto-Join" : "Enable Auto-Join";
        BMessage* toggleMsg = new BMessage(MSG_TOGGLE_AUTOJOIN);
        toggleMsg->AddPointer("chan_item", chanItem); 
        menu->AddItem(new BMenuItem(label.String(), toggleMsg));
        
        menu->AddSeparatorItem();
        
        // NEW: View Channel Ban List Option
        BMessage* banListMsg = new BMessage(MSG_CONTEXT_SHOW_BANS);
        banListMsg->AddPointer("chan_item", chanItem); // Pass the specific channel node pointer
        menu->AddItem(new BMenuItem("View Channel Ban List...", banListMsg));
        
        menu->AddSeparatorItem();
        
        BMessage* removeMsg = new BMessage('rmch'); 
        removeMsg->AddPointer("channel_item", chanItem);
        menu->AddItem(new BMenuItem("Remove Channel", removeMsg));
        
        menu->SetTargetForItems(this);
        menu->Go(screenPoint, true, true, true);
        return;
    }


    // 2. Server menu logic
    ServerTreeItem* srvItem = dynamic_cast<ServerTreeItem*>(item);
    if (srvItem != nullptr) {
        BPopUpMenu* menu = new BPopUpMenu("ServerOptions", false, false);
        
        bool isConnected = false;
        if (fServerSockets.find(srvItem) != fServerSockets.end()) {
            if (fServerSockets[srvItem] != nullptr) {
                isConnected = true;
            }
        }

        if (isConnected) {
            BMessage* disconnectMsg = new BMessage(MSG_DISCONNECT_SERVER);
            disconnectMsg->AddPointer("server_item", srvItem);
            menu->AddItem(new BMenuItem("Disconnect", disconnectMsg));
        } else {
            if (srvItem->IsCustom()) {
                BMessage* connectMsg = new BMessage(MSG_CONNECT_CUSTOM_SERVER);
                connectMsg->AddPointer("server_item", srvItem);
                menu->AddItem(new BMenuItem("Connect", connectMsg));
            } else {
                uint32 commandMsg = (srvItem == fLiberaNode) ? MSG_CONNECT_LIBERA : MSG_CONNECT_OFTC;
                menu->AddItem(new BMenuItem("Connect", new BMessage(commandMsg)));
            }
        }
        menu->AddSeparatorItem();

        // Toggle Auto-Connect Option
        BString toggleConnectLabel = "Auto-Connect on Startup";
        BMessage* toggleConnectMsg = new BMessage(MSG_TOGGLE_AUTOCONNECT);
        toggleConnectMsg->AddPointer("server_item", srvItem);
        BMenuItem* connectMenuItem = new BMenuItem(toggleConnectLabel.String(), toggleConnectMsg);
        if (srvItem->IsAutoConnect()) connectMenuItem->SetMarked(true);
        menu->AddItem(connectMenuItem);

        // Toggle Auto-Reconnect Option
        BString toggleReconnectLabel = srvItem->IsAutoReconnect() ? "Auto-Reconnect on Disconnect" : "Auto-Reconnect on Disconnect";
        BMessage* toggleReconnectMsg = new BMessage(MSG_TOGGLE_AUTORECONNECT);
        toggleReconnectMsg->AddPointer("server_item", srvItem);
        BMenuItem* reconnectMenuItem = new BMenuItem(toggleReconnectLabel.String(), toggleReconnectMsg);
        if (srvItem->IsAutoReconnect()) reconnectMenuItem->SetMarked(true);
        menu->AddItem(reconnectMenuItem);
        
        // Status Messages Suppression Option
        BString toggleStatusLabel = srvItem->IsHideStatus() ? "Hide Status Messages" : "Hide Status Messages";
        BMessage* toggleStatusMsg = new BMessage(MSG_TOGGLE_HIDE_STATUS);
        toggleStatusMsg->AddPointer("server_item", srvItem);
        BMenuItem* statusMenuItem = new BMenuItem(toggleStatusLabel.String(), toggleStatusMsg);
        if (srvItem->IsHideStatus()) statusMenuItem->SetMarked(true);
        menu->AddItem(statusMenuItem); // <-- FIXED: Added missing menu allocation injection pass


        // Custom Inline Emoticons Toggle 
        BString toggleEmotesLabel = "Enable Emoticons";
        BMessage* toggleEmotesMsg = new BMessage('tgem');
        toggleEmotesMsg->AddPointer("server_item", srvItem);
        BMenuItem* emotesMenuItem = new BMenuItem(toggleEmotesLabel.String(), toggleEmotesMsg);
        
        /*
        // Find the configuration matching this specific server item to set initial state
        if (srvItem != nullptr) {
            BString targetServerName(srvItem->Text());
            bool initialMarkState = false;
            bool serverFound = false;

            // 1. Search primary servers list
            for (const auto& srv : cfg.servers) {
                if (BString(srv.name.c_str()) == targetServerName) {
                    initialMarkState = srv.enableEmoticons;
                    serverFound = true;
                    break;
                }
            }
            
            // 2. Fallback to user-defined custom server profiles ONLY if not found in primary list
            if (!serverFound) {
                for (const auto& srv : cfg.customServers) {
                    if (BString(srv.name.c_str()) == targetServerName) {
                        initialMarkState = srv.enableEmoticons;
                        break;
                    }
                }
            }

            if (initialMarkState) emotesMenuItem->SetMarked(true);
        }

        menu->AddItem(emotesMenuItem);
        */
                
        // Channel List Option
       	menu->AddSeparatorItem();
        BMessage* listMsg = new BMessage(MSG_CONTEXT_CHAN_LIST);
        listMsg->AddPointer("server_item", srvItem);
        menu->AddItem(new BMenuItem("Channel List", listMsg));
        menu->AddSeparatorItem();
        
                
        // Open specific server configuration window

        BMessage* configMsg = new BMessage(MSG_CONTEXT_CONFIGURE_SERVER);
        configMsg->AddPointer("server_item", srvItem);
        menu->AddItem(new BMenuItem("Server Settings...", configMsg));  
        
        
        /*
        menu->AddSeparatorItem();

        // Custom Draw Engine Toggle 
        BString toggleDrawLabel = "Enable High Performance Draw Engine";
        BMessage* toggleDrawMsg = new BMessage(MSG_TOGGLE_CUSTOM_DRAW);
        toggleDrawMsg->AddPointer("server_item", srvItem);
        BMenuItem* drawMenuItem = new BMenuItem(toggleDrawLabel.String(), toggleDrawMsg);
        // Uses the runtime configuration flag to set initial checkmark state
        bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
		if (drawEngineActive) drawMenuItem->SetMarked(true);
        menu->AddItem(drawMenuItem);
        */
        
        // Put Add Server Here 
        menu->AddSeparatorItem();
        BMessage* addSrvMsg = new BMessage('adcs'); // Reuses your original button message identifier
        menu->AddItem(new BMenuItem("Add Custom Server…", addSrvMsg));
        

        if (srvItem->IsCustom()) {
            menu->AddSeparatorItem();
            BMessage* deleteMsg = new BMessage('dlcs'); 
            deleteMsg->AddPointer("server_item", srvItem);
            menu->AddItem(new BMenuItem("Delete Custom Server", deleteMsg));
        }
        
        
            // About Option
        menu->AddSeparatorItem();
        BMessage* aboutMsg = new BMessage(MSG_CONTEXT_ABOUT);
        menu->AddItem(new BMenuItem("About...", aboutMsg));    
        
        menu->SetTargetForItems(this);
        menu->Go(screenPoint, true, true, true);
    }
}




    void ConnectToServer(ServerTreeItem* targetNode) {
        if (targetNode == nullptr) return;

        // 1. Check if this specific server is already connecting/connected
        if (fServerSockets.count(targetNode) > 0 && fServerSockets[targetNode] != nullptr) {
            return; 
        }
        
       
        // If nothing is selected yet, make this server root item active
        if (fActiveBufferItem == nullptr) {
            fActiveBufferItem = targetNode;
        }

        // 2. Clear out old historical text errors directly on the targetNode tracking key
        fTextBuffers[targetNode] = "";

        // Dynamically read the name from the item text for the log buffer echo
        BString logMessage;
        logMessage.SetToFormat("Connecting to %s...\n", targetNode->Text());
        fTextBuffers[targetNode] << logMessage;
        
        // Update the screen draw engine if the current node matches the active view focus
        if (fActiveBufferItem == targetNode && fChatLog != nullptr) {
            fChatLog->SetText(fTextBuffers[targetNode].String());
        } else if (fActiveBufferItem == targetNode && fCustomChatLog != nullptr && cfg.useCustomDrawFunction) {
            this->RebuildActiveChannelBuffer();
        }

        // 3. Generate a unique worker name thread label string dynamically
        BString threadName;
        threadName.SetToFormat("%sWorker", targetNode->Text());
        threadName.ReplaceAll(" ", ""); 

        // Spawn the thread and associate it natively inside our index tracker map
        thread_id newThread = spawn_thread(NetworkLoop, threadName.String(), B_NORMAL_PRIORITY, targetNode);
        if (newThread >= 0) {
            fServerThreads[targetNode] = newThread;
            
            if (targetNode == fLiberaNode) fLiberaThread = newThread;
            if (targetNode == fOftcNode)   fOftcThread = newThread;

            resume_thread(newThread);
        }
    }


void LogToItemBuffer(BStringItem* itemNode, BString text) {
    if (itemNode == nullptr) return;
    
    // Ensure trailing newline for layout alignment consistency
    if (!text.EndsWith("\n")) text << "\n";
    fTextBuffers[itemNode] << text;
    
     
    // --- NEW: FIRE THE AUTOMATIC DISK WRITER HOOK ---
    // It filters out active servers automatically based on individual user config states     
    this->WriteLogToFile(itemNode, text);
    
    if (fActiveBufferItem == itemNode) {
        if (!fChatLog || !fCustomChatLog || !fChatContainer) return;

        // Fetch the context-driven active server config
        size_t activeIdx = (size_t)selectedConfig;
        ServerConfig* activeSrv = nullptr;
        
        if (activeIdx < cfg.servers.size()) {
            activeSrv = &cfg.servers[activeIdx];
        } else {
            size_t customIdx = activeIdx - cfg.servers.size();
            if (customIdx < cfg.customServers.size()) {
                activeSrv = &cfg.customServers[customIdx];
            }
        }

        // Determine the precise baseline font size for this active buffer window
        int32 targetChatFontSize = activeSrv ? activeSrv->chatLogFontSize : cfg.chatLogFontSize;

        // TERMINAL DIAGNOSTIC: Print the incoming raw text line string
        // if (cfg.debugEnable) printf("\n[DEBUG] LogToItemBuffer Input text: %s", text.String());

        // Prepare Font Variations using the contextual target scale
        BFont regularChatFont;
        fChatLog->GetFont(&regularChatFont);
        regularChatFont.SetSize(targetChatFontSize);

        BFont boldChatFont = regularChatFont;
        boldChatFont.SetFace(B_BOLD_FACE);

        BFont urlLinkFont = regularChatFont;
        urlLinkFont.SetFace(B_UNDERSCORE_FACE);

        // Define explicit, adaptive colors matching Haiku global UI preferences
        rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);     
        rgb_color hyperlinkColor = {51, 153, 255, 255}; 

        // Find Positions
        int32 openingBracketIdx = text.FindFirst("<");
        int32 closingBracketIdx = text.FindFirst("> ");
        int32 urlStartIdx = text.IFindFirst("http://");
        if (urlStartIdx == B_ERROR) urlStartIdx = text.IFindFirst("https://");

        // TERMINAL DIAGNOSTIC: Print calculated indexing values
        // if (cfg.debugEnable) printf("[DEBUG] Offsets calculated -> OpenBracket: %d, CloseBracket: %d, URLStart: %d\n", 
        //       (int)openingBracketIdx, (int)closingBracketIdx, (int)urlStartIdx);

        // 1. Allocate the run array layout cleanly before routing
        int32 maxRuns = 6;
        size_t arraySize = sizeof(text_run_array) + (sizeof(text_run) * (maxRuns - 1));
        text_run_array* runArray = (text_run_array*)malloc(arraySize);
        
        if (runArray != nullptr) {
            int32 runCount = 0;

            // RULE 0: Start of line is always Regular
            runArray->runs[runCount].offset = 0;
            runArray->runs[runCount].font = regularChatFont;
            runArray->runs[runCount].color = textColor;
            runCount++;

            // NICKNAME BOLDING (<Nick>)
            if (openingBracketIdx != B_ERROR && closingBracketIdx != B_ERROR && openingBracketIdx < closingBracketIdx) {
                if (openingBracketIdx > 0) {
                    runArray->runs[runCount].offset = openingBracketIdx;
                    runArray->runs[runCount].font = boldChatFont;
                    runArray->runs[runCount].color = textColor;
                    // if (cfg.debugEnable) printf("[DEBUG] Added Run %d: Bold face applied at index %d\n", runCount, (int)openingBracketIdx);
                    runCount++;
                } else {
                    runArray->runs[0].font = boldChatFont;
                }

                // Reset to Regular text right after the closing bracket "> "
                runArray->runs[runCount].offset = closingBracketIdx + 2;
                runArray->runs[runCount].font = regularChatFont;
                runArray->runs[runCount].color = textColor;
                // if (cfg.debugEnable) printf("[DEBUG] Added Run %d: Regular face reset applied at index %d\n", runCount, (int)(closingBracketIdx + 2));
                runCount++;
            }

            // URL COLORING (http...)
            if (urlStartIdx != B_ERROR) {
                bool inserted = false;
                for (int32 i = 0; i < runCount; i++) {
                    if (runArray->runs[i].offset == urlStartIdx) {
                        runArray->runs[i].font = urlLinkFont;
                        runArray->runs[i].color = hyperlinkColor;
                        inserted = true;
                        break;
                    }
                }

                if (!inserted && (runCount == 0 || urlStartIdx > runArray->runs[runCount - 1].offset)) {
                    runArray->runs[runCount].offset = urlStartIdx;
                    runArray->runs[runCount].font = urlLinkFont;
                    runArray->runs[runCount].color = hyperlinkColor;
                    // if (cfg.debugEnable) printf("[DEBUG] Added Run %d: Blue link applied at index %d\n", runCount, (int)urlStartIdx);
                    runCount++;
                }

                // Reset back to Regular text after the URL
                int32 urlEndIdx = text.FindFirst(" ", urlStartIdx);
                if (urlEndIdx == B_ERROR) urlEndIdx = text.FindFirst("\n", urlStartIdx);
                if (urlEndIdx == B_ERROR) urlEndIdx = text.Length();
                
                // Enforce structural style reset at the link terminal layout boundary 
                if (urlEndIdx <= text.Length() && runCount < maxRuns) {
                    runArray->runs[runCount].offset = urlEndIdx;
                    runArray->runs[runCount].font = regularChatFont;
                    runArray->runs[runCount].color = textColor;
                    //  if (cfg.debugEnable) printf("[DEBUG] Added Run %d: Regular face reset after link applied at index %d\n", runCount, (int)urlEndIdx);
                    runCount++;
                }
            }

            runArray->count = runCount;
            // if (cfg.debugEnable) printf("[DEBUG] Total run array layout counts submitted: %d\n", (int)runCount);

            // 2. Perform the view-swapping via Layout API using the parent scrollviews
            BView* chatScroll = fChatLog->Parent();       
            BView* customScroll = fCustomChatLog->Parent(); 
            BLayout* layout = fChatContainer->GetLayout();

            bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
			if (drawEngineActive) { 
                if (chatScroll && chatScroll->Parent() == fChatContainer) {
                    layout->RemoveView(chatScroll);
                }
                if (customScroll && customScroll->Parent() != fChatContainer) {
                    layout->AddView(customScroll);
                }
                
                // Ensure the canvas updates its active channel visibility node during additions
                fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                
                // Pass itemNode through to the custom draw view engine router 
                fCustomChatLog->AddStyledLine(itemNode, text, runArray); 
            } else {
                if (customScroll && customScroll->Parent() == fChatContainer) {
                    layout->RemoveView(customScroll);
                }
                if (chatScroll && chatScroll->Parent() != fChatContainer) {
                    layout->AddView(chatScroll);
                }
                
                int32 textLength = fChatLog->TextLength();
                fChatLog->Select(textLength, textLength);
                fChatLog->Insert(text.String(), runArray);
            }
            
            free(runArray);
        } else {
            // Fallback insertion route if system out of memory
           bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
			if (drawEngineActive) { 
                fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                fCustomChatLog->AddStyledLine(itemNode, text, nullptr);
            } else {
                int32 textLength = fChatLog->TextLength();
                fChatLog->Select(textLength, textLength);
                fChatLog->Insert(text.String());
            }
        }

        // Handle view boundary updates and scrolling
       bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
		if (drawEngineActive) { 
            fChatContainer->InvalidateLayout();
        } else {
            fChatLog->ScrollToSelection();
        }
    }
}



    BStringItem* FindServerLogNode(ServerTreeItem* serverNode) {
        if (!serverNode) return nullptr;                
        return static_cast<BStringItem*>(serverNode);
    }


    ChannelTreeItem* FindChannelNode(ServerTreeItem* serverNode, BString channelName) {
        if (!serverNode) return nullptr;
        for (int32 i = 0; i < fChannelTree->CountItemsUnder(serverNode, true); i++) {
            // CAST TO OUR NEW TYPE:
            ChannelTreeItem* item = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemUnderAt(serverNode, true, i));
            if (item && BString(item->Text()).ICompare(channelName) == 0) {
                return item;
            }
        }
        return nullptr;
    }
    
    
void UpdateUserAwayState(ChannelTreeItem* chanNode, const char* nickname, bool isAway) {
    if (!chanNode || fChannelUsers.find(chanNode) == fChannelUsers.end() || fChannelUsers[chanNode] == nullptr) {
        return;
    }

    BObjectList<UserListItem, true>* users = fChannelUsers[chanNode];
    BString cleanSearchNick = nickname;
    cleanSearchNick.Trim();

    for (int32 i = 0; i < users->CountItems(); i++) {
        UserListItem* item = users->ItemAt(i);
        if (item != nullptr) {
            BString itemTxt = item->Text();
            itemTxt.Trim();

            int32 prefixCount = 0;
            while (prefixCount < itemTxt.Length()) {
                char c = itemTxt.ByteAt(prefixCount);
                if (c == '~' || c == '&' || c == '%' || c == '@' || c == '+') {
                    prefixCount++;
                } else {
                    break;
                }
            }
            if (prefixCount > 0) {
                itemTxt.Remove(0, prefixCount);
            }

            // If we find our target user inside the persistent cache array, toggle the flag!
            if (itemTxt == cleanSearchNick) {
                item->SetAway(isAway);

                // --- LIVE INDIVIDUAL ROW REPAINT ---
                if (fActiveBufferItem == chanNode) {
                    int32 uiCount = fUserList->CountItems();
                    for (int32 uiIdx = 0; uiIdx < uiCount; uiIdx++) {
                        UserListItem* uiUser = dynamic_cast<UserListItem*>(fUserList->ItemAt(uiIdx));
                        if (uiUser != nullptr) {
                            BString uiItemTxt = uiUser->Text();
                            while (uiItemTxt.StartsWith("~") || uiItemTxt.StartsWith("&") || 
                                   uiItemTxt.StartsWith("%") || uiItemTxt.StartsWith("@") || 
                                   uiItemTxt.StartsWith("+")) {
                                uiItemTxt.Remove(0, 1);
                            }

                            if (uiItemTxt == cleanSearchNick) {
                                uiUser->SetAway(isAway);
                                fUserList->InvalidateItem(uiIdx); // Redraw just this single row frame!
                                break;
                            }
                        }
                    }
                }
                break; 
            }
        }
    }
}




    
    
    

void RefreshUserListUI() {
    if (fActiveBufferItem != nullptr && BString(fActiveBufferItem->Text()).StartsWith("#")) {
        
        while (fUserList->CountItems() > 0) {
            delete fUserList->RemoveItem((int32)0);
        }

        if (fChannelUsers.find(fActiveBufferItem) != fChannelUsers.end()) {
            BObjectList<UserListItem, true>* users = fChannelUsers[fActiveBufferItem];
            if (users != nullptr) {
                
                BFont userListFont;
                fUserList->GetFont(&userListFont);
                userListFont.SetSize(cfg.userListFontSize);

                for (int32 i = 0; i < users->CountItems(); i++) {
                    UserListItem* cachedItem = users->ItemAt(i);
                    if (cachedItem != nullptr) {
                        // Read away status directly from our typed memory list object
                        bool userIsAway = cachedItem->IsAway();

                        // Instantiate our custom visual item class
                        UserListItem* newUserItem = new UserListItem(cachedItem->Text(), userIsAway);
                        fUserList->AddItem(newUserItem);
                        newUserItem->Update(fUserList, &userListFont);
                    }
                }
                
                fUserList->SortItems(SortUsersByRank);
                fUserList->InvalidateLayout();
                fUserList->Invalidate();
            }
        }
    }
}




void ParseAndDisplayIRC(BString line, ServerTreeItem* contextServer) {   	    	
    line.ReplaceAll("\r", "");
    line.ReplaceAll("\n", ""); // Safe to clean network markers; we append gracefully later

    if (line.Length() == 0 || line.StartsWith("PING")) return;

    BString prefix = "", command = "", trailing = "";
    int32 trailingIdx = line.FindFirst(" :");
    if (trailingIdx != B_ERROR) {
        line.CopyInto(trailing, trailingIdx + 2, line.Length() - trailingIdx - 2);
        line.Truncate(trailingIdx);
    }

    int32 firstSpace = line.FindFirst(" ");
    if (line.StartsWith(":")) {
        line.CopyInto(prefix, 1, firstSpace - 1);
        line.Remove(0, firstSpace + 1);
        firstSpace = line.FindFirst(" ");
    }
    if (firstSpace != B_ERROR) {
        line.CopyInto(command, 0, firstSpace);
        line.Remove(0, firstSpace + 1);
    } else {
        command = line;
    }

    // Use the passed context connection. Fallback to Libera only if null.
    if (contextServer == nullptr) {
        contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
    }


	// @JOIN
     // Catch Successful Join actions (Handles Us AND Other Users)
    if (command == "JOIN") {
        BString channelJoined = trailing.Length() > 0 ? trailing : line;
        
        // --- Aggressively trim all accidental network padding spaces ---
        channelJoined.Trim();
        channelJoined.ReplaceAll(" ", "");
        
        BString userWhoJoined = prefix;
        int32 exclamIdx = userWhoJoined.FindFirst("!");
        if (exclamIdx != B_ERROR) userWhoJoined.Truncate(exclamIdx);

        // Target the specific server connection context pointer explicitly
        ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelJoined);
        if (!chanNode) {
            // Pass contextServer's custom status flag down into the child channel item node
            ChannelTreeItem* newChanNode = new ChannelTreeItem(channelJoined.String(), 
                                                               contextServer->GetIndex(), 
                                                               contextServer->IsCustom());

            fChannelTree->AddUnder(newChanNode, contextServer); 
            fChannelTree->Expand(contextServer);
            
            // --- Explicitly force trailing newline layout alignment consistent with Custom View Rules ---
            LogToItemBuffer(newChanNode, BString("--- Joined channel ") << channelJoined << "\n");
            fChannelUsers[newChanNode] = new BObjectList<UserListItem, true>(20);
            
            // --- Automatically pivot active channel workspace to the newly joined channel room ---
            fActiveBufferItem = newChanNode;
            if (fCustomChatLog != nullptr) {
                fCustomChatLog->SetActiveChannel(fActiveBufferItem);
            }
            
            // Clear out user list panel right away to prepare for the room's NAMES dump payload
            while (fUserList->CountItems() > 0) {
                delete fUserList->RemoveItem((int32)0);
            }
            fTopicView->SetText(BString("Joined ") << channelJoined << ". Awaiting topic payload...");
            
            // === DRY REFACTOR: Simplified socket lookup call ===
            BSecureSocket* activeSocket = GetActiveSocket(contextServer);

            if (activeSocket != nullptr) {
                BString syncWho;
                syncWho << "WHO " << channelJoined << "\r\n";
                activeSocket->Write(syncWho.String(), syncWho.Length());
            }
            
        } else {
            // Someone else joined a channel we are already in!
            if (fChannelUsers[chanNode] != nullptr) {
                bool userExists = false;
                for (int32 i = 0; i < fChannelUsers[chanNode]->CountItems(); i++) {
                    BString itemTxt = fChannelUsers[chanNode]->ItemAt(i)->Text();
                    // Strip prefix for comparison matching
                    if (itemTxt.StartsWith("@") || itemTxt.StartsWith("+")) itemTxt.Remove(0, 1);
                    if (itemTxt == userWhoJoined) { userExists = true; break; }
                }
                
                if (!userExists) {
                    fChannelUsers[chanNode]->AddItem(new UserListItem(userWhoJoined.String(), false));
                    
                    // Read status visibility suppression from the explicitly scoped node
                    bool hideStatusOnThisServer = contextServer->IsHideStatus();
                    
                    if (!hideStatusOnThisServer) {
                        LogToItemBuffer(chanNode, BString("--> ") << userWhoJoined << " has joined " << channelJoined << "\n");
                    }
                    
                    if (fActiveBufferItem == chanNode) {
                        RefreshUserListUI();
                    }

                    // ==================== LIVE AUTO-OP INTERCEPTOR ====================
                    // Scan our running tracker list to see if this user has an active timed op window
                    bool shouldAutoOp = false;
                    for (int32 i = 0; i < fAutoOpList.CountItems(); i++) {
                        BString* storedNick = fAutoOpList.ItemAt(i);
                        if (storedNick != nullptr && *storedNick == userWhoJoined) {
                            shouldAutoOp = true;
                            break;
                        }
                    }

                    // If a match is found, immediately issue the network command to restore their Op
                    if (shouldAutoOp) {
                        BSecureSocket* activeSocket = GetActiveSocket(contextServer);
                        if (activeSocket != nullptr) {
                            BString autoOpPayload;
                            autoOpPayload << "MODE " << channelJoined << " +o " << userWhoJoined << "\r\n";
                            activeSocket->Write(autoOpPayload.String(), autoOpPayload.Length());
                        }
                    }
                    // ==================================================================
                }
            }
        }
        return;
    }



        // ==================== HARVEST BAN DATA (367 RPL_BANLIST) ====================
        if (command == "367") {
            // Reconstruct or read the parameters space from 'line'
            BString fullParams = line;
            fullParams.Trim();

            // Expected format inside line parameter: "YourNick #channel mask kicker timestamp"
            // Let's break it down word-by-word safely by splitting into an array or stepping spaces
            int32 firstSpace = fullParams.FindFirst(" ");  // End of YourNick
            if (firstSpace != B_ERROR) {
                BString afterNick = fullParams.String() + firstSpace + 1;
                afterNick.Trim();

                int32 secondSpace = afterNick.FindFirst(" "); // End of #channel
                if (secondSpace != B_ERROR) {
                    BString maskPart = afterNick.String() + secondSpace + 1;
                    maskPart.Trim();

                    // Isolate just the mask word token before kicker and timestamp spaces
                    int32 thirdSpace = maskPart.FindFirst(" ");
                    if (thirdSpace != B_ERROR) {
                        maskPart.Truncate(thirdSpace);
                    }
                    maskPart.Trim();

                    // Clean up any rogue formatting or line-stuffing parameters safely
                    maskPart.ReplaceAll("\r", "");
                    maskPart.ReplaceAll("\n", "");

                    if (maskPart.Length() > 0) {
                        // Add the verified clean mask string directly into our harvest list array
                        fCurrentBanHarvest.AddItem(new BString(maskPart));
                    }
                }
            }
            return;
        }

        // ==================== GENERATE DIALOG (368 RPL_ENDOFBANLIST) ====================
        if (command == "368") {
            BString targetChannel = line;
            targetChannel.Trim();
            
            // Expected format: "YourNick #channel :End of Channel Ban List"
            int32 firstSpace = targetChannel.FindFirst(" ");
            if (firstSpace != B_ERROR) {
                BString afterNick = targetChannel.String() + firstSpace + 1;
                afterNick.Trim();

                int32 secondSpace = afterNick.FindFirst(" ");
                if (secondSpace != B_ERROR) {
                    afterNick.Truncate(secondSpace);
                }
                targetChannel = afterNick;
            }
            targetChannel.Trim();

            // Safely initialize the window layout dashboard now that data collection is finished
            DisplayBanListDialog(targetChannel);
            return;
        }





 
 
        // Live AWAY Handler: Catches other users changing their status in real-time (Requires CAP REQ)
        if (command == "AWAY") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            BString userWhoChanged = prefix;
            int32 exclamIdx = userWhoChanged.FindFirst("!");
            if (exclamIdx != B_ERROR) {
                userWhoChanged.Truncate(exclamIdx);
            }
            userWhoChanged.Trim();

            // If trailing parameter has length, they went away. If empty, they returned.
            bool isNowAway = (trailing.Length() > 0);

            bool uiRefreshNeeded = false;
            
            // Loop through all active channel buffers
            for (auto it = fChannelUsers.begin(); it != fChannelUsers.end(); ++it) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(it->first);
                if (chanNode != nullptr) {
                    
                    // FIX: Isolate the parent item safely
                    BListItem* superItem = fChannelTree->Superitem(chanNode);
                    
                    // If superItem is null, this is a Private Query PM tab, not a channel room!
                    // If it is a PM tab, verify it matches the user changing status
                    if (superItem == nullptr) {
                        if (BString(chanNode->Text()) == userWhoChanged) {
                            // Update PM away indicators here if your custom draw engine supports it
                            if (fActiveBufferItem == chanNode) uiRefreshNeeded = true;
                        }
                        continue; // Skip the channel-specific network matching loops safely
                    }

                    // Standard validation guard for legitimate channel rooms
                    if (superItem != contextServer) continue;

                    BObjectList<UserListItem, true>* userVector = it->second;
                    if (userVector != nullptr) {
                        for (int32 i = 0; i < userVector->CountItems(); i++) {
                            UserListItem* uiUser = userVector->ItemAt(i);
                            if (uiUser != nullptr) {
                                BString cleanName = GetCleanNickname(uiUser->Text());
                                if (cleanName == userWhoChanged) {
                                    UpdateUserAwayState(chanNode, userWhoChanged.String(), isNowAway);
                                    
                                    if (fActiveBufferItem == chanNode) {
                                        uiRefreshNeeded = true;
                                    }
                                    break; 
                                }
                            }
                        }
                    }
                }
            }

            if (uiRefreshNeeded) {
                RefreshUserListUI();
            }
            return;
        }


   
   
    

        // RPL_UNAWAY (305): Server confirms you are no longer marked away
        if (command == "305") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }
            
            BStringItem* serverLog = FindServerLogNode(contextServer);
            BString notice = "--- Server Notification: You are no longer marked as being away.\n";
            LogToItemBuffer(serverLog ? serverLog : fActiveBufferItem, notice);
            
            // DRY helper updates your state across all channels on this server instantly
            UpdateMyGlobalAwayState(contextServer, false); // false = active/bright
            return;
        }

        // RPL_NOWAWAY (306): Server confirms you are now successfully marked away
        if (command == "306") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }
    
            BStringItem* serverLog = FindServerLogNode(contextServer);
            BString notice;
            notice << "--- Server Notification: You are now marked as away (" << cfg.awayMessage.c_str() << ").\n";
            LogToItemBuffer(serverLog ? serverLog : fActiveBufferItem, notice);
    
            // DRY helper updates your state across all channels on this server instantly
            UpdateMyGlobalAwayState(contextServer, true); // true = away/dimmed
            return;
        }


        

         // ERR_NICKNAMEINUSE: Handshake registration collision router
        if (command == "433") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }
            
            // --- Safe map lookup to prevent accidental dirty null-key insertion ---
            BSecureSocket* activeSocket = nullptr;
            if (fServerSockets.count(contextServer) > 0) {
                activeSocket = fServerSockets[contextServer];
            } else {
                activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
            }

            if (activeSocket != nullptr) {
                // Keep network-isolated retry counts decoupled per server context instance
                fNickAttempts[contextServer]++;
                int32 attempt = fNickAttempts[contextServer];

                BString fallbackNick;
                if (attempt == 1) {
                    fallbackNick = contextServer->GetAltNick();
                } else if (attempt == 2) {
                    fallbackNick = contextServer->GetAltNick2();
                } else {
                    uint32 randomSeed = static_cast<uint32>(real_time_clock_usecs() & 0xFFFF);
                    fallbackNick.SetToFormat("%s%u", contextServer->GetNick().String(), (randomSeed % 90) + 10);
                }

                fallbackNick.ReplaceAll(" ", "");
                
                // --- Tentatively cache assigned name so your outgoing filters align ---
                // (This can be finalized later when the server replies with an official NICK confirmation acknowledgement)
                fMyNick = fallbackNick;

                BString nickCommand;
                nickCommand << "NICK " << fallbackNick << "\r\n";
                activeSocket->Write(nickCommand.String(), nickCommand.Length());

                BString itemNotice;
                itemNotice << "--- Nickname in use! Trying alternate identifier: " << fallbackNick << "\n";
                
                // Route message safely into the correct targeted network status server log node
                BStringItem* serverLogNode = FindServerLogNode(contextServer);
                if (serverLogNode != nullptr) {
                    LogToItemBuffer(serverLogNode, itemNotice);
                } else {
                    LogToItemBuffer(fActiveBufferItem, itemNotice);
                }
            }
            return;
        }


        // RPL_WHOREPLY: Processes bulk channel user attributes (Bypasses trailing chunk limitations)
        if (command == "352") {
            std::vector<BString> tokens;
            int32 currentPos = 0;
            int32 nextSpace;
            
            // Build the data array using the argument data populated inside 'line'
            while ((nextSpace = line.FindFirst(" ", currentPos)) != B_ERROR) {
                BString t;
                line.CopyInto(t, currentPos, nextSpace - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
                currentPos = nextSpace + 1;
            }
            if (currentPos < line.Length()) {
                BString t;
                line.CopyInto(t, currentPos, line.Length() - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
            }

            // Ensure we have a complete 352 standard argument footprint (Indices 0 through 6)
            if (tokens.size() >= 7) {
                BString channelTarget  = tokens[1]; // Index 1: #ablyss-test
                BString targetUserNick = tokens[5]; // Index 5: ablyss_
                BString flags          = tokens[6]; // Index 6: G or H

                channelTarget.Trim();
                targetUserNick.Trim();
                flags.Trim();

                ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelTarget);
                if (chanNode != nullptr) {
                    // Check if the flags token contains 'G' for Gone / Away
                    bool isAway = (flags.FindFirst("G") != B_ERROR);
                    
                    // Trigger our updated, prefix-safe away status setter function
                    UpdateUserAwayState(chanNode, targetUserNick.String(), isAway);
                }
            }
            return;
        }






        // RPL_AWAY: Triggered when trying to ping or query an away target user
        if (command == "301") {
            // 1. Declare and build the tokens vector explicitly inside this scope
            std::vector<BString> tokens;
            int32 currentPos = 0;
            int32 nextSpace;
            
            while ((nextSpace = trailing.FindFirst(" ", currentPos)) != B_ERROR) {
                BString t;
                trailing.CopyInto(t, currentPos, nextSpace - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
                currentPos = nextSpace + 1;
            }
            if (currentPos < trailing.Length()) {
                BString t;
                trailing.CopyInto(t, currentPos, trailing.Length() - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
            }

            // 2. Safely verify that we found our tokens
            if (tokens.size() >= 2) {
                BString awayUserNick = tokens[1]; // Index 0 is your nick, 1 is their nick

                // 3. Step through your channels to apply the faded visual state safely
                for (auto it = fChannelUsers.begin(); it != fChannelUsers.end(); ++it) {
                    ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(it->first);
                    if (chanNode != nullptr) {
                        UpdateUserAwayState(chanNode, awayUserNick.String(), true);
                    }
                }
            }
            return;
        }




        // RPL_WELCOME: Network handshake authentication completed successfully
        if (command == "001") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }
            
            // 1. Reset nickname collision attempt metrics for this server context instance
            fNickAttempts[contextServer] = 0;

            // 2. --- Parse the official nickname confirmed by the remote server ---
            // In IRC protocol specifications, the first parameter after the '001' command 
            // is always the definitive nick assigned to your socket connection.
            BString officialNick = "";
            int32 firstParamSpace = line.FindFirst(" ");
            if (firstParamSpace != B_ERROR) {
                // If a prefix or target address shifts layout offsets, grab token safely
                BString remainingParams = line;
                remainingParams.Remove(0, firstParamSpace + 1);
                int32 nextParamSpace = remainingParams.FindFirst(" ");
                if (nextParamSpace != B_ERROR) {
                    remainingParams.CopyInto(officialNick, 0, nextParamSpace);
                } else {
                    officialNick = remainingParams;
                }
                officialNick.Trim();
                
                if (officialNick.Length() > 0 && officialNick != "*") {
                    fMyNick = officialNick; // Finalize local identity sync tracking
                    if (cfg.debugEnable) {
                        printf("[DEBUG] 001 Welcome confirmation: Finalized local nick as '%s'\n", fMyNick.String());
                    }
                }
            }

            // 3. FETCH THE ACTIVE NETWORK PIPELINE STREAM SOCKET SAFELY
            BSecureSocket* activeSocket = nullptr;
            if (fServerSockets.count(contextServer) > 0) {
                activeSocket = fServerSockets[contextServer];
            } else {
                activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
            }

            // Automatically loop and send JOIN commands ONLY after the server welcomes us!
            if (activeSocket != nullptr) {
                std::vector<std::string> targetAutoJoin;
                bool foundServerInConfig = false;

                // --- Map autojoin lists by Server Name/Address matching rather than volatile indices ---
                BString contextServerAddr = contextServer->Text(); // Assuming .Text() returns address or we use a getter
                
                // Determine whether this is a custom or standard server to pull the correct vector array
                if (contextServer->IsCustom()) {
                    // --- Rely on safe vector index boundary verification ---
                    size_t srvIdx = contextServer->GetIndex();
                    if (srvIdx < cfg.customServers.size()) {
                        targetAutoJoin = cfg.customServers[srvIdx].autojoin;
                        foundServerInConfig = true;
                    }
                } else {
                    size_t srvIdx = contextServer->GetIndex();
                    if (srvIdx < cfg.servers.size()) {
                        targetAutoJoin = cfg.servers[srvIdx].autojoin;
                        foundServerInConfig = true;
                    }
                }


                // Process the autojoin channels safely after welcome validation
                if (foundServerInConfig) {
                    for (const auto& chan : targetAutoJoin) {
                        if (chan.empty()) continue;
                        
                        BString joinCommand;
                        joinCommand << "JOIN " << chan.c_str() << "\r\n";
                        activeSocket->Write(joinCommand.String(), joinCommand.Length());
                        
                        if (cfg.debugEnable) {
                            LogDebugStream(contextServer->Text(), "OUTGOING", joinCommand.String(), joinCommand.Length());
                        }
                    }
                }
            }
            
            // Route structural confirmation status messages down into the correct network view log node
            BStringItem* serverLog = FindServerLogNode(contextServer);
            if (serverLog != nullptr) {
                LogToItemBuffer(serverLog, "--- Connection fully established with network.\n");
            } else {
                LogToItemBuffer(fActiveBufferItem, "--- Connection fully established with network.\n");
            }
        }





        // RPL_LIST: Individual channel description entry token row
        if (command == "322") { 
            // Server data line parameter syntax: <YourNick> <#Channel> <UserCount> :[Topic Text]
            BString channelName = "";
            BString userCount = "";
            
            int32 firstSpace = line.FindFirst(" ");
            if (firstSpace != B_ERROR) {
                BString argsBlock = line;
                argsBlock.Remove(0, firstSpace + 1); // Strip nickname
                
                int32 secondSpace = argsBlock.FindFirst(" ");
                if (secondSpace != B_ERROR) {
                    argsBlock.CopyInto(channelName, 0, secondSpace);
                    
                    BString countBlock = argsBlock;
                    countBlock.Remove(0, secondSpace + 1);
                    int32 thirdSpace = countBlock.FindFirst(" ");
                    if (thirdSpace != B_ERROR) {
                        countBlock.CopyInto(userCount, 0, thirdSpace);
                    } else {
                        userCount = countBlock;
                    }
                }
            }
            
            channelName.Trim();
            userCount.Trim();

            if (channelName.Length() == 0) return;

            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // --- 1. LOCAL CACHING FIX ---
            // We copy the pointer to a local variable. If the window thread nullifies 
            // fActiveListWindow mid-block, our 'targetWindow' remains a valid address 
            // for the duration of this function's scope.
            IRCChannelListWindow* targetWindow = fActiveListWindow;

            if (targetWindow != nullptr) {
                
                BSecureSocket* activeNetworkSocket = nullptr;
                if (fServerSockets.count(contextServer) > 0) {
                    activeNetworkSocket = fServerSockets[contextServer];
                } else {
                    activeNetworkSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }

                bool targetMatches = false;
                
                // --- 2. SAFE LOCKING ---
                // In Haiku, if the window is being destroyed, Lock() will return false.
                // This is our primary defense against using a "dead" object.
                if (targetWindow->Lock()) {
                    if (targetWindow->GetTargetSocket() == activeNetworkSocket) {
                        targetMatches = true;
                    }
                    targetWindow->Unlock();
                }

                // --- 3. ASYNCHRONOUS POSTING ---
                if (targetMatches) {
                    BMessage* rowPackage = new BMessage(MSG_ADD_LIST_ROW);
                    rowPackage->AddString("channel", channelName.String());
                    rowPackage->AddString("users", userCount.String());
                    rowPackage->AddString("topic", trailing.String()); 
                    
                    // PostMessage is thread-safe and won't crash even if the 
                    // target thread dies immediately after this call.
                    targetWindow->PostMessage(rowPackage);
                }
            }
            return;
        }








        // Live MODE & KICK Handlers (Tokenizer-Integrated Block)
        if (command == "MODE" || command == "KICK") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // Extract the channel target from the line parameter
            BString targetChannel = line;
            targetChannel.Trim();
            
            // Isolate the first token (the channel name) from the line parameter
            BString remainderParams = "";
            int32 firstSpace = targetChannel.FindFirst(" ");
            if (firstSpace != B_ERROR) {
                targetChannel.CopyInto(remainderParams, firstSpace + 1, targetChannel.Length() - firstSpace - 1);
                targetChannel.Truncate(firstSpace);
            }
            targetChannel.Trim();
            remainderParams.Trim();

            // Loop through the channel tree structure to locate the targeted channel room
            int32 totalTreeItems = fChannelTree->CountItems();
            for (int32 c = 0; c < totalTreeItems; c++) {
                BListItem* baseItem = fChannelTree->ItemAt(c);
                if (baseItem == nullptr || fChannelTree->Superitem(baseItem) != contextServer) continue;

                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(baseItem);
                if (!chanNode || BString(chanNode->Text()) != targetChannel) continue;

                BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                if (userVector == nullptr) continue;

                // ==================== SUB-BRANCH A: MODE PROCESSING ====================
                if (command == "MODE") {
                    // Flags and target now sit cleanly inside remainderParams: "+o HaikuUser"
                    int32 flagSpace = remainderParams.FindFirst(" ");
                    if (flagSpace != B_ERROR) {
                        BString flagString;
                        remainderParams.CopyInto(flagString, 0, flagSpace);
                        
                        BString targetNick = remainderParams.String() + flagSpace + 1;
                        targetNick.ReplaceAll("\r", "");
                        targetNick.ReplaceAll("\n", "");
                        targetNick.Trim();

                        bool adding = flagString.StartsWith("+");
                        BString symbol = "";
                        if (flagString.FindFirst("o") != B_ERROR) symbol = "@";
                        if (flagString.FindFirst("v") != B_ERROR) symbol = "+";

                        if (symbol.Length() > 0) {
                            bool itemWasUpdated = false;

                            for (int32 i = 0; i < userVector->CountItems(); i++) {
                                UserListItem* uiUser = userVector->ItemAt(i);
                                if (uiUser == nullptr) continue;

                                BString cleanName = GetCleanNickname(uiUser->Text());
                                if (cleanName == targetNick) {
                                    if (adding) {
                                        uiUser->SetText((BString(symbol) << cleanName).String());
                                    } else {
                                        uiUser->SetText(cleanName.String());
                                    }
                                    itemWasUpdated = true;
                                    break;
                                }
                            }

                            // If we modified an item in the active channel, force the UI to repaint
                            if (itemWasUpdated && fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                            }
                        }
                    }
                }
                // ==================== SUB-BRANCH B: KICK PROCESSING ====================
                else if (command == "KICK") {
                    BString kicker = prefix;
                    int32 exclamIdx = kicker.FindFirst("!");
                    if (exclamIdx != B_ERROR) kicker.Truncate(exclamIdx);

                    // Remainder contains: "TargetNick :Reason"
                    BString targetNick = remainderParams;
                    BString kickReason = trailing; // If tokenizer puts reason in trailing
                    
                    int32 reasonColon = targetNick.FindFirst(":");
                    if (reasonColon != B_ERROR) {
                        kickReason = targetNick.String() + reasonColon + 1;
                        targetNick.Truncate(reasonColon);
                    }
                    targetNick.Trim();
                    kickReason.ReplaceAll("\r", "");
                    kickReason.ReplaceAll("\n", "");
                    kickReason.Trim();

                    for (int32 i = userVector->CountItems() - 1; i >= 0; i--) {
                        UserListItem* uiUser = userVector->ItemAt(i);
                        if (uiUser == nullptr) continue;

                        if (GetCleanNickname(uiUser->Text()) == targetNick) {
                            UserListItem* removedUser = userVector->RemoveItemAt(i);
                            delete removedUser;

                            BString logLine;
                            logLine << "<-- " << targetNick << " was kicked by " << kicker;
                            if (kickReason.Length() > 0) {
                                logLine << " (" << kickReason << ")\n";
                            } else {
                                logLine << "\n";
                            }
                            LogToItemBuffer(chanNode, logLine);

                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                            }
                            break;
                        }
                    }
                }
                break; // Break tree traversal loop cleanly once room resolves
            }
            return;
        }








            // Live PART & QUIT Handlers (Safely removes users dynamically when they depart or disconnect)
        if (command == "PART" || command == "QUIT") {
            BString userWhoLeft = prefix;
            int32 exclamIdx = userWhoLeft.FindFirst("!");
            if (exclamIdx != B_ERROR) userWhoLeft.Truncate(exclamIdx);

            BString targetChannel = line;
            targetChannel.Trim();
            targetChannel.ReplaceAll(" ", "");

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // Safe Haiku BOutlineListView Sub-Item Traversal Loop
            int32 totalTreeItems = fChannelTree->CountItems();
            for (int32 c = 0; c < totalTreeItems; c++) {
                BListItem* baseItem = fChannelTree->ItemAt(c);
                if (baseItem == nullptr) continue;

                // Ensure this item is nested directly under our targeted server connection node
                if (fChannelTree->Superitem(baseItem) != contextServer) continue;

                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(baseItem);
                if (!chanNode) continue;
                
                // If it's a room-specific PART, skip loops on non-matching channel strings
                if (command == "PART" && BString(chanNode->Text()) != targetChannel) continue;

                // If it's a global QUIT, the user leaves *all* channels belonging to this server!
                if (fChannelUsers[chanNode] != nullptr) {
                    BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                    
                    // Iterate backward to prevent index shifting bugs when items are deleted
                    for (int32 i = userVector->CountItems() - 1; i >= 0; i--) {
                        BStringItem* userItem = userVector->ItemAt(i);
                        if (userItem == nullptr) continue;

                        BString itemTxt = userItem->Text();
                        
                        // Explicitly clean up the channel rank prefix before checking against the user string
                        if (itemTxt.StartsWith("@") || itemTxt.StartsWith("+")) itemTxt.Remove(0, 1);
                        
                        if (itemTxt == userWhoLeft) {
                            // Safely remove from the vector array and free heap memory pool allocations entirely
                            BStringItem* removedUser = userVector->RemoveItemAt(i);
                            delete removedUser;
                            
                            // Query the specific, thread-safe server context node for configuration properties
                            bool hideStatusOnThisServer = contextServer->IsHideStatus();
                            
                            if (!hideStatusOnThisServer) {
                                BString partNotice = (command == "PART") ? "has left" : "has quit";
                                BString logLine;
                                
                                // --- Replaced unsafe SetToFormat string operators with bulletproof appends ---
                                logLine << "<-- " << userWhoLeft << " " << partNotice;
                                if (trailing.Length() > 0) {
                                    logLine << " (" << trailing << ")\n";
                                } else {
                                    logLine << "\n";
                                }
                                
                                LogToItemBuffer(chanNode, logLine);
                            }
                            
                            // Immediately sync visual modules if the user departed the visible view frame
                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                            }
                            
                            // If it was a PART command, the user can only belong to one channel; break safely.
                            if (command == "PART") break;
                        }
                    }
                }
            }
            return;
        }


           // Catch Server Topic Event Numbers (332 = Topic Set, 331 = No Topic Set) or live /TOPIC updates
        if (command == "332" || command == "331" || command == "TOPIC") {
            BString targetChannel = "";
            
            if (command == "332" || command == "331") {
                BString numericArgs = line;
                int32 chSpace = numericArgs.FindFirst(" ");
                if (chSpace != B_ERROR) numericArgs.Remove(0, chSpace + 1);
                int32 nextSpace = numericArgs.FindFirst(" ");
                if (nextSpace != B_ERROR) numericArgs.Truncate(nextSpace);
                targetChannel = numericArgs;
            } else {
                // Live /TOPIC command looks like: <#Channel>
                BString liveArgs = line;
                int32 cmdSpace = liveArgs.FindFirst(" ");
                if (cmdSpace != B_ERROR) liveArgs.Truncate(cmdSpace);
                targetChannel = liveArgs;
            }
            
            targetChannel.Trim();
            targetChannel.ReplaceAll(" ", "");

            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            ChannelTreeItem* chanNode = FindChannelNode(contextServer, targetChannel);
            if (chanNode != nullptr) {
                BString dynamicTopic = (command == "331") ? "No topic set." : trailing;
                
                dynamicTopic.Trim();
                dynamicTopic.ReplaceAll("\r", "");
                dynamicTopic.ReplaceAll("\n", "");
                
                // === HISTORY BACKLOG PLAYBACK PROTECTION BARRIER ===
                // If we are currently replaying old lines from our log history text files,
                // do NOT allow it to overwrite the live topic string currently active in memory!
                if (fIsLoadingHistory && fChannelTopics.find(chanNode) != fChannelTopics.end()) {
                    // Skip the memory overwrite pass entirely to protect our live topic!
                } else {
                    // Cache the clean live text payload into our memory map securely
                    fChannelTopics[chanNode] = dynamicTopic;
                    
                    // Force immediate live layout synchronization to the top bar
                    if (fActiveBufferItem == chanNode || (fActiveBufferItem != nullptr && fActiveBufferItem->Text() == targetChannel)) {
                        fTopicView->SetText(dynamicTopic.String());
                        fTopicView->InvalidateLayout();
                        fTopicView->Invalidate();
                    }
                }
                
                if (command == "332") {
                    LogToItemBuffer(chanNode, BString("--- Channel topic: ") << dynamicTopic << "\n");
                } else if (command == "TOPIC") {
                    BString changingUser = prefix;
                    int32 exclamIdx = changingUser.FindFirst("!");
                    if (exclamIdx != B_ERROR) changingUser.Truncate(exclamIdx);
                    
                    LogToItemBuffer(chanNode, BString("--- ") << changingUser << " has changed the topic to: " << dynamicTopic << "\n");
                } else {
                    LogToItemBuffer(chanNode, "--- No channel topic is set.\n");
                }
            }
            return;
        }




        // RPL_ENDOFWHO: Server indicates the complete bulk user data burst is finished
        if (command == "315") {
            // 'line' holds: "<YourNick> <#Channel>"
            std::vector<BString> tokens;
            int32 currentPos = 0;
            int32 nextSpace;
            
            while ((nextSpace = line.FindFirst(" ", currentPos)) != B_ERROR) {
                BString t;
                line.CopyInto(t, currentPos, nextSpace - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
                currentPos = nextSpace + 1;
            }
            if (currentPos < line.Length()) {
                BString t;
                line.CopyInto(t, currentPos, line.Length() - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
            }

            if (tokens.size() >= 2) {
                BString channelTarget = tokens[1]; // Index 1 isolates the room name
                channelTarget.Trim();

                ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelTarget);
                
                
                // BATCH EXECUTION: If this is the active tab frame, refresh the screen just ONCE!
                if (chanNode != nullptr && fActiveBufferItem == chanNode) {
                    RefreshUserListUI();
                }
                
            }
            return;
        }




           // RPL_NAMREPLY: Active channel user list payload burst
        if (command == "353") { 
            // Raw argument string syntax before the colon separator looks like:
            // "<YourNick> <Type: = or * or @> <#Channel>"
            // Since our main loop stripped prefixes, 'line' holds exactly this block.
            
            BString targetChannel = "";
            int32 lastSpace = line.FindLast(" ");
            if (lastSpace != B_ERROR) {
                line.CopyInto(targetChannel, lastSpace + 1, line.Length() - (lastSpace + 1));
            } else {
                targetChannel = line;
            }
            targetChannel.Trim();
            targetChannel.ReplaceAll(" ", "");

            if (targetChannel.Length() == 0) return;

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // Explicitly cast to ChannelTreeItem to access specialized rank properties if present
            ChannelTreeItem* chanNode = FindChannelNode(contextServer, targetChannel);
            if (chanNode != nullptr && fChannelUsers[chanNode] != nullptr) {
                BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                
                // --- Isolate tokenization using a safe character loop to stop infinite freezes ---
                BString namesBuffer = trailing;
                namesBuffer.Trim(); // Strip trailing/leading space configurations
                
                int32 searchStart = 0;
                while (searchStart < namesBuffer.Length()) {
                    int32 nextSpace = namesBuffer.FindFirst(" ", searchStart);
                    BString singleUser;
                    
                    if (nextSpace != B_ERROR) {
                        namesBuffer.CopyInto(singleUser, searchStart, nextSpace - searchStart);
                        searchStart = nextSpace + 1;
                    } else {
                        namesBuffer.CopyInto(singleUser, searchStart, namesBuffer.Length() - searchStart);
                        searchStart = namesBuffer.Length(); // Terminates the loop cleanly
                    }
                    
                    singleUser.Trim();
                    if (singleUser.Length() == 0) continue;

                    // --- Duplication Guard Pass ---
                    // Verify the name handle doesn't already occupy a slot in this channel vector array
                    bool duplicateFound = false;
                    for (int32 u = 0; u < userVector->CountItems(); u++) {
                        if (userVector->ItemAt(u) != nullptr && 
                            userVector->ItemAt(u)->Text() == singleUser) {
                            duplicateFound = true;
                            break;
                        }
                    }
						if (!duplicateFound) {
    						userVector->AddItem(new UserListItem(singleUser.String(), false));
						}

                }

                // If the user is actively viewing this tab frame, sync visual modules instantly
                if (fActiveBufferItem == chanNode) {
                    RefreshUserListUI();
                    
                    fUserList->SortItems(SortUsersByRank);
                    fUserList->Invalidate();
                }
            }
            return;
        }



         // Dynamic NICK Modification Handlers (Saves identity renames for Us and other network users)
        if (command == "NICK") {
            BString oldNick = prefix;
            int32 exclamIdx = oldNick.FindFirst("!");
            if (exclamIdx != B_ERROR) oldNick.Truncate(exclamIdx);

            BString newNick = trailing.Length() > 0 ? trailing : line;
            newNick.Trim();
            newNick.ReplaceAll(" ", "");

            if (newNick.Length() == 0) return;

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // Check nickname against the specific server's runtime tracker
            if (oldNick.ICompare(contextServer->GetNick()) == 0) {
                // Safely update memory vector state without rewriting your settings file ---
                // This updates your client's active tracking loop inline while preserving your raw files!
                size_t serverIdx = contextServer->GetIndex();
                if (contextServer->IsCustom() && serverIdx < cfg.customServers.size()) {
                    cfg.customServers[serverIdx].nick = newNick.String();
                } else if (!contextServer->IsCustom() && serverIdx < cfg.servers.size()) {
                    cfg.servers[serverIdx].nick = newNick.String();
                }
                
                BString itemNotice;
                itemNotice << "--- Your nickname on this server is now " << newNick << "\n";
                LogToItemBuffer(fActiveBufferItem, itemNotice);
                
                // Keep fMyNick sync'd for backwards compatibility if needed elsewhere
                fMyNick = newNick;
            }


            // Safe Haiku BOutlineListView Sub-Item Traversal Loop ---
            // Iterate flatly across all items and cross-validate parental association context
            int32 totalTreeItems = fChannelTree->CountItems();
            for (int32 c = 0; c < totalTreeItems; c++) {
                BListItem* baseItem = fChannelTree->ItemAt(c);
                if (baseItem == nullptr) continue;

                // Ensure this item is nested strictly under our targeted server connection node
                if (fChannelTree->Superitem(baseItem) != contextServer) continue;

                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(baseItem);
                if (!chanNode) continue;
                
                if (fChannelUsers[chanNode] != nullptr) {
                    BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                    
                    // Slicing from top to bottom is safe here because we preserve vector sizes via in-place updates
                    for (int32 i = 0; i < userVector->CountItems(); i++) {
                        BStringItem* userItem = userVector->ItemAt(i);
                        if (userItem == nullptr) continue;

                        BString currentEntry = userItem->Text();
                        BString modePrefix = "";
                        
                        if (currentEntry.StartsWith("@") || currentEntry.StartsWith("+")) {
                            currentEntry.CopyInto(modePrefix, 0, 1);
                            currentEntry.Remove(0, 1);
                        }

                        if (currentEntry == oldNick) {
                            BString updatedEntry;
                            updatedEntry << modePrefix << newNick;
                            
                            // Free old memory pool allocations and re-insert the updated string element
                            BStringItem* oldItem = userVector->RemoveItemAt(i);
                            delete oldItem;
                            
                            userVector->AddItem(new UserListItem(updatedEntry.String(), false), i);                      
                            
                            BString nickNotice;
                            nickNotice << "--- " << oldNick << " is now known as " << newNick << "\n";
                            LogToItemBuffer(chanNode, nickNotice);
                            
                            // Force the UI to refresh live if the nickname changed in the active channel view
                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                                fUserList->SortItems(SortUsersByRank);
                                fUserList->Invalidate();
                            }
                            break; // This user can only appear once per channel vector array
                        }
                    }
                }
            }
            return;
        }




        // PRIVMSG & NOTICE Message Routing Engine
        if (command == "PRIVMSG" || command == "NOTICE") {
            BString senderNick = prefix;
            int32 exclamIdx = senderNick.FindFirst("!");
            if (exclamIdx != B_ERROR) senderNick.Truncate(exclamIdx);            
            
            int32 msgTargetSpace = line.FindFirst(" ");
            BString targetRoom = line;
            if (msgTargetSpace != B_ERROR) targetRoom.Truncate(msgTargetSpace);
            targetRoom.ReplaceAll(" ", "");

            // Handle Automated CTCP Version Queries
            if (command == "PRIVMSG" && (trailing.StartsWith("\x01VERSION\x01") || trailing.StartsWith("\1VERSION\1"))) {
                BSecureSocket* activeSocket = nullptr;
                if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }

                if (activeSocket != nullptr) {
                    BString versionReply;
                    versionReply << "NOTICE " << senderNick << " :\x01VERSION " << AppInfo::VERSION_STRING << "\x01\r\n";                    
                    activeSocket->Write(versionReply.String(), versionReply.Length());
                    
                    BString logNotice;
                    logNotice << "--- [CTCP] Version query from " << senderNick << " answered automatically.\n";
                    LogToItemBuffer(FindServerLogNode(contextServer), logNotice);
                }
                return;
            }


            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // 1. Establish destination nodes supporting Channels AND Private Message Queries ---
            BStringItem* targetNode = nullptr;
            if (targetRoom.StartsWith("#")) {
                targetNode = FindChannelNode(contextServer, targetRoom);
            } else {
                // Look for an existing one-on-one query tab
                targetNode = FindChannelNode(contextServer, senderNick);
                
                // === AUTOMATION HOOK: Auto-create tab if someone pings us first ===
                if (targetNode == nullptr) {
                    ChannelTreeItem* newQueryNode = new ChannelTreeItem(senderNick.String(), 
                                                                        contextServer->GetIndex(), 
                                                                        contextServer->IsCustom());
                    
                    fChannelTree->AddUnder(newQueryNode, contextServer);
                    fChannelTree->Expand(contextServer);
                    
                    // Allocate an empty list container for their user panel cache matrix
                    fChannelUsers[newQueryNode] = new BObjectList<UserListItem, true>(20);
                    
                    // Log the opening initialization notice header
                    LogToItemBuffer(newQueryNode, BString("--- Incoming Private Conversation started with ") << senderNick << "\n");
                    
                    targetNode = newQueryNode;
                }
            }


            if (targetNode == nullptr) {
                targetNode = FindServerLogNode(contextServer);
            }

            // 2. Thread-Safe Timing Calculation Engine ---
            BString timestampPrefix = "";
            bigtime_t currentTime = real_time_clock_usecs(); // Native Haiku system clock            
            bigtime_t thirtyMinutesInUsecs = (bigtime_t)30 * 60 * 1000000;
            
            if (targetNode != nullptr) {
                bool needsTimestamp = false;
                if (fLastTimestampTime.count(targetNode) == 0) {
                    needsTimestamp = true;
                } else {
                    bigtime_t lastTime = fLastTimestampTime.find(targetNode)->second;
                    if ((currentTime - lastTime) >= thirtyMinutesInUsecs) {
                        needsTimestamp = true;
                    }
                }

                if (needsTimestamp) {                    
                    fLastTimestampTime[targetNode] = currentTime;
                    time_t rawTime = (time_t)(currentTime / 1000000);
                    struct tm* timeInfo = localtime(&rawTime);                    
                    if (timeInfo != nullptr) {
                        char timeBuffer[32];
                        strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M] ", timeInfo);
                        timestampPrefix = timeBuffer;
                    }
                }
            }

            // 3. Parse and Format Message Strings
            BString formattedMsg;
            formattedMsg << timestampPrefix;
            
            if (command == "PRIVMSG" && (trailing.StartsWith("\x01" "ACTION ") || trailing.StartsWith("\1ACTION "))) {                        
                trailing.Remove(0, 8);                
                if (trailing.EndsWith("\x01") || trailing.EndsWith("\1")) {
                    trailing.Truncate(trailing.Length() - 1);
                }                
                formattedMsg << "* " << senderNick << " " << trailing << "\n";
            } else {
                formattedMsg << "<" << senderNick << "> " << trailing << "\n";
            }

            // 4. AUTOMATION HOOK: Detect NickServ post-auth identification triggers
            if (senderNick.ICompare("NickServ") == 0 && trailing.IFindFirst("identify") != B_ERROR) {                
                BSecureSocket* activeSocket = nullptr;
                if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }                
                if (activeSocket != nullptr && contextServer != nullptr) {
                    BString savedPassword = contextServer->GetPass();
                    if (savedPassword.Length() > 0) {
                        BString autoIdentify;
                        autoIdentify << "PRIVMSG NickServ :IDENTIFY " << savedPassword << "\r\n";
                        activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                        
                        BString logNotice = "--- [Auto-Services] Identification credentials automatically sent to NickServ.\n";
                        LogToItemBuffer(FindServerLogNode(contextServer), logNotice);
                    }
                }
            }

            // 5. Route structured messages to their designated text buffers
            if (targetRoom.StartsWith("#")) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(targetNode);
                if (chanNode) {
                    
                    // === FIXED AUTOMATION HOOK: Only mark back if *I* am the one typing ===
                    if (senderNick == fMyNick) {
                        UpdateUserAwayState(chanNode, senderNick.String(), false);
                    }
                    // =====================================================================

                    if (fActiveBufferItem != chanNode) {
                        chanNode->SetUnread(true);
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(chanNode)); 
                    }
                    LogToItemBuffer(chanNode, formattedMsg);
                } else {
                    LogToItemBuffer(FindServerLogNode(contextServer), formattedMsg);
                }
            } else {
                // Route Private Query PM messages safely to their own tree nodes if available
                LogToItemBuffer(targetNode, formattedMsg);
            }
            return;

        }
     
        
        // Bracket-Sanitized Global Server Protocol Fallback Route ---
        // This catches generic server data lines (MOTD, Notices) outside of channels
        if (trailing.Length() > 0) {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // AUTOMATION HOOK FALLBACK: Intercept raw notices before registration fully establishes
            if (prefix.IFindFirst("NickServ") != B_ERROR && trailing.IFindFirst("identify") != B_ERROR) {                
                BSecureSocket* activeSocket = nullptr;
                if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }
                if (activeSocket != nullptr && contextServer != nullptr && contextServer->GetPass().Length() > 0) {
                    BString autoIdentify;
                    autoIdentify << "PRIVMSG NickServ :IDENTIFY " << contextServer->GetPass() << "\r\n";
                    activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                    
                    BString logNotice = "--- [Auto-Services] Pre-registration identification sent to NickServ.\n";
                    LogToItemBuffer(static_cast<BStringItem*>(contextServer), logNotice);
                }
            }

            if (contextServer != nullptr) {
                BString rawLog;
                rawLog << trailing << "\n";
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), rawLog);
            }
        }
    }



    static status_t NetworkLoop(void* data) {
        ServerTreeItem* targetNode = static_cast<ServerTreeItem*>(data);
        if (targetNode == nullptr) return B_ERROR;

        CricketWindow* window = dynamic_cast<CricketWindow*>(be_app->WindowAt(0));
        if (window == nullptr) return B_ERROR;

        BNetworkAddress address(targetNode->GetHost().String(), targetNode->GetPort());
        BSecureSocket* localSocket = new BSecureSocket();
        
        if (localSocket->Connect(address) != B_OK) {
            BMessage* reply = new BMessage(MSG_IRC_RECEIVED);
            reply->AddString("text", BString("Connection failed to ") << targetNode->GetHost() << "\n");
            reply->AddPointer("server_node", targetNode);
            window->PostMessage(reply);
            delete localSocket;
            return B_ERROR;
        }

        // Dynamically track socket connection instances using our tracker map
        window->Lock(); // Ensure thread-safe map insertion
        window->fServerSockets[targetNode] = localSocket;
        
        // Map legacy pointers to prevent breakages elsewhere 
        if (targetNode == window->fLiberaNode) window->fLiberaSocket = localSocket;
        if (targetNode == window->fOftcNode)   window->fOftcSocket = localSocket;
        window->Unlock();

        // 1. PASS MUST BE SENT FIRST PER IRC SPECIFICATION
        if (targetNode->GetPass().Length() > 0) {
            BString passHandshake;
            passHandshake << "PASS " << targetNode->GetPass() << "\r\n";
            localSocket->Write(passHandshake.String(), passHandshake.Length());
            if (cfg.debugEnable) {
                LogDebugStream(targetNode->Text(), "OUTGOING", passHandshake.String(), passHandshake.Length());
            }
        }

        // ==================== NEW: IRCv3 CAPABILITY NEGOTIATION ====================
        // Request the capability listing and register for away status events immediately
        BString capHandshake;
        capHandshake << "CAP LS 302\r\nCAP REQ :away-notify\r\n";
        localSocket->Write(capHandshake.String(), capHandshake.Length());
        if (cfg.debugEnable) {
            LogDebugStream(targetNode->Text(), "OUTGOING", capHandshake.String(), capHandshake.Length());
        }
        // ===========================================================================

        // 2. Dynamic Nickname Handshake Configuration
        BString nickHandshake;
        nickHandshake << "NICK " << targetNode->GetNick() << "\r\n";
        localSocket->Write(nickHandshake.String(), nickHandshake.Length());
        if (cfg.debugEnable) {
            LogDebugStream(targetNode->Text(), "OUTGOING", nickHandshake.String(), nickHandshake.Length());
        }
		
        // 3. Dynamic User Identity Handshake Configuration
        BString userHandshake;
        userHandshake << "USER haiku 0 * :" << targetNode->GetNick() << " Client\r\n";
        localSocket->Write(userHandshake.String(), userHandshake.Length());
        if (cfg.debugEnable) {
            LogDebugStream(targetNode->Text(), "OUTGOING", userHandshake.String(), userHandshake.Length());
        }

        // ==================== NEW: CLOSE CAPABILITY CAP WINDOW ====================
        // Complete the handshake pass so registration can finalize completely on the server
        BString capEndHandshake = "CAP END\r\n";
        localSocket->Write(capEndHandshake.String(), capEndHandshake.Length());
        if (cfg.debugEnable) {
            LogDebugStream(targetNode->Text(), "OUTGOING", capEndHandshake.String(), capEndHandshake.Length());
        }
        // ===========================================================================

        char buffer[512];
        ssize_t bytesRead;
        BString lineBuffer;
        
        while ((bytesRead = localSocket->Read(buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';           

            if (cfg.debugEnable) {
                LogDebugStream(targetNode->Text(), "INCOMING", buffer, bytesRead);
            }

            lineBuffer << buffer;

            int32 newlinePos;
            while ((newlinePos = lineBuffer.FindFirst("\n")) != B_ERROR) {
                BString line;
                lineBuffer.CopyInto(line, 0, newlinePos + 1);
                lineBuffer.Remove(0, newlinePos + 1);
                
                if (line.StartsWith("PING")) {
                    BString pong = line;
                    pong.Replace("PING", "PONG", 1);
                    localSocket->Write(pong.String(), pong.Length());
                    if (cfg.debugEnable) {
                        LogDebugStream(targetNode->Text(), "OUTGOING", pong.String(), pong.Length());
                    }
                    continue;
                }

                BMessage* reply = new BMessage(MSG_IRC_RECEIVED);
                reply->AddString("text", line.String());
                reply->AddPointer("server_node", targetNode);
                window->PostMessage(reply);
            }
        }

        // 4. Thread and Socket Cleanup State Reset
        bool triggerReconnect = targetNode->IsAutoReconnect();
        
        if (be_app->CountWindows() > 0 && be_app->WindowAt(0) == window) {
            if (window->Lock()) { 
                window->fServerSockets.erase(targetNode);
                window->fServerThreads.erase(targetNode);
                
                if (targetNode == window->fLiberaNode) {
                    window->fLiberaThread = -1;
                    window->fLiberaSocket = nullptr;
                } else if (targetNode == window->fOftcNode) {
                    window->fOftcThread = -1;
                    window->fOftcSocket = nullptr;
                }
                window->Unlock();
                
                if (triggerReconnect) {
                    BMessage* reconnectMessage = new BMessage(MSG_RECONNECT_SERVER);
                    reconnectMessage->AddPointer("server_item", targetNode);
                    window->PostMessage(reconnectMessage);
                }
            }
        }

        delete localSocket;
        return B_OK;
    }





public:
    void DispatchMessage(BMessage* message, BHandler* handler) override {
        // 1. LEFT-CLICK INTERCEPTOR: Catch clicks on WebPositive URL links inside fChatLog
        if (message->what == B_MOUSE_DOWN && handler == fChatLog) {
            int32 buttons;
            BPoint point;
            
            if (message->FindInt32("buttons", &buttons) == B_OK && buttons == B_PRIMARY_MOUSE_BUTTON) {
                message->FindPoint("be:view_where", &point);
                
                int32 textOffset = fChatLog->OffsetAt(point);
                if (textOffset >= 0) {
                    BString fullChatHistory(fChatLog->Text());
                    
                    int32 urlStart = fullChatHistory.IFindLast("http://", textOffset);
                    if (urlStart == B_ERROR) {
                        urlStart = fullChatHistory.IFindLast("https://", textOffset);
                    }
                    
                    if (urlStart != B_ERROR) {
                        int32 urlEnd = fullChatHistory.FindFirst(" ", urlStart);
                        int32 newlineCheck = fullChatHistory.FindFirst("\n", urlStart);
                        
                        if (urlEnd == B_ERROR || (newlineCheck != B_ERROR && newlineCheck < urlEnd)) {
                            urlEnd = newlineCheck;
                        }
                        if (urlEnd == B_ERROR) {
                            urlEnd = fullChatHistory.Length();
                        }

                        if (textOffset >= urlStart && textOffset < urlEnd) {
                            BString targetURL;
                            fullChatHistory.CopyInto(targetURL, urlStart, urlEnd - urlStart);
                            targetURL.ReplaceAll("\r", "");
                            targetURL.ReplaceAll("\n", "");
                            targetURL.Trim();

                            if (targetURL.Length() > 0) {
                                const char* args[] = { targetURL.String(), nullptr };
                                
                                if (be_roster->Launch("application/x-vnd.WebPositive", 1, const_cast<char**>(args)) != B_OK) {
                                    be_roster->Launch("text/html", 1, const_cast<char**>(args));
                                }
                                return; // Stop early so text doesn't highlight
                            }
                        }
                    }
                }
            }
        }
        
        // 2. RIGHT-CLICK INTERCEPTOR: Triggers Context Menus on the Sidebar Tree (fChannelTree)
        if (message->what == B_MOUSE_DOWN && handler == fChannelTree) {
            int32 buttons;
            if (message->FindInt32("buttons", &buttons) == B_OK && buttons == B_SECONDARY_MOUSE_BUTTON) {
                BPoint point;
                message->FindPoint("be:view_where", &point);                
                int32 index = fChannelTree->IndexOf(point);                 
                if (index >= 0) {
                    fChannelTree->Select(index);
                    BListItem* generalItem = fChannelTree->ItemAt(index);
                    
                    if (generalItem != nullptr) {
                        ServerTreeItem* serverItem = dynamic_cast<ServerTreeItem*>(generalItem);
                        if (serverItem != nullptr) {
                            ShowContextMenu(fChannelTree->ConvertToScreen(point), serverItem);
                            return; 
                        }
                        
                        ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(generalItem);
                        if (chanItem != nullptr) {
                            ShowContextMenu(fChannelTree->ConvertToScreen(point), chanItem);
                            return; 
                        }
                    }
                }
            }
        }


        
        // Pass everything else to the default handler loop
        BWindow::DispatchMessage(message, handler);
    }




	//@messagereceived
    void MessageReceived(BMessage* message) override {
        switch (message->what) {


        case 'tgem': {
            // 1. Recover the pointer to the target server item
            ServerTreeItem* srvItem = nullptr;
            if (message->FindPointer("server_item", (void**)&srvItem) == B_OK && srvItem != nullptr) {
                
                // 2. Locate the matching server struct inside your config mapping
                BString currentServerName(srvItem->Text());
                bool targetState = false;
                bool serverFound = false;
                
                // Search primary servers list
                for (auto& srv : cfg.servers) {
                    if (BString(srv.name.c_str()) == currentServerName) {
                        srv.enableEmoticons = !srv.enableEmoticons;
                        targetState = srv.enableEmoticons;
                        serverFound = true;
                        break;
                    }
                }

                // Fallback to custom servers list if not found in primary list
                if (!serverFound) {
                    for (auto& srv : cfg.customServers) {
                        if (BString(srv.name.c_str()) == currentServerName) {
                            srv.enableEmoticons = !srv.enableEmoticons;
                            targetState = srv.enableEmoticons;
                            break;
                        }
                    }
                }
                
                // 3. CRITICAL: Save the modified configuration to disk immediately!
                save_config();
                
                // 4. Update the source menu item's visual checkmark state dynamically
                void* sourcePtr = nullptr;
                if (message->FindPointer("source", &sourcePtr) == B_OK) {
                    BMenuItem* clickedItem = static_cast<BMenuItem*>(sourcePtr);
                    if (clickedItem != nullptr) {
                        clickedItem->SetMarked(targetState);
                    }
                }
                
                // 5. Force UI/Toolbar layout sync if editing the active window view context
                if (fCurrentServerNode == srvItem) {
                    if (fIconToggleButton != nullptr) {
                        if (targetState) {
                            fIconToggleButton->Show();
                        } else {
                            fIconToggleButton->Hide();
                        }
                    }
                    
                    if (fCustomChatLog != nullptr) {
                        fCustomChatLog->Invalidate();
                    }
                    
                    this->InvalidateLayout(true);
                }
            }
            break;
        }




        // POPUP LIFECYCLE MONITOR: Resets tracking pointer cleanly whenever the popup closes
        case MSG_POPUP_WAS_DESTROYED: {
            fActiveIconPopup = nullptr;
            break;
        }



        // ICON GRID MANAGER LAUNCHER: Toggles the custom borderless icon canvas on and off
        case MSG_TOGGLE_ICON_POPUP: {
            if (fIconToggleButton != nullptr) {
                // 1. FIXED TOGGLE: If the window is already active, close it and break out
                if (fActiveIconPopup != nullptr) {
                    fActiveIconPopup->PostMessage(B_QUIT_REQUESTED);
                    fActiveIconPopup = nullptr;
                    break;
                }

                // 2. Safe layout geometry extraction relative to our parent window context
                BRect buttonFrame = fIconToggleButton->Frame();
                BPoint buttonWindowPos(buttonFrame.left, buttonFrame.top);
                BPoint screenCoordinate = ConvertToScreen(buttonWindowPos);
                
                // 3. FIX: Spin up the popup passing ONLY the messenger target thread handle.
                // We completely remove any reference to fEmoticonGrid to bypass pointer corruption crashes!
                EmotePopupWindow* popWin = new EmotePopupWindow(screenCoordinate, BMessenger(this));
                
                // 4. Update our active class reference tracker
                fActiveIconPopup = popWin;
                popWin->Show();
            }
            break;
        }





        // EMOTICON SELECTION HANDLER: Inserts text smileys into the typing control field
        case 'emcl': {
            void* popWinPtr = nullptr;
            if (message->FindPointer("popup_window_ref", &popWinPtr) == B_OK && popWinPtr != nullptr) {
                BWindow* popupWindow = static_cast<BWindow*>(popWinPtr);
                popupWindow->PostMessage(B_QUIT_REQUESTED);
                fActiveIconPopup = nullptr;
            }

            BString emoteTrigger;
            if (message->FindString("trigger", &emoteTrigger) == B_OK) {
                BTextView* inputTextView = fInputControl->TextView();
                if (inputTextView != nullptr) {
                    fInputControl->MakeFocus(true);
                    
                    BString insertionText = emoteTrigger;
                    insertionText << " ";
                    
                    // FIX: Safe Haiku cursor tracking using the standard GetSelection method parameters
                    int32 startPos = 0;
                    int32 endPos = 0;
                    inputTextView->GetSelection(&startPos, &endPos);
                    
                    // Insert the emoticon code bytes straight into the active cursor point location
                    inputTextView->Insert(startPos, insertionText.String(), insertionText.Length());
                    
                    // Reposition the blinking cursor bar right after the newly inserted text
                    int32 newCursorPos = startPos + insertionText.Length();
                    inputTextView->Select(newCursorPos, newCursorPos);
                }
            }
            break;
        }





        // REMOVE BAN EXECUTOR: Transmits the unban command string out over the network connection
        case MSG_CONTEXT_UNBAN_SUBMIT: {
            BString targetChannel;
            void* winPtr = nullptr;

            if (message->FindString("target_channel", &targetChannel) == B_OK &&
                message->FindPointer("window_ref", &winPtr) == B_OK && winPtr != nullptr) {
                
                BWindow* modalWindow = static_cast<BWindow*>(winPtr);
                BString selectedMask = "";

                if (modalWindow->Lock()) {
                    BListView* listWidget = dynamic_cast<BListView*>(modalWindow->FindView("banListWidget"));
                    if (listWidget != nullptr) {
                        int32 selectionIdx = listWidget->CurrentSelection();
                        if (selectionIdx >= 0) {
                            BStringItem* selectedRow = dynamic_cast<BStringItem*>(listWidget->ItemAt(selectionIdx));
                            if (selectedRow != nullptr) {
                                selectedMask = selectedRow->Text();
                            }
                        }
                    }
                    modalWindow->Unlock();
                }

                // Verify a valid list row selection was established before building network payload
                if (selectedMask.Length() > 0) {
                    ServerTreeItem* contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                    BSecureSocket* activeSocket = GetActiveSocket(contextServer);

                    if (activeSocket != nullptr) {
                        BString outgoingPayload;
                        outgoingPayload << "MODE " << targetChannel << " -b " << selectedMask << "\r\n";
                        activeSocket->Write(outgoingPayload.String(), outgoingPayload.Length());
                        
                        // Close window frame cleanly
                        modalWindow->PostMessage(B_QUIT_REQUESTED);
                    }
                } else {
                    // Fallback helper notice alert box if no item row row highlight index was targeted
                    BAlert* alert = new BAlert("SelectionMissing", "Please select a ban entry mask row element to remove first.", "OK");
                    alert->Go();
                }
            }
            break;
        }






        // BAN DUMP ENGINE: Requests the raw server ban list data structure
        case MSG_CONTEXT_SHOW_BANS: {
            void* chanPtr = nullptr;
            ChannelTreeItem* targetedChanNode = nullptr;
            
            if (message->FindPointer("chan_item", &chanPtr) == B_OK && chanPtr != nullptr) {
                targetedChanNode = static_cast<ChannelTreeItem*>(chanPtr);
            } else {
                // FIX: Cast fActiveBufferItem safely from BStringItem* to ChannelTreeItem*
                targetedChanNode = dynamic_cast<ChannelTreeItem*>(fActiveBufferItem);
            }
            
            if (targetedChanNode == nullptr) break;
            BString activeChannel = targetedChanNode->Text();

            ServerTreeItem* contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            if (contextServer == nullptr) break;

            int32 spaceIdx = activeChannel.FindLast(" ");
            if (spaceIdx != B_ERROR) {
                BString extractedChannel;
                activeChannel.CopyInto(extractedChannel, spaceIdx + 1, activeChannel.Length() - spaceIdx);
                activeChannel = extractedChannel;
            }
            if (!activeChannel.StartsWith("#") && !activeChannel.StartsWith("&")) break;

            BSecureSocket* activeSocket = GetActiveSocket(contextServer);
            if (activeSocket != nullptr) {
                BString outgoingPayload;
                outgoingPayload << "MODE " << activeChannel << " b\r\n";
                activeSocket->Write(outgoingPayload.String(), outgoingPayload.Length());
            }
            break;
        }





         // CHANNEL MODE EXECUTOR: Dispatches mode modifications (-o, +v, -v) to the server
        case MSG_CONTEXT_DEOP:
        case MSG_CONTEXT_VOICE:
        case MSG_CONTEXT_DEVOICE: {
            BString targetNick;
            if (message->FindString("target_nick", &targetNick) == B_OK && targetNick.Length() > 0) {
                
                ServerTreeItem* contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                if (contextServer == nullptr) break;

                if (fActiveBufferItem == nullptr) break;
                BString activeChannel = fActiveBufferItem->Text();

                int32 spaceIdx = activeChannel.FindLast(" ");
                if (spaceIdx != B_ERROR) {
                    BString extractedChannel;
                    activeChannel.CopyInto(extractedChannel, spaceIdx + 1, activeChannel.Length() - spaceIdx);
                    activeChannel = extractedChannel;
                }

                if (!activeChannel.StartsWith("#") && !activeChannel.StartsWith("&")) break; 

                BString flag;
                switch (message->what) {
                    case MSG_CONTEXT_OP:      flag = "+o"; break;
                    case MSG_CONTEXT_DEOP:    flag = "-o"; break;
                    case MSG_CONTEXT_VOICE:   flag = "+v"; break;
                    case MSG_CONTEXT_DEVOICE: flag = "-v"; break;
                }

                // DRY Call replaces the old 6-line map check block
                BSecureSocket* activeSocket = GetActiveSocket(contextServer);

                if (activeSocket != nullptr) {
                    BString outgoingPayload;
                    outgoingPayload << "MODE " << activeChannel << " " << flag << " " << targetNick << "\r\n";
                    activeSocket->Write(outgoingPayload.String(), outgoingPayload.Length());
                }
            }
            break;
        }
        
 
 
        // OP ENVELOPE GENERATOR: Prompts window with time duration operator options
        case MSG_CONTEXT_OP: {
            BString targetNick;
            if (message->FindString("target_nick", &targetNick) == B_OK && targetNick.Length() > 0) {
                
                BRect windowFrame(0, 0, 400, 140);
                BRect screenFrame = Frame();
                windowFrame.OffsetTo(
                    screenFrame.left + (screenFrame.Width() - windowFrame.Width()) / 2,
                    screenFrame.top + (screenFrame.Height() - windowFrame.Height()) / 2
                );

                BWindow* inputWin = new BWindow(windowFrame, "Give Operator Status", 
                                               B_MODAL_WINDOW, 
                                               B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS);

                BView* panel = new BView(inputWin->Bounds(), "bgPanel", B_FOLLOW_ALL, B_WILL_DRAW);
                panel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
                inputWin->AddChild(panel);

                BTextView* promptLabel = new BTextView(BRect(15, 10, 385, 35), "prompt", 
                                                      BRect(0, 0, 370, 25), B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
                BString promptText;
                promptText << "Select how long to grant Operator status to '" << targetNick << "':";
                promptLabel->SetText(promptText.String());
                promptLabel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
                promptLabel->MakeEditable(false);
                promptLabel->MakeSelectable(false);
                panel->AddChild(promptLabel);

                // Create Durations Dropdown
                BPopUpMenu* timeMenu = new BPopUpMenu("OpDurations");
                timeMenu->AddItem(new BMenuItem("Permanently", nullptr));
                timeMenu->AddItem(new BMenuItem("5 Minutes", nullptr));
                timeMenu->AddItem(new BMenuItem("1 Hour", nullptr));
                timeMenu->AddItem(new BMenuItem("1 Day", nullptr));
                
                if (timeMenu->ItemAt(0) != nullptr) timeMenu->ItemAt(0)->SetMarked(true);

                BMenuField* durationField = new BMenuField(BRect(15, 45, 385, 70), "opDurationDropdown", 
                                                         "Duration: ", timeMenu);
                durationField->SetDivider(panel->StringWidth("Duration: ") + 5);
                panel->AddChild(durationField);

                // Action buttons
                BButton* cancelBtn = new BButton(BRect(200, 100, 285, 125), "cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
                
                // FIX: Corrected spelling to MSG_CONTEXT_OP_SUBMIT here
                BButton* opBtn = new BButton(BRect(295, 100, 385, 125), "op", "Grant Op", new BMessage(MSG_CONTEXT_OP_SUBMIT));
                
                cancelBtn->SetTarget(inputWin);
                panel->AddChild(cancelBtn);
                panel->AddChild(opBtn);

                BMessage* buttonPayload = new BMessage(MSG_CONTEXT_OP_SUBMIT);
                buttonPayload->AddString("target_nick", targetNick);
                buttonPayload->AddPointer("window_ref", inputWin);
                opBtn->SetMessage(buttonPayload);
                opBtn->SetTarget(this);

                inputWin->Show();
            }
            break;
        }


  
  
          // OP TRANSMITTER: Grants operator status and schedules the auto-deop runner
        case MSG_CONTEXT_OP_SUBMIT: {
            BString targetNick;
            BString opDuration = "Permanently";
            
            if (message->FindString("target_nick", &targetNick) == B_OK) {
                void* winPtr = nullptr;
                
                if (message->FindPointer("window_ref", &winPtr) == B_OK && winPtr != nullptr) {
                    BWindow* modalWindow = static_cast<BWindow*>(winPtr);
                    
                    if (modalWindow->Lock()) {
                        BMenuField* durationField = dynamic_cast<BMenuField*>(modalWindow->FindView("opDurationDropdown"));
                        if (durationField != nullptr && durationField->Menu() != nullptr) {
                            BMenuItem* markedItem = durationField->Menu()->FindMarked();
                            if (markedItem != nullptr) {
                                opDuration = markedItem->Label();
                            }
                        }
                        modalWindow->Unlock();
                        modalWindow->PostMessage(B_QUIT_REQUESTED);
                    }
                }

                ServerTreeItem* contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                if (contextServer == nullptr) break;

                if (fActiveBufferItem == nullptr) break;
                BString activeChannel = fActiveBufferItem->Text();

                int32 spaceIdx = activeChannel.FindLast(" ");
                if (spaceIdx != B_ERROR) {
                    BString extractedChannel;
                    activeChannel.CopyInto(extractedChannel, spaceIdx + 1, activeChannel.Length() - spaceIdx);
                    activeChannel = extractedChannel;
                }
                if (!activeChannel.StartsWith("#") && !activeChannel.StartsWith("&")) break;

                BSecureSocket* activeSocket = GetActiveSocket(contextServer);
                if (activeSocket != nullptr) {
                    // 1. Send the immediate +o payload over the wire
                    BString outgoingPayload;
                    outgoingPayload << "MODE " << activeChannel << " +o " << targetNick << "\r\n";
                    activeSocket->Write(outgoingPayload.String(), outgoingPayload.Length());

                    // 2. Schedule the automatic de-op payload if a time limit was picked
                    bigtime_t delayMicroseconds = 0;
                    if (opDuration == "5 Minutes") delayMicroseconds = 5LL * 60LL * 1000000LL;
                    else if (opDuration == "1 Hour") delayMicroseconds = 60LL * 60LL * 1000000LL;
                    else if (opDuration == "1 Day")  delayMicroseconds = 24LL * 60LL * 60LL * 1000000LL;

                    if (delayMicroseconds > 0) {
                        // 1. Permanently track their name locally so we can intercept future rejoins
                        BString* trackedNick = new BString(targetNick);
                        fAutoOpList.AddItem(trackedNick);

                        // 2. Create the notification parcel to remove them when the timer expires
                        BMessage* deopTrigger = new BMessage(MSG_CONTEXT_TIMED_DEOP_TRIGGER);
                        deopTrigger->AddString("target_channel", activeChannel);
                        deopTrigger->AddString("target_nick", targetNick);
                        deopTrigger->AddPointer("server_node", contextServer);

                        new BMessageRunner(BMessenger(this), deopTrigger, delayMicroseconds, 1);
                    }

                }
            }
            break;
        }

  
  
        // TIMED DEOP EVENT: Automatically executes when the scheduled BMessageRunner countdown expires
        case MSG_CONTEXT_TIMED_DEOP_TRIGGER: {
            BString targetChannel;
            BString targetNick;
            void* serverPtr = nullptr;

            if (message->FindString("target_channel", &targetChannel) == B_OK &&
                message->FindString("target_nick", &targetNick) == B_OK &&
                message->FindPointer("server_node", &serverPtr) == B_OK && serverPtr != nullptr) {

                // WIPE FROM AUTO-OP LIST: They are no longer protected by the automatic interceptor
                for (int32 i = fAutoOpList.CountItems() - 1; i >= 0; i--) {
                    BString* storedNick = fAutoOpList.ItemAt(i);
                    if (storedNick != nullptr && *storedNick == targetNick) {
                        delete fAutoOpList.RemoveItemAt(i);
                    }
                }

                ServerTreeItem* targetServer = static_cast<ServerTreeItem*>(serverPtr);
                BSecureSocket* activeSocket = GetActiveSocket(targetServer);

                if (activeSocket != nullptr) {
                    BString stripPayload;
                    stripPayload << "MODE " << targetChannel << " -o " << targetNick << "\r\n";
                    activeSocket->Write(stripPayload.String(), stripPayload.Length());
                }
            }
            break;
        }


        
        
        
        // KICK & BAN ENVELOPE GENERATOR: Prompts window with time ban option settings
        case MSG_CONTEXT_KICK: {
            BString targetNick;
            if (message->FindString("target_nick", &targetNick) == B_OK && targetNick.Length() > 0) {
                
                // Tallied the box height frame from 110 to 180 to fit new control elements cleanly
                BRect windowFrame(0, 0, 400, 180);
                BRect screenFrame = Frame();
                windowFrame.OffsetTo(
                    screenFrame.left + (screenFrame.Width() - windowFrame.Width()) / 2,
                    screenFrame.top + (screenFrame.Height() - windowFrame.Height()) / 2
                );

                BWindow* inputWin = new BWindow(windowFrame, "Kick / Ban User Control", 
                                               B_MODAL_WINDOW, 
                                               B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS);

                BView* panel = new BView(inputWin->Bounds(), "bgPanel", B_FOLLOW_ALL, B_WILL_DRAW);
                panel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
                inputWin->AddChild(panel);

                BTextView* promptLabel = new BTextView(BRect(15, 10, 385, 35), "prompt", 
                                                      BRect(0, 0, 370, 25), B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
                BString promptText;
                promptText << "Enter administration criteria for user '" << targetNick << "':";
                promptLabel->SetText(promptText.String());
                promptLabel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
                promptLabel->MakeEditable(false);
                promptLabel->MakeSelectable(false);
                panel->AddChild(promptLabel);

                // Initialize the structural base text entry field configuration box
                BMessage* enterPayload = new BMessage(MSG_CONTEXT_KICK_SUBMIT);
                enterPayload->AddString("target_nick", targetNick);
                
                BTextControl* reasonInput = new BTextControl(BRect(15, 45, 385, 70), "reasonField", 
                                                            "Reason: ", "Requested via context menu", enterPayload);
                reasonInput->SetDivider(panel->StringWidth("Reason: ") + 5);
                panel->AddChild(reasonInput);

                // 1. ADD BAN CHECKBOX (Uses 'CheckBox.h' interface properties natively)
                BCheckBox* banCheck = new BCheckBox(BRect(15, 80, 150, 105), "banCheckbox", "Also Ban User", nullptr);
                panel->AddChild(banCheck);

                // 2. ADD DURATION DROPDOWN (Uses 'MenuField.h' and 'PopUpMenu.h' controls)
                BPopUpMenu* timeMenu = new BPopUpMenu("Durations");
                timeMenu->AddItem(new BMenuItem("Forever", nullptr));
                timeMenu->AddItem(new BMenuItem("5 Minutes", nullptr));
                timeMenu->AddItem(new BMenuItem("1 Hour", nullptr));
                timeMenu->AddItem(new BMenuItem("1 Day", nullptr));
                
                // Select default configuration item marker
                if (timeMenu->ItemAt(0) != nullptr) timeMenu->ItemAt(0)->SetMarked(true);

                BMenuField* durationField = new BMenuField(BRect(160, 80, 385, 105), "durationDropdown", 
                                                         "Duration: ", timeMenu);
                durationField->SetDivider(panel->StringWidth("Duration: ") + 5);
                panel->AddChild(durationField);

                // Move operational button widgets safely down to bottom edge rows
                BButton* cancelBtn = new BButton(BRect(200, 140, 285, 165), "cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
                BButton* kickBtn = new BButton(BRect(295, 140, 385, 165), "kick", "Execute", new BMessage(MSG_CONTEXT_KICK_SUBMIT));
                
                cancelBtn->SetTarget(inputWin);
                panel->AddChild(cancelBtn);
                panel->AddChild(kickBtn);

                enterPayload->AddPointer("window_ref", inputWin);
                
                BMessage* buttonPayload = new BMessage(MSG_CONTEXT_KICK_SUBMIT);
                buttonPayload->AddString("target_nick", targetNick);
                buttonPayload->AddPointer("window_ref", inputWin);
                kickBtn->SetMessage(buttonPayload);
                
                reasonInput->SetTarget(this);
                kickBtn->SetTarget(this);

                reasonInput->MakeFocus(true);
                inputWin->Show();
            }
            break;
        }






        // KICK & BAN TRANSMITTER: Dispatches eviction and ban parameters to the network
        case MSG_CONTEXT_KICK_SUBMIT: {
            BString targetNick;
            BString kickReason = "Requested via context menu";
            bool shouldBan = false;
            BString banDuration = "Forever";
            
            if (message->FindString("target_nick", &targetNick) == B_OK) {
                void* winPtr = nullptr;
                
                if (message->FindPointer("window_ref", &winPtr) == B_OK && winPtr != nullptr) {
                    BWindow* modalWindow = static_cast<BWindow*>(winPtr);
                    
                    if (modalWindow->Lock()) {
                        // 1. Extract the text reason string
                        BTextControl* reasonField = dynamic_cast<BTextControl*>(modalWindow->FindView("reasonField"));
                        if (reasonField != nullptr) {
                            kickReason = reasonField->Text();
                        }
                        
                        // 2. Extract the ban checkbox state
                        BCheckBox* banCheck = dynamic_cast<BCheckBox*>(modalWindow->FindView("banCheckbox"));
                        if (banCheck != nullptr) {
                            shouldBan = (banCheck->Value() == B_CONTROL_ON);
                        }

                        // 3. Extract the active dropdown duration value
                        BMenuField* durationField = dynamic_cast<BMenuField*>(modalWindow->FindView("durationDropdown"));
                        if (durationField != nullptr && durationField->Menu() != nullptr) {
                            BMenuItem* markedItem = durationField->Menu()->FindMarked();
                            if (markedItem != nullptr) {
                                banDuration = markedItem->Label();
                            }
                        }
                        
                        modalWindow->Unlock();
                        modalWindow->PostMessage(B_QUIT_REQUESTED);
                    }
                }
                
                kickReason.Trim();
                if (kickReason.Length() == 0) {
                    kickReason = "No reason specified";
                }

                ServerTreeItem* contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                if (contextServer == nullptr) break;

                if (fActiveBufferItem == nullptr) break;
                BString activeChannel = fActiveBufferItem->Text();

                int32 spaceIdx = activeChannel.FindLast(" ");
                if (spaceIdx != B_ERROR) {
                    BString extractedChannel;
                    activeChannel.CopyInto(extractedChannel, spaceIdx + 1, activeChannel.Length() - spaceIdx);
                    activeChannel = extractedChannel;
                }
                if (!activeChannel.StartsWith("#") && !activeChannel.StartsWith("&")) break;

                BSecureSocket* activeSocket = GetActiveSocket(contextServer);
                if (activeSocket != nullptr) {
                    BString outgoingPayload;

                    // ==================== LIVE BAN PROCESSING ====================
                    if (shouldBan) {
                        // Standard IRC ban blocks nick variations securely via wildcard mask
                        outgoingPayload << "MODE " << activeChannel << " +b " << targetNick << "!*@*\r\n";
                        
                        // Optional note appended directly to the channel view string context
                        kickReason << " [Banned: " << banDuration << "]";
                    }

                    // Append the standard KICK instruction payload string
                    outgoingPayload << "KICK " << activeChannel << " " << targetNick << " :" << kickReason << "\r\n";
                    activeSocket->Write(outgoingPayload.String(), outgoingPayload.Length());
                }
            }
            break;
        }







        // PRIVATE QUERY EXECUTOR: Opens a new chat session branch for one-on-one chats
        case MSG_CONTEXT_PRIVMSG: {
            BString targetNick;
            if (message->FindString("target_nick", &targetNick) == B_OK && targetNick.Length() > 0) {
                
                // 1. Resolve which server connection context is currently active
                ServerTreeItem* contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                if (contextServer == nullptr) break;

                // 2. Check if a private query tree node already exists for this person
                ChannelTreeItem* queryNode = FindChannelNode(contextServer, targetNick);
                
                if (!queryNode) {
                    // Create a brand-new workspace window tab leaf node for this user
                    queryNode = new ChannelTreeItem(targetNick.String(), 
                                                   contextServer->GetIndex(), 
                                                   contextServer->IsCustom());

                    fChannelTree->AddUnder(queryNode, contextServer); 
                    fChannelTree->Expand(contextServer);
                    
                    // Instantiate their local channel room message buffer caches
                    LogToItemBuffer(queryNode, BString("--- Started Private Conversation with ") << targetNick << "\n");
                    fChannelUsers[queryNode] = new BObjectList<UserListItem, true>(20);
                }

                // 3. Automatically swap the active workspace panel view directly over to the new chat tab
                fActiveBufferItem = queryNode;
                if (fCustomChatLog != nullptr) {
                    fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                }

                // Update UI text bars instantly
                fTopicView->SetText(BString("Private query with ") << targetNick);
                
                // Trigger a full layout redraw to update lists and selection boxes cleanly
                RefreshUserListUI();
            }
            break;
        }


        // 1. CONTEXT CLICK HANDLER: Intercepts row interactions to show the menu
        case MSG_USER_LIST_CONTEXT_CLICK: {
            BPoint mousePoint;
            uint32 mouseBtn = 0;
            
            fUserList->GetMouse(&mousePoint, &mouseBtn);
            
            if (mouseBtn & B_SECONDARY_MOUSE_BUTTON) {
                int32 selectedIdx = fUserList->CurrentSelection();
                if (selectedIdx < 0) break;

                UserListItem* clickedItem = dynamic_cast<UserListItem*>(fUserList->ItemAt(selectedIdx));
                if (clickedItem == nullptr) break;

                // DRY call replaces manual stripping loop cleanly
                BString cleanNick = GetCleanNickname(clickedItem->Text());
                BPopUpMenu* contextMenu = new BPopUpMenu("UserContext", false, false);

                // CONDITION A: You right-clicked YOUR OWN nickname handle
                if (cleanNick == fMyNick) {
                    BMenuItem* toggleAwayItem = nullptr;
                    if (clickedItem->IsAway()) {
                        toggleAwayItem = new BMenuItem("Set Status: Back", new BMessage(MSG_CONTEXT_SET_AWAY));
                    } else {
                        toggleAwayItem = new BMenuItem("Set Status: Away", new BMessage(MSG_CONTEXT_SET_AWAY));
                    }
                    contextMenu->AddItem(toggleAwayItem);
                } 
                // CONDITION B: You right-clicked SOMEONE ELSE's nickname handle
                else {
                    BString menuLabel;
                    menuLabel << "Private Message " << cleanNick;
                    
                    BMessage* pmMsg = new BMessage(MSG_CONTEXT_PRIVMSG);
                    pmMsg->AddString("target_nick", cleanNick);
                    
                    contextMenu->AddItem(new BMenuItem(menuLabel.String(), pmMsg));
                    contextMenu->AddSeparatorItem();

                    BMenu* modesMenu = new BMenu("Channel Modes");
                    struct {
                        const char* label;
                        uint32 command;
                    } opActions[] = {
                        { "Op", MSG_CONTEXT_OP },
                        { "Deop", MSG_CONTEXT_DEOP },
                        { "Voice", MSG_CONTEXT_VOICE },
                        { "Devoice", MSG_CONTEXT_DEVOICE },
                        { "Kick", MSG_CONTEXT_KICK }
                    };

                    for (const auto& action : opActions) {
                        BMessage* actionMsg = new BMessage(action.command);
                        actionMsg->AddString("target_nick", cleanNick);
                        modesMenu->AddItem(new BMenuItem(action.label, actionMsg));
                    }

                    modesMenu->SetTargetForItems(this);
                    contextMenu->AddItem(modesMenu);
                }

                contextMenu->SetTargetForItems(this);
                contextMenu->Go(fUserList->ConvertToScreen(mousePoint), true, true, true);
            }
            break;
        }





         // 2. EXECUTE SERVER TRANSMISSION: Dispatches the corresponding raw /AWAY parameters
        case MSG_CONTEXT_SET_AWAY: {
            if (fActiveBufferItem != nullptr) {
                ServerTreeItem* targetedServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                
                // DRY Call simplifies your away status code branch
                BSecureSocket* activeSocket = GetActiveSocket(targetedServer);

                if (activeSocket != nullptr) {
                    BString outgoingPayload;
                    bool currentlyAway = false;

                    for (int32 i = 0; i < fUserList->CountItems(); i++) {
                        UserListItem* uiUser = dynamic_cast<UserListItem*>(fUserList->ItemAt(i));
                        if (uiUser != nullptr) {
                            BString txt = uiUser->Text();
                            while (txt.StartsWith("~") || txt.StartsWith("&") || 
                                   txt.StartsWith("%") || txt.StartsWith("@") || 
                                   txt.StartsWith("+")) {
                                txt.Remove(0, 1);
                            }
                            if (txt == fMyNick) {
                                currentlyAway = uiUser->IsAway();
                                break;
                            }
                        }
                    }

                    if (currentlyAway) {
                        outgoingPayload << "AWAY\r\n";
                    } else {
                        outgoingPayload << "AWAY :" << cfg.awayMessage.c_str() << "\r\n";
                    }
                    
                    activeSocket->Write(outgoingPayload.String(), outgoingPayload.Length());
                }
            }
            break;
        }




        
        case MSG_TOPIC_CHANGED: {
            if (fActiveBufferItem != nullptr) {
                BString channelName = fActiveBufferItem->Text();
                
                if (channelName.StartsWith("#")) {
                    BString newlyTypedTopic = fTopicView->Text();
                    newlyTypedTopic.Trim();

                    if (newlyTypedTopic.Length() > 0) {
                        // 1. Determine the correct active server node from the active channel node context
                        ChannelTreeItem* activeChan = dynamic_cast<ChannelTreeItem*>(fActiveBufferItem);
                        ServerTreeItem* targetedServer = nullptr;
                        
                        if (activeChan != nullptr) {
                            // Loop through your parent server nodes to locate the match, or query your tree structure directly
                            // Alternatively, fallback to your active server node tracking pointer:
                            targetedServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                        } else {
                            targetedServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                        }

                        // 2. Safely resolve the associated secure writing socket pipeline
                        BSecureSocket* activeSocket = nullptr;
                        if (targetedServer != nullptr && fServerSockets.count(targetedServer) > 0) {
                            activeSocket = fServerSockets[targetedServer];
                        } else {
                            activeSocket = (targetedServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                        }

                        if (activeSocket != nullptr) {
                            BString topicCommand;
                            topicCommand << "TOPIC " << channelName << " :" << newlyTypedTopic << "\r\n";
                            activeSocket->Write(topicCommand.String(), topicCommand.Length());
                        }
                    }
                }
            }
            break;
        }



		case MSG_TOGGLE_CUSTOM_DRAW: {
			bool initialBootPass = false;
			message->FindBool("initial_boot_pass", &initialBootPass);

			// 1. Locate the active server config instance
			size_t activeIdx = (size_t)selectedConfig;
			ServerConfig* activeSrv = nullptr;
			
			if (activeIdx < cfg.servers.size()) {
				activeSrv = &cfg.servers[activeIdx];
			} else {
				size_t customIdx = activeIdx - cfg.servers.size();
				if (customIdx < cfg.customServers.size()) {
					activeSrv = &cfg.customServers[customIdx];
				}
			}

			// 2. Flip the state contextually
			if (!initialBootPass && activeSrv != nullptr) {
				activeSrv->useCustomDrawFunction = !activeSrv->useCustomDrawFunction;
				save_config();
			}

			// 3. Resolve active visibility constraints state
			bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;

			void* sourcePtr = nullptr;
			if (message->FindPointer("source", &sourcePtr) == B_OK) {
				BMenuItem* clickedItem = static_cast<BMenuItem*>(sourcePtr);
				if (clickedItem != nullptr) {
					clickedItem->SetMarked(drawEngineActive);
				}
			}

			if (fChatLog == nullptr || fCustomChatLog == nullptr || fChatContainer == nullptr)
				break;

			BView* chatScroll = fChatLog->Parent();       
			BView* customScroll = fCustomChatLog->Parent(); 
			BLayout* layout = fChatContainer->GetLayout();

			if (layout == nullptr)
				break;

			// Swap layout hierarchies using contextual state profile choice
			if (drawEngineActive) {
				if (chatScroll && chatScroll->Parent() == fChatContainer) {
					layout->RemoveView(chatScroll);
				}
				if (customScroll && customScroll->Parent() != fChatContainer) {
					layout->AddView(customScroll);
				}
			} else {
				if (customScroll && customScroll->Parent() == fChatContainer) {
					layout->RemoveView(customScroll);
				}
				if (chatScroll && chatScroll->Parent() != fChatContainer) {
					layout->AddView(chatScroll);
				}
			}
    
			if (fCustomChatLog) {
				fCustomChatLog->SetExplicitMinSize(BSize(100.0f, 100.0f));
				fCustomChatLog->SetExplicitMaxSize(BSize(B_SIZE_UNSET, B_SIZE_UNSET));
				fCustomChatLog->SetExplicitPreferredSize(BSize(B_SIZE_UNSET, B_SIZE_UNSET));
				fCustomChatLog->ClearAllLines();

				if (activeSrv != nullptr) {
					fCustomChatLog->SetLineHeight(activeSrv->chatLogFontSize + 4.0f);

					if (!activeSrv->backgroundImagePath.empty()) {
						fCustomChatLog->SetBackgroundImage(activeSrv->backgroundImagePath.c_str());
						fCustomChatLog->SetBackgroundDimming(activeSrv->backgroundOpacity);
					} else {
						fCustomChatLog->SetBackgroundImage(nullptr);
					}
				}
			}

			this->RebuildActiveChannelBuffer();
			
			fChatContainer->InvalidateLayout(true);
			this->InvalidateLayout(true);
			break;
		}








        case MSG_DISCONNECT_SERVER: {
            ServerTreeItem* srvItem = nullptr;
            if (message->FindPointer("server_item", (void**)&srvItem) == B_OK && srvItem != nullptr) {
                
                // Thread-safe map inspection
                Lock();
                if (fServerSockets.count(srvItem) > 0 && fServerSockets[srvItem] != nullptr) {
                    
                    // 1. Send an optional quick QUIT message so the remote network drops you gracefully
                    BString quitPayload;
                    quitPayload << "QUIT :Disconnecting from client\r\n";
                    fServerSockets[srvItem]->Write(quitPayload.String(), quitPayload.Length());
                    
                    // 2. Forcefully close the network pipe descriptor. 
                    // This wakes up background worker thread immediately!
                    fServerSockets[srvItem]->Disconnect();
                    
                    // 3. Print a clean disconnection status notice locally
                    BStringItem* serverLog = FindServerLogNode(srvItem);
                    if (serverLog != nullptr) {
                        LogToItemBuffer(serverLog, "--- Disconnected from server by user choice.\n");
                    }
                }
                Unlock();
            }
            break;
        }


        case 'dlcs': { // Delete Custom Server Message
            ServerTreeItem* srvItem = nullptr;
            if (message->FindPointer("server_item", (void**)&srvItem) != B_OK || srvItem == nullptr) {
                break;
            }

            size_t targetIndex = srvItem->GetIndex();
            
            // Check bounds against configuration memory spaces safely
            if (srvItem->IsCustom() && targetIndex < cfg.customServers.size()) {
                
                // 1. Thread Cleanup Stage (No blocking wait loops inside UI lock space)
                Lock();
                thread_id tid = (fServerThreads.count(srvItem) > 0) ? fServerThreads[srvItem] : -1;
                BSecureSocket* socketPtr = (fServerSockets.count(srvItem) > 0) ? fServerSockets[srvItem] : nullptr;
                
                if (socketPtr != nullptr) {
                    BString quitPayload;
                    quitPayload << "QUIT :Server entry deleted by user\r\n";
                    socketPtr->Write(quitPayload.String(), quitPayload.Length());
                    socketPtr->Disconnect(); 
                }

                // Clean the internal key trackers immediately to prevent reuse conflicts
                fServerThreads.erase(srvItem);
                fServerSockets.erase(srvItem);
                Unlock();

                // 2. Kill the socket thread cleanly without freezing the window interaction thread
                if (tid >= 0) {
                    // Signal the Haiku kernel to kill the thread if it fails to exit smoothly on disconnect
                    kill_thread(tid); 
                }

                // 3. Remove item out of global configurations vector array
                cfg.customServers.erase(cfg.customServers.begin() + targetIndex);
                
                // 4. Cleanly strip all nested channel child nodes under this server to prevent dangling sub-views
                int32 srvTreePos = fChannelTree->IndexOf(srvItem);
                if (srvTreePos != B_ERROR) {
                    // Iterate and remove children until we hit another server item or out-of-bounds layout limit
                    while (srvTreePos + 1 < fChannelTree->CountItems()) {
                        BListItem* nextItem = fChannelTree->ItemAt(srvTreePos + 1);
                        // If it's a ServerTreeItem, we reached the next network bracket; stop deleting
                        if (dynamic_cast<ServerTreeItem*>(nextItem) != nullptr) {
                            break;
                        }
                        // It is a subchannel item node (like a BStringItem/ChannelItem) — strip it safely
                        BListItem* removedChild = fChannelTree->RemoveItem(srvTreePos + 1);
                        delete removedChild;
                    }
                }

                // 5. Remove the actual server item node from visual container
                fChannelTree->RemoveItem(srvItem);
                delete srvItem;

                // 6. Fix internal indexes across remaining custom layout slots safely
                for (int32 i = 0; i < fChannelTree->CountItems(); i++) {
                    ServerTreeItem* current = dynamic_cast<ServerTreeItem*>(fChannelTree->ItemAt(i));
                    if (current != nullptr && current->IsCustom()) {
                        size_t curIdx = current->GetIndex();
                        if (curIdx > targetIndex) {
                            current->SetIndex(curIdx - 1); 
                        }
                    }
                }

                // 7. Commit changes to persistent system disk storage
                save_config();
                
                // Force active view adjustments to re-paint tree nodes
                fChannelTree->Invalidate();
            }
            break;
        }



        case 'adcs': {            
            AddServerWindow* dlg = new AddServerWindow(this);
            dlg->Show(); 
            break;
        }

        case MSG_ADD_CUSTOM_SERVER_SUBMIT: {
            BString name, host, nick, pass;
            BString altNick, altNick2; 
            int32 port;

            if (message->FindString("name", &name) == B_OK &&
                message->FindString("host", &host) == B_OK &&
                message->FindInt32("port", &port) == B_OK &&
                message->FindString("nick", &nick) == B_OK) {
                
                message->FindString("altNick", &altNick);
                message->FindString("altNick2", &altNick2);
                message->FindString("pass", &pass); // Can remain blank safely

                // 1. Build the data structure block matching settings scheme
                ServerConfig customSrv;
                customSrv.name = name.String();
                customSrv.host = host.String();
                customSrv.port = static_cast<uint16>(port);
                customSrv.nick = nick.String();
                
                // Assign the extracted alternate strings, applying safe defaults if left blank in the form
                customSrv.altNick = (altNick.Length() > 0) ? altNick.String() : BString(nick).Append("+").String();
                customSrv.altNick2 = (altNick2.Length() > 0) ? altNick2.String() : BString(nick).Append("__").String();
                
                customSrv.pass = pass.String();
                customSrv.autoConnect = false;
                customSrv.autoReconnect = false;
                customSrv.hideStatusMessages = false;

                // 2. Append directly to the config struct vector array and save file changes
                cfg.customServers.push_back(customSrv);
                save_config();

                // 3. Update the UI tree instantly on screen without restarting!
                size_t newIndex = cfg.customServers.size() - 1;
                
                // Keeping precise signature fields exactly the same
                ServerTreeItem* customNode = new ServerTreeItem(customSrv.name.c_str(), newIndex, false, true);
                
                fChannelTree->AddItem(customNode);
                fChannelTree->Invalidate(); 
            }
            break;
        }




        	
    case 'rmch': { // Remove Channel action handler
        ChannelTreeItem* chanItem = nullptr;
        
        if (message->FindPointer("channel_item", (void**)&chanItem) != B_OK || chanItem == nullptr) {
            break;
        }
        
        // 1. Clean dynamic tracking tags from raw string labels before checking prefixes
        BString targetChanName(chanItem->Text());
        int32 tagPos = targetChanName.FindFirst(" [");
        if (tagPos != B_ERROR) {
            targetChanName.Truncate(tagPos);
        }

        // ====================================================================
        // STATE INTERCEPTION VALUE CHECK (Channel vs Private Query)
        // ====================================================================
        // In the IRC protocol, channels MUST begin with a specific prefix symbol 
        // (typically '#', '&', or '!'). If missing, this item is a private query!
        bool isActualIrcChannel = targetChanName.StartsWith("#") || 
                                  targetChanName.StartsWith("&") || 
                                  targetChanName.StartsWith("!");

        ServerTreeItem* parentServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(chanItem));
        
        // Only modify network sockets or configuration lists if the item is a true channel room
        if (parentServer != nullptr && isActualIrcChannel) {
            
            // Dynamically pull the correct network connection out of our lookup map
            BSecureSocket* activeSocket = nullptr;
            if (fServerSockets.count(parentServer) > 0) {
                activeSocket = fServerSockets[parentServer];
            } else {
                activeSocket = (parentServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
            }

            if (activeSocket != nullptr) {
                BString partCmd;
                partCmd << "PART " << targetChanName << " :Channel removed by user\r\n";
                activeSocket->Write(partCmd.String(), partCmd.Length());
            }
            
            // 2. Remove the channel name from the configuration autojoin vector array
            size_t serverIdx = parentServer->GetIndex();
            
            if (parentServer->IsCustom()) {
                if (serverIdx < cfg.customServers.size()) {
                    auto& autojoinVec = cfg.customServers[serverIdx].autojoin;
                    for (auto it = autojoinVec.begin(); it != autojoinVec.end(); ) {
                        if (targetChanName.ICompare(it->c_str()) == 0) {
                            it = autojoinVec.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            } else {
                if (serverIdx < cfg.servers.size()) {
                    auto& autojoinVec = cfg.servers[serverIdx].autojoin;
                    for (auto it = autojoinVec.begin(); it != autojoinVec.end(); ) {
                        if (targetChanName.ICompare(it->c_str()) == 0) {
                            it = autojoinVec.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        } else if (parentServer != nullptr && !isActualIrcChannel) {
            if (cfg.debugEnable) {
                printf("[DEBUG] Removing private query tab (%s). Skipping network PART commands.\n", 
                    targetChanName.String());
            }
        }

        // 3. Wipe current chat logs from active memory views if we delete the room we are looking at
        if (fActiveBufferItem == chanItem) {
            fActiveBufferItem = nullptr;
            
            fChatLog->SetText("");
            
            if (fCustomChatLog) {
                fCustomChatLog->SetActiveChannel(nullptr);
                fCustomChatLog->ClearAllLines();
            }
            
            while (fUserList->CountItems() > 0) {
                delete fUserList->RemoveItem((int32)0);
            }
            
            fTopicView->SetText("No active channel buffer targeted.");
        }
        
        // 4. Safely purge dynamic memory trackers associated with this pointer key
        fTextBuffers.erase(chanItem);
        fChannelTopics.erase(chanItem);
        
        if (fChannelUsers.find(chanItem) != fChannelUsers.end()) {
            delete fChannelUsers[chanItem];
            fChannelUsers.erase(chanItem);
        }

        // 5. Safely drop the element out of the UI tree structure entirely
        fChannelTree->RemoveItem(chanItem);
        delete chanItem; 
        
        // 6. Select a fallback tree item if the workspace went completely blank
        if (fActiveBufferItem == nullptr && fChannelTree->CountItems() > 0) {
            fChannelTree->Select(0);
        }

        // 7. Save settings file to disk immediately
        save_config();
        break;
    }



        	
     case 'mscf': {
        save_config(); 

        size_t activeIdx = (size_t)selectedConfig;
        ServerConfig* activeSrv = nullptr;
        
        if (activeIdx < cfg.servers.size()) {
            activeSrv = &cfg.servers[activeIdx];
        } else {
            size_t customIdx = activeIdx - cfg.servers.size();
            if (customIdx < cfg.customServers.size()) {
                activeSrv = &cfg.customServers[customIdx];
            }
        }

        int32 targetServerListFontSize = activeSrv ? activeSrv->serverListFontSize : cfg.serverListFontSize;
        int32 targetChatLogFontSize    = activeSrv ? activeSrv->chatLogFontSize : cfg.chatLogFontSize;
        int32 targetUserListFontSize   = activeSrv ? activeSrv->userListFontSize : cfg.userListFontSize;

        // 1. Update Server List Font AND recalculate item heights (Left panel)
        BFont treeFont;
        fChannelTree->GetFont(&treeFont);
        treeFont.SetSize(targetServerListFontSize);
        fChannelTree->SetFont(&treeFont);

        int32 treeCount = fChannelTree->CountItems();
        for (int32 i = 0; i < treeCount; i++) {
            BListItem* item = fChannelTree->ItemAt(i);
            if (item != nullptr) {
                item->Update(fChannelTree, &treeFont);
            }
        }
        fChannelTree->InvalidateLayout();
        fChannelTree->Invalidate(); 

        // 2. Update Topic View Bar Font Size
        BFont topicFont;
        fTopicView->GetFont(&topicFont);
        topicFont.SetSize(targetChatLogFontSize);
        fTopicView->SetFont(&topicFont);
        
        if (fTopicView->TextView()) {
            fTopicView->TextView()->SetFontAndColor(&topicFont, B_FONT_SIZE);
        }
        fTopicView->InvalidateLayout();
        fTopicView->Invalidate();

        // 3. Update BTextView history without erasing formatted style loops (Center panel)
        BFont chatFont;
        fChatLog->GetFont(&chatFont);
        chatFont.SetSize(targetChatLogFontSize);
        fChatLog->SetFont(&chatFont);
        fChatLog->SetFontAndColor(&chatFont, B_FONT_SIZE); 
        fChatLog->Invalidate();

        // 4. Update User List Font AND recalculate item heights (Right panel)
        BFont userFont;
        fUserList->GetFont(&userFont);
        userFont.SetSize(targetUserListFontSize);
        fUserList->SetFont(&userFont);

        int32 userCount = fUserList->CountItems();
        for (int32 i = 0; i < userCount; i++) {
            BListItem* item = fUserList->ItemAt(i);
            if (item != nullptr) {
                item->Update(fUserList, &userFont);
            }
        }
        fUserList->InvalidateLayout();
        fUserList->Invalidate();

        // 5. Live Resize and Recalculate Custom Draw Engine Layout Geometry
        if (fCustomChatLog != nullptr) {
            fCustomChatLog->SetLineHeight(targetChatLogFontSize + 4.0f);
            
            std::string bgPath = "";
            int32 dimmingLevel = 30;
            
            if (activeSrv != nullptr) {
                bgPath = activeSrv->backgroundImagePath;
                dimmingLevel = activeSrv->backgroundOpacity;
            }

            if (bgPath.empty()) {
                fCustomChatLog->SetBackgroundImage(nullptr);
            } else {
                fCustomChatLog->SetBackgroundImage(bgPath.c_str());
            }

            fCustomChatLog->SetBackgroundDimming(dimmingLevel);
            fCustomChatLog->RecalculateAllLineWraps();
            fCustomChatLog->Invalidate();
        }

        break;
    }


        case 'clch': {
            void* nodePtr = nullptr;
            if (message->FindPointer("active_node", &nodePtr) == B_OK && nodePtr != nullptr) {
                BStringItem* targetNode = static_cast<BStringItem*>(nodePtr);
                
                // 1. Purge the text buffer memory map cache for this specific room node
                // This stops RebuildActiveChannelBuffer() from re-populating text on tab switches
                if (fTextBuffers.find(targetNode) != fTextBuffers.end()) {
                    fTextBuffers[targetNode] = ""; 
                }
                
                // 2. Also clear out your channel history user topics cache maps if tracked here
                if (fChannelTopics.find(targetNode) != fChannelTopics.end()) {
                    fChannelTopics[targetNode] = "";
                }
            }
            break;
        }





    	case MSG_CONTEXT_CONFIGURE_SERVER: {
        	ServerTreeItem* item = nullptr;
        	if (message->FindPointer("server_item", (void**)&item) == B_OK && item != nullptr) {
            
            	size_t matchingIndex = item->GetIndex(); // Extract the true index directly from the item!
            	bool isCustom = item->IsCustom();        // Extract whether it is custom or default
            
            	// Pass the custom flag as a new 4th argument to the dialog window constructor
            	ServerConfigWindow* configWin = new ServerConfigWindow(this, item, matchingIndex, isCustom);
            	configWin->Show();
        	}
        	break;
    	}


            case MSG_RECONNECT_SERVER: {
                void* ptr;
                if (message->FindPointer("server_item", &ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        BString textNotice;
                        textNotice << "--- [Auto-Reconnect] Connection lost. Attempting to reconnect to " << serverItem->Text() << "...\n";
                        LogToItemBuffer(FindServerLogNode(serverItem), textNotice);
                        
                        // Fire up the native connection runner cleanly
                        ConnectToServer(serverItem);
                    }
                }
                break;
            }


            case MSG_TOGGLE_AUTORECONNECT: {
                void* ptr;
                if (message->FindPointer("server_item", &ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        size_t idx = serverItem->GetIndex();
                        
                        bool newState = !serverItem->IsAutoReconnect();
                        serverItem->SetAutoReconnect(newState);
                        cfg.servers[idx].autoReconnect = newState;
                        
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(serverItem));
                    }
                }
                break;
            }


            case MSG_CONTEXT_ABOUT: {
                // Formulate the informational message body text
                BString aboutText;
                aboutText <<  AppInfo::VERSION_STRING << "\n" 
                		  << "By Kris Beazley (ablyss)\n"
                		  << "Copyright 2026 The MIT License\n\n"

                          << "A lightweight, multi-server IRC client built natively "
                          << "for the Haiku Operating System utilizing BSplitView layout architectures.\n\n"
                          << "Features JSON configuration saving, automated services identification, "
                          << "custom spreadsheet channel list navigators, and dynamic dark mode adaptability.";

                // Instantiate a native Haiku informational dialog modal box
                BAlert* aboutAlert = new BAlert("About Cricket", aboutText.String(), "OK", 
                                                nullptr, nullptr, B_WIDTH_AS_USUAL, B_INFO_ALERT);
                
                // Instruct the alert modal box to float asynchronously so it doesn't freeze the main app loop
                aboutAlert->Go();
                break;
            }

        	
		    case MSG_TOGGLE_HIDE_STATUS: {
        		ServerTreeItem* item = nullptr;
        		if (message->FindPointer("server_item", (void**)&item) == B_OK && item != nullptr) {
            		// 1. Flip the state inside the visible UI tree item element
            		bool newStatus = !item->IsHideStatus();
            		item->SetHideStatus(newStatus);
            
            		size_t targetIndex = item->GetIndex();

            		// 2. Directly target the correct array vector index using properties
            		if (item->IsCustom()) {
                        if (targetIndex < cfg.customServers.size()) {
                            cfg.customServers[targetIndex].hideStatusMessages = newStatus;
                        }
            		} else {
                        if (targetIndex < cfg.servers.size()) {
                            cfg.servers[targetIndex].hideStatusMessages = newStatus;
                        }
            		}
            		
            		// 3. Commit changes to CricketConfig.txt JSON file instantly
            		save_config(); 
        		}
        		break;
    		}
        	
        	
            case MSG_CONTEXT_CHAN_LIST: {
                void* ptr = nullptr;
                if (message->FindPointer("server_item", (void**)&ptr) != B_OK || ptr == nullptr) {
                    break;
                }

                ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                
                BSecureSocket* activeSocket = nullptr;
                if (fServerSockets.count(serverItem) > 0) {
                    activeSocket = fServerSockets[serverItem];
                } else {
                    activeSocket = (serverItem == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }
                
                if (activeSocket != nullptr) {
                    if (fActiveListWindow == nullptr) {
                        fActiveListWindow = new IRCChannelListWindow(this, activeSocket, serverItem, &fActiveListWindow);
                        fActiveListWindow->Show();
                    } else {
                        if (fActiveListWindow->Lock()) {
                            fActiveListWindow->Activate(true);
                            fActiveListWindow->Unlock();
                        }
                    }
                } else {
                    BString warning;
                    warning.SetToFormat("System Error: You must connect to '%s' first before requesting a channel list.\n", serverItem->Text());
                    LogToItemBuffer(fActiveBufferItem, warning);
                }
                break;
            }




        	
           case 'cldc':
                fActiveListWindow = nullptr;
                break;

        	
            case MSG_TOGGLE_AUTOCONNECT: {
                void* ptr;
                // Cast to (void**)&ptr to comply with the Haiku FindPointer API signature
                if (message->FindPointer("server_item", (void**)&ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        size_t idx = serverItem->GetIndex();
                        
                        // Invert state value
                        bool newState = !serverItem->IsAutoConnect();
                        serverItem->SetAutoConnect(newState);
                        
                        // Route the configuration assignment to the correct target profile vector
                        if (serverItem->IsCustom()) {
                            if (idx < cfg.customServers.size()) {
                                cfg.customServers[idx].autoConnect = newState;
                            }
                        } else {
                            if (idx < cfg.servers.size()) {
                                cfg.servers[idx].autoConnect = newState;
                            }
                        }
                        
                        // Flush to settings file and force immediate sidebar graphic redraw loop pass
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(serverItem));
                    }
                }
                break;
            }

            
            case B_COLORS_UPDATED: {
                // 1. Fetch the newly selected color vectors
                rgb_color newBgColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
                rgb_color newTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);
                rgb_color panelBgColor = ui_color(B_PANEL_BACKGROUND_COLOR);
                rgb_color panelTextColor = ui_color(B_PANEL_TEXT_COLOR);

                // 2. Clear and update the standard BTextView layout tree properties
                if (fChatLog) {
                    fChatLog->SetViewColor(newBgColor);
                    fChatLog->SetLowColor(newBgColor);
                    
                    // Force BTextView's internal styling loop to flag a full redraw
                    fChatLog->SetStylable(true);
                    
                    // Safely update text run foregrounds without altering your custom bold/underlines
                    fChatLog->SetFontAndColor(nullptr, 0, &newTextColor);

                    // Ascend through parent wrappers (like BScrollView) to wipe out sticky backgrounds
                    BView* parentView = fChatLog->Parent();
                    while (parentView != nullptr && parentView != fChatContainer) {
                        parentView->SetViewColor(newBgColor);
                        parentView->SetLowColor(newBgColor);
                        parentView->Invalidate();
                        parentView = parentView->Parent();
                    }
                    
                    fChatLog->Invalidate();
                }

                // 3. Explicitly propagate theme updates down to your Custom Draw Engine View ---
                if (fCustomChatLog) {
                    // Update the custom drawing view base parameters
                    fCustomChatLog->SetViewColor(newBgColor);
                    fCustomChatLog->SetLowColor(newBgColor);
                    
                    // Route the message directly into the view's own MessageReceived handler 
                    // so it updates its system backgrounds and repaints!
                    fCustomChatLog->MessageReceived(message);
                    fCustomChatLog->Invalidate();
                }
                
                // 4. Update the global channel Topic text view layout blocks
                if (fTopicView) {
                    fTopicView->SetViewColor(panelBgColor);
                    fTopicView->SetLowColor(panelBgColor);
                    
                    BTextView* topicText = fTopicView->TextView();
                    if (topicText) {
                        topicText->SetViewColor(panelBgColor);
                        topicText->SetLowColor(panelBgColor);
                        topicText->SetFontAndColor(be_plain_font, B_FONT_ALL, &panelTextColor);
                        topicText->Invalidate();
                    }
                    fTopicView->Invalidate();
                }

                // 5. Instruct the layout engine to completely refresh container structures
                if (fChatContainer) {
                    fChatContainer->InvalidateLayout(true);
                    fChatContainer->Invalidate();
                }
                
                // 6. Fall back to base class processing safely at the end ---
                // This lets the window route the event down to all other standard panels naturally
                BWindow::MessageReceived(message); 
                break;
            }






        	
            case MSG_TOGGLE_AUTOJOIN: {
                void* ptr;
                // Cast to (void**)&ptr to comply with the Haiku FindPointer API signature
                if (message->FindPointer("chan_item", (void**)&ptr) == B_OK) {
                    ChannelTreeItem* chanItem = static_cast<ChannelTreeItem*>(ptr);
                    if (chanItem != nullptr) {
                        size_t srvIdx = chanItem->GetServerIndex();
                        std::string targetChan = chanItem->Text();
                        
                        // Route data manipulation to customServers or servers using the item flag
                        if (chanItem->IsCustom()) {
                            if (srvIdx < cfg.customServers.size()) {
                                auto& vec = cfg.customServers[srvIdx].autojoin;
                                if (chanItem->IsAutoJoin()) {
                                    // Deactivate: Remove the string leaf value from our configuration vector
                                    vec.erase(std::remove(vec.begin(), vec.end(), targetChan), vec.end());
                                    chanItem->SetAutoJoin(false);
                                } else {
                                    // Activate: Append string into configuration profile
                                    vec.push_back(targetChan);
                                    chanItem->SetAutoJoin(true);
                                }
                            }
                        } else {
                            if (srvIdx < cfg.servers.size()) {
                                auto& vec = cfg.servers[srvIdx].autojoin;
                                if (chanItem->IsAutoJoin()) {
                                    // Deactivate: Remove the string leaf value from our configuration vector
                                    vec.erase(std::remove(vec.begin(), vec.end(), targetChan), vec.end());
                                    chanItem->SetAutoJoin(false);
                                } else {
                                    // Activate: Append string into configuration profile
                                    vec.push_back(targetChan);
                                    chanItem->SetAutoJoin(true);
                                }
                            }
                        }
                        
                        // Flush the changes instantly onto the hard disk and force row graphics updates
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(chanItem));
                    }
                }
                break;
            }


        	
        case 'slch': {
            // 1. Declare the safe scope target variables right at the top
            int32 targetServerListFontSize = cfg.serverListFontSize;
            int32 targetChatLogFontSize    = cfg.chatLogFontSize;
            int32 targetUserListFontSize   = cfg.userListFontSize;

            int32 selectedIdx = fChannelTree->CurrentSelection();
            if (selectedIdx >= 0) {
                BListItem* rawItem = fChannelTree->ItemAt(selectedIdx);
                BStringItem* selectedItem = dynamic_cast<BStringItem*>(rawItem);
                
                if (selectedItem != nullptr && selectedItem != fActiveBufferItem) {
                    
                    if (fActiveBufferItem != nullptr && !cfg.useCustomDrawFunction) {
                        fTextBuffers[fActiveBufferItem] = fChatLog->Text();
                    }
                    
                    fActiveBufferItem = selectedItem;
                    
                    // === FIXED CONTEXT POINTER SYNCHRONIZATION ===
                    BListItem* superItem = fChannelTree->Superitem(rawItem);
                    if (superItem != nullptr) {
                        ServerTreeItem* parentServer = dynamic_cast<ServerTreeItem*>(superItem);
                        if (parentServer != nullptr) {
                            fCurrentServerNode = parentServer;
                        }
                    } else {
                        ServerTreeItem* clickedServer = dynamic_cast<ServerTreeItem*>(rawItem);
                        if (clickedServer != nullptr) {
                            fCurrentServerNode = clickedServer;
                        }
                    }
                    // =========================================================================
                    
                    // === DYNAMIC BACKGROUND SEPARATION PASS ===
                    if (fCustomChatLog != nullptr && fCurrentServerNode != nullptr) {
                        BString currentServerName(fCurrentServerNode->Text());
                        bool bgFound = false;

                        for (const auto& srv : cfg.servers) {
                            if (BString(srv.name.c_str()) == currentServerName) {
                                fCustomChatLog->SetBackgroundImage(srv.backgroundImagePath.c_str());
                                fCustomChatLog->SetBackgroundDimming(srv.backgroundOpacity);
                                bgFound = true;
                                break;
                            }
                        }

                        if (!bgFound) {
                            for (const auto& srv : cfg.customServers) {
                                if (BString(srv.name.c_str()) == currentServerName) {
                                    fCustomChatLog->SetBackgroundImage(srv.backgroundImagePath.c_str());
                                    fCustomChatLog->SetBackgroundDimming(srv.backgroundOpacity);
                                    break;
                                }
                            }
                        }
                        
                        fCustomChatLog->Invalidate();
                    }
                    // =========================================================================

                    // --- INSERTION POINT: ADD THE NEW RE-RESOLVE CONTEXT CODE HERE ---
                    if (fCurrentServerNode != nullptr) {
                        BString currentServerName(fCurrentServerNode->Text());
                        bool foundIndex = false;

                        for (size_t i = 0; i < cfg.servers.size(); i++) {
                            if (BString(cfg.servers[i].name.c_str()) == currentServerName) {
                                selectedConfig = (int32)i;
                                foundIndex = true;
                                break;
                            }
                        }

                        if (!foundIndex) {
                            for (size_t i = 0; i < cfg.customServers.size(); i++) {
                                if (BString(cfg.customServers[i].name.c_str()) == currentServerName) {
                                    selectedConfig = (int32)(cfg.servers.size() + i);
                                    foundIndex = true;
                                    break;
                                }
                            }
                        }

                        ServerConfig* activeSrv = nullptr;
                        if (foundIndex) {
                            activeSrv = (selectedConfig < (int32)cfg.servers.size()) 
                                ? &cfg.servers[selectedConfig] 
                                : &cfg.customServers[selectedConfig - cfg.servers.size()];
                        }

                        if (activeSrv != nullptr) {
                            targetServerListFontSize = activeSrv->serverListFontSize;
                            targetChatLogFontSize    = activeSrv->chatLogFontSize;
                            targetUserListFontSize   = activeSrv->userListFontSize;
                        }

                        // --- LIVE RESIZE CHAT VIEWS ---
                        BFont tempFont;
                        fChannelTree->GetFont(&tempFont);
                        tempFont.SetSize(targetServerListFontSize);
                        fChannelTree->SetFont(&tempFont, B_FONT_SIZE);
                        for (int32 i = 0; i < fChannelTree->CountItems(); i++) {
                            BListItem* it = fChannelTree->ItemAt(i);
                            if (it) it->Update(fChannelTree, &tempFont);
                        }
                        fChannelTree->Invalidate();

                        fChatLog->GetFont(&tempFont);
                        tempFont.SetSize(targetChatLogFontSize);
                        fChatLog->SetFont(&tempFont, B_FONT_SIZE);
                        fChatLog->SetFontAndColor(&tempFont, B_FONT_SIZE);

                        if (fCustomChatLog != nullptr) {
                            fCustomChatLog->SetLineHeight(targetChatLogFontSize + 4.0f);
                        }

                        fTopicView->GetFont(&tempFont);
                        tempFont.SetSize(targetChatLogFontSize);
                        fTopicView->SetFont(&tempFont, B_FONT_SIZE);
                        if (fTopicView->TextView()) {
                            fTopicView->TextView()->SetFontAndColor(&tempFont, B_FONT_SIZE);
                        }

                        fUserList->GetFont(&tempFont);
                        tempFont.SetSize(targetUserListFontSize);
                        fUserList->SetFont(&tempFont, B_FONT_SIZE);
                        fUserList->Invalidate();
                    }
                    // =========================================================================

                    if (fCustomChatLog != nullptr) {
                        fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                    }

                    ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(selectedItem);
                    if (chanItem != nullptr && chanItem->HasUnread()) {
                        chanItem->SetUnread(false);
                        fChannelTree->InvalidateItem(selectedIdx);
                    }
                    
                    fChatLog->SetText("");
                    if (fCustomChatLog != nullptr) {
                        fCustomChatLog->ClearAllLines();
                    }

                    while (fUserList->CountItems() > 0) {
                        delete fUserList->RemoveItem((int32)0);
                    }

                    fTopicView->SetText("No topic set.");

                    ServerTreeItem* isServerRootNode = dynamic_cast<ServerTreeItem*>(selectedItem);
                    BString itemText(selectedItem->Text());
                    
                    if (isServerRootNode != nullptr) {
                        fTopicView->SetText("Network connection logs status feed.");
                    } else {
                        BString cleanedItemText = itemText;
                        cleanedItemText.Trim();
                        while (cleanedItemText.StartsWith("@") || cleanedItemText.StartsWith("+")) {
                            cleanedItemText.Remove(0, 1);
                        }

                        if (cleanedItemText.StartsWith("#") || cleanedItemText.StartsWith("&")) {
                            bool topicFoundInCache = false;
                            for (auto it = fChannelTopics.begin(); it != fChannelTopics.end(); ++it) {
                                if (it->first != nullptr) {
                                    BString cachedNodeText = it->first->Text();
                                    cachedNodeText.Trim();
                                    while (cachedNodeText.StartsWith("@") || cachedNodeText.StartsWith("+")) {
                                        cachedNodeText.Remove(0, 1);
                                    }

                                    if (cachedNodeText == cleanedItemText) {
                                        fTopicView->SetText(it->second.String());
                                        topicFoundInCache = true;
                                        break;
                                    }
                                }
                            }

                            if (!topicFoundInCache) {
                                fTopicView->SetText("No topic set.");
                            }
                        }

                        if (fChannelUsers.find(fActiveBufferItem) != fChannelUsers.end()) {
                            BObjectList<UserListItem, true>* userVector = fChannelUsers[fActiveBufferItem];
                            
                            if (userVector != nullptr) {
                                BFont userListFont;

                                fUserList->GetFont(&userListFont);
                                userListFont.SetSize(cfg.userListFontSize);

                                for (int32 i = 0; i < userVector->CountItems(); i++) {
                                    UserListItem* cachedItem = userVector->ItemAt(i);
                                    if (cachedItem != nullptr) {
                                        bool userIsAway = cachedItem->IsAway();
                                        UserListItem* newUserItem = new UserListItem(cachedItem->Text(), userIsAway);
                                        
                                        // Use the dynamically resolved target font size we computed earlier
                                        fUserList->AddItem(newUserItem);
                                        
                                        BFont componentUserFont;
                                        fUserList->GetFont(&componentUserFont);
                                        // Crucial: Use our target size calculated in the synchronization pass
                                        componentUserFont.SetSize(targetUserListFontSize);
                                        newUserItem->Update(fUserList, &componentUserFont);
                                    }
                                }
                                // Sort Channel Operators
                                fUserList->SortItems(SortUsersByRank); 
                                
                                fUserList->InvalidateLayout();
                                fUserList->Invalidate();
                            }
                        }
                    }

                    // 3. Re-populate visible chat log panel via Rebuild routing block ---
                    if (fTextBuffers.find(fActiveBufferItem) != fTextBuffers.end()) {
                        this->RebuildActiveChannelBuffer();
                    } else {
                        fChatLog->SetText("");
                        if (fCustomChatLog != nullptr) {
                            fCustomChatLog->ClearAllLines();
                        }
                    }
                    
                    // --- NEW: DYNAMIC PICKER PANEL TOOLBAR SYNCHRONIZATION ---
                    if (fEmoticonGrid != nullptr && fCurrentServerNode != nullptr) {
                        bool emotesActive = true;
                        BString currentServerName(fCurrentServerNode->Text());

                        // 1. Scan standard servers array
                        for (const auto& srv : cfg.servers) {
                            if (BString(srv.name.c_str()) == currentServerName) {
                                emotesActive = srv.enableEmoticons;
                                break;
                            }
                        }

                        // 2. Scan custom servers array if not found
                        for (const auto& srv : cfg.customServers) {
                            if (BString(srv.name.c_str()) == currentServerName) {
                                emotesActive = srv.enableEmoticons;
                                break;
                            }
                        }

                        // 3. Resolve active custom draw engine state for this network item pass
                        ServerConfig* activeSrv = nullptr;
                        if (selectedConfig < (int32)cfg.servers.size()) {
                            activeSrv = &cfg.servers[selectedConfig];
                        } else if ((size_t)(selectedConfig - cfg.servers.size()) < cfg.customServers.size()) {
                            activeSrv = &cfg.customServers[selectedConfig - cfg.servers.size()];
                        }
                        bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;

                        // 4. ROBUST VISIBILITY CHECK: Hide if emotes are off OR if the draw engine itself is disabled
                        if (emotesActive && drawEngineActive) {
                            if (fIconToggleButton->IsHidden()) {
                                fIconToggleButton->Show();
                            }
                        } else {
                            if (!fIconToggleButton->IsHidden()) {
                                fIconToggleButton->Hide();
                            }
                        }

                        // 5. FORCE INSTANT RUNTIME LAYOUT RE-FLOW CALCULATIONS
                        this->InvalidateLayout(true);
                        this->Layout(true); // Forces Haiku to redraw and re-flow widgets immediately
                    }
                    // --- END OF PICKER PANEL RE-FLOW PATCH ---

                    
                    
                    // =========================================================================
                    // --- INSERTION POINT: DYNAMIC DRAW ENGINE VIEW SWAPPING PASS ---
                    // =========================================================================
                    // 1. Resolve our exact server state profile target pointer
                    ServerConfig* activeSrv = nullptr;
                    if (selectedConfig < (int32)cfg.servers.size()) {
                        activeSrv = &cfg.servers[selectedConfig];
                    } else if ((size_t)(selectedConfig - cfg.servers.size()) < cfg.customServers.size()) {
                        activeSrv = &cfg.customServers[selectedConfig - cfg.servers.size()];
                    }

                    bool currentDrawState = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
                    
                    BView* standardChatScroll = fChatLog->Parent();       
                    BView* customChatScroll = fCustomChatLog->Parent(); 
                    BLayout* containerLayout = fChatContainer->GetLayout();

                    if (containerLayout != nullptr && standardChatScroll != nullptr && customChatScroll != nullptr) {
                        if (currentDrawState) {
                            if (standardChatScroll->Parent() == fChatContainer) {
                                containerLayout->RemoveView(standardChatScroll);
                            }
                            if (customChatScroll->Parent() != fChatContainer) {
                                containerLayout->AddView(customChatScroll);
                            }
                        } else {
                            if (customChatScroll->Parent() == fChatContainer) {
                                containerLayout->RemoveView(customChatScroll);
                            }
                            if (standardChatScroll->Parent() != fChatContainer) {
                                containerLayout->AddView(standardChatScroll);
                            }
                        }
                        fChatContainer->InvalidateLayout(true);
                    }
                    // =========================================================================


                    // Force the standard scrollbar viewport directly back down to the bottom
                    // MODIFIED: Uses contextual engine choice variable instead of global config flag
                    if (!currentDrawState) {
                        int32 newLength = fChatLog->TextLength();
                        fChatLog->Select(newLength, newLength);
                        fChatLog->ScrollToSelection();
                    } else if (fCustomChatLog != nullptr) {
                        fCustomChatLog->RecalculateAllLineWraps();
                        fCustomChatLog->Invalidate();
                    }
                }
            }
            break;
        }






            case MSG_CONNECT_CUSTOM_SERVER: { 
                ServerTreeItem* customNode = nullptr;
                if (message->FindPointer("server_item", (void**)&customNode) == B_OK && customNode != nullptr) {
                    ConnectToServer(customNode);
                }
                break;
            }        	
        	
            case MSG_CONNECT_LIBERA:
                ConnectToServer(fLiberaNode);
                break;

            case MSG_CONNECT_OFTC:
                ConnectToServer(fOftcNode);
                break;

            case MSG_START_SIRC:
                ConnectToServer(fOftcNode); 
                break;

        case MSG_SEND_MESSAGE: {
            BString text = fInputControl->Text();
            if (text.Length() > 0) {
                BString activeTarget = "";
                ServerTreeItem* contextServer = nullptr;
                bool isServerLogTab = false; // Tracks if we are looking at a server status feed

                int32 selectedIdx = fChannelTree->CurrentSelection();
                if (selectedIdx >= 0) {
                    BStringItem* selectedItem = dynamic_cast<BStringItem*>(fChannelTree->ItemAt(selectedIdx));
                    if (selectedItem != nullptr) {
                        BString itemText(selectedItem->Text());
                        
                        // --- Strip out dynamic activity suffix tags before matching IRC parameters ---
                        int32 tagPos = itemText.FindFirst(" [");
                        if (tagPos != B_ERROR) itemText.Truncate(tagPos);

                        // Extract custom class types directly from the selected sidebar tree item
                        ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(selectedItem);
                        ServerTreeItem* servItem = dynamic_cast<ServerTreeItem*>(selectedItem);

                        if (chanItem != nullptr) {
                            // CONDITION 1: It is either a #channel OR a private message query leaf node
                            activeTarget = itemText; 
                            isServerLogTab = false; // Private chats are NOT server status logs!

                            // Climb the tree safely to find the parent server connection context pointer
                            BListItem* parentItem = fChannelTree->Superitem(selectedItem);
                            if (parentItem != nullptr) {
                                contextServer = dynamic_cast<ServerTreeItem*>(parentItem);
                            }
                        } else if (servItem != nullptr || itemText.FindFirst(".") != B_ERROR) {

                            activeTarget = ""; 
                            isServerLogTab = true; // This IS a server status log view node!
                            contextServer = servItem;

                            // Fallback safety if checking old dot-notated server strings
                            if (contextServer == nullptr) {
                                BListItem* parentItem = fChannelTree->Superitem(selectedItem);
                                if (parentItem != nullptr) {
                                    contextServer = dynamic_cast<ServerTreeItem*>(parentItem);
                                }
                            }
                        }
                    }
                }

                
                // 1. Fallback safety constraint mapping
                if (contextServer == nullptr) {
                    contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                }
                
                // 2. Dynamic lookup matching from our socket array map
                BSecureSocket* activeSocket = nullptr;
                if (fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }
                
                if (activeSocket != nullptr) {
                    BString rawPayload;
                    
                    // =========================================================================
                    // SLASH COMMAND INTERPRETER ENGINE
                    // =========================================================================
                    if (text.StartsWith("/")) {
                        BString commandLine = text;
                        commandLine.Remove(0, 1); // strip the '/'                            
                        
                        if (commandLine.ICompare("clear", 5) == 0) {
                            if (fActiveBufferItem != nullptr) {
                                fTextBuffers[fActiveBufferItem] = "";
                                fChatLog->SetText("");
                                
                                // --- Clear your custom text row canvas vectors instantly ---
                                if (fCustomChatLog != nullptr) {
                                    fCustomChatLog->ClearAllLines();
                                }
                            }
                        }
                        
                        else if (commandLine.ICompare("me ", 3) == 0) {
                            commandLine.Remove(0, 3); // strip "me " keyword and space
                            
                            if (activeTarget.Length() > 0 && commandLine.Length() > 0) {
                                rawPayload << "PRIVMSG " << activeTarget << " :" "\x01" "ACTION " << commandLine << "\x01\r\n";
                                
                                BString timestampPrefix = "";
                                bigtime_t currentTime = real_time_clock_usecs();
                                time_t rawTime = (time_t)(currentTime / 1000000);
                                struct tm* timeInfo = localtime(&rawTime);
                                if (timeInfo != nullptr) {
                                    char timeBuffer[32];
                                    strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M] ", timeInfo);
                                    timestampPrefix = timeBuffer;
                                }

                                BString echoStr;
                                echoStr << timestampPrefix << "* " << fMyNick << " " << commandLine << "\n";
                                LogToItemBuffer(fActiveBufferItem, echoStr);
                            } else {
                                BString warning = "System Error: You can only use /me actions inside a channel room leaf node.\n";
                                LogToItemBuffer(fActiveBufferItem, warning);
                            }
                        }
                        
                        else if (commandLine.ICompare("topic ", 6) == 0) {
                            commandLine.Remove(0, 6); // strip "topic " keyword and space
                            commandLine.Trim();

                            if (activeTarget.StartsWith("#")) {
                                if (commandLine.Length() > 0) {
                                    // Construct and send the standard IRC TOPIC payload format
                                    rawPayload << "TOPIC " << activeTarget << " :" << commandLine << "\r\n";
                                } else {
                                    BString warning = "Usage: /topic <New channel topic content string>\n";
                                    LogToItemBuffer(fActiveBufferItem, warning);
                                }
                            } else {
                                BString warning = "System Error: You can only set a topic inside a channel room leaf node.\n";
                                LogToItemBuffer(fActiveBufferItem, warning);
                            }
                        }

                        else if (commandLine.ICompare("msg ", 4) == 0) {
                            commandLine.Remove(0, 4); // strip "msg " keyword
                            int32 firstSpace = commandLine.FindFirst(" ");
                            
                            if (firstSpace != B_ERROR) {
                                BString dmTarget, msgBody;
                                commandLine.CopyInto(dmTarget, 0, firstSpace);
                                commandLine.CopyInto(msgBody, firstSpace + 1, commandLine.Length() - (firstSpace + 1));
                                
                                rawPayload << "PRIVMSG " << dmTarget << " :" << msgBody << "\r\n";
                                
                                BString echoStr;
                                echoStr << "-> To " << dmTarget << ": " << msgBody << "\n";
                                LogToItemBuffer(fActiveBufferItem, echoStr);
                            }
                        } else if (commandLine.ICompare("list", 4) == 0) {
  
                            bool windowIsValid = false;
                            if (fActiveListWindow != nullptr) {
                                BMessenger messenger(fActiveListWindow);
                                if (messenger.IsValid()) {
                                    windowIsValid = true;
                                } else {
                                    fActiveListWindow = nullptr;
                                }
                            }

                            if (fActiveListWindow == nullptr || !windowIsValid) {
                                fActiveListWindow = new IRCChannelListWindow(this, activeSocket, contextServer, &fActiveListWindow);
                                fActiveListWindow->Show();
                            } else {
                                if (fActiveListWindow->Lock()) {
                                    fActiveListWindow->Activate(true);
                                    fActiveListWindow->Unlock();
                                }
                            }
                        } else {
                            rawPayload << commandLine << "\r\n";
                        }
                    } else {
                        // =========================================================================
                        // REGULAR CHAT TEXT ROUTING ENGINE (PLAIN CHAT SUBMISSION)
                        // =========================================================================
                        
                        if (isServerLogTab) {
                            BString warning = "System Error: Use slash commands (like /JOIN) when typing inside the server status log.\n";
                            LogToItemBuffer(fActiveBufferItem, warning);
                        } else if (activeTarget.Length() > 0) {
                            // Construct the raw outgoing socket network string package stream
                            rawPayload << "PRIVMSG " << activeTarget << " :" << text << "\r\n";
                            
                            // Generate localized chat timestamp prefix bounds
                            BString timestampPrefix = "";
                            bigtime_t currentTime = real_time_clock_usecs();
                            time_t rawTime = (time_t)(currentTime / 1000000);
                            struct tm* timeInfo = localtime(&rawTime);
                            if (timeInfo != nullptr) {
                                char timeBuffer[32];
                                strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M] ", timeInfo);
                                timestampPrefix = timeBuffer;
                            }


                            BString echoStr;
                            echoStr << timestampPrefix << "<" << fMyNick << "> " << text << "\n";
                            LogToItemBuffer(fActiveBufferItem, echoStr);
                        }
                    }

                    if (rawPayload.Length() > 0) {
                        activeSocket->Write(rawPayload.String(), rawPayload.Length());
                        if (cfg.debugEnable) {
                            LogDebugStream(contextServer->Text(), "OUTGOING", rawPayload.String(), rawPayload.Length());
                        }
                    }
                }
            }
            fInputControl->SetText("");
            fInputControl->MakeFocus(true);
            break;
        }




        case MSG_IRC_RECEIVED: {
            BString rawLine;
            // 1. Intercept incoming raw text payload line
            if (message->FindString("text", &rawLine) == B_OK) {
                
                // 2. Declare a local, scope-isolated node context variable.
                // This eliminates cross-talk when multiple network threads drop lines simultaneously.
                void* nodePtr = nullptr;
                ServerTreeItem* targetServerNode = nullptr;

                if (message->FindPointer("server_node", (void**)&nodePtr) == B_OK && nodePtr != nullptr) {
                    targetServerNode = static_cast<ServerTreeItem*>(nodePtr);
                    
                    // Optional: Synchronize your fallback tracker safely if needed elsewhere
                    fCurrentServerNode = targetServerNode; 
                } else {
                    // Fallback to absolute baseline if network metadata thread signatures fail
                    targetServerNode = fCurrentServerNode != nullptr ? fCurrentServerNode : fLiberaNode;
                }
                
                // 3. Forward the scope-isolated immutable context directly into parsing architecture
                ParseAndDisplayIRC(rawLine, targetServerNode);
            }
            break;
        }




            default:
                BWindow::MessageReceived(message);
                break;
        }
    }


private:

BString
GetCleanNickname(const char* rawNick)
{
    BString cleanNick(rawNick);
    while (cleanNick.StartsWith("~") || cleanNick.StartsWith("&") || 
           cleanNick.StartsWith("%") || cleanNick.StartsWith("@") || 
           cleanNick.StartsWith("+")) {
        cleanNick.Remove(0, 1);
    }
    return cleanNick;
}



BSecureSocket*
GetActiveSocket(ServerTreeItem* contextServer)
{
    if (contextServer == nullptr)
        return nullptr;

    if (fServerSockets.count(contextServer) > 0) {
        return fServerSockets[contextServer];
    }
    
    return (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
}


void WriteLogToFile(BStringItem* itemNode, const BString& rawLineText) {
    if (itemNode == nullptr || rawLineText.Length() == 0) return;

    // 1. Resolve our exact server state profile target pointer context
    size_t activeIdx = (size_t)selectedConfig;
    ServerConfig* activeSrv = nullptr;
    if (activeIdx < cfg.servers.size()) {
        activeSrv = &cfg.servers[activeIdx];
    } else if ((size_t)(activeIdx - cfg.servers.size()) < cfg.customServers.size()) {
        activeSrv = &cfg.customServers[activeIdx - cfg.servers.size()];
    }

    // Fast return if logging is completely turned off for this specific server context
    if (activeSrv == nullptr || !activeSrv->logChatsToFile) return;

    // 2. Build and guarantee that the 'cricket/logs/' settings directory exists layout safely
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK) return;
    
    path.Append("cricket/logs");
    create_directory(path.Path(), 0755); // Native Haiku folder generator utility

    // 3. Clean up the target filename string safely 
    // Replaces network name slash blocks or special channel tags to create clean filenames
    BString serverNameClean = activeSrv->name.c_str();
    serverNameClean.ReplaceAll("/", "_");
    serverNameClean.ReplaceAll(" ", "_");

    BString channelNameClean = itemNode->Text();
    channelNameClean.ReplaceAll("/", "_"); // Strip illegal path tokens

    BString fullFileName;
    fullFileName.SetToFormat("%s_%s.log", serverNameClean.String(), channelNameClean.String());
    path.Append(fullFileName.String());

    // 4. Generate a clean [YYYY-MM-DD HH:MM:SS] timestamp tag
    time_t now = time(nullptr);
    struct tm* tstruct = localtime(&now);
    char timeBuffer[40];
    strftime(timeBuffer, sizeof(timeBuffer), "[%Y-%m-%d %H:%M:%S] ", tstruct);

    // 5. Open and append the stream data straight onto disk
    std::ofstream logFile(path.Path(), std::ios_base::app);
    if (logFile.is_open()) {
        BString cleanedText = rawLineText;
        cleanedText.Trim(); // Remove trailing system returns before storing
        
        logFile << timeBuffer << cleanedText.String() << "\n";
        logFile.close();
    }
}



void UpdateMyGlobalAwayState(ServerTreeItem* contextServer, bool isAway)
{
    for (auto it = fChannelUsers.begin(); it != fChannelUsers.end(); ++it) {
        ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(it->first);
        if (chanNode != nullptr) {
            // Safety check: ensure we only update channels that belong to the current server
            if (fChannelTree->Superitem(chanNode) != contextServer) continue;
            
            UpdateUserAwayState(chanNode, fMyNick.String(), isAway);
        }
    }
    
    // Instantly refresh the right-hand nickname list panel if we modified the active view
    RefreshUserListUI();
}


    BOutlineListView* fChannelTree;
    ServerTreeItem*   fLiberaNode;
    ServerTreeItem*   fOftcNode;
    ServerTreeItem*   fCurrentServerNode; 
    BString           fMyNick;
    BStringItem*      fActiveBufferItem;
    
    std::map<BStringItem*, BString> fTextBuffers;
    std::map<BStringItem*, BString> fChannelTopics;
    
    std::map<BStringItem*, BObjectList<UserListItem, true>*> fChannelUsers;
    std::map<BStringItem*, bigtime_t> fLastTimestampTime;
    std::map<ServerTreeItem*, thread_id> fServerThreads;
    std::map<ServerTreeItem*, BSecureSocket*> fServerSockets;
	std::map<ServerTreeItem*, int32> fNickAttempts;

	BTextControl*     fTopicView;
    BTextView*        fChatLog;
    BListView*        fUserList;
    BTextControl*     fInputControl;
    
    thread_id         fLiberaThread;
    thread_id         fOftcThread;
    BSecureSocket*    fLiberaSocket;
    BSecureSocket*    fOftcSocket;
    
    IRCChannelListWindow* fActiveListWindow;

    BMenuField*         fServerListFontMenu;
    BMenuField*         fChatLogFontMenu;
    BMenuField*         fUserListFontMenu;

    BMenuField*         CreateFontMenu(const char* label, int32 currentSize);
	BCheckBox*          fHideStatusCheck;	

    BView*           fChatContainer; 
    CustomChatView*  fCustomChatLog; 
    bool fIsLoadingHistory = false; 

	BObjectList<BString, true> fAutoOpList;
	BObjectList<BString, true> fCurrentBanHarvest;
	
 	BGridView*    fEmoticonGrid;
    BButton*    fIconToggleButton;  
	BWindow*    fActiveIconPopup; 
}; 

class Cricket : public BApplication {
public:
    Cricket() : BApplication("application/x-vnd.cricket") {}
    void ReadyToRun() override {
        CricketWindow* window = new CricketWindow();
        window->Show();
    }
};

int main() {
    Cricket app;
    app.Run();
    return 0;
}
