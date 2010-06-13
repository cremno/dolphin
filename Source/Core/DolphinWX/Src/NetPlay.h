#ifndef _NETPLAY_H
#define _NETPLAY_H

#include "Common.h"
#include "CommonTypes.h"
//#define WIN32_LEAN_AND_MEAN
#include "Thread.h"
#include "Timer.h"

// hax, i hope something like this isn't needed on non-windows
#define _WINSOCK2API_
#include <SFML/Network.hpp>

#include "GCPadStatus.h"
#include "svnrev.h"

//#include <wx/wx.h>

#include <map>
#include <queue>
#include <sstream>

class NetPlayDiag;

class NetPad
{
public:
	NetPad();
	NetPad(const SPADStatus* const);

	u32 nHi;
	u32 nLo;
};

struct Rpt : public std::vector<u8>
{
	u16		channel;
};

typedef std::vector<Rpt>	NetWiimote;

#define NETPLAY_VERSION		"Dolphin NetPlay 2.2"

#ifdef _M_X64
	#define	NP_ARCH "x64"
#else
	#define	NP_ARCH "x86"
#endif

#ifdef _WIN32
	#define NETPLAY_DOLPHIN_VER		SVN_REV_STR" W"NP_ARCH
#elif __APPLE__
	#define NETPLAY_DOLPHIN_VER		SVN_REV_STR" M"NP_ARCH
#else
	#define NETPLAY_DOLPHIN_VER		SVN_REV_STR" L"NP_ARCH
#endif

// messages
#define NP_MSG_PLAYER_JOIN		0x10
#define NP_MSG_PLAYER_LEAVE		0x11

#define NP_MSG_CHAT_MESSAGE		0x30

#define NP_MSG_PAD_DATA			0x60
#define NP_MSG_PAD_MAPPING		0x61
#define NP_MSG_PAD_BUFFER		0x62

#define NP_MSG_WIIMOTE_DATA		0x70
#define NP_MSG_WIIMOTE_MAPPING	0x71	// just using pad mapping for now

#define NP_MSG_START_GAME		0xA0
#define NP_MSG_CHANGE_GAME		0xA1
#define NP_MSG_STOP_GAME		0xA2
#define NP_MSG_DISABLE_GAME		0xA3

#define NP_MSG_READY			0xD0
#define NP_MSG_NOT_READY		0xD1

#define NP_MSG_PING				0xE0
#define NP_MSG_PONG				0xE1
// end messages

typedef u8	MessageId;
typedef u8	PlayerId;
typedef s8	PadMapping;
typedef u32	FrameNum;

enum
{
	CON_ERR_SERVER_FULL = 1,
	CON_ERR_GAME_RUNNING,
	CON_ERR_VERSION_MISMATCH	
};

THREAD_RETURN NetPlayThreadFunc(void* arg);

// something like this should be in Common stuff
class CritLocker
{
public:
	//CritLocker(const CritLocker&);
	CritLocker& operator=(const CritLocker&);
	CritLocker(Common::CriticalSection& crit) : m_crit(crit) { m_crit.Enter(); }
	~CritLocker() { m_crit.Leave(); }

private:
	Common::CriticalSection&	m_crit;
};

class NetPlay
{
public:
	NetPlay();
	virtual ~NetPlay();
	virtual void Entry() = 0;

	bool	is_connected;
	
	// Send and receive pads values
	void WiimoteInput(int _number, u16 _channelID, const void* _pData, u32 _Size);
	void WiimoteUpdate(int _number);
	bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);
	virtual bool ChangeGame(const std::string& game) = 0;
	virtual void GetPlayerList(std::string& list, std::vector<int>& pid_list) = 0;
	virtual void SendChatMessage(const std::string& msg) = 0;

	virtual bool StartGame(const std::string &path);
	virtual bool StopGame();

	//void PushPadStates(unsigned int count);

	u8 GetPadNum(u8 numPAD);

protected:
	//NetPlay(Common::ThreadFunc entry, void* arg) : m_thread(entry, arg) {}
	//void GetBufferedPad(const u8 pad_nb, NetPad* const netvalues);
	void ClearBuffers();
	void UpdateGUI();
	void AppendChatGUI(const std::string& msg);
	virtual void SendPadState(const PadMapping local_nb, const NetPad& np) = 0;

	struct
	{
		Common::CriticalSection		game;
		// lock order
		Common::CriticalSection		players, buffer, send;
	} m_crit;

	class Player
	{
	public:
		Player();
		std::string ToString() const;

		PlayerId		pid;
		std::string		name;
		PadMapping		pad_map[4];
		std::string		revision;
	};

	std::queue<NetPad>		m_pad_buffer[4];
	std::queue<NetWiimote>	m_wiimote_buffer[4];

	NetWiimote		m_wiimote_input[4];

	NetPlayDiag*	m_dialog;
	sf::SocketTCP	m_socket;
	Common::Thread*	m_thread;
	sf::Selector<sf::SocketTCP>		m_selector;

	std::string		m_selected_game;
	volatile bool	m_is_running;
	volatile bool	m_do_loop;

	unsigned int	m_target_buffer_size;

	Player*		m_local_player;

	u32		m_on_game;

private:

};

void NetPlay_Enable(NetPlay* const np);
void NetPlay_Disable();

class NetPlayServer : public NetPlay
{
public:
	void Entry();

	NetPlayServer(const u16 port, const std::string& name, NetPlayDiag* const npd = NULL, const std::string& game = "");
	~NetPlayServer();

	void GetPlayerList(std::string& list, std::vector<int>& pid_list);

	// Send and receive pads values
	//bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);
	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

	bool StartGame(const std::string &path);
	bool StopGame();

	bool GetPadMapping(const int pid, int map[]);
	bool SetPadMapping(const int pid, const int map[]);

	u64 CalculateMinimumBufferTime();
	void AdjustPadBufferSize(unsigned int size);

private:
	class Client : public Player
	{
	public:
		Client() : ping(0), on_game(0) {}

		sf::SocketTCP	socket;
		u64				ping;	
		u32				on_game;
	};

	void SendPadState(const PadMapping local_nb, const NetPad& np);
	void SendToClients(sf::Packet& packet, const PlayerId skip_pid = 0);
	unsigned int OnConnect(sf::SocketTCP& socket);
	unsigned int OnDisconnect(sf::SocketTCP& socket);
	unsigned int OnData(sf::Packet& packet, sf::SocketTCP& socket);
	void UpdatePadMapping();

	std::map<sf::SocketTCP, Client>	m_players;

	Common::Timer	m_ping_timer;
	u32		m_ping_key;
	bool	m_update_pings;
};

class NetPlayClient : public NetPlay
{
public:
	void Entry();

	NetPlayClient(const std::string& address, const u16 port, const std::string& name, NetPlayDiag* const npd = NULL);
	~NetPlayClient();

	void GetPlayerList(std::string& list, std::vector<int>& pid_list);

	// Send and receive pads values
	//bool GetNetPads(const u8 pad_nb, const SPADStatus* const, NetPad* const netvalues);
	bool StartGame(const std::string &path);
	bool ChangeGame(const std::string& game);
	void SendChatMessage(const std::string& msg);

private:
	void SendPadState(const PadMapping local_nb, const NetPad& np);
	unsigned int OnData(sf::Packet& packet);

	PlayerId		m_pid;
	std::map<PlayerId, Player>	m_players;
};

#endif
