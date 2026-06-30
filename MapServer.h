
#pragma once
//#ifndef MAPSERVER_H
//#define MAPSERVER_H


#include "..\\BaseServer\\BaseServer.h"
#include "..\\Common\\Limits.h"
#include "..\\Common\\DataProc.h"
//#include "..\\Common\\netapi.h"
#include "..\\common\\Protoco.h"
#include "..\\common\\protocoMap.h"
#include "..\\common\\protocoLogin.h"
#include "..\\common\\ProtocoDB.h"
#include "..\\common\\protocoLog.h"
#include "..\\common\\protocoVT.h"
//#include "..\\BaseServer\\BaseServer.h"

#define DIV_PROCESS_MAGIC_NUM				8

#define MAPBLOCK_SKIP_ENTER_AURA_SYNC

#include "MapPlayer.h"
#ifdef USE_OFFLINE_STALL
#include "MapServerOfflineStall.h"
#endif
#include "MapManager.h"
#include "MapServerShipSchedule.h"
#include "MapServerCityGuard.h"
#include "MapServerHistory.h"
//#include "MapServerHistoryOrgPVP.h"
#include "MapServerSpace.h"
#include <map>
#include <set>

#include "Lang.h"

#include "..\\data\\Magic_exp2.h"

////#define USE_PACKAGE_DATA					// ��Ƴ����O�_�ϥΫʥ]�ɧΦ�(�s�F�u)

//#define NO_SPEED_LIMIT
#define NO_USE_ITEM_UID2					// ���n�O�����~�X�֤�����D

#define mapPACKAGE_FILENAME					"data.pak"

#ifdef USERJOY_INNER_TEST
	#define NO_CLIENT_SPEED_CHECK				// ���n�[�t��t�_�u
#endif

#define NO_TIGHT_CLIENT_SPEED_CHECK			// �Ȱ����L�^���ιL���[�t
//#define NO_FILL_MAPCODE						// �H�����ʮɤ�����l(����@)

//#define CHECK_ATTACK_FREQUENCE				// �ˬd�����ʥ]�W�v
//#define CHECK_BOT_SO_SYSTEM					// �ˬdBOT����(story.so)

//#define NO_ROUND_ATTACK_EFFECT				// ���n���[���ˮ`�B�z
#define PARTY_DISTRIBUTE_AVERAGE				// �ҥβն��g��Ȫ��~����
#define NO_AUTO_DELETE_SUMMON				// ���n�n�J�R���l���~

// ASK VER �^����
#if (defined(GBMode) || defined(GBMode_TW))
	#define ASK_VER_COUNTRY_NAME				"CN"
#elif defined(JapanMode)
	#define ASK_VER_COUNTRY_NAME				"JP"
#elif defined(ThaiMode)
	#define ASK_VER_COUNTRY_NAME				"Thai"
#elif defined(VietnamMode)
	#define ASK_VER_COUNTRY_NAME				"VN"
#elif (defined(GAMEFLIER_NORMAL) || defined(GAMEFLIER_ITEMMALL) || defined(GAMEFLIER_CLASSIC))
	#define ASK_VER_COUNTRY_NAME				"TW"
#else
	#define ASK_VER_COUNTRY_NAME				"INNER"
#endif

// ���ե�
//#define CHECK_MAP_UPDATE_OBJECT				// �ˬd Map->Update() �ɬO�_���󭫽�
//#define DEBUG_SAVE_GATE_MAPCODE_FILE			// �ˬd����������D

#define NO_AUTO_KICK_BOT						// ���n�۰ʽ�X�~��
#define AUTO_KICK_BOT_LOCK_TIME		(60 * 24 * 365*3)	// ���G��

#define MAGIC_TELEPORT_DISTANCE				(gameIconSize*10)	// �������ʶZ��

#ifdef ThaiMode
	//#define SOUL_ITEM_DEFAULT_TIME			(60*60*24)		// �Z��ɶ�: �w�]�@��
	#define SOUL_ITEM_DEFAULT_TIME			(60*60*24*3)	// �Z��ɶ�: �w�]�T��
#else
	#define SOUL_ITEM_DEFAULT_TIME			(60*60*24*3)	// �Z��ɶ�: �w�]�T��
#endif
#define SOUL_ITEM_BOSS_DROP_TIME			(60*60*24)	// �Z��ɶ�: �@��
#define SOUL_DIE_ENTER_BATTLE_TIME			(30)		// �Z��D�N���`����h�[�~�i�i��

		// 4 ����B�z�A2��������

#define FIX_CITY_GATE_COLD_DOWN_TICK		1000	// �ײz�����P�ɦ��Ī��׮ɶ�

// �~���������~�X
#define BOT_REASON_PING_ERR				1		// Ping�ʥ]�ˬd�X���~
#define BOT_REASON_WALK_ERR				2		// ���ʫʥ]�ˬd�X���~
#define BOT_REASON_ATTACK_ERR			3		// �����ʥ]�ˬd�X���~
#define BOT_REASON_CAST_ERR				4		// �ޯ�ʥ]�ˬd�X���~
#define BOT_REASON_HACKER_CLIENT		5		// �ק� Client �{��(���ϥ�)
#define BOT_REASON_PING_ERR2			6		// Client �����ˬd���~
#define BOT_REASON_INVALID_PROTOCO		7		// ���ϥΪ��«ʥ]
#define BOT_REASON_INVALID_COMMAND		8		// �n�D�����\
#define BOT_REASON_INVALID_COMMAND2		9		// �n�D�����\
//
#define BOT_REASON_TIMEOUT				10		// �U�ɥ����ܡA���� Client �e�X�T�w���ˬd�ȡA�i��O�~���۰ʰe�ʥ]
#define BOT_REASON_CHANGE_FAST			11		// �u�ɶ����ܡA���� Client �b���հ����{���A�άO���ӳW�w�����ˬd��
#define BOT_REASON_CODE_NOT_FOUND		12		// �䤣���ˬd�X�A���� Client �e�X���ˬd�ȿ��~
#define BOT_REASON_CODE_SEQUENCE		13		// �ˬd�X���ǻ~�t�L�j�A���� Client ���Ӧ��ǧ����ˬd��
#define BOT_REASON_CODE_ERROR			14		// �ˬd�X���šA�����ˬd�{�Ǥw�����A�� Client �e�X���ˬd�ȿ��~
//
#define BOT_REASON_ATTACK_BOT_ENEMY		20		// �������Ω�
#define BOT_REASON_ATTACK_OUTOFRANGE	21		// �����W�L�d��
#define BOT_REASON_MEM_CHECKSUM_ERR		22		// Client �ݭ��n�禡�O����Q�ק�
#define BOT_REASON_STALL_OUTOFRANGE		23		// �n�D��Ƥ��X�k(�\�u)

#define BOT_REASON_GM_NOTICE			99		// �æ������D	(�o�ӥH�᪺�O GM ���`�q��)
#define BOT_REASON_GM_NOTICE_NET_ERR	100		// �ʥ]�q���`
#define BOT_REASON_GM_NOTICE_CNPC_SKL	101		// �p�L�ޯಧ�`
#define BOT_REASON_GM_NOTICE_BOT1		102		// �~���欰
#define BOT_REASON_GM_NOTICE_UNKNOWN_PROTOCO	103		// �D�k�ʥ] Protoco ID
#define BOT_REASON_GM_NOTICE_NO_TARGET	104		// �S���ؼ�
#define BOT_REASON_GM_NOTICE_TARGET_ERR	105		// �ؼФ������
#define BOT_REASON_GM_NOTICE_ATTACKSPD	106		// �����t�ײ��`

