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
#include <MessageFilter.h>
#include <Clipboard.h>
#include <TabView.h>
#include <StringView.h>
#include <SeparatorView.h>
#include <ColorControl.h>

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
#include <ListItem.h>
#include <ObjectList.h>
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
#include <vector>
#include <algorithm>

// Data Types, Sockets & Networking Kit Headers
#include <String.h>
#include <NetworkAddress.h>
#include <SecureSocket.h>
#include <Socket.h>
#include <NetEndpoint.h>
#include <openssl/ssl.h>
#include <openssl/err.h>



// Standard C/C++ STL & POSIX Includes
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <map>
#include "nlohmann/json.hpp"


// Define application messages
enum {
	MSG_CONTEXT_ADD_COLOR = 'cccl',
	MSG_TRIGGER_REPLIES_SEARCH = 'trrs',
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
    MSG_CONTEXT_SHOW_MODES = 'mCSM',
    MSG_GENERATE_CERTFP_KEYPAIR = 'gcfp',
    MSG_AUTO_REGISTER_FINGERPRINT = 'argf',
};



// Forward Declarations 
class CricketWindow; 
class ServerTreeItem; 
#include <map>
static std::map<void*, SSL*> gServerSslHandles;
static std::map<void*, int>  gServerRawSockets;



namespace AppInfo {
    static const char* const VERSION_STRING = "Cricket IRC Client v.0.0.31 (Haiku OS)";
}

using json = nlohmann::json;

const std::string DEFAULT_BG_PATH = "";

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
    bool enableColorCodes = true;
    std::vector<std::string> ignoredNicks; 
    std::vector<std::string> nickColors;
    std::vector<rgb_color>   nickColorValues;
    int32 timestampInterval = 30;
    bool useSASL = false;
    std::string saslUser;
    bool useCertFP = false;      
    std::string certProfileName; 
    std::string certFileName;   
    std::string keyFileName;    
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
    bool useCustomDrawFunction = true;
    int32 timestampInterval = 30;
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
    
    // --- STRIP THE VERSION SUFFIX BEFORE SAVING ---
    std::string cleanQuitMsg = cfg.quitMessage;
    size_t suffixPos = cleanQuitMsg.find(" [Cricket IRC Client");
    if (suffixPos != std::string::npos) {
        cleanQuitMsg = cleanQuitMsg.substr(0, suffixPos); 
    }
    j["quitMessage"] = cleanQuitMsg; 
    j["awayMessage"] = cfg.awayMessage;
    j["useCustomDrawFunction"] = cfg.useCustomDrawFunction;
    
    // 1. Save standard/default servers
    json serverArray = json::array();
    for (auto& srv : cfg.servers) {
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
        s["timestampInterval"] = srv.timestampInterval;
        
        s["use_sasl"] = srv.useSASL;
        s["sasl_user"] = srv.saslUser;
        s["use_certfp"] = srv.useCertFP;
        s["cert_profile_name"] = srv.certProfileName;
        s["cert_file_name"] = srv.certFileName;
        s["key_file_name"]  = srv.keyFileName;
        
        if (srv.backgroundImagePath.empty()) {
            srv.backgroundImagePath = DEFAULT_BG_PATH;
        }
        s["background_image"] = srv.backgroundImagePath; 
        
        s["bg_opacity"] = srv.backgroundOpacity; 
        s["enable_emoticons"] = srv.enableEmoticons; 
        s["useCustomDrawFunction"] = srv.useCustomDrawFunction;
        s["logChatsToFile"] = srv.logChatsToFile;
        s["enableColorCodes"] = srv.enableColorCodes; 
		
        s["serverListFontSize"] = srv.serverListFontSize;
        s["chatLogFontSize"] = srv.chatLogFontSize;
        s["userListFontSize"] = srv.userListFontSize;

        json ajArray = json::array();
        for (const auto& chan : srv.autojoin) {
            ajArray.push_back(chan);
        }
        s["autojoin"] = ajArray;

        json ignoreArray = json::array();
        for (const auto& nick : srv.ignoredNicks) {
            ignoreArray.push_back(nick);
        }
        s["ignored_nicks"] = ignoreArray; 

        json colorNickArray = json::array();
        for (const auto& name : srv.nickColors) {
            colorNickArray.push_back(name);
        }
        s["nick_color_names"] = colorNickArray;

        json colorValueArray = json::array();
        for (const auto& color : srv.nickColorValues) {
            json rgbObj;
            rgbObj["r"] = color.red;
            rgbObj["g"] = color.green;
            rgbObj["b"] = color.blue;
            colorValueArray.push_back(rgbObj);
        }
        s["nick_color_values"] = colorValueArray;

        serverArray.push_back(s);
    }
    j["servers"] = serverArray;

    // 2. Save custom servers into their own isolated JSON array
    json customServerArray = json::array();
    for (auto& srv : cfg.customServers) {
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
        
        s["use_sasl"] = srv.useSASL;
        s["sasl_user"] = srv.saslUser;
        s["use_certfp"] = srv.useCertFP;
        s["cert_profile_name"] = srv.certProfileName;
        s["cert_file_name"] = srv.certFileName;
        s["key_file_name"]  = srv.keyFileName;
        
        if (srv.backgroundImagePath.empty()) {
            srv.backgroundImagePath = DEFAULT_BG_PATH;
        }
        s["background_image"] = srv.backgroundImagePath;
        
        s["bg_opacity"] = srv.backgroundOpacity; 
        s["enable_emoticons"] = srv.enableEmoticons;  
        s["useCustomDrawFunction"] = srv.useCustomDrawFunction;
        s["logChatsToFile"] = srv.logChatsToFile;
        s["enableColorCodes"] = srv.enableColorCodes;   
 	    
        s["serverListFontSize"] = srv.serverListFontSize;
        s["chatLogFontSize"] = srv.chatLogFontSize;
        s["userListFontSize"] = srv.userListFontSize;

        json ajArray = json::array();
        for (const auto& chan : srv.autojoin) {
            ajArray.push_back(chan);
        }
        s["autojoin"] = ajArray;

        json ignoreArray = json::array();
        for (const auto& nick : srv.ignoredNicks) {
            ignoreArray.push_back(nick);
        }
        s["ignored_nicks"] = ignoreArray; 

        json colorNickArray = json::array();
        for (const auto& name : srv.nickColors) {
            colorNickArray.push_back(name);
        }
        s["nick_color_names"] = colorNickArray;

        json colorValueArray = json::array();
        for (const auto& color : srv.nickColorValues) {
            json rgbObj;
            rgbObj["r"] = color.red;
            rgbObj["g"] = color.green;
            rgbObj["b"] = color.blue;
            colorValueArray.push_back(rgbObj);
        }
        s["nick_color_values"] = colorValueArray;

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

    // Ensure the fallback path constant is accessible
    const std::string DEFAULT_BG_PATH = "";
	
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket/cricketConfig.txt");
        std::ifstream infile(path.Path());        
        if (infile.is_open()) {
            try {
                json j = json::parse(infile);
                
                // ---  DYNAMIC VERSION APFENDER AT BOOT ---
                // 1. Fetch the raw saved string value from the json map
                BString savedQuit = j.value("quitMessage", "App Quit").c_str();
                
                // 2. Clear out any legacy hardcoded string formats (like "App Quit: Cricket IRC Client...")
                int32 legacyColonIdx = savedQuit.FindFirst(": Cricket IRC Client");
                if (legacyColonIdx != B_ERROR) {
                    savedQuit.Truncate(legacyColonIdx);
                }
                int32 legacyBracketIdx = savedQuit.FindFirst(" [Cricket IRC Client");
                if (legacyBracketIdx != B_ERROR) {
                    savedQuit.Truncate(legacyBracketIdx);
                }
                savedQuit.Trim();
                
                // 3. Combine user text with the active version string
                BString finalQuitMsg;
                finalQuitMsg << savedQuit << " [" << AppInfo::VERSION_STRING << "]";
                cfg.quitMessage = finalQuitMsg.String();
                // -----------------------------------------------

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
                        srv.timestampInterval = s.value("timestampInterval", cfg.timestampInterval);
                        
                        srv.backgroundImagePath = s.value("background_image", DEFAULT_BG_PATH); 
                        if (srv.backgroundImagePath.empty()) {
                            srv.backgroundImagePath = DEFAULT_BG_PATH;
                        }

                        srv.backgroundOpacity = s.value("bg_opacity", 30); 
                        srv.enableEmoticons = s.value("enable_emoticons", true); 
                        srv.useCustomDrawFunction = s.value("useCustomDrawFunction", cfg.useCustomDrawFunction);
                        srv.logChatsToFile = s.value("logChatsToFile", false);
                        srv.enableColorCodes = s.value("enableColorCodes", true);
                        srv.useSASL = s.value("use_sasl", false);
                        srv.saslUser = s.value("sasl_user", "");
                        srv.useCertFP = s.value("use_certfp", false);
                        srv.certProfileName = s.value("cert_profile_name", "");                        
                        srv.certFileName = s.value("cert_file_name", "");                    
                        srv.keyFileName = s.value("key_file_name", "");   


                        
                        srv.serverListFontSize = s.value("serverListFontSize", cfg.serverListFontSize);
                        srv.chatLogFontSize    = s.value("chatLogFontSize", cfg.chatLogFontSize);
                        srv.userListFontSize   = s.value("userListFontSize", cfg.userListFontSize);

                        if (s.contains("autojoin") && s["autojoin"].is_array()) {
                            for (const auto& chan : s["autojoin"]) {
                                srv.autojoin.push_back(chan.get<std::string>());
                            }
                        }

                        if (s.contains("ignored_nicks") && s["ignored_nicks"].is_array()) {
                            for (const auto& nick : s["ignored_nicks"]) {
                                srv.ignoredNicks.push_back(nick.get<std::string>());
                            }
                        }
                        
                         // --- NEW: DESERIALIZE NICKNAME COLORS ---
                        srv.nickColors.clear();
                        if (s.contains("nick_color_names") && s["nick_color_names"].is_array()) {
                            for (const auto& name : s["nick_color_names"]) {
                                srv.nickColors.push_back(name.get<std::string>());
                            }
                        }

                        srv.nickColorValues.clear();
                        if (s.contains("nick_color_values") && s["nick_color_values"].is_array()) {
                            for (const auto& colorVal : s["nick_color_values"]) {
                                rgb_color c;
                                c.red   = colorVal.value("r", 255);
                                c.green = colorVal.value("g", 255);
                                c.blue  = colorVal.value("b", 255);
                                c.alpha = 255;
                                srv.nickColorValues.push_back(c);
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
                        
                        srv.backgroundImagePath = s.value("background_image", DEFAULT_BG_PATH); 
                        if (srv.backgroundImagePath.empty()) {
                            srv.backgroundImagePath = DEFAULT_BG_PATH;
                        }

                        srv.backgroundOpacity = s.value("bg_opacity", 30); 
                        srv.enableEmoticons = s.value("enable_emoticons", true); 
                        srv.useCustomDrawFunction = s.value("useCustomDrawFunction", cfg.useCustomDrawFunction);
                        srv.logChatsToFile = s.value("logChatsToFile", false);
                        srv.enableColorCodes = s.value("enableColorCodes", true);
                        srv.useSASL = s.value("use_sasl", false);
                        srv.saslUser = s.value("sasl_user", "");
                        srv.useCertFP = s.value("use_certfp", false);
                        srv.certProfileName = s.value("cert_profile_name", "");
                        srv.certFileName = s.value("cert_file_name", "");                    
                        srv.keyFileName = s.value("key_file_name", "");   
                        
                        srv.serverListFontSize = s.value("serverListFontSize", cfg.serverListFontSize);
                        srv.chatLogFontSize    = s.value("chatLogFontSize", cfg.chatLogFontSize);
                        srv.userListFontSize   = s.value("userListFontSize", cfg.userListFontSize);

                        if (s.contains("autojoin") && s["autojoin"].is_array()) {
                            for (const auto& chan : s["autojoin"]) {
                                srv.autojoin.push_back(chan.get<std::string>());
                            }
                        }

                        if (s.contains("ignored_nicks") && s["ignored_nicks"].is_array()) {
                            for (const auto& nick : s["ignored_nicks"]) {
                                srv.ignoredNicks.push_back(nick.get<std::string>());
                            }
                        }
                        
                        // --- NEW: DESERIALIZE NICKNAME COLORS ---
                        srv.nickColors.clear();
                        if (s.contains("nick_color_names") && s["nick_color_names"].is_array()) {
                            for (const auto& name : s["nick_color_names"]) {
                                srv.nickColors.push_back(name.get<std::string>());
                            }
                        }

                        srv.nickColorValues.clear();
                        if (s.contains("nick_color_values") && s["nick_color_values"].is_array()) {
                            for (const auto& colorVal : s["nick_color_values"]) {
                                rgb_color c;
                                c.red   = colorVal.value("r", 255);
                                c.green = colorVal.value("g", 255);
                                c.blue  = colorVal.value("b", 255);
                                c.alpha = 255;
                                srv.nickColorValues.push_back(c);
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
        
        // --- FIXED FALLBACK FORMAT MATCH ---
        BString defaultQuit;
        defaultQuit << "App Quit [" << AppInfo::VERSION_STRING << "]";
        cfg.quitMessage = defaultQuit.String();
        cfg.awayMessage = "I am away from my computer right now.";
		
        srand(static_cast<unsigned int>(real_time_clock_usecs()));
        int randomSuffix = 1000 + (rand() % 9000);
        BString dynamicNick;
        dynamicNick << "HaikuIRCUser" << randomSuffix;

        // 1. RECONSTRUCT LIBERA CHAT DEFAULT PROFILE
        ServerConfig libera;
        libera.name = "Libera Chat";
        libera.host = "irc.libera.chat";
        libera.port = 6697;
        libera.nick = dynamicNick.String();
        libera.altNick = BString(dynamicNick).Append("+").String(); 
        libera.altNick2 = BString(dynamicNick).Append("__").String();
        libera.pass = "";  
        libera.autoConnect = false;    
        libera.autoReconnect = false;  
       
        libera.useSASL = false;
        libera.saslUser = "";
        libera.useCertFP = false;
        libera.certProfileName = "";
        libera.certFileName = "";                    
        libera.keyFileName = "";   
        
        libera.autojoin = {""};
        libera.ignoredNicks = {}; 
        libera.nickColors = {};       
        libera.nickColorValues = {};  
        libera.timestampInterval = 30;
        
        libera.backgroundImagePath = DEFAULT_BG_PATH; 
        libera.backgroundOpacity = 30; 
        libera.enableEmoticons = true; 
        libera.useCustomDrawFunction = cfg.useCustomDrawFunction;
        libera.logChatsToFile = false;
        libera.enableColorCodes = true;
         
        libera.serverListFontSize = cfg.serverListFontSize;
        libera.chatLogFontSize    = cfg.chatLogFontSize;
        libera.userListFontSize   = cfg.userListFontSize;
        cfg.servers.push_back(libera);

        // 2. OFTC NETWORK DEFAULT PROFILE
        ServerConfig oftc;
        oftc.name = "OFTC";
        oftc.host = "irc.oftc.net";
        oftc.port = 6697;
        oftc.nick = dynamicNick.String();
        oftc.altNick = BString(dynamicNick).Append("+").String();
        oftc.altNick2 = BString(dynamicNick).Append("__").String();
        oftc.pass = ""; 
        oftc.autoConnect = false;
        oftc.autoReconnect = false;         
        oftc.useSASL = false;
        oftc.saslUser = "";
        oftc.useCertFP = false;
        oftc.certProfileName = "";
        oftc.certFileName = "";                    
        oftc.keyFileName = "";          
        
        oftc.autojoin = {""};
        oftc.ignoredNicks = {};
        oftc.nickColors = {};       
        oftc.nickColorValues = {};  
		oftc.timestampInterval = 30;
		
        oftc.backgroundImagePath = DEFAULT_BG_PATH;
        oftc.backgroundOpacity = 30;
        oftc.enableEmoticons = true;
        oftc.useCustomDrawFunction = cfg.useCustomDrawFunction;
        oftc.logChatsToFile = false;
        oftc.enableColorCodes = true;

        oftc.serverListFontSize = cfg.serverListFontSize;
        oftc.chatLogFontSize    = cfg.chatLogFontSize;
        oftc.userListFontSize   = cfg.userListFontSize;
        cfg.servers.push_back(oftc);
    }
}

// Lightweight, case-insensitive shell-style wildcard matcher (* and ?)
static bool MatchWildcard(const char* stringToTest, const char* wildcardPattern) {
    if (stringToTest == nullptr || wildcardPattern == nullptr) return false;

    // Loop through both strings simultaneously
    while (*wildcardPattern) {
        if (*wildcardPattern == '*') {
            // Consecutive wildcards can be compressed into a single match pass
            while (*wildcardPattern == '*') {
                wildcardPattern++;
            }
            // If the wildcard string ends in a trailing '*', it is an instant match prefix success
            if (!*wildcardPattern) return true;
            
            // Scan through stringToTest to look for next matching segment
            while (*stringToTest) {
                if (MatchWildcard(stringToTest, wildcardPattern)) return true;
                stringToTest++;
            }
            return false;
        } 
        else if (*wildcardPattern == '?' || 
                 tolower(static_cast<unsigned char>(*stringToTest)) == tolower(static_cast<unsigned char>(*wildcardPattern))) {
            // Character match or single-character wildcard match, step forward
            if (!*stringToTest) return false;
            stringToTest++;
            wildcardPattern++;
        } 
        else {
            // Explicit character mismatch
            return false;
        }
    }
    // Match is true only if both pointers reached their terminators at the same time
    return !*stringToTest;
}


class UserListItem : public BStringItem {
public:
	UserListItem(const char* nickname, bool isAway = false)
	    : BStringItem(""), fIsAway(isAway), fIsOwner(false), fIsAdmin(false), fIsOp(false), fIsHalfOp(false), fIsVoiced(false) {
	    
	    BString rawNick = nickname;
	    rawNick.Trim();
	    
	    // GREEDY PREFIX SCAN: Keep stripping until no symbols remain
	    bool mapping = true;
	    while (rawNick.Length() > 0 && mapping) {
	        char c = rawNick.ByteAt(0);
	        mapping = true;
	        if (c == '~')      fIsOwner = true;
	        else if (c == '&') fIsAdmin = true;
	        else if (c == '@') fIsOp = true;
	        else if (c == '%') fIsHalfOp = true;
	        else if (c == '+') fIsVoiced = true;
	        else               mapping = false; // Hit the actual name
	
	        if (mapping) rawNick.Remove(0, 1);
	    }
	    fCleanNick = rawNick;
	    UpdateDisplay();
	}


    void SetAway(bool isAway) { fIsAway = isAway; }
    bool IsAway() const { return fIsAway; }

    const char* GetCleanNick() const { return fCleanNick.String(); }

    void SetMode(char modeChar, bool active) {
        if (modeChar == 'q') fIsOwner = active;
        else if (modeChar == 'a') fIsAdmin = active;
        else if (modeChar == 'o') fIsOp = active;
        else if (modeChar == 'h') fIsHalfOp = active;
        else if (modeChar == 'v') fIsVoiced = active;
        UpdateDisplay();
    }

    // Reconstruct the display string dynamically based on active modes hierarchy
    void UpdateDisplay() {
        BString displayString = "";
        if (fIsOwner) displayString << "~";
        else if (fIsAdmin) displayString << "&";
        else if (fIsOp) displayString << "@";
        else if (fIsHalfOp) displayString << "%";
        else if (fIsVoiced) displayString << "+";
        
        displayString << fCleanNick;
        SetText(displayString.String());
    }

    virtual void DrawItem(BView* owner, BRect frame, bool complete = false) override {
        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
        } else {
            owner->SetHighColor(ui_color(B_LIST_BACKGROUND_COLOR));
        }
        owner->FillRect(frame);

        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
        } else if (fIsAway) {
            rgb_color awayColor = { 140, 140, 140, 255 }; 
            owner->SetHighColor(awayColor);
        } else {
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
        }

        font_height fh;
        owner->GetFontHeight(&fh);
        float textHeight = fh.ascent + fh.descent;
        
        owner->MovePenTo(frame.left + 4, frame.top + fh.ascent + (frame.Height() - textHeight) / 2);
        owner->DrawString(Text());
    }
   // Helper to check if this user has any elevated administrative permissions
    bool HasOpPower() const {
        return fIsOwner || fIsAdmin || fIsOp; 
    }


private:
    BString fCleanNick;
    bool    fIsAway;
    
    // Explicit mode tracking flags
    bool    fIsOwner;  // ~
    bool    fIsAdmin;  // &
    bool    fIsOp;     // @
    bool    fIsHalfOp; // %
    bool    fIsVoiced; // +
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

    void AddStyledLine(BStringItem* itemNode, const BString& text, const text_run_array* runs);
    void ClearAllLines();    
    void SetLineHeight(float height);
    void RecalculateAllLineWraps();
    void SetActiveChannel(BStringItem* activeNode);
    void SetBackgroundImage(const char* filePath);
    void SetBackgroundDimming(int32 level); 

	virtual void KeyDown(const char* bytes, int32 numBytes) override;
	BString GetSelectedText(); 
    virtual void MessageReceived(BMessage* message); 
    virtual void MouseMoved(BPoint point, uint32 transit, const BMessage* dragMessage) override;
	virtual void MouseUp(BPoint point) override;
	virtual void MouseDown(BPoint point);
	virtual void Draw(BRect updateRect);
    virtual void FrameResized(float newWidth, float newHeight);
    int32 CountTotalLines() const;
    BString GetLineTextAt(int32 index) const;

private:
    void ParseTextAndIcons(const BString& text, const text_run_array* runs, BObjectList<StyledRunFragment, true>* rawFragments);
    void ComputeWrapForLine(StyledLine* line, float maxWidth, BObjectList<StyledRunFragment, true>* rawFragments);
    virtual void ScrollTo(BPoint point);
	void UpdateScrollRange(bool scrollToBottom = false);
    BObjectList<StyledLine, true> fLines; 
    BStringItem*                  fActiveChannelNode;
    float                         fLineHeight;
    BBitmap* fBackgroundBitmap;
    int32    fBackgroundDimmingLevel; 
    bool        fIsSelecting;       // Is the user actively dragging the mouse?
    BPoint      fSelectionStart;    // Mouse down starting point
    BPoint      fSelectionEnd;      // Current mouse dragging point
};



// Constructor
// Update Constructor to handle initial default allocation state 
CustomChatView::CustomChatView(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
    : BView(frame, name, resizingMode, flags | B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE | B_FULL_UPDATE_ON_RESIZE),
      fIsSelecting(false),      
      fSelectionStart(0, 0),
      fSelectionEnd(0, 0),
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


int32 CustomChatView::CountTotalLines() const {
    return fLines.CountItems();
}

BString CustomChatView::GetLineTextAt(int32 index) const {
    StyledLine* line = fLines.ItemAt(index);
    if (line != nullptr) {
        // Returns the clean unformatted text representation stored in struct
        return line->text; 
    }
    return BString("");
}



void CustomChatView::KeyDown(const char* bytes, int32 numBytes)
{
    if (Window() == nullptr) return;

    // Check if exactly one byte was captured (a standard alphanumeric keystroke)
    if (numBytes == 1) {
        char key = bytes[0];

        // Read the currently active system modifier keys (Shift, Alt, Ctrl, etc.)
        int32 modifiers = 0;
        Window()->CurrentMessage()->FindInt32("modifiers", &modifiers);

        // --- INTERCEPT CTRL+C OR COMMAND+C ---
        // Haiku maps its application shortcuts primarily via the B_COMMAND_KEY modifier flag
        if ((key == 'c' || key == 'C') && (modifiers & B_COMMAND_KEY) != 0) {
            
            BString selectedText = GetSelectedText();
            if (selectedText.Length() > 0) {
                
                // Native Haiku System Clipboard Transaction
                if (be_clipboard->Lock()) {
                    be_clipboard->Clear();
                    
                    BMessage* clipData = be_clipboard->Data();
                    if (clipData != nullptr) {
                        // Pack text selection as raw system character data bytes
                        clipData->AddData("text/plain", B_MIME_TYPE, selectedText.String(), selectedText.Length());
                        be_clipboard->Commit();
                    }
                    be_clipboard->Unlock();
                }
            }
            return; // Swallows the keystroke event so it doesn't leak into underlying views
        }
    }

    // Always pass unhandled keystrokes up to the base class to maintain scrollability
    BView::KeyDown(bytes, numBytes);
}


void CustomChatView::ParseTextAndIcons(const BString& text, const text_run_array* runs, BObjectList<StyledRunFragment, true>* rawFragments)
{
    if (rawFragments == nullptr) return;

    // Standard mIRC 16-color palette
    rgb_color ircPalette[] = {
        { 255, 255, 255, 255 }, { 0,   0,   0,   255 }, { 0,   0,   127, 255 }, { 0,   127, 0,   255 },
        { 255, 0,   0,   255 }, { 127, 0,   0,   255 }, { 127, 0,   127, 255 }, { 255, 127, 0,   255 },
        { 255, 255, 0,   255 }, { 0,   255, 0,   255 }, { 0,   127, 127, 255 }, { 0,   255, 255, 255 },
        { 0,   0,   255, 255 }, { 255, 0,   255, 255 }, { 127, 127, 127, 255 }, { 192, 192, 192, 255 }
    };

    // --- EXTRACT LIVE SERVER OPTION SETTINGS ---
    bool emoticonsEnabled = true;
    bool colorCodesEnabled = false; // Safe default baseline

    if (selectedConfig < (int)cfg.servers.size()) {
        emoticonsEnabled = cfg.servers[selectedConfig].enableEmoticons;
        colorCodesEnabled = cfg.servers[selectedConfig].enableColorCodes;
    } else {
        int customIdx = selectedConfig - cfg.servers.size();
        if (customIdx >= 0 && customIdx < (int)cfg.customServers.size()) {
            emoticonsEnabled = cfg.customServers[customIdx].enableEmoticons;
            colorCodesEnabled = cfg.customServers[customIdx].enableColorCodes;
        }
    }

    struct EmoteMap { const char* trigger; const uint8* data; };
    EmoteMap emotes[] = {
        {":)", (const uint8*)kIconSmile},      {":-)", (const uint8*)kIconSmile},
        {"^^", (const uint8*)kIconCheerful},   {"^_^", (const uint8*)kIconCheerful},
        {":D", (const uint8*)kIconLaughing},   {":-D", (const uint8*)kIconLaughing},
        {":d", (const uint8*)kIconLaughing},   {"lol", (const uint8*)kIconLaughing},  
        {"LOL", (const uint8*)kIconLaughing},  {":|", (const uint8*)kIconConfused},   
        {":-|", (const uint8*)kIconConfused},  {":/", (const uint8*)kIconConfused},   
        {":-\\", (const uint8*)kIconConfused}, {"o_O", (const uint8*)kIconConfused},  
        {"O_o", (const uint8*)kIconConfused},  {":(", (const uint8*)kIconFrown},      
        {":-(", (const uint8*)kIconFrown},     {">_ <", (const uint8*)kIconAnnoyed},  
        {">_(", (const uint8*)kIconAnnoyed},   {">_<", (const uint8*)kIconAnnoyed},
        {";'(", (const uint8*)kIconCrying},    {";-'(", (const uint8*)kIconCrying},
        {"T_T", (const uint8*)kIconCrying},    {";_;", (const uint8*)kIconCrying},
        {";)", (const uint8*)kIconWink},       {";-)", (const uint8*)kIconWink},
        {":P", (const uint8*)kIconTongue},     {":p", (const uint8*)kIconTongue},
        {":-P", (const uint8*)kIconTongue},    {":-p", (const uint8*)kIconTongue},
        {":o", (const uint8*)kIconAstonished}, {":O", (const uint8*)kIconAstonished},
        {":-o", (const uint8*)kIconAstonished},{":-O", (const uint8*)kIconAstonished},
        {":0", (const uint8*)kIconAstonished}, {":-0", (const uint8*)kIconAstonished},
        {"O_O", (const uint8*)kIconWideeyed},  {"o_o", (const uint8*)kIconWideeyed},
        {"8-)", (const uint8*)kIconSunglasses}, {"B)", (const uint8*)kIconSunglasses},
        {"8)", (const uint8*)kIconSunglasses},  {"b)", (const uint8*)kIconSunglasses},
        {"<3", (const uint8*)kIconHeart},       {":*", (const uint8*)kIconKiss},       
        {":-*", (const uint8*)kIconKiss},       {":X", (const uint8*)kIconFrown},      
        {":x", (const uint8*)kIconFrown}
    };
    int32 emoteCount = sizeof(emotes) / sizeof(emotes[0]);

    BFont baseFont;
    GetFont(&baseFont);

    rgb_color activeColor = ui_color(B_DOCUMENT_TEXT_COLOR);
    BFont activeFont = baseFont;
    activeFont.SetFace(B_REGULAR_FACE);

    int32 runCount = (runs != nullptr && runs->count > 0) ? runs->count : 1;
    
    for (int32 i = 0; i < runCount; i++) {
        int32 startPos = 0;
        int32 endPos = text.Length();
        
        if (runs != nullptr && runs->count > 0) {
            startPos = runs->runs[i].offset;
            activeFont = runs->runs[i].font;
            activeColor = runs->runs[i].color;
            if (i + 1 < runs->count) endPos = runs->runs[i+1].offset;
        }
        
        if (startPos >= text.Length() || endPos <= startPos) continue;

        BString runText;
        text.CopyInto(runText, startPos, endPos - startPos);
        int32 currentPos = 0;
        
        while (currentPos < runText.Length()) {
            int32 tagOpen = runText.FindFirst("[", currentPos);
            int32 nextTrigger = runText.Length();
            int32 triggerLength = 0;
            const uint8* foundIcon = nullptr;
            
            if (emoticonsEnabled) {
                for (int32 e = 0; e < emoteCount; e++) {
                    int32 pos = runText.FindFirst(emotes[e].trigger, currentPos);
                    if (pos != B_ERROR && pos < nextTrigger) {
                        bool isValidMatch = true;
                        int32 trigLen = strlen(emotes[e].trigger);
                        
                        if (emotes[e].trigger[0] == ':' && pos + 1 < runText.Length()) {
                            char nextChar = runText.ByteAt(pos + 1);
                            if (isdigit(nextChar)) isValidMatch = false;
                        }
                        if (isValidMatch && pos > 0) {
                            char prevChar = runText.ByteAt(pos - 1);
                            if (isalnum(prevChar) || prevChar == '/') isValidMatch = false;
                        }
                        if (isValidMatch && (pos + trigLen < runText.Length())) {
                            char nextChar = runText.ByteAt(pos + trigLen);
                            if (isalnum(nextChar) || nextChar == '/') isValidMatch = false;
                        }
                        if (isValidMatch) {
                            nextTrigger = pos;
                            triggerLength = trigLen;
                            foundIcon = emotes[e].data;
                        }
                    }
                }
            }

            bool processStyleTagFirst = (tagOpen != B_ERROR && tagOpen < nextTrigger);

            // 1. Process Style Tag Bracket Envelope Block
            if (processStyleTagFirst) {
                if (tagOpen > currentPos) {
                    StyledRunFragment* frag = new StyledRunFragment();
                    frag->type = FRAG_TEXT;
                    runText.CopyInto(frag->subText, currentPos, tagOpen - currentPos);
                    frag->font = activeFont;
                    frag->color = activeColor;
                    frag->width = activeFont.StringWidth(frag->subText.String());
                    rawFragments->AddItem(frag);
                }

                int32 tagClose = runText.FindFirst("]", tagOpen);
                if (tagClose == B_ERROR) {
                    currentPos = tagOpen + 1;
                    continue;
                }

                BString tagContent;
                runText.CopyInto(tagContent, tagOpen + 1, tagClose - tagOpen - 1);

                // --- FOOLPROOF VALIDATION CHECK ---
                bool isValidTag = false;
                if (colorCodesEnabled) {
                    if (tagContent == "B" || tagContent == "R" || tagContent == "C:Reset" || tagContent.StartsWith("C:")) {
                        isValidTag = true;
                    }
                }

                // Check for timestamp fallback rules
                bool looksLikeTimestamp = false;
                if (!isValidTag && tagContent.Length() > 0) {
                    int32 digitCount = 0;
                    int32 localColonCount = 0;
                    for (int32 cIdx = 0; cIdx < tagContent.Length(); cIdx++) {
                        char c = tagContent.ByteAt(cIdx);
                        if (isdigit(c)) digitCount++;
                        else if (c == ':') localColonCount++;
                    }
                    if (digitCount >= 2 && localColonCount >= 1) {
                        isValidTag = true; 
                        looksLikeTimestamp = true;
                    }
                }

 				// If it's a valid formatting tag OR a recognized timestamp, process it
                if (isValidTag) {
                    if (colorCodesEnabled) {
                        if (tagContent.StartsWith("C:") && tagContent != "C:Reset") {
                            BString tempContent = tagContent;
                            tempContent.Remove(0, 2);
                            int32 commaIdx = tempContent.FindFirst(",");
                            BString fgToken = (commaIdx != B_ERROR) ? tempContent.Truncate(commaIdx) : tempContent;
                            fgToken.Trim();

                            int32 pIdx = atoi(fgToken.String());
                            if (pIdx >= 0 && pIdx < 16) activeColor = ircPalette[pIdx];
                        } 
                        else if (tagContent == "B") {
                            activeFont.SetFace(B_BOLD_FACE);
                        } 
                        else if (tagContent == "R" || tagContent == "C:Reset") {
                            activeColor = (runs != nullptr) ? runs->runs[i].color : ui_color(B_DOCUMENT_TEXT_COLOR);
                            activeFont.SetFace(B_REGULAR_FACE);
                        }
                    }
                    
                    //  Replaced the broken check with explicit flag mapping
                    if (looksLikeTimestamp) { 
                        StyledRunFragment* frag = new StyledRunFragment();
                        frag->type = FRAG_TEXT;
                        runText.CopyInto(frag->subText, tagOpen, (tagClose - tagOpen) + 1);
                        frag->font = activeFont;
                        frag->color = activeColor;
                        frag->width = activeFont.StringWidth(frag->subText.String());
                        rawFragments->AddItem(frag);
                    }

                    currentPos = tagClose + 1;
                }
                // --- FALLBACK: It's regular text like [mycode], don't strip it! ---
                else {
                    StyledRunFragment* frag = new StyledRunFragment();
                    frag->type = FRAG_TEXT;
                    runText.CopyInto(frag->subText, tagOpen, (tagClose - tagOpen) + 1);
                    frag->font = activeFont;
                    frag->color = activeColor;
                    frag->width = activeFont.StringWidth(frag->subText.String());
                    rawFragments->AddItem(frag);

                    currentPos = tagClose + 1;
                }
            }


               // 2. Process Emoticon Token Fragment Block
            else {
                if (nextTrigger > currentPos) {
                    StyledRunFragment* frag = new StyledRunFragment();
                    frag->type = FRAG_TEXT;
                    runText.CopyInto(frag->subText, currentPos, nextTrigger - currentPos);
                    frag->font = activeFont;
                    
                    BString cleanCandidate = frag->subText;
                    cleanCandidate.Trim();
                    
                    // --- REVERTED & CLEANED STRING STRIPPER ---
                    if (cleanCandidate.Length() > 0) {
                        // 1. Strip common channel ranking prefixes (@, +, %)
                        if (cleanCandidate.ByteAt(0) == '@' || cleanCandidate.ByteAt(0) == '+' || cleanCandidate.ByteAt(0) == '%') {
                            cleanCandidate.Remove(0, 1);
                        }
                        
                        // 2. Strip surrounding message angle brackets (<ablyss> -> ablyss)
                        if (cleanCandidate.StartsWith("<") && cleanCandidate.EndsWith(">")) {
                            cleanCandidate.Remove(0, 1);
                            cleanCandidate.Truncate(cleanCandidate.Length() - 1);
                        }
                        
                        // 3. Strip surrounding alternative message parenthesis
                        if (cleanCandidate.StartsWith("(") && cleanCandidate.EndsWith(")")) {
                            cleanCandidate.Remove(0, 1);
                            cleanCandidate.Truncate(cleanCandidate.Length() - 1);
                        }
                        
                        // 4. Strip typical trailing message delimiters
                        if (cleanCandidate.EndsWith(":")) {
                            cleanCandidate.Truncate(cleanCandidate.Length() - 1);
                        }
                        cleanCandidate.Trim();
                    }
                    
                    rgb_color textFragmentColor = activeColor;
                    
					// --- CLEAN REVERTED INDEX LOOKUP LOOP ---
					// --- Main Nick Colorizer ---
					if (cleanCandidate.Length() > 0) {
					    // Dynamically determine the correct config vectors
					    const std::vector<std::string>* colorNicks = nullptr;
					    const std::vector<rgb_color>* colorVals = nullptr;
					
					    if (selectedConfig >= 0 && selectedConfig < (int)cfg.servers.size()) {
					        // Standard server context
					        colorNicks = &cfg.servers[selectedConfig].nickColors;
					        colorVals  = &cfg.servers[selectedConfig].nickColorValues;
					    } else {
					        // Custom server context
					        int customIdx = selectedConfig - (int)cfg.servers.size();
					        if (customIdx >= 0 && customIdx < (int)cfg.customServers.size()) {
					            colorNicks = &cfg.customServers[customIdx].nickColors;
					            colorVals  = &cfg.customServers[customIdx].nickColorValues;
					        }
					    }
					
					    // Execute matching using your wildcard engine
					    if (colorNicks != nullptr && colorVals != nullptr) {
					        for (size_t k = 0; k < colorNicks->size(); k++) {
					            // Using MatchWildcard instead of ICompare allows brackets and patterns to match
					            if (MatchWildcard(cleanCandidate.String(), (*colorNicks)[k].c_str())) {
					                textFragmentColor = (*colorVals)[k];
					                break;
					            }
					        }
					    }
					}
					// --- End Main Nick Colorizer ---
                    frag->color = textFragmentColor; 
                    frag->width = activeFont.StringWidth(frag->subText.String());
                    rawFragments->AddItem(frag);
                }

                if (foundIcon != nullptr) {
                    float iconDimension = fLineHeight;
                    BBitmap* cachedIcon = new BBitmap(BRect(0, 0, iconDimension - 1.0f, iconDimension - 1.0f), B_RGBA32);
                    
                    if (cachedIcon->InitCheck() == B_OK && BIconUtils::GetVectorIcon(foundIcon, 1024, cachedIcon) == B_OK) {
                        StyledRunFragment* frag = new StyledRunFragment();
                        frag->type = FRAG_ICON;
                        frag->cachedBitmap = cachedIcon;
                        frag->width = iconDimension;
                        rawFragments->AddItem(frag);
                    } else {
                        delete cachedIcon;
                    }
                    currentPos = nextTrigger + triggerLength;
                } else {
                    currentPos = nextTrigger;
                }
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


BString CustomChatView::GetSelectedText() {
    if (fSelectionStart == fSelectionEnd) return BString("");

    BString result = "";
    float currentY = 10.0f;

    BRect selRect;
    selRect.left = min_c(fSelectionStart.x, fSelectionEnd.x);
    selRect.right = max_c(fSelectionStart.x, fSelectionEnd.x);
    selRect.top = min_c(fSelectionStart.y, fSelectionEnd.y);
    selRect.bottom = max_c(fSelectionStart.y, fSelectionEnd.y);

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        if (!line || line->itemNode != fActiveChannelNode) continue;

        for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            float rowTop = currentY;
            float rowBottom = currentY + fLineHeight;

            if (rowBottom >= selRect.top && rowTop <= selRect.bottom) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    if (frag->type != FRAG_TEXT) {
                        currentX += frag->width;
                        continue;
                    }

                    BFont font = frag->font;
                    const char* rawStr = frag->subText.String();
                    int32 textLen = frag->subText.Length();
                    float charLeft = currentX;

                    // Step through the text byte-by-byte to find character layout boundaries
                    for (int32 c = 1; c <= textLen; c++) {
                        // Skip multi-byte UTF-8 continuation bytes so we only measure whole characters
                        if (c < textLen && ((rawStr[c] & 0xC0) == 0x80)) {
                            continue;
                        }

                        float charRight = currentX + font.StringWidth(rawStr, c);

                        // CHARACTER-PRECISION CHECK: 
                        // Does this individual character intersect the selection box?
                        if (charRight >= selRect.left && charLeft <= selRect.right) {
                            // Extract just this specific character sequence into our selection pool
                            int32 startByte = c - 1;
                            while (startByte > 0 && ((rawStr[startByte] & 0xC0) == 0x80)) {
                                startByte--; // Back up to the start of the UTF-8 sequence
                            }
                            
                            BString singleChar;
                            frag->subText.CopyInto(singleChar, startByte, c - startByte);
                            result << singleChar;
                        }
                        charLeft = charRight;
                    }
                    currentX += frag->width;
                }
                
                // Add an explicit newline space if the selection box spans across multiple line rows
                if (rowTop > selRect.top && rowBottom < selRect.bottom) {
                    result << "\n";
                }
            }
            currentY += fLineHeight;
        }
    }
    return result;
}



void CustomChatView::AddStyledLine(BStringItem* itemNode, const BString& text, const text_run_array* runs) {
    if (itemNode == nullptr) return;
    
    BString cleanText = text;
    cleanText.ReplaceAll("\r", " ");
    cleanText.ReplaceAll("\n", " ");

    StyledLine* newLine = new StyledLine(itemNode, cleanText, runs);
    fLines.AddItem(newLine);

    if (itemNode == fActiveChannelNode) {
        // Enforce the standard 36px buffer so text wrapping matches recalculation loop
        float maxWidth = Bounds().Width() - 36.0f;
        if (maxWidth <= 50.0f) maxWidth = 50.0f;

        BObjectList<StyledRunFragment, true> rawFragments(10);
        ParseTextAndIcons(cleanText, runs, &rawFragments);

        ComputeWrapForLine(newLine, maxWidth, &rawFragments);
        
        // Pass 'true' so the view snaps down to show newly processed line immediately
        UpdateScrollRange(true); 
        Invalidate();
    }
}






// Update Loop Filter Pass
void CustomChatView::RecalculateAllLineWraps()
{
    // Match the exact 36.0f padding used in AddStyledLine to stop the clipping
    float maxWidth = Bounds().Width() - 36.0f;
    if (maxWidth <= 50.0f) maxWidth = 50.0f;

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        
        if (line->itemNode == fActiveChannelNode) {
            // OPTIMIZATION: Instead of manually parsing every single line from scratch,
            // construct a temporary list and use pre-built logic pipelines.
            BObjectList<StyledRunFragment, true> rawFragments(10);
            ParseTextAndIcons(line->text, line->runs, &rawFragments);

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
            int32 lastSpaceIdx = -1;
            int32 textLen = frag->subText.Length();
            const char* rawStr = frag->subText.String();
            
            // Step safely through characters to find the smart layout break point
            for (int32 c = 1; c <= textLen; c++) {
                // Keep track of the last space character boundary seen on this line
                if (rawStr[c - 1] == ' ' || rawStr[c - 1] == '\t' || rawStr[c - 1] == '-') {
                    lastSpaceIdx = c;
                }

                float measuredWidth = font.StringWidth(rawStr, c);
                if ((measuredWidth + 1.0f) <= availableWidth) {
                    charCount = c;
                } else {
                    break;
                }
            }
            
            // INTELLIGENT WRAP CORRECTION: 
            // If we found a space character inside the text block that fits, slice 
            // at the word boundary instead of slicing midway through a word!
            if (lastSpaceIdx > 0 && charCount < textLen && lastSpaceIdx <= charCount) {
                charCount = lastSpaceIdx;
            }
            
            // Infinite Loop Protection: If a single word or a long URL is simply too wide 
            // to fit on an empty line row, force character-by-character slicing.
            if (charCount == 0 && currentX == 0.0f && textLen > 0) {
                charCount = 1;
                while (charCount < textLen && ((rawStr[charCount] & 0xC0) == 0x80)) {
                    charCount++;
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
            
            // Allocate a clean row container for the next line block row
            currentRow = new BObjectList<StyledRunFragment, true>(5);
            currentX = 0.0f;

            if (frag->subText.Length() > 0 || frag->type == FRAG_ICON) {
                rawFragments->AddItem(frag, 0);
            } else {
                delete frag; 
            }
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

                    // Force a global redraw of the view layout hierarchy.
                    // This tells the App Server to refresh the background image canvas,
                    // completely eliminating text smearing and tearing when scrolling.
                    Invalidate();
                    return; 
                }
            }
            break;
        }
        
        
        // =========================================================================
        // SYSTEM CLIPBOARD COPY ACTION CASE EXTENSION
        // =========================================================================
        case B_COPY: {
            BString selectedText = GetSelectedText();
            
            // Clean up raw text string format boundaries before clipboard transaction commit
            selectedText.Trim();
            selectedText.ReplaceAll("\r", "");
            selectedText.ReplaceAll("\n", " ");
            selectedText.Trim();

            if (selectedText.Length() > 0) {
                // 1. Gain an exclusive operational access lock over the Haiku App Server clipboard matrix
                if (be_clipboard->Lock()) {
                    // 2. Clear out legacy string payloads sitting inside the clipboard storage registers
                    be_clipboard->Clear();
                    
                    // 3. Fetch the storage message data parcel object container
                    BMessage* clipboardData = be_clipboard->Data();
                    if (clipboardData != nullptr) {
                        // Pack text content matching the standard canonical raw data guidelines
                        clipboardData->AddData("text/plain", B_MIME_TYPE, 
                                               selectedText.String(), selectedText.Length());
                        
                        // 4. Commit transaction immediately to broad-cast clipboard values globally
                        be_clipboard->Commit();
                    }
                    
                    // 5. Release access constraints safely to prevent application lock stalls
                    be_clipboard->Unlock();
                }
            }
            break;
        }

        case MSG_TRIGGER_REPLIES_SEARCH: {
            if (Window() != nullptr) {
                // Post our unique identifier token cleanly up to the main window thread
                BMessage searchCmd(MSG_TRIGGER_REPLIES_SEARCH);
                Window()->PostMessage(&searchCmd);
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
        
        

        // =========================================================================
        // NEW: DUCKDUCKGO SEARCH ACTION CASE EXTENSION
        // =========================================================================
        case 'ddgs': {
            BString selectedText = GetSelectedText();
            
            // Trim any layout whitespace remnants or stray characters safely
            selectedText.Trim();
            selectedText.ReplaceAll("\r", "");
            selectedText.ReplaceAll("\n", " ");
            selectedText.Trim();

            if (selectedText.Length() > 0) {
                // URL-escape special network characters securely
                selectedText.ReplaceAll("%", "%25"); // Escape percent first!
                selectedText.ReplaceAll(" ", "+");
                selectedText.ReplaceAll("&", "%26");
                selectedText.ReplaceAll("?", "%3F");
                selectedText.ReplaceAll("=", "%3D");
                selectedText.ReplaceAll("#", "%23");
                selectedText.ReplaceAll("/", "%2F"); 
                
                // Point straight to the DuckDuckGo html search query path fallback
                BString ddgUrl = "https://duckduckgo.com?q=";
                ddgUrl << selectedText;
                
                // Package web link as a port-safe POSIX argument array
                char* args[] = {
                    (char*)ddgUrl.String(),
                    nullptr
                };

                // Command the App Roster to launch the browser using the atomic argument pass
                status_t err = be_roster->Launch("text/html", 1, args);
                
                if (err != B_OK && err != B_ALREADY_RUNNING) {
                    // Fallback: Track down the binary directly if global mapping hits a block
                    entry_ref browserRef;
                    if (be_roster->FindApp("text/html", &browserRef) == B_OK) {
                        be_roster->Launch(&browserRef, 1, args);
                    }
                }
            }
            break;
        }

        // =========================================================================
        // GOOGLE SEARCH ACTION CASE EXTENSION
        // =========================================================================
        case 'gsh': {
            BString selectedText = GetSelectedText();
            
            // Trim any layout whitespace remnants or stray characters safely
            selectedText.Trim();
            selectedText.ReplaceAll("\r", "");
            selectedText.ReplaceAll("\n", " ");
            selectedText.Trim();

            if (selectedText.Length() > 0) {
                // URL-escape special network characters securely
                selectedText.ReplaceAll("%", "%25"); // Escape percent first!
                selectedText.ReplaceAll(" ", "+");
                selectedText.ReplaceAll("&", "%26");
                selectedText.ReplaceAll("?", "%3F");
                selectedText.ReplaceAll("=", "%3D");
                selectedText.ReplaceAll("#", "%23");
                selectedText.ReplaceAll("/", "%2F"); 
                
                //  Explicitly use the search engine execution script query path
                BString googleUrl = "https://www.google.com/search?q=";
                googleUrl << selectedText;
                
                // Package web link as a port-safe POSIX argument array
                char* args[] = {
                    (char*)googleUrl.String(),
                    nullptr
                };

                // Command the App Roster to launch the browser using the atomic argument pass
                status_t err = be_roster->Launch("text/html", 1, args);
                
                if (err != B_OK && err != B_ALREADY_RUNNING) {
                    // Fallback: Track down the binary directly if global mapping hits a block
                    entry_ref browserRef;
                    if (be_roster->FindApp("text/html", &browserRef) == B_OK) {
                        be_roster->Launch(&browserRef, 1, args);
                    }
                }
            }
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

    // Calculate system luminance to dynamically check for a Light vs Dark theme
    // Formula: 0.2126*R + 0.7152*G + 0.0722*B
    float luminance = (0.2126f * systemBgColor.red) + 
                      (0.7152f * systemBgColor.green) + 
                      (0.0722f * systemBgColor.blue);
    bool isLightTheme = (luminance > 128.0f);

    // --- BRANCH A: SCALED CUSTOM BACKGROUND REFRESH ---
    if (fBackgroundBitmap != nullptr) {
        SetDrawingMode(B_OP_COPY);
        DrawBitmap(fBackgroundBitmap, Bounds());

        // --- NEW: HIGH-PERFORMANCE DIMMING OVERLAY PASS ---
        if (fBackgroundDimmingLevel > 0) {
            uint8 alphaIntensity = (uint8)((fBackgroundDimmingLevel / 100.0f) * 255.0f);
            
            // Over light themes, wash with white. Over dark themes, dim with black.
            rgb_color dimColor = isLightTheme ? rgb_color{ 255, 255, 255, alphaIntensity }
                                              : rgb_color{ 0, 0, 0, alphaIntensity };
            
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

        // 1. Draw custom kIconCricket data asset dynamically via BMemoryIO
        float iconDimension = 64.0f;
        BBitmap* customIcon = new BBitmap(BRect(0, 0, iconDimension - 1.0f, iconDimension - 1.0f), B_RGBA32);

        if (customIcon != nullptr && customIcon->InitCheck() == B_OK) {
            status_t err = BIconUtils::GetVectorIcon(kIconCricket, kIconCricketSize, customIcon);
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
        
        // Dynamic Check: Dark text for Light theme, bright text for Dark theme
        if (isLightTheme) {
            SetHighColor(40, 40, 40, 255); 
        } else {
            SetHighColor(220, 220, 220, 255); 
        }

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
        infoFont.SetSize(12.0f); 
        infoFont.SetFace(B_BOLD_FACE | B_ITALIC_FACE);
        SetFont(&infoFont);
        
        // Muted gray shift depending on contrast direction
        if (isLightTheme) {
            SetHighColor(100, 100, 100, 255);
        } else {
            SetHighColor(140, 140, 140, 255); 
        }

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
        
        // Deeper navy blue link variant for light screens to keep text legible
        if (isLightTheme) {
            SetHighColor(25, 95, 175, 255);
        } else {
            SetHighColor(114, 172, 230, 255); 
        }

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

    SetDrawingMode(B_OP_ALPHA);

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        if (!line || line->itemNode != fActiveChannelNode) continue;

        int32 totalRows = line->wrappedRows.CountItems();

        for (int32 rowIdx = 0; rowIdx < totalRows; rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            if (currentY + fLineHeight >= 0 && currentY <= Bounds().bottom) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    SetFont(&(frag->font));

                    if (frag->type == FRAG_TEXT) {
                        float fragLeft = currentX;
                        float fragRight = currentX + frag->width;
                        
                        // =========================================================================
                        // LIVE SELECTION HIGHLIGHT PASS (Render background box BEFORE the string)
                        // =========================================================================
                        if (fSelectionStart != fSelectionEnd) {
                            BRect selRect;
                            selRect.left = min_c(fSelectionStart.x, fSelectionEnd.x);
                            selRect.right = max_c(fSelectionStart.x, fSelectionEnd.x);
                            selRect.top = min_c(fSelectionStart.y, fSelectionEnd.y);
                            selRect.bottom = max_c(fSelectionStart.y, fSelectionEnd.y);

                            // Check if this explicit fragment row falls inside selection coordinate box
                            if (currentY + fLineHeight >= selRect.top && currentY <= selRect.bottom) {
                                if (fragRight >= selRect.left && fragLeft <= selRect.right) {
                                    // Math boundary clipping for single row precision
                                    float hLeft = max_c(fragLeft, selRect.left);
                                    float hRight = min_c(fragRight, selRect.right);
                                    
                                    // Snaps Haiku's standard document selection color securely
                                    SetHighColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
                                    FillRect(BRect(hLeft, currentY, hRight, currentY + fLineHeight));
                                }
                            }
                        }

                        // Determine render text coloring cleanly
                        rgb_color renderColor = frag->color;
                        if ((renderColor.red == 255 && renderColor.green == 255 && renderColor.blue == 255) ||
                            (renderColor.red == 0   && renderColor.green == 0   && renderColor.blue == 0)) {
                            renderColor = systemTextColor;
                        }
                        SetHighColor(renderColor);
                        
                        // Draw text over the highlighted background tint layer safely
                        DrawString(frag->subText.String(), BPoint(currentX, currentY + fLineHeight - 2.0f));
                        
                        currentX += frag->width;
                    } 
                    else if (frag->type == FRAG_ICON) {
                        BRect iconRect(currentX, 
                                       currentY + fLineHeight - fontAscent - 2.0f, 
                                       currentX + fLineHeight, 
                                       currentY + fLineHeight + fh.descent - 2.0f);
                        
                        if (frag->cachedBitmap != nullptr) {
                            DrawBitmap(frag->cachedBitmap, iconRect);
                        }
                        currentX += frag->width + 2.0f;
                    }
                }
            }
            currentY += fLineHeight;
        }
    }
    
    SetDrawingMode(B_OP_COPY);
}



void CustomChatView::MouseUp(BPoint point) {
    if (fIsSelecting) {
        fIsSelecting = false;
        // Keep the selection coordinates so right-clicking can grab the cached selection text!
    }
}



// Scoped layout mouse hover tracking engine for dynamic cursor feedback and selection highlighting
void CustomChatView::MouseMoved(BPoint point, uint32 transit, const BMessage* dragMessage) {
    
    // =========================================================================
    // NEW: LIVE SELECTION HIGHLIGHT TRACKING INTERCEPTION
    // =========================================================================
    if (fIsSelecting) {
        fSelectionEnd = point;
        Invalidate(); // Smoothly triggers paint refreshes as you sweep the mouse cursor!
        
        // Change cursor to standard text I-BEAM during selecting sweeps for visual polish
        BCursor textCursor(B_CURSOR_ID_I_BEAM);
        SetViewCursor(&textCursor);
        
        BView::MouseMoved(point, transit, dragMessage);
        return;
    }

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

    // 2. Spatial matching mirror layout loop from MouseDown implementation
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
                    float segmentWidth = frag->width; // OPTIMIZATION: Read calculated width directly

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
        // Switch to an explicit I-BEAM text selector cursor when hovering over normal chat text rows
        BCursor textCursor(B_CURSOR_ID_I_BEAM);
        SetViewCursor(&textCursor);
    }

    // Call the base class implementation to preserve default system drag/drop pipelines
    BView::MouseMoved(point, transit, dragMessage);
}




// Scoped layout mouse click tracker logic implementation with Channel Filtering Isolation and Text Selection
void CustomChatView::MouseDown(BPoint point) {
    if (Window() == nullptr) return;
	 MakeFocus(true);
    // =========================================================================
    // NEW: INITIAL APP BOOT HOVER CLICK TARGET CHECK
    // =========================================================================
    if (fActiveChannelNode == nullptr) {
        BRect bounds = Bounds();
        float centerX = bounds.Width() / 2.0f;
        float centerY = bounds.Height() / 2.0f;

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
        BPopUpMenu* contextMenu = new BPopUpMenu("ChatViewContext", false, false);
        
        BString selectedText = GetSelectedText();
        if (selectedText.Length() > 0) {
            BString labelText = selectedText;
            labelText.Trim();
            labelText.ReplaceAll("\r", "");
            labelText.ReplaceAll("\n", " ");
            
            if (labelText.Length() > 15) {
                labelText.Truncate(15);
                labelText << "...\"";
            } else {
                labelText << "\"";
            }

            // 1. DuckDuckGo Search Option
            BString ddgLabel = "Search DuckDuckGo for \"";
            ddgLabel << labelText;
            contextMenu->AddItem(new BMenuItem(ddgLabel.String(), new BMessage('ddgs')));

            // 2. Google Search Option choice row
            BString googleLabel = "Search Google for \"";
            googleLabel << labelText;
            contextMenu->AddItem(new BMenuItem(googleLabel.String(), new BMessage('gsh')));
            
            // =========================================================================
            // DYNAMIC GOOGLE TRANSLATION SUBMENU WITH EMBEDDED TITLE LAYOUT
            // =========================================================================
            // 1. Build a clean, safely truncated label for the main sub-menu header row
            BString subMenuHeaderLabel = "Translate \"";
            BString truncatedLabelText = selectedText;
            truncatedLabelText.Trim();
            truncatedLabelText.ReplaceAll("\r", "");
            truncatedLabelText.ReplaceAll("\n", " ");

            if (truncatedLabelText.Length() > 15) {
                truncatedLabelText.Truncate(15);
                subMenuHeaderLabel << truncatedLabelText << "...\" with Google";
            } else {
                subMenuHeaderLabel << truncatedLabelText << "\" with Google";
            }

            // 2. Instantiate the sub-menu container using our dynamic header label string
            BMenu* translateSubMenu = new BMenu(subMenuHeaderLabel.String());
            
            BString labelEng = BString("English");
            BString labelSpa = BString("Spanish");
            BString labelFre = BString("French");
            BString labelGer = BString("German");
            BString labelJap = BString("Japanese");
            BString labelChi = BString("Chinese");
            BString labelIta = BString("Italian");
            BString labelPor = BString("Portuguese");
            BString labelRus = BString("Russian");
            BString labelKor = BString("Korean");
            BString labelAra = BString("Arabic");
            BString labelHin = BString("Hindi");
            BString labelDut = BString("Dutch");
            BString labelSwe = BString("Swedish");
            BString labelTur = BString("Turkish");

            BMessage* msgEng = new BMessage('trns'); msgEng->AddString("lang", "English"); msgEng->AddString("text", selectedText);
            BMessage* msgSpa = new BMessage('trns'); msgSpa->AddString("lang", "Spanish"); msgSpa->AddString("text", selectedText);
            BMessage* msgFre = new BMessage('trns'); msgFre->AddString("lang", "French"); msgFre->AddString("text", selectedText);
            BMessage* msgGer = new BMessage('trns'); msgGer->AddString("lang", "German"); msgGer->AddString("text", selectedText);
            BMessage* msgJap = new BMessage('trns'); msgJap->AddString("lang", "Japanese"); msgJap->AddString("text", selectedText);
            BMessage* msgChi = new BMessage('trns'); msgChi->AddString("lang", "Chinese"); msgChi->AddString("text", selectedText);
            BMessage* msgIta = new BMessage('trns'); msgIta->AddString("lang", "Italian"); msgIta->AddString("text", selectedText);
            BMessage* msgPor = new BMessage('trns'); msgPor->AddString("lang", "Portuguese"); msgPor->AddString("text", selectedText);
            BMessage* msgRus = new BMessage('trns'); msgRus->AddString("lang", "Russian"); msgRus->AddString("text", selectedText);
            BMessage* msgKor = new BMessage('trns'); msgKor->AddString("lang", "Korean"); msgKor->AddString("text", selectedText);
            BMessage* msgAra = new BMessage('trns'); msgAra->AddString("lang", "Arabic"); msgAra->AddString("text", selectedText);
            BMessage* msgHin = new BMessage('trns'); msgHin->AddString("lang", "Hindi"); msgHin->AddString("text", selectedText);
            BMessage* msgDut = new BMessage('trns'); msgDut->AddString("lang", "Dutch"); msgDut->AddString("text", selectedText);
            BMessage* msgSwe = new BMessage('trns'); msgSwe->AddString("lang", "Swedish"); msgSwe->AddString("text", selectedText);
            BMessage* msgTur = new BMessage('trns'); msgTur->AddString("lang", "Turkish"); msgTur->AddString("text", selectedText);
            
            BMenuItem* itemEng = new BMenuItem(labelEng.String(), msgEng);
            BMenuItem* itemSpa = new BMenuItem(labelSpa.String(), msgSpa);
            BMenuItem* itemFre = new BMenuItem(labelFre.String(), msgFre);
            BMenuItem* itemGer = new BMenuItem(labelGer.String(), msgGer);
            BMenuItem* itemJap = new BMenuItem(labelJap.String(), msgJap);
            
            BMenuItem* itemChi = new BMenuItem(labelChi.String(), msgChi);
            BMenuItem* itemIta = new BMenuItem(labelIta.String(), msgIta);
            BMenuItem* itemPor = new BMenuItem(labelPor.String(), msgPor);
            BMenuItem* itemRus = new BMenuItem(labelRus.String(), msgRus);
            BMenuItem* itemKor = new BMenuItem(labelKor.String(), msgKor);
            BMenuItem* itemAra = new BMenuItem(labelAra.String(), msgAra);
            BMenuItem* itemHin = new BMenuItem(labelHin.String(), msgHin);
            BMenuItem* itemDut = new BMenuItem(labelDut.String(), msgDut);
            BMenuItem* itemSwe = new BMenuItem(labelSwe.String(), msgSwe);
            BMenuItem* itemTur = new BMenuItem(labelTur.String(), msgTur);


            // FORCE TARGETING TO THE MAIN WINDOW (Matches top-level 'gsh' behavior)
            if (this->Window() != nullptr) {
                itemEng->SetTarget(this->Window());
                itemSpa->SetTarget(this->Window());
                itemFre->SetTarget(this->Window());
                itemGer->SetTarget(this->Window());
                itemJap->SetTarget(this->Window());
                
                itemChi->SetTarget(this->Window());
	            itemIta->SetTarget(this->Window());
	            itemPor->SetTarget(this->Window());
	            itemRus->SetTarget(this->Window());
	            itemKor->SetTarget(this->Window());
	            itemAra->SetTarget(this->Window());
	            itemHin->SetTarget(this->Window());
	            itemDut->SetTarget(this->Window());
	            itemSwe->SetTarget(this->Window());
	            itemTur->SetTarget(this->Window());

            }

            translateSubMenu->AddItem(itemEng);
            translateSubMenu->AddItem(itemSpa);
            translateSubMenu->AddItem(itemFre);
            translateSubMenu->AddItem(itemGer);
            translateSubMenu->AddItem(itemJap);    
            translateSubMenu->AddItem(itemChi);
            translateSubMenu->AddItem(itemIta);
            translateSubMenu->AddItem(itemPor);
            translateSubMenu->AddItem(itemRus);
            translateSubMenu->AddItem(itemKor);
            translateSubMenu->AddItem(itemAra);
            translateSubMenu->AddItem(itemHin);
            translateSubMenu->AddItem(itemDut);
            translateSubMenu->AddItem(itemSwe);
            translateSubMenu->AddItem(itemTur);                     
            
            contextMenu->AddItem(translateSubMenu);

            contextMenu->AddSeparatorItem();
            contextMenu->AddItem(new BMenuItem("Copy to Clipboard", new BMessage(B_COPY)));
            contextMenu->AddSeparatorItem();
        }

           
        // --- PASSIVE CATCH UP ROW (Injected right above Clear Buffer) ---
        // Uses a soft, human-centric name instead of rigid terms like "Mentions"
        BMenuItem* repliesItem = new BMenuItem("Catch Up on Replies", new BMessage(MSG_TRIGGER_REPLIES_SEARCH));
        contextMenu->AddItem(repliesItem);
        
        // Add the Clear option tied to our unique identifier token constant
        BMenuItem* clearItem = new BMenuItem("Clear Buffer", new BMessage(MSG_CLEAR_CUSTOM_BUFFER));
        contextMenu->AddItem(clearItem);
        
        contextMenu->SetTargetForItems(this);
        contextMenu->Go(ConvertToScreen(point), true, true, true);
        return;
    }


    // =========================================================================
    // LEFT-CLICK LINK TRACKER, SELECTION DRAG, & DOUBLE-CLICK WORD SELECTOR
    // =========================================================================
    if (buttons != B_PRIMARY_MOUSE_BUTTON) return;

    // Fetch the raw system click event counter from the incoming window message package
    int32 clickCount = 1;
    if (Window()->CurrentMessage()->FindInt32("clicks", &clickCount) != B_OK) {
        clickCount = 1;
    }

    if (clickCount == 2) {
        // --- DOUBLE CLICK MODE: Snap selection to the targeted word boundaries ---
        fIsSelecting = false; // Disable background drag tracking during explicit snaps
        
        float currentY = 10.0f;
        for (int32 i = 0; i < fLines.CountItems(); i++) {
            StyledLine* line = fLines.ItemAt(i);
            if (!line || line->itemNode != fActiveChannelNode) continue;

            for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
                BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
                if (!row) continue;

                // Check if our cursor height matches this row layout block boundary
                if (point.y >= currentY && point.y <= (currentY + fLineHeight)) {
                    float currentX = 10.0f;

                    for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                        StyledRunFragment* frag = row->ItemAt(fragIdx);
                        if (!frag || frag->type != FRAG_TEXT) {
                            if (frag) currentX += frag->width;
                            continue;
                        }

                        float fragLeft = currentX;
                        float fragRight = currentX + frag->width;

                        // Check if the cursor X coordinate intersects this explicit fragment zone
                        if (point.x >= fragLeft && point.x <= fragRight) {
                            BFont font = frag->font;
                            const char* rawStr = frag->subText.String();
                            int32 textLen = frag->subText.Length();
                            float charLeft = fragLeft;

                            // Step safely through text byte boundaries to identify the targeted character index
                            for (int32 c = 1; c <= textLen; c++) {
                                if (c < textLen && ((rawStr[c] & 0xC0) == 0x80)) {
                                    continue; // Skip multi-byte UTF-8 continuation segments
                                }

                                float charRight = fragLeft + font.StringWidth(rawStr, c);

                                if (point.x >= charLeft && point.x <= charRight) {
                                    // Target index found! Now expand left and right to map the full word boundaries
                                    int32 wordStart = c - 1;
                                    int32 wordEnd = c - 1;

                                    // Scan backwards to find the start of the word block layout
                                    while (wordStart > 0 && rawStr[wordStart - 1] != ' ' && 
                                           rawStr[wordStart - 1] != '\t' && rawStr[wordStart - 1] != '[') {
                                        wordStart--;
                                    }

                                    // Scan forwards to find the end of the word block layout
                                    while (wordEnd < textLen && rawStr[wordEnd] != ' ' && 
                                           rawStr[wordEnd] != '\t' && rawStr[wordEnd] != ']') {
                                        wordEnd++;
                                    }

                                    // Calculate the exact bounding coordinates of our target word
                                    float selectPixelLeft = fragLeft + font.StringWidth(rawStr, wordStart);
                                    float selectPixelRight = fragLeft + font.StringWidth(rawStr, wordEnd);

                                    // Snap selection parameters to wrap the calculated text pixel constraints perfectly
                                    fSelectionStart = BPoint(selectPixelLeft, currentY + 2.0f);
                                    fSelectionEnd   = BPoint(selectPixelRight, currentY + 2.0f);
                                    
                                    Invalidate(); // Trigger a canvas redraw pass instantly
                                    return;
                                }
                                charLeft = charRight;
                            }
                        }
                        currentX += frag->width;
                    }
                    return;
                }
                currentY += fLineHeight;
            }
        }
        return;
    }

    // --- SINGLE CLICK MODE: Fallback to manual selection dragging logic ---
    fIsSelecting = true;
    fSelectionStart = point;
    fSelectionEnd = point;
    
    SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
    Invalidate();

    float currentY = 10.0f;
    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        if (!line || line->itemNode != fActiveChannelNode) continue;

        for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            if (point.y >= currentY && point.y <= (currentY + fLineHeight)) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    float segmentWidth = frag->width;

                    if (frag->font.Face() & B_UNDERSCORE_FACE) {
                        if (point.x >= currentX && point.x <= (currentX + segmentWidth)) {
                            fIsSelecting = false;
                            
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
    if (!cfg.debugEnable || rawData == nullptr || dataLength <= 0) return;

    // Use static thread-local string buffers to accumulate single-byte streams into full lines
    // This stops disk I/O throttling and prevents printing binary chunk artifacts!
    static std::map<BString, BString> sLineAccumulators;
    BString keyKey = BString(serverName) << "_" << direction;
    
    sLineAccumulators[keyKey].Append(rawData, dataLength);
    
    // Only proceed to disk output once a complete line boundary (\n) is finalized
    if (sLineAccumulators[keyKey].FindFirst("\n") == B_ERROR) {
        return; 
    }

    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket/cricket_debug_log.txt");
        
        std::ofstream logFile(path.Path(), std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            
            // Process all completed lines trapped in our string cache container
            while (sLineAccumulators[keyKey].FindFirst("\n") != B_ERROR) {
                int32 newlinePos = sLineAccumulators[keyKey].FindFirst("\n");
                BString line;
                sLineAccumulators[keyKey].CopyInto(line, 0, newlinePos + 1);
                sLineAccumulators[keyKey].Remove(0, newlinePos + 1);
                
                BString cleanBuffer = line;
                cleanBuffer.ReplaceAll("\r", "");
                cleanBuffer.ReplaceAll("\n", "");
                cleanBuffer.Trim();
                
                if (cleanBuffer.Length() == 0) continue;

                // Timestamp Generation
                bigtime_t currentTime = real_time_clock_usecs();
                time_t rawTime = (time_t)(currentTime / 1000000);
                struct tm* timeInfo = localtime(&rawTime);
                char timeBuffer[32] = {0};
                if (timeInfo != nullptr) {
                    strftime(timeBuffer, sizeof(timeBuffer), "[%Y-%m-%d %H:%M:%S] ", timeInfo);
                    logFile << timeBuffer;
                }

                // Write the clean, un-mangled structural text transaction line
                logFile << "[" << serverName << "] " << direction << ": " << cleanBuffer.String() << "\n";
            }
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






// Structure to preserve master data for instant filtering
struct ChannelDataRecord {
    BString name;
    BString users;
    BString topic;
};

class IRCChannelListWindow : public BWindow {
public:
    IRCChannelListWindow(BWindow* owner, ServerTreeItem* serverItem, IRCChannelListWindow** tracker) 
        : BWindow(BRect(150, 150, 800, 650), "Network Channel List", 
                  B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS) {

        fOwnerWindow = owner;
        fServerContext = serverItem; 
        fTracker = tracker;
        
        // (Your existing view allocations, list views, and button layouts continue down here...)


        fListView = new BListView("chan_list_view");
        fListView->SetInvocationMessage(new BMessage('join'));
        BScrollView* scrollPane = new BScrollView("scroll_list", fListView, 0, false, true);

        fFilterControl = new BTextControl("filter_bar", "Filter by Keyword:", "", new BMessage('fltr'));
        fFilterControl->SetModificationMessage(new BMessage('fltr'));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 5)
            .SetInsets(8)
            .Add(fFilterControl, 0.0) 
            .Add(scrollPane, 1.0)     
            .End();

        // =========================================================================
        // FIXED MULTI-SERVER SSL_WRITE CHANNEL LIST REQUEST GATE
        // =========================================================================
        if (fServerContext != nullptr) {
            // Check the static global matrix map registry for a live OpenSSL session handle
            SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(fServerContext)];
            
            if (activeSslHandle != nullptr) {
                // Transmit straight down the open background pipe safely!
                SSL_write(activeSslHandle, "LIST\r\n", 6);
            } else if (fSocket != nullptr) {
                // Flat native fallback path for unmanaged legacy connections
                fSocket->Write("LIST\r\n", 6);
            }
        }
        // =========================================================================
    }

    BSecureSocket* GetTargetSocket() const { return fSocket; }
    ServerTreeItem* GetServerContext() const { return fServerContext; }
    
    void FrameResized(float newWidth, float newHeight) override {
        BWindow::FrameResized(newWidth, newHeight);
        if (fListView != nullptr) {
            fListView->InvalidateLayout();
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
                        menu->AddItem(new BMenuItem("Clear Filter Bar", new BMessage('clrf')));
                        
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
            
                    ChannelDataRecord record = { channelName, userCount, topic };
                    fMasterRecords.push_back(record);

                    BString keyword = fFilterControl->Text();
                    keyword.Trim();
                    
                    if (keyword.Length() == 0 || 
                        BString(channelName).IFindFirst(keyword) != B_ERROR || 
                        BString(topic).IFindFirst(keyword) != B_ERROR) {
                        
                        fListView->AddItem(new ChannelRowItem(channelName, userCount, topic));
                        fListView->SortItems(SortChannelsByUsers); 
                    }
                }
                break;
            }

            case 'fltr': { 
                ApplyFilter();
                break;
            }

            case 'clrf': { 
                fFilterControl->SetText(""); 
                break;
            }

            case 'join': {
                int32 selectedIdx = fListView->CurrentSelection();
                if (selectedIdx >= 0) {
                    ChannelRowItem* selectedItem = dynamic_cast<ChannelRowItem*>(fListView->ItemAt(selectedIdx));
                    if (selectedItem != nullptr && fServerContext != nullptr) {
                        BString joinCmd;
                        joinCmd << "JOIN " << selectedItem->GetChannelName() << "\r\n";
                        
                        // =========================================================================
                        // FIXED MULTI-SERVER SSL_WRITE JOIN TRANSIT ENGINE
                        // =========================================================================
                        SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(fServerContext)];
                        
                        if (activeSslHandle != nullptr) {
                            SSL_write(activeSslHandle, joinCmd.String(), joinCmd.Length());
                        } else if (fSocket != nullptr) {
                            fSocket->Write(joinCmd.String(), joinCmd.Length());
                        }
                        // =========================================================================
                        
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

    bool QuitRequested() override {
        while (fListView->CountItems() > 0) {
            delete fListView->RemoveItem((int32)0);
        }
        fMasterRecords.clear(); 

        if (fTracker != nullptr) {
            *fTracker = nullptr;
        }
        return true; 
    }

private:
    void ApplyFilter() {
        if (fListView->LockLooper()) {
            while (fListView->CountItems() > 0) {
                delete fListView->RemoveItem((int32)0);
            }

            BString keyword = fFilterControl->Text();
            keyword.Trim();

            for (const auto& rec : fMasterRecords) {
                if (keyword.Length() == 0 || 
                    rec.name.IFindFirst(keyword) != B_ERROR || 
                    rec.topic.IFindFirst(keyword) != B_ERROR) {
                    
                    fListView->AddItem(new ChannelRowItem(rec.name.String(), rec.users.String(), rec.topic.String()));
                }
            }
            
            fListView->SortItems(SortChannelsByUsers);
            fListView->UnlockLooper();
        }
    }

    BWindow*                      fOwnerWindow;
    BListView*                    fListView;
    BTextControl*                 fFilterControl; 
    BSecureSocket*                fSocket;
    ServerTreeItem*               fServerContext; 
    IRCChannelListWindow**        fTracker;
    std::vector<ChannelDataRecord> fMasterRecords; 
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
        : BStringItem(text), 
          fConfigIndex(configIndex), 
          fIsChannel(isChannel), 
          fIsCustom(isCustom),
          fHasIdentifiedThisSession(false),
          fHasFinalizedCap(false),
          fEnableColorCodes(false) {
        	
        // --- DATA-DRIVEN CAPABILITIES INIT ---
        fTypeAModes = "eIbq"; // Type A: Lists (Bans, Quiets, Exceptions)
        fTypeBModes = "k";    // Type B: Channel Password Access Keys
        fTypeCModes = "l";    // Type C: User Count Maximum Limits
        fTypeDModes = "cimnpstz"; // Type D: Parameterless Flags

        // Safety bounds check to pull structural configs securely
        if (fIsCustom) {
            if (fConfigIndex < cfg.customServers.size()) {
                fAutoConnect = cfg.customServers[fConfigIndex].autoConnect;
                fAutoReconnect = cfg.customServers[fConfigIndex].autoReconnect;
                fHideStatus = cfg.customServers[fConfigIndex].hideStatusMessages;
            } else {
                fAutoConnect = false;
                fAutoReconnect = false;
                fHideStatus = false;
            }
        } else {
            if (fConfigIndex < cfg.servers.size()) {
                fAutoConnect = cfg.servers[fConfigIndex].autoConnect;
                fAutoReconnect = cfg.servers[fConfigIndex].autoReconnect;
                fHideStatus = cfg.servers[fConfigIndex].hideStatusMessages;
            } else {
                fAutoConnect = false;
                fAutoReconnect = false;
                fHideStatus = false;
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
    
    // --- CLASS MEMBERS PROPERTY FIELDS ---
    bool fHasIdentifiedThisSession;
    bool fHasFinalizedCap;
    bool fEnableColorCodes;
    BString fSupportedCaps;
    bool fSASLSuccess;
  
    
    
    std::vector<std::string> fRuntimeIgnoreList;
    std::vector<std::string> fRuntimeColorNicks;
    std::vector<rgb_color>   fRuntimeColorValues;
    
    // Dynamic server modes mapping
    BString fTypeAModes; 
    BString fTypeBModes; 
    BString fTypeCModes; 
    BString fTypeDModes; 

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
    bool   fAutoConnect;
    bool   fAutoReconnect;
    bool   fHideStatus;
    bool   fIsChannel; 
    bool   fIsCustom; 
};



//@con

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
        bool useSASL = false;
        std::string saslUser;
        
        
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

        // --- SASL Authentication UI Controls ---
        fUseSASLCheck = new BCheckBox("usesasl", "Enable SASL Authentication", new BMessage('sasl'));
        fUseSASLCheck->SetValue(srv.useSASL ? B_CONTROL_ON : B_CONTROL_OFF);
        
        fSASLUserInput = new BTextControl("sasluser", "SASL Username:", srv.saslUser.c_str(), nullptr);
        fSASLUserInput->SetEnabled(srv.useSASL);

        // --- CertFP Authentication UI Controls ---
        fUseCertFPCheck = new BCheckBox("usecertfp", "Enable TLS CertFP Authentication", new BMessage('cfpa'));
        fUseCertFPCheck->SetValue(srv.useCertFP ? B_CONTROL_ON : B_CONTROL_OFF);
        
        fCertProfileInput = new BTextControl("certprofile", "SASL Cert/Profile (.pem):", srv.certProfileName.c_str(), nullptr);
        fCertProfileInput->SetEnabled(srv.useCertFP);

        fCertFileInput = new BTextControl("certfile", "Public Cert File (.crt):", srv.certFileName.c_str(), nullptr);
        fCertFileInput->SetEnabled(srv.useCertFP);

        fKeyFileInput = new BTextControl("keyfile", "Private Key File (.key):", srv.keyFileName.c_str(), nullptr);
        fKeyFileInput->SetEnabled(srv.useCertFP);
        
        // NEW: Instantiate the click button action item natively
        fGenerateCertsButton = new BButton("gencertbtn", "Generate Fresh CertFP Keypair", new BMessage(MSG_GENERATE_CERTFP_KEYPAIR));
        fGenerateCertsButton->SetEnabled(srv.useCertFP); // Only let them click it if CertFP is checked on!

        BStringView* certLocationNotice = new BStringView("cert_notice", 
            "💡 (.pem, .crt, .key) files automatically stored in ~/config/settings/cricket/certs/ folder.");


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
        fAutoConnectCheck->SetToolTip("Automatically connect to this IRC server when Cricket starts up.");

        fAutoReconnectCheck = new BCheckBox("autoreconnect", "Automatically reconnect on drop", nullptr);
        fAutoReconnectCheck->SetValue(srv.autoReconnect ? B_CONTROL_ON : B_CONTROL_OFF);
        fAutoReconnectCheck->SetToolTip("Attempt to automatically reconnect if the network connection drops unexpectedly.");

        fHideStatusCheck = new BCheckBox("hidestatus", "Hide channel status messages (Joins/Parts/Quits)", nullptr);
        fHideStatusCheck->SetValue(srv.hideStatusMessages ? B_CONTROL_ON : B_CONTROL_OFF);
        fHideStatusCheck->SetToolTip("Hides user join, part, and quit messages to keep busy chat channels less cluttered.");

        fDebugEnableCheck = new BCheckBox("debug", "Enable low-level socket engine logs", nullptr);
        fDebugEnableCheck->SetValue(srv.debugEnable ? B_CONTROL_ON : B_CONTROL_OFF);
        fDebugEnableCheck->SetToolTip("Outputs detailed raw socket traffic data to the system console for debugging connection issues.");

        fEnableEmoticonsCheck = new BCheckBox("enable_emotes", "Enable custom inline emoticons for this server", nullptr);
        fEnableEmoticonsCheck->SetValue(srv.enableEmoticons ? B_CONTROL_ON : B_CONTROL_OFF);
        fEnableEmoticonsCheck->SetToolTip("Renders text emoticons as custom inline graphical emoticons in your chat logs.");

        fUseCustomDrawCheck = new BCheckBox("use_custom_draw", "Enable High Performance Draw Engine", nullptr);
        fUseCustomDrawCheck->SetValue(srv.useCustomDrawFunction ? B_CONTROL_ON : B_CONTROL_OFF);
        fUseCustomDrawCheck->SetToolTip("Bypasses standard layout constraints, using Cricket's optimized direct canvas drawing engine for faster line rendering and smooth log scrolling.");

        fLogChatsToFileCheck = new BCheckBox("log_chats", "Log server chats to file", nullptr);
        fLogChatsToFileCheck->SetValue(srv.logChatsToFile ? B_CONTROL_ON : B_CONTROL_OFF);
        fLogChatsToFileCheck->SetToolTip("Saves plain text history logs of all conversations and channel text onto your local system disk.");

        fEnableColorCodesCheck = new BCheckBox("enable_color_codes", "Enable Visual Block Tags for mIRC color codes", nullptr);
        fEnableColorCodesCheck->SetValue(srv.enableColorCodes ? B_CONTROL_ON : B_CONTROL_OFF);
        fEnableColorCodesCheck->SetToolTip("Parses legacy mIRC rich color code tags into colored block backgrounds inside the window.");



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
        
        // --- Center Aligned Server Title Header ---
        BStringView* titleHeader = new BStringView("server_title_header", srv.name.c_str());
        titleHeader->SetAlignment(B_ALIGN_CENTER);
        
        // Make the text stand out slightly by setting its font style to Bold
        BFont titleFont;
        titleHeader->GetFont(&titleFont);
        titleFont.SetFace(B_BOLD_FACE);
        titleHeader->SetFont(&titleFont, B_FONT_FACE);

         // --- Window Actions ---
        BButton* cancelBtn = new BButton("cancel", "Cancel", new BMessage('cfcn'));
        BButton* saveBtn = new BButton("save", "Save", new BMessage('cfsv'));
        saveBtn->MakeDefault(true);


        // --- NEW: IGNORE FILTERS INTERFACE COMPONENTS ---
        fIgnoreListView = new BListView("ignore_list");
        BScrollView* ignoreScroll = new BScrollView("ignore_scroll", fIgnoreListView, 0, false, true);
        ignoreScroll->SetExplicitMinSize(BSize(150, 100)); // Keeps the framing size identical to autojoin

        // Populate the interactive ignore list from configuration data
        fLocalIgnoreList = srv.ignoredNicks; 
        for (const auto& nick : fLocalIgnoreList) {
            if (!nick.empty()) {
                fIgnoreListView->AddItem(new BStringItem(nick.c_str()));
            }
        }

        // Custom action triggers
        fIgnoreInput = new BTextControl("ignore_input", "", "", nullptr);
        fAddIgnoreBtn = new BButton("add_ignore", "Add", new BMessage('igad'));
        fRemoveIgnoreBtn = new BButton("rem_ignore", "Remove", new BMessage('igrm'));

        // --- NEW: CUSTOM NICKNAME COLORS INTERFACE COMPONENTS ---
        fColorListView = new BListView("color_nicks_list");
        BScrollView* colorScroll = new BScrollView("color_scroll", fColorListView, 0, false, true);
        colorScroll->SetExplicitMinSize(BSize(150, 60));
       fColorListView->SetSelectionMessage(new BMessage(0xCC005544));

        // Populate color entries from configuration data arrays
        // (Assumes srv.nickColors and srv.nickColorValues vectors exist in ServerConfig structure)
        fLocalColorNicknames = srv.nickColors;
        fLocalColorValues = srv.nickColorValues;
        for (const auto& nick : fLocalColorNicknames) {
            if (!nick.empty()) {
                fColorListView->AddItem(new BStringItem(nick.c_str()));
            }
        }

        fColorNickInput = new BTextControl("color_nick_input", "Target Nickname:", "", nullptr);
        // Instantiates a layout-compact 32-cell native Haiku block palette picker
        fColorPicker = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 1.0f, "color_picker");
        fAddColorBtn = new BButton("add_color", "Assign Color", new BMessage('clad'));
        fRemoveColorBtn = new BButton("rem_color", "Remove Color", new BMessage('clrm'));

        // --- Create Tab View Architecture ---
        BTabView* tabView = new BTabView("config_tabs", B_WIDTH_AS_USUAL);

        // --- TAB 1: IDENTITY & CREDS ---
        BGroupView* identityTab = new BGroupView(B_VERTICAL, 5);
        identityTab->SetName("Identity");
        BLayoutBuilder::Group<>(identityTab, B_VERTICAL, 0)
            .SetInsets(10)
            .AddGrid(5.0f, 5.0f)
                .Add(fNickInput->CreateLabelLayoutItem(), 0, 0)
                .Add(fNickInput->CreateTextViewLayoutItem(), 1, 0)
                
                .Add(fAltNickInput->CreateLabelLayoutItem(), 0, 1)
                .Add(fAltNickInput->CreateTextViewLayoutItem(), 1, 1)
                
                .Add(fAltNick2Input->CreateLabelLayoutItem(), 0, 2)
                .Add(fAltNick2Input->CreateTextViewLayoutItem(), 1, 2)
                
                .Add(fPassInput->CreateLabelLayoutItem(), 0, 3)     
                .Add(fPassInput->CreateTextViewLayoutItem(), 1, 3)

                // --- SASL Section (Rows 4 & 5) ---
                .Add(fUseSASLCheck, 0, 4, 2, 1)
                .Add(fSASLUserInput->CreateLabelLayoutItem(), 0, 5)
                .Add(fSASLUserInput->CreateTextViewLayoutItem(), 1, 5)

                // --- CertFP Section (Rows 6, 7, 8, 9, 10 & 11) ---
                .Add(fUseCertFPCheck, 0, 6, 2, 1)
                
                .Add(fCertProfileInput->CreateLabelLayoutItem(), 0, 7)
                .Add(fCertProfileInput->CreateTextViewLayoutItem(), 1, 7)

                .Add(fCertFileInput->CreateLabelLayoutItem(), 0, 8)
                .Add(fCertFileInput->CreateTextViewLayoutItem(), 1, 8)

                .Add(fKeyFileInput->CreateLabelLayoutItem(), 0, 9)
                .Add(fKeyFileInput->CreateTextViewLayoutItem(), 1, 9)
                
                .Add(fGenerateCertsButton, 0, 10, 2, 1)
                
                .Add(certLocationNotice, 0, 11, 2, 1)
            .End()
            .AddGlue();


 



        // --- TAB 2: PREFERENCES & THEMING (COMPACT LAYOUT WITH TIMESTAMP OPTION) ---
        BGroupView* prefsTab = new BGroupView(B_VERTICAL, 5);
        prefsTab->SetName("Preferences");
        
        BGroupView* scrollableCheckboxes = new BGroupView(B_VERTICAL, 4);
        BLayoutBuilder::Group<>(scrollableCheckboxes, B_VERTICAL, 4)
            .Add(fAutoConnectCheck)
            .Add(fAutoReconnectCheck)
            .Add(fHideStatusCheck)
            .Add(fEnableEmoticonsCheck)
            .Add(fUseCustomDrawCheck)
            .Add(fLogChatsToFileCheck)
            .Add(fEnableColorCodesCheck);

        // --- NEW: INSTANTIATE THE TIMESTAMP INTERVAL POPUP MENU ---
        BPopUpMenu* tsMenu = new BPopUpMenu("timestamp_select");
        int32 intervals[] = { 5, 10, 15, 30, 60 };
        
        for (int32 i = 0; i < 5; i++) {
            BString label;
            label << "Every " << intervals[i] << " minutes";
            
            // Create item message containing the target minutes integer token payload
            BMessage* msg = new BMessage('tsch'); // Timestamp changed token constant
            msg->AddInt32("minutes", intervals[i]);
            
            BMenuItem* item = new BMenuItem(label.String(), msg);
            if (intervals[i] == srv.timestampInterval) {
                item->SetMarked(true); // Pre-select their active saved choice matching disk
            }
            tsMenu->AddItem(item);
        }
        fTimestampMenu = new BMenuField("ts_menu", "Timestamp Frequency:", tsMenu);

        BLayoutBuilder::Group<>(prefsTab, B_HORIZONTAL, 15)
            .SetInsets(12)
            .AddGroup(B_VERTICAL, 5, 0.55)
                // 3-COLUMN CONTROL GRID MATRIX
                .AddGrid(5.0f, 5.0f)
                    .Add(fServerListFontMenu->CreateLabelLayoutItem(), 0, 0)
                    .Add(fServerListFontMenu->CreateMenuBarLayoutItem(), 1, 0, 2, 1)
                    .Add(fChatLogFontMenu->CreateLabelLayoutItem(), 0, 1)
                    .Add(fChatLogFontMenu->CreateMenuBarLayoutItem(), 1, 1, 2, 1)
                    .Add(fUserListFontMenu->CreateLabelLayoutItem(), 0, 2)
                    .Add(fUserListFontMenu->CreateMenuBarLayoutItem(), 1, 2, 2, 1)
                    .Add(fBgPathInput->CreateLabelLayoutItem(), 0, 3)
                    .Add(fBgPathInput->CreateTextViewLayoutItem(), 1, 3, 1, 1)
                    .Add(fBrowseBgBtn, 2, 3)
                .End()
                .Add(fBgOpacitySlider)
                // --- INJECTED DIRECTLY UNDERNEATH WALLPAPER SLIDER ---
                .AddStrut(5.0f) // Soft micro-padding gap separator spacer
                .Add(fTimestampMenu)
                .AddGlue() 
            .End()
            .Add(scrollableCheckboxes, 0.45);

        // --- TAB 3: AUTOJOIN INTERFACE ---
        BGroupView* autojoinTab = new BGroupView(B_VERTICAL, 5);
        autojoinTab->SetName("Autojoin");
        
        BBox* autojoinBox = new BBox(B_FANCY_BORDER);
        autojoinBox->SetLabel("Autojoin Channels");
        BLayoutBuilder::Group<>(autojoinBox, B_VERTICAL, 5)
            .SetInsets(10, 20, 10, 10)
            .Add(autojoinScroll, 1.0)
            .AddGroup(B_HORIZONTAL, 5)
                .Add(fAutojoinInput, 1.0)
                .Add(fAddAutojoinBtn, 0.0)
                .Add(fRemoveAutojoinBtn, 0.0)
            .End();

        BLayoutBuilder::Group<>(autojoinTab, B_VERTICAL, 0)
            .SetInsets(10)
            .Add(autojoinBox, 1.0);          

        // --- TAB 4: FILTERS & COLORS INTERFACE ---
        BGroupView* filtersTab = new BGroupView(B_VERTICAL, 5);
        filtersTab->SetName("Filters");
        
        // A. Top Box Section: Color Setup Matrix
        BBox* colorBox = new BBox(B_FANCY_BORDER);
        colorBox->SetLabel("Custom Nickname Coloring");
        BLayoutBuilder::Group<>(colorBox, B_HORIZONTAL, 10)
            .SetInsets(10, 20, 10, 10)
            .Add(colorScroll, 1.0)
            .AddGroup(B_VERTICAL, 5, 1.0)
                .Add(fColorNickInput)
                .Add(fColorPicker)
                .AddGroup(B_HORIZONTAL, 5)
                    .AddGlue()
                    .Add(fRemoveColorBtn)
                    .Add(fAddColorBtn)
                .End()
            .End();

        // B. Bottom Box Section: Spam / Wildcard Ignorers
        BBox* filtersBox = new BBox(B_FANCY_BORDER);
        filtersBox->SetLabel("Ignored Nicknames");
        BLayoutBuilder::Group<>(filtersBox, B_VERTICAL, 5)
            .SetInsets(10, 20, 10, 10) 
            .Add(ignoreScroll, 1.0)
            .AddGroup(B_HORIZONTAL, 5)
                .Add(fIgnoreInput, 1.0)
                .Add(fAddIgnoreBtn, 0.0)
                .Add(fRemoveIgnoreBtn, 0.0)
            .End();

        // Unified layout notice string positioned as a clean master header element
        BStringView* wildcardNotice = new BStringView("wildcard_notice", 
            "💡 Both Colored Nicknames and Ignored Nicknames support wildcard filters (e.g., nickname* or *nickname).");

        // Combine all layout blocks vertically from top to bottom
        BLayoutBuilder::Group<>(filtersTab, B_VERTICAL, 8)
            .SetInsets(10)
            .Add(wildcardNotice, 0.0) // Positioned directly above both configurations boxes
            .Add(colorBox, 1.0)     
            .Add(filtersBox, 1.0);

        // --- Assemble Tabs into the Window ---
        tabView->AddTab(identityTab);
        tabView->AddTab(prefsTab);
        tabView->AddTab(autojoinTab);
        tabView->AddTab(filtersTab); 

        // --- Master Window Root Layout ---
        BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
            .SetInsets(12)
            .Add(titleHeader, 0.0) 
            .Add(tabView, 1.0)     
            
            .AddGrid(5.0f, 5.0f)
                .Add(fAwayInput->CreateLabelLayoutItem(), 0, 0)
                .Add(fAwayInput->CreateTextViewLayoutItem(), 1, 0, 2, 1) 
                .Add(fQuitInput->CreateLabelLayoutItem(), 3, 0)
                .Add(fQuitInput->CreateTextViewLayoutItem(), 4, 0, 2, 1) 
                
                .Add(cancelBtn, 4, 1)
                .Add(saveBtn, 5, 1)
            .End();

        ResizeTo(560, 440);    

            
        if (parent) {
            CenterIn(parent->Frame()); 
        } else {
            CenterOnScreen(); 
        }
    }


    void MessageReceived(BMessage* message) override {
        switch (message->what) {
        	
	     case 'sasl': {
	         fSASLUserInput->SetEnabled(fUseSASLCheck->Value() == B_CONTROL_ON);
	         break;
	     }
        case 'cfpa': { // CertFP Toggle Message
            fCertProfileInput->SetEnabled(fUseCertFPCheck->Value() == B_CONTROL_ON);
            fCertFileInput->SetEnabled(fUseCertFPCheck->Value() == B_CONTROL_ON);
            fKeyFileInput->SetEnabled(fUseCertFPCheck->Value() == B_CONTROL_ON);
            fGenerateCertsButton->SetEnabled(fUseCertFPCheck->Value() == B_CONTROL_ON);
            break;
        }

        case MSG_GENERATE_CERTFP_KEYPAIR: {
            // 1. Resolve active target nickname strings to name the cert profile cleanly
            BString activeNickToken = fNickInput->Text();
            activeNickToken.Trim();
            if (activeNickToken.Length() == 0 || activeNickToken == "HaikuIRCUser") {
                activeNickToken = "cricket_user";
            }
            activeNickToken.ToLower();

            // 2. Extrapolate standard target file directories profile structures
            BPath certDirectoryPath;
            if (find_directory(B_USER_SETTINGS_DIRECTORY, &certDirectoryPath) == B_OK) {
                certDirectoryPath.Append("cricket/certs");
                create_directory(certDirectoryPath.Path(), 0755); // Safeguard creation pass

                BString pemFilename = BString(activeNickToken) << ".pem"; // NEW: Unified SASL container name
                BString crtFilename = BString(activeNickToken) << ".crt";
                BString keyFilename = BString(activeNickToken) << ".key";

                BString fullPemPath = BString(certDirectoryPath.Path()) << "/" << pemFilename; // NEW: Path target
                BString fullCrtPath = BString(certDirectoryPath.Path()) << "/" << crtFilename;
                BString fullKeyPath = BString(certDirectoryPath.Path()) << "/" << keyFilename;

                // 3. Assemble the unified native command parameter execution string block
                // Generates an unencrypted 2048-bit RSA key first, then chains a self-signed cert 
                // valid for 3650 days (10 years) with basic placeholder subject metadata signatures.
                BString openSslCmdString;
                openSslCmdString.SetToFormat(
                    "openssl req -new -newkey rsa:2048 -days 3650 -nodes -x509 "
                    "-subj \"/CN=%s/O=CricketClient/OU=IRC/\" "
                    "-keyout \"%s\" -out \"%s\" > /dev/null 2>&1",
                    activeNickToken.String(), fullKeyPath.String(), fullCrtPath.String()
                );

                if (cfg.debugEnable) printf("[DEBUG_CERT] Executing background system cryptographic generation tool pass...\n");
                
                // Invoke standard shell execution
                int systemResultCode = system(openSslCmdString.String());

                if (systemResultCode == 0) {
                    // 1. Bundle the crt and key together into the .pem file for backwards compatibility
                    BString pemFilename = BString(activeNickToken) << ".pem";
                    BString fullPemPath = BString(certDirectoryPath.Path()) << "/" << pemFilename;
                    
                    BString bundleCmdString;
                    bundleCmdString.SetToFormat("cat \"%s\" \"%s\" > \"%s\"", 
                        fullCrtPath.String(), fullKeyPath.String(), fullPemPath.String());
                    system(bundleCmdString.String());

                    // =========================================================================
                    //  AUTOMATED SHA-1 FINGERPRINT EXTRACTION ENGINE
                    // =========================================================================
                    // Executes the precise pipeline to capture the clean lowercase 40-character string
                    BString fingerprintCmd;
                    fingerprintCmd.SetToFormat(
                        "openssl x509 -noout -fingerprint -sha1 -in \"%s\" | "
                        "sed 's/SHA1 Fingerprint=//g' | tr -d ':' | tr '[:upper:]' '[:lower:]' | tr -d '\\n' > \"%s/hash.tmp\"",
                        fullCrtPath.String(), certDirectoryPath.Path()
                    );
                    system(fingerprintCmd.String());

                    // Read the hash string output back out of the temporary file cache
                    BString cleanFingerprint = "";
                    BString tmpFilePath = BString(certDirectoryPath.Path()) << "/hash.tmp";
                    FILE* hashFile = fopen(tmpFilePath.String(), "r");
                    if (hashFile != nullptr) {
                        char readBuffer[64] = {0};
                        if (fgets(readBuffer, sizeof(readBuffer), hashFile) != nullptr) {
                            cleanFingerprint = readBuffer;
                            cleanFingerprint.Trim();
                        }
                        fclose(hashFile);
                        remove(tmpFilePath.String()); // Delete temporary file instantly
                    }
                    // =========================================================================
                    
                    // 2. Auto-populate the preference input fields immediately
                    fCertProfileInput->SetText(pemFilename.String());
                    fCertFileInput->SetText(crtFilename.String());
                    fKeyFileInput->SetText(keyFilename.String());
                    
                    // 3.
                    if (this->fTarget != nullptr && cleanFingerprint.Length() == 40) {
                        BMessage* regMessage = new BMessage(MSG_AUTO_REGISTER_FINGERPRINT);
                        regMessage->AddString("fingerprint", cleanFingerprint);
                        this->fTarget->PostMessage(regMessage);
                    }


                    
                    // 4. Inform the user with an explicit alert to restart the client
                    BAlert* successNotification = new BAlert("CertFP Security Setup",
                        BString("Success! Generated your secure 2048-bit CertFP keypair profile.\n\n"
                                "Clean Fingerprint: ") << cleanFingerprint << "\n\n"
                                "🚀 Cricket has automatically dispatched the 'CERT ADD' registration command to NickServ!\n\n"
                                "Please click 'Save' and restart Cricket IRC for your secure certificate passport to apply completely.",
                        "Excellent", nullptr, nullptr, B_WIDTH_AS_USUAL, B_INFO_ALERT);
                    successNotification->Go();
                } else {

                    if (cfg.debugEnable) printf("[DEBUG_CERT] CRITICAL FAIL: OpenSSL generation process returned error status code: %d\n", systemResultCode);
                    BAlert* errorNotification = new BAlert("CertFP Security Error",
                        "Failed to generate certificates. Please ensure the 'openssl' command-line packages "
                        "are installed natively via HaikuDepot on your device.",
                        "Understood", nullptr, nullptr, B_WIDTH_AS_USUAL, B_STOP_ALERT);
                    errorNotification->Go();
                }
            }
            break;
        }



         case 'tsch': {
            int32 minutes = 30;
            if (message->FindInt32("minutes", &minutes) == B_OK) {
                if (fIsCustom) {
                    if (fServerIdx < cfg.customServers.size()) cfg.customServers[fServerIdx].timestampInterval = minutes;
                } else {
                    if (fServerIdx < cfg.servers.size()) cfg.servers[fServerIdx].timestampInterval = minutes;
                }
                save_config();
            }
            break;
        }
 
        // =========================================================================
        // PROPERTIES DIALOG ACTION: SELECTION CHANGED IN COLOR ROSTER
        // =========================================================================
        case 0xCC005544: {
            int32 selection = fColorListView->CurrentSelection();
            if (selection >= 0) {
                BStringItem* item = static_cast<BStringItem*>(fColorListView->ItemAt(selection));
                if (item != nullptr && item->Text() != nullptr) {
                    
                    // 1. Pre-populate the text input box with the selected nickname handle
                    fColorNickInput->SetText(item->Text());

                    // 2. Safeguard bounds check against our local parallel configuration vectors
                    if (selection >= 0 && selection < (int32)fLocalColorValues.size()) {
                        rgb_color savedColor = fLocalColorValues[selection];
                        
                        // 3. Force the native BColorControl sliders to update their visual states instantly
                        fColorPicker->SetValue(savedColor);
                    }
                }
            }
            break;
        }

 
  
        // =========================================================================
        // HANDLER FOR WINDOW SYNCHRONIZATION: PRE-FILL COLOR TARGET NICKNAME ('fllc')
        // =========================================================================
        case 'fllc': {
            BString targetNick;
            if (message->FindString("nick", &targetNick) == B_OK) {
                // 1. Force the Tab View layout to select TAB 3 (Index 3 is Filters)
                // (Assumes tabView pointer is tracked or findable via FindView)
                BTabView* tabs = dynamic_cast<BTabView*>(FindView("config_tabs"));
                if (tabs != nullptr) {
                    tabs->Select(3);
                }

                // 2. Pre-fill text and snap cursor focus to the user coloring controls
                if (fColorNickInput != nullptr) {
                    fColorNickInput->SetText(targetNick.String());
                    fColorNickInput->MakeFocus(true);
                }
            }
            break;
        }

  
  
        	
          // --- ADD / UPDATE CUSTOM NICKNAME COLOR ('clad') ---
        case 'clad': {
            BString targetNick = fColorNickInput->Text();
            targetNick.Trim();
            if (targetNick.Length() > 0) {
                rgb_color chosenColor = fColorPicker->ValueAsColor();
                
                // 1. Scan local vector array using a true BString conversion
                int32 existingIndex = -1;
                for (size_t i = 0; i < fLocalColorNicknames.size(); ++i) {
                    BString existingNick(fLocalColorNicknames[i].c_str());
                    if (targetNick.ICompare(existingNick) == 0) {
                        existingIndex = static_cast<int32>(i);
                        break;
                    }
                }

                if (existingIndex != -1) {
                    // --- PATH A: OVERWRITE EXISTING RULE COLOR ---
                    fLocalColorValues[existingIndex] = chosenColor;
                    fColorListView->InvalidateItem(existingIndex);
                } else {
                    // --- PATH B: APPEND A BRAND NEW NICKNAME RULE ---
                    // Force clean conversion to standard std::string object explicitly
                    fLocalColorNicknames.push_back(std::string(targetNick.String()));
                    fLocalColorValues.push_back(chosenColor);
                    
                    // Explicitly pass targetNick directly into the constructor
                    fColorListView->AddItem(new BStringItem(targetNick));
                }


                fColorNickInput->SetText(""); // Clear box input field

                // 2. Sync modifications directly down to the running server row item
                if (fItem != nullptr) {
                    fItem->fRuntimeColorNicks = fLocalColorNicknames;
                    fItem->fRuntimeColorValues = fLocalColorValues;
                }

                // 3. Sync changes directly into the global app config engine vectors
                if (fIsCustom) {
                    if (fServerIdx < cfg.customServers.size()) {
                        cfg.customServers[fServerIdx].nickColors = fLocalColorNicknames;
                        cfg.customServers[fServerIdx].nickColorValues = fLocalColorValues;
                    }
                } else {
                    if (fServerIdx < cfg.servers.size()) {
                        cfg.servers[fServerIdx].nickColors = fLocalColorNicknames;
                        cfg.servers[fServerIdx].nickColorValues = fLocalColorValues;
                    }
                }

                // 4. Export the data immediately out to text file on the hard disk
                save_config();
            }
            break;
        }


        // --- REMOVE CUSTOM NICKNAME COLOR ('clrm') ---
        case 'clrm': {
            int32 selection = fColorListView->CurrentSelection();
            if (selection >= 0) {
                fLocalColorNicknames.erase(fLocalColorNicknames.begin() + selection);
                fLocalColorValues.erase(fLocalColorValues.begin() + selection);
                BStringItem* item = static_cast<BStringItem*>(fColorListView->RemoveItem(selection));
                delete item;

                // 1. Sync right to the runtime node
                if (fItem != nullptr) {
                    fItem->fRuntimeColorNicks = fLocalColorNicknames;
                    fItem->fRuntimeColorValues = fLocalColorValues;
                }

                // 2.  Remove directly from the global config arrays via index bounds
                if (fIsCustom) {
                    if (fServerIdx < cfg.customServers.size()) {
                        cfg.customServers[fServerIdx].nickColors = fLocalColorNicknames;
                        cfg.customServers[fServerIdx].nickColorValues = fLocalColorValues;
                    }
                } else {
                    if (fServerIdx < cfg.servers.size()) {
                        cfg.servers[fServerIdx].nickColors = fLocalColorNicknames;
                        cfg.servers[fServerIdx].nickColorValues = fLocalColorValues;
                    }
                }

                // 3. Save clean layout tables out to disk
                save_config();
            }
            break;
        }


        // =========================================================================
        // PROPERTIES DIALOG ACTION: ADD TYPED NICK TO LIST BOX ('igad')
        // =========================================================================
        case 'igad': {
            BString newNick = fIgnoreInput->Text();
            newNick.Trim();
            if (newNick.Length() > 0) {
                // 1. Prevent adding duplicate entries case-insensitively
                bool duplicate = false;
                for (auto& n : fLocalIgnoreList) {
                    if (newNick.ICompare(n.c_str()) == 0) {
                        duplicate = true;
                        break;
                    }
                }
                
                if (!duplicate) {
                    // 2. Add to local workspace tracking array
                    fLocalIgnoreList.push_back(newNick.String());
                    fIgnoreListView->AddItem(new BStringItem(newNick.String()));
                    fIgnoreInput->SetText(""); // Clear box input field execution frame row

                    // 3. Force synchronization back to runtime ServerTreeItem instantly
                    if (fItem != nullptr) {
                        fItem->fRuntimeIgnoreList = fLocalIgnoreList;
                    }

                    // 4. Update back-end persistent config data structures
                    ServerConfig& srv = GetActiveConfig();
                    srv.ignoredNicks = fLocalIgnoreList;
                    
                    // 5. Instantly save the updated settings file out to the hard disk
                    save_config();
                }
            }
            break;
        }

        // =========================================================================
        // PROPERTIES DIALOG ACTION: REMOVE NICK FROM LIST BOX ('igrm')
        // =========================================================================
        case 'igrm': {
            int32 selection = fIgnoreListView->CurrentSelection();
            if (selection >= 0) {
                BStringItem* item = static_cast<BStringItem*>(fIgnoreListView->ItemAt(selection));
                if (item != nullptr && item->Text() != nullptr) {
                    
                    // 1. Isolate the target nickname string from the selected row
                    std::string targetNick(item->Text());
                    
                    // 2. Erase the name from the local layout tracking vector array
                    fLocalIgnoreList.erase(
                        std::remove(fLocalIgnoreList.begin(), fLocalIgnoreList.end(), targetNick),
                        fLocalIgnoreList.end()
                    );
                    
                    // 3. Remove the graphic element row from the BListView and delete its memory allocation
                    fIgnoreListView->RemoveItem(selection);
                    delete item;

                    // 4. Force synchronization back to runtime ServerTreeItem instantly
                    if (fItem != nullptr) {
                        fItem->fRuntimeIgnoreList = fLocalIgnoreList;
                    }

                    // 5. Update back-end persistent config data structures
                    ServerConfig& srv = GetActiveConfig();
                    srv.ignoredNicks = fLocalIgnoreList;
                    
                    // 6. Instantly save the settings file map out to the hard disk
                    save_config();
                }
            }
            break;
        }

            
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
                
				srv.useSASL = (fUseSASLCheck->Value() == B_CONTROL_ON);
				srv.saslUser = fSASLUserInput->Text();
				
				srv.useCertFP = (fUseCertFPCheck->Value() == B_CONTROL_ON);
				srv.certProfileName = fCertProfileInput->Text();
                srv.certFileName = fCertFileInput->Text();
                srv.keyFileName  = fKeyFileInput->Text();
                
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
                srv.enableColorCodes = (fEnableColorCodesCheck->Value() == B_CONTROL_ON);
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
	BWindow*       		fTarget;
	BButton*            fGenerateCertsButton;
    BTextControl*       fCertFileInput; 
    BTextControl*       fKeyFileInput;  
	BCheckBox*    		fUseCertFPCheck;   
	BTextControl* 		fCertProfileInput; 
    BCheckBox*        	fUseSASLCheck;
    BTextControl*     	fSASLUserInput;    
    BMenuField*         fTimestampMenu;
    std::vector<std::string> fLocalColorNicknames;
    std::vector<rgb_color>   fLocalColorValues;
    BListView*    		fColorListView;
    BTextControl* 		fColorNickInput;
    BColorControl* 		fColorPicker; 
    BButton*      		fAddColorBtn;
    BButton*      		fRemoveColorBtn;
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
	BCheckBox*          fEnableColorCodesCheck; 
    std::vector<std::string> fLocalIgnoreList;
    BListView*               fIgnoreListView;
    BTextControl*            fIgnoreInput;
    BButton*                 fAddIgnoreBtn;
    BButton*                 fRemoveIgnoreBtn;
	
    //  Safe helper function to fetch correct active reference target safely
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

    // Fires right before the window closes to alert our parent window thread
    virtual void Quit() {
        // Send a notification message thread-safely across the boundary 
        fTargetMessenger.SendMessage(MSG_POPUP_WAS_DESTROYED);
        BWindow::Quit();
    }

private:
    BMessenger fTargetMessenger;
};





class RightClickSingleClickFilter : public BMessageFilter {
public:
    RightClickSingleClickFilter(BHandler* targetHandler) 
        : BMessageFilter(B_MOUSE_DOWN), fTargetHandler(targetHandler) {}

    virtual filter_result Filter(BMessage* message, BHandler** target) {
        int32 buttons;
        BPoint point;
        
        // 1. Intercept ONLY the right mouse button click
        if (message->FindInt32("buttons", &buttons) == B_OK && (buttons & B_SECONDARY_MOUSE_BUTTON)) {
            if (message->FindPoint("where", &point) == B_OK) {
                
                // 2. Package the point up and bypass the list view logic completely
                BMessage contextMsg(MSG_USER_LIST_CONTEXT_CLICK);
                contextMsg.AddPoint("where", point);
                fTargetHandler->Looper()->PostMessage(&contextMsg, fTargetHandler);
                
                return B_SKIP_MESSAGE; // Swallow it so BListView won't mess with selection or require double-clicks!
            }
        }
        return B_DISPATCH_MESSAGE; // Let normal left clicks select items safely
    }
private:
    BHandler* fTargetHandler;
};




class ChannelModesDialog : public BWindow {
public:
    virtual ~ChannelModesDialog() {}

    ChannelModesDialog(BWindow* parent, void* contextServer, const BString& channelName, bool isOperator)
        : BWindow(BRect(0, 0, 460, 360), "Channel Modes Management", B_TITLED_WINDOW_LOOK, 
                  B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE) {
        
        fParentWindow = parent;
        fContextServer = contextServer;
        fChannelName = channelName;
        fIsOperator = isOperator;

        AddToSubset(parent);

        BRect parentFrame = parent->Frame();
        MoveTo(parentFrame.left + (parentFrame.Width() - 460) / 2,
               parentFrame.top + (parentFrame.Height() - 360) / 2);

        BView* panel = new BView(Bounds(), "modesPanel", B_FOLLOW_ALL, B_WILL_DRAW);
        panel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        AddChild(panel);

        BString titleStr = "Manage Modes for ";
        titleStr << fChannelName;
        BTextView* titleText = new BTextView(BRect(15, 15, 445, 40), "title", BRect(0,0,430,20), B_FOLLOW_ALL, B_WILL_DRAW);
        titleText->SetText(titleStr.String());
        titleText->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        titleText->MakeEditable(false);
        titleText->MakeSelectable(false);
        panel->AddChild(titleText);

        fModeratedCheck = new BCheckBox(BRect(20, 50, 210, 75), "mod", "Moderated (+m)", nullptr);
        fSecretCheck    = new BCheckBox(BRect(20, 80, 210, 105), "sec", "Secret Channel (+s)", nullptr);
        fInviteCheck    = new BCheckBox(BRect(20, 110, 210, 135), "inv", "Invite Only (+i)", nullptr);
        fTopicCheck     = new BCheckBox(BRect(20, 140, 210, 165), "top", "Ops Topic Only (+t)", nullptr);
        fNoExtCheck     = new BCheckBox(BRect(20, 170, 210, 195), "noext", "No Ext. Msgs (+n)", nullptr);
        fNoColorCheck   = new BCheckBox(BRect(20, 200, 210, 225), "nocol", "Block Colors (+c)", nullptr);

        panel->AddChild(fModeratedCheck);
        panel->AddChild(fSecretCheck);
        panel->AddChild(fInviteCheck);
        panel->AddChild(fTopicCheck);
        panel->AddChild(fNoExtCheck);
        panel->AddChild(fNoColorCheck);

        fKeyInput   = new BTextControl(BRect(230, 50, 440, 75), "key_in", "Room Key:", "", nullptr);
        fLimitInput = new BTextControl(BRect(230, 90, 440, 115), "lim_in", "User Limit:", "", nullptr);
        
        fKeyInput->SetDivider(panel->StringWidth("Room Key: ") + 5);
        fLimitInput->SetDivider(panel->StringWidth("User Limit: ") + 5);
        
        panel->AddChild(fKeyInput);
        panel->AddChild(fLimitInput);

        if (!fIsOperator) {
            fModeratedCheck->SetEnabled(false);
            fSecretCheck->SetEnabled(false);
            fInviteCheck->SetEnabled(false);
            fTopicCheck->SetEnabled(false);
            fNoExtCheck->SetEnabled(false);
            fNoColorCheck->SetEnabled(false);
            fKeyInput->SetEnabled(false);
            fLimitInput->SetEnabled(false);

            BTextView* warnText = new BTextView(BRect(20, 240, 440, 280), "warn", BRect(0,0,420,40), B_FOLLOW_ALL, B_WILL_DRAW);
            warnText->SetText("Warning: You must have operator status (@) to change these flags.");
            warnText->SetFont(be_plain_font);
            warnText->SetHighColor(220, 100, 100);
            warnText->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
            warnText->MakeEditable(false);
            warnText->MakeSelectable(false);
            panel->AddChild(warnText);
        }

        BButton* cancelBtn = new BButton(BRect(240, 315, 335, 342), "cancel", "Cancel", new BMessage('mdcn'));
        BButton* applyBtn = new BButton(BRect(345, 315, 440, 342), "apply", "Apply", new BMessage('mdap'));
        
        cancelBtn->SetTarget(this);
        applyBtn->SetTarget(this);
        applyBtn->MakeDefault(true);

        if (!fIsOperator) {
            applyBtn->SetEnabled(false);
        }

        panel->AddChild(cancelBtn);
        panel->AddChild(applyBtn);

        if (fParentWindow != nullptr) {
            BMessage registerMsg('rgmd');
            registerMsg.AddPointer("dialog_ptr", this);
            registerMsg.AddString("channel_target", fChannelName);
            fParentWindow->PostMessage(&registerMsg);
        }

        BString queryCmd;
        queryCmd << "MODE " << fChannelName << "\r\n";
        SendSecurePayload(queryCmd);
    }

    void UpdateCheckedStates(bool m, bool s, bool i, bool t, bool n, bool c, const char* key, const char* limit) {
        if (Lock()) {
            fModeratedCheck->SetValue(m ? B_CONTROL_ON : B_CONTROL_OFF);
            fSecretCheck->SetValue(s ? B_CONTROL_ON : B_CONTROL_OFF);
            fInviteCheck->SetValue(i ? B_CONTROL_ON : B_CONTROL_OFF);
            fTopicCheck->SetValue(t ? B_CONTROL_ON : B_CONTROL_OFF);
            fNoExtCheck->SetValue(n ? B_CONTROL_ON : B_CONTROL_OFF);
            fNoColorCheck->SetValue(c ? B_CONTROL_ON : B_CONTROL_OFF);
            
            fKeyInput->SetText(key ? key : "");
            fLimitInput->SetText(limit ? limit : "");
            Unlock();
        }
    }
    
    // Public getter accessors with automated lock protection to stop thread race crashes
    bool IsModerated()  { bool v = false; if (Lock()) { v = (fModeratedCheck->Value() == B_CONTROL_ON); Unlock(); } return v; }
    bool IsSecret()     { bool v = false; if (Lock()) { v = (fSecretCheck->Value() == B_CONTROL_ON); Unlock(); } return v; }
    bool IsInviteOnly() { bool v = false; if (Lock()) { v = (fInviteCheck->Value() == B_CONTROL_ON); Unlock(); } return v; }
    bool IsTopicOps()   { bool v = false; if (Lock()) { v = (fTopicCheck->Value() == B_CONTROL_ON); Unlock(); } return v; }
    bool IsNoExt()      { bool v = false; if (Lock()) { v = (fNoExtCheck->Value() == B_CONTROL_ON); Unlock(); } return v; }
    bool IsBlockColor() { bool v = false; if (Lock()) { v = (fNoColorCheck->Value() == B_CONTROL_ON); Unlock(); } return v; }

    // Returning BString objects instead of const char* prevents memory freeing violations
    BString GetKeyText() {
        BString text = "";
        if (Lock()) {
            text = fKeyInput->Text();
            Unlock();
        }
        return text;
    }

    BString GetLimitText() {
        BString text = "";
        if (Lock()) {
            text = fLimitInput->Text();
            Unlock();
        }
        return text;
    }

    virtual bool QuitRequested() override {
        if (fParentWindow != nullptr) {
            BMessage unregisterMsg('unmd');
            
            // SYNCHRONOUS FIX: Blocks the dialog thread until CricketWindow 
            // runs the 'unmd' case and sets fActiveModesDialog = nullptr!
            BMessenger messenger(fParentWindow);
            messenger.SendMessage(&unregisterMsg);
        }
        return BWindow::QuitRequested();
    }


    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case 'mdcn': {
                Quit();
                break;
            }
            case 'mdap': {
                if (fIsOperator) {
                    BString cmdM; cmdM << "MODE " << fChannelName << " " << ((fModeratedCheck->Value() == B_CONTROL_ON) ? "+m" : "-m") << "\r\n"; SendSecurePayload(cmdM);
                    BString cmdS; cmdS << "MODE " << fChannelName << " " << ((fSecretCheck->Value() == B_CONTROL_ON) ? "+s" : "-s") << "\r\n"; SendSecurePayload(cmdS);
                    BString cmdI; cmdI << "MODE " << fChannelName << " " << ((fInviteCheck->Value() == B_CONTROL_ON) ? "+i" : "-i") << "\r\n"; SendSecurePayload(cmdI);
                    BString cmdT; cmdT << "MODE " << fChannelName << " " << ((fTopicCheck->Value() == B_CONTROL_ON) ? "+t" : "-t") << "\r\n"; SendSecurePayload(cmdT);
                    BString cmdN; cmdN << "MODE " << fChannelName << " " << ((fNoExtCheck->Value() == B_CONTROL_ON) ? "+n" : "-n") << "\r\n"; SendSecurePayload(cmdN);
                    BString cmdC; cmdC << "MODE " << fChannelName << " " << ((fNoColorCheck->Value() == B_CONTROL_ON) ? "+c" : "-c") << "\r\n"; SendSecurePayload(cmdC);

                    BString keyStr = fKeyInput->Text();
                    keyStr.Trim();
                    BString cmdKey;
                    if (keyStr.Length() > 0) {
                        cmdKey << "MODE " << fChannelName << " +k " << keyStr << "\r\n";
                    } else {
                        cmdKey << "MODE " << fChannelName << " -k *\r\n";
                    }
                    SendSecurePayload(cmdKey);

                    BString limStr = fLimitInput->Text();
                    limStr.Trim();
                    BString cmdLim;
                    if (limStr.Length() > 0) {
                        cmdLim << "MODE " << fChannelName << " +l " << limStr << "\r\n";
                    } else {
                        cmdLim << "MODE " << fChannelName << " -l\r\n";
                    }
                    SendSecurePayload(cmdLim);
                }
                Quit();
                break;
            }
            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

private:
    void SendSecurePayload(const BString& payload) {
        if (fContextServer == nullptr) return;
        SSL* activeSslHandle = gServerSslHandles[fContextServer];
        if (activeSslHandle != nullptr) {
            SSL_write(activeSslHandle, payload.String(), payload.Length());
        }
    }

    BWindow*        fParentWindow;
    void*           fContextServer;
    BString         fChannelName;
    bool            fIsOperator;

public: // Keeps UI widgets visible to parser loops safely
    BCheckBox*      fModeratedCheck;
    BCheckBox*      fSecretCheck;
    BCheckBox*      fInviteCheck;
    BCheckBox*      fTopicCheck;
    BCheckBox*      fNoExtCheck;
    BCheckBox*      fNoColorCheck;
    BTextControl*   fKeyInput;
    BTextControl*   fLimitInput;
};




extern std::map<void*, SSL*> gServerSslHandles;

class OpDurationWindow : public BWindow {
public:
    // Updated constructor to explicitly accept the target server context pointer
    OpDurationWindow(BRect frame, const char* title, const BString& nick, BHandler* mainWin, ChannelTreeItem* contextItem, void* contextServer)
        : BWindow(frame, title, B_MODAL_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
          fTargetNick(nick), fMainWindow(mainWin), fContextItem(contextItem), fContextServer(contextServer) {
        
        BWindow* parentWin = dynamic_cast<BWindow*>(mainWin);
        if (parentWin != nullptr) {
            BRect parentFrame = parentWin->Frame();
            float centerX = parentFrame.left + (parentFrame.Width() - frame.Width()) / 2.0f;
            float centerY = parentFrame.top + (parentFrame.Height() - frame.Height()) / 2.0f;
            MoveTo(centerX, centerY);
        }

        BView* panel = new BView(Bounds(), "bgPanel", B_FOLLOW_ALL, B_WILL_DRAW);
        panel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        AddChild(panel);

        BTextView* promptLabel = new BTextView(BRect(15, 10, 385, 35), "prompt", BRect(0, 0, 370, 25), B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
        BString promptText;
        promptText << "Select how long to grant Operator status to '" << fTargetNick << "':";
        promptLabel->SetText(promptText.String());
        promptLabel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        promptLabel->MakeEditable(false);
        promptLabel->MakeSelectable(false);
        panel->AddChild(promptLabel);

        fTimeMenu = new BPopUpMenu("OpDurations");
        fTimeMenu->AddItem(new BMenuItem("Permanently", nullptr));
        fTimeMenu->AddItem(new BMenuItem("5 Minutes", nullptr));
        fTimeMenu->AddItem(new BMenuItem("1 Hour", nullptr));
        fTimeMenu->AddItem(new BMenuItem("1 Day", nullptr));
        fTimeMenu->ItemAt(0)->SetMarked(true);

        BMenuField* durationField = new BMenuField(BRect(15, 45, 385, 70), "opDurationDropdown", "Duration: ", fTimeMenu);
        durationField->SetDivider(panel->StringWidth("Duration: ") + 5);
        panel->AddChild(durationField);

        BButton* cancelBtn = new BButton(BRect(200, 100, 285, 125), "cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        BButton* opBtn = new BButton(BRect(295, 100, 385, 125), "op", "Grant Op", new BMessage('opsv'));
        
        cancelBtn->SetTarget(this);
        opBtn->SetTarget(this);
        
        panel->AddChild(cancelBtn);
        panel->AddChild(opBtn);

        opBtn->MakeFocus(true);
    }

    void MessageReceived(BMessage* message) override {
        if (message->what == 'opsv') {
            BString selectedDuration = "Permanently";
            BMenuItem* markedItem = fTimeMenu->FindMarked();
            if (markedItem != nullptr) {
                selectedDuration = markedItem->Label();
            }

            BMessage finalPayload(MSG_CONTEXT_OP_SUBMIT);
            finalPayload.AddString("target_nick", fTargetNick);
            finalPayload.AddString("op_duration", selectedDuration);
            
            if (fContextItem != nullptr) {
                finalPayload.AddPointer("channel_item", fContextItem);
            }
            
            // NEW: Attach the unique server pointer tracking key to the output payload
            if (fContextServer != nullptr) {
                finalPayload.AddPointer("server_context", fContextServer);
            }
            
            fMainWindow->Looper()->PostMessage(&finalPayload, fMainWindow);
            
            BMessenger(this).SendMessage(B_QUIT_REQUESTED);

        } else {
            BWindow::MessageReceived(message);
        }
    }

private:
    BString           fTargetNick;
    BHandler*         fMainWindow;
    BPopUpMenu*       fTimeMenu;
    ChannelTreeItem*  fContextItem;
    void*             fContextServer; // Added tracking pointer field
};



class InputFieldKeyFilter : public BMessageFilter {
public:
    InputFieldKeyFilter(BTextControl* inputControl, BObjectList<BString, true>* list, int32* index, BListView* userList) 
        : BMessageFilter(B_KEY_DOWN), 
          fInputControl(inputControl), 
          fHistoryList(list), 
          fHistoryIndex(index),
          fUserList(userList),
          fInCompletionMode(false),
          fLastMatchIndex(-1),
          fCompletionStartPos(0) {}

    virtual filter_result Filter(BMessage* message, BHandler** target) {
        int32 key = 0;
        if (message->FindInt32("key", &key) != B_OK || fInputControl == nullptr || fHistoryList == nullptr || fHistoryIndex == nullptr) {
            return B_DISPATCH_MESSAGE;
        }

        int32 totalItems = fHistoryList->CountItems();

        // --- TAB KEY DETECTED (Haiku Raw Key: 0x26) ---
        if (key == 0x26) {
            HandleTabCompletion();
            return B_SKIP_MESSAGE; // Swallow key to prevent focus shifting
        }

        // Reset tab completion state on any key press that isn't Tab
        fInCompletionMode = false;

        // --- UP ARROW KEY DETECTED (Haiku Raw Key: 0x57) ---
        if (key == 0x57) { 
            if (totalItems > 0 && *fHistoryIndex > 0) {
                (*fHistoryIndex)--; // Move backwards in time
                
                BString* historicalText = fHistoryList->ItemAt(*fHistoryIndex);
                if (historicalText != nullptr) {
                    UpdateInputBox(historicalText->String());
                }
                return B_SKIP_MESSAGE; // Swallow key event
            }
        }
        // --- DOWN ARROW KEY DETECTED (Haiku Raw Key: 0x62) ---
        else if (key == 0x62) {
            if (totalItems > 0 && *fHistoryIndex < totalItems) {
                (*fHistoryIndex)++; // Move forwards in time
                
                if (*fHistoryIndex == totalItems) {
                    // We scrolled all the way back to the present blank line
                    UpdateInputBox("");
                } else {
                    BString* historicalText = fHistoryList->ItemAt(*fHistoryIndex);
                    if (historicalText != nullptr) {
                        UpdateInputBox(historicalText->String());
                    }
                }
                return B_SKIP_MESSAGE; // Swallow key event
            }
        }
        return B_DISPATCH_MESSAGE;
    }

private:
    void UpdateInputBox(const char* text) {
        fInputControl->SetText(text);
        BTextView* tv = fInputControl->TextView();
        if (tv != nullptr) {
            int32 textLen = tv->TextLength();
            tv->Select(textLen, textLen); // Force pen cursor to the end
        }
    }

    void HandleTabCompletion() {
        if (fUserList == nullptr) return;

        BTextView* textView = fInputControl->TextView();
        if (textView == nullptr) return;

        int32 start, end;
        textView->GetSelection(&start, &end);

        // If not currently cycling matches, initialize completion state
        if (!fInCompletionMode) {
            BString currentText(textView->Text());
            
            // Extract the partial word by scanning backward from the cursor to the nearest space
            int32 i = start - 1;
            while (i >= 0 && currentText.ByteAt(i) != ' ') {
                i--;
            }
            fCompletionStartPos = i + 1;
            
            currentText.CopyInto(fOriginalPrefix, fCompletionStartPos, start - fCompletionStartPos);
            
            if (fOriginalPrefix.Length() == 0) return;

            fInCompletionMode = true;
            fLastMatchIndex = -1; // Reset to start before the list
        }

        int32 totalUsers = fUserList->CountItems();
        if (totalUsers == 0) return;

        BString nextMatch = "";
        
        // Loop through BListView items to find the next match matching the prefix
        for (int32 i = 0; i < totalUsers; ++i) {
            int32 checkIndex = (fLastMatchIndex + 1 + i) % totalUsers;
            
            BStringItem* item = static_cast<BStringItem*>(fUserList->ItemAt(checkIndex));
            if (item == nullptr || item->Text() == nullptr) continue;

            BString nick(item->Text());
            
            // Strip common IRC mode prefixes (@, +, %) if they are visible in UI list
            if (nick.Length() > 0 && (nick.ByteAt(0) == '@' || nick.ByteAt(0) == '+' || nick.ByteAt(0) == '%')) {
                nick.Remove(0, 1);
            }

            // Perform a case-insensitive prefix match using Haiku's BString API
            if (nick.ICompare(fOriginalPrefix, fOriginalPrefix.Length()) == 0) {
                nextMatch = nick;
                fLastMatchIndex = checkIndex; // Cache index for the next Tab stroke
                break;
            }
        }

        if (nextMatch.Length() > 0) {
            // Apply proper context spacing (colon suffix at the start of a sentence)
            if (fCompletionStartPos == 0) {
                nextMatch << ": ";
            } else {
                nextMatch << " ";
            }

            // Replace the old text chunk with our newly completed name match
            textView->GetSelection(&start, &end);
            textView->Select(fCompletionStartPos, start);
            textView->Delete();
            textView->Insert(nextMatch.String());
        }
    }

    BTextControl*               fInputControl;
    BObjectList<BString, true>* fHistoryList;
    int32*                      fHistoryIndex;
    BListView*                  fUserList;

    // Tab completion state tracking
    bool                        fInCompletionMode;
    BString                     fOriginalPrefix;
    int32                       fLastMatchIndex;
    int32                       fCompletionStartPos;
};



class ChannelKeyPromptWindow : public BWindow {
public:
    // Updated constructor signature accepting a void* context server token instead of a socket reference
    ChannelKeyPromptWindow(BWindow* parent, void* contextServer, const BString& channelName)
        : BWindow(BRect(0, 0, 360, 130), "Channel Key Required", B_MODAL_WINDOW_LOOK, 
                  B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE) {
        
        fContextServer = contextServer;
        fChannelName = channelName;

        AddToSubset(parent);

        // Center smoothly relative to the main app window layout bounds
        BRect parentFrame = parent->Frame();
        MoveTo(parentFrame.left + (parentFrame.Width() - 360) / 2,
               parentFrame.top + (parentFrame.Height() - 130) / 2);

        BView* panel = new BView(Bounds(), "bgPanel", B_FOLLOW_ALL, B_WILL_DRAW);
        panel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        AddChild(panel);

        // Informative Label
        BString promptText;
        promptText << "'" << fChannelName << "' requires an access key to join:";
        BTextView* promptLabel = new BTextView(BRect(15, 12, 345, 35), "prompt", BRect(0, 0, 330, 20), B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
        promptLabel->SetText(promptText.String());
        promptLabel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        promptLabel->MakeEditable(false);
        promptLabel->MakeSelectable(false);
        panel->AddChild(promptLabel);

        // Key Input Field (Masks typing for password privacy)
        fKeyInput = new BTextControl(BRect(15, 45, 345, 75), "key_field", "Key / Password:", "", nullptr);
        fKeyInput->SetDivider(panel->StringWidth("Key / Password: ") + 5);
        BTextView* tv = fKeyInput->TextView();
        if (tv != nullptr) {
            tv->HideTyping(true); // Masks input as bullets/asterisks
        }
        panel->AddChild(fKeyInput);

        // Buttons
        BButton* cancelBtn = new BButton(BRect(155, 90, 245, 115), "cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        BButton* joinBtn = new BButton(BRect(255, 90, 345, 115), "join", "Join", new BMessage('jkey'));
        
        cancelBtn->SetTarget(this);
        joinBtn->SetTarget(this);
        joinBtn->MakeDefault(true);

        panel->AddChild(cancelBtn);
        panel->AddChild(joinBtn);
        
        fKeyInput->MakeFocus(true);
    }

    void MessageReceived(BMessage* message) override {
        if (message->what == 'jkey') {
            BString key = fKeyInput->Text();
            key.Trim();

            if (fContextServer != nullptr && key.Length() > 0) {
                // 1. Lookup live encrypted connection handle via global map context tracking
                SSL* activeSslHandle = gServerSslHandles[fContextServer];

                // 2. Verification step
                if (activeSslHandle != nullptr) {
                    // 3. Construct and transmit the passworded JOIN payload over OpenSSL
                    BString joinCmd;
                    joinCmd << "JOIN " << fChannelName << " " << key << "\r\n";
                    SSL_write(activeSslHandle, joinCmd.String(), joinCmd.Length());
                }
            }
            Quit();
        } else {
            BWindow::MessageReceived(message);
        }
    }

private:
    void*          fContextServer; // Replaced BSecureSocket* with safe server tracking token
    BString        fChannelName;
    BTextControl*  fKeyInput;
};



class ChannelInviteOnlyWindow : public BWindow {
public:
    // FIXED: Correctly pass parameters into the base BWindow constructor call
    ChannelInviteOnlyWindow(BWindow* parent, void* contextServer, const BString& channelName)
        : BWindow(BRect(0, 0, 380, 180), "Channel Entry Bounced", B_MODAL_WINDOW_LOOK, 
                  B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE) {
        
        fContextServer = contextServer;
        fChannelName = channelName;

        AddToSubset(parent);

        BRect parentFrame = parent->Frame();
        MoveTo(parentFrame.left + (parentFrame.Width() - 380) / 2,
               parentFrame.top + (parentFrame.Height() - 180) / 2);

        BView* panel = new BView(Bounds(), "bgPanel", B_FOLLOW_ALL, B_WILL_DRAW);
        panel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        AddChild(panel);

        BTextView* promptLabel = new BTextView(BRect(15, 15, 365, 110), "prompt", BRect(0, 0, 340, 105), B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);
        BString alertText;
        alertText << "Cannot join '" << fChannelName << "' because it is marked Invite-Only (+i).\n\n"
                  << "Would you like to send a KNOCK request to the channel operators?";
        promptLabel->SetText(alertText.String());
        promptLabel->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        promptLabel->MakeEditable(false);
        promptLabel->MakeSelectable(false);
        panel->AddChild(promptLabel);

        BButton* cancelBtn = new BButton(BRect(175, 135, 265, 160), "cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        BButton* knockBtn = new BButton(BRect(275, 135, 365, 160), "knock", "Knock", new BMessage('knok'));
        
        cancelBtn->SetTarget(this);
        knockBtn->SetTarget(this);
        knockBtn->MakeDefault(true);

        panel->AddChild(cancelBtn);
        panel->AddChild(knockBtn);
    }

    void MessageReceived(BMessage* message) override {
        if (message->what == 'knok') {
            // FIXED: Look up map handle using your updated fContextServer token key variable name
            if (fContextServer != nullptr) {
                SSL* activeSslHandle = gServerSslHandles[fContextServer];

                if (activeSslHandle != nullptr) {
                    BString knockCmd;
                    knockCmd << "KNOCK " << fChannelName << " :Requesting entry via Cricket client\r\n";
                    SSL_write(activeSslHandle, knockCmd.String(), knockCmd.Length());
                }
            }
            Quit();
        } else {
            BWindow::MessageReceived(message);
        }
    }

private:
    void*   fContextServer; 
    BString fChannelName;
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
	    fHistoryIndex = 0;
		

        
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
            
            // =========================================================================
            // STARTUP SYNC: Push the config setting into runtime tree node!
            // =========================================================================
            serverNode->fEnableColorCodes = cfg.servers[i].enableColorCodes;
            // =========================================================================
            
            fChannelTree->AddItem(serverNode);
            
            if (i == 0) fLiberaNode = serverNode;
            if (i == 1) fOftcNode = serverNode;
        }

        // Loop B: Populate user-defined custom servers dynamically
        for (size_t i = 0; i < cfg.customServers.size(); i++) {
            ServerTreeItem* customNode = new ServerTreeItem(cfg.customServers[i].name.c_str(), i, false, true);
            customNode->SetHideStatus(cfg.customServers[i].hideStatusMessages);
            
            // =========================================================================
            // FIXED STARTUP SYNC: Push the custom config setting into the runtime tree node!
            // =========================================================================
            customNode->fEnableColorCodes = cfg.customServers[i].enableColorCodes;
            // =========================================================================
            
            fChannelTree->AddItem(customNode);
        }

        
        // 3. Create Topic View bar Control
        // REMOVED: MakeEditable(false) to allow typing inside the field
        fTopicView = new BTextControl("topic_view", "Topic:", "No topic set.", new BMessage(MSG_TOPIC_CHANGED));
        
        // Ensure that this text field sends its messaging packet back into main window context
        fTopicView->SetTarget(this);
        
        // 4. Setup Middle & Right Components
        fChatLog = new BTextView("chat_log");
        fChatLog->MakeEditable(false);
        fChatLog->SetStylable(true);

        BScrollView* chatScroll = new BScrollView("scroll_chat", fChatLog, 0, false, true);

        // --- NEW: Custom Draw Engine View Canvas Instantiation ---
        //  Added B_NAVIGABLE so this layout surface can accept keyboard events!
        fCustomChatLog = new CustomChatView(BRect(), "custom_draw_view", B_FOLLOW_ALL, B_WILL_DRAW | B_NAVIGABLE);
        
        // Note: Wrapping a direct BView inside a scrollview requires explicit scroll implementation.
        // For simple presentation management, we swap whole container view hierarchies.
        BScrollView* customScroll = new BScrollView("scroll_custom", fCustomChatLog, 0, false, true);

        
        

        // --- NEW: Layout-Safe Group View Container Setup ---
        fChatContainer = new BGroupView(B_VERTICAL, 0);
        
        fUserList = new BListView("user_list");
        
        // 1. Change the Selection Message so it doesn't trick the app into thinking a context click happened on every left-click
        fUserList->SetSelectionMessage(new BMessage('usr_')); // Or use a generic selection message
        fUserList->SetInvocationMessage(nullptr); 

        // 2. UNCOMMENT THIS LINE: This is the actual shield preventing focus shifting and accidental text sending!
        fUserList->AddFilter(new RightClickSingleClickFilter(this));      

       
        BScrollView* userScroll = new BScrollView("scroll_users", fUserList, 0, false, true);
        
        
        
        fInputControl = new BTextControl("input", "", "", new BMessage(MSG_SEND_MESSAGE));

        BTextView* inputTextView = fInputControl->TextView();
        if (inputTextView != nullptr) {
            // Pass the pointers to the collection list and index counters
            inputTextView->AddFilter(new InputFieldKeyFilter(fInputControl, &fHistoryList, &fHistoryIndex, fUserList));
        }






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
                .Add(fIconToggleButton, 0.0) //  Places the layout-safe toggle button here
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



    int32 GetServerIndexFromNode(BStringItem* node);

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

        // Flush command history string allocations out of the heap database safely on application close
        fHistoryList.MakeEmpty(); 

        // 5. Force the application process to instantly terminate and leave the Deskbar
        be_app->PostMessage(B_QUIT_REQUESTED); 
    }




//  Updated method signature to accept 3 arguments, resolving the compiler mismatch
void DisplayBanListDialog(ServerTreeItem* serverContext, BString channelName, BObjectList<BString, true>* harvestList)
{
    if (serverContext == nullptr) return;

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
    
    // ==================== REPOPULATE WITH MULTI-SERVER SECURE DATA ====================
    if (harvestList != nullptr) {
        for (int32 i = 0; i < harvestList->CountItems(); i++) {
            BString* storedMask = harvestList->ItemAt(i);
            if (storedMask != nullptr) {
                banListView->AddItem(new BStringItem(storedMask->String()));
            }
        }
        delete harvestList; 
    }
    // ===================================================================================

    //  Wrap the list view inside a scroll view container with a vertical scroll bar
    BScrollView* scrollView = new BScrollView("banListScroll", banListView, 
                                              B_FOLLOW_LEFT | B_FOLLOW_TOP, 
                                              0,      // Flags (0 is default)
                                              false,  // Horizontal scroll bar (false = no)
                                              true);  // Vertical scroll bar (true = yes)
    
    // Add the scroll view to the panel instead of adding the list view directly
    panel->AddChild(scrollView);


    BButton* closeBtn = new BButton(BRect(245, 260, 330, 285), "close", "Close", new BMessage(B_QUIT_REQUESTED));
    BButton* unbanBtn = new BButton(BRect(340, 260, 435, 285), "unban", "Remove Ban", new BMessage(MSG_CONTEXT_UNBAN_SUBMIT));
    
    closeBtn->SetTarget(banWin);
    unbanBtn->SetTarget(this);

    // Bind the unban target parameters securely to the context server pointer
    BMessage* unbanPayload = new BMessage(MSG_CONTEXT_UNBAN_SUBMIT);
    unbanPayload->AddString("target_channel", channelName);
    unbanPayload->AddPointer("window_ref", banWin);
    unbanPayload->AddPointer("server_context", serverContext); // Ensures unban cases can find the matching socket!
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
        
             // 1. Determine your active Operator capability status for this specific channel tab
        bool iamOperator = false;
        if (fChannelUsers.count(chanItem) > 0) {
            BObjectList<UserListItem, true>* userList = fChannelUsers[chanItem];
            if (userList != nullptr) {
                for (int32 i = 0; i < userList->CountItems(); i++) {
                    UserListItem* user = userList->ItemAt(i);
                    if (user != nullptr && user->Text() != nullptr) {
                        BString userText(user->Text());
                        BString cleanUserNick = GetCleanNickname(userText);
                        
                        // Check if this row is you, and if it starts with the operator tag
                        if (fMyNick.ICompare(cleanUserNick) == 0) {
                            if (userText.StartsWith("@")) {
                                iamOperator = true;
                            }
                            break;
                        }
                    }
                }
            }
        }
        
        // HIDE COMPLETELY ENFORCEMENT: Only generate and append these items if you are an Operator
        if (iamOperator) {
            

            // 2. Channel Modes Controller Window Trigger
            BMessage* modesWindowMsg = new BMessage(MSG_CONTEXT_SHOW_MODES);
            modesWindowMsg->AddPointer("chan_item", chanItem);
            BMenuItem* modesMenuItem = new BMenuItem("Channel Modes...", modesWindowMsg);
            menu->AddItem(modesMenuItem);
            
            // 3. View Channel Ban List Option
            BMessage* banListMsg = new BMessage(MSG_CONTEXT_SHOW_BANS);
            banListMsg->AddPointer("chan_item", chanItem);
            BMenuItem* banListMenuItem = new BMenuItem("View Channel Ban List...", banListMsg);
            menu->AddItem(banListMenuItem);
            
            menu->AddSeparatorItem();
        }

        // Available to everyone
        BMessage* removeMsg = new BMessage('rmch'); 
        removeMsg->AddPointer("channel_item", chanItem);
        menu->AddItem(new BMenuItem("Remove Channel", removeMsg));
        
        menu->SetTargetForItems(this);
        menu->Go(screenPoint, true, true, true);
        return;
    }

    // 2. Server menu logic...


    ServerTreeItem* srvItem = dynamic_cast<ServerTreeItem*>(item);
    if (srvItem != nullptr) {
        BPopUpMenu* menu = new BPopUpMenu("ServerOptions", false, false);
        
        //  Ensure activeSrv is mapped exactly to the tree item currently being right-clicked,
        // rather than blindly trusting the global 'selectedConfig' index state!
        ServerConfig* activeSrv = nullptr;
        std::string targetServerName = srvItem->Text();
        
        for (auto& srv : cfg.servers) {
            if (srv.name == targetServerName) {
                activeSrv = &srv;
                break;
            }
        }
        if (activeSrv == nullptr) {
            for (auto& srv : cfg.customServers) {
                if (srv.name == targetServerName) {
                    activeSrv = &srv;
                    break;
                }
            }
        }

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
        
        //  Pull directly from activeSrv configuration structural states
        if (activeSrv != nullptr && activeSrv->autoConnect) {
            connectMenuItem->SetMarked(true);
        }
        menu->AddItem(connectMenuItem);

        // Toggle Auto-Reconnect Option
        BString toggleReconnectLabel = "Auto-Reconnect on Disconnect";
        BMessage* toggleReconnectMsg = new BMessage(MSG_TOGGLE_AUTORECONNECT);
        toggleReconnectMsg->AddPointer("server_item", srvItem);
        BMenuItem* reconnectMenuItem = new BMenuItem(toggleReconnectLabel.String(), toggleReconnectMsg);
        
        //  Pull directly from activeSrv configuration structural states
        if (activeSrv != nullptr && activeSrv->autoReconnect) {
            reconnectMenuItem->SetMarked(true);
        }
        menu->AddItem(reconnectMenuItem);
        
        // Status Messages Suppression Option
        BString toggleStatusLabel = "Hide Status Messages";
        BMessage* toggleStatusMsg = new BMessage(MSG_TOGGLE_HIDE_STATUS);
        toggleStatusMsg->AddPointer("server_item", srvItem);
        BMenuItem* statusMenuItem = new BMenuItem(toggleStatusLabel.String(), toggleStatusMsg);
        
        //  Pull directly from activeSrv configuration structural states (mapping hideStatusMessages)
        if (activeSrv != nullptr && activeSrv->hideStatusMessages) {
            statusMenuItem->SetMarked(true);
        }
        menu->AddItem(statusMenuItem); 

   /*
        // Custom Inline Emoticons Toggle 
        BString toggleEmotesLabel = "Enable Emoticons";
        BMessage* toggleEmotesMsg = new BMessage('tgem');
        toggleEmotesMsg->AddPointer("server_item", srvItem);
        BMenuItem* emotesMenuItem = new BMenuItem(toggleEmotesLabel.String(), toggleEmotesMsg);
        
     
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
        BMessage* addSrvMsg = new BMessage('adcs'); // Reuses original button message identifier
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
    
    // =========================================================================
    //  MANUAL CONNECTION REGISTRATION SESSION STATE RESETTER
    // =========================================================================
    // Clear out trailing parameter parameters from a past session before 
    // launching the worker thread, preventing capabilities lookups from stalling!
    targetNode->fHasFinalizedCap = false;          // Unlocks the CAP engine gate
    targetNode->fSASLSuccess = false;              // Resets SASL flag metrics
    targetNode->fHasIdentifiedThisSession = false; // Unlocks NickServ text backup script
    
    if (targetNode->fSupportedCaps.Length() > 0) {
        targetNode->fSupportedCaps.Truncate(0);    // Flushes old capabilities buffer
    }
    
    if (cfg.debugEnable) {
        printf("[DEBUG_CONN] [%s] Flushed session state flags for fresh connection pass.\n", targetNode->Text());
    }
    // =========================================================================
   
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
    
    // Fire the automatic disk writer hook
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

        //  Expanded safety bounds count to 8 to prevent thread array overflows
        int32 maxRuns = 8;
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
                if (openingBracketIdx > 0 && runCount < maxRuns) {
                    runArray->runs[runCount].offset = openingBracketIdx;
                    runArray->runs[runCount].font = boldChatFont;
                    runArray->runs[runCount].color = textColor;
                    runCount++;
                } else if (openingBracketIdx == 0) {
                    runArray->runs[0].font = boldChatFont;
                }

                // Reset to Regular text right after the closing bracket "> "
                if (closingBracketIdx + 2 < text.Length() && runCount < maxRuns) {
                    runArray->runs[runCount].offset = closingBracketIdx + 2;
                    runArray->runs[runCount].font = regularChatFont;
                    runArray->runs[runCount].color = textColor;
                    runCount++;
                }
            }

            // URL COLORING (http...)
            if (urlStartIdx != B_ERROR && runCount < maxRuns) {
                runArray->runs[runCount].offset = urlStartIdx;
                runArray->runs[runCount].font = urlLinkFont;
                runArray->runs[runCount].color = hyperlinkColor;
                runCount++;

                // Reset back to Regular text after the URL
                int32 urlEndIdx = text.FindFirst(" ", urlStartIdx);
                if (urlEndIdx == B_ERROR) urlEndIdx = text.FindFirst("\n", urlStartIdx);
                if (urlEndIdx == B_ERROR) urlEndIdx = text.Length();
                
                if (urlEndIdx < text.Length() && runCount < maxRuns) {
                    runArray->runs[runCount].offset = urlEndIdx;
                    runArray->runs[runCount].font = regularChatFont;
                    runArray->runs[runCount].color = textColor;
                    runCount++;
                }
            }

            // =========================================================================
            // FIXED MONOTONIC RUN-ARRAY SORTING ENGINE
            // =========================================================================
            for (int32 i = 0; i < runCount - 1; i++) {
                for (int32 j = 0; j < runCount - i - 1; j++) {
                    if (runArray->runs[j].offset > runArray->runs[j + 1].offset) {
                        text_run temp = runArray->runs[j];
                        runArray->runs[j] = runArray->runs[j + 1];
                        runArray->runs[j + 1] = temp;
                    }
                }
            }

            // De-duplicate any identical offsets generated by formatting intersections
            int32 uniqueCount = 0;
            for (int32 i = 0; i < runCount; i++) {
                if (uniqueCount == 0 || runArray->runs[i].offset > runArray->runs[uniqueCount - 1].offset) {
                    if (i != uniqueCount) {
                        runArray->runs[uniqueCount] = runArray->runs[i];
                    }
                    uniqueCount++;
                }
            }
            runArray->count = uniqueCount;
            // =========================================================================

            // --- Direct View Data Routing (No View Swapping Overhead inside Data Loops!) ---
            bool drawEngineActive = activeSrv ? activeSrv->useCustomDrawFunction : cfg.useCustomDrawFunction;
            if (drawEngineActive) { 
                fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                fCustomChatLog->AddStyledLine(itemNode, text, runArray); 
                
                // Repaint immediately in real time
                fCustomChatLog->InvalidateLayout();
                fCustomChatLog->Invalidate();
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
                fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                fCustomChatLog->AddStyledLine(itemNode, text, nullptr);
                fCustomChatLog->InvalidateLayout();
                fCustomChatLog->Invalidate();
            } else {
                int32 textLength = fChatLog->TextLength();
                fChatLog->Select(textLength, textLength);
                fChatLog->Insert(text.String());
            }
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






static std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}


void ParseAndDisplayIRC(BString line, ServerTreeItem* contextServer) {   	

    // 1. Absolute Null Check
    if (contextServer == nullptr) return;

    // 2. Multi-Server Pointer Validation Check
    // Scan the tree layout container to confirm this pointer address still actually exists
    bool isPointerValid = false;
    int32 totalTreeItems = fChannelTree->CountItems();
    
    for (int32 i = 0; i < totalTreeItems; ++i) {
        if (fChannelTree->ItemAt(i) == static_cast<BListItem*>(contextServer)) {
            isPointerValid = true;
            break;
        }
    }

    // If the pointer is dangling or orphaned from an old connection state, drop it safely
    if (!isPointerValid) {
        return; 
    }    	
    
    line.ReplaceAll("\r", "");
    line.ReplaceAll("\n", ""); // Safe to clean network markers; we append gracefully later

    if (line.Length() == 0 || line.StartsWith("PING")) return;

    // =========================================================================
    //  IRCv3 TAG STRIPPER (Handles server-time prefixes seamlessly)
    // =========================================================================
    BString msgTags = "";
    if (line.StartsWith("@")) {
        int32 firstSpace = line.FindFirst(" ");
        if (firstSpace != B_ERROR) {
            // Isolate the tags block into 'msgTags' if you want to parse timestamps later
            line.CopyInto(msgTags, 0, firstSpace);
            
            // Cleanly slice the tags off the front of the active processing line
            line.Remove(0, firstSpace + 1);
            line.Trim();
        }
    }
    // =========================================================================

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

       
       
       
       
         // Live MODE & Channel Feature Syncer (RPL_CHANNELMODEIS / MODE Handler)
        if (command == "324" || command == "MODE") {
            if (contextServer == nullptr) {
                return;
            }

            // DIAGNOSTIC MONITOR PASS: Log the raw incoming line before tokenizing
            if (cfg.debugEnable) {
                printf("[DEBUG_MODE_PARSER] Raw Command Recieved: %s | Line: '%s' | Trailing: '%s'\n", 
                       command.String(), line.String(), trailing.String());
            }

            BString dialogTargetChannel = ""; 
            BString activeFlags = "";
            BString tokenTrail = "";
            BString channelKey = "";
            BString channelLimit = "";

            if (command == "324") {
                // RPL_CHANNELMODEIS Layout: :server 324 yournick #channel +flags [args]
                BString payload = line;
                payload.Trim();
                
                int32 firstSpace = payload.FindFirst(" ");
                if (firstSpace != B_ERROR) {
                    payload.Remove(0, firstSpace + 1); // Skip past yournick
                    
                    int32 secondSpace = payload.FindFirst(" ");
                    if (secondSpace != B_ERROR) {
                        payload.CopyInto(dialogTargetChannel, 0, secondSpace);
                        
                        BString remaining = "";
                        payload.CopyInto(remaining, secondSpace + 1, payload.Length() - (secondSpace + 1));
                        remaining.Trim();

                        int32 thirdSpace = remaining.FindFirst(" ");
                        if (thirdSpace == B_ERROR) {
                            activeFlags = remaining;
                        } else {
                            remaining.CopyInto(activeFlags, 0, thirdSpace);
                            remaining.CopyInto(tokenTrail, thirdSpace + 1, remaining.Length() - (thirdSpace + 1));
                            tokenTrail.Trim();
                        }
                    }
                }
            } 
            else if (command == "MODE") {
                // Layout: '#ablyss-test +m' packed entirely inside the line string
                BString payload = line;
                payload.Trim();

                int32 firstSpace = payload.FindFirst(" ");
                if (firstSpace == B_ERROR) {
                    dialogTargetChannel = payload;
                    activeFlags = trailing;
                } else {
                    payload.CopyInto(dialogTargetChannel, 0, firstSpace);
                    
                    BString remainingFlags = "";
                    payload.CopyInto(remainingFlags, firstSpace + 1, payload.Length() - (firstSpace + 1));
                    remainingFlags.Trim();

                    // FIX: If trailing has parameters (like a nickname for +o), merge them correctly
                    if (trailing.Length() > 0) {
                        activeFlags = remainingFlags;
                        tokenTrail = trailing;
                    } else {
                        // If trailing is empty, split the flag from any trailing text-arguments
                        int32 flagsSpace = remainingFlags.FindFirst(" ");
                        if (flagsSpace == B_ERROR) {
                            activeFlags = remainingFlags; // This safely captures "+m"!
                        } else {
                            remainingFlags.CopyInto(activeFlags, 0, flagsSpace);
                            remainingFlags.CopyInto(tokenTrail, flagsSpace + 1, remainingFlags.Length() - (flagsSpace + 1));
                        }
                    }
                }
                activeFlags.Trim();
                tokenTrail.Trim();
            }


            // Clean any brackets, spaces, or tags off the extracted channel string
            dialogTargetChannel.Trim();
            int32 tagIndex = dialogTargetChannel.FindFirst(" [");
            if (tagIndex != B_ERROR) {
                dialogTargetChannel.Truncate(tagIndex);
            }

            // DIAGNOSTIC MONITOR PASS: Verify the isolated tokens are correct
            if (cfg.debugEnable) {
                printf("[DEBUG_MODE_PARSER] Tokenization Result -> Channel: '%s', Flags: '%s', Args: '%s'\n",
                       dialogTargetChannel.String(), activeFlags.String(), tokenTrail.String());
            }

            // =========================================================================
            // DATA-DRIVEN TOKEN LAYER ENGINE: Extract key/limit arguments dynamically
            // =========================================================================
            if (activeFlags.Length() > 0 && tokenTrail.Length() > 0) {
                bool addingMode = true;
                int32 currentTokenIdx = 0;

                for (int32 i = 0; i < activeFlags.Length(); i++) {
                    char modeChar = activeFlags[i];
                    if (modeChar == '+') { addingMode = true; continue; }
                    if (modeChar == '-') { addingMode = false; continue; }

                    bool expectsParam = (contextServer->fTypeAModes.FindFirst(modeChar) != B_ERROR) ||
                                        (contextServer->fTypeBModes.FindFirst(modeChar) != B_ERROR) ||
                                        (addingMode && contextServer->fTypeCModes.FindFirst(modeChar) != B_ERROR);

                    if (expectsParam) {
                        BString paramWord = "";
                        int32 nextSpace = tokenTrail.FindFirst(" ", currentTokenIdx);
                        
                        if (nextSpace == B_ERROR) {
                            tokenTrail.CopyInto(paramWord, currentTokenIdx, tokenTrail.Length() - currentTokenIdx);
                            currentTokenIdx = tokenTrail.Length();
                        } else {
                            tokenTrail.CopyInto(paramWord, currentTokenIdx, nextSpace - currentTokenIdx);
                            currentTokenIdx = nextSpace + 1;
                        }
                        paramWord.Trim();

                        if (paramWord.Length() > 0) {
                            if (modeChar == 'k') {
                                channelKey = paramWord;
                            } else if (modeChar == 'l') {
                                channelLimit = paramWord;
                            }
                        }
                    }
                }
            }

            // DIAGNOSTIC MONITOR PASS: Check if the UI sync routing targets match
            if (cfg.debugEnable) {
                printf("[DEBUG_MODE_PARSER] UI Sync Verification -> fActiveModesDialog Active: %s | fActiveModesChannel: '%s' | Target: '%s'\n",
                       (fActiveModesDialog != nullptr) ? "YES" : "NO", fActiveModesChannel.String(), dialogTargetChannel.String());
            }
            // Sync the states up to the checkbox view container smoothly
            if (fActiveModesDialog != nullptr && fActiveModesChannel == dialogTargetChannel) {
                
                // Secure thread isolation lock
                if (fActiveModesDialog->Lock()) {
                    
                    bool hasM = (fActiveModesDialog->fModeratedCheck->Value() == B_CONTROL_ON);
                    bool hasS = (fActiveModesDialog->fSecretCheck->Value() == B_CONTROL_ON);
                    bool hasI = (fActiveModesDialog->fInviteCheck->Value() == B_CONTROL_ON);
                    bool hasT = (fActiveModesDialog->fTopicCheck->Value() == B_CONTROL_ON);
                    bool hasN = (fActiveModesDialog->fNoExtCheck->Value() == B_CONTROL_ON);
                    bool hasC = (fActiveModesDialog->fNoColorCheck->Value() == B_CONTROL_ON);

                    // FIX: Deep-copy strings immediately into safe local thread storage!
                    BString safeKeyStr = fActiveModesDialog->fKeyInput->Text();
                    BString safeLimitStr = fActiveModesDialog->fLimitInput->Text();

                    if (command == "324") {
                        hasM = (activeFlags.FindFirst("m") != B_ERROR);
                        hasS = (activeFlags.FindFirst("s") != B_ERROR);
                        hasI = (activeFlags.FindFirst("i") != B_ERROR);
                        hasT = (activeFlags.FindFirst("t") != B_ERROR);
                        hasN = (activeFlags.FindFirst("n") != B_ERROR);
                        hasC = (activeFlags.FindFirst("c") != B_ERROR);
                        
                        safeKeyStr = (channelKey.Length() > 0) ? channelKey : "";
                        safeLimitStr = (channelLimit.Length() > 0) ? channelLimit : "";
                    } 
                    else {
                        bool adding = true;
                        for (int32 k = 0; k < activeFlags.Length(); k++) {
                            char c = activeFlags[k];
                            if (c == '+') { adding = true; continue; }
                            if (c == '-') { adding = false; continue; }

                            if (c == 'm') hasM = adding;
                            else if (c == 's') hasS = adding;
                            else if (c == 'i') hasI = adding;
                            else if (c == 't') hasT = adding;
                            else if (c == 'n') hasN = adding;
                            else if (c == 'c') hasC = adding;
                            else if (c == 'k') safeKeyStr = adding ? channelKey : "";
                            else if (c == 'l') safeLimitStr = adding ? channelLimit : "";
                        }
                    }

                    if (cfg.debugEnable) {
                        printf("[DEBUG_MODE_PARSER] Updating UI window states with resolved cumulative mapping values.\n");
                    }

                    // Update states cleanly using our local deep-copied string references
                    fActiveModesDialog->UpdateCheckedStates(
                        hasM, hasS, hasI, hasT, hasN, hasC, 
                        safeKeyStr.String(), safeLimitStr.String()
                    );

                    fActiveModesDialog->Unlock();
                }
            }
        }






	// @JOIN
     // Catch Successful Join actions (Handles Us AND Other Users)
    if (command == "JOIN") {
        if (contextServer == nullptr) {
            return;
        }

        BString channelJoined = trailing.Length() > 0 ? trailing : line;
        
        channelJoined.Trim();
        channelJoined.ReplaceAll(" ", "");
        
        BString userWhoJoined = prefix;
        int32 exclamIdx = userWhoJoined.FindFirst("!");
        if (exclamIdx != B_ERROR) userWhoJoined.Truncate(exclamIdx);
        
        std::string targetServerName = contextServer->Text();
        bool foundServerInConfig = false;
        size_t resolvedIndex = 0;
        bool resolvedIsCustom = false;
        bool hideStatusOnThisServer = false;

        for (size_t i = 0; i < cfg.servers.size(); i++) {
            if (cfg.servers[i].name == targetServerName) {
                resolvedIndex = i;
                resolvedIsCustom = false;
                hideStatusOnThisServer = cfg.servers[i].hideStatusMessages;
                foundServerInConfig = true;
                break;
            }
        }
        if (!foundServerInConfig) {
            for (size_t i = 0; i < cfg.customServers.size(); i++) {
                if (cfg.customServers[i].name == targetServerName) {
                    resolvedIndex = i;
                    resolvedIsCustom = true;
                    hideStatusOnThisServer = cfg.customServers[i].hideStatusMessages;
                    foundServerInConfig = true;
                    break;
                }
            }
        }

        ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelJoined);
        if (!chanNode) {
            ChannelTreeItem* newChanNode = new ChannelTreeItem(channelJoined.String(), 
                                                               resolvedIndex, 
                                                               resolvedIsCustom);

            fChannelTree->AddUnder(newChanNode, contextServer); 
            fChannelTree->Expand(contextServer);
            
            LogToItemBuffer(newChanNode, BString("--- Joined channel ") << channelJoined << "\n");
            fChannelUsers[newChanNode] = new BObjectList<UserListItem, true>(20);
            
            // =========================================================================
            //  AUTOMATED INCOMING JOIN VIEW FOCUS LOCK
            // =========================================================================
            // Automatically snaps view focus to this room if it matches typed target!
            bool shouldGrabFocus = (contextServer == fCurrentServerNode);
            
            if (!fPendingFocusChannel.IsEmpty() && fPendingFocusServer == contextServer) {
                if (channelJoined.ICompare(fPendingFocusChannel) == 0) {
                    shouldGrabFocus = true;
                    fPendingFocusChannel.Truncate(0); // Clear the pending cache
                    fPendingFocusServer = nullptr;
                }
            }

            if (shouldGrabFocus) {
                fActiveBufferItem = newChanNode;
                
                int32 itemIdx = fChannelTree->IndexOf(static_cast<BListItem*>(newChanNode));
                if (itemIdx != B_ERROR) {
                    fChannelTree->Select(itemIdx); // Highlights the row visually in sidebar list drawer
                }
                
                if (fCustomChatLog != nullptr) {
                    fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                }
                
                // CLEANED: Avoid double-deletions on fUserList items since they are direct tracking pointers
                while (fUserList->CountItems() > 0) {
                    fUserList->RemoveItem((int32)0);
                }
                
                fTopicView->SetText(BString("Joined ") << channelJoined << ". Awaiting topic payload...");
                
                RefreshUserListUI(); 
                if (fUserList != nullptr) {
                    fUserList->InvalidateLayout();
                    fUserList->Invalidate();
                }
            }
            // =========================================================================
            
            SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
            int activeFd = gServerRawSockets[static_cast<void*>(contextServer)];
            
            if (activeSslHandle != nullptr && activeFd >= 0) {
                BString syncWho; syncWho << "WHO " << channelJoined << "\r\n";
                SSL_write(activeSslHandle, syncWho.String(), syncWho.Length());
            } else {
                BSecureSocket* activeSocket = GetActiveSocket(contextServer);
                if (activeSocket != nullptr) {
                    BString syncWho; syncWho << "WHO " << channelJoined << "\r\n";
                    activeSocket->Write(syncWho.String(), syncWho.Length());
                }
            }

            
        } else {
            // Someone else joined a channel we are already in!
            if (fChannelUsers[chanNode] != nullptr) {
                bool userExists = false;
                for (int32 i = 0; i < fChannelUsers[chanNode]->CountItems(); i++) {
                    BString itemTxt = fChannelUsers[chanNode]->ItemAt(i)->Text();
                    if (itemTxt.StartsWith("@") || itemTxt.StartsWith("+")) itemTxt.Remove(0, 1);
                    if (itemTxt == userWhoJoined) { userExists = true; break; }
                }
                
                if (!userExists) {
                    fChannelUsers[chanNode]->AddItem(new UserListItem(userWhoJoined.String(), false));
                    
                    if (!hideStatusOnThisServer) {
                        LogToItemBuffer(chanNode, BString("--> ") << userWhoJoined << " has joined " << channelJoined << "\n");
                    }
                    
                    if (fActiveBufferItem == chanNode) {
                        RefreshUserListUI();
                        if (fUserList != nullptr) {
                            fUserList->Invalidate();
                        }
                    }

                    // ==================== LIVE AUTO-OP INTERCEPTOR ====================
                    bool shouldAutoOp = false;
                    for (int32 i = 0; i < fAutoOpList.CountItems(); i++) {
                        BString* storedNick = fAutoOpList.ItemAt(i);
                        if (storedNick != nullptr && *storedNick == userWhoJoined) {
                            shouldAutoOp = true;
                            break;
                        }
                    }

                    if (shouldAutoOp) {
                        SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                        int activeFd = gServerRawSockets[static_cast<void*>(contextServer)];
                        
                        if (activeSslHandle != nullptr && activeFd >= 0) {
                            BString autoOpPayload; autoOpPayload << "MODE " << channelJoined << " +o " << userWhoJoined << "\r\n";
                            SSL_write(activeSslHandle, autoOpPayload.String(), autoOpPayload.Length());
                        } else {
                            BSecureSocket* activeSocket = GetActiveSocket(contextServer);
                            if (activeSocket != nullptr) {
                                BString autoOpPayload; autoOpPayload << "MODE " << channelJoined << " +o " << userWhoJoined << "\r\n";
                                activeSocket->Write(autoOpPayload.String(), autoOpPayload.Length());
                            }
                        }
                    }
                    // ==================================================================
                }
            }
        }
        return;
    }

    
        // =========================================================================
        // HANDLER FOR NUMERIC 042: RPL_YOURID (Unique Connection Hash)
        // =========================================================================
        if (command == "042") {
            if (contextServer == nullptr) return;

            // Incoming Format: :server 042 YourNick 1ABCDE234 :unique ID
            // 'line' contains: "YourNick 1ABCDE234"
            // 'trailing' contains: "unique ID"

            BString idPayload = line;
            idPayload.Trim();

            BString uniqueID = "";
            int32 spaceIdx = idPayload.FindFirst(" ");
            if (spaceIdx != B_ERROR) {
                // Isolate the unique ID hash token following nickname
                idPayload.CopyInto(uniqueID, spaceIdx + 1, idPayload.Length() - (spaceIdx + 1));
                uniqueID.Trim();
            } else {
                uniqueID = "UNKNOWN"; // Fallback safety
            }

            // Reconstruct a clean, informative notification string
            BString idNotice;
            idNotice.SetToFormat("--> Session Assigned: Unique ID '%s' (%s)\n", uniqueID.String(), trailing.String());

            // Route it directly to server's active logging destination
            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, idNotice);
            } else {
                LogToItemBuffer(fActiveBufferItem, idNotice);
            }
            return;
        }

    
    
         // =========================================================================
        // HANDLER FOR NUMERIC 005: RPL_ISUPPORT (Server Features List)
        // =========================================================================
        if (command == "005") {
            if (contextServer == nullptr) return;

            // Incoming Format: :server 005 YourNick NICKLEN=30 CHANMODES=I,k,l :are supported by this server
            // 'line' contains: "YourNick NICKLEN=30 CHANMODES=I,k,l ..."
            // 'trailing' contains: "are supported by this server"

            BString featuresPayload = line;
            featuresPayload.Trim();

            BString featuresOnly = "";
            int32 spaceIdx = featuresPayload.FindFirst(" ");
            if (spaceIdx != B_ERROR) {
                // Isolate the feature tokens following nickname string
                featuresPayload.CopyInto(featuresOnly, spaceIdx + 1, featuresPayload.Length() - (spaceIdx + 1));
                featuresOnly.Trim();
            }

            // Fallback safety if the line parsing yielded an empty feature block
            if (featuresOnly.Length() == 0) {
                featuresOnly = "Default capabilities";
            }

            // --- CORRECTED HAIKU BSTRING::SPLIT LOOP ---
            // 1. First include <StringList.h> at the top of cricket.cpp if you haven't yet.
            BStringList tokens;
            
            // Haiku splits directly into a string list container, dropping empty elements
            if (featuresOnly.Split(" ", true, tokens)) {
                int32 tokenCount = tokens.CountStrings();
                
                for (int32 i = 0; i < tokenCount; ++i) {
                    BString singleToken = tokens.StringAt(i);
                    
                    int32 eqIdx = singleToken.FindFirst("=");
                    if (eqIdx != B_ERROR) {
                        BString key, value;
                        singleToken.CopyInto(key, 0, eqIdx);
                        singleToken.CopyInto(value, eqIdx + 1, singleToken.Length() - (eqIdx + 1));
                        
                        // Example: Cache maximum nick length supported by Libera or OFTC
                        if (key == "NICKLEN") {
                            // contextServer->fMaxNickLength = atoi(value.String());
                        }
                    }
                }
            }

            // Reconstruct a perfectly formatted status line showcasing what was actually received
            BString supportNotice;
            supportNotice.SetToFormat("--> Server Features: %s (%s)\n", featuresOnly.String(), trailing.String());

            // Route the completed features notice straight into server's status log tab
            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, supportNotice);
            } else {
                LogToItemBuffer(fActiveBufferItem, supportNotice);
            }
            return;
        }


    
    
            // =========================================================================
        // HANDLER FOR NUMERIC 396: RPL_HOSTHIDDEN (Cloaked Hostname Update)
        // =========================================================================
        if (command == "396") {
            if (contextServer == nullptr) return;

            // Incoming Format: :server 396 YourNick user/cloakedhost :is now hidden host
            // 'line' contains: "YourNick user/cloakedhost"
            // 'trailing' contains: "is now hidden host (set by services.)"

            BString hostPayload = line;
            hostPayload.Trim();

            BString hiddenHost = "";
            int32 spaceIdx = hostPayload.FindFirst(" ");
            if (spaceIdx != B_ERROR) {
                // Isolate the cloaked hostname parameter following nickname
                hostPayload.CopyInto(hiddenHost, spaceIdx + 1, hostPayload.Length() - (spaceIdx + 1));
                hiddenHost.Trim();
            } else {
                hiddenHost = "user/cloaked"; // Structural fallback
            }

            // Reconstruct a perfectly formatted, clear, unclipped status notification line
            BString cloakedNotice;
            cloakedNotice.SetToFormat("--> Network Security: '%s' %s\n", hiddenHost.String(), trailing.String());

            // Route the completed string straight into server's status log tab
            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, cloakedNotice);
            } else {
                LogToItemBuffer(fActiveBufferItem, cloakedNotice);
            }
            return;
        }

    
    
    
  
        // =========================================================================
        // HANDLER FOR INCOMING KNOCK ALERTS (Numeric 710 For Channel Operators)
        // =========================================================================
        if (command == "710") {
            BString targetChannel = "";
            BString knockerNick = "";
            
            // Raw line contains: "YourNick #channel knocker!user@host"
            BString lineData(line);
            lineData.Trim();
            
            // 1. Strip own nickname (the first token)
            int32 firstSpace = lineData.FindFirst(" ");
            if (firstSpace != B_ERROR) {
                BString remainder;
                lineData.CopyInto(remainder, firstSpace + 1, lineData.Length() - (firstSpace + 1));
                remainder.Trim();
                
                // 2. Extract the channel (the second token)
                int32 secondSpace = remainder.FindFirst(" ");
                if (secondSpace != B_ERROR) {
                    remainder.CopyInto(targetChannel, 0, secondSpace);
                    
                    // 3. Extract the knocker identity (the third token)
                    BString knockerIdentity;
                    remainder.CopyInto(knockerIdentity, secondSpace + 1, remainder.Length() - (secondSpace + 1));
                    knockerIdentity.Trim();
                    
                    // Isolate nickname from hostmask
                    int32 nickEnd = knockerIdentity.FindFirst("!");
                    if (nickEnd != B_ERROR) {
                        knockerIdentity.CopyInto(knockerNick, 0, nickEnd);
                    } else {
                        knockerNick = knockerIdentity;
                    }
                } else {
                    targetChannel = remainder; // Fallback if line cuts short
                }
            }

            targetChannel.Trim();
            knockerNick.Trim();

            // Format a clean, standard IRC notification string
            BString styledNotice;
            styledNotice << "--> [KNOCK] " << knockerNick << " " << trailing << "\n";

            // Locate the channel node in Haiku UI tree
            ChannelTreeItem* targetedChanNode = nullptr;
            for (int32 i = 0; i < fChannelTree->CountItems(); i++) {
                ChannelTreeItem* item = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemAt(i));
                if (item != nullptr && BString(item->Text()).ICompare(targetChannel) == 0) {
                    if (fChannelTree->Superitem(item) == contextServer) {
                        targetedChanNode = item;
                        break;
                    }
                }
            }

            // Route to the specific channel tab if found, otherwise use fallback
            if (targetedChanNode != nullptr) {
                LogToItemBuffer(targetedChanNode, styledNotice);
            } else {
                LogToItemBuffer(fActiveBufferItem, styledNotice);
            }

            // =========================================================================
            // ASYNCHRONOUS BALERT POPUP DISPATCH
            // =========================================================================
            if (knockerNick.Length() > 0 && targetChannel.Length() > 0) {
                BString alertText;
                alertText << "User " << knockerNick << " is knocking on " << targetChannel 
                          << ".\nDo you want to invite them into the channel?";
                
                BAlert* knockAlert = new BAlert("Incoming Knock", alertText.String(), "Ignore", "Invite User", 
                    nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);

                // Shortcut: Hitting 'Enter' fires off the Invite button automatically
                knockAlert->SetShortcut(1, B_ENTER);

                // Package target names inside the asynchronous reply tracking payload
                BMessage* replyMessage = new BMessage('hknc'); // Handle KNock Choice
                replyMessage->AddString("knocker", knockerNick);
                replyMessage->AddString("channel", targetChannel);
                
                // Pass a pointer to the active server node context tracking maps key cleanly
                replyMessage->AddPointer("server_context", contextServer);
                
                // Go(BInvoker) fires the alert independently, fully eliminating deadlocks
                knockAlert->Go(new BInvoker(replyMessage, this));
            }
        }




    
             // =========================================================================
        // HANDLER FOR INCOMING INVITATIONS (Auto-Join Trigger)
        // =========================================================================
        if (command == "INVITE") {
            if (contextServer == nullptr) return;

            // Incoming Format: :OperatorNick!user@host INVITE YourNick :#channel
            BString invitedTarget = line;
            invitedTarget.Trim();
            
            BString targetChannel = trailing;
            targetChannel.Trim();

            if (targetChannel.StartsWith(":")) {
                targetChannel.Remove(0, 1);
                targetChannel.Trim();
            }

            // =========================================================================
            // FIXED NICKNAME RESOLUTION LAYER
            // =========================================================================
            // Parse the sidebar text carefully. If it's "Libera Chat [ablyss_]", extract "ablyss_"
            BString activeMyNick = "";
            BString serverText = contextServer->Text();
            
            int32 openBracket = serverText.FindFirst("[");
            int32 closeBracket = serverText.FindFirst("]");
            
            if (openBracket != B_ERROR && closeBracket != B_ERROR && closeBracket > openBracket) {
                serverText.CopyInto(activeMyNick, openBracket + 1, closeBracket - (openBracket + 1));
            } else {
                // If there are no brackets or it's just "Libera", look for spaces
                int32 spacePos = serverText.FindFirst(" ");
                if (spacePos != B_ERROR) {
                    serverText.Truncate(spacePos);
                }
                activeMyNick = serverText;
            }
            activeMyNick.Trim();

            // CRITICAL FALLBACK: If the string parsing yielded a network name instead of a nick,
            // fall back to fMyNick or our known configuration settings
            if (activeMyNick.ICompare("Libera") == 0 || activeMyNick.ICompare("OFTC") == 0 || activeMyNick.Length() == 0) {
                activeMyNick = fMyNick;
            }

            if (cfg.debugEnable) {
                printf("[DEBUG_INVITE] Resolved True Run-time Nick -> Extracted: '%s' | Invited: '%s' | Target: '%s'\n",
                       activeMyNick.String(), invitedTarget.String(), targetChannel.String());
            }

            // Perform the case-insensitive comparison safely
            if (activeMyNick.Length() > 0 && activeMyNick.ICompare(invitedTarget) == 0 && targetChannel.Length() > 0) {
                
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];

                if (activeSslHandle != nullptr) {
                    BString autoJoinNotice;
                    autoJoinNotice.SetToFormat("--> [INVITE] Auto-joining channel '%s'...\n", targetChannel.String());
                    LogToItemBuffer(fActiveBufferItem, autoJoinNotice);

                    BString joinCommand;
                    joinCommand << "JOIN " << targetChannel << "\r\n";
                    
                    SSL_write(activeSslHandle, joinCommand.String(), joinCommand.Length());
                    
                    if (cfg.debugEnable) {
                        printf("[DEBUG OPENSSL] Automated response execution: JOIN written to secure stream for %s\n", targetChannel.String());
                    }
                }
            }
            
        }



    
    
    
        // =========================================================================
        // HANDLER FOR NUMERIC 473: ERR_INVITEONLYCHAN
        // =========================================================================
        if (command == "473") {
            // Numeric 473 structure looks like: :server 473 yournick #channel :Cannot join channel (+i)
            // 'line' holds the parameters following the code: "yournick #channel"
            BString payload = line;
            payload.Trim();

            int32 spaceIdx = payload.FindFirst(" ");
            if (spaceIdx != B_ERROR) {
                BString bouncedChannel = "";
                payload.CopyInto(bouncedChannel, spaceIdx + 1, payload.Length() - (spaceIdx + 1));
                bouncedChannel.Trim();

                // Strip out trailing server comments if present
                int32 commentIdx = bouncedChannel.FindFirst(" ");
                if (commentIdx != B_ERROR) {
                    bouncedChannel.Truncate(commentIdx);
                }

                // =========================================================================
                // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                // =========================================================================
                // Verify that this server pointer currently possesses a live, active secure handle
                bool serverStillConnected = false;
                Lock();
                if (gServerSslHandles.find(static_cast<void*>(contextServer)) != gServerSslHandles.end()) {
                    serverStillConnected = true;
                }
                Unlock();

                if (serverStillConnected && bouncedChannel.Length() > 0) {
                    // Log notice to the current active chat buffer buffer
                    BString notice;
                    notice.SetToFormat("(!) Network Alert: '%s' is Invite-Only. Prompting for option...\n", bouncedChannel.String());
                    LogToItemBuffer(fActiveBufferItem, notice);

                    // Launch the custom modal notification framework asynchronously
                    // NEW SIGNATURE: Hand over the contextServer pointer token instead of raw activeSocket reference
                    ChannelInviteOnlyWindow* inviteWin = new ChannelInviteOnlyWindow(this, contextServer, bouncedChannel);
                    inviteWin->Show();
                }
            }
           
        }

    
    
    
        // =========================================================================
        // =========================================================================
        // HANDLER FOR NUMERIC 475: ERR_BADCHANNELKEY
        // =========================================================================
        if (command == "475") {
            // Numeric 475 structure looks like: :server 475 yournick #channel :Cannot join channel (+k)
            // 'line' holds the parameters following the code: "yournick #channel"
            BString payload = line;
            payload.Trim();

            int32 spaceIdx = payload.FindFirst(" ");
            if (spaceIdx != B_ERROR) {
                BString badChannel = "";
                payload.CopyInto(badChannel, spaceIdx + 1, payload.Length() - (spaceIdx + 1));
                badChannel.Trim();

                // Strip out trailing server comment fragments if present
                int32 commentIdx = badChannel.FindFirst(" ");
                if (commentIdx != B_ERROR) {
                    badChannel.Truncate(commentIdx);
                }

                // =========================================================================
                // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                // =========================================================================
                // Verify that this server pointer currently possesses a live, active secure handle
                bool serverStillConnected = false;
                Lock();
                if (gServerSslHandles.find(static_cast<void*>(contextServer)) != gServerSslHandles.end()) {
                    serverStillConnected = true;
                }
                Unlock();

                if (serverStillConnected && badChannel.Length() > 0) {
                    // Inform the local room log buffer cleanly
                    BString notice;
                    notice.SetToFormat("(!) Network Alert: '%s' requires an access key. Prompting for password...\n", badChannel.String());
                    LogToItemBuffer(fActiveBufferItem, notice);

                    // Launch the modal password collection dialog framework asynchronously
                    // FIXED: Hand over contextServer and pass the correct 'badChannel' local variable string
                    ChannelKeyPromptWindow* promptWin = new ChannelKeyPromptWindow(this, contextServer, badChannel);
                    promptWin->Show();
                }
            }
           
        }


    
    

        // =========================================================================
        // UNIFIED COMPREHENSIVE NUMERIC 005: RPL_ISUPPORT CORE ENGINE
        // =========================================================================
        if (command == "005") { // RPL_ISUPPORT Capabilities Broadcast
            if (contextServer == nullptr) return;

            BString featuresPayload = line;
            featuresPayload.Trim();

            BString featuresOnly = "";
            int32 spaceIdx = featuresPayload.FindFirst(" ");
            if (spaceIdx != B_ERROR) {
                featuresPayload.CopyInto(featuresOnly, spaceIdx + 1, featuresPayload.Length() - (spaceIdx + 1));
                featuresOnly.Trim();
            }

            if (featuresOnly.Length() == 0) {
                featuresOnly = "Default capabilities";
            }

            // 1. EXTRACT AND PARSE DYNAMIC CHANMODES FOR THE USER LIST ENGINE
            int32 modePos = featuresOnly.FindFirst("CHANMODES=");
            if (modePos != B_ERROR) {
                BString modeString;
                featuresOnly.CopyInto(modeString, modePos + 10, featuresOnly.Length() - (modePos + 10));
                
                int32 endSpace = modeString.FindFirst(" ");
                if (endSpace != B_ERROR) {
                    modeString.Truncate(endSpace);
                }
                
                int32 firstComma  = modeString.FindFirst(",");
                int32 secondComma = modeString.FindFirst(",", firstComma + 1);
                int32 thirdComma  = modeString.FindFirst(",", secondComma + 1);
                
                if (firstComma != B_ERROR && secondComma != B_ERROR && thirdComma != B_ERROR) {
                    modeString.CopyInto(contextServer->fTypeAModes, 0, firstComma);
                    modeString.CopyInto(contextServer->fTypeBModes, firstComma + 1, secondComma - (firstComma + 1));
                    modeString.CopyInto(contextServer->fTypeCModes, secondComma + 1, thirdComma - (secondComma + 1));
                    modeString.CopyInto(contextServer->fTypeDModes, thirdComma + 1, modeString.Length() - (thirdComma + 1));
                    
                    if (cfg.debugEnable) {
                        printf("[DEBUG] Dynamic CHANMODES parsed for %s -> A:%s | B:%s | C:%s | D:%s\n",
                            contextServer->Text(), contextServer->fTypeAModes.String(), 
                            contextServer->fTypeBModes.String(), contextServer->fTypeCModes.String(), 
                            contextServer->fTypeDModes.String());
                    }
                }
            }

            // 2. HAIKU BSTRINGLIST TOKENIZER FOR MISC VALUES (NICKLEN, ETC)
            BStringList tokens;
            if (featuresOnly.Split(" ", true, tokens)) {
                int32 tokenCount = tokens.CountStrings();
                for (int32 i = 0; i < tokenCount; ++i) {
                    BString singleToken = tokens.StringAt(i);
                    int32 eqIdx = singleToken.FindFirst("=");
                    if (eqIdx != B_ERROR) {
                        BString key, value;
                        singleToken.CopyInto(key, 0, eqIdx);
                        singleToken.CopyInto(value, eqIdx + 1, singleToken.Length() - (eqIdx + 1));
                        
                        if (key == "NICKLEN") {
                            // contextServer->fMaxNickLength = atoi(value.String());
                        }
                    }
                }
            }

            // 3. RECONSTRUCT AND ROUTE FORMATTED NOTICE TO THE SERVER LOGS WINDOWS
            BString supportNotice;
            supportNotice.SetToFormat("--> Server Features: %s (%s)\n", featuresOnly.String(), trailing.String());

            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, supportNotice);
            } else {
                LogToItemBuffer(fActiveBufferItem, supportNotice);
            }
            return;
        }





        // ERR_ERRONEUSNICKNAME (432): Triggered if an automated nickname assignment is blocked or illegal
        if (command == "432") {
            if (contextServer == nullptr) return;

            BSecureSocket* activeSocket = GetActiveSocket(contextServer);
            if (activeSocket != nullptr) {
                // Generate a randomized guest name configuration as an emergency fallback
                uint32 randomSeed = static_cast<uint32>(real_time_clock_usecs() & 0xFFFF);
                BString emergencyNick;
                emergencyNick.SetToFormat("Guest%u", (randomSeed % 900) + 100);

                fNickAttempts[contextServer]++; // Advance retry metrics to maintain safety layout structures
                fMyNick = emergencyNick;

                BString nickCmd;
                nickCmd << "NICK " << emergencyNick << "\r\n";
                activeSocket->Write(nickCmd.String(), nickCmd.Length());

                BStringItem* serverLogNode = FindServerLogNode(contextServer);
                BString itemNotice = "--- Illegal or blocked nickname encountered! Falling back onto emergency label: ";
                itemNotice << emergencyNick << "\n";
                LogToItemBuffer(serverLogNode ? serverLogNode : static_cast<BStringItem*>(contextServer), itemNotice);
            }
            return;
        }




          // ==================== HARVEST BAN DATA (367 RPL_BANLIST) ====================
        if (command == "367") {
            // FIX 1: Drop packet immediately if background context server tracking node is dead
            if (contextServer == nullptr) {
                return;
            }

            BString fullParams = line;
            fullParams.Trim();

            // Expected format inside line parameter: "YourNick #channel mask kicker timestamp"
            int32 firstSpace = fullParams.FindFirst(" ");  // End of YourNick
            if (firstSpace != B_ERROR) {
                // FIX 2: Replace unsafe pointer arithmetic string indexing with safe CopyInto bounds
                BString afterNick;
                fullParams.CopyInto(afterNick, firstSpace + 1, fullParams.Length() - (firstSpace + 1));
                afterNick.Trim();

                int32 secondSpace = afterNick.FindFirst(" "); // End of #channel
                if (secondSpace != B_ERROR) {
                    BString maskPart;
                    afterNick.CopyInto(maskPart, secondSpace + 1, afterNick.Length() - (secondSpace + 1));
                    maskPart.Trim();

                    // Isolate just the mask word token before kicker and timestamp spaces
                    int32 thirdSpace = maskPart.FindFirst(" ");
                    if (thirdSpace != B_ERROR) {
                        maskPart.Truncate(thirdSpace);
                    }
                    maskPart.Trim();

                    maskPart.ReplaceAll("\r", "");
                    maskPart.ReplaceAll("\n", "");

                    if (maskPart.Length() > 0) {
                        // FIX 3: PER-SERVER ISOLATED HARVEST CACHING
                        // Instead of a global array, route collected items to a map tracking per contextServer.
                        if (fServerBanHarvests.find(contextServer) == fServerBanHarvests.end()) {
                            fServerBanHarvests[contextServer] = new BObjectList<BString, true>(50);
                        }
                        fServerBanHarvests[contextServer]->AddItem(new BString(maskPart));
                    }
                }
            }
            return;
        }




        // ==================== GENERATE DIALOG (368 RPL_ENDOFBANLIST) ====================
        if (command == "368") {
            if (contextServer == nullptr) return;

            BString targetChannel = line;
            targetChannel.Trim();
            
            int32 firstSpace = targetChannel.FindFirst(" ");
            if (firstSpace != B_ERROR) {
                BString afterNick;
                targetChannel.CopyInto(afterNick, firstSpace + 1, targetChannel.Length() - (firstSpace + 1));
                afterNick.Trim();

                int32 secondSpace = afterNick.FindFirst(" ");
                if (secondSpace != B_ERROR) {
                    afterNick.Truncate(secondSpace);
                }
                targetChannel = afterNick;
            }
            targetChannel.Trim();

            BObjectList<BString, true>* harvestList = nullptr;
            if (fServerBanHarvests.find(contextServer) != fServerBanHarvests.end()) {
                harvestList = fServerBanHarvests[contextServer];
                fServerBanHarvests.erase(contextServer);
            }

            // Fire the global display window 
            DisplayBanListDialog(contextServer, targetChannel, harvestList);

            //  Removed 'delete harvestList;' from here entirely! 
            // The heap pointer is now safely deleted inside the dialog initialization sequence below.
            return;
        }


 
        // Live AWAY Handler: Catches other users changing their status in real-time (Requires CAP REQ)
        if (command == "AWAY") {
            if (contextServer == nullptr) {
                return;
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
                    
                    BListItem* superItem = fChannelTree->Superitem(chanNode);
                    
                    // =========================================================================
                    // FIXED PRIVATE QUERY ROOT VERIFICATION (REPLACES BROKEN SUPERITEM LOGIC)
                    // =========================================================================
                    if (superItem == nullptr) {
                        bool belongsToThisServer = false;
                        if (chanNode->IsCustom() == contextServer->IsCustom()) {
                            belongsToThisServer = true;
                        }

                        if (belongsToThisServer) {
                            if (BString(chanNode->Text()).ICompare(userWhoChanged) == 0) {
                                if (fActiveBufferItem == chanNode) uiRefreshNeeded = true;
                            }
                        }
                        continue; 
                    }
                    // =========================================================================

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
                //  Force an immediate graphical view repaint pass 
                // to render the color shift or away status icon instantly on screen!
                RefreshUserListUI();
                if (fUserList != nullptr) {
                    fUserList->InvalidateLayout();
                    fUserList->Invalidate();
                }
            }
            return;
        }




   
   
    

        // RPL_UNAWAY (305): Server confirms you are no longer marked away
        if (command == "305") {
            // FIX 1: Strict concurrency safety guard. Drop packet if server tracking node is dead.
            if (contextServer == nullptr) {
                return;
            }
            
            BStringItem* serverLog = FindServerLogNode(contextServer);
            BString notice = "--- Server Notification: You are no longer marked as being away.\n";
            
            // FIX 2: Fallback to the context server node directly rather than the active visual tab
            LogToItemBuffer(serverLog ? serverLog : static_cast<BStringItem*>(contextServer), notice);
            
            // DRY helper updates state across all channels on this specific server instantly
            UpdateMyGlobalAwayState(contextServer, false); // false = active/bright
            return;
        }

        // RPL_NOWAWAY (306): Server confirms you are now successfully marked away
        if (command == "306") {
            // FIX 1: Strict concurrency safety guard. Drop packet if server tracking node is dead.
            if (contextServer == nullptr) {
                return;
            }
    
            BStringItem* serverLog = FindServerLogNode(contextServer);
            BString notice;
            notice << "--- Server Notification: You are now marked as away (" << cfg.awayMessage.c_str() << ").\n";
            
            // FIX 2: Fallback to the context server node directly rather than the active visual tab
            LogToItemBuffer(serverLog ? serverLog : static_cast<BStringItem*>(contextServer), notice);
    
            // DRY helper updates state across all channels on this specific server instantly
            UpdateMyGlobalAwayState(contextServer, true); // true = away/dimmed
            return;
        }



        

        // =========================================================================
        // FIXED NUMERIC 433: ERR_NICKNAMEINUSE RESOLVER (STALL PREVENTION GATE)
        // =========================================================================
        if (command == "433") {
            if (contextServer == nullptr) return;

            // Compute and resolve alternate nickname variables out of config
            size_t srvIdx = contextServer->GetIndex();
            bool isCustom = contextServer->IsCustom();
            
            // Track collision attempts to cycle from AltNick1 to AltNick2 safely
            if (fNickAttempts.count(contextServer) == 0) fNickAttempts[contextServer] = 0;
            fNickAttempts[contextServer]++;

            std::string newTargetNick = "";
            if (fNickAttempts[contextServer] == 1) {
                newTargetNick = isCustom ? cfg.customServers[srvIdx].altNick : cfg.servers[srvIdx].altNick;
            } else {
                newTargetNick = isCustom ? cfg.customServers[srvIdx].altNick2 : cfg.servers[srvIdx].altNick2;
            }

            if (!newTargetNick.empty()) {
                BString nickPayload;
                nickPayload << "NICK " << newTargetNick.c_str() << "\r\n";
                
                //  Explicitly chain CAP END to release the new registration hold!
                BString capReleasePayload = "CAP END\r\n";

                // Pull the live background encryption reference handle natively from the map
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                int activeFd = gServerRawSockets[static_cast<void*>(contextServer)];

                if (activeSslHandle != nullptr && activeFd >= 0) {
                    // Send NICK update first
                    SSL_write(activeSslHandle, nickPayload.String(), nickPayload.Length());
                    // Send CAP END immediately after to release the network lock!
                    SSL_write(activeSslHandle, capReleasePayload.String(), capReleasePayload.Length());
                    
                    if (cfg.debugEnable) printf("[DEBUG_CONN] [%s] Nickname collision resolved. Sent fallback NICK & CAP END over OpenSSL.\n", 
                           contextServer->Text());
                } else {
                    // Legacy unmanaged socket fallback path
                    BSecureSocket* activeSocket = GetActiveSocket(contextServer);
                    if (activeSocket != nullptr) {
                        activeSocket->Write(nickPayload.String(), nickPayload.Length());
                        activeSocket->Write(capReleasePayload.String(), capReleasePayload.Length());
                    }
                }

                BString statusLog;
                statusLog << "--- Nickname in use! Trying alternate identifier: " << newTargetNick.c_str() << "\n";
                LogToItemBuffer(FindServerLogNode(contextServer), statusLog);
            }
            return;
        }





        // RPL_WHOREPLY: Processes bulk channel user attributes (Bypasses trailing chunk limitations)
        if (command == "352") {
            if (contextServer == nullptr) return;

            std::vector<BString> tokens;
            int32 currentPos = 0, nextSpace;
            while ((nextSpace = line.FindFirst(" ", currentPos)) != B_ERROR) {
                BString t; line.CopyInto(t, currentPos, nextSpace - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
                currentPos = nextSpace + 1;
            }
            if (currentPos < line.Length()) {
                BString t; line.CopyInto(t, currentPos, line.Length() - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
            }

            if (tokens.size() >= 7) {
                BString channelTarget  = tokens[1]; 
                BString targetUserNick = tokens[5]; 
                BString flags          = tokens[6]; 

                channelTarget.Trim(); targetUserNick.Trim(); flags.Trim();

                ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelTarget);
                if (chanNode != nullptr) {
                    bool isAway = (flags.FindFirst("G") != B_ERROR);
                    
                    // --- OPTIMIZATION STEP ---
                    // Make sure implementation of UpdateUserAwayState ONLY edits the 
                    // text string inside the userVector item object on the heap, and 
                    // DOES NOT call fUserList->Invalidate() or RefreshUserListUI() here!
                    UpdateUserAwayState(chanNode, targetUserNick.String(), isAway);
                }
            }
            return;
        }








          // RPL_AWAY: Triggered when trying to ping or query an away target user
        if (command == "301") {
            // FIX 1: Drop packet immediately if background context server tracking node is dead
            if (contextServer == nullptr) {
                return;
            }

            // 1. Declare and build the tokens vector explicitly inside this scope
            // FIX 1b: Parse 'line' (args block) instead of 'trailing' (reason block) to capture the nicknames!
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

            // 2. Safely verify that we found our tokens
            if (tokens.size() >= 2) {
                BString awayUserNick = tokens[1]; // Index 0 is nick, 1 is the target away nick
                awayUserNick.Trim();

                if (awayUserNick.Length() > 0) {
                    // 3. FIX 2: Step through channels belonging ONLY to this server connection context node!
                    int32 totalTreeItems = fChannelTree->CountItems();
                    for (int32 c = 0; c < totalTreeItems; c++) {
                        BListItem* baseItem = fChannelTree->ItemAt(c);
                        if (baseItem == nullptr) continue;

                        // Enforce rigid server root boundaries
                        if (fChannelTree->Superitem(baseItem) != contextServer) continue;

                        ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(baseItem);
                        if (chanNode != nullptr) {
                            UpdateUserAwayState(chanNode, awayUserNick.String(), true);
                        }
                    }
                }
            }
            return;
        }




         // RPL_WELCOME: Network handshake authentication completed successfully
        if (command == "001") {
            // FIX 1: If contextServer is missing, we must abandon routing to avoid corrupting active tabs
            if (contextServer == nullptr) {
                return; 
            }
            
            // =========================================================================
            //  ENFORCE MASTER VIEW FOCUS LOCK ON VALID SERVER CONTAINER NODE
            // =========================================================================
            // Prevents asynchronous thread focus swaps from breaking screen rendering layouts!
            if (fCurrentServerNode == contextServer) {
                fActiveBufferItem = static_cast<BStringItem*>(contextServer);
                if (fCustomChatLog != nullptr) {
                    fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                }
            }
            // =========================================================================
            
            // Reset nickname collision attempt metrics for this server context instance
            fNickAttempts[contextServer] = 0;

            // --- Parse the official nickname confirmed by the remote server ---
            BString officialNick = "";
            int32 firstParamSpace = line.FindFirst(" ");
            if (firstParamSpace != B_ERROR) {
                BString remainingParams = line;
                remainingParams.Remove(0, firstParamSpace + 1);
                int32 nextParamSpace = remainingParams.FindFirst(" ");
                if (nextParamSpace != B_ERROR) {
                    remainingParams.CopyInto(officialNick, 0, nextParamSpace);
                } else {
                    officialNick = remainingParams;
                }
                officialNick.Trim();
            }

            // --- FIX 2: Map the visual ServerTreeItem node down to its backend Config struct safely ---
            std::string targetServerName = contextServer->Text();
            bool foundServerInConfig = false;
            ServerConfig* resolvedProfile = nullptr;

            for (auto& srv : cfg.servers) {
                if (srv.name == targetServerName) {
                    resolvedProfile = &srv;
                    foundServerInConfig = true;
                    break;
                }
            }
            if (!foundServerInConfig) {
                for (auto& srv : cfg.customServers) {
                    if (srv.name == targetServerName) {
                        resolvedProfile = &srv;
                        foundServerInConfig = true;
                        break;
                    }
                }
            }

            // Sync the official confirmed nick straight to the per-server config slot to prevent global leakage
            if (resolvedProfile != nullptr && officialNick.Length() > 0 && officialNick != "*") {
                resolvedProfile->nick = officialNick.String(); 
                if (cfg.debugEnable) {
                    printf("[DEBUG] 001 Welcome confirmation for %s: Finalized nick as '%s'\n", 
                           resolvedProfile->name.c_str(), resolvedProfile->nick.c_str());
                }
            }

            // =========================================================================
            // RESTORED AUTO-JOIN HANDSHAKE DISPATCH ENGINE (OPENSSL STREAM SYNCHRONIZED)
            // =========================================================================
            // Automatically loop and send JOIN commands safely via active OpenSSL Context
            if (foundServerInConfig && resolvedProfile != nullptr) {
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                int activeFd = gServerRawSockets[static_cast<void*>(contextServer)];

                for (const auto& chan : resolvedProfile->autojoin) {
                    if (chan.empty()) continue;
                    
                    BString joinCommand;
                    joinCommand << "JOIN " << chan.c_str() << "\r\n";
                    
                    if (activeSslHandle != nullptr && activeFd >= 0) {
                        SSL_write(activeSslHandle, joinCommand.String(), joinCommand.Length());
                    } else {
                        BSecureSocket* activeSocket = GetActiveSocket(contextServer);
                        if (activeSocket != nullptr) {
                            activeSocket->Write(joinCommand.String(), joinCommand.Length());
                        }
                    }
                    
                    if (cfg.debugEnable) {
                        LogDebugStream(contextServer->Text(), "OUTGOING", joinCommand.String(), joinCommand.Length());
                    }
                }
            }
            // =========================================================================
            
            // Route structural confirmation status messages down into the correct network view log node
            BStringItem* serverLog = FindServerLogNode(contextServer);
            if (serverLog != nullptr) {
                LogToItemBuffer(serverLog, "--- Connection fully established with network.\n");
            } else {
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), "--- Connection fully established with network.\n");
            }
            return;
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

            // FIX 1: If background socket context missing, drop packet to avoid populating wrong window
            if (contextServer == nullptr) {
                return;
            }

            IRCChannelListWindow* targetWindow = fActiveListWindow;

            if (targetWindow != nullptr) {
                // FIX 2: FETCH THE ACTIVE NETWORK PIPELINE STREAM SOCKET SECURELY
                BSecureSocket* activeNetworkSocket = nullptr;
                if (fServerSockets.count(contextServer) > 0) {
                    activeNetworkSocket = fServerSockets[contextServer];
                } else if (contextServer == fOftcNode) {
                    activeNetworkSocket = fOftcSocket;
                } else if (contextServer == fLiberaNode) {
                    activeNetworkSocket = fLiberaSocket;
                }

                bool targetMatches = false;
                
                // In Haiku, if the window is being destroyed, Lock() will return false.
                if (targetWindow->Lock()) {
                    if (targetWindow->GetTargetSocket() == activeNetworkSocket) {
                        targetMatches = true;
                    }
                    targetWindow->Unlock();
                }

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
            // FIX 1: Abort processing if context background socket server is completely missing 
            if (contextServer == nullptr) {
                return;
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
                    int32 flagSpace = remainderParams.FindFirst(" ");
                    
                    // CASE 1: Standard Mode Flag String with a Target Nick/Param (+o Nick, +k Key, etc.)
                    if (flagSpace != B_ERROR) {
                        BString flagString;
                        remainderParams.CopyInto(flagString, 0, flagSpace);
                        
                        BString targetParam;
                        remainderParams.CopyInto(targetParam, flagSpace + 1, remainderParams.Length() - flagSpace - 1);
                        targetParam.ReplaceAll("\r", "");
                        targetParam.ReplaceAll("\n", "");
                        targetParam.Trim();

                        bool adding = flagString.StartsWith("+");
                        
                        // Identify mode character matching standard IRC protocol specs
                        char targetModeChar = '\0';
                        if (flagString.FindFirst("q") != B_ERROR) targetModeChar = 'q';
                        else if (flagString.FindFirst("a") != B_ERROR) targetModeChar = 'a';
                        else if (flagString.FindFirst("o") != B_ERROR) targetModeChar = 'o';
                        else if (flagString.FindFirst("h") != B_ERROR) targetModeChar = 'h';
                        else if (flagString.FindFirst("v") != B_ERROR) targetModeChar = 'v';

                        // If it's a nick status change (op, voice, etc.), update the user entry
                        if (targetModeChar != '\0') {
                            bool itemWasUpdated = false;

                            for (int32 i = 0; i < userVector->CountItems(); i++) {
                                UserListItem* uiUser = userVector->ItemAt(i);
                                if (uiUser == nullptr) continue;

                                BString cleanName = uiUser->GetCleanNick();
                                if (cleanName == targetParam) {
                                    uiUser->SetMode(targetModeChar, adding);
                                    itemWasUpdated = true;
                                    break;
                                }
                            }

                            if (itemWasUpdated && fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                            }
                        }
                        
                        // INTERACTION SYNC: Update Dialog UI inputs if Key (+k) or User Limit (+l) change live
                        if (fActiveModesDialog != nullptr && fActiveModesChannel == targetChannel) {
                            if (flagString.FindFirst("k") != B_ERROR) {
                                fActiveModesDialog->UpdateCheckedStates(
                                    fActiveModesDialog->fModeratedCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fSecretCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fInviteCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fTopicCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fNoExtCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fNoColorCheck->Value() == B_CONTROL_ON,
                                    adding ? targetParam.String() : "", 
                                    fActiveModesDialog->fLimitInput->Text()
                                );
                            }
                            else if (flagString.FindFirst("l") != B_ERROR) {
                                fActiveModesDialog->UpdateCheckedStates(
                                    fActiveModesDialog->fModeratedCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fSecretCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fInviteCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fTopicCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fNoExtCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fNoColorCheck->Value() == B_CONTROL_ON,
                                    fActiveModesDialog->fKeyInput->Text(),
                                    adding ? targetParam.String() : ""
                                );
                            }
                        }
                    }
                    // CASE 2: Raw Channel Toggle Mode String (+m, -s, +t, etc. without explicit targets)
                    else {
                        BString flagString = remainderParams;
                        bool adding = flagString.StartsWith("+");

                        // INTERACTION SYNC: Sync checkboxes dynamically inside your active dialog context
                        if (fActiveModesDialog != nullptr && fActiveModesChannel == targetChannel) {
                            
                            // CRITICAL THREAD SAFETY GUARD: Secure window lock before touching UI components
                            if (fActiveModesDialog->Lock()) {
                                bool m = (fActiveModesDialog->fModeratedCheck->Value() == B_CONTROL_ON);
                                bool s = (fActiveModesDialog->fSecretCheck->Value() == B_CONTROL_ON);
                                bool i = (fActiveModesDialog->fInviteCheck->Value() == B_CONTROL_ON);
                                bool t = (fActiveModesDialog->fTopicCheck->Value() == B_CONTROL_ON);
                                bool n = (fActiveModesDialog->fNoExtCheck->Value() == B_CONTROL_ON);
                                bool c = (fActiveModesDialog->fNoColorCheck->Value() == B_CONTROL_ON);

                                // Deep-copy string memory arrays onto local thread stack safely
                                BString safeKeyStr = fActiveModesDialog->fKeyInput->Text();
                                BString safeLimitStr = fActiveModesDialog->fLimitInput->Text();

                                if (flagString.FindFirst("m") != B_ERROR) m = adding;
                                if (flagString.FindFirst("s") != B_ERROR) s = adding;
                                if (flagString.FindFirst("i") != B_ERROR) i = adding;
                                if (flagString.FindFirst("t") != B_ERROR) t = adding;
                                if (flagString.FindFirst("n") != B_ERROR) n = adding;
                                if (flagString.FindFirst("c") != B_ERROR) c = adding;

                                fActiveModesDialog->UpdateCheckedStates(
                                    m, s, i, t, n, c, 
                                    safeKeyStr.String(), 
                                    safeLimitStr.String()
                                );

                                // ALWAYS unlock looper when finished reading/writing fields
                                fActiveModesDialog->Unlock();
                            }
                        }
                    }
                }

                // ==================== SUB-BRANCH B: KICK PROCESSING ====================
                else if (command == "KICK") {
                    // DIAGNOSTIC MONITOR PASS: Log the raw payload state on arrival
                    if (cfg.debugEnable) {
                        printf("[DEBUG_KICK] KICK Command captured in parser loop!\n");
                        printf("[DEBUG_KICK] Prefix: '%s' | Remainder: '%s' | Trailing: '%s'\n", 
                               prefix.String(), remainderParams.String(), trailing.String());
                    }

                    BString kicker = prefix;
                    int32 exclamIdx = kicker.FindFirst("!");
                    if (exclamIdx != B_ERROR) kicker.Truncate(exclamIdx);

                    BString targetNick = remainderParams;
                    BString kickReason = trailing; 
                    
                    int32 reasonColon = targetNick.FindFirst(":");
                    if (reasonColon != B_ERROR) {
                        // FIX 2b: Safe layout copy extractor to isolate kick reason securely 
                        targetNick.CopyInto(kickReason, reasonColon + 1, targetNick.Length() - reasonColon - 1);
                        targetNick.Truncate(reasonColon);
                    }
                    targetNick.Trim();
                    kickReason.ReplaceAll("\r", "");
                    kickReason.ReplaceAll("\n", "");
                    kickReason.Trim();

                    // DIAGNOSTIC MONITOR PASS: Verify localized text extractions
                    if (cfg.debugEnable) {
                        printf("[DEBUG_KICK] Processed Entities -> Kicker: '%s' | Target Victim: '%s' | Reason: '%s'\n",
                               kicker.String(), targetNick.String(), kickReason.String());
                    }

                    bool victimFoundInLocalList = false;
                    for (int32 i = userVector->CountItems() - 1; i >= 0; i--) {
                        UserListItem* uiUser = userVector->ItemAt(i);
                        if (uiUser == nullptr) continue;

                        if (GetCleanNickname(uiUser->Text()) == targetNick) {
                            victimFoundInLocalList = true;
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
                    
                         if (cfg.debugEnable && !victimFoundInLocalList) {
                        printf("[DEBUG_KICK] Warning: Target nick '%s' was not found inside fChannelUsers userVector map loop.\n", 
                               targetNick.String());
                    }
                    
                }                
                
                break; 
            }
            return;
        }





           // =========================================================================
        // FULLY UN-CUT INSTRUMENTED CAP HANDLER ENGINE
        // =========================================================================
        if (command == "CAP") {
            if (contextServer == nullptr) {
                if (cfg.debugEnable) printf("[DEBUG_CAP] CRITICAL ERR: Intercepted CAP command, but contextServer is NULL!\n");
                return;
            }
            
            if (contextServer->fHasFinalizedCap) {
                if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] CAP line dropped: Handshake already finalized for this session.\n", contextServer->Text());
                return;
            }

            if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Inbound Line Raw: '%s' | Trailing data field: '%s'\n", 
                   contextServer->Text(), line.String(), trailing.String());

            // Fetch live static encryption handle references safely via void pointer conversions
            int activeFd = gServerRawSockets[static_cast<void*>(contextServer)];
            SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
            
            if (activeSslHandle == nullptr || activeFd < 0) {
                if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] CRITICAL FAIL: Active global static encryption handle reference is NULL!\n", contextServer->Text());
                return;
            } else {
                if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Verified tracking structures safely. (Live kernel FD: %d)\n", contextServer->Text(), activeFd);
            }

            // 1. MATCH CAP LS LISTINGS
            if (line.IFindFirst("LS") != B_ERROR) {
                if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Match condition found: LS capability listings menu menu layer.\n", contextServer->Text());
                
                if (contextServer->fSupportedCaps.Length() > 0) contextServer->fSupportedCaps << " ";
                contextServer->fSupportedCaps << trailing;

                bool hasMoreChunks = (line.IFindFirst(" * LS ") != B_ERROR || line.IFindFirst(" * ") != B_ERROR);
                if (line.IFindFirst(" * ") != B_ERROR && trailing.IFindFirst("multi-prefix") != B_ERROR) {
                    hasMoreChunks = false;
                }

                if (!hasMoreChunks) {
                    BString localCapBuffer;
                    localCapBuffer << " " << contextServer->fSupportedCaps << " ";
                    if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Unified Capability Menu Buffer String: '%s'\n", contextServer->Text(), localCapBuffer.String());

                    // Thread-safe dynamic hostname profile context evaluation
                    bool userWantsSASL = false;
                    BString innerHost = contextServer->GetHost();
                    for (const auto& srv : cfg.servers) {
                        if (innerHost.ICompare(srv.host.c_str()) == 0) { userWantsSASL = srv.useSASL; break; }
                    }
                    for (const auto& srv : cfg.customServers) {
                        if (innerHost.ICompare(srv.host.c_str()) == 0) { userWantsSASL = srv.useSASL; break; }
                    }

                    if (userWantsSASL && (localCapBuffer.IFindFirst(" sasl ") != B_ERROR || localCapBuffer.IFindFirst(" sasl=") != B_ERROR)) {
                        BString reqCommand = "CAP REQ :sasl away-notify\r\n";
                        SSL_write(activeSslHandle, reqCommand.String(), reqCommand.Length());
                        if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Dispatched outbound -> CAP REQ :sasl\n", contextServer->Text());
                    } else {
                        BString capEnd = "CAP END\r\n";
                        SSL_write(activeSslHandle, capEnd.String(), capEnd.Length());
                        contextServer->fHasFinalizedCap = true; 
                        if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] SASL skipped. Dispatched outbound fallback -> CAP END\n", contextServer->Text());
                    }
                    contextServer->fSupportedCaps.Truncate(0);
                }
            }
            // 2. MATCH CAP ACKNOWLEDGEMENT 
            else if (line.IFindFirst("ACK") != B_ERROR) {
                if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Match condition found: ACK acknowledgment confirmation token.\n", contextServer->Text());
                
                // Scans both line and trailing buffers to catch sasl on any network format variation
                if (line.IFindFirst("sasl") != B_ERROR || trailing.IFindFirst("sasl") != B_ERROR) {
                    BString authCmd = "AUTHENTICATE PLAIN\r\n";
                    ssize_t written = SSL_write(activeSslHandle, authCmd.String(), authCmd.Length());
                    if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Dispatched outbound -> AUTHENTICATE PLAIN (Written: %ld bytes)\n", contextServer->Text(), written);
                } else {
                    BString capEnd = "CAP END\r\n";
                    SSL_write(activeSslHandle, capEnd.String(), capEnd.Length());
                    contextServer->fHasFinalizedCap = true;
                    if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Non-SASL feature ACK. Dispatched outbound -> CAP END\n", contextServer->Text());
                }
            }
            // 3. MATCH CAP REJECTION 
            else if (line.IFindFirst("NAK") != B_ERROR) {
                if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Match condition found: NAK capability rejection frame.\n", contextServer->Text());
                BString capEnd = "CAP END\r\n";
                SSL_write(activeSslHandle, capEnd.String(), capEnd.Length());
                contextServer->fHasFinalizedCap = true; 
                if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Dispatched outbound error release -> CAP END\n", contextServer->Text());
            }
            return;
        }



        // =========================================================================
        //  SASL COMPLETION OUTCOME RESPONSES (UNIFIED OPENSSL GATE)
        // =========================================================================
        if (command == "903" || command == "904" || command == "905") {
            if (contextServer != nullptr) {
                // Fetch the live, working background encryption handle reference
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                int activeFd = gServerRawSockets[static_cast<void*>(contextServer)];
                
                if (activeSslHandle != nullptr && activeFd >= 0) {
                    if (command == "903") {
                        contextServer->fSASLSuccess = true; // Blocks text identification backup scripts
                        fTextBuffers[contextServer] << ">>> SASL Authentication Successful. Logged in securely.\n";
                    } else {
                        fTextBuffers[contextServer] << ">>> SASL Authentication Failed. Falling back to legacy protocols...\n";
                    }
                    
                    //  Send CAP END natively via SSL_write to release the capability hold!
                    BString capEnd = "CAP END\r\n";
                    SSL_write(activeSslHandle, capEnd.String(), capEnd.Length());
                    contextServer->fHasFinalizedCap = true;
                    
                    if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] SASL finalized. Sent CAP END over live context.\n", contextServer->Text());
                    this->RebuildActiveChannelBuffer();
                } else {
                    if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] ERROR: Outbound transmission handle is unavailable during 903 pass!\n", contextServer->Text());
                }
            }
            return;
        }





        // Live PART & QUIT Handlers (Safely removes users dynamically when they depart or disconnect)
        if (command == "PART" || command == "QUIT") {
            if (contextServer == nullptr) return;

            BString userWhoLeft = prefix;
            int32 exclamIdx = userWhoLeft.FindFirst("!");
            if (exclamIdx != B_ERROR) userWhoLeft.Truncate(exclamIdx);
            userWhoLeft.Trim();

            BString targetChannel = line;
            targetChannel.Trim();
            // Clean out everything after the first space to isolate the channel name
            int32 spacePos = targetChannel.FindFirst(" ");
            if (spacePos != B_ERROR) targetChannel.Truncate(spacePos);

            // Safe Haiku BOutlineListView Sub-Item Traversal
            int32 totalTreeItems = fChannelTree->CountItems();
            for (int32 c = 0; c < totalTreeItems; c++) {
                BListItem* baseItem = fChannelTree->ItemAt(c);
                if (baseItem == nullptr) continue;

                // Explicit cast to BListItem* for comparison with Superitem
                if (fChannelTree->Superitem(baseItem) != static_cast<BListItem*>(contextServer)) continue;

                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(baseItem);
                if (!chanNode) continue;
                
                // =========================================================================
                // FIXED: ROBUST CHANNEL MATCHING (Supports ##trivia and case-insensitivity)
                // =========================================================================
                if (command == "PART") {
                    BString cleanNodeText(chanNode->Text());
                    int32 tagIdx = cleanNodeText.FindFirst(" [");
                    if (tagIdx != B_ERROR) {
                        cleanNodeText.Truncate(tagIdx);
                    }
                    cleanNodeText.Trim();
                    
                    // FIXED: Corrected object method notation and completed comparison check
                    if (targetChannel.ICompare(cleanNodeText) != 0) continue;
                }

                // FIXED: Corrected map validation bounds container check
                if (fChannelUsers.count(chanNode) > 0 && fChannelUsers[chanNode] != nullptr) {
                    BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                    
                    for (int32 i = userVector->CountItems() - 1; i >= 0; i--) {
                        UserListItem* userItem = userVector->ItemAt(i);
                        if (userItem == nullptr) continue;

                        // =========================================================================
                        // ROBUST NICKNAME PREFIX STRIPPER (Supports OFTC ~ and &)
                        // =========================================================================
                        BString itemTxt = userItem->Text();
                        while (itemTxt.Length() > 0 && 
                              (itemTxt.ByteAt(0) == '@' || itemTxt.ByteAt(0) == '+' || 
                               itemTxt.ByteAt(0) == '%' || itemTxt.ByteAt(0) == '~' || 
                               itemTxt.ByteAt(0) == '&')) {
                            itemTxt.Remove(0, 1);
                        }
                        
                        if (itemTxt == userWhoLeft) {
                            // Using RemoveItemAt(i) for indexed loop as per BObjectList API.
                            // Since the list is "Owning" (true), this automatically calls delete for you.
                            userVector->RemoveItemAt(i);
                            
                            // Check "Hide Status Messages" preference
                            bool hideStatusOnThisServer = false;
                            std::string checkServerName = contextServer->Text();
                            for (const auto& srv : cfg.servers) {
                                if (srv.name == checkServerName) { 
                                    hideStatusOnThisServer = srv.hideStatusMessages; 
                                    break; 
                                }
                            }
                            if (!hideStatusOnThisServer) {
                                for (const auto& srv : cfg.customServers) {
                                    if (srv.name == checkServerName) { 
                                        hideStatusOnThisServer = srv.hideStatusMessages; 
                                        break; 
                                    }
                                }
                            }
                            
                            if (!hideStatusOnThisServer) {
                                BString partNotice = (command == "PART") ? "has left" : "has quit";
                                BString logLine;
                                logLine << "<-- " << userWhoLeft << " " << partNotice;
                                if (trailing.Length() > 0) logLine << " (" << trailing << ")\n";
                                else logLine << "\n";
                                
                                LogToItemBuffer(chanNode, logLine);
                            }
                            
                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                            }
                            
                            if (command == "PART") break; 
                        }
                    }
                }
            }
            return;
        }





        // RPL_TOPICWHOTIME (333): Identifies who set the topic and when
        if (command == "333") {
            // FIX 1: Drop packet immediately if background context server tracking node is dead
            if (contextServer == nullptr) {
                return;
            }

            // Expected format: "<YourNick> <#Channel> <WhoSetIt> <UnixTimestamp>"
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

            // Ensure we have enough parameters parsed (Channel at index 0, User at index 1, Time at index 2)
            if (tokens.size() >= 3) {
                BString channelTarget = tokens[0];
                BString topicSetter  = tokens[1];
                BString timeString   = tokens[2];
                
                channelTarget.Trim();
                topicSetter.Trim();
                timeString.Trim();

                // Because contextServer is verified, this channel lookup is securely sandboxed
                ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelTarget);
                if (chanNode != nullptr) {
                    time_t rawTime = static_cast<time_t>(atoll(timeString.String()));
                    struct tm* timeInfo = localtime(&rawTime);
                    
                    BString dateNotice = "--- Topic set by ";
                    dateNotice << topicSetter;
                    
                    if (timeInfo != nullptr) {
                        char timeBuffer[64];
                        strftime(timeBuffer, sizeof(timeBuffer), " on %Y-%m-%d %H:%M:%S", timeInfo);
                        dateNotice << timeBuffer;
                    }
                    dateNotice << "\n";

                    // Send the metadata notice strictly to the correct channel buffer
                    LogToItemBuffer(chanNode, dateNotice);
                }
            }
            return;
        }


        // =========================================================================
        // HANDLER FOR NUMERIC 311: RPL_WHOISUSER
        // =========================================================================
        if (command == "311") {
            if (contextServer == nullptr) return;

            BString whoisLogLine = "--- [WHOIS] ";
            BStringList tokens;
            
            if (line.Split(" ", true, tokens) && tokens.CountStrings() >= 4) {
                BString targetNick = tokens.StringAt(1);
                BString username   = tokens.StringAt(2);
                BString hostname   = tokens.StringAt(3);
                
                whoisLogLine.SetToFormat("--- [WHOIS] %s is %s@%s (%s)\n", 
                    targetNick.String(), username.String(), hostname.String(), trailing.String());
            } else {
                whoisLogLine << trailing << "\n"; // Safe fallback
            }

            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, whoisLogLine);
            } else {
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), whoisLogLine);
            }
            return;
        }

        // =========================================================================
        // HANDLER FOR NUMERIC 312: RPL_WHOISSERVER
        // =========================================================================
        if (command == "312") {
            if (contextServer == nullptr) return;

            BString whoisLogLine = "--- [WHOIS] ";
            BStringList tokens;
            
            if (line.Split(" ", true, tokens) && tokens.CountStrings() >= 3) {
                BString targetNick = tokens.StringAt(1);
                BString serverName = tokens.StringAt(2);
                
                whoisLogLine.SetToFormat("--- [WHOIS] %s is using server: %s (%s)\n", 
                    targetNick.String(), serverName.String(), trailing.String());
            } else {
                whoisLogLine << trailing << "\n";
            }

            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, whoisLogLine);
            } else {
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), whoisLogLine);
            }
            return;
        }

        // =========================================================================
        // HANDLER FOR NUMERIC 317: RPL_WHOISIDLE
        // =========================================================================
        if (command == "317") {
            if (contextServer == nullptr) return;

            BString whoisLogLine = "--- [WHOIS] ";
            BStringList tokens;
            
            if (line.Split(" ", true, tokens) && tokens.CountStrings() >= 3) {
                BString targetNick = tokens.StringAt(1);
                BString seconds    = tokens.StringAt(2);
                
                whoisLogLine.SetToFormat("--- [WHOIS] %s has been %s: %s seconds\n", 
                    targetNick.String(), trailing.String(), seconds.String());
            } else {
                whoisLogLine << trailing << "\n";
            }

            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, whoisLogLine);
            } else {
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), whoisLogLine);
            }
            return;
        }

        // =========================================================================
        // HANDLER FOR NUMERIC 319: RPL_WHOISCHANNELS
        // =========================================================================
        if (command == "319") {
            if (contextServer == nullptr) return;

            BString whoisLogLine = "--- [WHOIS] ";
            BStringList tokens;
            
            if (line.Split(" ", true, tokens) && tokens.CountStrings() >= 2) {
                BString targetNick = tokens.StringAt(1);
                whoisLogLine.SetToFormat("--- [WHOIS] %s is on channels: %s\n", 
                    targetNick.String(), trailing.String());
            } else {
                whoisLogLine << trailing << "\n";
            }

            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, whoisLogLine);
            } else {
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), whoisLogLine);
            }
            return;
        }

        // =========================================================================
        // HANDLER FOR NUMERIC 330: RPL_WHOISACCOUNT (Services Login Node)
        // =========================================================================
        if (command == "330") {
            if (contextServer == nullptr) return;

            BString whoisLogLine = "--- [WHOIS] ";
            BStringList tokens;
            
            if (line.Split(" ", true, tokens) && tokens.CountStrings() >= 3) {
                BString targetNick  = tokens.StringAt(1);
                BString accountName = tokens.StringAt(2);
                whoisLogLine.SetToFormat("--- [WHOIS] %s %s account: %s\n", 
                    targetNick.String(), trailing.String(), accountName.String());
            } else {
                whoisLogLine << trailing << "\n";
            }

            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, whoisLogLine);
            } else {
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), whoisLogLine);
            }
            return;
        }

        // =========================================================================
        // HANDLER FOR NUMERIC 671: RPL_WHOISSECURE (TLS Security Protocols)
        // =========================================================================
        if (command == "671") {
            if (contextServer == nullptr) return;

            BString whoisLogLine = "--- [WHOIS] ";
            BStringList tokens;
            
            if (line.Split(" ", true, tokens) && tokens.CountStrings() >= 2) {
                BString targetNick = tokens.StringAt(1);
                whoisLogLine.SetToFormat("--- [WHOIS] %s %s\n", targetNick.String(), trailing.String());
            } else {
                whoisLogLine << trailing << "\n";
            }

            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, whoisLogLine);
            } else {
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), whoisLogLine);
            }
            return;
        }

        // =========================================================================
        // HANDLER FOR NUMERIC 318: RPL_ENDOFWHOIS (Data Burst Terminus)
        // =========================================================================
        if (command == "318") {
            if (contextServer == nullptr) return;

            BString endNotice = "--- [WHOIS] End of WHOIS list.\n";
            
            BStringItem* serverLogNode = FindServerLogNode(contextServer);
            if (serverLogNode != nullptr) {
                LogToItemBuffer(serverLogNode, endNotice);
            } else {
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), endNotice);
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

            // FIX 1: Abort if the context server background socket node is completely missing
            if (contextServer == nullptr) {
                return;
            }

            ChannelTreeItem* chanNode = FindChannelNode(contextServer, targetChannel);
            if (chanNode != nullptr) {
                BString dynamicTopic = (command == "331") ? "No topic set." : trailing;
                
                dynamicTopic.Trim();
                dynamicTopic.ReplaceAll("\r", "");
                dynamicTopic.ReplaceAll("\n", "");
                
                // === HISTORY BACKLOG PLAYBACK PROTECTION BARRIER ===
                if (fIsLoadingHistory && fChannelTopics.find(chanNode) != fChannelTopics.end()) {
                    // Skip the memory overwrite pass entirely to protect our live topic!
                } else {
                    // Cache the clean live text payload into our memory map securely
                    fChannelTopics[chanNode] = dynamicTopic;
                    
                    // FIX 2: STRICT ROOT VERIFICATION
                    // Only push changes to the active topic view banner if the channel node *exactly* matches,
                    // completely blocking text name overlaps from unrelated background networks.
                    if (fActiveBufferItem == chanNode) {
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
            // FIX 1: Secure guard to drop the data payload packet immediately if the context server is unmapped
            if (contextServer == nullptr) {
                return;
            }

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

                // Because contextServer is strictly verified, this search is safely sandboxed
                ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelTarget);
                
                // BATCH EXECUTION: Only refresh the layout view grid if this specific room is the active tab frame
                if (chanNode != nullptr && fActiveBufferItem == chanNode) {
                    
                    // --- THE LAG REMOVER: ATOMIC UI REPAINT TRANSACTION ---
                    // Locks the window thread loop to apply sorting, formatting and drawing changes 
                    // concurrently in a single processing frame rather than hundreds of tiny updates.
                    if (LockLooper()) {
                        RefreshUserListUI();
                        
                        if (fUserList != nullptr) {
                            fUserList->SortItems(SortUsersByRank);
                            fUserList->Invalidate();
                        }
                        
                        UnlockLooper(); // Safely hand control back to Haiku's App Server loop
                    }
                }
            }
            return;
        }





        // RPL_NAMREPLY: Active channel user list payload burst
        if (command == "353") { 
            if (contextServer == nullptr) return;

            BString targetChannel = "";
            int32 lastSpace = line.FindLast(" ");
            if (lastSpace != B_ERROR) {
                line.CopyInto(targetChannel, lastSpace + 1, line.Length() - (lastSpace + 1));
            } else {
                targetChannel = line;
            }
            targetChannel.Trim();

            if (targetChannel.Length() == 0) return;

            ChannelTreeItem* chanNode = FindChannelNode(contextServer, targetChannel);
            if (chanNode != nullptr && fChannelUsers[chanNode] != nullptr) {
                BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                
                BString namesBuffer = trailing;
                namesBuffer.Trim();
                
                int32 searchStart = 0;
                while (searchStart < namesBuffer.Length()) {
                    int32 nextSpace = namesBuffer.FindFirst(" ", searchStart);
                    BString rawToken;
                    
                    if (nextSpace != B_ERROR) {
                        namesBuffer.CopyInto(rawToken, searchStart, nextSpace - searchStart);
                        searchStart = nextSpace + 1;
                    } else {
                        namesBuffer.CopyInto(rawToken, searchStart, namesBuffer.Length() - searchStart);
                        searchStart = namesBuffer.Length();
                    }
                    
                    rawToken.Trim();
                    if (rawToken.Length() == 0) continue;

                    // 1. Create a temporary item to parse prefixes (~, @, +, etc.)
                    UserListItem* tempUser = new UserListItem(rawToken.String(), false);
                    BString cleanIncomingNick = tempUser->GetCleanNick();

                    // 2. REFINED DUPLICATION & UPDATE GUARD
                    bool userExists = false;
                    for (int32 u = 0; u < userVector->CountItems(); u++) {
                        UserListItem* existingUser = userVector->ItemAt(u);
                        if (existingUser != nullptr) {
                            // Compare Clean Nick to Clean Nick (e.g., "haikuuser01" == "haikuuser01")
                            if (cleanIncomingNick.ICompare(existingUser->GetCleanNick()) == 0) {
                                userExists = true;
                                
                                // UPDATE LOGIC: If the prefix changed (e.g. they got @), update the existing row
                                if (BString(existingUser->Text()) != BString(tempUser->Text())) {
                                    existingUser->SetText(tempUser->Text());
                                    // Transfer flags if your UserListItem has a Copy/Update method, 
                                    // or just recreate the text as you've done here.
                                }
                                break;
                            }
                        }
                    }

                    if (!userExists) {
                        userVector->AddItem(tempUser);
                    } else {
                        delete tempUser; // Clean up the temporary object
                    }
                }
                
                // Refresh UI if this is the active channel
                if (fActiveBufferItem == chanNode) {
                    RefreshUserListUI();
                }
            }
            return; // Changed return to break for multi-line safety
        }


        else if (command == "276") { // RPL_WHOISCERTFP
            contextServer->fSASLSuccess = true; // Locks out any accidental NickServ fallbacks
            fTextBuffers[contextServer] << ">>> TLS CertFP Authentication Successful. Identified securely via nick.pem.\n";
            this->RebuildActiveChannelBuffer();
            return;
        }


        // =========================================================================
        // DYNAMIC NICK MODIFICATION HANDLER (FIXED LIFE-CYCLE SYNC)
        // =========================================================================
        if (command == "NICK") {
            if (contextServer == nullptr) return;

            BString oldNick = prefix;
            int32 exclamIdx = oldNick.FindFirst("!");
            if (exclamIdx != B_ERROR) oldNick.Truncate(exclamIdx);

            BString newNick = trailing.Length() > 0 ? trailing : line;
            newNick.Trim();
            newNick.ReplaceAll(" ", "");
            if (newNick.Length() == 0) return;

            // --- Map visual ServerTreeItem node down to its backend Config struct safely ---
            std::string targetServerName = contextServer->Text();
            bool foundProfile = false;
            ServerConfig* resolvedProfile = nullptr;

            for (auto& srv : cfg.servers) {
                if (srv.name == targetServerName) {
                    resolvedProfile = &srv;
                    foundProfile = true;
                    break;
                }
            }
            if (!foundProfile) {
                for (auto& srv : cfg.customServers) {
                    if (srv.name == targetServerName) {
                        resolvedProfile = &srv;
                        foundProfile = true;
                        break;
                    }
                }
            }

            // ==========================================================
            // FIX 1: UPDATE SIDEBAR SERVER TAB (Dynamic Identity Sync)
            // ==========================================================
            // This ensures contextServer->Text() returns your NEW nick!
            BString serverLabel = contextServer->Text();
            int32 bracketPos = serverLabel.FindFirst(" [");
            BString networkName = " [Network]";
            
            if (bracketPos != B_ERROR) {
                // FIXED: Restored missing dot and parameter boundaries
                serverLabel.CopyInto(networkName, bracketPos, serverLabel.Length() - bracketPos);
            }
            
            BString newServerLabel;
            newServerLabel << newNick << networkName; 
            contextServer->SetText(newServerLabel.String());

            // Check nickname against the resolved specific server profile tracker
            if (resolvedProfile != nullptr && oldNick.ICompare(resolvedProfile->nick.c_str()) == 0) {
                resolvedProfile->nick = newNick.String();
                
                BString itemNotice;
                itemNotice << "--- Your nickname on this server is now " << newNick << "\n";
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), itemNotice);
                
                fMyNick = newNick; // Backwards compatibility
            }

            // ==========================================================
            // FIX 2: UPDATE CHANNEL USER VECTORS SECURELY
            // ==========================================================
            int32 totalTreeItems = fChannelTree->CountItems();
            for (int32 c = 0; c < totalTreeItems; c++) {
                BListItem* baseItem = fChannelTree->ItemAt(c);
                if (baseItem == nullptr) continue;
                
                if (fChannelTree->Superitem(baseItem) != contextServer) continue;
                
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(baseItem);
                if (chanNode == nullptr) continue;
                
                if (fChannelUsers[chanNode] != nullptr) {
                    BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                    
                    for (int32 i = 0; i < userVector->CountItems(); i++) {
                        UserListItem* uiUser = userVector->ItemAt(i);
                        if (uiUser == nullptr) continue;

                        if (oldNick.ICompare(uiUser->GetCleanNick()) == 0) {
                            // Create NEW item with NEW name but OLD prefixes
                            BString fullOldText = uiUser->Text();
                            BString prefixes = "";
                            
                            // Extract all existing symbols (~, &, @, %, +)
                            int32 pIdx = 0;
                            while (pIdx < fullOldText.Length()) {
                                char c = fullOldText.ByteAt(pIdx);
                                if (c == '~' || c == '&' || c == '@' || c == '%' || c == '+') {
                                    prefixes << c;
                                    pIdx++;
                                } else {
                                    break;
                                }
                            }

                            BString updatedDisplayName;
                            updatedDisplayName << prefixes << newNick;

                            // Replace the item to ensure UserListItem internal flags are re-parsed
                            userVector->RemoveItemAt(i);
                            delete uiUser;
                            userVector->AddItem(new UserListItem(updatedDisplayName.String(), false), i);
                            
                            BString nickNotice;
                            nickNotice << "--- " << oldNick << " is now known as " << newNick << "\n";
                            LogToItemBuffer(chanNode, nickNotice);
                            
                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                                fUserList->SortItems(SortUsersByRank);
                                fUserList->Invalidate();
                            }
                            break; 
                        }
                    }
                }
            }
             // Change return to break for multi-line processing safety
        }






		
		/*  Still working on this one
        // =========================================================================
        // FRESH MULTI-SERVER CHAT & NOTICE ROUTING ENGINE (OPENSSL INTEGRATED)
        // =========================================================================
        if (command == "PRIVMSG" || command == "NOTICE") {
            if (contextServer == nullptr) return;

            BString senderNick = prefix;
            int32 exclamIdx = senderNick.FindFirst("!");
            if (exclamIdx != B_ERROR) senderNick.Truncate(exclamIdx);            
            
            // --- UPDATED: WILDCARD PRIVMSG / NOTICE FILTER ENGINE ---
            if (senderNick.Length() > 0 && !contextServer->fRuntimeIgnoreList.empty()) {
                bool shouldDrop = false;
                for (const auto& ignoredNick : contextServer->fRuntimeIgnoreList) {
                    if (MatchWildcard(senderNick.String(), ignoredNick.c_str())) {
                        shouldDrop = true;
                        break;
                    }
                }
                if (shouldDrop) {
                    return; // SILENTLY DROP PACKET PAYLOAD MESSAGES
                }
            }

            int32 msgTargetSpace = line.FindFirst(" ");
            BString targetRoom = line;
            if (msgTargetSpace != B_ERROR) targetRoom.Truncate(msgTargetSpace);
            targetRoom.ReplaceAll(" ", "");

            // =========================================================================
            // OPENSSL INTEGRATED CTCP AUTOMATED RESPONSE ENGINE
            // =========================================================================
            if (command == "PRIVMSG" && trailing.StartsWith("\x01") && trailing.EndsWith("\x01")) {
                BString ctcpQuery = trailing;
                ctcpQuery.RemoveAll("\x01");
                
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                int activeFd = (gServerRawSockets.count(static_cast<void*>(contextServer)) > 0) ? 
                                gServerRawSockets[static_cast<void*>(contextServer)] : -1;

                if (activeSslHandle != nullptr && activeFd >= 0) {
                    BString ctcpResponse = "";
                    BString logText = "";

                    if (ctcpQuery.StartsWith("VERSION")) {
                        ctcpResponse << "NOTICE " << senderNick << " :\x01VERSION Cricket IRC v" << AppInfo::VERSION_STRING << " (Haiku-OS)\x01\r\n";
                        logText << "--- [CTCP] Version query from " << senderNick << " answered automatically via OpenSSL.\n";
                    }
                    else if (ctcpQuery.StartsWith("PING")) {
                        ctcpResponse << "NOTICE " << senderNick << " :\x01" << ctcpQuery << "\x01\r\n";
                        logText << "--- [CTCP] PING query from " << senderNick << " replied over secure context.\n";
                    }

                    if (ctcpResponse.Length() > 0) {
                        SSL_write(activeSslHandle, ctcpResponse.String(), ctcpResponse.Length());
                        LogToItemBuffer(FindServerLogNode(contextServer), logText);
                        //break; // Secure breakout pass
                    }
                }
            }

            // --- Map visual ServerTreeItem node down to its backend Config indices safely ---
            std::string targetServerName = contextServer->Text();
            bool foundProfile = false;
            size_t resolvedIndex = 0;
            bool resolvedIsCustom = false;
            ServerConfig* resolvedProfile = nullptr;

            for (size_t i = 0; i < cfg.servers.size(); i++) {
                if (cfg.servers[i].name == targetServerName) {
                    resolvedIndex = i;
                    resolvedIsCustom = false;
                    foundProfile = true;
                    resolvedProfile = &cfg.servers[i];
                    break;
                }
            }
            if (!foundProfile) {
                for (size_t i = 0; i < cfg.customServers.size(); i++) {
                    if (cfg.customServers[i].name == targetServerName) {
                        resolvedIndex = i;
                        resolvedIsCustom = true;
                        foundProfile = true;
                        resolvedProfile = &cfg.customServers[i];
                        break;
                    }
                }
            }

            // Establish destination nodes supporting Channels AND Private Message Queries
            BStringItem* targetNode = nullptr;
            if (targetRoom.StartsWith("#") || targetRoom.StartsWith("&") || targetRoom.StartsWith("!")) {
                targetNode = FindChannelNode(contextServer, targetRoom);
            } else {
                targetNode = FindChannelNode(contextServer, senderNick);
                
                // === AUTOMATION HOOK: Auto-create tab if someone pings us first ===
                if (targetNode == nullptr && contextServer != nullptr) {
                    size_t activeServerIdx = contextServer->GetIndex();
                    bool activeServerIsCustom = contextServer->IsCustom();

                    ChannelTreeItem* newQueryNode = new ChannelTreeItem(senderNick.String(), 
                                                                        activeServerIdx, 
                                                                        activeServerIsCustom);
                    
                    fChannelTree->AddUnder(newQueryNode, contextServer);
                    fChannelTree->Expand(contextServer);
                    
                    fChannelUsers[newQueryNode] = new BObjectList<UserListItem, true>(20);
                    LogToItemBuffer(newQueryNode, BString("--- Incoming Private Conversation started with ") << senderNick << "\n");
                    targetNode = newQueryNode;
                }
            }

            if (targetNode == nullptr) {
                targetNode = FindServerLogNode(contextServer);
            }
            if (targetNode == nullptr) {
                targetNode = static_cast<BStringItem*>(contextServer);
            }

            // Thread-Safe Timing Calculation Engine
            BString timestampPrefix = "";
            bigtime_t currentTime = real_time_clock_usecs(); // Native Haiku system clock            
            
            if (targetNode != nullptr) {
                int32 intervalMinutes = 30; // Foolproof fallback baseline standard
                int32 activeSrvIdx = GetServerIndexFromNode(targetNode); // Query server index matrix
                
                if (activeSrvIdx >= 0) {
                    if (activeSrvIdx < (int32)cfg.servers.size()) {
                        intervalMinutes = cfg.servers[activeSrvIdx].timestampInterval;
                    } else {
                        int32 cIdx = activeSrvIdx - (int32)cfg.servers.size();
                        if (cIdx >= 0 && cIdx < (int32)cfg.customServers.size()) {
                            intervalMinutes = cfg.customServers[cIdx].timestampInterval;
                        }
                    }
                }

                bigtime_t dynamicIntervalInUsecs = (bigtime_t)intervalMinutes * 60 * 1000000;

                bool needsTimestamp = false;
                if (fLastTimestampTime.count(targetNode) == 0) {
                    needsTimestamp = true;
                } else {
                    bigtime_t lastTime = fLastTimestampTime.find(targetNode)->second;
                    if ((currentTime - lastTime) >= dynamicIntervalInUsecs) {
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

            // =========================================================================
            // FIXED INTEGRATED HOOK: UNIVERSAL INBOUND SERVICES SCANNER & DISPATCH (ANTI-FLOOD GATE)
            // =========================================================================
            bool isServicesMessage = (prefix.IFindFirst("nickserv") != B_ERROR || 
                                      prefix.IFindFirst("services") != B_ERROR || 
                                      senderNick.ICompare("NickServ") == 0);
                                      
            bool isIdentifyChallenge = (line.IFindFirst("identify") != B_ERROR || 
                                        trailing.IFindFirst("identify") != B_ERROR ||
                                        line.IFindFirst("authenticate") != B_ERROR ||
                                        line.IFindFirst("registered and protected") != B_ERROR);

            // ANTI-FLOOD ENFORCEMENT: Check fHasIdentifiedThisSession to prevent multi-line notification floods!
            if (isServicesMessage && isIdentifyChallenge && contextServer != nullptr && 
                !contextServer->fSASLSuccess && !contextServer->fHasIdentifiedThisSession) {                
                
                if (cfg.debugEnable) printf("[DEBUG_NOTICE] [%s] Fallback service request matched. Processing credentials...\n", contextServer->Text());
                
                if (foundProfile && resolvedProfile != nullptr && !resolvedProfile->pass.empty()) {
                    BString autoIdentify = "";
                    BString currentTargetHost = contextServer->GetHost();

                    if (currentTargetHost.IFindFirst("oftc.net") != B_ERROR) {
                        autoIdentify << "IDENTIFY " << resolvedProfile->pass.c_str() << "\r\n";
                    } else {
                        autoIdentify << "PRIVMSG NickServ :IDENTIFY " << resolvedProfile->pass.c_str() << "\r\n";
                    }
                    
                    SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                    int activeFd = (gServerRawSockets.count(static_cast<void*>(contextServer)) > 0) ? 
                                    gServerRawSockets[static_cast<void*>(contextServer)] : -1;

                    if (activeSslHandle != nullptr && activeFd >= 0) {
                        SSL_write(activeSslHandle, autoIdentify.String(), autoIdentify.Length());
                        
                        // ANTI-FLOOD LOCK: Hard lock the machine state instantly to skip following notices!
                        contextServer->fHasIdentifiedThisSession = true;
                        if (cfg.debugEnable) printf("[DEBUG_NOTICE] [%s] Auto-Identify sent via SSL_write. Anti-flood gate activated.\n", contextServer->Text());
                    }

                    BString logNotice = "--- [Auto-Services] Pre-registration identification payload sent to NickServ.\n";
                    LogToItemBuffer(static_cast<BStringItem*>(contextServer), logNotice);
                }
            }
            // =========================================================================

            // Assemble and Format Chat Log Lines
            BString formattedMessage = "";
            if (timestampPrefix.Length() > 0) {
                formattedMessage << timestampPrefix;
            }

            if (command == "PRIVMSG") {
                BString messageBody = trailing;

                // =========================================================================
                //  CTCP ACTION /me GREEDY DECODER HOOK
                // =========================================================================
                // Breaking up string literals prevents the preprocessor from misinterpreting the hex bounds!
					if (messageBody.StartsWith("\x01" "ACTION ") || messageBody.StartsWith("\001" "ACTION ")) {

                    // Strip the leading \x01ACTION marker safely
                    messageBody.Remove(0, 8);
                    
                    // Strip the trailing \x01 marker if it exists
                    if (messageBody.EndsWith("\x01") || messageBody.EndsWith("\001")) {
                        messageBody.Truncate(messageBody.Length() - 1);
                    }
                    
                    // Format as classic standard action text output style: * Nick message
                    formattedMessage << "* " << senderNick << " " << messageBody << "\n";
                } else {
                    // Standard explicit room message delivery style
                    formattedMessage << "<" << senderNick << "> " << messageBody << "\n";
                }
            } else {
                formattedMessage << "-!- " << senderNick << " NOTICE: " << trailing << "\n";
            }

            // Append formatting payload to the matching memory array cell tracking keys
            LogToItemBuffer(targetNode, formattedMessage);

            // =========================================================================
            // FIXED UNIFIED INTERFACE GRAPHICS REPAINT FOR AUTOTABS & MOTD LOGS
            // =========================================================================
            if (fActiveBufferItem == targetNode) {
                if (fCustomChatLog != nullptr) {
                    fCustomChatLog->InvalidateLayout();
                    fCustomChatLog->Invalidate();
                } else if (fChatLog != nullptr) {
                    fChatLog->Invalidate();
                }
            }            
             
        }            
       return; 
    	*/ 
   





      // PRIVMSG & NOTICE Message Routing Engine
        if (command == "PRIVMSGxx" || command == "NOTICExx") {
            if (contextServer == nullptr) return;

            BString senderNick = prefix;
            int32 exclamIdx = senderNick.FindFirst("!");
            if (exclamIdx != B_ERROR) senderNick.Truncate(exclamIdx);            
            
            // --- UPDATED: WILDCARD PRIVMSG / NOTICE FILTER ENGINE ---
            if (senderNick.Length() > 0 && !contextServer->fRuntimeIgnoreList.empty()) {
                bool shouldDrop = false;
                for (const auto& ignoredNick : contextServer->fRuntimeIgnoreList) {
                    
                    // Call our wildcard matcher cleanly using the incoming nickname 
                    if (MatchWildcard(senderNick.String(), ignoredNick.c_str())) {
                        shouldDrop = true;
                        break;
                    }
                }
                if (shouldDrop) {
                    return; // SILENTLY DROP PACKET PAYLOAD MESSAGES
                }
            }

            int32 msgTargetSpace = line.FindFirst(" ");
            BString targetRoom = line;
            if (msgTargetSpace != B_ERROR) targetRoom.Truncate(msgTargetSpace);
            targetRoom.ReplaceAll(" ", "");

            // Handle Automated CTCP Version Queries
            if (command == "PRIVMSG" && (trailing.StartsWith("\x01VERSION\x01") || trailing.StartsWith("\1VERSION\1"))) {
                BSecureSocket* activeSocket = nullptr;
                if (fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else if (contextServer == fOftcNode) {
                    activeSocket = fOftcSocket;
                } else if (contextServer == fLiberaNode) {
                    activeSocket = fLiberaSocket;
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

            // --- FIX 3: Map visual ServerTreeItem node down to its backend Config indices safely ---
            std::string targetServerName = contextServer->Text();
            bool foundProfile = false;
            size_t resolvedIndex = 0;
            bool resolvedIsCustom = false;

            for (size_t i = 0; i < cfg.servers.size(); i++) {
                if (cfg.servers[i].name == targetServerName) {
                    resolvedIndex = i;
                    resolvedIsCustom = false;
                    foundProfile = true;
                    break;
                }
            }
            if (!foundProfile) {
                for (size_t i = 0; i < cfg.customServers.size(); i++) {
                    if (cfg.customServers[i].name == targetServerName) {
                        resolvedIndex = i;
                        resolvedIsCustom = true;
                        foundProfile = true;
                        break;
                    }
                }
            }

        // =========================================================================
        // FIXED PRIVATE MESSAGE INTAKE ROUTER (NETWORK ISOLATION FIX)
        // =========================================================================

        // 1. Establish destination nodes supporting Channels AND Private Message Queries
        BStringItem* targetNode = nullptr;
        if (targetRoom.StartsWith("#")) {
            targetNode = FindChannelNode(contextServer, targetRoom);
        } else {
            // Look for an existing one-on-one query tab under this explicit server
            targetNode = FindChannelNode(contextServer, senderNick);
            
            // === AUTOMATION HOOK: Auto-create tab if someone pings us first ===
            if (targetNode == nullptr && contextServer != nullptr) {
                
                // FIXED VALUE MAP ROUTING: Derive the index parameters explicitly 
                // from the active contextServer object to prevent Libera Chat hijack cross-talk!
                size_t activeServerIdx = contextServer->GetIndex();
                bool activeServerIsCustom = contextServer->IsCustom();

                ChannelTreeItem* newQueryNode = new ChannelTreeItem(senderNick.String(), 
                                                                    activeServerIdx, 
                                                                    activeServerIsCustom);
                
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
        // =========================================================================


            // 2. Thread-Safe Timing Calculation Engine
            BString timestampPrefix = "";
            bigtime_t currentTime = real_time_clock_usecs(); // Native Haiku system clock            
            
            if (targetNode != nullptr) {
                // 1. FETCH DYNAMIC CONFIGURATION INTERVAL
                int32 intervalMinutes = 30; // Foolproof fallback baseline standard
                int32 activeSrvIdx = GetServerIndexFromNode(targetNode); // Query server index matrix
                
                if (activeSrvIdx >= 0) {
                    if (activeSrvIdx < (int32)cfg.servers.size()) {
                        intervalMinutes = cfg.servers[activeSrvIdx].timestampInterval;
                    } else {
                        int32 cIdx = activeSrvIdx - (int32)cfg.servers.size();
                        if (cIdx >= 0 && cIdx < (int32)cfg.customServers.size()) {
                            intervalMinutes = cfg.customServers[cIdx].timestampInterval;
                        }
                    }
                }

                // 2. Mathematically compute intervals dynamically
                bigtime_t dynamicIntervalInUsecs = (bigtime_t)intervalMinutes * 60 * 1000000;

                bool needsTimestamp = false;
                if (fLastTimestampTime.count(targetNode) == 0) {
                    needsTimestamp = true;
                } else {
                    bigtime_t lastTime = fLastTimestampTime.find(targetNode)->second;
                    //  Checks against dynamic duration choice instead of hardcoded 30 minutes!
                    if ((currentTime - lastTime) >= dynamicIntervalInUsecs) {
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

            // ---  Safely reuse un-typed variables, and declare resolvedProfile inside this block scope ---
            targetServerName = contextServer->Text();
            foundProfile = false;
            ServerConfig* resolvedProfile = nullptr; // Explicitly declared here so the rest of the function can see it!

            for (auto& srv : cfg.servers) {
                if (srv.name == targetServerName) {
                    resolvedProfile = &srv;
                    foundProfile = true;
                    break;
                }
            }
            if (!foundProfile) {
                for (auto& srv : cfg.customServers) {
                    if (srv.name == targetServerName) {
                        resolvedProfile = &srv;
                        foundProfile = true;
                        break;
                    }
                }
            }
            
            // =========================================================================
            // DYNAMIC ANTI-FLOOD GATEWAY: CHANNELS SERVICES AUTHENTICATION HANDLER
            // =========================================================================   
            if (senderNick.StartsWith("NickServ") && trailing.IFindFirst("identify") != B_ERROR && contextServer != nullptr && !contextServer->fSASLSuccess) {                
                BSecureSocket* activeSocket = nullptr;
                if (fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else if (contextServer == fOftcNode) {
                    activeSocket = fOftcSocket;
                } else if (contextServer == fLiberaNode) {
                    activeSocket = fLiberaSocket;
                }
                if (activeSocket != nullptr) {
                    if (contextServer->fHasIdentifiedThisSession) {
                        return;
                    }

                    const ServerConfig* activeProfile = nullptr;                    
                    for (const auto& srv : cfg.servers) {
                        if (BString(srv.host.c_str()).ICompare(contextServer->GetHost()) == 0) {
                            activeProfile = &srv;
                            break;
                        }
                    }                    
                    if (activeProfile == nullptr) {
                        for (const auto& srv : cfg.customServers) {
                            if (BString(srv.host.c_str()).ICompare(contextServer->GetHost()) == 0) {
                                activeProfile = &srv;
                                break;
                            }
                        }
                    }
                    if (activeProfile != nullptr && activeProfile->pass.length() > 0) {
                        BString currentActiveNick = contextServer->GetNick();                        
                        BString primaryConfigNick(activeProfile->nick.c_str());
                        BString altConfigNick(activeProfile->altNick.c_str());
                        BString altConfigNick2(activeProfile->altNick2.c_str());
                        
                        if (currentActiveNick.ICompare(primaryConfigNick) == 0) {
                            
                            BString autoIdentify;
                            autoIdentify << "PRIVMSG NickServ :IDENTIFY " << activeProfile->pass.c_str() << "\r\n";
                            activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                            contextServer->fHasIdentifiedThisSession = true;                            
                            BString logNotice = "--- [Auto-Services] Identification credentials automatically sent to NickServ.\n";
                            LogToItemBuffer(FindServerLogNode(contextServer), logNotice);
                            
                        } else {
                            contextServer->fHasIdentifiedThisSession = true;
                            if (cfg.debugEnable) printf("[ANTI-FLOOD] Aborted automated authentication: Active handle '%s' is an alternate nickname variant.\n",
                                   currentActiveNick.String());
                        }
                    }
                }
            }
            // ANTI-FLOOD HOOK 2: Catch explicit failure notices instantly

            if (senderNick.StartsWith("NickServ") && (trailing.IFindFirst("Identify failed") != B_ERROR || trailing.IFindFirst("incorrect password") != B_ERROR)) {
                if (contextServer != nullptr) {
                    contextServer->fHasIdentifiedThisSession = true; // Turn off the machine!
                    if (cfg.debugEnable) printf("[ANTI-FLOOD] NickServ failure warning detected. Identity authentication loop halted completely.\n");
                }
            }
            // 5. Route structured messages to their designated text buffers
            if (targetRoom.StartsWith("#")) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(targetNode);
                if (chanNode) {                    
                    BString localNickToCheck = (resolvedProfile != nullptr) 
                        ? BString(resolvedProfile->nick.c_str()) 
                        : BString(fMyNick);
                        
                    if (senderNick == localNickToCheck) {
                        UpdateUserAwayState(chanNode, senderNick.String(), false);
                    }
                    if (fActiveBufferItem != chanNode) {
                        chanNode->SetUnread(true);
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(chanNode)); 
                    }
                    LogToItemBuffer(chanNode, formattedMsg);
                } else {
                    LogToItemBuffer(FindServerLogNode(contextServer), formattedMsg);
                }
            } else {
                LogToItemBuffer(targetNode, formattedMsg);
            }
            return;

        }       
        // Bracket-Sanitized Global Server Protocol Fallback Route
        if (trailing.Length() > 0) {
            if (contextServer == nullptr) {
                contextServer = fCurrentServerNode;
            }
            
        // =========================================================================
        // UNIVERSAL SERVICES INBOUND SCANNER (TAB-MISMATCH RESILIENT)
        // =========================================================================
        BString lowerLine = line; lowerLine.ToLower();
        BString lowerPrefix = prefix; lowerPrefix.ToLower();

        bool isAuthNotice = (lowerPrefix.FindFirst("nickserv") != B_ERROR || 
                             lowerPrefix.FindFirst("services@") != B_ERROR || 
                             lowerLine.FindFirst("nickserv") != B_ERROR);
                             
        bool containsIdentifyReq = (lowerLine.FindFirst("identify") != B_ERROR || 
                                    lowerLine.FindFirst("authenticate") != B_ERROR);

        if (isAuthNotice && containsIdentifyReq && contextServer != nullptr && !contextServer->fSASLSuccess) {                
            if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Inbound services challenge matched! Processing fallback authentication...\n", contextServer->Text());
            
            std::string targetServerName = contextServer->Text();
            bool foundProfile = false;
            ServerConfig* resolvedProfile = nullptr;

            for (auto& srv : cfg.servers) {
                if (srv.name == targetServerName) { resolvedProfile = &srv; foundProfile = true; break; }
            }
            if (!foundProfile) {
                for (auto& srv : cfg.customServers) {
                    if (srv.name == targetServerName) { resolvedProfile = &srv; break; }
                }
            }
            
            if (resolvedProfile != nullptr && resolvedProfile->pass.length() > 0) {
                BString autoIdentify;
                autoIdentify << "PRIVMSG NickServ :IDENTIFY " << resolvedProfile->pass.c_str() << "\r\n";
                
                // Fetch the live background encryption reference handle natively out of the top global tracker map
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                int activeFd = gServerRawSockets[static_cast<void*>(contextServer)];

                if (activeSslHandle != nullptr && activeFd >= 0) {
                    SSL_write(activeSslHandle, autoIdentify.String(), autoIdentify.Length());
                    if (cfg.debugEnable) printf("[DEBUG_CAP] [%s] Auto-Identify sent successfully over live OpenSSL handle context.\n", contextServer->Text());
                } else {
                    BSecureSocket* activeSocket = GetActiveSocket(contextServer);
                    if (activeSocket != nullptr) {
                        activeSocket->Write(autoIdentify.String(), autoIdentify.Length());                        
                    }
                }

                BString logNotice = "--- [Auto-Services] Pre-registration identification sent to NickServ.\n";
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), logNotice);
            }
        }
        		// =========================================================================
				/*
	            // FIX 2: Only print message data loops if we have a valid resolved text panel workspace node
	            if (contextServer != nullptr) {
	                BString rawLog;
	                rawLog << trailing << "\n";
	                LogToItemBuffer(static_cast<BStringItem*>(contextServer), rawLog);
	        	}
	        	*/
        }
	}
	
	
	

static status_t NetworkLoop(void* data) {    	
    ServerTreeItem* targetNode = static_cast<ServerTreeItem*>(data);
    if (targetNode == nullptr) {
       if (cfg.debugEnable) printf("[DEBUG_LOOP] CRITICAL: targetNode is NULL.\n");
        return B_ERROR;
    }

    CricketWindow* window = dynamic_cast<CricketWindow*>(be_app->WindowAt(0));
    if (window == nullptr) {
       if (cfg.debugEnable) printf("[DEBUG_LOOP] CRITICAL: CricketWindow context is NULL.\n");
        return B_ERROR;
    }
	
   if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Starting thread connection pipeline.\n", targetNode->Text());
    targetNode->fHasFinalizedCap = false;
    targetNode->fHasIdentifiedThisSession = false;
    targetNode->fSASLSuccess = false;
	 
    BNetworkAddress address(targetNode->GetHost().String(), targetNode->GetPort());
    
    // 1. Thread-Safe Profiling Lookups (Executed once safely at the entryway!)
    bool useSASL = false;
    bool useCertFP = false;
    std::string certName = "";
    std::string cachedPass = ""; 
    std::string cachedNick = targetNode->GetNick().String();
    
    std::string cachedCertFile = "";
    std::string cachedKeyFile = "";

    BString currentLoopHost = targetNode->GetHost();
    for (const auto& srv : cfg.servers) {
        if (currentLoopHost.ICompare(srv.host.c_str()) == 0) {
            useSASL = srv.useSASL; useCertFP = srv.useCertFP; certName = srv.certProfileName; cachedPass = srv.pass;
            cachedCertFile = srv.certFileName; cachedKeyFile = srv.keyFileName;
            break;
        }
    }
    for (const auto& srv : cfg.customServers) {
        if (currentLoopHost.ICompare(srv.host.c_str()) == 0) {
            useSASL = srv.useSASL; useCertFP = srv.useCertFP; certName = srv.certProfileName; cachedPass = srv.pass;
            cachedCertFile = srv.certFileName; cachedKeyFile = srv.keyFileName;
            break;
        }
    }

    if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Target Parameters -> useSASL: %d, useCertFP: %d, cachedNick: '%s'\n", 
           targetNode->Text(), useSASL, useCertFP, cachedNick.c_str());

    // 2. Establish raw TCP connection pipeline via BNetEndpoint
    BNetEndpoint* localSocket = new BNetEndpoint();
    if (localSocket->Connect(targetNode->GetHost().String(), targetNode->GetPort()) != B_OK) {
        if (window->Lock()) {
            BMessage* reply = new BMessage(MSG_IRC_RECEIVED);
            reply->AddString("text", BString("Connection failed to ") << targetNode->GetHost() << "\n");
            reply->AddPointer("server_node", targetNode);
            window->PostMessage(reply);
            window->Unlock();
        }
        delete localSocket;
        return B_ERROR;
    }

    // 3. OpenSSL Manual TLS Handshake Layer
    SSL* globalSslHandle = nullptr;
    SSL_CTX* globalSslContext = nullptr;

    BPath certPath;
    BPath keyPath;
    bool hasValidCert = false;
    
    std::string targetCertName = (!cachedCertFile.empty()) ? cachedCertFile : "nick.crt";
    std::string targetKeyName  = (!cachedKeyFile.empty())  ? cachedKeyFile  : "nick.key";

    if (useCertFP) {
        if (find_directory(B_USER_SETTINGS_DIRECTORY, &certPath) == B_OK &&
            find_directory(B_USER_SETTINGS_DIRECTORY, &keyPath) == B_OK) {
            
            certPath.Append("cricket/certs");
            certPath.Append(targetCertName.c_str());
            
            keyPath.Append("cricket/certs");
            keyPath.Append(targetKeyName.c_str());
            
            struct stat stCert, stKey;
            if (stat(certPath.Path(), &stCert) == 0 && stat(keyPath.Path(), &stKey) == 0) {
                hasValidCert = true;
                if (cfg.debugEnable) {
                    printf("[DEBUG_LOOP] [%s] CertFP files verified -> CRT: '%s', KEY: '%s'\n", 
                           targetNode->Text(), targetCertName.c_str(), targetKeyName.c_str());
                }
            } else {
                if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] WARNING: CertFP files missing on disk! Verification failed.\n", targetNode->Text());
            }
        }
    }

    //  Instantiate OpenSSL Context universally for all servers!
    globalSslContext = SSL_CTX_new(TLS_client_method());
    if (globalSslContext != nullptr) {
        
        //  Only load user-defined identity keys if CertFP is explicitly validated
        if (hasValidCert) {
            if (SSL_CTX_use_certificate_chain_file(globalSslContext, certPath.Path()) != 1) {
                if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] OpenSSL Error: Failed to chain public certificate component.\n", targetNode->Text());
            }
            if (SSL_CTX_use_PrivateKey_file(globalSslContext, keyPath.Path(), SSL_FILETYPE_PEM) != 1) {
                if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] OpenSSL Error: Failed to link private cryptographic key component.\n", targetNode->Text());
            } else {
                if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] OpenSSL verified and locked custom private keys cleanly.\n", targetNode->Text());
            }
        }

        //  Allocation handles and encryption connection calls now run safely outside the cert file logic check!
        globalSslHandle = SSL_new(globalSslContext);
        if (globalSslHandle != nullptr) {
            SSL_set_fd(globalSslHandle, localSocket->Socket());
            
            if (SSL_connect(globalSslHandle) != 1) {
                if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] SSL Handshake Failed via OpenSSL pipeline.\n", targetNode->Text());
            } else {
                if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Secure TLS encryption handshake fully completed.\n", targetNode->Text());
            }
        }
    }



    window->Lock(); 
    window->fServerSockets[targetNode] = reinterpret_cast<BSecureSocket*>(localSocket);
    gServerSslHandles[static_cast<void*>(targetNode)] = globalSslHandle;
    gServerRawSockets[static_cast<void*>(targetNode)] = localSocket->Socket();
    
    if (targetNode == window->fLiberaNode) window->fLiberaSocket = reinterpret_cast<BSecureSocket*>(localSocket);
    if (targetNode == window->fOftcNode)   window->fOftcSocket = reinterpret_cast<BSecureSocket*>(localSocket);
    window->Unlock();

      // 4. Outbound Handshake Registrations
    if (targetNode->GetPass().Length() > 0 && !useSASL) {
        BString passHandshake; passHandshake << "PASS " << targetNode->GetPass() << "\r\n";
        SSL_write(globalSslHandle, passHandshake.String(), passHandshake.Length());
    }

    BString capHandshake = "CAP LS 302\r\n";
    SSL_write(globalSslHandle, capHandshake.String(), capHandshake.Length());

    BString nickHandshake; nickHandshake << "NICK " << targetNode->GetNick() << "\r\n";
    SSL_write(globalSslHandle, nickHandshake.String(), nickHandshake.Length());
	
    BString userHandshake; userHandshake << "USER " << targetNode->GetNick() << " 0 * :Cricket IRC Client\r\n";
    SSL_write(globalSslHandle, userHandshake.String(), userHandshake.Length());
    
    // =========================================================================
    //  CAP ENROLLMENT HANDSHAKE ASSIGNMENTS
    // =========================================================================
    if (!useSASL && !useCertFP) {
        BString finalizeCap = "CAP END\r\n";
        SSL_write(globalSslHandle, finalizeCap.String(), finalizeCap.Length());
        
        // REMOVED: targetNode->fHasFinalizedCap = true;
        // Letting this remain false allows ParseAndDisplayIRC to process 
        // the incoming server CAP lines natively without throwing them out!
        
        if (cfg.debugEnable) {
            LogDebugStream(targetNode->Text(), "OUTGOING", finalizeCap.String(), finalizeCap.Length());
        }
    }
    // =========================================================================



    // =========================================================================
    // 5. HIGH-SPEED MAIN STREAM READ LOOP (FIXED STATIC ARRAY CONTAINER)
    // =========================================================================
    //  Swapped 'char buffer;' for a true 1024-byte static block array!
    char buffer[1024]; 
    ssize_t bytesRead;
    BString lineBuffer;
    
    if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Entering data streaming loop pass (Buffer size: 1024).\n", targetNode->Text());

    while ((bytesRead = SSL_read(globalSslHandle, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0'; // Natively safe termination bounds pass!          

        lineBuffer.Append(buffer, bytesRead);

        // =========================================================================
        // DIAGNOSTIC INLINE SASL AUTHENTICATION CHALLENGE INTERCEPT ENGINE
        // =========================================================================
        if (lineBuffer.IFindFirst("AUTHENTICATE +") != B_ERROR) {
            if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] INLINE HOOK: Captured raw 'AUTHENTICATE +' challenge token!\n", targetNode->Text());
            
            std::string rawPayload = cachedNick;
            rawPayload.push_back('\0'); rawPayload += cachedNick; rawPayload.push_back('\0'); rawPayload += cachedPass;
            
            BString sendPayload;
            sendPayload << "AUTHENTICATE " << base64_encode(rawPayload).c_str() << "\r\n";
            
            if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Transmitting Base64 dynamic token frame payload down the wire...\n", targetNode->Text());
            ssize_t written = SSL_write(globalSslHandle, sendPayload.String(), sendPayload.Length());
            if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Handshake challenge payload transmitted. (Bytes written: %ld)\n", targetNode->Text(), written);
            
            lineBuffer.ReplaceAll("AUTHENTICATE +", "");
            lineBuffer.ReplaceAll("\r", ""); lineBuffer.ReplaceAll("\n", "");
            if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Line cache cleared. Resuming reader loop stream.\n", targetNode->Text());
            continue;
        }
        // =========================================================================

        int32 newlinePos;
        while ((newlinePos = lineBuffer.FindFirst("\n")) != B_ERROR) {
            BString line;
            lineBuffer.CopyInto(line, 0, newlinePos + 1);
            lineBuffer.Remove(0, newlinePos + 1);
            BString cleanLine = line;
            cleanLine.ReplaceAll("\r", ""); cleanLine.ReplaceAll("\n", ""); cleanLine.Trim();

            if (cleanLine.Length() == 0) continue;

            if (cleanLine.StartsWith("PING")) {
                BString pong = cleanLine;
                pong.Replace("PING", "PONG", 1);
                pong << "\r\n"; 
                SSL_write(globalSslHandle, pong.String(), pong.Length());
                continue;
            }

            // Watch raw lines entering display parsing handlers
            if (cleanLine.IFindFirst("CAP") != B_ERROR || cleanLine.IFindFirst("903") != B_ERROR || cleanLine.IFindFirst("904") != B_ERROR) {
                if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Forwarding to UI Handler -> '%s'\n", targetNode->Text(), cleanLine.String());
            }

            BMessage* reply = new BMessage(MSG_IRC_RECEIVED);
            reply->AddString("text", cleanLine.String());
            reply->AddPointer("server_node", targetNode);
            window->PostMessage(reply);
        }
    }        
     

    bool triggerReconnect = targetNode->IsAutoReconnect();
    if (be_app->CountWindows() > 0 && be_app->WindowAt(0) == window) {
        if (window->Lock()) { 
            window->fServerSockets.erase(targetNode);
            window->fServerThreads.erase(targetNode);
            gServerSslHandles.erase(static_cast<void*>(targetNode));
            gServerRawSockets.erase(static_cast<void*>(targetNode));
            if (targetNode == window->fLiberaNode) { window->fLiberaThread = -1; window->fLiberaSocket = nullptr; }
            if (targetNode == window->fOftcNode)   { window->fOftcThread = -1;   window->fOftcSocket = nullptr; }
            window->Unlock(); 
        }
    }

    if (triggerReconnect && be_app->CountWindows() > 0 && be_app->WindowAt(0) == window) {
        if (window->Lock()) {
            BMessage* reconnectMessage = new BMessage(MSG_RECONNECT_SERVER);
            reconnectMessage->AddPointer("server_item", targetNode);
            window->PostMessage(reconnectMessage);
            window->Unlock();
        }
    }

    if (globalSslHandle) SSL_free(globalSslHandle);
    if (globalSslContext) SSL_CTX_free(globalSslContext);
    delete localSocket;
    if (cfg.debugEnable) printf("[DEBUG_LOOP] [%s] Thread pipeline clean-up complete. Exiting.\n", targetNode->Text());
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

        // =========================================================================
        // GOOGLE TRANSLATE SELECTION EXTENSION (MAIN WINDOW IMPLEMENTATION)
        // =========================================================================
        case 'trns': {
            BString targetLanguage = "en"; 
            if (message->FindString("lang", &targetLanguage) != B_OK) {
                targetLanguage = "en";
            }

            // Unpack the full text selection string directly from the message payload basket
            BString selectedText = "";
            if (message->FindString("text", &selectedText) != B_OK) {
                // Safe component fallback check if the text field was somehow missing
                if (fCustomChatLog != nullptr) {
                    selectedText = fCustomChatLog->GetSelectedText();
                }
            }

            // Trim any layout whitespace remnants or stray characters safely
            selectedText.Trim();
            selectedText.ReplaceAll("\r", "");
            selectedText.ReplaceAll("\n", " ");
            selectedText.Trim();

            // =========================================================================
            // FIXED UNIFIED WEB TRANSLATOR (MIMICS GOOGLE SEARCH W/ STRING APPENDING)
            // =========================================================================
            if (selectedText.Length() > 0) {
                // URL-escape special network characters securely (Matches Search engine logic)
                selectedText.ReplaceAll("%", "%25"); // Escape percent signs first
                selectedText.ReplaceAll(" ", "+");   // Google Search uses '+' for spaces
                selectedText.ReplaceAll("&", "%26");
                selectedText.ReplaceAll("?", "%3F");
                selectedText.ReplaceAll("=", "%3D");
                selectedText.ReplaceAll("#", "%23");
                selectedText.ReplaceAll("/", "%2F"); 
                
                // =========================================================================
                //  EXPLICIT STRING APPENDING LAYOUT (GUARANTEES TAIL-END PLACEMENT)
                // =========================================================================
                // This mimics 'gsh' logic perfectly and structures the query string cleanly
                BString translateUrl = "https://google.com/search?q=translate+";
                translateUrl << selectedText << "+in+" << targetLanguage;
                // =========================================================================
                
                if (cfg.debugEnable) {
                    printf("[DEBUG_TRANS] Final Compiled Web Destination Link: \"%s\"\n", translateUrl.String());
                }

                // Package web link as a port-safe atomic argument pass array
                char* args[] = {
                    (char*)translateUrl.String(),
                    nullptr
                };

                // Command the App Roster to launch default browser cleanly via system mime-type
                status_t err = be_roster->Launch("text/html", 1, args);
                
                if (err != B_OK && err != B_ALREADY_RUNNING) {
                    // Direct Binary Signature lookup fallback if global mapping hits a block
                    entry_ref browserRef;
                    if (be_roster->FindApp("text/html", &browserRef) == B_OK) {
                        err = be_roster->Launch(&browserRef, 1, args);
                    }
                }

                // HIGH-SPEED FALLBACK ENGINE: If Haiku's Roster restricts direct query parameters,
                // this native POSIX shell utility will fire it directly into WebPositive instantly!
                if (err != B_OK && err != B_ALREADY_RUNNING) {
                    BString fallbackShellCmd = BString("open \"") << translateUrl << "\" &";
                    system(fallbackShellCmd.String());
                }
            }
            break;
        }



        // =========================================================================
        // HANDLER FOR CONTEXT ACTION: CHOOSE USER NICKNAME COLOR ('cxcl')
        // =========================================================================
        case MSG_CONTEXT_ADD_COLOR: {
            BString targetNick;
            void* serverPtr = nullptr;
            
            if (message->FindString("target_nick", &targetNick) == B_OK &&
                message->FindPointer("context_server", &serverPtr) == B_OK) {
                
                ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(serverPtr);
                if (serverItem != nullptr && targetNick.Length() > 0) {
                    
                    //  Allocate as a clean local ServerConfigWindow instead of breaking types
                    ServerConfigWindow* configWin = new ServerConfigWindow(this, serverItem, serverItem->GetIndex(), serverItem->IsCustom());
                    configWin->Show();

                    // Pass an explicit data messenger payload to pre-fill the nickname text entry box
                    BMessage fillMsg('fllc'); 
                    fillMsg.AddString("nick", targetNick);
                    configWin->PostMessage(&fillMsg);
                }
            }
            break;
        }



        case MSG_AUTO_REGISTER_FINGERPRINT: {
            BString extractedHash;
            if (message->FindString("fingerprint", &extractedHash) == B_OK && extractedHash.Length() == 40) {
                
                // Ensure we use the current active server node context safely
                if (fCurrentServerNode != nullptr) {
                    SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(fCurrentServerNode)];
                    int activeFd = gServerRawSockets[static_cast<void*>(fCurrentServerNode)];

                    BString registerCommand;
                    registerCommand << "PRIVMSG NickServ :CERT ADD " << extractedHash << "\r\n";

                    if (activeSslHandle != nullptr && activeFd >= 0) {
                        // Transmit completely encrypted down the active thread's session
                        SSL_write(activeSslHandle, registerCommand.String(), registerCommand.Length());
                        
                        if (cfg.debugEnable) {
                            printf("[DEBUG_CERT] Dispatched automated NickServ CERT ADD registration command string down SSL.\n");
                        }
                    } else {
                        // Legacy socket path fallback
                        BSecureSocket* activeSocket = GetActiveSocket(fCurrentServerNode);
                        if (activeSocket != nullptr) {
                            activeSocket->Write(registerCommand.String(), registerCommand.Length());
                        }
                    }

                    BString localLogNotice;
                    localLogNotice << "--- [Auto-CertFP] Automated registration command transmitted: CERT ADD " << extractedHash << "\n";
                    LogToItemBuffer(FindServerLogNode(fCurrentServerNode), localLogNotice);
                }
            }
            break;
        }



        // =========================================================================
        // HANDLER FOR CONTEXT ACTION: MOUSE CLICK CATCH-UP SEARCH
        // =========================================================================
        case MSG_TRIGGER_REPLIES_SEARCH: {
            if (fMyNick.Length() == 0) {
                LogToItemBuffer(fActiveBufferItem, "System Error: Cannot query replies until nickname registration is finalized.\n");
            } else if (fCustomChatLog == nullptr) {
                LogToItemBuffer(fActiveBufferItem, "System Error: Chat view engine canvas is currently unmapped.\n");
            } else {
                BString resultsSummary = "\n--- [Quiet Replies Catch-Up Search Log] ---\n";
                int32 matchCount = 0;
                
                // Read the lines safely right from the active custom draw engine canvas
                int32 totalStoredLines = fCustomChatLog->CountTotalLines(); 
                
                for (int32 i = 0; i < totalStoredLines; ++i) {
                    BString lineText = fCustomChatLog->GetLineTextAt(i);
                    
                    // Perform a case-insensitive check for nickname
                    if (lineText.IFindFirst(fMyNick) != B_ERROR) {
                        resultsSummary << "  " << lineText << "\n";
                        matchCount++;
                    }
                }

                if (matchCount == 0) {
                    resultsSummary << "No background mentions or nickname replies found across active log windows.\n";
                } else {
                    // Isolate the formatting to a temporary string object
                    BString summaryFooter;
                    summaryFooter.SetToFormat("--- Found %" B_PRId32 " total matching unread catch-up threads above ---\n\n", matchCount);
                    
                    // Safely append it to the main block
                    resultsSummary << summaryFooter;
                }


                // Append the output directly to the active channel layout node
                LogToItemBuffer(fActiveBufferItem, resultsSummary);
            }
            break;
        }


        // =========================================================================
        // HANDLER FOR CONTEXT ACTION: ADD NICK TO IGNORE FILTER ENGINE ('igad')
        // =========================================================================
        case 'igad': {
            BString targetNick;
            void* serverPtr = nullptr;
            
            if (message->FindString("target_nick", &targetNick) == B_OK &&
                message->FindPointer("context_server", &serverPtr) == B_OK) {
                
                ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(serverPtr);
                if (serverItem != nullptr && targetNick.Length() > 0) {
                    
                    // 1. Prevent duplicate entries in the live engine vector
                    bool isDuplicate = false;
                    for (const auto& ignored : serverItem->fRuntimeIgnoreList) {
                        if (targetNick.ICompare(ignored.c_str()) == 0) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    
                    if (!isDuplicate) {
                        // 2. Commit the new nickname string to the live runtime engine vector
                        serverItem->fRuntimeIgnoreList.push_back(targetNick.String());
                        
                        // 3. Sync changes straight into back-end persistent config lists
                        std::string srvName = serverItem->Text();
                        bool synced = false;
                        
                        for (auto& srv : cfg.servers) {
                            if (srv.name == srvName) {
                                srv.ignoredNicks.push_back(targetNick.String());
                                synced = true;
                                break;
                            }
                        }
                        if (!synced) {
                            for (auto& srv : cfg.customServers) {
                                if (srv.name == srvName) {
                                    srv.ignoredNicks.push_back(targetNick.String());
                                    break;
                                }
                            }
                        }
                        
                        // 4. Export the updated config file layout directly to JSON settings disk
                        save_config();
                        
                        // 5. Output a visual verification log line to the status console
                        BString feedback;
                        feedback.SetToFormat("--- [Filters] User '%s' is now being ignored on this network.\n", targetNick.String());
                        LogToItemBuffer(FindServerLogNode(serverItem), feedback);
                    }
                }
            }
            break;
        }

        // =========================================================================
        // HANDLER FOR CONTEXT ACTION: REMOVE NICK FROM IGNORE FILTER ENGINE ('igrm')
        // =========================================================================
        case 'igrm': {
            BString targetNick;
            void* serverPtr = nullptr;
            
            if (message->FindString("target_nick", &targetNick) == B_OK &&
                message->FindPointer("context_server", &serverPtr) == B_OK) {
                
                ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(serverPtr);
                if (serverItem != nullptr && targetNick.Length() > 0) {
                    
                    // 1. Prune the rule completely from active runtime engine vector
                    std::string targetStr(targetNick.String());
                    serverItem->fRuntimeIgnoreList.erase(
                        std::remove_if(serverItem->fRuntimeIgnoreList.begin(), serverItem->fRuntimeIgnoreList.end(),
                            [&](const std::string& item) { return targetNick.ICompare(item.c_str()) == 0; }),
                        serverItem->fRuntimeIgnoreList.end()
                    );
                    
                    // 2. Prune the rule from back-end core config data structures
                    std::string srvName = serverItem->Text();
                    bool synced = false;
                    
                    for (auto& srv : cfg.servers) {
                        if (srv.name == srvName) {
                            srv.ignoredNicks.erase(
                                std::remove_if(srv.ignoredNicks.begin(), srv.ignoredNicks.end(),
                                    [&](const std::string& item) { return targetNick.ICompare(item.c_str()) == 0; }),
                                srv.ignoredNicks.end()
                            );
                            synced = true;
                            break;
                        }
                    }
                    if (!synced) {
                        for (auto& srv : cfg.customServers) {
                            if (srv.name == srvName) {
                                srv.ignoredNicks.erase(
                                    std::remove_if(srv.ignoredNicks.begin(), srv.ignoredNicks.end(),
                                        [&](const std::string& item) { return targetNick.ICompare(item.c_str()) == 0; }),
                                    srv.ignoredNicks.end()
                                );
                                break;
                            }
                        }
                    }
                    
                    // 3. Export the clean profile file out to JSON storage system
                    save_config();
                    
                    // 4. Output a visual removal verification line directly to the status console
                    BString feedback;
                    feedback.SetToFormat("--- [Filters] User '%s' has been unignored on this network.\n", targetNick.String());
                    LogToItemBuffer(FindServerLogNode(serverItem), feedback);
                }
            }
            break;
        }




        // =========================================================================
        // UPDATED: HANDLING BALERT BUTTON SELECTION RESPONSE PIPELINE
        // =========================================================================
        case 'hknc': {
            int32 buttonIndex;
            // Haiku BAlert: Index 0 is "Ignore", Index 1 is "Invite User"
            if (message->FindInt32("which", &buttonIndex) == B_OK && buttonIndex == 1) {
                
                BString knocker = message->FindString("knocker");
                BString channel = message->FindString("channel");
                void* serverPtr = nullptr;
                
                if (message->FindPointer("server_context", &serverPtr) == B_OK && serverPtr != nullptr) {
                    // Properly cast the pointer so we can access class methods like ->Text()
                    ServerTreeItem* serverCtx = static_cast<ServerTreeItem*>(serverPtr);
                    
                    // Lookup the live secure pipe handle using the context pointer key
                    SSL* activeSslHandle = gServerSslHandles[serverPtr];
                    
                    // Verify the OpenSSL handle and dispatch the expanded invitation payload
                    if (activeSslHandle != nullptr && knocker.Length() > 0 && channel.Length() > 0) {
                        BString rawInvite;
                        rawInvite << "INVITE " << knocker << " " << channel << "\r\n";
                        
                        // Secure transmission
                        SSL_write(activeSslHandle, rawInvite.String(), rawInvite.Length());
                        
                        if (cfg.debugEnable) {
                            // FIXED: Corrected the printf syntax and added all details
                            printf("[DEBUG POPUP] Invitation dispatched -> User: %s | Channel: %s | Network: %s\n", 
                                   knocker.String(), channel.String(), serverCtx->Text());
                        }

                        // Local visual log update so you see it in your current channel buffer
                        BString feedback;
                        feedback.SetToFormat("--> System: Sending invitation for %s to join %s on %s\n", 
                                             knocker.String(), channel.String(), serverCtx->Text());
                        LogToItemBuffer(fActiveBufferItem, feedback);

                    } else {
                        if (cfg.debugEnable) {
                            printf("[DEBUG POPUP] Error: Invitation failed. Missing handle or invalid parameters.\n");
                        }
                    }
                }
            }
            break;
        }




		case 'rgmd': // Register Modes Dialog
            message->FindPointer("dialog_ptr", (void**)&fActiveModesDialog);
            message->FindString("channel_target", &fActiveModesChannel);
            break;

        case 'unmd': // Unregister Modes Dialog
            fActiveModesDialog = nullptr;
            fActiveModesChannel = "";
            break;



        case MSG_CONTEXT_SHOW_MODES: {
            ChannelTreeItem* chanItem = nullptr;
            if (message->FindPointer("chan_item", (void**)&chanItem) == B_OK && chanItem != nullptr) {
                
                BListItem* parentItem = fChannelTree->Superitem(chanItem);
                ServerTreeItem* contextServer = dynamic_cast<ServerTreeItem*>(parentItem);
                
                if (contextServer != nullptr) {
                    // ==========================================================
                    // 1. Thread-Safe OpenSSL Mapping Verification
                    // ==========================================================
                    bool serverStillConnected = false;
                    Lock();
                    if (gServerSslHandles.find(static_cast<void*>(contextServer)) != gServerSslHandles.end()) {
                        serverStillConnected = true;
                    }
                    Unlock();

                    if (serverStillConnected) {
                        // Extract clean channel name (stripping [users] tag)
                        BString cleanChanName(chanItem->Text());
                        int32 tagPos = cleanChanName.FindFirst(" [");
                        if (tagPos != B_ERROR) {
                            cleanChanName.Truncate(tagPos);
                        }
                        cleanChanName.Trim();

                        // Register global tracking variables for the live parser
                        this->fActiveModesChannel = cleanChanName;

                        // ==========================================================
                        // 2. Launch Dialog (Simplified Status)
                        // ==========================================================
                        // Passing 'true' ensures the dialog opens with controls enabled
                        // so the server query can actually populate the UI.
                        ChannelModesDialog* modesDialog = new ChannelModesDialog(this, contextServer, chanItem->Text(), true);
                        
                        this->fActiveModesDialog = modesDialog;
                        modesDialog->Show();
                    } else {
                        BString warning;
                        warning.SetToFormat("System Error: Server '%s' is disconnected. Cannot view channel modes.\n", 
                                            contextServer->Text());
                        LogToItemBuffer(fActiveBufferItem, warning);
                    }
                }
            }
            break;
        }






        case 'tgem': {
            // 1. Recover the pointer to the target server item
            ServerTreeItem* srvItem = nullptr;
            if (message->FindPointer("server_item", (void**)&srvItem) == B_OK && srvItem != nullptr) {
                
                // 2. Locate the matching server struct inside config mapping
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
                
                // 3. Spin up the popup passing ONLY the messenger target thread handle.
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
                    
                    // Safe Haiku cursor tracking using the standard GetSelection method parameters
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
                    
                    // =========================================================================
                    // TRACE NETWORK via EXPLICIT TARGETED CHANNEL MATCH
                    // =========================================================================
                    // Instead of risking a race condition with fCurrentServerNode, look through
                    // the dynamic tree hierarchy to discover which server owns this channel.
                    ServerTreeItem* contextServer = nullptr;
                    
                    if (fChannelTree != nullptr) {
                        for (int32 idx = 0; idx < fChannelTree->CountItems(); idx++) {
                            ChannelTreeItem* item = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemAt(idx));
                            if (item != nullptr) {
                                BString itemChanName(item->Text());
                                int32 spaceIdx = itemChanName.FindLast(" ");
                                if (spaceIdx != B_ERROR) {
                                    itemChanName.Truncate(spaceIdx);
                                }
                                int32 tagPos = itemChanName.FindFirst(" [");
                                if (tagPos != B_ERROR) {
                                    itemChanName.Truncate(tagPos);
                                }

                                if (targetChannel.ICompare(itemChanName) == 0) {
                                    contextServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(item));
                                    break;
                                }
                            }
                        }
                    }

                    // Fallback safe assignment to current if tree scanning turned up empty
                    if (contextServer == nullptr) {
                        contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                    }

                    if (contextServer != nullptr) {
                        // =========================================================================
                        // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                        // =========================================================================
                        SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];

                        if (activeSslHandle != nullptr) {
                            BString outgoingPayload;
                            outgoingPayload << "MODE " << targetChannel << " -b " << selectedMask << "\r\n";
                            
                            // Transmit securely using OpenSSL write instead of raw socket write
                            SSL_write(activeSslHandle, outgoingPayload.String(), outgoingPayload.Length());
                            
                            // Close window frame cleanly
                            modalWindow->PostMessage(B_QUIT_REQUESTED);
                        }
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
                // Use static_cast instead of dynamic_cast to match tree's node guarantees
                targetedChanNode = static_cast<ChannelTreeItem*>(fActiveBufferItem);
            }
            
            if (targetedChanNode == nullptr) break;

            // =========================================================================
            //  TRACE SERVER AND CLEAN CHANNEL STRINGS VIA HIERARCHY
            // =========================================================================
            ServerTreeItem* contextServer = nullptr;
            if (fChannelTree != nullptr) {
                contextServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(targetedChanNode));
            }
            if (contextServer == nullptr) break;

            // Clean tracking tags from channel name safely
            BString activeChannel(targetedChanNode->Text());
            int32 spaceIdx = activeChannel.FindLast(" ");
            if (spaceIdx != B_ERROR) {
                BString extractedChannel;
                activeChannel.CopyInto(extractedChannel, spaceIdx + 1, activeChannel.Length() - spaceIdx);
                activeChannel = extractedChannel;
            }
            int32 tagPos = activeChannel.FindFirst(" [");
            if (tagPos != B_ERROR) {
                activeChannel.Truncate(tagPos);
            }

            if (!activeChannel.StartsWith("#") && !activeChannel.StartsWith("&") && !activeChannel.StartsWith("!")) break;

            // =========================================================================
            // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
            // =========================================================================
            SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];

            if (activeSslHandle != nullptr) {
                BString outgoingPayload;
                outgoingPayload << "MODE " << activeChannel << " b\r\n";
                
                // Transmit using OpenSSL write instead of raw socket write
                SSL_write(activeSslHandle, outgoingPayload.String(), outgoingPayload.Length());
            }
            break;
        }





       // CHANNEL MODE EXECUTOR: Dispatches mode modifications (-o, +v, -v) to the server
        case MSG_CONTEXT_DEOP:
        case MSG_CONTEXT_VOICE:
        case MSG_CONTEXT_DEVOICE: {
            BString targetNick;
            if (message->FindString("target_nick", &targetNick) == B_OK && targetNick.Length() > 0) {
                
                // Ensure an active chat buffer room is actually targeted
                if (fActiveBufferItem == nullptr) break;

                // =========================================================================
                //  TRACE SERVER AND CLEAN CHANNEL STRINGS VIA HIERARCHY
                // =========================================================================
                ServerTreeItem* contextServer = nullptr;
                if (fChannelTree != nullptr) {
                    contextServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(fActiveBufferItem));
                }
                if (contextServer == nullptr) break;

                // Clean tracking tags from channel name safely
                BString activeChannel(fActiveBufferItem->Text());
                int32 spaceIdx = activeChannel.FindLast(" ");
                if (spaceIdx != B_ERROR) {
                    BString extractedChannel;
                    activeChannel.CopyInto(extractedChannel, spaceIdx + 1, activeChannel.Length() - spaceIdx);
                    activeChannel = extractedChannel;
                }
                int32 tagPos = activeChannel.FindFirst(" [");
                if (tagPos != B_ERROR) {
                    activeChannel.Truncate(tagPos);
                }

                if (!activeChannel.StartsWith("#") && !activeChannel.StartsWith("&") && !activeChannel.StartsWith("!")) break; 

                BString flag;
                switch (message->what) {
                    case MSG_CONTEXT_DEOP:    flag = "-o"; break;
                    case MSG_CONTEXT_VOICE:   flag = "+v"; break;
                    case MSG_CONTEXT_DEVOICE: flag = "-v"; break;
                    default:                  break; // Guards unexpected fallthroughs safely
                }
                if (flag.Length() == 0) break;

                // =========================================================================
                // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                // =========================================================================
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];

                if (activeSslHandle != nullptr) {
                    BString outgoingPayload;
                    outgoingPayload << "MODE " << activeChannel << " " << flag << " " << targetNick << "\r\n";
                    
                    // Transmit using OpenSSL write instead of raw socket write
                    SSL_write(activeSslHandle, outgoingPayload.String(), outgoingPayload.Length());
                }
            }
            break;
        }


        
 
 
          case MSG_CONTEXT_OP: {
            BString targetNick;
            if (message->FindString("target_nick", &targetNick) == B_OK && targetNick.Length() > 0) {
                
                // 1. Resolve channel and server context
                ChannelTreeItem* chanItem = static_cast<ChannelTreeItem*>(fActiveBufferItem);
                if (chanItem == nullptr) break;

                BListItem* parentItem = fChannelTree->Superitem(chanItem);
                ServerTreeItem* contextServer = dynamic_cast<ServerTreeItem*>(parentItem);
                if (contextServer == nullptr) break;

                // 2. USE WORKING LOGIC: Determine if YOU are an operator in this channel
                bool iamOperator = false;
                if (fChannelUsers.count(chanItem) > 0) {
                    BObjectList<UserListItem, true>* userList = fChannelUsers[chanItem];
                    if (userList != nullptr) {
                        for (int32 i = 0; i < userList->CountItems(); i++) {
                            UserListItem* user = userList->ItemAt(i);
                            if (user != nullptr && user->Text() != nullptr) {
                                BString userText(user->Text());
                                BString cleanUserNick = GetCleanNickname(userText);
                                
                                // Dynamic nickname comparison (works for guest or real nicks)
                                if (fMyNick.ICompare(cleanUserNick) == 0) {
                                    if (userText.StartsWith("@")) {
                                        iamOperator = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }

                // 3. Security Guard: Warn if attempting to OP without permission
                if (!iamOperator) {
                    LogToItemBuffer(chanItem, "System Warning: You must be an Operator (@) to grant Op status.\n");
                    break; 
                }

                // 4. Open the Duration Window passing the context server for OpenSSL lookup later
                BRect windowFrame(0, 0, 400, 140);
                OpDurationWindow* inputWin = new OpDurationWindow(windowFrame, "Give Operator Status", 
                                                                  targetNick, this, chanItem, contextServer);
                inputWin->Show();
            }
            break;
        }






         case MSG_CONTEXT_OP_SUBMIT: {
            BString targetNick;
            BString opDuration = "Permanently";
            
            if (message->FindString("target_nick", &targetNick) == B_OK) {
                message->FindString("op_duration", &opDuration);

                // DYNAMIC VERIFICATION: Pull the context item straight out of the message bundle.
                ChannelTreeItem* targetItem = nullptr;
                if (message->FindPointer("channel_item", (void**)&targetItem) != B_OK || targetItem == nullptr) {
                    targetItem = static_cast<ChannelTreeItem*>(fActiveBufferItem);
                }

                if (targetItem == nullptr) break;

                // Trace the true server instance using the packed payload pointer first, falling back to tree layout
                ServerTreeItem* contextServer = nullptr;
                if (message->FindPointer("server_context", (void**)&contextServer) != B_OK || contextServer == nullptr) {
                    if (fChannelTree != nullptr) {
                        contextServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(targetItem));
                    }
                }
                
                if (contextServer == nullptr) break;

                // Extract the clean network channel room string
                BString activeChannel(targetItem->Text());
                
                int32 spaceIdx = activeChannel.FindLast(" ");
                if (spaceIdx != B_ERROR) {
                    BString extractedChannel;
                    activeChannel.CopyInto(extractedChannel, spaceIdx + 1, activeChannel.Length() - spaceIdx);
                    activeChannel = extractedChannel;
                }
                
                int32 tagPos = activeChannel.FindFirst(" [");
                if (tagPos != B_ERROR) {
                    activeChannel.Truncate(tagPos);
                }

                // Check prefixes
                if (!activeChannel.StartsWith("#") && !activeChannel.StartsWith("&") && !activeChannel.StartsWith("!")) {
                    break;
                }

                // 1. Lookup our active secure handle using the context key
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                
                // 2. Verify connection viability
                if (activeSslHandle != nullptr) {
                    BString outgoingPayload;
                    outgoingPayload << "MODE " << activeChannel << " +o " << targetNick << "\r\n";
                    
                    // 3. Transmit using OpenSSL
                    SSL_write(activeSslHandle, outgoingPayload.String(), outgoingPayload.Length());

                    bigtime_t delayMicroseconds = 0;
                    if (opDuration == "5 Minutes") delayMicroseconds = 5LL * 60LL * 1000000LL;
                    else if (opDuration == "1 Hour") delayMicroseconds = 60LL * 60LL * 1000000LL;
                    else if (opDuration == "1 Day")  delayMicroseconds = 24LL * 60LL * 60LL * 1000000LL;

                    if (delayMicroseconds > 0) {
                        BString* trackedNick = new BString(targetNick);
                        fAutoOpList.AddItem(trackedNick);

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
                
                // =========================================================================
                // CRASH PROTECTION PASS: VERIFY THE SERVER NODE STILL EXISTS IN MEMORY
                // =========================================================================
                // Check our active network OpenSSL handle map to ensure this server pointer wasn't 
                // deleted or freed due to a user disconnection while the timer was running.
                bool serverStillExists = false;
                
                Lock(); // Keep map exploration thread-safe
                if (gServerSslHandles.find(static_cast<void*>(targetServer)) != gServerSslHandles.end()) {
                    serverStillExists = true;
                }
                Unlock();

                if (!serverStillExists) {
                    if (cfg.debugEnable) {
                        printf("[DEBUG] Timed de-op aborted. Target server node was destroyed before timer expired.\n");
                    }
                    break; 
                }

                // 1. Lookup our active secure handle using the validated context key
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(targetServer)];
                
                // 2. Verify connection viability and transmit over OpenSSL infrastructure
                if (activeSslHandle != nullptr) {
                    BString stripPayload;
                    stripPayload << "MODE " << targetChannel << " -o " << targetNick << "\r\n";
                    
                    // 3. Secure transmission
                    SSL_write(activeSslHandle, stripPayload.String(), stripPayload.Length());
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
            // DIAGNOSTIC MONITOR PASS: Log the initiation of the submit event handler loop
            if (cfg.debugEnable) {
                printf("[DEBUG_KICK_SUBMIT] MSG_CONTEXT_KICK_SUBMIT triggered.\n");
            }

            BString targetNick;
            BString kickReason = "Requested via context menu";
            bool shouldBan = false;
            BString banDuration = "Forever";
            
            if (message->FindString("target_nick", &targetNick) == B_OK) {
                void* winPtr = nullptr;
                
                if (message->FindPointer("window_ref", &winPtr) == B_OK && winPtr != nullptr) {
                    BWindow* modalWindow = static_cast<BWindow*>(winPtr);
                    
                    if (modalWindow->Lock()) {
                        // 1. Extract the text reason string safely into local deep-copied memory
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

                // =========================================================================
                // ANTI-RACE CONTEXT MATCHING LAYER: Extract verified parent pointers
                // =========================================================================
                // Attempt to unpack a clean pointer attached dynamically by the opening trigger
                ServerTreeItem* contextServer = nullptr;
                if (message->FindPointer("server_context", (void**)&contextServer) != B_OK || contextServer == nullptr) {
                    // Fallback to active tree node elements only if the message pack is missing it
                    contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                }
                
                if (contextServer == nullptr) break;

                if (fActiveBufferItem == nullptr) break;
                BString activeChannel = fActiveBufferItem->Text();

                // Clean tracking parameters and user counts off the channel target room string safely
                int32 spaceIdx = activeChannel.FindLast(" ");
                if (spaceIdx != B_ERROR) {
                    BString extractedChannel;
                    activeChannel.CopyInto(extractedChannel, spaceIdx + 1, activeChannel.Length() - spaceIdx);
                    activeChannel = extractedChannel;
                }
                int32 tagPos = activeChannel.FindFirst(" [");
                if (tagPos != B_ERROR) {
                    activeChannel.Truncate(tagPos);
                }

                if (!activeChannel.StartsWith("#") && !activeChannel.StartsWith("&") && !activeChannel.StartsWith("!")) break;

                // =========================================================================
                // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                // =========================================================================
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                
                if (activeSslHandle != nullptr) {
                    BString outgoingPayload;

                    // ==================== LIVE BAN PROCESSING ====================
                    if (shouldBan) {
                        outgoingPayload << "MODE " << activeChannel << " +b " << targetNick << "!*@*\r\n";
                        kickReason << " [Banned: " << banDuration << "]";
                    }

                    // Append the standard KICK instruction payload string
                    outgoingPayload << "KICK " << activeChannel << " " << targetNick << " :" << kickReason << "\r\n";
                    
                    if (cfg.debugEnable) {
                        printf("[DEBUG_KICK_SUBMIT] Writing out raw encrypted OpenSSL payload lines:\n%s", outgoingPayload.String());
                    }

                    // Transmit cleanly over the wire using OpenSSL write instead of raw socket write
                    SSL_write(activeSslHandle, outgoingPayload.String(), outgoingPayload.Length());
                } else {
                    if (cfg.debugEnable) {
                        printf("[DEBUG_KICK_SUBMIT] Error: Found null or dead activeSslHandle mapping context token.\n");
                    }
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
            
            // Revert back to direct mouse polling checks to eliminate message filter interference
            fUserList->GetMouse(&mousePoint, &mouseBtn);
            
            // Only continue if the secondary mouse button (right click) is actively held down
            if ((mouseBtn & B_SECONDARY_MOUSE_BUTTON) == 0) {
                break;
            }
            
            // Find the item directly under the translated coordinates
            int32 clickedIdx = fUserList->IndexOf(mousePoint);
            if (clickedIdx < 0) break;

            // Visual Polish: Force the visual selection matrix to snap instantly to our target
            if (fUserList->CurrentSelection() != clickedIdx) {
                fUserList->Select(clickedIdx);
            }

            UserListItem* clickedItem = dynamic_cast<UserListItem*>(fUserList->ItemAt(clickedIdx));
            if (clickedItem == nullptr) break;

            BString cleanNick = GetCleanNickname(clickedItem->Text());
            BPopUpMenu* contextMenu = new BPopUpMenu("UserContext", false, false);

            // CONDITION A: You right-clicked OWN nickname handle
            if (cleanNick == fMyNick) {
                const char* statusText = clickedItem->IsAway() ? "Set Status: Back" : "Set Status: Away";
                contextMenu->AddItem(new BMenuItem(statusText, new BMessage(MSG_CONTEXT_SET_AWAY)));
            } 
            // CONDITION B: You right-clicked SOMEONE ELSE's nickname handle
            else {
                BString menuLabel;
                menuLabel << "Private Message " << cleanNick;
                
                BMessage* pmMsg = new BMessage(MSG_CONTEXT_PRIVMSG);
                pmMsg->AddString("target_nick", cleanNick);
                contextMenu->AddItem(new BMenuItem(menuLabel.String(), pmMsg));
                
                // --- NEW: DYNAMIC IGNORE / UNIGNORE INJECTION ---
                ServerTreeItem* parentServer = nullptr;
                ChannelTreeItem* activeChan = dynamic_cast<ChannelTreeItem*>(fActiveBufferItem);
                
                // Backtrack from the active channel row to find its parent server context node
                if (activeChan != nullptr && fChannelTree != nullptr) {
                    BListItem* parentItem = fChannelTree->Superitem(activeChan);
                    if (parentItem != nullptr) {
                        parentServer = dynamic_cast<ServerTreeItem*>(parentItem);
                    }
                }

                if (parentServer != nullptr) {
                    bool isIgnored = false;
                    for (const auto& ignoredNick : parentServer->fRuntimeIgnoreList) {
                        if (cleanNick.ICompare(ignoredNick.c_str()) == 0) {
                            isIgnored = true;
                            break;
                        }
                    }

                    // --- NEW: INJECT NICKNAME COLOR ASSIGNMENT RIGHT ABOVE IGNORE ---
                    BString colorLabel;
                    colorLabel << "Add Color to " << cleanNick;
                    BMessage* colorMsg = new BMessage(MSG_CONTEXT_ADD_COLOR);
                    colorMsg->AddString("target_nick", cleanNick);
                    colorMsg->AddPointer("context_server", parentServer);
                    contextMenu->AddItem(new BMenuItem(colorLabel.String(), colorMsg));
                    // -----------------------------------------------------------------

                    BString ignoreLabel;
                    BMessage* ignoreMsg = nullptr;

                    if (isIgnored) {
                        ignoreLabel << "Unignore " << cleanNick;
                        ignoreMsg = new BMessage('igrm'); 
                    } else {
                        ignoreLabel << "Ignore " << cleanNick;
                        ignoreMsg = new BMessage('igad'); 
                    }

                    ignoreMsg->AddString("target_nick", cleanNick);
                    ignoreMsg->AddPointer("context_server", parentServer); 
                    contextMenu->AddItem(new BMenuItem(ignoreLabel.String(), ignoreMsg));
                }

                
                // DYNAMIC VISIBILITY: Scan the channel cache to see if WE hold operator status (@)
                bool iamOperator = false;
                
                if (activeChan != nullptr && fChannelUsers.count(activeChan) > 0) {
                    BObjectList<UserListItem, true>* userList = fChannelUsers[activeChan];
                    if (userList != nullptr) {
                        for (int32 i = 0; i < userList->CountItems(); i++) {
                            UserListItem* user = userList->ItemAt(i);
                            if (user != nullptr && user->Text() != nullptr) {
                                BString userText(user->Text());
                                BString cleanUserNick = GetCleanNickname(userText);
                                
                                // Check if this specific row represents nickname profile
                                if (fMyNick.ICompare(cleanUserNick) == 0) {
                                    if (userText.StartsWith("@")) {
                                        iamOperator = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }

                // Only generate and inject the Channel Modes submenu option if user is an Operator
                if (iamOperator) {
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
            }

            contextMenu->SetTargetForItems(this);
            
            // Open the menu asynchronously so it doesn't block the app message loop
            contextMenu->Go(fUserList->ConvertToScreen(mousePoint), true, true, true);
            break;
        }








         // 2. EXECUTE SERVER TRANSMISSION: Dispatches the corresponding raw /AWAY parameters
        case MSG_CONTEXT_SET_AWAY: {
            if (fActiveBufferItem != nullptr) {
                // Determine active server context safely
                ServerTreeItem* targetedServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                if (targetedServer == nullptr) break;

                // =========================================================================
                // DYNAMIC NICKNAME RESOLUTION FIX
                // =========================================================================
                // Extract your true runtime server name dynamically from the tab text
                BString activeMyNick = targetedServer->Text();
                int32 spacePos = activeMyNick.FindFirst(" ");
                if (spacePos != B_ERROR) activeMyNick.Truncate(spacePos);
                activeMyNick.Trim();

                // Lookup secure pipe handle via tracking maps
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(targetedServer)];

                if (activeSslHandle != nullptr) {
                    BString outgoingPayload;
                    bool currentlyAway = false;

                    // Scan user items using the dynamic runtime nickname
                    for (int32 i = 0; i < fUserList->CountItems(); i++) {
                        UserListItem* uiUser = dynamic_cast<UserListItem*>(fUserList->ItemAt(i));
                        if (uiUser != nullptr) {
                            // FIXED: Now uses activeMyNick instead of the static fMyNick
                            if (uiUser->GetCleanNick() == activeMyNick) {
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
                    
                    // Secure OpenSSL transmission
                    SSL_write(activeSslHandle, outgoingPayload.String(), outgoingPayload.Length());
                }
            }
            break;
        }






        
            case MSG_TOPIC_CHANGED: {
            if (fActiveBufferItem != nullptr) {
                // Strip activity tag markers out of the channel text if present before checking prefix notation
                BString channelName = fActiveBufferItem->Text();
                int32 tagPos = channelName.FindFirst(" [");
                if (tagPos != B_ERROR) channelName.Truncate(tagPos);

                if (channelName.StartsWith("#") || channelName.StartsWith("&")) {
                    BString newlyTypedTopic = fTopicView->Text();
                    newlyTypedTopic.Trim();

                    if (newlyTypedTopic.Length() > 0) {
                        // 1. MULTI-SERVER: Climb the tree hierarchy safely to find the 
                        // exact parent server node that owns this specific channel.
                        ServerTreeItem* targetedServer = nullptr;
                        BListItem* parentItem = fChannelTree->Superitem(fActiveBufferItem);
                        
                        if (parentItem != nullptr) {
                            targetedServer = dynamic_cast<ServerTreeItem*>(parentItem);
                        }

                        // Fallback tracking point if the hierarchy traversal is unmapped
                        if (targetedServer == nullptr) {
                            targetedServer = fCurrentServerNode;
                        }

                        // =========================================================================
                        // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                        // =========================================================================
                        SSL* activeSslHandle = nullptr;
                        if (targetedServer != nullptr) {
                            activeSslHandle = gServerSslHandles[static_cast<void*>(targetedServer)];
                        }

                        // 3. Dispatch the payload instructions securely over the wire
                        if (activeSslHandle != nullptr) {
                            BString topicCommand;
                            topicCommand << "TOPIC " << channelName << " :" << newlyTypedTopic << "\r\n";
                            
                            // Secure OpenSSL transmission instead of raw socket write
                            SSL_write(activeSslHandle, topicCommand.String(), topicCommand.Length());
                        } else {
                            BString warning;
                            warning.SetToFormat("System Error: Cannot update topic. The server context is disconnected.\n");
                            LogToItemBuffer(fActiveBufferItem, warning);
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
                
                // Enforce proper thread-safe local window lock boundaries
                Lock();
                if (fServerSockets.count(srvItem) > 0 && fServerSockets[srvItem] != nullptr) {
                    
                    // 1. Send an optional quick QUIT message down the live OpenSSL context if available
                    SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(srvItem)];
                    if (activeSslHandle != nullptr) {
                        BString quitPayload = "QUIT :Disconnecting from client\r\n";
                        SSL_write(activeSslHandle, quitPayload.String(), quitPayload.Length());
                    } else {
                        BString quitPayload = "QUIT :Disconnecting from client\r\n";
                        fServerSockets[srvItem]->Write(quitPayload.String(), quitPayload.Length());
                    }
                    
                    // 2. Forcefully close the network pipe descriptor to wake up background loops
                    fServerSockets[srvItem]->Disconnect();
                    
                    // =========================================================================
                    //  INSTANT COMPREHENSIVE MAP PURGE (PREVENTS RACING CONTEXT MENUS)
                    // =========================================================================
                    // Erasing these pointers here guarantees the context menu updates instantly!
                    fServerSockets.erase(srvItem);
                    gServerSslHandles.erase(static_cast<void*>(srvItem));
                    gServerRawSockets.erase(static_cast<void*>(srvItem));
                    
                    // Sync legacy stock wrapper pointer alignments
                    if (srvItem == fLiberaNode) fLiberaSocket = nullptr;
                    if (srvItem == fOftcNode)   fOftcSocket = nullptr;
                    // =========================================================================
                    
                    // 3. Print a clean disconnection status notice locally
                    BStringItem* serverLog = FindServerLogNode(srvItem);
                    if (serverLog != nullptr) {
                        LogToItemBuffer(serverLog, "--- Disconnected from server by user choice.\n");
                    } else {
                        LogToItemBuffer(static_cast<BStringItem*>(srvItem), "--- Disconnected from server by user choice.\n");
                    }

                    // 4. Recalibrate Selected Config Index Mappings
                    size_t srvConfigIndex = srvItem->GetIndex();
                    if (srvItem->IsCustom()) {
                        selectedConfig = (int)(cfg.servers.size() + srvConfigIndex);
                    } else {
                        selectedConfig = (int)srvConfigIndex;
                    }

                    // Force an immediate visual refresh pass on sidebar panel widgets
                    if (fChannelTree != nullptr) {
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(srvItem));
                        fChannelTree->Invalidate();
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
                
                // =========================================================================
                // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                // =========================================================================
                SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(srvItem)];
                int rawSocketFd = (gServerRawSockets.count(static_cast<void*>(srvItem)) > 0) ? gServerRawSockets[static_cast<void*>(srvItem)] : -1;
                
                if (activeSslHandle != nullptr) {
                    BString quitPayload;
                    quitPayload << "QUIT :Server entry deleted by user\r\n";
                    
                    // Transmit encrypted QUIT payload before teardown
                    SSL_write(activeSslHandle, quitPayload.String(), quitPayload.Length());
                    
                    // Cleanly close the TLS/SSL layer connection protocol
                    SSL_shutdown(activeSslHandle);
                    SSL_free(activeSslHandle);
                }

                // If a raw underlying file descriptor exists, close it down at the Haiku kernel layer
                if (rawSocketFd >= 0) {
                    close(rawSocketFd);
                }

                // Clean the modern internal tracking keys immediately to prevent reuse conflicts
                fServerThreads.erase(srvItem);
                gServerSslHandles.erase(static_cast<void*>(srvItem));
                gServerRawSockets.erase(static_cast<void*>(srvItem));
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
        
        // 1. Clean dynamic tracking tags (like) from raw labels
        BString targetChanName(chanItem->Text());
        int32 tagPos = targetChanName.FindFirst(" [");
        if (tagPos != B_ERROR) {
            targetChanName.Truncate(tagPos);
        }
        targetChanName.Trim();

        // ====================================================================
        // FIXED: ROBUST CHANNEL PREFIX CHECK (Supports ##trivia, &channels, etc)
        // ====================================================================
        // IRC Standards (RFC 2811) allow #, &, +, and !. We check the first char.
        bool isActualIrcChannel = false;
        if (targetChanName.Length() > 0) {
            char c = targetChanName.ByteAt(0);
            if (c == '#' || c == '&' || c == '!' || c == '+') {
                isActualIrcChannel = true;
            }
        }

        ServerTreeItem* parentServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(chanItem));
        
        if (parentServer != nullptr && isActualIrcChannel) {
            // =========================================================================
            // FIXED: SECURE OPENSSL TRANSMITTER FOR PART COMMANDS
            // =========================================================================
            SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(parentServer)];
            int activeFd = gServerRawSockets[static_cast<void*>(parentServer)];

            BString partCmd;
            partCmd << "PART " << targetChanName << " :Channel removed by user\r\n";

            if (activeSslHandle != nullptr && activeFd >= 0) {
                // Secure path down the live OpenSSL context pipe
                SSL_write(activeSslHandle, partCmd.String(), partCmd.Length());
            } else {
                // Fallback for unmanaged sockets
                BSecureSocket* activeSocket = GetActiveSocket(parentServer);
                if (activeSocket != nullptr) {
                    activeSocket->Write(partCmd.String(), partCmd.Length());
                }
            }
            
            // 2. Remove from autojoin configuration
            size_t serverIdx = parentServer->GetIndex();
            auto& srvRef = parentServer->IsCustom() ? cfg.customServers[serverIdx] : cfg.servers[serverIdx];
            
            for (auto it = srvRef.autojoin.begin(); it != srvRef.autojoin.end(); ) {
                if (targetChanName.ICompare(it->c_str()) == 0) {
                    it = srvRef.autojoin.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // 3. UI Cleanup
        if (fActiveBufferItem == chanItem) {
            fActiveBufferItem = nullptr;
            fChatLog->SetText("");
            if (fCustomChatLog) {
                fCustomChatLog->SetActiveChannel(nullptr);
                fCustomChatLog->ClearAllLines();
            }
            // Use RemoveItem only to avoid double-free crashes
            while (fUserList->CountItems() > 0) {
                fUserList->RemoveItem((int32)0);
            }
            fTopicView->SetText("No active channel buffer targeted.");
        }
        
        // 4. Memory Purge
        fTextBuffers.erase(chanItem);
        fChannelTopics.erase(chanItem);
        if (fChannelUsers.count(chanItem) > 0) {
            delete fChannelUsers[chanItem];
            fChannelUsers.erase(chanItem);
        }

        // 5. Drop from Tree
        fChannelTree->RemoveItem(chanItem);
        delete chanItem; 
        
        if (fActiveBufferItem == nullptr && fChannelTree->CountItems() > 0) {
            fChannelTree->Select(0);
        }

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
                // Keep existing layout/height updates intact
                item->Update(fChannelTree, &treeFont);

                // ---  LIVE [A] AUTOJOIN FLAG RECALCULATION ROUTINE ---
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(item);
                if (chanNode != nullptr && activeSrv != nullptr) {
                    ServerTreeItem* parentSrv = dynamic_cast<ServerTreeItem*>(fChannelTree->Superitem(chanNode));
                    if (parentSrv != nullptr && BString(parentSrv->Text()) == activeSrv->name.c_str()) {
                        
                        // Extracts the pure channel name from Text() since [A] is only added during rendering
                        BString chanName = chanNode->Text(); 
                        bool currentlyInAutojoin = false;

                        for (const auto& aChan : activeSrv->autojoin) {
                            if (chanName.ICompare(aChan.c_str()) == 0) {
                                currentlyInAutojoin = true;
                                break;
                            }
                        }

                        // Update the internal tracker flag using existing public setter: SetAutoJoin()
                        chanNode->SetAutoJoin(currentlyInAutojoin);
                        fChannelTree->InvalidateItem(i); // Force Haiku to repaint this specific row
                    }
                }
                // ----------------============================== ---
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
                
                // 2. Also clear out channel history user topics cache maps if tracked here
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
                        
                        // --- DEFENSIVE ENGINE RESET ---
                        // Force volatile layout indices back to safe default zones 
                        // so incoming packets from a dying thread can't write to wrong active buffers
                        fHistoryIndex = 0; 
                        fActiveBufferItem = FindServerLogNode(serverItem); 
                        
                        // =========================================================================
                        //  RECONNECTION HANDSHAKE MATRIX RESETTER
                        // =========================================================================
                        // Clears out stale parameters from the old session so the incoming 
                        // connection passes through the capabilities loop seamlessly!
                        serverItem->fHasFinalizedCap = false;          // Unlocks CAP engine gate
                        serverItem->fSASLSuccess = false;              // Resets SASL flag metrics
                        serverItem->fHasIdentifiedThisSession = false; // Unlocks NickServ text backup script
                        
                        if (serverItem->fSupportedCaps.Length() > 0) {
                            serverItem->fSupportedCaps.Truncate(0);    // Flushes old capabilities string
                        }
                        
                        if (cfg.debugEnable) {
                            printf("[DEBUG_RECONN] [%s] Session flags reset for fresh loop reload.\n", serverItem->Text());
                        }
                        // =========================================================================

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
                
                // =========================================================================
                // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                // =========================================================================
                // Verify that this server pointer currently possesses a live, active secure handle
                bool serverStillConnected = false;
                Lock();
                if (gServerSslHandles.find(static_cast<void*>(serverItem)) != gServerSslHandles.end()) {
                    serverStillConnected = true;
                }
                Unlock();
                
                if (serverStillConnected) {
                    if (fActiveListWindow == nullptr) {
                        // NEW SIGNATURE: Pass the serverItem pointer token instead of a raw socket reference
                        fActiveListWindow = new IRCChannelListWindow(this, serverItem, &fActiveListWindow);
                        fActiveListWindow->Show();
                    } else {
                        if (fActiveListWindow->Lock()) {
                            fActiveListWindow->Activate(true);
                            fActiveListWindow->Unlock();
                        }
                    }
                } else {
                    // Inform the user dynamically based on the explicit server name they right-clicked
                    BString warning;
                    warning.SetToFormat("System Error: You must connect to '%s' first before requesting a channel list.\n", 
                                        serverItem->Text());
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
                    
                    // Safely update text run foregrounds without altering custom bold/underlines
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

                // 3. Explicitly propagate theme updates down to Custom Draw Engine View ---
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
                
                // Verify we don't accidentally duplicate the exact same message back-to-back
                bool isDuplicate = false;
                int32 totalHistory = fHistoryList.CountItems();
                if (totalHistory > 0) {
                    BString* lastItem = fHistoryList.ItemAt(totalHistory - 1);
                    if (lastItem != nullptr && text == *lastItem) {
                        isDuplicate = true;
                    }
                }

                // If it's a fresh sentence, save it to our historical memory database heap
                if (!isDuplicate) {
                    fHistoryList.AddItem(new BString(text));
                }

                // Snap the history index indicator straight to the total array item length.
                fHistoryIndex = fHistoryList.CountItems();

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

                // 1. MULTI-SERVER: Use dynamic fallback tracking rather than hardcoded global nodes
                if (contextServer == nullptr) {
                    contextServer = fCurrentServerNode;
                }
                
                // =========================================================================
                // OPENSSL MAPPING UPDATE: Lookup secure pipe handle via tracking maps
                // =========================================================================
                SSL* activeSslHandle = nullptr;
                if (contextServer != nullptr) {
                    activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                }
                
                if (activeSslHandle != nullptr) {
                    BString rawPayload;
                    
                    // =========================================================================
                    // SLASH COMMAND INTERPRETER ENGINE
                    // ==================================================================
                    if (text.StartsWith("/")) {
                        BString commandLine = text;
                        commandLine.Remove(0, 1); // strip the '/'                            
                        
                        if (commandLine.ICompare("clear", 5) == 0) {
                            if (fActiveBufferItem != nullptr) {
                                fTextBuffers[fActiveBufferItem] = "";
                                fChatLog->SetText("");
                                
                                // --- Clear custom text row canvas vectors instantly ---
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
                                bigtime_t thirtyMinutesInUsecs = (bigtime_t)30 * 60 * 1000000;
                                BStringItem* targetNode = fActiveBufferItem;

                                if (targetNode != nullptr) {
                                    bool needsTimestamp = (fLastTimestampTime.count(targetNode) == 0);
                                    if (!needsTimestamp) {
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

       
       
       
       
       
       
                                       // Local Output Formatting Loop for our CTCP /me Chat Window display
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

                            if (activeTarget.StartsWith("#") || activeTarget.StartsWith("&")) {
                                if (commandLine.Length() > 0) {
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
                                // NEW ARCHITECTURE: Stripped activeSocket out of signature parameters cleanly
                                fActiveListWindow = new IRCChannelListWindow(this, contextServer, &fActiveListWindow);
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
                            rawPayload << "PRIVMSG " << activeTarget << " :" << text << "\r\n";
                            
                            BString timestampPrefix = "";
                            bigtime_t currentTime = real_time_clock_usecs();
                            bigtime_t thirtyMinutesInUsecs = (bigtime_t)30 * 60 * 1000000;
                            BStringItem* targetNode = fActiveBufferItem;

                            if (targetNode != nullptr) {
                                bool needsTimestamp = (fLastTimestampTime.count(targetNode) == 0);
                                if (!needsTimestamp) {
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

                            BString echoStr;
                            echoStr << timestampPrefix << "<" << fMyNick << "> " << text << "\n";
                            LogToItemBuffer(fActiveBufferItem, echoStr);
                        }
                    }

                    // =========================================================================
                    // UNIFIED OPENSSL STREAM TEXT ROUTER ENGINE (FIXED MULTI-SERVER GATE)
                    // =========================================================================
                    if (rawPayload.Length() > 0) {
                        SSL* activeSslHandle = gServerSslHandles[static_cast<void*>(contextServer)];
                        int activeFd = (gServerRawSockets.count(static_cast<void*>(contextServer)) > 0) ? gServerRawSockets[static_cast<void*>(contextServer)] : -1;

                        if (activeSslHandle != nullptr && activeFd >= 0) {
                            SSL_write(activeSslHandle, rawPayload.String(), rawPayload.Length());
                            
                            if (cfg.debugEnable) {
                                printf("[DEBUG_TEXT] Transmitted text data payload safely over live OpenSSL context on FD %d.\n", activeFd);
                            }
                        } else {
                            BString warning = "System Error: OpenSSL session handle context is unallocated or dead.\n";
                            LogToItemBuffer(fActiveBufferItem, warning);
                        }

                        if (cfg.debugEnable && contextServer != nullptr) {
                            LogDebugStream(contextServer->Text(), "OUTGOING", rawPayload.String(), rawPayload.Length());
                        }
                    }
                    
                    fInputControl->SetText(""); // Clear text layout input area cleanly for next entry
                } else {
                    BString warning = "System Error: Cannot transmit text message. This server context is completely disconnected.\n";
                    LogToItemBuffer(fActiveBufferItem, warning);
                }
            }
            break;
        }










        case MSG_IRC_RECEIVED: {   	
                	
            BString rawLine;
            // 1. Intercept incoming raw text payload line
            if (message->FindString("text", &rawLine) == B_OK) {
                
                void* nodePtr = nullptr;
                ServerTreeItem* targetServerNode = nullptr;

                if (message->FindPointer("server_node", (void**)&nodePtr) == B_OK && nodePtr != nullptr) {
                    targetServerNode = static_cast<ServerTreeItem*>(nodePtr);
                    fCurrentServerNode = targetServerNode; 
                } else {
                    targetServerNode = fCurrentServerNode != nullptr ? fCurrentServerNode : fLiberaNode;
                }
                
                bool showCodesForThisServer = false;
                if (targetServerNode != nullptr) {
                    showCodesForThisServer = targetServerNode->fEnableColorCodes;
                }

                // ===================================================================
                // --- ROBUST PREFIX-LOCKED NICKNAME COLOR INJECTOR ---
                // ===================================================================
                if (targetServerNode != nullptr) {
                    int32 cmdIdx = rawLine.FindFirst(" PRIVMSG ");
                    if (cmdIdx == B_ERROR) cmdIdx = rawLine.FindFirst(" NOTICE ");

                    // SAFETY FILTER: Only process lines that are public channel streams (contain '#')
                    // and ensure it's a real user message rather than a server log connection frame
                    if (cmdIdx != B_ERROR && rawLine.FindFirst("#") != B_ERROR) {
                        BString senderNick = "";
                        
                        // 1. Cleanly isolate the protocol routing block before PRIVMSG/NOTICE
                        BString prefixBlock;
                        rawLine.CopyInto(prefixBlock, 0, cmdIdx);
                        prefixBlock.Trim();

                        if (prefixBlock.StartsWith(":")) {
                            prefixBlock.Remove(0, 1);
                        }

                        // 2. Extract nickname cleanly right before the userhost exclamation mask
                        int32 exclamIdx = prefixBlock.FindFirst("!");
                        
                        // CRITICAL SIDEBAR PROTECTION: Only colorize if it contains '!' (a real human user)
                        // Server daemons and automated core services do not have exclamation hostmasks.
                        if (exclamIdx != B_ERROR) {
                            prefixBlock.Truncate(exclamIdx);
                            senderNick = prefixBlock;
                            senderNick.Trim();

                            // 3. Clear away user status privileges (~, &, %, @, +)
                            while (senderNick.Length() > 0 && 
                                   (senderNick.StartsWith("~") || senderNick.StartsWith("&") || 
                                    senderNick.StartsWith("%") || senderNick.StartsWith("@") || 
                                    senderNick.StartsWith("+"))) {
                                senderNick.Remove(0, 1);
                            }

                            // 4. Run matching loops with zero structural fragmentation
                            if (senderNick.Length() > 0) {
                                int32 matchedColorIndex = -1;
                                int32 serverIdx = targetServerNode->GetIndex();
                                bool isCustomServer = targetServerNode->IsCustom();

                                if (serverIdx >= 0) {
                                    const auto& colorNicks = (!isCustomServer) ? cfg.servers[serverIdx].nickColors 
                                                                               : cfg.customServers[serverIdx].nickColors;
                                    const auto& colorVals  = (!isCustomServer) ? cfg.servers[serverIdx].nickColorValues 
                                                                               : cfg.customServers[serverIdx].nickColorValues;

                                    for (size_t k = 0; k < colorNicks.size(); k++) {
                                        if (MatchWildcard(senderNick.String(), colorNicks[k].c_str())) {
                                            rgb_color targetColor = colorVals[k];
                                            
                                            // Standard mIRC 16-color configuration palette
                                            rgb_color ircPalette[] = {
                                                { 255, 255, 255, 255 }, { 0,   0,   0,   255 }, { 0,   0,   127, 255 }, { 0,   127, 0,   255 },
                                                { 255, 0,   0,   255 }, { 127, 0,   0,   255 }, { 127, 0,   127, 255 }, { 255, 127, 0,   255 },
                                                { 255, 255, 0,   255 }, { 0,   255, 0,   255 }, { 0,   127, 127, 255 }, { 0,   255, 255, 255 },
                                                { 0,   0,   255, 255 }, { 255, 0,   255, 255 }, { 127, 127, 127, 255 }, { 192, 192, 192, 255 }
                                            };
                                            
                                            int32 bestIdx = 4; // Fallback index default to Red
                                            uint32 minDistance = 0xFFFFFFFF;
                                            for (int32 p = 0; p < 16; p++) {
                                                int32 dr = targetColor.red - ircPalette[p].red;
                                                int32 dg = targetColor.green - ircPalette[p].green;
                                                int32 db = targetColor.blue - ircPalette[p].blue;
                                                uint32 dist = (dr*dr) + (dg*dg) + (db*db);
                                                if (dist < minDistance) {
                                                    minDistance = dist;
                                                    bestIdx = p;
                                                }
                                            }
                                            matchedColorIndex = bestIdx;
                                            break;
                                        }
                                    }
                                }

	                            // --- TARGETED PREFIX-LOCKED ENCAPSULATION ---
	                            if (matchedColorIndex != -1) {
	                                BString colorOpenCode, colorCloseCode;
	                                colorOpenCode.SetToFormat("[C:%d][B]", matchedColorIndex);
	                                colorCloseCode = "[R]";
	
	                                // Find exactly where the clean sender nickname starts inside the REAL rawLine buffer
	                                int32 nickStartPos = rawLine.FindFirst(senderNick);
	                                
	                                // Safety check: ensure we are modifying within the prefix boundary
	                                if (nickStartPos != B_ERROR && nickStartPos < cmdIdx) {
	                                    // FIXED: Insert the reset code right after the nick length (before the '!')
	                                    // This forces it to stay bound to the nick token when ParseAndDisplayIRC splits it
	                                    rawLine.Insert(colorCloseCode, nickStartPos + senderNick.Length());
	                                    
	                                    // Insert the opening style code right before the nickname start
	                                    rawLine.Insert(colorOpenCode, nickStartPos);
	                                }
	                            }
                            }
                        }
                    }
                }
                // ===================================================================

                // A. Process variable-length color indicators (ASCII 3)
                while (true) {
                    int32 colorIdx = rawLine.FindFirst((char)3);
                    if (colorIdx == B_ERROR) break;

                    int32 stripLength = 1; 
                    int32 searchPos = colorIdx + 1;
                    BString colorNumbers = "";

                    if (searchPos < rawLine.Length() && isdigit(rawLine.ByteAt(searchPos))) {
                        colorNumbers.Append((char)rawLine.ByteAt(searchPos), 1);
                        stripLength++;
                        searchPos++;
                        if (searchPos < rawLine.Length() && isdigit(rawLine.ByteAt(searchPos))) {
                            colorNumbers.Append((char)rawLine.ByteAt(searchPos), 1);
                            stripLength++;
                            searchPos++;
                        }
                    }
                    if (searchPos < rawLine.Length() && rawLine.ByteAt(searchPos) == ',') {
                        int32 bgCheck = searchPos + 1;
                        if (bgCheck < rawLine.Length() && isdigit(rawLine.ByteAt(bgCheck))) {
                            colorNumbers.Append(",", 1);
                            colorNumbers.Append((char)rawLine.ByteAt(bgCheck), 1);
                            stripLength += 2; 
                            bgCheck++;
                            if (bgCheck < rawLine.Length() && isdigit(rawLine.ByteAt(bgCheck))) {
                                stripLength++;
                            }
                        }
                    }

                    rawLine.Remove(colorIdx, stripLength);

                    if (showCodesForThisServer) {
                        BString blockMarker;
                        if (colorNumbers.Length() > 0) {
                            blockMarker << "[C:" << colorNumbers << "]";
                        } else {
                            blockMarker = "[C:Reset]";
                        }
                        rawLine.Insert(blockMarker, colorIdx);
                    }
                }

                // B. Process single-byte toggle markers (Bold, Reset, Underline, Reverse)
                if (showCodesForThisServer) {
                rawLine.ReplaceAll("\x02",  "[B]");
                rawLine.ReplaceAll("\x0F",  "[R]");
                rawLine.ReplaceAll("\x1F",  "[U]");
                rawLine.ReplaceAll("\x16",  "[REV]");
                } else {
                	rawLine.ReplaceAll("\x1F",  "");
                	rawLine.ReplaceAll("\x16",  "");
                	}
                	// 3. Forward the sanitized string directly into the parser architecture
                	
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

    // 1. Check if the socket is actively registered inside the multi-server tracking map
    if (fServerSockets.count(contextServer) > 0) {
        return fServerSockets[contextServer];
    }
    
    // 2. Fallback check specifically for hardcoded primary default nodes
    if (contextServer == fOftcNode) {
        return fOftcSocket;
    }
    if (contextServer == fLiberaNode) { // Make sure you use fLiberaNode variable tracking reference here
        return fLiberaSocket;
    }
    
    // 3. ROBUST GUARD: If it's a custom server and not active yet, return nullptr safely
    return nullptr;
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
    if (contextServer == nullptr) return;

    // 1. Resolve our exact per-server nickname state straight from config profiles
    std::string targetServerName = contextServer->Text();
    BString localNickToCheck = fMyNick; // Fallback baseline
    
    for (auto& srv : cfg.servers) {
        if (srv.name == targetServerName) {
            localNickToCheck = srv.nick.c_str();
            break;
        }
    }
    for (auto& srv : cfg.customServers) {
        if (srv.name == targetServerName) {
            localNickToCheck = srv.nick.c_str();
            break;
        }
    }

    bool activeViewWasModified = false;

    // 2. Loop through all active channel buffers safely
    for (auto it = fChannelUsers.begin(); it != fChannelUsers.end(); ++it) {
        ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(it->first);
        if (chanNode != nullptr) {
            // Safety check: ensure we only update channels that belong to the current server
            if (fChannelTree->Superitem(chanNode) != contextServer) continue;
            
            // Pass our securely resolved context-driven local nickname string
            UpdateUserAwayState(chanNode, localNickToCheck.String(), isAway);
            
            // Mark true if we modified the channel the user is currently staring at
            if (fActiveBufferItem == chanNode) {
                activeViewWasModified = true;
            }
        }
    }
    
    // 3. CONCURRENCY REPAINT CONTROL
    // Instantly refresh the right-hand nickname list panel ONLY if we modified the active view frame!
    if (activeViewWasModified && LockLooper()) {
        RefreshUserListUI();
        if (fUserList != nullptr) {
            fUserList->SortItems(SortUsersByRank);
            fUserList->Invalidate();
        }
        UnlockLooper();
    }
}


    BOutlineListView* fChannelTree;
    ServerTreeItem*   fLiberaNode;
    ServerTreeItem*   fOftcNode;
    ServerTreeItem*   fCurrentServerNode; 
    BString           fMyNick;
    BStringItem*      fActiveBufferItem;
    
    std::map<BStringItem*, BString> fTextBuffers;
    std::map<BStringItem*, BString> fChannelTopics;
    
    std::map<ServerTreeItem*, BObjectList<BString, true>*> fServerBanHarvests;

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

	ChannelModesDialog* fActiveModesDialog;
	BString             fActiveModesChannel;
	
    BObjectList<BString, true> fHistoryList;  
    int32                      fHistoryIndex; 
    BString                fPendingFocusChannel; 
    ServerTreeItem*        fPendingFocusServer = nullptr; 
	
}; 

int32 CricketWindow::GetServerIndexFromNode(BStringItem* node)
{
    if (node == nullptr) return -1;

    // 1. If the active node is a channel leaf, crawl up the tree layout to isolate the parent server
    ServerTreeItem* serverItem = nullptr;
    if (fChannelTree != nullptr) {
        BListItem* parentItem = fChannelTree->Superitem(node);
        if (parentItem != nullptr) {
            serverItem = dynamic_cast<ServerTreeItem*>(parentItem);
        } else {
            // It's already a root level server node item
            serverItem = dynamic_cast<ServerTreeItem*>(node);
        }
    }

    if (serverItem == nullptr) return -1;

    // 2. Direct hardcoded fast-track lookup paths
    if (serverItem == fLiberaNode) return 0;
    if (serverItem == fOftcNode)   return 1;

    // 3. Fallback traversal matching loop across custom servers array configurations
    for (size_t i = 0; i < cfg.customServers.size(); i++) {
        // Match custom servers by evaluating their server vector index positions
        if (serverItem->IsCustom() && serverItem->GetIndex() == i) {
            return static_cast<int32>(cfg.servers.size() + i);
        }
    }

    // Fallback standard index mapping match via custom tracker property ID
    return static_cast<int32>(serverItem->GetIndex());
}


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