//#define CHAR_LEVEL_MAX				120		// ���ŭ���

#define BOSS_ARMY_START_MAPCODE			3000	// ���s�b���a��

#ifdef USE_PACKAGE_DATA
 #define CHAR_KILL_COUNT_UNIT			100		// �H�Q�Ƥ��W��Ǽƥ�
#else
 #define CHAR_KILL_COUNT_UNIT			2
#endif


#define SP_STATUS_MAX_HOUR_SEC			(18*60*60)
// �[���ɶ������w�q
//#define SP_STATUS_DETECT_FLAG_MONTH		0x80000000	// �벼�����ɶ����ˬd(�����\����s�b)
//#define SP_STATUS_DETECT_FLAG_HOUR		0x40000000	// �u�ɶ����ˬd(�̦h18�p��)


// �j�����I�g
#ifdef USE_PACKAGE_DATA

#define SPECMODE_CN_OFFLINE_CLEAR_TIME		(60 * 60 * 5)	// �U�u�M���ɶ� 5�p��
#define SPECMODE_CN_ONLINE_WARN_TIME_1		(60 * 60 * 3)	// �W�u�ɶ� 3�p��
#define SPECMODE_CN_ONLINE_WARN_TIME_2		(60 * 60 * 5)	// �W�u�ɶ� 5�p��
#define SPECMODE_CN_ONLINE_NOTIFY_TIME_1	(60 * 60)		// �����ɶ� 1�p��
#define SPECMODE_CN_ONLINE_NOTIFY_TIME_2	(60 * 30)		// �����ɶ� 30����
#define SPECMODE_CN_ONLINE_NOTIFY_TIME_3	(60 * 15)		// �����ɶ� 15����

#else

// ���ճ]�w
#define SPECMODE_CN_OFFLINE_CLEAR_TIME		(60 * 5)	// �U�u�M���ɶ� 5��
#define SPECMODE_CN_ONLINE_WARN_TIME_1		(60 * 3)	// �W�u�ɶ� 3��
#define SPECMODE_CN_ONLINE_WARN_TIME_2		(60 * 5)	// �W�u�ɶ� 5��
#define SPECMODE_CN_ONLINE_NOTIFY_TIME_1	(60)		// �����ɶ� 1��
#define SPECMODE_CN_ONLINE_NOTIFY_TIME_2	(30)		// �����ɶ� 30��
#define SPECMODE_CN_ONLINE_NOTIFY_TIME_3	(15)		// �����ɶ� 15��

#endif


void gameServer_MakeNPC(struct plrDATA_NPC * pSave, struct plrDATA_NPCSHOW * pData, CMapPlayer * pPlayer);


class CMapServerDlg;
class BotManager;
class BotFSM;

//#define	MAX_SERVER_OBJECTS			65536	// �����`��: �Ȯɩw�q, ����i�אּ�q�w�q��Ū���ƭ�
#define	MAX_SERVER_OBJECTS			200000		// 120000
#define	NPC_AI_ATTACK_TIMEOUT		20000			// 20 ��

#define PLAYER_DELETE_KEEP_TIME		1000
			// ���a�R���ɡA����O�d�ɶ�
#define PARTY_UPDATE_POS_TICK		(1000 * 4)		// ��s������m�ɶ�

#define CWAR_RESURRECT_DELAY_TIME	30			// ��Ԧa�Ϧ��`����

// �a�ϼаO ID
#define MAPCODE_ID_WALL				0xffffffff
#define MAPCODE_ID_FREE				0xfffffffe

#define	NPC_BOSS_RESTORE_HP_TICK	(1000 * 60 * 5)	// 5 ����
#define WAR_TELEPORT_SUPER_TIMES	2

#define SPECIAL_STATUS_TIME_GOOD_LUCK			(60 * 60)
#define SPECIAL_STATUS_TIME_ADD_EXP				(60 * 60)
#define SPECIAL_STATUS_TIME_ADD_SKILL_EXP		(60 * 60)
#define SPECIAL_STATUS_TIME_ADD_HONOR			(60 * 60)
#define SPECIAL_STATUS_TIME_ADD_CNPC_EXP		(60 * 60)
#define SPECIAL_STATUS_TIME_UNDEAD				(60 * 60 * 2)
#define SPECIAL_STATUS_TIME_SOLDIER_UNDEAD		(60 * 60)

extern struct plrDATA_SKILL plrEnemySkillTable;
extern struct plrDATA_NPC plrEnemyNPC;

// �S������ UID
enum
{
	MAP_GATE_UID		= gameMAX_ENEMY_UID + 1,	// ����
	MAP_WARP_POINT_UID,								// �ǰe�I
	MAP_MAGIC_UID,									// �]�k����
	MAP_SHOP_POINT_UID								// �ө��ǰe�I����
};

enum
{
	COMP_ENH_TIMES_EXCEED,		// �j�Ʀ��ƨ�F�W��
	COMP_ENH_OK,				// ���\
	COMP_ENH_NO_SET_DATA,		// �S���t����
	COMP_ENH_NO_MOD_SPACE,		// �S���ݩʬ������Ŷ�
	COMP_ENH_NO_EQUIP_TIME,		// �S���������˳Ʀ���
	COMP_ENH_NO_MAX_EQUIP_TIME,	// �S���������̤j�˳Ʀ���
	COMP_ENH_NO_ADD_MAX_TIME,	// ����W�[�˳Ʀ���
	COMP_ENH_NO_CLEAR_TIME,		// ����M���˳Ʀ���
	COMP_ENH_MUST_SELF_USED,	// �����O�ۤv�ϥΤ����˳�
};

class CLoginCheck
{
public:
	unsigned long	m_nCheckCode;
	unsigned long	m_nAccountID;
	unsigned long	m_nCancelTime;
	long			m_nPrivilege;
	long			m_nIsFirstFromLogin;		// �O�_�Ĥ@���n�J(�q Login �L��)
	unsigned long	m_nMapCode;
	long			m_nMapFlags;
	//
	unsigned char	m_nBotCheckType;
	unsigned char	m_nBotCheckTimes;
	//
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	unsigned short	m_nOnlineTime;	// ���I�g����
	unsigned short	m_nOfflineTime;
	long			m_nLastLogoutTime;
#endif

	enum
	{
		CANCEL_TIME			= 60000,			// 60 Second
		CANCEL_TIME_LONG	= 60000,			// 35 Second, �h�@�I���� Server ���ഫ�a��
	};
};

class CSpeedRecord
{
private:
	unsigned long m_nTickStart;
	unsigned long m_nFrame;
	unsigned long m_nLastFrame;
	unsigned long m_nElapseTickStart;
	unsigned long m_nCountDown;
	unsigned long m_nTickStart2;

public:
	CSpeedRecord(void);
	virtual	~CSpeedRecord(void);

	void SetFrameRatioStart();
	long GetFrameRatio(unsigned long *ratio);
	//
	void SetElapseTickStart();
	unsigned long GetElapseTick();
	//
	void SetCountDownTick(unsigned long tick);
	unsigned long GetCountDownTick();
};

enum
{
	FACE_STATUS_ASK_TO_DB,		// ���V DB �n�D��
	FACE_STATUS_OK,
};

struct PIC_PLAYER_HEAD
{
	long status;
	long time;
	char pic[ gameHEADPIC_WIDTH * gameHEADPIC_HEIGHT * 2 ];
};

struct KEEP_IMPORTANT_MSG_DATA2
{
	unsigned short nProtoco;
	unsigned short nSize;
	char * nBuffer;
	struct KEEP_IMPORTANT_MSG_DATA2 * nLink;
};

struct KEEP_IMPORTANT_MSG_DATA
{
//	long nKeep_Time;
	unsigned long nUID;
	struct KEEP_IMPORTANT_MSG_DATA2 * nLink;
	struct KEEP_IMPORTANT_MSG_DATA2 * nLink_Last;	// �̫�@�Ӫ���}
};

struct ORG_MAP_PK_DATA
{
	short nMode;				// 0=�L, 1=���ݹ��^��, 2=�}�Ԧ��\, 3=���ݮɶ��}�l
	unsigned short nMapCode;
	long nTimeOut;
	unsigned long nTargetOrgUID;
	unsigned long nTargetOrgLeaderUID;
	char szTargetOrgName[gameMAX_ORGANIZE_NAME_SIZE+1];
	//char szTargetLeaderName[gameMAX_PLAYER_NAME_SIZE+1];
};

struct ORG_MAP_PK_COUNT_DOWN_DATA
{
	unsigned long nOrgUID1;
	unsigned long nOrgUID2;
	unsigned long nLeaderUID1;
	unsigned long nLeaderUID2;
	long nTimeOut;
};

//CProtocoInfo
//�ʥ]��T�O��
class CProtocoInfo
{
public:
	long protoco;
	long count;
	long size;

	//CProtocoInfo
	CProtocoInfo(long _protoco=0, long _count=0, long _size=0)
	{
		protoco = _protoco;
		count = _count;
		size = _size;
	}

	bool operator < (const CProtocoInfo& info)
	{
		return protoco < info.protoco;
	}
};

//==========================================================================
// CMapServer �a�Ϧ��A��
//
//
//
//==========================================================================
class CMapServer : public CBaseServer
{
	friend class BotManager;
	friend class BotFSM;
public:
	static CMapServer *	m_pServer;

	unsigned long	m_nConnectCount;		// �s�u��
	CSpeedRecord m_SpeedRecord;				// Server �t�ײέp

	unsigned long nOptSerial_NPC;			// NPC div frame
protected:
#ifndef CONSOL_MODE
	CMapServerDlg *	m_pDialog;
#endif

	CMapManager		m_MapManager;
	CMapObject **	m_pMapObject;
	long			m_nFreeHandle;

	std::map<int, CMapPlayer *>				m_ClientMap;
	std::map<unsigned long, CMapObject *>	m_UIDMap;
	std::map<unsigned long, CLoginCheck>	m_LoginCheckMap;
	std::map<long, CMapObject *>			m_OuterMap;			// �a�ϥ~������޲z

	std::map<long, CMapObject *>			m_TempMap;			// ���󪬺A���ܼȦs

	unsigned long	m_nItemUIDMin;								// ItemUID �t�m�d��̤p��
	unsigned long	m_nItemUIDMax;								// ItemUID �t�m�d��̤j��
	unsigned long	m_nItemUID;									// �U�@�� ItemUID ��

public:
	int				m_hLoginServer;
	int				m_hDBServer;
	int				m_hLogServer;
	int				m_hVTServer;

	long			m_nIsLoginAskID;		// Login Server �O�_�n�D Map ID

	//struct plrDATA_WORLD_TOWN	m_TownData[gameMAX_WORLD_TOWN];
	struct plrDATA_WORLD_TOWN	m_TownData;
	struct plrDATA_WORLD_FORCE	m_ForceData[gameMAX_PLAYER_COUNTRY_ID];
	int				m_nLoadWorldDataTime;
	bool			m_IsWorldDataReady;

	unsigned long	m_nLocalBaseTime;
	unsigned long	m_nWorldBaseTime;
	bool			m_IsWorldTimeReady;
	//bool			m_IsDay;
	long			m_nDayState;

	CMapNPC*	m_KingStatueNPC[6]; //�U¾�Ĥ@(�J��NPC)
	struct plrDATA_WORLD_COUNTRY_WAR_LIST_DETAIL m_KingStatue[6]; //�U¾�Ĥ@(��)
	struct plrDATASHOWSELF m_KingStatue2[6]; //�U¾�Ĥ@(�Բ�)
	unsigned long m_KingStatueFlag[6];
	unsigned long m_KingStatueTime[6];

	unsigned long	m_nLastTickCount;
	unsigned long	m_nLoopTickCount;
	long			m_nLoopTime;
	/* 定时向 Login 拉战力榜以刷新头顶名次（秒级时间戳；间隔见 PollCombatPowerRankTitleFromLogin） */
	long			m_nLastCombatPowerRankTitlePoll;
	/* 仅服务端刷新：Login 回包对应 UID 时不向该客户端下发排行榜结果包 */
	std::set<unsigned long>	m_cpRankSilentPollUIDs;

	bool			m_IsCreating;
	long			m_IsWarPrep;
	long			m_IsWar;
	unsigned long	m_nWarEndTime;  /* Nation war end time (timestamp) */
	struct WAR_KING_SCORE m_nWarKingScore;

	struct plrDATA_WORLD_COUNTRY_WAR_LIST_REAL_TIME_SEND m_rtRankList[10];
	struct plrDATA_WORLD_COUNTRY_WAR_LIST_SHOW nCWarKListShow; // Loading�αƦ�](��A�e3�ΦU�A�U¾��1)

	bool			IsCreating(void);

	//virtual void	JoinSuccess( int nJoinIndex, int user_data );

	// callback functions
	static long		CB_GetMapDataResult				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetMapGeneralDataResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_LoginUpdatePeopleData(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_LoginEnterMap				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientLogin					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CheckPlayerUID				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataShowSelfResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataSkillResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataItemResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataStorageResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataMaterialLibResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataNPCResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataMissionResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataFriendResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataFashionResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataTianshuResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);	// 天枢数据回调
	static long		CB_GetPlayerDataAvatarAwakenResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);	// 化身觉醒数据回调
	static long		CB_GetPlayerDataDungeonClearResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);	// 副本通关数据回调
	static long		CB_GetPlayerDataTowerSlashResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginGetTowerSlashRankResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginSyncTowerSlashRank(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetPlayerDataCnpcItemSkillResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);	// 小兵道具技能数据回调
	static long		CB_InitClientReady				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientLogout					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_KickClient					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_ClientSpeedCheck				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetFace						(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetFaceResult				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_LoginToMap					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_MapToLoginResult				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_PlayerWalkTo					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_PlayerAttack					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_PlayerCastMagic				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_PlayerCancelMagic			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_PlayerResurrect				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ResurrectConfirm				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_ModifyPlayerItem				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_PlayerRebirth				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	// 装备升星系统
	static long		CB_EquipmentStarUpgrade			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CultivateOper				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_JinshanDig					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_TowerSlashEnter				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_TowerSlashQuery				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_TowerSlashSweep				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	// 装扮系统
	static long		CB_FashionActivate				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_FashionSetDisplay			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_FashionPowerUp				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_FashionSetAbility			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_FashionFurnace				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	// BOSS状态查询
	static long		CB_BossStatusRequest			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_LoginBossStatusSnapshotResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_BossInfoCatalogRequest		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_ModifySoldierSoul			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ModifySoldierEquip			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

public:
	static long		CB_ModifyPlayerHandItem			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
protected:
	static long		CB_PutIntoArrowBag				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetFromArrowBag				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ThrowItem					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ThrowItemBatch				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_UseItem						(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_OptionalLettoConfirm			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_UseItemOnItem				(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);

	static void		CB_FakeFunction					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	//�L���
	static long		CB_SoldierExchange				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ItemSoldierTransform			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	// �ө�
//	static long		CB_GetStoreList					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ShopBuy						(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ShopSell						(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_ShopTeleport					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	// �L��
	static long		CB_ArmyShopBuy					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyShopRevive				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyShopRename				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyShopUpgrade				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_NpcChangeItem				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyShopConvert				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyShopDecompose			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyShopGroup				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyShopSoldierExpTokens		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);


	//�X����
	static long		CB_Synthesis					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_Alchemy						(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static long		CB_Card							(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	
	// 抽卡系统
	static void		CB_GachaDraw					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_AvatarTransform				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);  // 变身系统
	static void		CB_AvatarHideAppearance			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);  // 隐藏/显示外观
	static void		CB_AvatarRefine					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);  // 炼化系统
	static void		CB_AvatarCardActivate			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);  // 化身卡激活图鉴
	static void		CB_AvatarAwakenInject			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);  // 注魂系统（扣魂能、增加段经验）
	static void		CB_AvatarAwakenUpgrade			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);  // 觉醒升级（扣升级材料、推进觉醒等级）
	static void		CB_AvatarAwakenQuery			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);  // 查询单张化身卡觉醒状态

	//�c��
	static long		CB_Promote						(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_JoinNation					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_AskJoinNationCharge			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//�ӧQ�Ũ�
	static long		CB_WinAnnounce					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

//	static long		CB_test							(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SetHotKey					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SetChannel					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SetOption					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SetWawaSingleOption			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_SiteDown(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ChangeWeapon					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SetFaceSymbol				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_UseFaceSymbol				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_SkillLvUp					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SkillAdjLvUp					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SkillAdjLvDown				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SkillSetLv					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_AddAttrib					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_AddAttrib2Batch				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_GetShowData					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetSkillData					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetStatus					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetSoldierData				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	
	static long		CB_SendMessageToClient			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_SendMessageToClientGroup		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SendChatMessageToClient		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SendMessageFromClient		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SendChatMessageFromClient	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GMCommand					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_SendBroadcastMessage			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_AskMapPlayerPos				(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_MapGotoPlayerResult			(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_MapGMCallPlayer(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_MapGMCallPlayerResult(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static long		CB_SavePlayerDataResult			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapTraceServerResult(char * pBuffer, int nLen, int nClientID, proto_COMM *pComm);

	static void		CB_MapSendMsgToClientByID(char * pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_MapSendMsgToClientByIDAndTime(char * pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_UpdateArmyChannelGroup(char * pBuffer, int nLen, int nID, proto_COMM * pComm);
	static void		CB_UpdateArmyChannel(char * pBuffer, int nLen, int nID, proto_COMM * pComm);
	static void		CB_LoginGetArmyDataResult(char * pBuffer, int nLen, int nID, proto_COMM * pComm);
		//from client
	static long		CB_GetBillboard					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetCombatPowerRank			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClaimCombatPowerRankReward	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetArmyBillboard				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetHistoryBattleRecord		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetWarRecord					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_GetWarReward					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_LoginAskHistoryBattleRecord	( char * pBuffer, int nLen, int nClientID, proto_COMM * pComm );
	static void		CB_LoginAskHistoryBattleRecordResult	( char * pBuffer, int nLen, int nClientID, proto_COMM * pComm );
	static long		CB_GetKSWarRTRecord					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_SetKSWarRTRank					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
		//from login server
	static long		CB_GetBillboardResult			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetArmyBillboardResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_GetCombatPowerRankResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClaimCombatPowerRankRewardResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	// ��ѫ�
	static long		CB_LoginGetChatroomListResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginOpenChatroomResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginJoinChatroomResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginQuitChatroomResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginChatroomInvite			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_LoginJoinChatroomNotify		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginQuitChatroomNotify		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_ClientGetChatroomList		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientOpenChatroom			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientJoinChatroom			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientQuitChatroom			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientChatroomInvite			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientChatroomKick			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_ClientCancelOpenChatroom		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	// ����
	static long		CB_LoginJoinPartyResult			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginInviteByParty			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginJoinPartyNotify			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_LoginQuitPartyNotify			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_ClientJoinParty				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientQuitParty				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientPartyInvite			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ClientPartyKick				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_ClientDismissParty(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_PartyPlayerStatus(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_PartyPlayerStatusMapPos(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_AssignPartyViceLeader(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_AssignPartyViceLeaderResult(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);

	//�x��
		//from client
	static long		CB_ArmyGetMemberList		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyGetArmyList			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmySendBulletin			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyLeaderResign			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyDismiss				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyDonate				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyQuit					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyAppointViceLeader	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_ArmyAppointStaff			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyExpelMember			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmySetupArmy			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyInvite				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyJoin					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyGetData				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyAskAdd2Channel		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyAdd2ChannelReply		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyChannelDelArmy		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyChannelGetList		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_ArmyJobClassUpDown		(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_ArmyStorageItemNotify	( char * pBuffer, int nLen, int nClientID, proto_COMM * pComm );
	static void		CB_MapInviteOrgPVP			( char * pBuffer, int nLen, int nClientID, proto_COMM * pComm );
		//from login server
	static long		CB_ArmyGetMemberListResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyGetArmyListResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmySetupArmyResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmySendBulletinNotify	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyDismissNotify		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyJoinNotify			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyDonateNotify			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyQuitNotify			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyExpelledNotify		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyNewViceLeaderNotify	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyMemberUpdateNotify	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyDataUpdateNotify		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyStorageUpdateNotify	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyBulletinUpdateNotify	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyInviteNotify			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyJoinNationNotify		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_ArmyGetDataResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_ArmyUpdateData			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_ArmyJobClassUpDownResult	(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static long		CB_ArmyPointUpdateNotify	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	
	static void		CB_GetArmyDataSingleResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_ArmyOpenBattleFieldResult( char *pBuffer, int nLen, int nID, proto_COMM *pComm );

	//����
	static long		CB_GetCityData				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityDepositGold			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityWithdrawGold			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CitySetTaxRate			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityAbandonCity			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityGateUpgrade			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityStatueUpgrade		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityGetPrebend			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityGetSalary			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityHireGuard			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityFireGuard			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CitySetupForce			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CitySetupSong			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_CityGotoAuditorium		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_CityFaithAdd				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//��Z��
	static long		CB_GetSoulTicketTable		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_BuySoulTicket			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_EnterSoulArena			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	//item mode
	static long		CB_AskForWebPts					( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_ItemMallBuy					( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_BagClaimVItem				( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_ItemMallGetList				( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static void		CB_ItemMallGetListResult		( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_ItemMallGetItem				( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static void		CB_ItemMallGetItemResult		( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static void		CB_ItemMallReceiveGift			( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static void		CB_ItemMallAddCharSlotResult	( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static void		CB_ItemMallAddMallPointResult	( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	//�x��pk
	static long		CB_OrgPVP_GetBattleBetList	( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_OpenBattle		( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_GiveUpOpenBattle	( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_KickOutChallenger	( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_AcceptChallenge	( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_GetOpenBattleList	( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_JoinBattle		( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_GiveUpJoinBattle	( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_Bet				( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_GetReward			( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	static long		CB_OrgPVP_EnterArena		( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	// LRG �s�W
	static void		CB_MapLogoutToLogin				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void 	CB_MapLogout2LoginFromMapResult	(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_LoginGetPartyInfoResult		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TalkToNPC(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TalkToNPCEnd(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_NPCSavePos(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_NPCNextStep(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_LoginAskMapServerID(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_MapBossBroadcastAllMapMessage(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_MapBossBroadcastMapMessage(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_MapBossBroadcastPlayerMessage(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_MapUpdateForceOrganizeTotal(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);
	static void		CB_MapUpdateForceData(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_NPCHotelSavePos(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_NPCHotelRest(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_NPCVTAskRecordResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_NPCVTCardAskRecordResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_NPCPointToVTCardResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_NPCTalkPackDataResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_FixSiegeWeapon(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	// NPC �p�L
	static void		CB_CNPCWalkTo(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_CNPCAttack(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_CNPCCastMagic(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_CNPCInOut(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_AskClientNumber(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_AskClientNumberServerResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	// ���
	static void		CB_CountryWar_CountdownNotify(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_CountryWar_InfoUpdate(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_CountryWar_UpdateForceTownData(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_CountryWar_TeleportOtherCountry(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_CountryWar_StatueBrokenMessage(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUpdateCountryWarListResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUpdateCWarKListResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_LoginUpdateTownForceData(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapSetRepayment(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapSetSoulTime(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapSetHistoryTimeData(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static void		CB_MapWarTownTaxAdd(char * pBuffer, int nLen, int nID, proto_COMM * pComm);

	static void		CB_MapLoginUpdateSTicketCount(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapCityDataPack(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUpdateAutoSO(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUpdateSPCModeTime(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUseRenameItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUseRenameItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUseGetPlayerPosItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUseGetPlayerPosItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_MapTradePswAskClientResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapTradePswClientAskAnswer(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapLoginTradePswVerifyResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_BroadcastGetDropItemMessage(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_MapPutWAWAItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapChangeWAWAItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapPutDeputyItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapChangeDeputyItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUpdateOnlineStatus(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapAutoSort(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapAutoSortStorage(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapGetMaterialLib(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapSaveMaterialLib(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//
	static void		CB_TradeGetItemList(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeGetItemListResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeGetItemByTypeID(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeGetItemByTypeIDResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeBuyItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeBuyItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeBuyItemNotify(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeSellItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeSellItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeGetTreasureBoxItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeGetTreasureBoxItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeGetSelfSellItemList(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeGetSelfSellItemListResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeCancelSelfSellItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeCancelSelfSellItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeAddSubGold(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_TradeUpdateAddSubGold(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
			//
#ifdef TW_PVP_MODE
	static void		CB_AskTWPVPListResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
#endif
	static void		CB_ClientPackData(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_LoginToMapMsgInfo(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_SoulCardMerge(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		GetDungeonCoolDown(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_MapDungeonSweep(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		SyncDungeonCoolDown(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_GetDungeonCoolDown(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_SetPayMapPosition(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	//�K���P��
	static void		CB_OpenGate(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_PowUpGate(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm);
	static void		CB_OpenEightGateFunc( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
	//�����J��
	static void		CB_GetKingStatue(char *pBuffer, int nLen, int nID, proto_COMM *pComm);//�������J�����
	static void		CB_UpdateKingStatue( char * pBuffer, int nLen, int nClientID, proto_COMM * pComm );//��s�����J�����

	//�ԧިt��
	static void		CB_BattleSkillCommand(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);

	//�ˮ`�p���
	static void		CB_AttackCalculate(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm);

			//
#ifdef CROSS_SERVER_SYSTEM			// �{�ɸ�SERVER����
 #ifdef CROSS_SERVER_CWAR
	static void		CB_LoginUpdateClientArmyData(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUpdateCountryKSCWarListRealTime(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_MapUpdateCWarKListShowResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
 #endif
#endif
			//
			long	PlayerCheckFreeNPCPos(CMapPlayer * pPlayer, long pos, long role_item_code, long summon_role_code = 0);
			long	CreateNPCFromItem(struct itemDATA_SAVE * pItem, CMapPlayer * pPlayer, long npc_pos_id);
			long	InsertItemFromNPC(CMapPlayer * pPlayer, unsigned long npcUID, struct itemDATA_SAVE * pItem);
		//	long	SetNPC_UID(unsigned long uid, unsigned long old_uid);
		//	long	ChangeNPC_UID(unsigned long uid1, unsigned long uid2);

			void PlayerSavePos(CMapPlayer * pPlayer, long disp_x, long disp_y);
			void PlayerRestoreAll(CMapPlayer * pPlayer);

public:
			void	InnerInitCarryNPCData(CMapNPC * pNPC, CMapPlayer * pPlayer, struct plrDATA_NPC_SAVE * pNPCSaveData = NULL);
			long	CreateNPCFromSummon(long role_code, long summon_time, CMapPlayer * pPlayer, long npc_pos_id);
			void	SendPlayerSystemMessageWithItem(CMapPlayer * pPlayer, long type, long res_id, long item_id);
			long	IsSpecialNPC(long type, long role_code);
			long	ChangeCarryNPCCode(CMapPlayer * pPlayer, CMapNPC * pNPC, long role_code, long time_sec);

			// �g Log ��ƨ� Log Server
			void SendLogMessage_System(CMapPlayer * pPlayer, long type);
			void SendLogMessage_Talk(CMapPlayer * pPlayer, long type, char * char_chat_name, char * msg);
			void SendLogMessage_Item(CMapPlayer * pPlayer, long type, CMapChar * pTarget, struct itemDATA_SAVE * pItem, struct itemDATA_SAVE * pItem_new = NULL);
			void SendLogMessage_ItemMerge(CMapPlayer * pPlayer, struct itemDATA_SAVE * pItem_disappear, struct itemDATA_SAVE * pItem_new);
			void SendLogMessage_ItemSeparate(CMapPlayer * pPlayer, struct itemDATA_SAVE * pItem_old, struct itemDATA_SAVE * pItem_new);
			void SendLogMessage_Act(struct LOG_INSERT_MSG_ACT * pReq);

	//���
	static long		CB_TradeSendReq					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_TradeAcceptReq				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_TradeRejectReq				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_TradeUploadObj				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
public: //xiun 04/03/19
	static long		CB_TradeCancel					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
protected: //end xiun
	static long		CB_TradeConfirm					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	//�\�u
	static long		CB_StallPrepare					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_StallCancelPrepare			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_StallUpload					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_StallGetItem					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_StallGetData					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
public: //xiun 04/06/14
	static long		CB_StallClose					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_StallLeave					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
protected: //end xiun
	static long		CB_StallBuy						(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_StallSell					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	//����
	static long		CB_AbortMission					(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_SetAutoHP_ItemID				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SetAutoMP_ItemID				(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	static long		CB_SetSoldierBattleArray		(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static long		CB_SetSoldierAI_Mode			(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

			bool	InitMenuProtocol(void);

			void	OuterObjectProc(void);
			void	TempObjectProc(void);
			void	LoginCheckProc(void);
			void	WorldTimeProc(void);

			long	CreateObject(unsigned short nClassID, long nCode, unsigned long nUID);
public:
			void	SendPartyPlayerPos(CMapPlayer * pPlayer, CMapPlayer * pDest = NULL);
			void	SendPartyPlayerStatus(CMapPlayer * pPlayer, struct MAP_PARTY_PLAYER_STATUS * pMsg, unsigned long skip_uid);

public:
	CMapServer(void);
	virtual	~CMapServer(void);

	void	SendToAllClients(long protoco, char* buffer, long size, long send_flag = 0);

	static CMapServer *	GetServer(void)						{ return(m_pServer); }

			int		GetDBServer(void)						{ return(m_hDBServer); }
			int		GetLoginServer(void)					{ return(m_hLoginServer); }
			int		GetVTServer(void)						{ return(m_hVTServer); }

			long	gameLoadTextResourceFiles();
	virtual bool	Create(int nServerType);
			void	LoadSetting(char *szProfile);
	virtual void	Destroy(void);
	virtual bool	Loop(void);
	virtual	void	Log(char *szFormat, ...);
			void	Log2(char *szFormat, ...);
			void	FileLog(long nLogType, char * szFrom, char * szTo, char * szDescribe, ...);
			void	PlayerErrLog(CMapPlayer * pPlayer, char *szFormat, ...);

	virtual bool	InitProtocol(void);
	virtual	long	ConnectClient(int nID);
	virtual	void	DisconnectClient(int nID);
			void	FlushAllClientSendQueue(void);
			void	Disconnect_ClearAllRecord( unsigned long uid, long type );
			void	DisconnectToLoginServer();
			void	UpdateLoginServerRecord();

	virtual void	Shutdown_ClearOnly(HWND hWnd, long wait_sec, long no_close_server = 0);
			void	Shutdown_SaveAllData();

	CMapCtrl *		FindMap(int nID,unsigned short nCopyUID=0){ return(m_MapManager.FindMap(nID,nCopyUID)); }
	CMapManager *	GetMapManager(void)						{ return(&m_MapManager); }

	unsigned short pwfOrgTotal_WEI;		// �x���`��
	unsigned short pwfOrgTotal_SHU;
	unsigned short pwfOrgTotal_WU;
	//
	unsigned long pwTotalPlayer_WEI;	// �Q���`�H��
	unsigned long pwTotalPlayer_SHU;	// �����`�H��
	unsigned long pwTotalPlayer_WU;		// �d���`�H��
	unsigned long pwTotalPlayer_NONE;	// �L���y�`�H��
	//
	unsigned short pwfTownTotal_WEI;	// �����`��
	unsigned short pwfTownTotal_SHU;
	unsigned short pwfTownTotal_WU;

	struct plrDATA_WORLD_TOWN *	GetTownDataArray(void)		{ return(&m_TownData); }
	struct plrDATA_WORLD_TOWN_DATA * GetTownDataByID(long town_id, long no_safe_ptr = 0);
	struct plrDATA_WORLD_TOWN_DETAIL * GetTownDetailDataByID(long town_id);
	// �p�⫰���`��
	void CalcTownTotalData();

	struct plrDATA_WORLD_FORCE * GetForceDataArray(void)	{ return(m_ForceData); }
	struct plrDATA_WORLD_FORCE * GetForceDataByID(long country_id);

			bool	IsWorldDataReady(void)					{ return(m_IsWorldDataReady); }

			void	SetLocalBaseTime(unsigned long nTime)	{ m_nLocalBaseTime = nTime; }
			void	SetWorldBaseTime(unsigned long nTime)	{ m_nWorldBaseTime = nTime; }
	unsigned long	GetLocalBaseTime(void)					{ return(m_nLocalBaseTime); }
	unsigned long	GetWorldBaseTime(void)					{ return(m_nWorldBaseTime); }
				// Server �ɶ�
	//unsigned long	GetWorldTime(void)				{ return(::time(NULL) - GetLocalBaseTime() + GetWorldBaseTime()); }
			long	GetWorldTime(void)				{ return(m_nLoopTime - GetLocalBaseTime() + GetWorldBaseTime()); }
			bool	IsWorldTimeReady(void)					{ return(m_IsWorldTimeReady); }
	//		bool	IsDay(void)								{ return(m_IsDay); }

	unsigned long	GetLoopTickCount(void)					{ return(m_nLoopTickCount); }
	unsigned long	GetLastTickCount(void)					{ return(m_nLastTickCount); }
			long	GetLoopTime(void)						{ return(m_nLoopTime); }

	// �󴫰��y�ɡA�ˬd�H�ƬO�_���\
			long	CheckForcePeople(long country_id);
			long	CheckForceForFree(long country_id);
			long	FindForceChangeCity(long map_code);

			long	IsWarPrep(void)							{ return(m_IsWarPrep); }
	// �O�_���
			long	IsWar(void)								{ return(m_IsWar); }
			long	IsWarType(void)							{ return(m_nWarType); }
			long	IsNPCWar(long map_code);							// NPC �n�ˬd�O�_�O��Ԧa��
			void	SetWarPrepStatus(long status);
			long	SetWarStatus(long status, long broadcast = 1);
			long	IsMapWar(long map_id, long include_3_city = 0);		// ���a�ϬO�_����Ԧa��(NPC �H�Υ\��B�z�W�P�_)
			long	IsSpecialMapWar(long map_id, long * country_id);
			long	CountryWar_TeleportOtherCountryPlayer(long map_code, long teleport_other);
	// �^�_�X��
	long			m_IsNPCCountryUpdateFlag;
	long			m_IsNPCCountryUpdate;

			void	SetNPCCountryUpdate(long clear = 0);
			long	IsNPCCountryUpdate();
			void	SetNPCRestorePos(long clear = 0);
			long	IsNPCRestorePos();
			void	SetNPCSetColor(long clear = 0);
			long	IsNPCSetColor();
			void	SetNPCMapSpaceUpdate(long clear = 0);
			long	IsNPCMapSpaceUpdate();
	// ���ͳB�z
			void	PlayerResurrectToSavePos(CMapPlayer * pPlayer);
	// �̶դO���o�����ǰe��m
			void	GetCapitalTeleportMapPos(long country_id, long * map_code, long * map_x, long * map_y,long jobID = 0);
	// ��ԡG�Ǧ^�x�s�I�μаO�I
			bool	ChangeSaveMap(CMapPlayer * pPlayer, long map_code, long map_x, long map_y, long force_msg = 0, long emul_war = 0,long copy_uid = 0);
	// ���W
			void	SendMessageToAllPlayers(long protoco, char * msg, long msg_size, long if_keep = 1);
			void	SendMessageToAllPlayers_Map(long map_code, long protoco, char * msg, long msg_size, long if_keep = 1, unsigned short nCopyUID = 0);
			void	ChangeAllPlayersForce(long old_country, long country_id, long type);

	// �Ҧ�����@�ߥѦ��إߩM�R��
			long	CreateWarpPoint(int nMapID, long nTargetX, long nTargetY, long set_color_only, long copy_uid = 0);
			long	CreateShopPoint(int nShopID);
			long	CreateStatue(long nCode, unsigned long nUID);
			long	CreateGate(long nCode, unsigned long nUID);
			long	CreateNPC(long nCode, unsigned long nUID, int nType);
			CMapNPC * CreateEnemy(long x, long y, long code, long map_id, long is_enter_map = 1, long is_die_delete = 1, long copyUID = 0);
			void	ChangeEnemyCodeData(CMapNPC * pNPC, long x, long y, long code, long map_id, long is_enter_map = 1, long is_die_delete = 1);
			long	FindPos(long map_id, long x, long y, long x_range, long y_range, long * dx, long * dy, long mode = 0);
			void	Army_RecordDel(long code, long army_id);
			long	CreatePlayer(int nClientID, unsigned long nPlayerUID);
			long	CreateMagic(unsigned long nRange);
			void	DeleteObject(long hObject);
			bool	IsObjectDeleted(long hObject);

			void	InsertOuterObject(long hObject);
			void	RemoveOuterObject(long hObject);

			long	GetDayState(void)						{ return(m_nDayState); }

			void	ChangeObjectState(long hObject, long nStateProc);

			CMapObject *	FindObjectByHandle(long hObject);
			CMapObject *	FindObjectByUID(unsigned long nUID, long class_id = CMapPlayer::CLASS_ID, long * is_deleted = NULL);
			CMapObject *	FindObjectByUIDForce(unsigned long nUID, long class_id = CMapPlayer::CLASS_ID);
			CMapPlayer *	FindPlayerByClientID(int nClientID, long * is_deleted = NULL);
			CMapPlayer *	FindPlayerByClientIDForce(int nClientID);
			CMapChar *		FindAndCheckCharExist(long hChar, CMapChar ** pCharPtr = NULL);
			CMapChar *		FindAndCheckCharExistByUID(unsigned long nUID, long class_id = CMapPlayer::CLASS_ID);

			CMapPlayer *	FindPlayerByCharName(char * name);
	std::map<std::string,unsigned long>	m_mCharToUID;	// �H���a����W�ٷ����ޡA���o UID

	// �s�u�w���G�P�@�� IP �̤j�s�u��
	std::map<std::string,unsigned long>	m_mConnectIPCount;

	// ���W��: �Ҧ����a, int = �s�uID
	std::map<int, CMapPlayer *> &	GetClientPlayerMap(void)	{ return(m_ClientMap); }

	// face ���a�Y��
	unsigned long	m_nFaceCacheNum;
	std::map<unsigned long, struct PIC_PLAYER_HEAD> m_nPlayerFace;
			void			DeletePlayerFaceCache(unsigned long uid);

			void			CharName_Add(char * name, unsigned long uid);
			void			CharName_Del(char * name);
			unsigned long	CharName_FindUID(char * name);
//			unsigned long	CharName_FindFirst();				// �ֳt�M��Ҧ����a��
//			unsigned long	CharName_FindNext();

	// �t�m ItemUID
			unsigned long	GenerateItemUID(void);
//			bool	MakeItem(CMapPlayer * pCreator, long nItemID, long nCount, struct itemDATA_SAVE * pItem);
			long	CompositeEnchanceItem(struct itemDATA_SAVE * pItem, long composite_id, long player_uid, long super_mode);

	// �O�d���a���n�T��
	// �_�u�P�s�u���\�ɲM��
	std::map<unsigned long, struct KEEP_IMPORTANT_MSG_DATA> m_nKeepImportantData;
			struct KEEP_IMPORTANT_MSG_DATA * kimGetData(unsigned long uid);
			void	kimKeepData(unsigned long uid, long protoco, char * buffer, long size);
			void	kimKeepDataToCheckLogin(long map_code, long protoco, char * buffer, long size);
			void	kimClear(unsigned long uid);
			void	kimSendData(CMapPlayer * pPlayer, unsigned long uid, long nClientID);		// �e����|�۰ʲM��
			void	kimSetFlagToCheckLogin(long map_code, long flag);		// �n�J�̧@�O��
	//		void	kimLoop();

#ifdef USE_COOL_DOWN_SYSTEM
			void	SaveCoolDownData(CMapPlayer * pPlayer);						// �x�s���
			void	GetCoolDownData(unsigned long player_uid, long is_first);	// Ū�����
			void	UpdateClientCoolDownData(CMapPlayer * pPlayer);				// ��s Client ���
	static void		CB_DBUpdateCoolDownResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
#endif
#ifdef SETUP_FORCE_CONTROL
	static void		CB_LoginPackGetJNPlayerList(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
	static void		CB_ExpelForcePlayer(char *pBuffer, int nLen, int nID, proto_COMM *pComm);
#endif
	static void		CB_OnDataStatus(char *pBuffer, int nLen, int nID, proto_COMM *pComm);

	// �O�d�W�l�P�����W�l
	CStringNameTable m_nPNameReverse;				// �O�d�W�l
	CStringNameTable m_nPNameDirty;					// �����W�l
	CStringNameTable m_nPNameCountry;				// ��W�L�o

	// �~��
	bool			IsBotEnemyCheck;
			void	BOT_NotifyGM(CMapPlayer * pPlayer, long type);

	// ���v���
	struct MAP_SET_REPAYMENT m_nRepayData;
	struct MAP_SET_ACTIVITY m_nActivityData;
			struct MAP_SET_REPAYMENT * GetRepayData()				{ return(&m_nRepayData); }
			struct MAP_SET_ACTIVITY * GetActivityData()				{ return(&m_nActivityData); }
			long	GetActivityStatus(long type);

#ifdef USE_OFFLINE_STALL
	COfflineStallManager	m_OfflineStallMgr;
#endif
	// ���v�ԧ�
	CMapHistory		m_nHistoryManager;
	struct MAP_SET_HISTORY_BATTLE m_nHistorySetData[gameMAX_HISTORYBATTLE_SETTING];

			struct MAP_SET_HISTORY_BATTLE * GetHistorySetTimeData(long id);
			CMapHistory * GetHistoryManager()						{ return(&m_nHistoryManager); }
			CMapHistoryOrgPVP * GetHistoryOrgPVPManager()			{ return(&m_nHistoryManager.m_OrgPVPManager); }

	unsigned char	m_nWarWeekDay;				// �P���X(1 - 7)
	unsigned char	m_nWarType;					// ����
	unsigned char	m_nWarHour;					// ��(0 - 23)
	unsigned char	m_nWarMin;					// ��(0 - 59)
	unsigned char	m_nNationWarAttacker;		// ��Ԧ����a
	unsigned char	m_nNationWarDefender;		// ��Ԧu���a
	// �Z��
	unsigned char	m_IsSoulBattle;
	unsigned char	m_CanEnterMode;
	std::map<unsigned long, long> m_EnterDueTime;
			//long	MakeTime(long year, long mon, long day, long hour, long min, long sec);
			void	Soul_SetSoulItemDuedate(long duedate);
			long	Soul_GetSoulItemDuedate();
			void	Soul_SetSoulTicketDuedate(long duedate);
			long	Soul_GetSoulTickDuedate();
			long	SetSoulBattleStatus(long status, long broadcast);
			void	Soul_BossDieTeleport(CMapNPC * pNPC);
			//
			void	Gen_BossDieTeleport(CMapNPC * pNPC, long mode = 0);
				// �ˬd���O��x�٬O�Z�x
				// return: -1 error, 0 �Z, 1 ��
			long	Soul_CheckTicketGenType(struct itemDATA_SAVE * pItem);
			long	Soul_CanBuyTicket();							// �ण��R��
			long	Soul_CheckCanEnter(CMapPlayer * pPlayer, long goto_map_code);		// ��_�J��
					// �O�_�O�D�Ԫ�
			long	Soul_IsChallenger(CMapPlayer * pPlayer, long goto_map_code, long * soul_job_type = NULL, struct gameSOUL_SET_DATA ** pSoulSetPtr = NULL, long hope_gen_pos = -1);
			void	Soul_ChallengerTeleport(CMapPlayer * pPlayer);	// �D�Ԫ̶ǰe����ɳ�

	struct MAP_SET_SOUL_DATA m_nSoulSetData;
	long			m_nSoulTicketDuedate;			// �������
	long			m_nSoulItemDuedate;				// �Z��~�����
	long			m_nSoulBattleStartTime;			// �԰��}�l�ɶ�
	long			m_nSoulBattleEndTime;			// �԰������ɶ�
	struct MAP_SOUL_RECORD_DATA m_nSoulRecordData;	// Login �ϥΧ��㵲�c�AMap �ϥι�ڤj�p

			struct MAP_SET_SOUL_DATA * GetSoulSetData()				{ return(&m_nSoulSetData); }
			struct MAP_SOUL_RECORD_DATA * GetSoulRecordData()		{ return(&m_nSoulRecordData); }
			void InnerSoulRecordInit();
			void InnerSoulCalcData();
			struct MAP_SOUL_RECORD_INNER_DATA * GetSoulRecordDetail(long map_code);
	// Trace
			void Trace_SendPlayerFullData(unsigned long uid, CMapPlayer * pTarget_Player);
		//	void Trace_SendPlayerFullData2(CMapPlayer * pPlayer, long type, char * buffer, long len);
			void Trace_MonitorSendSightObjects(unsigned long monitor_uid, CMapChar * pTarget);

	// ��d
			// �M�䪫�~
			struct itemDATA_SAVE * PlayerFindCarryItem_ByUID(CMapPlayer * pPlayer, struct itemUIDDATA * piUID, long * index);
			struct itemDATA_SAVE * CarryItem_FindFirstItem(CMapPlayer * pPlayer, long item_id, long * item_pos = NULL, long assign_item_pos = 0);
			struct itemDATA_SAVE * Card_FindItem(CMapPlayer * pPlayer, long item_id, char * CardNo, long * index = NULL);
			struct itemDATA_SAVE * Card_FindFuncItem(CMapPlayer * pPlayer, long item_id, long func_id, long * index = NULL);
			void Card_SetItemStatus(CMapPlayer * pPlayer, long item_id, char * CardNo, long status);
			long Card_SetOwner(CMapPlayer * pPlayer, char * CardNo, CMapPlayer * pNewPlayer);

	// ���
			//void InnerShipScheduleInit();
			CMapShipSchedule m_nShipSchedule;
	// �u��
			CMapCityGuard m_nCityGuard;

	// ���a�]�����`�A�����_�u
			void	KickPlayer(CMapPlayer * pPlayer);

	// ��ԱƦW
	unsigned short nCWarListSize;
	unsigned short nCWarKListSize;

	struct plrDATA_WORLD_COUNTRY_WAR_LIST nCWarList;
	struct plrDATA_WORLD_COUNTRY_WAR_LIST nCWarKList; // ��A�Ʀ�]
	
	// �B�z���y�ܧ���D(��Ԥ��]�դO�ܤƼv�T)
	// �O�� timeout �M���ɶ��A���a�n�Jloading���\��ۦ�M��
	struct KEEP_CHANGE_FORCE
	{
		long m_nType;			// type = 0 ���x�Ϊ��H���~(�w�B�z�L)
		long m_nCountryID;
		long m_nOldCountryID;
	};
	long m_nKeepChangeForceTime;
	std::map<unsigned long, struct KEEP_CHANGE_FORCE> m_nPlayerForceChange;

			void	pfcAdd(unsigned long uid, struct KEEP_CHANGE_FORCE * pData);
			void	pfcUpdateData(unsigned long uid);
			void	pfcClearData(unsigned long uid);
			void	pfcLoopProcess();
			void	PollCombatPowerRankTitleFromLogin(void);
	// ���I�g���m�ɶ�
	long m_nResetSpecCNModeTime;
			void ResetAllPlayer_SpecCNModeTime();

	// �ƥ�
	CMapSpace m_MapSpaceManager;
			CMapSpace * GetMapSpaceManager(void)				{ return(&m_MapSpaceManager); }

			void MapSpace_InitCopyObjects(CMapCtrl* pCtrl);

	// ��o�_�����W
			void BroadcastGetDropItemMessageToAllMap(char * msg);
			void BroadcastGetDropItemMessage(char * msg);			// �|�ǵ� Login Server �s��
	// �x�Φa��Pk���
	std::map<long, struct ORG_MAP_PK_DATA> m_nOrgMapPK;
	std::deque<struct ORG_MAP_PK_COUNT_DOWN_DATA> m_nOrgMapPKCountDown;
			long OrgMapPK_Invite(CMapPlayer * pPlayer, CMapPlayer * pTargetPlayer);
			long OrgMapPK_ResponseInvite(CMapPlayer * pPlayer, CMapPlayer * pTargetPlayer, long type);	// type = 0 No, 1 Ok
			unsigned long OrgMapPK_IsOrgWar(CMapPlayer * pPlayer);					// �O�_�ӭx�γB��x��PK(�Ǧ^�Ĺ�x��uid)
			void OrgMapPK_SetLoser(CMapPlayer * pPlayer);
			void OrgMapPK_CountDown();
			void OrgMapPK_SendCountDownMsg(struct ORG_MAP_PK_COUNT_DOWN_DATA * pCountDown);
			void OrgMapPK_InitEnterMapPlayer(CMapPlayer * pPlayer);
	// ���q ItemMode �ӫ~
#if (defined(ItemMode))
	std::map<long, long> m_mapItemModeQuota;
			void IMode_MakeQuotaInfo();						// Ū�� ItemModeShop �]�w�ɤ���I�s
			void IMode_SendQuotaInfoToLogin(unsigned long nPlayerUID = 0);				// ��s Login Server ���q���
			//void IMode_ItemQuotaDec(long item_set_id);		// Login Server �q���Y�ӳ]�w��� 1
	static	void CB_ItemMall_QuotaNotify( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm );
#endif
	// ���B
	std::map<unsigned long, long> m_mapMerryDelay;
			void SetMerryDelay(unsigned long player_uid);
			long GetMerryDelay(unsigned long player_uid);
			void ClearMerryDelay(unsigned long player_uid);
	// ����A��
			long IsCrossServer(long check_all=0);		// �O�_�O����A��Server
			long IsCrossCWarServer();					// �O�_�O����A�����Server
			long IsSoulWar(void)								{ return(m_CanEnterMode); }
#ifdef CROSS_SERVER_SYSTEM
			void KS_ConnectToCrossServer(CMapPlayer * pPlayer, unsigned long new_player_uid, long map_id, long posx, long posy, char * map_server_ip, long map_server_port, long serial);
			void KS_ReturnToOriginServer(CMapPlayer * pPlayer, unsigned long new_player_uid, long map_id, long posx, long posy, char * map_server_ip, long map_server_port, long serial);
			void KS_SendPlayerRedScoreToLogin(unsigned long player_uid, unsigned long rscore, unsigned long score = 0);
#endif			
			long IsHistorySaveDate();
	// �ۧګO�@
	long		m_nSelfBoomTime;
			void SetSelfBoomTime(long time);
			void StartSelfBoom(long time);
			void ProcSelfBoom();
	static	void	CB_GMToolGetFile( char *pBuffer, int nLen, int nID, proto_COMM *pComm );

	static void getPlayerSolSoulToAddSoldierAttr(struct plrDATA * pData,struct plrCALCDATA * pCData);
	static void getPlayerSkillToAddSoldierAttr(struct plrDATA * pData);
	static unsigned long long getPlayerSkillToAddSoldierAttrSF(unsigned long uid);

	//�έp�a�ϤH�� -- chenyin
public:	
	std::map<unsigned short, unsigned short> playerNumber;	
	unsigned short GetMapPlayersCount(unsigned short mapCode);
	unsigned short AddMapPlayer(unsigned short mapCode);
	unsigned short MinusMapPlayer(unsigned short mapCode);
	void CMapServer::EnterMap(long map_code, long copy_uid);
private:	
	std::map<unsigned short, unsigned short> playerNumberMax;		
	//bool cmp_by_value(const pair<unsigned short, unsigned short>& lhs, const pair<unsigned short, unsigned short>& rhs);
	void SavePlayerMaxNumber();
	
	struct CmpByValue {  
	  bool operator()(const pair<unsigned short, unsigned short>& lhs, const pair<unsigned short, unsigned short>& rhs) {  
		return lhs.second > rhs.second;  
	  }  
	}; 
};

