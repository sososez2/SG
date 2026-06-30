
#include "Stdafx.h"
#include "MapServer.h"
#include "MapPlayer.h"
#include "MapNPC.h"
#include "MapServerMenuLib.h"
#include "MapServerTowerSlash.h"
#include "MapServerCharWawaItem.h"
#include "..\\IncServer\\apiwin.h"
#include "..\\IncServer\\scribe.h"
#include "..\\Common\\R61_Common.h"
#include "..\\common\\utility.hpp"
//#include "..\\IncServer\\services.h"
#include "stdlib.h"

#include "..\\Common\\ItemProc.h"
#include "..\\Common\\NPCTalk.h"
#include "..\\Common\\Random.h"
#include "..\\Common\\DataRes.h"
#include "..\\Common\\Effect_Server.h"
#include "..\\Data\\ScriptFunc.h"
#ifndef scMIS_AskVTDataByType
#define scMIS_AskVTDataByType	122
#endif
#include "..\\Data\\magic_exp2.h"

#include "..\\PickupItem.h"
#include "MailProc.h"

#define TRANSFORM_EFFECT_SIGNATURE_MP 0x54524658L /* "TRFX" */
#define TRANSFORM_EFFECT_CLEAR_ID       (-39000)

#ifndef AVATAR_TRANSFORM_ADD_SECONDS
#define AVATAR_TRANSFORM_ADD_SECONDS 86400
#endif

/* plrStsTime_Month_PVP_DropItem 原未使用，借存 30 日功勋加倍等到期 unix 时间 */
#define plrStsTime_Month_AddHonor_field(s)  ((s)->plrStsTime_Month_PVP_DropItem)

#ifdef USE_COOL_DOWN_SYSTEM
	#include "..\\Common\\CoolDownProc.h"
#endif

// Add By R61

extern "C" struct plrDATA_FASHION_SAVE * GetPlayerFashionData(unsigned long uid);
// �i�ө�������
#include "MapServerPreShop.h"
// BattleExchange (兑换商店)
#include "MapServerBattleExchange.h"
// ShopType
#include "../Common/ShopProc.h"
//
#include "hDoc.h"
//#include "../hMenu/tinyxml.h"

#include <algorithm>

extern "C" long gameGetCWarResurrectDelaySecCfg();
extern "C" long gameGetLevelUpAttrPointCfg();

#ifndef USE_PACKAGE_DATA
 #ifndef GBMode
  #define SHOW_BOT_LOG
 #endif
#endif


#define AUTO_SAVE_DATA_TICK				(1000 * 60 * 5)		// 5 ����

#define CNPC_ADD_ROYALTY_PERIOD_TIME	(60 * 60 * 1)		// ���ۦ^�_�ɶ�


CMapPlayer * npcTalk_pPlayer = NULL;
static long g_NpcTalk_CurrentSelectId = 0;
static long g_NpcTalk_CurrentBtnIndex = 0;

// ============================================================
// ExchangeData.txt mapping (by NPC + button index)
// ============================================================
struct EXCHANGE_NPC_MAP_ENTRY
{
	long mapCode;		// NPC 所在地图（pos 第1个数）
	long posX;			// NPC 坐标X（pos 第2个数）
	long posY;			// NPC 坐标Y（pos 第3个数）
	long exchangeCode;	// ExchangeData.txt 里的 [exchange] code
};

#define EXCHANGE_NPC_MAP_MAX	2048
static struct EXCHANGE_NPC_MAP_ENTRY g_ExchangeNpcMap[EXCHANGE_NPC_MAP_MAX];
static long g_ExchangeNpcMapCount = 0;
static long g_ExchangeNpcMapLoaded = 0;

static void LoadExchangeNpcMapOnce()
{
	long node;
	long exchangeCode;
	char posStr[128];
	long posVal[3];
	long mapCode;
	long posX;
	long posY;
	long btnIndex;
	int si, di;
	char posNorm[128];

	if (g_ExchangeNpcMapLoaded) return;
	g_ExchangeNpcMapLoaded = 1;
	g_ExchangeNpcMapCount = 0;
	memset(g_ExchangeNpcMap, 0, sizeof(g_ExchangeNpcMap));

	if (!scribeLoadFile(baseMakeDataFileName("Data\\ExchangeData.txt")))
		return;

	node = scribeGetNodeSectionPosition("exchange", 1);
	while (node && g_ExchangeNpcMapCount < EXCHANGE_NPC_MAP_MAX)
	{
		exchangeCode = gameReadScribeFileNumber("code", 1, node, 0);
		mapCode = 0;
		posX = 0;
		posY = 0;
		btnIndex = 0;
		memset(posStr, 0, sizeof(posStr));
		memset(posVal, 0, sizeof(posVal));
		memset(posNorm, 0, sizeof(posNorm));

		// pos = <mapCode>,<posX>,<posY>
		// 按用户配置原样解析：第1个数=地图，第2/3个数=坐标
		if (scribeReadString("pos", 1, node, posStr))
		{
			// normalize spaces/tabs to improve parsing robustness
			di = 0;
			for (si = 0; posStr[si] && di < (int)sizeof(posNorm) - 1; si++)
			{
				if (posStr[si] != ' ' && posStr[si] != '\t' && posStr[si] != '\r' && posStr[si] != '\n')
					posNorm[di++] = posStr[si];
			}
			posNorm[di] = 0;
			gameStringTranslateSerialNumber2(posNorm, posVal, 3);
			mapCode = posVal[0];
			posX = posVal[1];
			posY = posVal[2];
		}

		if (exchangeCode > 0 && mapCode > 0)
		{
			g_ExchangeNpcMap[g_ExchangeNpcMapCount].mapCode = mapCode;
			g_ExchangeNpcMap[g_ExchangeNpcMapCount].posX = posX;
			g_ExchangeNpcMap[g_ExchangeNpcMapCount].posY = posY;
			g_ExchangeNpcMap[g_ExchangeNpcMapCount].exchangeCode = exchangeCode;
			g_ExchangeNpcMapCount++;
		}

		node = scribeGetNextNodeSectionPosition("exchange", node);
	}

	scribeFreeFile();
}

// Called from server init to preload ExchangeData.txt once.
void PreloadExchangeDataForNpcTalk()
{
	LoadExchangeNpcMapOnce();
}

static long ResolveExchangeCodeByNpc(unsigned long talkNpcUid, long btnIndex)
{
	long i;
	long mapCode = 0;
	long posX = 0;
	long posY = 0;
	long iconX = 0;
	long iconY = 0;
	CMapNPC * pNPC;
	long matchCount;
	long firstCode;

	LoadExchangeNpcMapOnce();

	if (!talkNpcUid || talkNpcUid == 0xffffffff)
		return 0;

	pNPC = (CMapNPC *)CMapServer::GetServer()->FindAndCheckCharExistByUID(talkNpcUid, CMapNPC::CLASS_ID);
	if (!pNPC)
		return 0;

	// Use NPC current map+pos for matching ExchangeData pos=...
	mapCode = (long)pNPC->GetMapCode();
	posX = (long)pNPC->GetSaveData()->plrPosX;
	posY = (long)pNPC->GetSaveData()->plrPosY;
	iconX = (long)gameIconCalc_DIV(posX);
	iconY = (long)gameIconCalc_DIV(posY);
	if (mapCode <= 0)
		return 0;

	// 按配置原样：pos=map,iconX,iconY
	// 同一个 pos 允许多个 [exchange]（代表同 NPC 的多个按钮）。
	// 选择规则：btnIndex=1/2/3... 对应该 pos 下第 N 个 [exchange]（按文件出现顺序）。
	matchCount = 0;
	firstCode = 0;
	for (i = 0; i < g_ExchangeNpcMapCount; i++)
	{
		if (g_ExchangeNpcMap[i].mapCode == mapCode &&
			((g_ExchangeNpcMap[i].posX == posX && g_ExchangeNpcMap[i].posY == posY) ||
			 (g_ExchangeNpcMap[i].posX == iconX && g_ExchangeNpcMap[i].posY == iconY)))
		{
			matchCount++;
			if (!firstCode)
				firstCode = g_ExchangeNpcMap[i].exchangeCode;
			if (btnIndex > 0)
			{
				if (matchCount == btnIndex)
					return g_ExchangeNpcMap[i].exchangeCode;
			}
			else
			{
				// 没有按钮序号时，默认取第一个
				return g_ExchangeNpcMap[i].exchangeCode;
			}
		}
	}
	// 按钮序号超过配置数量时，兜底返回第一个（避免返回0导致打不开）
	if (firstCode)
		return firstCode;
	return 0;
}

//long fix_skill_number_tbl[] = { 58, 59, 86, 87, 88, 89, 90, 91, 92, 93, 94, 96, 97, 98, 99, 101, 102, 103,
//187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 201, 202, 203, 204, 205, 206, 207, 208, 0 };


//==========================================================================
// ��o�@�ӯx�νd�򤺶üƦ�m
//==========================================================================
void GetRandomPos(RECT * pRect, long * xPtr, long * yPtr)
{
	long x, y, xrange, yrange;

	x = (pRect->left + pRect->right) / 2;
	y = (pRect->top + pRect->bottom) / 2;
	xrange = abs(pRect->right - pRect->left) + 1;
	yrange = abs(pRect->bottom - pRect->top) + 1;
	//
	x = x + gameGetRandomRange(0, xrange) - (xrange/2);
	y = y + gameGetRandomRange(0, yrange) - (yrange/2);
	//
	*xPtr = x;
	*yPtr = y;
}
//==========================================================================
// CMapPlayer �a�ϤW�����a
//
//
//
//==========================================================================
long CMapPlayer::m_nLoadProtoco[DB_TOTAL_TYPES_ALL] =
{
	PROTO_DB_GETPLAYERDATA_SHOWSELFRESULT,		// 0: DB_CHAR_DATA
	PROTO_DB_GETPLAYERDATA_SKILL_RESULT,		// 1: DB_SKILL_DATA
	PROTO_DB_GETPLAYERDATA_ITEM_RESULT,			// 2: DB_ITEM_DATA
	PROTO_DB_GETPLAYERDATA_STORAGE_RESULT,		// 3: DB_STORAGE_DATA
	PROTO_DB_GETPLAYERDATA_NPC_RESULT,			// 4: DB_NPC_DATA
	PROTO_DB_GETPLAYERDATA_MISSION_RESULT,		// 5: DB_MISSION_DATA
	PROTO_DB_GETPLAYERDATA_FRIEND_RESULT,		// 6: DB_FRIEND_DATA
	PROTO_DB_GETPLAYERDATA_FASHION_RESULT,		// 7: DB_FASHION_DATA
	0,											// 8: DB_TOTAL_TYPES (占位符)
	PROTO_DB_GETPLAYERDATA_TIANSHU_RESULT,		// 9: DB_TIANSHU_DATA
	PROTO_DB_GETPLAYERDATA_AVATAR_AWAKEN_RESULT,	// 10: DB_AVATAR_AWAKEN_DATA
	PROTO_DB_GETPLAYERDATA_DUNGEON_CLEAR_RESULT,	// 11: DB_DUNGEON_CLEAR_DATA
	PROTO_DB_GETPLAYERDATA_CNPC_ITEM_SKILL_RESULT,	// 12: DB_CNPC_ITEM_SKILL_DATA
	PROTO_DB_GETPLAYERDATA_TOWER_SLASH_RESULT		// 13: DB_TOWER_SLASH_DATA
};

long CMapPlayer::m_nSaveProtoco[DB_TOTAL_TYPES_ALL] =
{
	PROTO_DB_SAVE_PLAYER_DATA,					// 0: DB_CHAR_DATA
	PROTO_DB_SAVE_PLAYERDATA_SKILL,				// 1: DB_SKILL_DATA
	PROTO_DB_SAVE_PLAYERDATA_ITEM,				// 2: DB_ITEM_DATA
	PROTO_DB_SAVE_PLAYERDATA_STORAGE,			// 3: DB_STORAGE_DATA
	PROTO_DB_SAVE_PLAYERDATA_NPC,				// 4: DB_NPC_DATA
	PROTO_DB_SAVE_PLAYERDATA_MISSION,			// 5: DB_MISSION_DATA
	PROTO_DB_SAVE_PLAYERDATA_FRIEND,			// 6: DB_FRIEND_DATA
	PROTO_DB_SAVE_PLAYERDATA_FASHION,			// 7: DB_FASHION_DATA
	0,											// 8: DB_TOTAL_TYPES (占位符)
	PROTO_DB_SAVE_PLAYERDATA_TIANSHU,			// 9: DB_TIANSHU_DATA
	PROTO_DB_SAVE_PLAYERDATA_AVATAR_AWAKEN,		// 10: DB_AVATAR_AWAKEN_DATA
	PROTO_DB_SAVE_PLAYERDATA_DUNGEON_CLEAR,		// 11: DB_DUNGEON_CLEAR_DATA
	PROTO_DB_SAVE_PLAYERDATA_CNPC_ITEM_SKILL,		// 12: DB_CNPC_ITEM_SKILL_DATA
	PROTO_DB_SAVE_PLAYERDATA_TOWER_SLASH			// 13: DB_TOWER_SLASH_DATA
};

CMapPlayer::CMapPlayer(void)
{
	m_nClassID			= CLASS_ID;
	m_nClientID			= 0;

	m_IsReady			= false;
	m_nLoadCount		= 0;
	m_nExitCode			= EXIT_LOGOUT;
	m_SaveLoadTick		= 0;

	m_nPlayTime			= 0;
	m_nSuperTime		= 0;
	m_nTalkNPC_UID		= 0;
	m_nDeleteKeepTime	= 0;
	m_NoSuperMode		= 0;

	m_nPlayerFlags = 0;
	m_nCWarRewardExp = 0;
	m_nCWarRewardDragonSilver = 0;
	m_nCWarRewardToken = 0;
	m_nCWarRewardHonor = 0;

	m_nSpeedCheckSerial	= 0;
	m_nSpeedCheckTick	= 0;
	m_nSpeedCheckCount	= 0;
	m_nSpeedCheckOdds	= 0;
	m_nSpeedCheckTime	= 0;
	m_nSpeedCheckCountTime = 0;
	m_nSpeedCheckErrorCount = 0;
	m_nSpeedCheckSkipCount = 0;
	m_nCheckMissionCount = 0;

	m_nBotKickTime = 0;
	m_nBotCount = 0;
	m_nBotMemoryCheck = 0;
	m_nBotFakeProtocoLockTime = (long)::time(NULL) + 60 * gameGetRandomRange(10, 60);

	m_bIsMapWar = 0;
	//m_WarSuperTimes = 0;
	m_nIsChallenger = 0;

	m_nCWarResurrectType = 0;
	m_nCWarResurrectTick = 0;

	m_nBotCheckType = -1;
	m_nBotCheckTimes = 0;
	m_nBotCheckErrCount = 0;
	m_nBotCheckDuedate = 0;
	m_nBotCheckLastUpdateTime = 0;
	m_nBotLastCode = 0xffffffff;
	memset(m_nBotReason, 0, sizeof(m_nBotReason));
	m_nBotCheckReFind = 0;
	memset(&m_nLastActionTick, 0, sizeof(m_nLastActionTick));
	m_nLastActionTickErrCount = 0;
	m_nLastActionTickClearTime = 0;

	m_WaitVT_Timeout = 0;
	m_WaitVT_State = 0;
	m_WaitVT_TalkID_NoData = 0;
	m_WaitVT_TalkID_Error = 0;
	m_WaitVT_TalkID_Ok = 0;

	m_nLastCPRankKindReq = 0;

	m_nSightBlocks		= 0;
	m_pSightBlock		= NULL;
//	::memset(m_pSightBlock, 0, sizeof(CMapBlock *) * MAP_BLOCK_COUNT_ALL);

	m_IsLoadingData		= false;
	m_IsSavingData		= false;
	::memset(m_nLoadTime, 0, sizeof(unsigned long) * DB_TOTAL_TYPES_ALL);
	::memset(m_nSaveTime, 0, sizeof(unsigned long) * DB_TOTAL_TYPES_ALL);
	::memset(&m_FashionData, 0, sizeof(m_FashionData));
	::memset(&m_TianshuData, 0, sizeof(m_TianshuData));
	::memset(&m_AvatarAwakenData, 0, sizeof(m_AvatarAwakenData));
	::memset(&m_CnpcItemSkillData, 0, sizeof(m_CnpcItemSkillData));
	::memset(&m_TowerSlashData, 0, sizeof(m_TowerSlashData));
	m_TowerSlashData.currentFloor = 1;

	m_nPartyNumber		= 0;
	m_nPartyGetItemIdx	= 0;
	::memset(&m_nPartyUID, 0, sizeof(m_nPartyUID));

	m_nNPCTalkMsgID		= 0;
	m_nNPCTalkSerial	= 0;
	memset(&m_nNPCSelectID, 0, sizeof(m_nNPCSelectID));

	m_nRoundAttackerNumber = 0;
	m_nRoundAttackerTime = 0;
	memset(&m_nRoundAttacker, 0, sizeof(m_nRoundAttacker));

	//
	m_nAutoSaveTick = AUTO_SAVE_DATA_TICK;

	m_nCharFlags |= CHAR_FLAG_NO_MOVE_BLOCK;
	m_bSpeedCheckSkip = true;

	m_bSaveDataFlag = false;	// �O�_�n�x�s Data ���
	m_bSaveItemFlag = false;
	m_bSaveNPCFlag = false;
	m_bSaveSkillFlag = false;
	m_bSaveStorageFlag = false;
	m_bIsTeleport = false;
	m_TeleportKickTime = 0;
	m_TeleportKeepOutTime = 0;

	m_nTeleportMapCode = 0;
	m_nTeleportDueTime = 0;
	m_nTeleport2MapCode = 0;
	m_nTeleport2DueTime = 0;
	m_nTimedMapPassLastProcTick = 0;
	m_nTimedMapHudLastNotifyTick = 0;

	m_MapCtrlPtr = NULL;
	m_szVictoryMsg[0] = 0;

	m_nBotEnemyAppearTime = gameGetRandomRange(BOT_ENEMY_APPEAR_MIN_TIME, BOT_ENEMY_APPEAR_MAX_TIME) * 1000;

    // By Raven61
    // ���a�i�J�ө����и�
    // 0 �N�� �S���i�J�ө�
    m_ShopID = 0;

	SetResurrectQueryTime( 0 );
	SetResurrectHpPercent( 0 );

	ResetTradeData(); //xiun 04/03/15
	ResetStallData(); //xiun 04/06/14
#ifdef USE_OFFLINE_STALL
	m_bOfflineStallLicense = false;
	m_bOfflineStallGhost = false;
#endif
	ClearArmyStorageData(); //xiun 04/08/17

	m_nTraceLastPosX = 0;
	m_nTraceLastPosY = 0;

	m_nWebIP_Flags = 0;
	m_nPAErrorCount = 0;
	m_nPAErrorTime = 0;

	m_uArmyChannelInviterUID = 0;

	m_nHonorVal_Max = 0xffff;		// 2006/05/02
	m_nPlayerAttackErrorCount = 0;
	m_nPlayerAttackErrorTime = 0;

	m_nPlayerItemWaitTime = 0;
	m_nPlayerTalkWaitTime = 0;

	m_nOuterDueTime = 0;

	//m_nModeCN_LoopCount = 0;

#ifdef USE_TRADE_PASSWORD
	m_nTradePswAskDueTime = 0;
	m_nAskCount = 0;
	//m_nTradePswSerial[TRADE_PSW_IMAGE_NUM_TOTAL];
#endif
	m_bIsChangeWAWA = false;
	m_nChangeWAWAShowTick = 0;
	m_mapDuedateItem.clear();
#ifdef USE_RESURRECT_DELAY
	m_nResurrectDelay = 0;
#endif
	m_nWaitMerryTime = 0;
	m_nMerryPlayerUID = 0;
	m_szMerryPlayerName[0] = 0;
	m_nMerryPlayerOfficer = 0;
#ifdef TW_PVP_MODE
	m_nCrimeClearTick = 0;
#endif

	m_nArmyPoints = 0;
	m_nArmyBattleFieldDueTime = 0;
	m_nArmyBattleFieldCheckTick = 0;
	memset(&m_OrgActiveData,0,sizeof(plrDATA_ORGANIZE_ACTIVE));
}


CMapPlayer::~CMapPlayer(void)
{
	if (GetUID())
		SaveCnpcItemSkillData(0);
	gameCNPC_ItemSkillUnbindPlayer(GetUID());
#ifdef USE_COOL_DOWN_SYSTEM
	if (GetUID())
	{
		CoolDown_Clear(GetUID());
 #ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("DEBUG: Clear cold down record(%d) !", GetUID());
 #endif
	}
#endif
}

// ���a�b �����B���ʡB�I�k �O�_���\
// flag = 0x00000001 ��Ԧa�Ϥ��M���L�Įɶ�
//		= 0x00000002 �M����ԵL�Ħ��Ƭ���
//		= 0x80000000 ���ˬd�αs����
long CMapPlayer::CheckCan_Attack_Cast_Move(long flag)
{
	unsigned short i;

	if (!IsReady())
		return(0);

	if (flag & 2)
	{
		if (GetSaveData()->plrWarSuperTimes)
		{
			GetSaveData()->plrWarSuperTimes = 0;
			AutoSaveCharData();
		}
	}
	// �M���n�J�ɪ��L�Įɶ�
	if (IsSuperMode())
	{
		if (flag & 1)
		{
			if (!(m_nCharFlags & CHAR_HISTORY_BATTLE))		// ���v�Գ����ʵL��
			{
				if (!PlayerIsMapWar() || (GetSaveData()->plrWarSuperTimes > WAR_TELEPORT_SUPER_TIMES))
				{
//CMapServer::GetServer()->Log("�M���L��: %d, %d(%d, %d)", PlayerIsMapWar(), GetSaveData()->plrWarSuperTimes, m_bIsMapWar, GetMapCode());
					ClearSuperMode();
				}
			}
		}
		else
		{
			ClearSuperMode();
		}
	}

	// �i�ө���
//	if (GetShopID()) //mark by xiun--��IsAvailable()���N
//		return(0);
	if( !IsAvailable() ) //xiun : ���b�ө����A�S������B�\�u�� 04/06/03 
		return(0);
	// �O�_�αs�����B���O�� 0
	if (!(flag & 0x80000000))
	{
		struct plrDATA * pChar = GetCharData();
		if (pChar->plrFlags & CHARFLAGS_SUMMON_LEADER)	// ���l���~����h
		{
			// ���ܥi�H�ϥδ��O���N�αs
			if (gameNPC_CalcLeader_GetNeedInt(pChar) > pChar->plrAttrInt)
				return(0);
			if (pChar->plrLeaderUsed_CNPC > pChar->plrAttrLeader)
				return(0);
		}
		else
		{
			if (pChar->plrLeaderUsed > pChar->plrAttrLeader)
				return(0);
		}
		//
		if (i = GetSaveData()->plrEquipEngineer.m_Item.itemCode)
		{
			if (GetSaveData()->plrEngineerTime != 65535)
			{
				if (i = (unsigned short)gameGetSoldierItemLeaderUsed(i))
				{
					if (i > pChar->plrAttrLeader)
						return(0);
				}
			}
		}

		//if (pChar->plrFlags & CHARFLAGS_ILLEGAL_EQUIP)	// �D�k�˳�(���ݩ�)
		//	return(0);
	}
	// �D�k�˳�(�˳�¾�~�ܧ�)
	if (IsIllegalEquipItem())
		return(0);
	// ���ܤ�
	if (m_nTalkNPC_UID)
		ExitNPCTalk();
//	if (IsNPCTalk())		// NPC talk �ثe�O�����M�����A
//		return(0);


	// ...
	return(1);
}

// LRG �W�[���B�z
long CMapPlayer::PartyFind(unsigned long uid)
{
	long i;

	if (uid)
	{
		for (i=0; i<m_nPartyNumber; i++)
		{
			if (m_nPartyUID[i] == uid)
				return(1);
		}
	}
	return(0);
}

void CMapPlayer::PartyAdd(unsigned long uid)
{
	long i;

	if (uid)
	{
		if (m_nPartyNumber < gameMAX_PARTY_PLAYER)
		{
			for (i=0; i<m_nPartyNumber; i++)
			{
				if (m_nPartyUID[i] == uid)
					return;
			}
			//
			m_nPartyUID[m_nPartyNumber++] = uid;
		}
	}
}

void CMapPlayer::PartyDel(unsigned long uid)
{
	long i, size;

	if (m_nPartyNumber && uid)
	{
		for (i=0; i<m_nPartyNumber; i++)
		{
			if (m_nPartyUID[i] == uid)	// �}�C���e��
			{
				if (i < (gameMAX_PARTY_PLAYER - 1))
				{
					size = ((gameMAX_PARTY_PLAYER-1) - i) * sizeof(unsigned long);
					memcpyT(&m_nPartyUID[i], &m_nPartyUID[i+1], size);
				}
				m_nPartyUID[gameMAX_PARTY_PLAYER-1] = 0;
				m_nPartyNumber--;
				break;
			}
		}
	}
}

void CMapPlayer::PartyClear()
{
	m_nLastPartyPosX = 0x87654321;
	m_nLastPartyPosY = 0x87654321;
	m_nPartyNumber = 0;
	m_nPartyGetItemIdx = 0;
	::memset(&m_nPartyUID, 0, sizeof(m_nPartyUID));
	PartySetLeader(0);
	PartySetViceLeader(0);

//CMapServer::GetServer()->Log("Player(%d, %s) clear party", GetUID(), GetSaveData()->plrName);
}

// pos = 0, 1, 2 ...
unsigned long CMapPlayer::PartyGetMemberByPos(long pos)
{
	unsigned long uid = 0;

	if (pos < m_nPartyNumber)
	{
		uid = m_nPartyUID[pos];
	}
	return(uid);
}

void CMapPlayer::PartySetLeader(unsigned long uid)
{
	m_nPartyLeader = uid;
}

long CMapPlayer::PartyIsLeader()
{
	if (m_nPartyLeader == GetUID())
		return(1);
	return(0);
}

void CMapPlayer::PartySetViceLeader(unsigned long uid)
{
	m_nPartyViceLeader = uid;
}

void CMapPlayer::PartySetType(long type)
{
	if ((type != gamePARTY_TYPE_INDEPENDENT) && (type != gamePARTY_TYPE_AVERAGE))
		type = gamePARTY_TYPE_INDEPENDENT;
	m_nPartyType = type;

//CMapServer::GetServer()->Log("Player(%d, %s) party set type", GetUID(), GetSaveData()->plrName);
}

long CMapPlayer::PartyGetType()
{
	return(m_nPartyType);
}

// �M�w����ѽ���o���Ǳ������~
CMapPlayer * CMapPlayer::PartyFindGetItemMember()
{
	CMapPlayer * pDest;
	CMapPlayer * pPlayer;
	long map_id;
	long i, cnt;

	pDest = this;
	cnt = m_nPartyNumber;
	if (cnt > 1)
	{
		if (PartyGetType() == gamePARTY_TYPE_AVERAGE)
		{
			// ���y
			i = m_nPartyGetItemIdx;			// �ثe�n�����H
			if (i >= m_nPartyNumber)
			{
				i = 0;
			}
			// �O�_�M�ۤv�P�a��
			map_id = GetMapCode();
			while(1)
			{
				pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(m_nPartyUID[i]);
				if (pPlayer)
				{
					if (!pPlayer->IsDead())
					{
						if (pPlayer->GetMapCode() == map_id)
						{
							pDest = pPlayer;
							break;
						}
					}
				}
				if (--cnt <= 0)
					break;
				if (++i >= m_nPartyNumber)
					i = 0;
			}
			//
			if (++i >= m_nPartyNumber)		// �U���n�����H
				i = 0;
			m_nPartyGetItemIdx = i;
		}
	}
	return(pDest);
}

// �a�Ϭ�������A�������ʩ� goto �P�a�ϥ�
void CMapPlayer::MapCtrl_Lock()
{
	m_MapCtrlPtr = GetMapCtrl();
}

// clear_mapctrl = 1 �p�G�������A�@�_�M�� CMapCtrl ����
void CMapPlayer::MapCtrl_UnLock(long clear_mapctrl)
{
	if (m_MapCtrlPtr)
	{
		if (clear_mapctrl)
			m_MapCtrlPtr->DeletePlayerRecord(GetHandle(), GetUID());
		m_MapCtrlPtr = NULL;
	}
}

long CMapPlayer::MapCtrl_IsLock()
{
	if (m_MapCtrlPtr)
		return(1);
	return(0);
}

CMapCtrl * CMapPlayer::MapCtrl_GetMapCtrl()
{
	return(m_MapCtrlPtr);
}
// ====================================================================
// �ݩ��I�ƭ��m
// ====================================================================
bool CMapPlayer::Create(long hObject, long nCode, unsigned long nUID)
{
	if(!CMapChar::Create(hObject, nCode, nUID))
		return(false);

	memset(&m_nPlayerData, 0, sizeof(m_nPlayerData));

	m_nPlayTime	= 0;
	m_IsReady	= false;

#ifdef SMART_PLR_DATA2
	GetCharData()->plrSkillTable = &m_nPlayerData.plrSkillTable;
	GetCharData()->plrNPC = &m_nPlayerData.plrNPC;
#endif
	gameCNPC_ItemSkillBindPlayer(nUID, &m_CnpcItemSkillData);
	return(true);
}

void CMapPlayer::MoveProc(void)
{
	CMapCtrl *		pMap = GetMapCtrl();
	CMapObject *	pObject;
	CMapWarpPoint *	pWarp;
	CMapShopPoint *	pShop;
	bool			IsWarp;
	long			ix, iy, nCode;
//	long			cnt1, nIcons;
//	long			ix_fix;
	long			ix2, iy2;
	//
	long nTick, nColor, nCharColor;
	int map_code;
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	struct gs_StageData * pStg;
	char tmpstr[1024];

	if (m_nSendPartyPosTivk > 0)
	{
		nTick = CMapServer::GetServer()->GetLoopTickCount();
		if (m_nSendPartyPosTivk <= nTick)
		{
			// ����ʰO����s
			// ���m��Ƨi�D�P�@�� Map Server �䥦����
	//		if (IsKindOf(CMapPlayer::CLASS_ID))
				CMapServer::GetServer()->SendPartyPlayerPos(this);
			m_nSendPartyPosTivk = 0;
		}
		else
			m_nSendPartyPosTivk -= (unsigned short)nTick;
	}

	if (!IsMoving() || IsOutMap())
		return;
//	if (!IsOutMap())
	{
	/*	ix		= gameIconCalc_DIV(m_Pos.x);
		iy		= gameIconCalc_DIV(m_Pos.y);
		IsWarp	= false;
		nIcons	= CalcRealIcons();
		//
		ix_fix = 0;
		switch(nIcons)
		{
		case 2:
			break;
		case 3:
			ix_fix++;
			break;
		}
		ix += ix_fix;
		//
		for(cnt1 = 0; cnt1 < nIcons; cnt1++)
		{
			nCode = pMap->GetIconData(ix, iy);
			if(nCode != 0 && nCode != MAPCODE_ID_WALL)
			{
				pObject = CMapServer::GetServer()->FindObjectByHandle(nCode - 1);
				if (pObject != NULL)
				if(pObject->IsKindOf(CMapWarpPoint::CLASS_ID) || pObject->IsKindOf(CMapShopPoint::CLASS_ID))
				{
					IsWarp = true;
					break;
				}
			}
			ix--;
		}
	*/
		// �w�缾�a���ֳt�B�z
		ix = gameIconCalc_DIV(m_Pos.x);
		iy = gameIconCalc_DIV(m_Pos.y);
		IsWarp	= false;
		nCode = pMap->GetIconData(ix, iy);
		if (nCode != 0 && nCode != MAPCODE_ID_WALL)
		{
			pObject = CMapServer::GetServer()->FindObjectByHandle(nCode - 1);
			if (pObject != NULL)
			{
				if (pObject->IsKindOf(CMapWarpPoint::CLASS_ID) || pObject->IsKindOf(CMapShopPoint::CLASS_ID))
				{
					IsWarp = true;
				}
			}
		}

		CMapChar::MoveProc();
		// ����ʰO����s
		if (PartyGetTotal())
		{
			if (!m_nSendPartyPosTivk)
			{
				m_nSendPartyPosTivk = PARTY_UPDATE_POS_TICK;
			}
		}

		// �w�缾�a���ֳt�B�z
		if (!IsWarp)
		{
			if (!IsDead())			// �קK���`�ɶi�J���I�ΰө�
			{
				ix2 = gameIconCalc_DIV(m_Pos.x);
				iy2 = gameIconCalc_DIV(m_Pos.y);
				if ((ix2 != ix) || (iy2 != iy))
				{
					nCode = pMap->GetIconData(ix2, iy2);
					if (nCode != 0 && nCode != MAPCODE_ID_WALL)
					{
						pObject = CMapServer::GetServer()->FindObjectByHandle(nCode - 1);
						if (pObject != NULL)
						{
							if (pObject->IsKindOf(CMapWarpPoint::CLASS_ID))
							{
								// ��i�a�Ϥ����I
								pWarp = (CMapWarpPoint *)pObject;
								if(pWarp->GetTargetMapID() != 0)
								{
									//��ԫe1����
									if(CMapServer::GetServer()->IsWarPrep())
									{
										map_code = pWarp->GetTargetMapID();
										if (pStg = gameStageGetPtr(map_code))
										{
											if (pStg->gstgMode == mapMode_CountryWar)
											{
												pTown = CMapServer::GetServer()->GetTownDataByID(map_code);									
												if (pTown->ptCountryID == 0 || (pTown->ptCountryID &&(GetSaveData()->plrCountryID != pTown->ptCountryID)))
												{
													wsprintf(tmpstr, gameGetResourceName(5000130));
													SendMessage(gameChannel_System, NULL, tmpstr);
													return;
												}

											}
										}
									}

									//��ԹL���I10��CD
#ifdef CWAR_CDTIME
									if(CMapServer::GetServer()->IsWar())
										if( m_TeleportKeepOutTime > CMapServer::GetServer()->GetLoopTime() )
										{
											wsprintf(tmpstr, gameGetResourceName(5000497));
											SendMessage(gameChannel_System, NULL, tmpstr);
											return;
										}
#endif
									
									nColor = pWarp->GetSetColor();
									if (nColor)
									{
										nCharColor = GetCharData()->plrSetColor;
										if (!nCharColor)
											nCharColor = gameGetCountryColor(GetSaveData()->plrCountryID) & 0xffff;
										if (nColor == nCharColor)
										{
											ChangeMap(pWarp->GetTargetMapID(), pWarp->GetTargetPosX(), pWarp->GetTargetPosY(),0,0,pWarp->GetTargetCopyUID());
#ifdef CWAR_CDTIME
											if(CMapServer::GetServer()->IsWar())
												m_TeleportKeepOutTime = CMapServer::GetServer()->GetLoopTime() + 10;
#endif
										}
#ifndef USE_PACKAGE_DATA
										else
											CMapServer::GetServer()->Log("Player(%s) warp color not match(%d,%d)", GetSaveData()->plrName, nColor, nCharColor);
#endif
									}
									else
									{
										ChangeMap(pWarp->GetTargetMapID(), pWarp->GetTargetPosX(), pWarp->GetTargetPosY(),0,0,pWarp->GetTargetCopyUID());
#ifdef CWAR_CDTIME
										if(CMapServer::GetServer()->IsWar())
											m_TeleportKeepOutTime = CMapServer::GetServer()->GetLoopTime() + 10;
#endif
									}
				//					CMapServer::GetServer()->Log("Player(%d, %s) warp to %d,(%d,%d)", GetUID(), GetSaveData()->plrName, pWarp->GetTargetMapID(), pWarp->GetTargetPosX(), pWarp->GetTargetPosY());
								}
							}
							else if (pObject->IsKindOf(CMapShopPoint::CLASS_ID))
							{
								if (!CMapServer::GetServer()->IsMapWar(GetMapCode()))	// ��ԡG��Ԧa�Ϥ��i�i�J
								{
									{
										// ��i�ө�
										pShop = (CMapShopPoint *)pObject;
										if (pShop->GetShopID() != 0)
										{
											if( IsAvailable() ) //xiun ���b�ө����A�S������B�\�u�� 04/06/03 
											{
												// By R61
												EnterShop( pShop->GetShopID() );
												// !!!!!!!
											}
										}
				//						CMapServer::GetServer()->Log("Player(%d, %s) into shop %d", GetUID(), GetSaveData()->plrName, pShop->GetShopID());
									}
								}
							}
						}
					}
				}
			}
		}
	/*	if(!IsWarp)
		{
			ix = gameIconCalc_DIV(m_Pos.x);
			iy = gameIconCalc_DIV(m_Pos.y);
			ix += ix_fix;
			for(cnt1 = 0; cnt1 < nIcons; cnt1++)
			{
				nCode = pMap->GetIconData(ix, iy);
				if(nCode != 0 && nCode != MAPCODE_ID_WALL)
				{
					pObject = CMapServer::GetServer()->FindObjectByHandle(nCode - 1);

                    if(pObject!=this)
                    {
					    if (pObject != NULL)
					    {
						    if (pObject->IsKindOf(CMapWarpPoint::CLASS_ID))
						    {
							    // ��i�a�Ϥ����I
							    pWarp = (CMapWarpPoint *)pObject;
							    if(pWarp->GetTargetMapID() != 0)
								    ChangeMap(pWarp->GetTargetMapID(), pWarp->GetTargetPosX(), pWarp->GetTargetPosY());
							    CMapServer::GetServer()->Log("Player(%d) step on warp point, change to map(%d) [%d,%d]", GetUID(), pWarp->GetTargetMapID(), pWarp->GetTargetPosX(), pWarp->GetTargetPosY());
							    break;
						    }
						    else if (pObject->IsKindOf(CMapShopPoint::CLASS_ID))
						    {
							    // ��i�ө�
							    pShop = (CMapShopPoint *)pObject;
							    if (pShop->GetShopID() != 0)
							    {
									if( IsAvailable() ) //xiun ���b�ө����A�S������B�\�u�� 04/06/03 
									{
										// By R61
										EnterShop( pShop->GetShopID() );
										// !!!!!!!
									}
							    }
							    CMapServer::GetServer()->Log("Player(%d) step on shop point, shop id = %d", GetUID(), pShop->GetShopID());
							    break;
						    }
					    }
                    }
				}
				ix--;
			}
		}
	*/
	}
//	else
//		CMapChar::MoveProc();
}

// �۰ʧֳt�P�_���a�O�_�b��Ԧa�ϡA�u���S�w�禡�ϥ�
long CMapPlayer::PlayerIsMapWar()
{
	if (!m_bIsMapWar)				// ����l
	{
		if (!CMapServer::GetServer()->IsMapWar(GetMapCode()))
		{
			m_bIsMapWar = 1;		// �L���
			return(0);
		}
		else
		{
			m_bIsMapWar = 2;		// �����
			return(1);
		}
	}
	//
	if (m_bIsMapWar == 2)
	{
		return(1);
	}
	return(0);
}

// �L�Įɶ��G�@�� 15���A��Ԧa�� 10��
// no_super_mode = 3 �� true �]�� false(�S���Ϊk)
void CMapPlayer::SetReady(bool IsReady, long no_super_mode)
{
	m_IsReady = IsReady;
	if (IsReady)
	{
		if (no_super_mode == 3)			// 2008/02/21
			m_IsReady = false;
		//
		if (!GetMapCode())		// ����l�a��
		{
			m_nSuperTime = 15*1000;
		}
		else
		{
			if (!m_NoSuperMode)
			{
				if (m_nCharFlags & CHAR_HISTORY_BATTLE)		// ���v�Գ����ʵL��
				{
					m_nSuperTime = 10*1000;
				}
				else
				{
					if (!PlayerIsMapWar())
					{
						if (GetSaveData()->plrWarSuperTimes)
						{
							GetSaveData()->plrWarSuperTimes = 0;
							AutoSaveCharData();
//	CMapServer::GetServer()->Log("�M���L�Ħ���(%d, %d)", m_bIsMapWar, GetMapCode());
						}
					//
//	CMapServer::GetServer()->Log("�L�Įɶ� 15�� 1(%d, %d)", m_bIsMapWar, GetMapCode());
			old:		m_nSuperTime = 15*1000;
					}
					else
					{
						if (GetSaveData()->plrWarSuperTimes <= WAR_TELEPORT_SUPER_TIMES)
						{
							GetSaveData()->plrWarSuperTimes++;
							AutoSaveCharData();
							//
							if (GetSaveData()->plrWarSuperTimes > WAR_TELEPORT_SUPER_TIMES)
							{
//	CMapServer::GetServer()->Log("�L�Įɶ� 15�� 2(%d, %d)", m_bIsMapWar, GetMapCode());
								goto old;
							}
							//
							m_nSuperTime = 4*1000;
//	CMapServer::GetServer()->Log("�L�Įɶ� 4��(%d, %d)", m_bIsMapWar, GetMapCode());
						}
						else
							goto old;
					}
				}
			}
			else
			{
				m_bIsMapWar = 0;		// �U�����s�]�w��ԧֳt�������A
				m_nSuperTime = 0;
				//
				if (!no_super_mode)
					m_NoSuperMode = 0;
			}
		}
	}
	else
	{
		m_bIsMapWar = 0;		// �U�����s�]�w��ԧֳt�������A
		m_NoSuperMode = (char)no_super_mode;
		m_nSuperTime = 15*1000;
	}
	//
	ClearAccelerateRecord();
	//
	//
	CarryNPC_Update();
	// ����ʰO����s
	// ���m��Ƨi�D�P�@�� Map Server �䥦����
	if (GetMapCode())
		CMapServer::GetServer()->SendPartyPlayerPos((CMapPlayer *)this);
}

void CMapPlayer::ProcSpecialStatus(unsigned long sec, long upart2_type, long no_save, long zero_send)
{
	CMapCharMsg * pMsg;

	if (zero_send)
	{
		if (sec)
			goto skip;
	}
	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	
	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add((unsigned short)upart2_type, 0, sec);

	pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
	// �۰ʦs��
skip:
	if (!no_save)
		AutoSaveCharData();
}

// �n�J�ɩΤ��� MapServer �ɳq���S�����A
void CMapPlayer::SendSpecialStatus()
{
	long type;
	unsigned long sec;
	long left_sec;

	type = UPART2_TYPE_STATUS_GOOD_LUCK;
	sec = GetSaveData()->plrStsTime_GoodLuck;
	if (sec)
		ProcSpecialStatus(sec, type, 1);
	//
	type = UPART2_TYPE_STATUS_ADD_EXP;
	sec = GetSaveData()->plrStsTime_AddExp;
	if (sec)
		ProcSpecialStatus(sec, type, 1);
	//
	type = UPART2_TYPE_STATUS_ADD_SKILL_EXP;
	sec = GetSaveData()->plrStsTime_AddSkillExp;
	if (sec)
		ProcSpecialStatus(sec, type, 1);
	//
	type = UPART2_TYPE_STATUS_ADD_HONOR;
	sec = GetSaveData()->plrStsTime_AddHonor;
	if (sec)
		ProcSpecialStatus(sec, type, 1);
	//
	type = UPART2_TYPE_STATUS_ADD_CNPC_EXP;
	sec = GetSaveData()->plrStsTime_AddCNPCExp;
	if (sec)
		ProcSpecialStatus(sec, type, 1);
	//
	type = UPART2_TYPE_STATUS_UNDEAD_TIME;
	sec = GetSaveData()->plrStsTime_Undead;
	if (sec)
		ProcSpecialStatus(sec, type, 1);
	//
	type = UPART2_TYPE_STATUS_SOLDIER_UNDEAD;
	sec = GetSaveData()->plrStsTime_SoldierUndead;
	if (sec)
		ProcSpecialStatus(sec, type, 1);

	// �Z��ɶ�
	if (GetSaveData()->plrSoulWID && GetSaveData()->plrSoulWID != 0xFFFF )
	{
		type = UPART2_TYPE_STATUS_SOUL_TIME;
		left_sec = GetSaveData()->plrSoulTime - CMapServer::GetServer()->GetLoopTime();
		if (left_sec > 0)
			ProcSpecialStatus(left_sec, type, 1);
	}
	// �]��
	sec = GetSaveData()->plrStsTime_Month_Exp;
	if (sec)
		ProcSpecialStatus(sec, UPART2_TYPE_STATUS_MONTH_TIME, 1);
	sec = GetSaveData()->plrStsTime_Month_Skill_Exp;
	if (sec)
		ProcSpecialStatus(sec, UPART2_TYPE_STATUS_MONTH_SKILL_TIME, 1);
	sec = GetSaveData()->plrStsTime_Month_CNPC_Exp;
	if (sec)
		ProcSpecialStatus(sec, UPART2_TYPE_STATUS_MONTH_CNPC_TIME, 1);

	sec = GetSaveData()->plrStsTime_Month_GoodLuck;
	if (sec)
		ProcSpecialStatus(sec, UPART2_TYPE_STATUS_MONTH_GOODLUCK, 1);
	sec = plrStsTime_Month_AddHonor_field(GetSaveData());
	if (sec)
		ProcSpecialStatus(sec, UPART2_TYPE_STATUS_ADD_HONOR, 1);
}

// uid = 0 �ۤv�A�_�h�O�p�L
void CMapPlayer::SendStatusDisappear(unsigned long uid, struct plrDATA * pData)
{
	struct MAP_STATUS_TIME_NOTIFY nData;

	if (!uid)
		uid = GetUID();
	nData.uid = uid;
	nData.nStatusDisappear = pData->plrStatusDisappear;
	nData.nStatus2Disappear = pData->plrStatus2Disappear;
	//
	::msgSendData( GetClientID(), 0, PROTO_MAP_STATUS_TIME_NOTIFY, (char *)&nData, sizeof(nData), 0 );
}

long CMapPlayer::Soul_CanEquipHorseCNPC()
{
	if (m_nIsChallenger & 4)
	{
		if (m_nIsChallenger & 1)		// bit 0 = Yes/No, bit 1 = Can Attack
			return(1);
		return(0);
	}
	return(1);
}

long CMapPlayer::Soul_CanAttack()
{
	if(CMapServer::GetServer()->IsCrossServer())
		return(1);

	if (m_nIsChallenger & 4)
	{
		if (m_nIsChallenger & 2)		// bit 0 = Yes/No, bit 1 = Can Attack, bit 2 = enter soul battle map
			return(1);
		return(0);
	}
	return(1);
}

// �]�w�Z��J�����A(���]�w can attack)
void CMapPlayer::SetSoulBattleStatus()
{
	m_nIsChallenger = 0;
	if (gameStageGetStageMode(GetMapCode()) == mapMode_SoulBattle)
	{
		m_nIsChallenger |= 4;
		if (CMapServer::GetServer()->Soul_IsChallenger(this, GetMapCode()))
		{
			m_nIsChallenger |= 1;
		}
	}
//	CMapServer::GetServer()->Log("%s mode = %d", GetName(), m_nIsChallenger);
}

long CMapPlayer::SetSpecialStatus(unsigned long sec, long upart2_type, unsigned long power)
{
	long ok = 0;

	if (sec && power)
	{
#ifdef USE_DOUBLE_ITEM_TIME_ADD			// �]�t�P�_�O�_�i�|�[�ɶ�
		unsigned short * pTime = NULL;
		unsigned char * pPower = NULL;
		long * pMonth = NULL;
		unsigned long sec2;

		switch(upart2_type)
		{
		case UPART2_TYPE_STATUS_GOOD_LUCK:
			pTime = &GetSaveData()->plrStsTime_GoodLuck;
			pPower = &GetSaveData()->plrStsPower_GoodLuck;
			pMonth = &GetSaveData()->plrStsTime_Month_GoodLuck;
	//plrStsTime_Month_Skill_Exp;				// �]��: �ޯ�g��
	//plrStsTime_Month_CNPC_Exp;					// �]��: �h�L�g��
	//plrStsTime_Month_Exp;						// �]��: �g��
			//
			if (sec > 65535)
			{
				if (GetSpecialStatus_GoodLuck(1))
					return(0);
				upart2_type = UPART2_TYPE_STATUS_MONTH_GOODLUCK;
				//
	do_month:
				sec = CMapServer::GetServer()->GetLoopTime() + sec;
				if (pPower)
					*pPower = (unsigned char)power;
				if (pMonth)
					*pMonth = sec;
				ProcSpecialStatus(sec, upart2_type);
  				SendSpecialTimeTable();
				return(1);
			}
			//
	do_set:
			// ���� power �O�_�۲�
			if (pMonth)
			{
				if (*pMonth)					// �]��ɶ�����
					return(0);
			}
			if (pPower)
			{
				if (*pTime)						// �ɶ�����
				{
					if (*pPower != (unsigned char)power)
						return(0);
				}
			}
			//
			sec = sec + (unsigned long)*pTime;
			if (sec > (unsigned long)SP_STATUS_MAX_HOUR_SEC)
				return(0);
			*pTime = (unsigned short)sec;
			if (pPower)
				*pPower = (unsigned char)power;
			ok++;
			break;
		case UPART2_TYPE_STATUS_ADD_EXP:
			pTime = &GetSaveData()->plrStsTime_AddExp;
			pPower = &GetSaveData()->plrStsPower_AddExp;
			pMonth = &GetSaveData()->plrStsTime_Month_Exp;
			if (sec > 65535)
			{
				if (GetSpecialStatus_AddExp(1))
					return(0);
				upart2_type = UPART2_TYPE_STATUS_MONTH_TIME;
				goto do_month;
			}
			goto do_set;
			break;
		case UPART2_TYPE_STATUS_ADD_CNPC_EXP:
			pTime = &GetSaveData()->plrStsTime_AddCNPCExp;
			pPower = &GetSaveData()->plrStsPower_AddCNPCExp;
			pMonth = &GetSaveData()->plrStsTime_Month_CNPC_Exp;
			if (sec > 65535)
			{
				if (GetSpecialStatus_AddCNPCExp(1))
					return(0);
				upart2_type = UPART2_TYPE_STATUS_MONTH_CNPC_TIME;
				goto do_month;
			}
			goto do_set;
			break;
		case 0xffffffff:		// �S��: �g�� + �p�L�g��
			if (sec > 65535)
			{
				if (GetSpecialStatus_AddExp(1) || GetSpecialStatus_AddCNPCExp(1))
					return(0);
				sec = CMapServer::GetServer()->GetLoopTime() + sec;
				GetSaveData()->plrStsPower_AddExp = (unsigned char)power;
				GetSaveData()->plrStsTime_Month_Exp = sec;
				ProcSpecialStatus(sec, UPART2_TYPE_STATUS_MONTH_TIME);
				//
				GetSaveData()->plrStsPower_AddCNPCExp = (unsigned char)power;
				GetSaveData()->plrStsTime_Month_CNPC_Exp = sec;
				ProcSpecialStatus(sec, UPART2_TYPE_STATUS_MONTH_CNPC_TIME);
				SendSpecialTimeTable();
				return(1);
			}
			//
			if (GetSaveData()->plrStsTime_Month_Exp || GetSaveData()->plrStsTime_Month_CNPC_Exp)		// �]��ɶ�����
				return(0);
			if (GetSaveData()->plrStsTime_AddExp)				// �ɶ�����
			{
				if (GetSaveData()->plrStsPower_AddExp != power)
					return(0);
			}
			if (GetSaveData()->plrStsTime_AddCNPCExp)			// �ɶ�����
			{
				if (GetSaveData()->plrStsPower_AddCNPCExp != power)
					return(0);
			}
			sec2 = sec + (unsigned long)GetSaveData()->plrStsTime_AddExp;
			if (sec2 > (unsigned long)SP_STATUS_MAX_HOUR_SEC)
				return(0);
			sec = sec + (unsigned long)GetSaveData()->plrStsTime_AddCNPCExp;
			if (sec > (unsigned long)SP_STATUS_MAX_HOUR_SEC)
				return(0);
			GetSaveData()->plrStsTime_AddExp = (unsigned short)sec2;
			GetSaveData()->plrStsTime_AddCNPCExp = (unsigned short)sec;
			GetSaveData()->plrStsPower_AddExp = (unsigned char)power;
			GetSaveData()->plrStsPower_AddCNPCExp = (unsigned char)power;
			ok++;
			break;
		case UPART2_TYPE_STATUS_ADD_SKILL_EXP:
			pTime = &GetSaveData()->plrStsTime_AddSkillExp;
			pPower = &GetSaveData()->plrStsPower_AddSkillExp;
			pMonth = &GetSaveData()->plrStsTime_Month_Skill_Exp;
			if (sec > 65535)
			{
				if (GetSpecialStatus_AddSkillExp(1))
					return(0);
				upart2_type = UPART2_TYPE_STATUS_MONTH_SKILL_TIME;
				goto do_month;
			}
			goto do_set;
			break;
		case UPART2_TYPE_STATUS_ADD_HONOR:
			pTime = &GetSaveData()->plrStsTime_AddHonor;
			pPower = &GetSaveData()->plrStsPower_AddHonor;
			pMonth = &plrStsTime_Month_AddHonor_field(GetSaveData());
			if (sec > 65535)
			{
				if (GetSpecialStatus_AddHonor(1))
					return(0);
				upart2_type = UPART2_TYPE_STATUS_ADD_HONOR;
				goto do_month;
			}
			goto do_set;
			break;
		case UPART2_TYPE_STATUS_UNDEAD_TIME:
			pTime = &GetSaveData()->plrStsTime_Undead;
			//pPower = &GetSaveData()->;
			//pMonth = &GetSaveData()->;
			if (sec > 65535)
			{
			//	if (GetSpecialStatus_Undead(1))						// �|���}�񦹥\��
					return(0);
			//	upart2_type = UPART2_TYPE_STATUS_MONTH_;
			//	goto do_month;
			}
			goto do_set;
			break;
		case UPART2_TYPE_STATUS_SOLDIER_UNDEAD:
			pTime = &GetSaveData()->plrStsTime_SoldierUndead;
			//pPower = &GetSaveData()->;
			//pMonth = &GetSaveData()->;
			if (sec > 65535)
			{
			//	if (GetSpecialStatus_SoldierUndead(1))				// �|���}�񦹥\��
					return(0);
			//	upart2_type = UPART2_TYPE_STATUS_MONTH_;
			//	goto do_month;
			}
			goto do_set;
			break;
		}
#else
		switch(upart2_type)
		{
		case UPART2_TYPE_STATUS_GOOD_LUCK:
			if (!GetSaveData()->plrStsTime_GoodLuck)
			{
				ok++;
				GetSaveData()->plrStsTime_GoodLuck = sec;
				GetSaveData()->plrStsPower_GoodLuck = power;
			}
			break;
		case UPART2_TYPE_STATUS_ADD_EXP:
			if (!GetSaveData()->plrStsTime_AddExp)
			{
				ok++;
				GetSaveData()->plrStsTime_AddExp = sec;
				GetSaveData()->plrStsPower_AddExp = power;
			}
			break;
		case UPART2_TYPE_STATUS_ADD_SKILL_EXP:
			if (!GetSaveData()->plrStsTime_AddSkillExp)
			{
				ok++;
				GetSaveData()->plrStsTime_AddSkillExp = sec;
				GetSaveData()->plrStsPower_AddSkillExp = power;
			}
			break;
		case UPART2_TYPE_STATUS_ADD_HONOR:
			if (!GetSaveData()->plrStsTime_AddHonor)
			{
				ok++;
				GetSaveData()->plrStsTime_AddHonor = sec;
				GetSaveData()->plrStsPower_AddHonor = power;
			}
			break;
		case UPART2_TYPE_STATUS_ADD_CNPC_EXP:
			if (!GetSaveData()->plrStsTime_AddCNPCExp)
			{
				ok++;
				GetSaveData()->plrStsTime_AddCNPCExp = sec;
				GetSaveData()->plrStsPower_AddCNPCExp = power;
			}
			break;
		case UPART2_TYPE_STATUS_UNDEAD_TIME:
			if (!GetSaveData()->plrStsTime_Undead)
			{
				ok++;
				GetSaveData()->plrStsTime_Undead = sec;
			}
			break;
		case UPART2_TYPE_STATUS_SOLDIER_UNDEAD:
			if (!GetSaveData()->plrStsTime_SoldierUndead)
			{
				ok++;
				GetSaveData()->plrStsTime_SoldierUndead = sec;
			}
			break;
		}
#endif
		//
		if (ok)
		{
			ProcSpecialStatus(sec, upart2_type);
			//
			SendSpecialTimeTable();
		}
	}
	return(ok);
}

long CMapPlayer::CancelSpecialStatus(long type)
{
	long r = 0;

	switch(type)
	{
	case magic_GOOD_LUCK:			// ��B
		if (GetSpecialStatus_GoodLuck(1))
		{
			GetSaveData()->plrStsTime_GoodLuck = 0;
			GetSaveData()->plrStsPower_GoodLuck = 0;
			GetSaveData()->plrStsTime_Month_GoodLuck = 0;
			r++;
		}
		break;
	case magic_ADD_EXP:				// �g��
		if (GetSpecialStatus_AddExp(1))
		{
			GetSaveData()->plrStsTime_AddExp = 0;
			GetSaveData()->plrStsPower_AddExp = 0;
			GetSaveData()->plrStsTime_Month_Exp = 0;
			r++;
		}	
		break;
	case magic_ADD_SKILL_EXP:		// �ޯ�
		if (GetSpecialStatus_AddSkillExp(1))
		{
			GetSaveData()->plrStsTime_AddSkillExp = 0;
			GetSaveData()->plrStsPower_AddSkillExp = 0;
			GetSaveData()->plrStsTime_Month_Skill_Exp = 0;
			r++;
		}
		break;
	case magic_ADD_HONOR:			// �\��
		if (GetSpecialStatus_AddHonor(1))
		{
			GetSaveData()->plrStsTime_AddHonor = 0;
			GetSaveData()->plrStsPower_AddHonor = 0;
			plrStsTime_Month_AddHonor_field(GetSaveData()) = 0;
			r++;
		}
		break;
	case magic_ADD_CNPC_EXP:		// �p�L�g��
		if (GetSpecialStatus_AddCNPCExp(1))
		{
			GetSaveData()->plrStsTime_AddCNPCExp = 0;
			GetSaveData()->plrStsPower_AddCNPCExp = 0;
			GetSaveData()->plrStsTime_Month_CNPC_Exp = 0;
			r++;
		}
		break;
	case magic_UNDEAD:				// �K����
		if (GetSpecialStatus_Undead(1))
		{
			GetSaveData()->plrStsTime_Undead = 0;
			//GetSaveData()-> = 0;
			r++;
		}
		break;
	case magic_SOLDIER_UNDEAD:		// �p�L�K��
		if (GetSpecialStatus_SoldierUndead(1))
		{
			GetSaveData()->plrStsTime_SoldierUndead = 0;
			//GetSaveData()-> = 0;
			r++;
		}
		break;
	}
	//
	if (r)
	{
		SendSpecialTimeTable();
		AutoSaveCharData();
	}
	return(r);
}

long CMapPlayer::GetWebIP_AddExp()
{
	if (m_nWebIP_Flags & LOGIN_CHAR_FLAGS_WEBIP_EXP)		// �g��
	{
#ifdef USE_WEB_IP_1_DOT_2
		return(120);
#else
		return(2);
#endif
	}
	return(0);
}

long CMapPlayer::GetWebIP_AddCNPCExp()
{
	if (m_nWebIP_Flags & LOGIN_CHAR_FLAGS_WEBIP_SOLEXP)		// �p�L�g��
		return(2);
	return(0);
}

long CMapPlayer::GetWebIP_AddSkillExp()
{
	if (m_nWebIP_Flags & LOGIN_CHAR_FLAGS_WEBIP_SKILLEXP)	// �ޯ��I
		return(2);
	return(0);
}

long CMapPlayer::GetWebIP_AddDrop()
{
	if (m_nWebIP_Flags & LOGIN_CHAR_FLAGS_WEBIP_DROP)		// ���_
		return(1);
	return(0);
}

long CMapPlayer::CalcDrop()
{
	long r;

	r = GetSpecialStatus_GoodLuck();
	if (GetWebIP_AddDrop())
	{
#ifdef USE_WEB_IP_1_DOT_2
		if (!r)
			r = 1;
		r = r * 120;
#else
		r = 2;
#endif
	}
	return(r);
}

long CMapPlayer::SPCMode_CN_CheckUseItem()
{
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	if (m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
		return(0);
#endif
	return(1);
}

long CMapPlayer::GetSpecialStatus_GoodLuck(long is_detect)
{
	long r, pwr;

	if (GetSaveData()->plrStsTime_GoodLuck || GetSaveData()->plrStsTime_Month_GoodLuck)
	{
		pwr = GetSaveData()->plrStsPower_GoodLuck;
	}
	else
		pwr = 0;
	//
	if (!is_detect)
	{
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			return(0);
#endif
		//
		r = CMapServer::GetServer()->GetActivityStatus(1);	// �S������
		if (r)
			return(r + pwr);
	}
	return(pwr);
}

// �p��ұo�g���
unsigned long CMapPlayer::CalcGetExp(unsigned long exp)
{
	long power;
	unsigned long newExp = 0;
	unsigned long newExpPer = 0;

#if defined USE_EXPFLAG
	newExpPer = this->GetCharData()->plrNewExp;
	if (newExpPer && newExpPer <= 200)
	{
		newExp = (int)(exp * (float)(newExpPer / 100.f)); // �B�I�B��A�Ʊ椣�|���C�t��
	}
#endif

	if (power = GetSpecialStatus_AddExp())
	{
#ifdef USE_WEB_IP_1_DOT_2
		if (power > 100)
		{
			exp = exp * power / 100;
		}
		else
#endif
		exp *= power;
	}
	exp += newExp;
	return(exp);
}

long CMapPlayer::GetSpecialStatus_AddExp(long is_detect)
{
	long r, pwr;

	//
	if (GetSaveData()->plrStsTime_AddExp || GetSaveData()->plrStsTime_Month_Exp)
	{
		pwr = GetSaveData()->plrStsPower_AddExp;
	}
	else
		pwr = 0;
	//
	if (!is_detect)
	{
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			return(0);
#endif
		//
		pwr += CMapServer::GetServer()->GetActivityStatus(4);	// �S������
		//
		r = GetWebIP_AddExp();
		if (r)
		{
			if (!pwr)
				return(r);
			pwr *= r;
		}
	}
	return(pwr);
}

long CMapPlayer::GetSpecialStatus_AddSkillExp(long is_detect)
{
	long r;

	//
	r = GetWebIP_AddSkillExp();
	if (GetSaveData()->plrStsTime_AddSkillExp || GetSaveData()->plrStsTime_Month_Skill_Exp)
	{
		if (is_detect)
			return(1);
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			return(r);
#endif
		if (r)
			return(GetSaveData()->plrStsPower_AddSkillExp * r);
		return(GetSaveData()->plrStsPower_AddSkillExp);
	}
	else
	{
		if (is_detect)
			return(0);
	}
	return(r);
}

long CMapPlayer::GetSpecialStatus_AddHonor(long is_detect)
{
	long r, pwr;
	long cur;

	cur = CMapServer::GetServer()->GetLoopTime();
	pwr = 0;
	if (plrStsTime_Month_AddHonor_field(GetSaveData()) > cur)
		pwr = GetSaveData()->plrStsPower_AddHonor;
	else if (GetSaveData()->plrStsTime_AddHonor)
		pwr = GetSaveData()->plrStsPower_AddHonor;
	//
	if (!is_detect)
	{
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			return(0);
#endif
		//
		r = CMapServer::GetServer()->GetActivityStatus(3);	// �S������
		if (r)
			return(r + pwr);
	}
	return(pwr);
}

long CMapPlayer::GetSpecialStatus_AddCNPCExp(long is_detect)
{
	long r, pwr;

	//
	if (GetSaveData()->plrStsTime_AddCNPCExp || GetSaveData()->plrStsTime_Month_CNPC_Exp)
	{
		pwr = GetSaveData()->plrStsPower_AddCNPCExp;
	}
	else
		pwr = 0;
	//
	if (!is_detect)
	{
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			return(0);
#endif
		//
		pwr += CMapServer::GetServer()->GetActivityStatus(2);	// �S������
		//
		r = GetWebIP_AddCNPCExp();
		if (r)
		{
			if (!pwr)
				return(r);
			pwr *= r;
		}
	}
	return(pwr);
}

long CMapPlayer::GetSpecialStatus_Undead(long is_detect)
{
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	if (!is_detect)
	{
		if (m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			return(0);
	}
#endif
	if (GetSaveData()->plrStsTime_Undead)
	{
		return(GetSaveData()->plrStsTime_Undead);
	}
	return(0);
}

long CMapPlayer::GetSpecialStatus_SoldierUndead(long is_detect)
{
 #ifdef USE_SPCMODE_CN_ADULT_LIMIT
	if (!is_detect)
	{
		if (m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			return(0);
	}
 #endif
	if (GetSaveData()->plrStsTime_SoldierUndead)
	{
		return(GetSaveData()->plrStsTime_SoldierUndead);
	}

	return(0);
}

void CMapPlayer::ClearAccelerateRecord(long mode)
{
	m_nSpeedCheckTime = 0;				// �M�������ɶ�
	if (!mode)
	{
		m_nSpeedCheckCountTime = 0;		// �L�^���ɶ�
		//m_nSpeedCheckSkipCount = 0;
	}
	//
	m_nSpeedCheckTick = 0;				// �зǮɶ�
	m_nSpeedCheckOdds = 0;				// �ֿn�~�t�A�����Ȫ��ܥ[�t, �t�Ȫ��ܴ�t
	m_nSpeedCheckCount = 0;				// Client �t�׭p��
}

void CMapPlayer::Update(long no_move)
{
	unsigned long loop_tick;
	long sts;
	//long nSecond;
 #ifndef NO_BOT_ENEMY
	unsigned long map_code;
	//CMapCtrl * pMapCtrl;
	CMapManager * pMapMgr;
 #endif

	if (CMapServer::GetServer()->IsObjectDeleted(GetHandle()))
	{
		SendMsgToPlayer();
		OuterUpdate();
		return;
	}
	CMapChar::Update();

	OuterUpdate();

	// 游戏时间累加计算
	loop_tick = CMapServer::GetServer()->GetLoopTickCount();
/*	m_nPlayTime += loop_tick;
	if(m_nPlayTime >= 1000)
	{
		nSecond = m_nPlayTime / 1000;
		GetSaveData()->plrPlayTime += nSecond;
		m_nPlayTime -= (nSecond * 1000);
		//
		if (++m_nCheckMissionCount >= 20)
		{
			m_nCheckMissionCount = 0;
			ProcessMissionRepeat();
		}
		// �j�����I�g
		if (m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			if(IsReady())
				SpecModeCN_Process(nSecond);
		}
	} */

	if(IsReady())
	{
		sts = GetStateProc();
		if ((sts == CMapObject::STATE_LEAVE_MAP) || (sts == CMapObject::STATE_DELETE))
		{
			ClearAccelerateRecord();
		//	m_nSpeedCheckTick = 0;
		//	m_nSpeedCheckTime = 0;
		//	m_nSpeedCheckCountTime = 0;
		//	m_nSpeedCheckOdds = 0;
		//	m_nSpeedCheckCount = 0;
		}
		else
		{
 #ifndef NO_BOT_ENEMY
			// �~�����Ϊ���
			if (m_nBotEnemyAppearTime)
			{
				m_nBotEnemyAppearTime -= loop_tick;
				if (m_nBotEnemyAppearTime <= 0)
				{
					m_nBotEnemyAppearTime = gameGetRandomRange(BOT_ENEMY_APPEAR_MIN_TIME, BOT_ENEMY_APPEAR_MAX_TIME) * 1000;
					// ��ԮɡB�]�V���S�w�a�ϡB���Ťp��20 ���X�{
					if (CMapServer::GetServer()->IsBotEnemyCheck)
					{
						if (GetSaveData()->plrLevel >= BOT_ENEMY_APPEAR_MIN_LEVEL)
						{
							map_code = GetSaveData()->plrMapCode;
							pMapMgr = CMapServer::GetServer()->GetMapManager();
							if (pMapMgr)
							{
								if (pMapMgr->CheckBotEnemyMapCode(map_code))
								{
									if (!pMapMgr->CreateBotEnemy(map_code, GetSaveData()->plrPosX, GetSaveData()->plrPosY))
									{
										m_nBotEnemyAppearTime = BOT_ENEMY_APPEAR_WAIT_TIME * 1000;
									}
								}
							}
						}
					}
				}
			}
 #endif
			// �_������
#ifdef USE_RESURRECT_DELAY
			//if (IsDead())
			sts = GetShowData()->plrStatus;
			if (sts & effFun_DEAD)
			{
				if (!(sts & effFun_MARK_RESURRECT))
				{
					if (m_nResurrectDelay > loop_tick)
					{
						m_nResurrectDelay -= loop_tick;
					}
					else
					{
						m_nResurrectDelay = 0;
						pStatus_Set(effFun_MARK_RESURRECT, 1);
					}
				}
			}
#endif
			//
			if (m_nChangeWAWAShowTick > loop_tick)		// ���Ȫ��ܳQ����
			{
				m_nChangeWAWAShowTick -= loop_tick;
			}
			else
			{
				m_nChangeWAWAShowTick = 0;
				if (m_bIsChangeWAWA)
				{
					if (IsMoving() || IsCasting() || IsAttacking() || IsDead())		// ���ʡB�I�k�B���������B�z(�[�J���`)
					{
						m_nChangeWAWAShowTick = 500;
					}
					else
					{
						struct plrDATA_SAVE * pSave;

						// �ˬd�O�_���˳Ƨ�˹D��
						pSave = GetSaveData();
#ifdef WAWA_SHOW_SEPARATE
				if (pSave->plrWawaShowWeaponR.nItemCode || pSave->plrWawaShowArmor.nItemCode || pSave->plrWawaShowHead.nItemCode || 
							pSave->plrWawaShowFoot.nItemCode || pSave->plrWawaShowP.nItemCode || pSave->plrWawaShowHorse.nItemCode || 
							pSave->plrWawaShowAllShape.nItemCode)
#else
				if (pSave->plrWawaWeaponR.nItemCode || pSave->plrWawaArmor.nItemCode || pSave->plrWawaHead.nItemCode || pSave->plrWawaFoot.nItemCode
							|| pSave->plrWawaP.nItemCode || pSave->plrWawaHorse.nItemCode || pSave->plrWawaAllShape.nItemCode)
						
#endif
						{
							UpdateShowData(1);
							m_nChangeWAWAShowTick = 1000 * 5;	// 5����
						}
						m_bIsChangeWAWA = false;
					}
				}
			}
			//
			if (m_nSuperTime > (unsigned long)loop_tick)
			{
				m_nSuperTime -= loop_tick;
			}
			else
				m_nSuperTime = 0;
			//
#ifndef NO_CLIENT_SPEED_CHECK
			// �[�t�ˬd�p��
			m_nSpeedCheckTick += loop_tick; // CMapServer::GetServer()->GetLoopTickCount();
			m_nSpeedCheckTime += loop_tick; // CMapServer::GetServer()->GetLoopTickCount();
			m_nSpeedCheckCountTime += loop_tick;

 #ifdef NO_TIGHT_CLIENT_SPEED_CHECK
			//if (m_nSpeedCheckCountTime >= (20 * 1000))		// 20 ���������n����q��
			if (m_nSpeedCheckCountTime >= ((gameSendSpeedPacketTime*4) * 1000 + 500))
			{
			//	if (m_nSpeedCheckCount < 3)					// ���ˬd�A������N�n
 #else
			//if (m_nSpeedCheckCountTime >= (12 * 1000))		// 15 ���������n���� 2�ӳq��
			if (m_nSpeedCheckCountTime >= ((gameSendSpeedPacketTime*3) * 1000 + 500))
			{
				if (m_nSpeedCheckCount < 2)
 #endif
				{
					CMapServer::GetServer()->Log("�����ٶ���������Ӧ��! ���(%d, %s)", GetUID(), GetSaveData()->plrName);
					CMapServer::GetServer()->SendLogMessage_System(this, LOGTYPE_SYS_SUB_SPEED);

					CMapServer::GetServer()->Disconnect_ClearAllRecord( GetUID(), 0 );
					SaveAllData();
					//SendMsgToClientByID(94);
					SetExitCode(CMapPlayer::EXIT_DATA_ERROR);
					CMapServer::GetServer()->DeleteObject(GetHandle());
				//	CMapServer::GetServer()->KickClient(GetClientID());
					return;
				}
			}
#endif
		}

#ifdef FORCE_BATTLE_FIELD
		if (m_nArmyBattleFieldCheckTick > loop_tick)
		{
			m_nArmyBattleFieldCheckTick -= loop_tick;
		}
 #if _DEBUG
		else
 #else
		else if (!IsGM(0))
 #endif
		{
			m_nArmyBattleFieldCheckTick = 5000;
			if (!IsDead() && GetMapCtrl() && GetMapCtrl()->CheckMapFlag(gameMAPFLAG_FORCE_BATTLEFIELD))
			{
				if (!m_nTeleportMapCode && !m_nTeleport2MapCode)
				{
					if (CMapServer::GetServer()->GetLoopTime() >= m_nArmyBattleFieldDueTime)
					{
						CMapServer::GetServer()->ChangeSaveMap(this, GetSaveData()->plrSaveMapCode, GetSaveData()->plrSavePosX, GetSaveData()->plrSavePosY);
					}
					else
					{
						unsigned short get_map_code = gameGetMyForceBattleFieldMap(GetSaveData()->plrCountryID, GetMapCode());
						if (GetMapCode() == get_map_code)
						{
						}
						else if (get_map_code)
						{
							ChangeMap(get_map_code, GetPosX(), GetPosY());
						}
						else
						{
							CMapServer::GetServer()->ChangeSaveMap(this, GetSaveData()->plrSaveMapCode, GetSaveData()->plrSavePosX, GetSaveData()->plrSavePosY);
						}
					}
				}
			}
		}
#endif
	}
}

	// 更新 Client 进入限时地图时间
void EnterStage_SendClientTimeNotify(CMapPlayer * pMapPlayer)
{
	CMapCharMsg * pMsg;

	pMsg = pMapPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	if (pMsg)
	{
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ENTER_STAGE_NOTIFY, (pMapPlayer->m_nPlayerFlags & PLAYER_FLAGS_ENTER_STAGE) ? 1 : 0, pMapPlayer->GetSaveData()->plrEnterStageID, pMapPlayer->GetSaveData()->plrEnterStageTime); // data1=stage id, data2=time

		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
	}
}

// return: -1 ���~����, 1 ���\���ݭ���(��50��), 0 ���Ѷ�����(��5��)
long InnerPlayerAutoTeleportProcess(CMapPlayer * pPlayer, long map_code, long x, long y)
{
	register struct plrDATA_SAVE * pSave;
	long r;

	if ((map_code & 0xffff) == 0xffff)			// �̷Ӧa�϶ǰe�]�w
	{
		if (!pPlayer->ChangeMapInStageMapPos())
		{
			r = -1;
#ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("SERIOUS: Map space no teleport setting(%d,%s)(%d)", pPlayer->GetUID(), pPlayer->GetName(), pPlayer->GetMapCode());
#endif
			//
do_stage_time:
			if (pPlayer->m_nPlayerFlags & PLAYER_FLAGS_ENTER_STAGE)
			{
				pSave = pPlayer->GetSaveData();
				//
				if (pSave->plrEnterStageTime > 0)
				{
					if (pSave->plrEnterStageTime <= 2)
					{
						pSave->plrEnterStageTime = 0;
						pPlayer->AutoSaveCharData();
						//
						EnterStage_SendClientTimeNotify(pPlayer);
					}
				}
			}
			return(r);
		}
		else
		{
			r = 1;
			goto do_stage_time;
		}
	}
	else
	{
		//pPlayer->ChangeMap(map_code, map_x, map_y);
		if (!CMapServer::GetServer()->ChangeSaveMap(pPlayer, map_code, x, y))
		{
#ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("SERIOUS: Map space teleport fail(%d,%s)(%d)(%d,%d,%d)", pPlayer->GetUID(), pPlayer->GetName(), pPlayer->GetMapCode(), map_code, x, y);
#endif
			return(0);
		}
		else
		{
			r = 1;
			goto do_stage_time;
		}
	}
	return(0);
}

void CMapPlayer::OuterUpdate(void)
{
	unsigned long	nTick;
	unsigned long	nLoadTime;
	unsigned long	nSaveTime;
	bool			IsLoading;
	bool			IsSaving;
	int				cnt1;
	long			lCurtime;
	struct proto_COMM nComm;
	register struct plrDATA_SAVE * pSave;
	long nSecond;
	long r;
#ifdef TW_PVP_MODE
	struct gs_StageData * pStg;
#endif

	nTick = CMapServer::GetServer()->GetLoopTickCount();
	lCurtime = CMapServer::GetServer()->GetLoopTime();

	// �O��
	if (m_nOuterDueTime)
	{
		if (lCurtime >= m_nOuterDueTime)
		{
#ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG: Player outer time-out kick (%d,%s)", GetUID(), GetName());
#endif
			CMapServer::GetServer()->KickPlayer(this);
			m_nOuterDueTime = 0;
			return;
		}
	}

	// 自动存档
	if (IsReady())
	{
		/* 每 tick 校验：勿放在 m_SaveLoadTick 每秒块内，否则队友卷进图可能长时间不踢 */
		TimedMapPass_EnforceExclusiveMap();
		if (m_nAutoSaveTick > (long)nTick)
		{
			m_nAutoSaveTick -= nTick;
		}
		else
		{
			m_nAutoSaveTick = AUTO_SAVE_DATA_TICK;
	//CMapServer::GetServer()->Log("Player(%d) auto save ...", GetUID());
			if (m_bSaveDataFlag)
			{
				if (!SaveCharData())
				{
					m_bSaveItemFlag = false;
					m_bSaveNPCFlag = false;
				}
				else
					m_bSaveDataFlag = false;
			}
			if (m_bSaveItemFlag)
			{
				SaveItemData();
				m_bSaveItemFlag = false;
			}
			if (m_bSaveNPCFlag)
			{
				SaveNPCData();
				m_bSaveNPCFlag = false;
			}
			if (m_bSaveSkillFlag)
			{
				SaveSkillData();
				m_bSaveSkillFlag = false;
			}
		}
		// �S���X��
		if (m_nPlayerFlags & PLAYER_FLAGS_SHIP_ARRIVE)
		{
			m_nPlayerFlags &= ~PLAYER_FLAGS_SHIP_ARRIVE;
			CMapServer::GetServer()->m_nShipSchedule.ShipSchedule_ArriveChangeMap(this);
		}
		// �۰ʶǰe
		if (m_nTeleportMapCode)
		{
			if (lCurtime >= m_nTeleportDueTime)
			{
				r = InnerPlayerAutoTeleportProcess(this, m_nTeleportMapCode, m_nTeleportPosX, m_nTeleportPosY);
				if (r < 0)				// ���~����
				{
					m_nTeleportMapCode = 0;
				}
				else if (r == 0)		// ���Ѷ�����(��2 - 5��)
				{
#ifdef USE_MAP_SPACE_TELEPORT_TIME_CHECK
					m_nTeleportDueTime += gameGetRandomRange(1, 4) + 1;
#else
					m_nTeleportDueTime += 5;
#endif
				}
				else					// ���\���ݭ���(��30 - 50��)
				{
#ifdef USE_MAP_SPACE_TELEPORT_TIME_CHECK
					m_nTeleportDueTime = lCurtime + gameGetRandomRange(1, 20) + 30;
#else
					m_nTeleportDueTime = lCurtime + 50;
#endif
				}
			}
		}
		//
		if (m_nTeleport2MapCode)
		{
			if (lCurtime >= m_nTeleport2DueTime)
			{
				if (IsReady())
				{
					r = InnerPlayerAutoTeleportProcess(this, m_nTeleport2MapCode, m_nTeleport2PosX, m_nTeleport2PosY);
					if (r < 0)				// ���~����
					{
						m_nTeleport2MapCode = 0;
					}
					else if (r == 0)		// ���Ѷ�����(��2 - 5��)
					{
#ifdef USE_MAP_SPACE_TELEPORT_TIME_CHECK
						m_nTeleport2DueTime += gameGetRandomRange(1, 4) + 1;
#else
						m_nTeleportDueTime += 5;
#endif
					}
					else					// ���\���ݭ���(��30 - 50��)
					{
#ifdef USE_MAP_SPACE_TELEPORT_TIME_CHECK
						m_nTeleport2DueTime = lCurtime + gameGetRandomRange(1, 20) + 30;
#else
						m_nTeleport2DueTime = lCurtime + 50;
#endif
					}
				}
				else
				{
#ifdef USE_MAP_SPACE_TELEPORT_TIME_CHECK
					m_nTeleport2DueTime += gameGetRandomRange(0, 3) + 1;
#else
					m_nTeleport2DueTime += 3;
#endif
				}
			}
		}
	}

	// ���_����
	if (m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)
	{
		//lCurtime = CMapServer::GetServer()->GetLoopTime();
		if (lCurtime >= m_WaitVT_Timeout)	// �ɶ������
		{
			m_WaitVT_Timeout = 0;
			m_WaitVT_TalkID_NoData = 0;
			m_WaitVT_TalkID_Error = 0;
			m_WaitVT_TalkID_Ok = 0;
			m_WaitVT_State = 0;
			//
			//if (IsNPCTalk() && (m_WaitVT_State & PLAYER_NPCTALK_WAIT_DATA))
			if (IsNPCTalk() && m_WaitVT_State)
			{
				ExitNPCTalk();
				//
				memset(&nComm, 0, sizeof(nComm));
				nComm.pcUID = m_Wait_PCOMM_UID;
				SendNPCTalkResult(&nComm, 0, 0, 0);
			}
		}
	}

	// �~��
	if (m_nBotKickTime > 0)
	{
		if (m_nBotKickTime > (long)nTick)
		{
			m_nBotKickTime -= nTick;
		}
		else
		{
			if( !CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->GetLoginServer()) )
			{
				m_nBotKickTime = gameGetRandomRange(5, 30) * 1000;	// 5 ���� 30 ��
			}
			else
			{
				m_nBotKickTime = -1;
				//
 #ifndef NO_AUTO_KICK_BOT
				struct LOGIN_MAP_KICK_PLAYER nReq;

				//nReq.uid = GetUID();
				nReq.uid = 0;
				nReq.nMinutes = AUTO_KICK_BOT_LOCK_TIME;		// ��w�ɶ�, ��X
				nReq.nType = 1;
				nReq.nBotType1 = m_nBotReason[0];
				nReq.nBotType2 = m_nBotReason[1];
				nReq.nBotType3 = m_nBotReason[2];
				msg_strncpy(nReq.m_szName, GetSaveData()->plrName, sizeof(nReq.m_szName));
				CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_MAP_KICK_PLAYER, (char *)&nReq, sizeof(nReq), 0);
 #endif
				//
				CMapServer::GetServer()->Log("SERIOUS: Find BOT(%d, %s)(%d, %s)", GetUID(), GetSaveData()->plrName, GetSaveData()->plrAccountID, GetSaveData()->plrAccount);
				//
				CMapServer::GetServer()->SendLogMessage_System(this, LOGTYPE_SYS_BOT);

			//	CMapServer::GetServer()->Disconnect_ClearAllRecord( GetUID(), 0 );
			//	SaveAllData();
			//	SetExitCode(CMapPlayer::EXIT_DATA_ERROR);
			//	CMapServer::GetServer()->DeleteObject(GetHandle());
				return;
			}
		}
	}

	// ����Ū���P�s�ɬO�_�U��(�C�@��)
	m_SaveLoadTick += (unsigned short)nTick;
	if (m_SaveLoadTick >= apiGetTickPreSec())
	{
		nTick = m_SaveLoadTick;
		if(IsLoadingData())
		{
			IsLoading = false;
			for(cnt1 = 0; cnt1 < DB_TOTAL_TYPES; cnt1++)
			{
				nLoadTime = m_nLoadTime[cnt1];
				if(nLoadTime > 0)
				{
					if(nLoadTime <= nTick)
					{
						CMapServer::GetServer()->Log(MSG_WARN_READ_TIMEOUT, GetUID());
						//m_nLoadTime[cnt1] = 0;
						m_nLoadTime[cnt1] = 1000 * 60 * 5;

						CMapServer::GetServer()->Log("ERROR: Load data time-out(%d)", GetUID());
						CMapServer::GetServer()->Disconnect_ClearAllRecord( GetUID(), 0 );
						// �j����
						if (!CMapServer::GetServer()->IsObjectDeleted(GetHandle()))
						{
							SetExitCode(CMapPlayer::EXIT_DATA_ERROR);
							CMapServer::GetServer()->DeleteObject(GetHandle());
						}
						//
						IsLoading = true;
						break;
					}
					else
					{
						m_nLoadTime[cnt1] -= nTick;
						IsLoading = true;
					}
				}
			}
			m_IsLoadingData = IsLoading;
		}

		if(IsSavingData())
		{
			IsSaving = false;
			for(cnt1 = 0; cnt1 < DB_TOTAL_TYPES; cnt1++)
			{
				nSaveTime = m_nSaveTime[cnt1];
				if(nSaveTime > 0)
				{
					if(nSaveTime <= nTick)
					{
						CMapServer::GetServer()->Log(MSG_WARN_WRITE_TIMEOUT, GetUID());
						m_nSaveTime[cnt1] = 0;
						//
						// ???
					//	// �x�s���ѡA�O�_�n�_�u
					//	// �j����
					//	if (!CMapServer::GetServer()->IsObjectDeleted(GetHandle()))
					//	{
					//		SetExitCode(CMapPlayer::EXIT_DATA_ERROR);
					//		CMapServer::GetServer()->DeleteObject(GetHandle());
					//	}
						//
						//IsSaving = true;	// �o�|������ä��|����
						//break;
					}
					else
					{
						m_nSaveTime[cnt1] -= nTick;
						IsSaving = true;
					}
				}
			}
			m_IsSavingData = IsSaving;
		}
		//
		m_SaveLoadTick -= apiGetTickPreSec();
		//
		if (IsReady() && !IsLoadingData())
		{
			pSave = GetSaveData();
			if (pSave->plrStsTime_GoodLuck)
			{
				pSave->plrStsTime_GoodLuck--;
				ProcSpecialStatus(pSave->plrStsTime_GoodLuck, UPART2_TYPE_STATUS_GOOD_LUCK, 0, 1);
			}
			if (pSave->plrStsTime_AddExp)
			{
				pSave->plrStsTime_AddExp--;
				ProcSpecialStatus(pSave->plrStsTime_AddExp, UPART2_TYPE_STATUS_ADD_EXP, 0, 1);
			}
			if (pSave->plrStsTime_AddSkillExp)
			{
				pSave->plrStsTime_AddSkillExp--;
				ProcSpecialStatus(pSave->plrStsTime_AddSkillExp, UPART2_TYPE_STATUS_ADD_SKILL_EXP, 0, 1);
			}
			if (pSave->plrStsTime_AddHonor)
			{
				pSave->plrStsTime_AddHonor--;
				ProcSpecialStatus(pSave->plrStsTime_AddHonor, UPART2_TYPE_STATUS_ADD_HONOR, 0, 1);
			}
			if (pSave->plrStsTime_AddCNPCExp)
			{
				pSave->plrStsTime_AddCNPCExp--;
				ProcSpecialStatus(pSave->plrStsTime_AddCNPCExp, UPART2_TYPE_STATUS_ADD_CNPC_EXP, 0, 1);
			}
			if (pSave->plrStsTime_Undead)
			{
				pSave->plrStsTime_Undead--;
				ProcSpecialStatus(pSave->plrStsTime_Undead, UPART2_TYPE_STATUS_UNDEAD_TIME, 0, 1);
			}
			if (pSave->plrStsTime_SoldierUndead)
			{
				pSave->plrStsTime_SoldierUndead--;
				ProcSpecialStatus(pSave->plrStsTime_SoldierUndead, UPART2_TYPE_STATUS_SOLDIER_UNDEAD, 0, 1);
			}
			// �]��
			if (pSave->plrStsTime_Month_Exp)
			{
				if (lCurtime >= pSave->plrStsTime_Month_Exp)
				{
				/*	// ��X�\�u
					if (pSave->plrPersonalShopTime == pSave->plrStsTime_Month)
					{
						pSave->plrPersonalShopTime = 0;
						CancelStall();
					} */
					pSave->plrStsTime_Month_Exp = 0;
					ProcSpecialStatus(pSave->plrStsTime_Month_Exp, UPART2_TYPE_STATUS_MONTH_TIME, 0, 1);
				}
			}
			if (pSave->plrStsTime_Month_Skill_Exp)
			{
				if (lCurtime >= pSave->plrStsTime_Month_Skill_Exp)
				{
					pSave->plrStsTime_Month_Skill_Exp = 0;
					ProcSpecialStatus(pSave->plrStsTime_Month_Skill_Exp, UPART2_TYPE_STATUS_MONTH_SKILL_TIME, 0, 1);
				}
			}
			if (pSave->plrStsTime_Month_CNPC_Exp)
			{
				if (lCurtime >= pSave->plrStsTime_Month_CNPC_Exp)
				{
					pSave->plrStsTime_Month_CNPC_Exp = 0;
					ProcSpecialStatus(pSave->plrStsTime_Month_CNPC_Exp, UPART2_TYPE_STATUS_MONTH_CNPC_TIME, 0, 1);
				}
			}

			if (pSave->plrStsTime_Month_GoodLuck)
			{
				if (lCurtime >= pSave->plrStsTime_Month_GoodLuck)
				{
					pSave->plrStsTime_Month_GoodLuck = 0;
					ProcSpecialStatus(pSave->plrStsTime_Month_GoodLuck, UPART2_TYPE_STATUS_MONTH_GOODLUCK, 0, 1);
				}
			}

			if (plrStsTime_Month_AddHonor_field(pSave))
			{
				if (lCurtime >= plrStsTime_Month_AddHonor_field(pSave))
				{
					plrStsTime_Month_AddHonor_field(pSave) = 0;
					pSave->plrStsPower_AddHonor = 0;
					ProcSpecialStatus(0, UPART2_TYPE_STATUS_ADD_HONOR, 0, 1);
					SendSpecialTimeTable();
				}
			}

			// �۰ʥ\��
			if (pSave->plrAutoFunctionTime)
			{
				if (lCurtime >= pSave->plrAutoFunctionTime)
				{
					pSave->plrAutoFunctionTime = 0;
					UpdateAutoFunctionTime(1);
					SaveCharData();
					//
					// ���� Login Server �ǰe���G
					struct LOGIN_NOTIFY_AUTO_STATUS nSts;

					nSts.nStatus = 0;
					::msgSendData( GetClientID(), 0, PROTO_LOGIN_NOTIFY_AUTO_STATUS, (char *)&nSts, sizeof(nSts), 0 );
				}
			}
			// 武魂到期：自动卸下并刷新外观/属性
			if (pSave->plrSoulWID && pSave->plrSoulWID != 0xFFFF && pSave->plrSoulTime > 0)
			{
				if (lCurtime >= (long)pSave->plrSoulTime)
				{
					pSave->plrSoulWID = 0;
					pSave->plrSoulOffice = 0;
					pSave->plrSoulTime = 0;
					pSave->plrSoulItem = 0;
					UpdateLoginPlayerOffice();
					UpdateSkillTable(3);
					::gameServer_CalcCharacterAttribute(GetCharData());
					UpdateShowData();
					UpdateClientShowData(0);	// �n�b UpdateShowData ����
					SendSpecialTimeTable();
					SaveCharData();
				}
			}

			// �Z��ɶ�
			// 变身系统：用 plrTransformNpcCode 判断是否有变身，并使用独立时间槽位避免与武魂(plrSoulTime/plrSoulWID)冲突
			if (pSave->plrTransformNpcCode > 0)
			{
				if (CMapServer::GetServer()->GetLoopTime() >= (long)pSave->plrTime_General_a)
				{
					// �M���Z����[���ޯ�
				//	if (pSave->plrSoulOffice)
				//		UpdateSkillTable(2, pSave->plrSoulOffice);
					
					// 化身变身系统：清除变身状态
					struct plrDATA* pCharData = GetCharData();
					unsigned long nowLoopX = (unsigned long)CMapServer::GetServer()->GetLoopTime();
					int hasSoul = (pSave->plrSoulItem != 0 && pSave->plrSoulWID && pSave->plrSoulWID != 0xFFFF && pSave->plrSoulTime > nowLoopX);
					/* Same visual restore as CB_AvatarHideAppearance when hiding transform model */
					pSave->plrTransformHideAppearance = 0;
					if (pCharData)
						pCharData->plrSetWID = 0;
					if (!hasSoul)
					{
						pSave->plrSoulWID = 0;
						pSave->plrSoulOffice = 0;
					}

					// 恢复原始外观
					if(pSave->plrOriginalFaceID > 0)
					{
						pSave->plrCode = pSave->plrOriginalFaceID;
						pSave->plrOriginalFaceID = 0;
					}
					
					// 清除装备隐藏标志
					pSave->plrTransformHideEquip = 0;

					{
						CMapCharMsg* pAuraMsg = InsertMsg(PROTO_MAP_USE_ITEM_RESULT_ADD_HP_MP, CMapCharMsg::SEND_TO_ALL);
						if (pAuraMsg)
						{
							pAuraMsg->m_nSize = sizeof(struct MAP_USE_ITEM_RESULT_ADD_HP_MP);
							pAuraMsg->m_Msg.m_UseItemAddHpMp.plrUID = GetUID();
							pAuraMsg->m_Msg.m_UseItemAddHpMp.add_HP_MP_Type = UIR_EFFECT;
							pAuraMsg->m_Msg.m_UseItemAddHpMp.addHP = TRANSFORM_EFFECT_CLEAR_ID;
							pAuraMsg->m_Msg.m_UseItemAddHpMp.addMP = TRANSFORM_EFFECT_SIGNATURE_MP;
							pAuraMsg->m_Msg.m_UseItemAddHpMp.itemID = 0;
						}
					}

					// 清除变身NPC code和装备标志（重要：这样下次检查时不会认为还有变身）
					pSave->plrTransformNpcCode = 0;
					pSave->plrTransformEquipWeapon = 0;
					pSave->plrTime_General_a = 0;
					
					// 清除所有保存的变身属性（属性会通过AddTransformAttrToPlayer自动移除）
					pSave->plrTransformStr = 0;
					pSave->plrTransformInt = 0;
					pSave->plrTransformMind = 0;
					pSave->plrTransformDex = 0;
					pSave->plrTransformVit = 0;
					pSave->plrTransformLeader = 0;
					pSave->plrTransformHP = 0;
					pSave->plrTransformMP = 0;
					pSave->plrTransformAttackPower = 0;
					pSave->plrTransformMagicPowerVal = 0;
					pSave->plrTransformMagicAttack_Slash = 0;
					pSave->plrTransformMagicAttack_Sting = 0;
					pSave->plrTransformMagicAttack_Break = 0;
					pSave->plrTransformMagicAttack_Arrow = 0;
					pSave->plrTransformMagicAttack_Fire = 0;
					pSave->plrTransformMagicAttack_Water = 0;
					pSave->plrTransformMagicAttack_God = 0;
					pSave->plrTransformMagicAttack_Evil = 0;
					
					UpdateSkillTable(3);
					//pSave->plrSoulTime = 0;
					// ��s Login Server �O��
					UpdateLoginPlayerOffice();
					//
					if (pCharData)
						::gameServer_CalcCharacterAttribute(pCharData);
					if (pCharData)
						pCharData->plrSetWID = 0;
					UpdateShowData();
					UpdateClientShowData(0);	// �n�b UpdateShowData ����
					SaveCharData();
				}
			}
			//
			if (pSave->plrEngineerTime)
			{
				if (pSave->plrEngineerTime != 65535)		// ���ܮɶ���L��
				{
					pSave->plrEngineerTime--;
					if (!pSave->plrEngineerTime)
					{
 #ifndef USE_PACKAGE_DATA
						CMapServer::GetServer()->Log("DEBUG: �����H����ɶ���(%d,%s) !", GetUID(), GetName());
 #endif
						pSave->plrEngineerTime = 65535;		// ���ܮɶ���L��
						//
						//UpdateSkillTable(12, pSave->plrEquipEngineer.m_Item.itemCode);
						UpdateSkillTable(3);
						//
						UpdateEngineerTime();
						//
						::gameServer_CalcCharacterAttribute(GetCharData());
						UpdateShowData();
						UpdateClientShowData(0);	// �n�b UpdateShowData ����
					}
				}
			}
			//
#ifdef USE_CNPC_LOYALITY_TIME_ADD				// �w�ɼW�[�p�L����(�u��server�ݡAItemMode)
			if (lCurtime >= (long)pSave->plrCNPC_LoyalityTime)
			{
				if (pSave->plrCNPC_LoyalityTime)
				{
					AutoAddCarryNPCRoyality();
				}
				pSave->plrCNPC_LoyalityTime = lCurtime + CNPC_ADD_ROYALTY_PERIOD_TIME;
				AutoSaveCharData();
			}
#endif
			// ���H
#ifdef TW_PVP_MODE
			if (pSave->plrCrimeVal > 0)
			{
				if (pSave->plrHP)
				{
					// �DPK�a�ϫݺ�1�p�ɡAPK�a�ϫݺ�30�����M���@�I
					if (m_nCrimeClearTick > 0)
					{
						if (--m_nCrimeClearTick <= 0)
						{
							cnt1 = pSave->plrCrimeVal - 1;
							UpdatePlayerCrime(cnt1);
							AutoSaveCharData();
							//
							if (!cnt1)
							{
								pStatus_Clear(effFun_RED, 1);
								SetCarryNPC_REDStatus();
							}
							else
								goto set_crime_clear_time;
						}
					}
					else
					{
						// �]�w�ɶ�
						if (m_nCrimeClearTick <= 0)
						{
	set_crime_clear_time:
							cnt1 = CLEAR_CRIME_TIME_1;
							// �ˬd�O�_�i�HPK
							pStg = gameStageGetPtr(GetMapCode());
							if (pStg)
							{
								if (pStg->gstgFlag & gstgFLAG_FORCE_PK)
									cnt1 = CLEAR_CRIME_TIME_2;
							}
							m_nCrimeClearTick = cnt1;
						}
					}
				}
			}
#else
			if (IsRedStatus())
			{
				if (CMapServer::GetServer()->GetLoopTime() > m_nRedDuedate)
				{
					pStatus_Clear(effFun_RED, 1);
					SetCarryNPC_REDStatus();
				}
			}
			//
#endif
			//
			if (pSave->plrEnterStageTime)
			{
				// �аO�b���O�a��
				if (m_nPlayerFlags & PLAYER_FLAGS_ENTER_STAGE)
				{
					pSave->plrEnterStageTime--;
					AutoSaveCharData();
					if (pSave->plrEnterStageTime == 0)
					{
						EnterStage_SendClientTimeNotify(this);
					}
				}
			}
			//
			DuedateItem_Process();
			TimedMapPass_Process();
			TimedMapPass_HUDNotify();
			// �\�u
#ifdef ItemMode
			if (pSave->plrPersonalShopTime)
			{
				lCurtime = CMapServer::GetServer()->GetLoopTime();
				if (lCurtime >= pSave->plrPersonalShopTime)
				{
					GetSaveData()->plrPersonalShopTime = 0;
					AutoSaveCharData();
					//
					UpdatePersonalShopTime();
					// ��X�\�u
					CancelStall();
					//
 #ifdef USE_STALL_NEW_RULE
					struct plrDATA_SKILL * pSkillTbl;

					pSkillTbl = GetSkillData();
					pSkillTbl->plrSkillLevelMax[magic_PERISONAL_SHOP] = 0;
					AutoSaveSkillData();
					//
					UpdateSkillTable();
 #endif
				}
			}
#endif
			// �D�B
			if (m_nPlayerFlags & PLAYER_FLAGS_WAIT_MERRY)
			{
				if (m_nWaitMerryTime)
				{
					CMapPlayer * pDest;

					pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(m_nMerryPlayerUID);
					if (!pDest || (lCurtime > m_nWaitMerryTime))
					{
						ProcWaitMerryResponseFail();
					}
					else if (GetMapCode() != pDest->GetMapCode())
					{
						ProcWaitMerryResponseFail();
					}
				}
			}
#ifdef EIGHT_GATES
			if(gameServerType_GetType())
			{
				//�K���P�Ҷ}�Үɶ�
				if (pSave->plrOpenGateTime)
				{
					register struct plrDATA * pCharData = GetCharData();
					pSave->plrOpenGateTime--;
					pCharData->plrCheckGateOpen = 1;
					if (pSave->plrOpenGateTime == 0)
					{
						pCharData->plrCheckGateOpen = 0;
						::gameServer_CalcCharacterAttribute(GetCharData());
						UpdateClientShowData(0);	// �n�b UpdateShowData ����
					}
					AutoSaveCharData();
					UpdateEightGateTime(pSave->plrOpenGateTime,pCharData->plrCheckGateOpen);
				}
			}
#endif
		}
		// �ǰe�U��
		if (m_TeleportKickTime)
		{
			if (CMapServer::GetServer()->GetLoopTime() > m_TeleportKickTime)
			{
				CMapServer::GetServer()->Log("ERROR: Teleport time out(%s,%d)", GetName(), GetMapCode());
				m_TeleportKickTime = 0;
				CMapServer::GetServer()->KickPlayer(this);
			}
		}
	}

	// ��ԭ��ͩ���
	CWar_ProcResurrectTick();
	//
	// �C���ɶ��֥[�p��
	nTick = CMapServer::GetServer()->GetLoopTickCount();
	m_nPlayTime += nTick;
	if (m_nPlayTime >= 1000)
	{
		nSecond = m_nPlayTime / 1000;
		GetSaveData()->plrPlayTime += nSecond;
		m_nPlayTime -= (nSecond * 1000);
		//
		if (++m_nCheckMissionCount >= 20)
		{
			m_nCheckMissionCount = 0;
			ProcessMissionRepeat();
		}
		// �j�����I�g
		if (m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
// #ifndef USE_PACKAGE_DATA
//			CMapServer::GetServer()->Log("DEBUG: SPC CN mode(%s) !", GetName());
// #endif
//			if (++m_nModeCN_LoopCount >= 5)
			{
// #ifndef USE_PACKAGE_DATA
//			CMapServer::GetServer()->Log("DEBUG: SPC CN mode 5 times(%s) !", GetName());
// #endif
//				m_nModeCN_LoopCount = 0;
				//
				if(IsReady())
				{
					if (!(m_nPrivilege & gamePRIVILEGE_TYPE_DELETED))	// �Ĥ@���n�J�a��
					{
// #ifndef USE_PACKAGE_DATA
//			CMapServer::GetServer()->Log("DEBUG: SPC CN mode Process(%s) !", GetName());
// #endif
						SpecModeCN_Process(nSecond);
					}
				}
			}
		}
	}
}

// �M���Ҧ��ޯ�í��s����
long InnerUpdateExtraSkill(CMapPlayer * pMapPlayer)
{
	struct plrDATA_SAVE * pSave;
	struct plrDATA_SKILL * pSkillTbl;
	long * pID;
	long i, id, nTotal;
	long r;
	long is_engineer;

	r = 0;
	pSkillTbl = pMapPlayer->GetSkillData();
	pSave = pMapPlayer->GetSaveData();
	//
	pID = gameGetGiveSkill_Soul(&nTotal);
	if (pID)
	{
		for (i=0; i<nTotal; i++)
		{
			id = pID[i];
			//
			if (pSkillTbl->plrSkillLevelMax[id])
			{
				pSkillTbl->plrSkillLevelMax[id] = 0;
				pSkillTbl->plrSkillLevel[id] = 0;
				r |= 1;
			}
		}
	}
	//
	is_engineer = 0;
	if (pSave->plrJobID == jobID_ENGINEER)
	{
		if (pSave->plrEngineerTime != 65535)		// �����P�Z���O����æs
			is_engineer++;
		//
		pID = gameGetGiveSkill_Engineer(&nTotal);
		if (pID)
		{
			for (i=0; i<nTotal; i++)
			{
				id = pID[i];
				//
				if (pSkillTbl->plrSkillLevelMax[id])
				{
					pSkillTbl->plrSkillLevelMax[id] = 0;
					pSkillTbl->plrSkillLevel[id] = 0;
					r |= 1;
 #ifndef USE_PACKAGE_DATA
					CMapServer::GetServer()->Log("DEBUG: �M�������H����ޯ�(%d,%s) !", id, pMapPlayer->GetName());
 #endif
				}
			}
		}
	}
	//
	pID = gameGetGiveSkill_Set(&nTotal);
	if (pID)
	{
		for (i=0; i<nTotal; i++)
		{
			id = pID[i];
			//
			if (pSkillTbl->plrSkillLevelMax[id])
			{
				pSkillTbl->plrSkillLevelMax[id] = 0;
				pSkillTbl->plrSkillLevel[id] = 0;
				r |= 1;
			}
		}
	}

	// �]�w
	if (is_engineer && pSave->plrEquipEngineer.m_Item.itemCode)
	{
		r |= pMapPlayer->UpdateSkillTable(11, pSave->plrEquipEngineer.m_Item.itemCode, 1);
	}
	else
	{
		if (pSave->plrSoulOffice)
			r |= pMapPlayer->UpdateSkillTable(1, pSave->plrSoulOffice, 1);
	}
	//
	if(pMapPlayer->GetCharData()->plrSetMagic[0] != 0)
		r |= pMapPlayer->UpdateSkillTable(21, 0, 1);
	//
	r |= pMapPlayer->UpdateSkillTable(1, pSave->plrOffice, 1);
	return(r);
}

// calc_soul = soul office id �p��Z��v�T, type = 1�]�w, = 2�M��, 3�ˬd���/���m;  11�]�w�����H, 12�M�������H ; 21�M�˧ޯ�
// PS: �@��x��i�H�� 1,2 �վ�A�Z�� �M �����H �@�ߨϥ� 3 ���m�վ�
long CMapPlayer::UpdateSkillTable(long type, long calc_soul, long no_send_client)
{
	struct gameOFFICER_DATA * pOffice;
	long update_flag, i;
	long id, level;
	struct plrDATA_SKILL * pSkillTbl;
	//
//	long hot_page;
//	struct plrHOTKEYDATA * pHotKey;
	//
	struct ENGINEER_SKILL_DATA * pESkill;
	struct itemDATA * pItemData;

	if (type)
	{
		update_flag = 0;
		//
		switch(type)
		{
		case 1:
			if (!calc_soul)
				goto abort;
			pOffice = gameGetOfficerPtr(calc_soul);
			if (!pOffice)
				goto abort;
			pSkillTbl = GetSkillData();
			for (i=0; i<gameMAX_OFFICER_SKILL; i++)
			{
				id = pOffice->odAddSkill[i];
				level = pOffice->odAddSkillLevel[i];
				if (id && level)
				{
					if (!pSkillTbl->plrSkillLevelMax[id])
					{
						pSkillTbl->plrSkillLevelMax[id] = (unsigned char)level;
						pSkillTbl->plrSkillLevel[id] = (unsigned char)level;
						update_flag |= 1;
					}
				//	else
				//		CMapServer::GetServer()->Log("ERROR: Soul skill already exist(%s,%d)", GetName(), id);
				}
			}
			break;
		case 2:
			if (!calc_soul)
				goto abort;
			pOffice = gameGetOfficerPtr(calc_soul);
			if (!pOffice)
				goto abort;
			pSkillTbl = GetSkillData();
			for (i=0; i<gameMAX_OFFICER_SKILL; i++)
			{
				id = pOffice->odAddSkill[i];
				level = pOffice->odAddSkillLevel[i];
				if (id && level)
				{
					if (pSkillTbl->plrSkillLevelMax[id])
					{
						pSkillTbl->plrSkillLevelMax[id] = 0;
						pSkillTbl->plrSkillLevel[id] = 0;
						update_flag |= 1;
					}
				//	else
				//		CMapServer::GetServer()->Log("ERROR: Soul clear skill not exist(%s,%d)", GetName(), id);
				}
			}
			// �⤣�s�b���ޯ����M��(����|���� UpdateClientShowData())
clear_hotkey:
		/*	if (update_flag)		// �O�d����
			{
				for (hot_page=0; hot_page<gameMAX_I_HOTKEY_PAGE; hot_page++)
				{
					for (i=0; i<gameMAX_I_HOTKEY_PAGE; i++)
					{
						pHotKey = &GetSaveData()->plrHotKey[i+hot_page*gameMAX_I_HOTKEY]; 
						if (id = pHotKey->phkID)
						{
							if (level = pHotKey->phkType)
							{
								if (level & pi_SKILL)
								{
									if (pSkillTbl->plrSkillLevelMax[id] == 0)
									{
										memset(pHotKey, 0, sizeof(struct plrHOTKEYDATA));
									}
								}
							}
						}
					}
				}
			}
		*/
			break;
		case 3:
			// �O�_�t���Z��ޯ�
//#ifdef USE_PACKAGE_DATA
		/*	if (!GetSaveData()->plrSoulWID)
			{
				long * pID;
				long nTotal;

				pSkillTbl = GetSkillData();

				pID = gameGetGiveSkill_Soul(&nTotal);
				if (pID)
				{
					for (i=0; i<nTotal; i++)
					{
						id = pID[i];
						//
						if (pSkillTbl->plrSkillLevelMax[id])
						{
							pSkillTbl->plrSkillLevelMax[id] = 0;
							pSkillTbl->plrSkillLevel[id] = 0;
							update_flag |= 1;
							CMapServer::GetServer()->Log("Illegal soul skill(%s, %d)", GetName(), id);
						}
					}
				}
			//	i = 0;
			//	while(1)
			//	{
			//		if (id = fix_skill_number_tbl[i])
			//		{
			//			if (pSkillTbl->plrSkillLevelMax[id])
			//			{
			//				pSkillTbl->plrSkillLevelMax[id] = 0;
			//				pSkillTbl->plrSkillLevel[id] = 0;
			//				update_flag |= 1;
			//				CMapServer::GetServer()->Log("Illegal skill(%s, %d)", GetName(), id);
			//			}
			//		}
			//		else
			//			break;
			//		i++;
			//	}
			}
		*/
			update_flag |= InnerUpdateExtraSkill(this);
//#endif
			break;
		case 11	:		// �]�w�����H
			if (!calc_soul)
				goto abort;
			if (GetSaveData()->plrJobID != jobID_ENGINEER)
				goto abort;
			pItemData = gameGetItemPtr(calc_soul);
			if (!pItemData)
				goto abort;
			pESkill = gameGetEngineerSkillTablePtr(pItemData->itemEngineerSkill);
			if (!pESkill)
				goto abort;
			pSkillTbl = GetSkillData();
			for (i=0; i<pESkill->esdTotalSkill; i++)
			{
				id = pESkill->esdSkill[i].siID;
				level = pESkill->esdSkill[i].siLevel;
				if (id && level)
				{
					if (!pSkillTbl->plrSkillLevelMax[id])
					{
						pSkillTbl->plrSkillLevelMax[id] = (unsigned char)level;
						pSkillTbl->plrSkillLevel[id] = (unsigned char)level;
						update_flag |= 1;
 #ifndef USE_PACKAGE_DATA
						CMapServer::GetServer()->Log("DEBUG: 11�]�w�����H����ޯ�(%d,%s) !", id, GetName());
 #endif
					}
				}
			}
			break;
		case 12:		// �M�������H
			if (!calc_soul)
				goto abort;
			if (GetSaveData()->plrJobID != jobID_ENGINEER)
				goto abort;
			pItemData = gameGetItemPtr(calc_soul);
			if (!pItemData)
				goto abort;
			pESkill = gameGetEngineerSkillTablePtr(pItemData->itemEngineerSkill);
			if (!pESkill)
				goto abort;
			pSkillTbl = GetSkillData();
			for (i=0; i<pESkill->esdTotalSkill; i++)
			{
				id = pESkill->esdSkill[i].siID;
				level = pESkill->esdSkill[i].siLevel;
				if (id && level)
				{
					if (pSkillTbl->plrSkillLevelMax[id])
					{
						pSkillTbl->plrSkillLevelMax[id] = 0;
						pSkillTbl->plrSkillLevel[id] = 0;
						update_flag |= 1;
					}
				}
			}
			goto clear_hotkey;
			break;
	/*	case 13:		// �ˬd�����H
			if (GetSaveData()->plrJobID != jobID_ENGINEER)
				return;
			if (!GetSaveData()->plrEquipEngineer.m_Item.itemCode)
			{
				long * pID;
				long nTotal;

				pSkillTbl = GetSkillData();

				pID = gameGetGiveSkill_Engineer(&nTotal);
				if (pID)
				{
					for (i=0; i<nTotal; i++)
					{
						id = pID[i];
						//
						if (pSkillTbl->plrSkillLevelMax[id])
						{
							pSkillTbl->plrSkillLevelMax[id] = 0;
							pSkillTbl->plrSkillLevel[id] = 0;
							update_flag |= 1;
							CMapServer::GetServer()->Log("Illegal engineer skill(%s, %d)", GetName(), id);
						}
					}
				}
			}
			break;
	*/
		case 21:
			pSkillTbl = GetSkillData();
			for (i=0; i<SET_MAGIC_NUM_MAX; i++)
			{
				id = GetCharData()->plrSetMagic[i];
				level = GetCharData()->plrSetMagicLevel[i];
				if (id && level)
				{
					if (!pSkillTbl->plrSkillLevelMax[id])
					{
						pSkillTbl->plrSkillLevelMax[id] = (unsigned char)level;
						pSkillTbl->plrSkillLevel[id] = (unsigned char)level;
						update_flag |= 1;
					}
				//	else
				//		CMapServer::GetServer()->Log("ERROR: Set skill already exist(%s,%d)", GetName(), id);
				}
			}
			break;
		}
		//
		if (!update_flag)
			return(update_flag);
		//
		SaveSkillData();
		//
		if (no_send_client)
			return(update_flag);
	}
	::msgSendData( GetClientID(), 0, PROTO_MAP_GET_SKILL_TABLE_RESULT, (char *)GetSkillData(), sizeof( struct plrDATA_SKILL ), 0 );
	//
abort:
	return(0);
}

void CMapPlayer::UpdateEngineerTime()
{
	CMapCharMsg * pMsg;

	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	
	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ENGINEER_SEC, 0, GetSaveData()->plrEngineerTime);
	
	pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

void CMapPlayer::UpdateArrowPack()
{
	CMapCharMsg * pMsg;

	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	
	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_ARROW_PACK, GetSaveData()->plrArrowPack_ItemID, GetSaveData()->plrArrowPack_ItemNumber, 0);
	
	pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

void CMapPlayer::UpdateCWarInfo()
{
	CMapCharMsg * pMsg;

	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	
	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_CWAR_INFO, 0, GetSaveData()->plrCWar_KillCount, 0);
	
	pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

void CMapPlayer::SendUseItemNotify(long item_id, long number)
{
	CMapCharMsg * pMsg;

	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	
	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_USE_ITEM_NOTIFY, 0, item_id, number);

	pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

void CMapPlayer::SendSkillInUseNotify(long skill_id, long tick_time)
{
	CMapCharMsg * pMsg;

	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	
	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_SKILL_IN_USE_NOTIFY, 0, skill_id, tick_time);
	
	pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

// mode = 1 Client �|��ܮɶ�
void CMapPlayer::UpdatePersonalShopTime()
{
	long cur_time;

	cur_time = CMapServer::GetServer()->GetLoopTime();
	if (cur_time >= GetSaveData()->plrPersonalShopTime)	// �k 0
		GetSaveData()->plrPersonalShopTime = 0;
	//
	{
		long nMsgSize;
		struct MAP_UPDATE_PLAYER_DATA_PART2 nMsg;

		nMsg.Reset();
		nMsg.Add(UPART2_TYPE_PERSON_SHOP_NOTIFY, 0, GetSaveData()->plrPersonalShopTime, 0);
		nMsgSize = nMsg.GetSize();
		if (GetClientID())
			::msgSendData(GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsgSize, 0);
	}
}

// ��s�ӧQ�Ũ��ɶ�
void CMapPlayer::UpdateAnnouncementTime()
{
	CMapCharMsg * pMsg;
	long cur_time;

	cur_time = CMapServer::GetServer()->GetLoopTime();
	if (cur_time >= GetSaveData()->plrStsTime_Announcement)	// �k 0
		GetSaveData()->plrStsTime_Announcement = 0;
	//
	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);

	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ANNOUNCEMENT_NOTIFY, 0, GetSaveData()->plrStsTime_Announcement, 0);

	pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

// type = 1 ��ܮɶ�, 2 �Ĥ@����ܮɶ�(�ɶ�=0�����)
void CMapPlayer::UpdateAutoFunctionTime(long type)
{
	long l_time;
	CMapCharMsg * pMsg;

	l_time = GetSaveData()->plrAutoFunctionTime;
	if (type == 2)
	{
		type = 1;
		if (l_time == 0)
			type = 0;
	}
	//
	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);

	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_AUTO_FUNCTION_NOTIFY, 0, l_time, type);

	pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

// ��쪫�~�èϥα�
// number = 0 �M��
// pos_id_first: �H�o�Ӧ�m���~�u��
long CMapPlayer::FindItemAndDelete(long item_id, long pos_id_first)
{
	long i, val, number;
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA * pItem;

	if (CanSaveData())
	{
		pItemData = GetItemData();
		number = 1;
		//for (i=0; i<gameMAX_CARRYITEM; i++)
		val = ::gameServer_GetCarryItemMax(GetSaveData());
		//
		if ((pos_id_first >= 0) && (pos_id_first < val))
		{
			i = pos_id_first;
			if (pItemData->plrCarryItem[i].m_Item.itemCode == item_id)
			{
				pItem = ::gameGetItemPtr(item_id);
				if (pItem)
				{
					if (pItemData->plrCarryItem[i].m_Item.itemNumber >= number)
						goto do_mrh;
				}
			}
		}
		//
		for (i=0; i<val; i++)
		{
			if (pItemData->plrCarryItem[i].m_Item.itemCode == item_id)
			{
				pItem = ::gameGetItemPtr(item_id);
				if (pItem)
				{
					if (pItemData->plrCarryItem[i].m_Item.itemNumber >= number)
					{
do_mrh:					//���log
						CMapServer::GetServer()->SendLogMessage_Item( (CMapPlayer *)this, LOGTYPE_ITEM_USE, 0, &pItemData->plrCarryItem[i] );
						//
						//if (!number)
						//	number = pItemData->plrCarryItem[i].m_Item.itemNumber;
						//
						pItemData->plrCarryItem[i].m_Item.itemNumber -= (unsigned char)number;
						if (pItemData->plrCarryItem[i].m_Item.itemNumber == 0)
							memset(&pItemData->plrCarryItem[i], 0, sizeof(pItemData->plrCarryItem[i]));
						//�^�ǭt��
						GetCharData()->plrWeight -= (pItem->itemWeight * number);
						((CMapPlayer *)this)->UpdateClientGoldAndWeight();
						((CMapPlayer *)this)->AutoSaveItemData();
						//�^�Ǫ��~�����
						struct MAP_USE_ITEM_RESULT sendData;

						::memset(&sendData, 0, sizeof(sendData));
						sendData.sourcePosID = i;
						::gameServer_Item_MakeShowSelf(&pItemData->plrCarryItem[i], &sendData.itemSelf);
						::msgSendData( ((CMapPlayer *)this)->GetClientID(), 0, PROTO_MAP_USE_ITEM_RESULT, (char *)&sendData, sizeof( sendData ), 0 );
						return(1);
					}
				}
			}
		}
	}
	return(0);
}

struct itemDATA_SAVE * CMapPlayer::FindItem(long item_id)
{
	long i, val;
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA * pItem;

	if (CanSaveData())
	{
		pItemData = GetItemData();
		//for (i=0; i<gameMAX_CARRYITEM; i++)
		val = ::gameServer_GetCarryItemMax(GetSaveData());
		for (i=0; i<val; i++)
		{
			if (pItemData->plrCarryItem[i].m_Item.itemCode == item_id)
			{
				pItem = ::gameGetItemPtr(item_id);
				if (pItem)
				{
					if (pItemData->plrCarryItem[i].m_Item.itemNumber >= 1)
						return(&pItemData->plrCarryItem[i]);
				}
			}
		}
	}
	return(0);
}
/*
struct itemDATA_SAVE * CMapPlayer::FindFreeItemPos(long * pos_idx)
{
	long i, val;
	struct plrCARRYITEM_SAVE * pItemData;

	pItemData = GetItemData();
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(GetSaveData());
	for (i=0; i<val; i++)
	{
		if (pItemData->plrCarryItem[i].m_Item.itemCode == 0)
		{
			if (pos_idx)
				*pos_idx = i;
			return(&pItemData->plrCarryItem[i]);
		}
	}
	return(NULL);
}
*/
int CMapPlayer::AccelerateCheck(void)
{
#ifdef NO_CLIENT_SPEED_CHECK
	return(0);
#else
	long	nOdds;
	int		nRes;
	long skip_count;

	if(!IsReady() || IsOutMap())
		return(0);

	nRes = 0;
	//
	if (m_nSpeedCheckSkipCount > 0)
	{
		m_nSpeedCheckSkipCount--;
		//
		m_nSpeedCheckCountTime = 0;				// �M��������^�����p��
		goto skip_check;
	}
	//
	if(m_nSpeedCheckCount >= 3)
	{
		if (m_bSpeedCheckSkip)
		{
			m_bSpeedCheckSkip = false;
			nOdds = 0;
			goto skip;
		}
		// �C 3 �ӥ[�t�ˬd�T���p��@���~�t��
		// m_nSpeedCheckOdds = �ֿn�~�t�A�����Ȫ��ܥ[�t, �t�Ȫ��ܴ�t
		// nOdds = �����~�t�A�����Ȫ��ܥ[�t, �t�Ȫ��ܴ�t
		nOdds				= m_nSpeedCheckCount * (gameSendSpeedPacketTime * 1000) - m_nSpeedCheckTick;
		//m_nSpeedCheckOdds	+= nOdds;
		m_nSpeedCheckTick = m_nSpeedCheckOdds;	// �ɥ� m_nSpeedCheckTick�A�U���|�M��
		m_nSpeedCheckOdds = nOdds;				// �����W���~�t
		if (m_nSpeedCheckOdds > 0)				// ���|�������L�ֱ��p
			m_nSpeedCheckOdds = 0;
		nOdds += m_nSpeedCheckTick;
#ifndef USE_PACKAGE_DATA
//CMapServer::GetServer()->Log("Speed check(%d, %d)", m_nSpeedCheckOdds, nOdds);
#endif
 #ifdef NO_TIGHT_CLIENT_SPEED_CHECK
		// �ˬd�~�t
		// �ھڹ���G�Ѱ�[�t��(gameSendSpeedPacketTime = 5)
		// �[�t�� x1(���`): nOdds = -31
		// �[�t�� -x1.52: nOdds = -7657
		// �[�t�� -x1.15: nOdds = -4500
		// �[�t�� x1.07: nOdds = 969
		// �[�t�� x1.15: nOdds = 1938
		// �[�t�� x1.23: nOdds = 2812
		// �[�t�� x1.32: nOdds = 3812
		// �[�t�� x1.52: nOdds = 5000
		//
		//if (nOdds > 3400)		// 2100(�� 1.15��)	// 3400 (�� 1.2��)
		if (nOdds > 900)
		{
			// �~�t�j�� n ��, �P�w���[�t
			nRes = 1;
			//
			if (nOdds > 1800)	// �ӧ֥[�t�_
			{
				m_nSpeedCheckErrorCount++;
			}
			if (nOdds >= 3500)	// �ӧ֥ߨ��_
			{
				m_nSpeedCheckErrorCount = 100;
			}
		}
		if (nOdds < -25000)					// -25 * 1000
		{
			// �~�t�p�� -n ��, �P�w����t
			nRes = -1;
		}
 #else
		// �ˬd�j�q�~�t
		if (nOdds > 800)				// 7
		{
			// �~�t�j�� n ��, �P�w���[�t
			nRes = 1;
			if (nOdds > 1800)	// �ӧ֥ߨ��_
			{
				m_nSpeedCheckErrorCount++;
			}
			if (nOdds >= 3500)	// �ӧ֥ߨ��_
			{
				m_nSpeedCheckErrorCount = 100;
			}
		}
		if (nOdds < -25 * 1000)			// -7 * 1000
		{
			// �~�t�p�� -n ��, �P�w����t
			nRes = -1;
		}
 #endif
skip:
		m_nSpeedCheckCount = 0;			// ���s�p��ƥ�
		m_nSpeedCheckTick = 0;			// ���s�p��ɶ�
	}
	m_nSpeedCheckCountTime = 0;				// �M��������^�����p��

	if (nRes)
	{
		if (nRes > 0)
		{
			if (++m_nSpeedCheckErrorCount < 3)			// �e�\���~����
			{
				nRes = 0;
			}
		//	else
		//		CMapServer::GetServer()->Log("WARNING: Speed check(%d, %d)", m_nSpeedCheckOdds, nOdds);
		}
		else
		{
			if (++m_nSpeedCheckErrorCount < 4)			// �e�\���~����
			{
				nRes = 0;
			}
		}
	}
	// n ����M������(���m)
	//if(m_nSpeedCheckTime >= gameSendSpeedPacketTime * (3*5) * 1000)	// 75 ��
	if(m_nSpeedCheckTime >= gameSendSpeedPacketTime * (3*10) * 1000)	// 150 ��
	{
		// �p��ݭn���L���ƥ�
		if ((m_nSpeedCheckOdds < 0) && (m_nSpeedCheckSkipCount <= 0))
		{
			skip_count = ((-m_nSpeedCheckOdds) / 1000) / gameSendSpeedPacketTime;
			if (skip_count > 0)
			{
				if (skip_count > 8)		// �����j����
					skip_count = 8;
				m_nSpeedCheckSkipCount = (short)skip_count;
			}
		}
		//
skip_check:
		////m_nSpeedCheckOdds	= 0;
		//m_nSpeedCheckTime	= 0;
		m_nSpeedCheckErrorCount = 0;
		ClearAccelerateRecord(1);		// �M���֭p�p��
	}
	return(nRes);
#endif
}

#ifdef USE_FORCE_CHANNEL
void SendHistoryBattleStageForceMsg(CMapPlayer * pPlayer, struct MAP_SEND_MESSAGE_FROM_CLIENT * pSendMsg)
{
	unsigned long player_uid;
	long map_id, team_id, send_size;
	CMapPlayer * pTargetPlayer;
	CMapHistory * pManager;
	struct HISTORY_REGISTER_DATA * pRegister;
	struct HISTORY_PLAYER_TEAM_DATA * pTeam;
	std::map<unsigned long,struct HISTORY_PLAYER_TEAM_DATA>::iterator iter;
	std::map<unsigned long,struct HISTORY_PLAYER_TEAM_DATA>::iterator iter_end;

	map_id = pPlayer->GetMapCode();
	team_id = pPlayer->GetCharData()->plrHistoryTeamID;
	//self_uid = pPlayer->GetUID();
	send_size = pSendMsg->GetSize();
	//
	pManager = &CMapServer::GetServer()->m_nHistoryManager;
	if (pManager->m_mapPlayerManager.find(map_id) != pManager->m_mapPlayerManager.end())
	{
//CMapServer::GetServer()->Log("DEBUG: Pass 1");
		pRegister = &pManager->m_mapPlayerManager[map_id];
		//
		iter = pRegister->m_mapPlayerList.begin();
		iter_end = pRegister->m_mapPlayerList.end();
		while(iter != iter_end)
		{
			player_uid = iter->first;
			if (player_uid)
			{
				pTeam = &iter->second;
//CMapServer::GetServer()->Log("DEBUG: Team(%d,%s,%d)", team_id, pTeam->szName, pTeam->nTeamID);
				if (pTeam->nTeamID == team_id)
				{
					pTargetPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(player_uid, CMapPlayer::CLASS_ID);
					if (pTargetPlayer)
					{
						if (pTargetPlayer->GetMapCode() == map_id)
						{
							::msgSendData(pTargetPlayer->GetClientID(), 0, PROTO_MAP_SYSTEM_MESSAGE, (char *)pSendMsg, send_size, 0);
						}
					}
				}
			}
			//
			iter++;
		}
	}
}
#endif

void CMapPlayer::SendChatMessage(struct MAP_SEND_MESSAGE_FROM_CLIENT * pSendMsg)
{
	unsigned long c;
	long it, val;
	long spec_item_pos;
 //#ifdef ItemMode
	struct plrCARRYITEM_SAVE * pItemData;
 //#endif
#ifdef USE_FORCE_CHANNEL
	unsigned short cost_honor;
#endif
	long log_type = 0;
	char * dest_name = NULL;
//	struct MAP_SYSTEM_MESSAGE nMsg;
	CMapCharMsg *	pMsg;
	int				hLoginServer = CMapServer::GetServer()->GetLoginServer();

#ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("DEBUG: Get message(%08x)", pSendMsg->m_nChannel);
#endif
	spec_item_pos = HIWORDPTR(pSendMsg->m_nChannel);			// Client ���w�ϥιD��(�I�]���~��m+1)
	if (LOWORDPTR(pSendMsg->m_nChannel) == gameChannel_7)
		pSendMsg->m_nChannel &= 0xffff;							// �K�y�O 0x00010000

	// �T��
	if (m_nPrivilege & gamePRIVILEGE_TYPE_STOPTALK)
	{
		it = CMapServer::GetServer()->GetLoopTime();
		if (it > (long)GetSaveData()->plrPrivilege_DeuDate)
		{
			GetSaveData()->plrPrivilege_DeuDate = 0;
			m_nPrivilege &= ~gamePRIVILEGE_TYPE_STOPTALK;
			GetSaveData()->plrPrivilege &= ~gamePRIVILEGE_TYPE_STOPTALK;
			AutoSaveCharData();
		}
		else
		{
			// �O�_�K GM
		/*	if (pSendMsg->m_nChannel == gameChannel_4)
			{
				if (!_memicmp((const void *)pSendMsg->m_TalkerName, "GM", 2))
					goto ok_send;
			}
		*/
			// �T���q��
			pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_STOP_TALK_NOTIFY, 0, GetSaveData()->plrPrivilege_DeuDate);

			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			return;
		}
	}
	// �L�o�D�k�r��
//ok_send:
	if ((!pSendMsg->m_nMessageSize) || (pSendMsg->m_nMessageSize > MAX_SEND_MESSAGE_LENGTH))
		return;
	c = pSendMsg->m_Message[pSendMsg->m_nMessageSize-1] & 0xff;
	if (c >= fontcodeChinese)
	{
		if (pSendMsg->m_nMessageSize < (MAX_SEND_MESSAGE_LENGTH-1))
		{
			pSendMsg->m_Message[pSendMsg->m_nMessageSize++] = 0;
		}
		else
			pSendMsg->m_nMessageSize--;
	}
	pSendMsg->m_Message[pSendMsg->m_nMessageSize] = 0;
	//
//	nMsg.m_nUID = pSendMsg->m_nUID;
//	nMsg.m_nTalkerUID = pSendMsg->m_nUID;		// �� Client �ӻ��S��
//	nMsg.m_nChannel = pSendMsg->m_nChannel;
//	nMsg.m_nMessageSize = pSendMsg->m_nMessageSize;
//	::msg_strncpy(nMsg.m_Message, pSendMsg->m_Message, sizeof(nMsg.m_Message));
//	::msg_strncpy(nMsg.m_TalkerName, GetSaveData()->plrName, gameMAX_PLAYER_NAME_SIZE + 1);
	//
	switch(pSendMsg->m_nChannel)
	{
	case gameChannel_1:		// �@��
		if (IsOutMap())
			return;
		log_type = LOGTYPE_TALK_NORMAL;
		::msg_strncpy(pSendMsg->m_TalkerName, GetSaveData()->plrName, gameMAX_PLAYER_NAME_SIZE + 1);

		pMsg = InsertMsg(PROTO_MAP_SYSTEM_MESSAGE, CMapCharMsg::SEND_TO_ALL);
		if(pMsg != NULL)
		{
			::memcpyT(&pMsg->m_Msg.m_ChatMsg, pSendMsg, pSendMsg->GetSize());
		//	::memcpyT(&pMsg->m_Msg.m_ChatMsg, &nMsg, nMsg.GetSize());
			pMsg->m_nSize	= pSendMsg->GetSize();
		}
		break;

	case gameChannel_2:		// ����
		log_type = LOGTYPE_TALK_PARTY;
		goto mrh001;
	case gameChannel_6:		// �x�W���q
		log_type = LOGTYPE_TALK_ORGANIZE_ALLY;
		goto mrh001;
	case gameChannel_3:		// �x��
		log_type = LOGTYPE_TALK_ORGANIZE;
mrh001:
		::msg_strncpy(pSendMsg->m_TalkerName, GetSaveData()->plrName, gameMAX_PLAYER_NAME_SIZE + 1);
		CMapServer::GetServer()->SendData(hLoginServer, 0, PROTO_LOGIN_SYSTEM_MESSAGE, (char *)pSendMsg, pSendMsg->GetSize(), 0);
		//CMapServer::GetServer()->SendData(hLoginServer, 0, PROTO_LOGIN_SYSTEM_MESSAGE, (char *)&nMsg, nMsg.GetSize(), 0);
		break;
#ifdef USE_FORCE_CHANNEL
	case gameChannel_8:		// �դO�W�D
		if ( !CMapServer::GetServer()->IsConnectedToServer(hLoginServer) )
		{
			SendMsgToClientByID(20863);		// �ثe�������n�D�I
			return;
		}
		//
		cost_honor = (unsigned short)::gameGetForceChannelCost(GetSaveData()->plrCountryID);
		if (GetSaveData()->plrHonor >= cost_honor)
		{
			GetSaveData()->plrHonor -= cost_honor;
			AutoSaveCharData();
			//
			pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_HONOR, 0, GetSaveData()->plrHonor);
			
			pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			//
			// ���v�ԧЮɯS�O�B�z(���a�ϦP�����a)
			if ((m_nCharFlags & CHAR_HISTORY_BATTLE) && GetCharData()->plrHistoryTeamID)
			{
				::msg_strncpy(pSendMsg->m_TalkerName, GetSaveData()->plrName, gameMAX_PLAYER_NAME_SIZE + 1);
				SendHistoryBattleStageForceMsg(this, pSendMsg);
				//
				log_type = LOGTYPE_TALK_FORCE;
			}
			else
			{
				log_type = LOGTYPE_TALK_FORCE;
				goto mrh001;
			}
		}
		else
			return;
		break;
#endif
	case gameChannel_4:		// �K�W
		val = GetCharData()->plrSPCFlag;			// �O�_�OGM, �ϥ�GM�W�D
		if (val & (spcFLAG_GM | spcFLAG_GM2))
		{
			pSendMsg->m_nChannel = gameChannel_GM;
		}
		//
		::msg_strncpy(pSendMsg->m_TalkerName, pSendMsg->m_TalkerName, sizeof(pSendMsg->m_TalkerName));
		//
		log_type = LOGTYPE_TALK_SECRET;
		dest_name = pSendMsg->m_TalkerName;
		// �o�̪� Talker Name �O�ؼЦW�l�A�����ܧ�
		CMapServer::GetServer()->SendData(hLoginServer, 0, PROTO_LOGIN_SYSTEM_MESSAGE, (char *)pSendMsg, pSendMsg->GetSize(), 0);
		//CMapServer::GetServer()->SendData(hLoginServer, 0, PROTO_LOGIN_SYSTEM_MESSAGE, (char *)&nMsg, nMsg.GetSize(), 0);
		break;
//#ifdef ItemMode
	case gameChannel_7:		// ���W
		if ( !CMapServer::GetServer()->IsConnectedToServer(hLoginServer) )
		{
			SendMsgToClientByID(20863);		// �ثe�������n�D�I
			return;
		}
		//
		val = CMapServer::GetServer()->GetLoopTime();
		if (val >= m_nPlayerTalkWaitTime)
		{
			m_nPlayerTalkWaitTime = val + gameALL_CHANNEL_TALK_DELAY;
			//
			val = ::gameServer_GetCarryItemMax(GetSaveData());
			spec_item_pos--;
			if ((spec_item_pos < 0) || (spec_item_pos >= val))
				return;
			pItemData = GetItemData();
			val = pItemData->plrCarryItem[spec_item_pos].m_Item.itemCode;
			if ((val == gameITEM_ID_ALL_CHANNEL) || (val == gameITEM_ID_ALL_CHANNEL_2) || (val == gameITEM_ID_ALL_CHANNEL_3))
			{
				if (FindItemAndDelete(val, spec_item_pos))
				{		// �A���ӤF%d��%s�I
					SendUseItemNotify(val, 1);
					//
					if (val == gameITEM_ID_ALL_CHANNEL || val == gameITEM_ID_ALL_CHANNEL_3)
					{
						CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(-1, pSendMsg->m_Message, gameSPMSG_WAV_ALL_CHANNEL_3, this);	// gameSPMSG_WAV_PLAYERTALK
					}
					else
						CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(-1, pSendMsg->m_Message, gameSPMSG_WAV_ALL_CHANNEL_2, this);
					//
					log_type = LOGTYPE_TALK_ALL;
				}
			}
			else
			{
				SendMsgToClientByID(24343);
				return;
			}
		}
		else
			return;
		break;
//#endif
	default:
	//	CMapServer::GetServer()->KickPlayer(this);
		return;
		break;
	}
	//
	if (log_type)
	{
		if (dest_name)
		{
			CMapServer::GetServer()->SendLogMessage_Talk(this, log_type, dest_name, pSendMsg->m_Message);
		}
		else
			CMapServer::GetServer()->SendLogMessage_Talk(this, log_type, "", pSendMsg->m_Message);
	}
}

//�����a�Ϯɲέp�a�ϤH�� -- chenyin
void CMapPlayer::SetMapID(unsigned short nMapID,unsigned short nCopyUID)
{
#ifdef NEW_PLAYER_STATISTICS
	//���ܦa�Ϯɥ������¹Ϫ��H�� --chenyinyang
	CMapServer *m_Server = CMapServer::GetServer();		
	unsigned short players = m_Server->MinusMapPlayer(GetMapID());	
 #ifdef _DEBUG
	m_Server->Log("Player leave Map %hu has %hu players",GetMapID(),players);
 #endif
	CMapChar::SetMapID(nMapID,nCopyUID);

	//���ܦa�ϫ�[�J�s�Ϫ��H�� --chenyinyang
	players = m_Server->AddMapPlayer(nMapID);
 #ifdef _DEBUG
	m_Server->Log("PlayerEnter Map %hu has %hu players",nMapID,players);
 #endif
#else
	CMapChar::SetMapID(nMapID,nCopyUID);
#endif
}

bool CMapPlayer::EnterMap(void)
{
	// �M���a����w����
	MapCtrl_UnLock(1);

	if(!CMapChar::EnterMap())
		return(false);

//	// �V Login Server �n�D���o�ثe������
//	struct LOGIN_GET_PARTY_INFO nParty;
//
//	nParty.m_nUID = GetSaveData()->plrUID;
//	CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_GET_PARTY_INFO, (char *)&nParty, sizeof(nParty), 0);
////CMapServer::GetServer()->Log("Player(%d) enter map and get party info... ", GetUID());

	CarryNPC_Update();

	TowerSlashServer_UpdateClientData(this);

	return(true);
}

void CMapPlayer::SendMsgToClientByID(long msg_id, long msg_type)
{
	struct MAP_SEND_MSG_BY_ID_TO_CLIENT nMsg;

	nMsg.msg_id = msg_id;
	nMsg.msg_type = msg_type;
	::msgSendData(GetClientID(), 0, PROTO_MAP_SEND_MSG_BY_ID_TO_CLIENT, (char *)&nMsg, sizeof(nMsg), 0);
}

void CMapPlayer::SendMsgAndTimeToClientByID(long msg_id,long msg_time, long msg_type)
{
	struct MAP_SEND_MSG_BY_ID_AND_TIME_TO_CLIENT nMsg;

	nMsg.msg_id = msg_id;
	nMsg.msg_time = msg_time;
	nMsg.msg_type = msg_type;
	::msgSendData(GetClientID(), 0, PROTO_MAP_SEND_MSG_BY_ID_AND_TIME_TO_CLIENT, (char *)&nMsg, sizeof(nMsg), 0);
}

// �� map �e�T��
void CMapPlayer::ToLogin_SendPlayerMsg(unsigned long uid, char *msg)
{
	CMapPlayer * pPlayer;
	struct LOGIN_SEND_PLAYER_MSG nReq;

	if (msg && *msg)
	{
		pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);
		if (pPlayer)
		{
			pPlayer->SendMessage(gameChannel_System, NULL, msg);
			return;
		}
		//
		memset(&nReq, 0, sizeof(nReq));
		//
		nReq.nChannel = gameChannel_System;
		nReq.nPlayerUID = uid;
		msg_strncpy(nReq.szMessage, msg, sizeof(nReq.szMessage));
		nReq.nMessageSize = strlen(nReq.szMessage);
		//
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_SEND_PLAYER_MSG, (char *)&nReq, nReq.GetSize(), 0);
	}
}

// �� map �e�T��
void CMapPlayer::ToLogin_SendPlayerMsgByID(unsigned long uid, long id)
{
	CMapPlayer * pPlayer;
	struct LOGIN_SEND_PLAYER_MSG_BY_ID nReq;

	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);
	if (pPlayer)
	{
		pPlayer->SendMsgToClientByID(id);
		return;
	}
	//
	memset(&nReq, 0, sizeof(nReq));
	//
	nReq.nPlayerUID = uid;
	nReq.nMsgType = MSG_TYPE_SYS_MSG;
	nReq.nMsgID = id;
	CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_SEND_PLAYER_MSG_BY_ID, (char *)&nReq, sizeof(nReq), 0);
}

bool CMapPlayer::LeaveMap(void)
{
	int	cnt1;

	if(IsOutMap())
		return(false);

//	// �M���[�t����
//	m_nSpeedCheckTick = 0;
//	m_nSpeedCheckTime = 0;
//	m_nSpeedCheckCountTime = 0;
//	m_nSpeedCheckOdds = 0;
//	m_nSpeedCheckCount = 0;

	CMapChar::LeaveMap();		// fix by LRG (???)

	// ���}���Ľd�򪺰϶�
	for(cnt1 = 0; cnt1 < m_nSightBlocks; cnt1++)
		m_pSightBlock[cnt1]->RemoveNotify(GetHandle());
	m_nSightBlocks = 0;

//	CMapChar::LeaveMap();		// fix by LRG

	CarryNPC_Update();

	return(true);
}

// no_super_mode bit 0 �����A bit 1 �Z����ǰe
bool CMapPlayer::ChangeMap(int nMapID, long x, long y, long no_super_mode, long limit_player_count,long copy_uid)
{
//	struct MAP_CHANGE_MAP	ClientMsg;
	struct MAP_MAP_TO_LOGIN	ServerMsg;
	struct plrDATASHOWSELF Self;
	CMapServer *	pServer;
	CMapCtrl *		pMap;
	//
	CMapCharMsg *	pMsg;
	long need_calc;

	if (!IsReady())
		return(true);

	if (!nMapID)
	{
		CMapServer::GetServer()->Log("SERIOUS: (%d,%s) change to map 0", GetUID(), GetName());
		return(true);
	}


	//if (IsBotPlayer())					// ������
	//	return(true);

	MapCtrl_UnLock(1);					// 2005/03/17

	if(IsCrouch())
		Crouch();

	m_nPlayerFlags &= ~PLAYER_FLAGS_WAIT_FORCE_CHG;	// �M���դO�ǰe���ݦ^�����A
		
	pMap = CMapServer::GetServer()->FindMap(nMapID,(unsigned short)copy_uid);
	if(pMap != NULL)
	{
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("DEBUG: Change Map(%d,%s)", GetUID(), GetName());
#endif
		if (no_super_mode)		// �p�G���n�L�ġA��������
		{
			CMapServer::GetServer()->Disconnect_ClearAllRecord( GetUID(), 4 );
		}
		else
			CMapServer::GetServer()->Disconnect_ClearAllRecord( GetUID(), 5 );
		// �� Server �a��
		//
		// ���G�o�̤��i�H�ϥ� LeaveMap()�A�]���W�h�i��O�b MoveProc() �I�s�A
		//     �ϥΤF LeaveMap() �|�y�� MapCtrl �� ->MoveProc() std::map �j�馺��
		//     �Τ��w���{�H
		//
		//CMapServer::GetServer()->ChangeObjectState(GetHandle(), CMapObject::STATE_LEAVE_MAP);
		CMapServer::GetServer()->ChangeObjectState(GetHandle(), CMapObject::STATE_CHANGE_MAP);
		m_nDeleteKeepTime = PLAYER_DELETE_KEEP_TIME;

		need_calc = 0;
		// �a����w����
		if (no_super_mode & 1)				// 2005/03/17 �����~�n
		{
			MapCtrl_Lock();
			m_bIsTeleport = true;
			//
			m_nOuterDueTime = CMapServer::GetServer()->GetLoopTime() + 20;		// �̦h 20��
		}
		else
		{
			m_bIsTeleport = false;
			// �p�G�a�Ϥ��P�A�M���ǰe�]�w
			if (m_nTeleportMapCode)
			{
				if (nMapID != GetMapCode())
					m_nTeleportMapCode = 0;			// �M���ǰe�]�w
			}
			m_nTeleport2MapCode = 0;				// �ĤG�h�����M���ǰe�]�w 2010/04/01
			// �]�w���v�ԧЪ��A�A�����X
			if (CMapServer::GetServer()->m_nHistoryManager.IsVIP_Flag_Player(this))
				CMapServer::GetServer()->m_nHistoryManager.SetVIP_Flag_Player(this, 0, 1 | 2);
			// �M�����H
#if !defined(TW_PVP_MODE)
			pStatus_Clear(effFun_RED, 1);
#endif
			// �M�w�O�_�q�L�d�m��
			if (IsCamelStage(GetMapCode()))
				need_calc ^= 1;
			if (IsCamelStage(nMapID))
				need_calc ^= 1;
#ifdef STAGE_EFFECT
			need_calc ^= 1;//�a�ϯS�ʶ}�һݭp��
#endif
		}

		// By LRG
		LeaveMapBeforeChangeMap();		// �b���ܦ�m�e���������

		//ExitShop(1);
#if 0
		CMapServer::GetServer()->Log("���(%u)%s �л���ͼ  ��(%u,%u) -> (%d,%d)",GetUID(),GetSaveData()->plrName,GetMapCode(),GetMapCopyUID(),nMapID,copy_uid);
#endif
		SetMapID(nMapID,(unsigned short)copy_uid);
		SetPos(x, y);
		// ��m��s�s���קK���ǰe
		//GetSaveData()->plrMapCode = nMapID;
		//GetSaveData()->plrPosX = x;
		//GetSaveData()->plrPosY = y;
		AutoSaveCharData();


		if (no_super_mode & 1)			// �p�G���n�L�ġA��������
		{
			//AutoSaveCharData();
			//AutoSaveCharData(1);
			//AutoSaveItemData(1);
			//AutoSaveSkillData(1);
			//AutoSaveStorageData(1);
			//AutoSaveNPCData(1);
			SaveAuotSaveData();
			//
			m_nDeleteKeepTime = 5;		// �n�O�d�@�U�l
		}
		else
		{
			////SaveAllData();
			//SaveAllData_Lite();
			//AutoSaveCharData(1);
			//AutoSaveItemData(1);
			//AutoSaveSkillData(1);
			//AutoSaveStorageData(1);
			//AutoSaveNPCData(1);
			SaveAuotSaveData();
			//
			if (no_super_mode & 2)
			{
				m_bIsTeleport = true;
			}
			else
			{
				m_nIsChallenger = 0;	// ���s�M�w�Z��J�����A
				//
				// �d�m�����s�M�w���ʳt��
				if (need_calc)
				{
					GetCharData()->plrFlags &= ~(CHARFLAGS_NO_ATTACK | CHARFLAGS_NO_ATTACK_MASK);//�����a�Ϯɭ���
					gameServer_CalcCharacterAttribute(GetCharData());
					::memset(&Self, 0, sizeof(plrDATASHOWSELF));
					::gameServer_MakeShowSelf(GetCharData(), &Self);
					::msgSendData( GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
					UpdateShowData();
				}
				//
#ifdef TW_PVP_MODE
				m_nCrimeClearTick = 0;					// ����ɶ�
#endif
			}
		}

		// �q�� Login Server ��m��s
		struct LOGIN_UPDATE_PLAYER_MAP_POS nMapPos;
		int hLoginServer = CMapServer::GetServer()->GetLoginServer();

		nMapPos.nUID = GetUID();
		nMapPos.nMapCode = nMapID;
		nMapPos.nPosX = x;
		nMapPos.nPosY = y;
		CMapServer::GetServer()->SendData(hLoginServer, 0, PROTO_LOGIN_UPDATE_PLAYER_MAP_POS, (char *)&nMapPos, sizeof(nMapPos), 0);

		SetReady(false, no_super_mode);

		m_TeleportKickTime = CMapServer::GetServer()->GetLoopTime() + 15;

		// GM �|�]�������P�A�m�W���P
		if (GetSaveData()->plrBaseSPCFlag & (spcFLAG_GM | spcFLAG_GM2))
		{
			if (!no_super_mode)
			{
				gameServer_MakeShow(m_pCharData, &m_pCharData->plrShowData);
				//
				pMsg = ReplaceMsg(PROTO_MAP_UPDATE_OBJECT_SHOW, CMapCharMsg::SEND_TO_SELF);
				pMsg->m_nSize	= sizeof(struct MAP_UPDATE_OBJECT_SHOW);
				pMsg->m_Msg.m_UpdateShowMsg.m_nUID	= GetUID();
				::memcpyT(&pMsg->m_Msg.m_UpdateShowMsg.m_DataShow, GetShowData(), sizeof(struct plrDATASHOW));
				SendMsgToPlayer();
			}
		}


	//	ClientMsg.m_nMapCode	= nMapID;
	//	ClientMsg.m_Pos.x		= x;
	//	ClientMsg.m_Pos.y		= y;
	//	ClientMsg.m_nMapCountry	= CMapServer::GetServer()->GetTownDataByID(pMap->GetID())->ptCountryID;
	//	ClientMsg.m_nMapMode	= pMap->GetStageData()->gstgMode;
	//	::msgSendData(GetClientID(), 0, PROTO_MAP_CHANGE_MAP, (char *)&ClientMsg, sizeof(struct MAP_CHANGE_MAP), 0);
		return(true);
	}
	else
	{
		// ��L Server �a��(�� Login Server �T�{��B�z)
		//CMapServer::GetServer()->ChangeObjectState(GetHandle(), CMapObject::STATE_LEAVE_MAP);

		//ExitShop(0);

//if (limit_player_count)			// ����
//	limit_player_count = 1;

		m_ChangeMapServer.m_nMapCode	= nMapID;
		m_ChangeMapServer.m_Pos.x		= x;
		m_ChangeMapServer.m_Pos.y		= y;

		ServerMsg.m_nMapCode	= nMapID;
		ServerMsg.m_nUID		= GetUID();
		ServerMsg.nAccountID	= GetAccountID();
		ServerMsg.m_nLimitPlayer = (unsigned short)limit_player_count;
		ServerMsg.nMapFlags = 0;
		if (IsBotPlayer())
			ServerMsg.nMapFlags |= LOGIN_CHAR_FLAGS_IS_BOT;
		ServerMsg.nMapFlags |= m_nWebIP_Flags;
		pServer = CMapServer::GetServer();
		pServer->SendData(pServer->GetLoginServer(), 0, PROTO_MAP_MAP_TO_LOGIN, (char *)&ServerMsg, sizeof(struct MAP_MAP_TO_LOGIN), 0);
		//
	//	m_nTeleportMapCode = 0;			// �M���ǰe�]�w	2010/01/12
		m_nTeleport2MapCode = 0;		// �M���ǰe�]�w 2010/04/01
	}
	return(false);
}

// �n�JŪ���� �P �_���� �P���Ȩƥ�� �ˬd�O�_�������S���a�I
// save_data_only �u���ܦs�ɸ��
long CMapPlayer::ChangeMapInStageMapPos(long save_data_only)
{
	long r = 0;
	struct gs_StageData * pStage;
	long xrange, yrange;
	long mapid, x, y;
	
	pStage = gameStageGetPtr(GetMapID());
	if (pStage->gstMapPos[0])
	{
		x = (pStage->gstMapPos[3] + pStage->gstMapPos[1]) / 2;
		y = (pStage->gstMapPos[4] + pStage->gstMapPos[2]) / 2;
		xrange = pStage->gstMapPos[3] - pStage->gstMapPos[1] + 1;
		yrange = pStage->gstMapPos[4] - pStage->gstMapPos[2] + 1;
		mapid = pStage->gstMapPos[0] & ~0x80000000;
		//
		x = x + gameGetRandomRange(0, xrange) - (xrange/2);
		y = y + gameGetRandomRange(0, yrange) - (yrange/2);
		//
		GetSaveData()->plrMapCode = (unsigned short)mapid;
		GetSaveData()->plrMapCopyCode = 0;
		GetSaveData()->plrPosX = x;
		GetSaveData()->plrPosY = y;
		if (!save_data_only)
		{
			// 2010/01/29 �קK�ǰe����(����G���Ǧa�ϳ]�w�O�P�a��)
		//	m_nTeleportDueTime = CMapServer::GetServer()->GetLoopTime();
		//	m_nTeleportMapCode = mapid;
		//	m_nTeleportPosX = x;
		//	m_nTeleportPosY = y;
			//
			//if (ChangeMap(mapid, x, y))
			if (CMapServer::GetServer()->ChangeSaveMap(this, mapid, x, y))
			{
				if (IsDead())
					ResurrectImmed(0);
			}
		}
		r++;
	}
	return(r);
}

// hp_percent = 0 �����ץ���
void CMapPlayer::ResurrectImmed(long hp_percent)		// ��a�_��
{
	long hp;
	CMapCharMsg * pFixMsg;
//	CMapCharMsg * pMsg;

	if(!IsDead())
		return;

	//pStatus_Clear(effFun_DEAD | effFun_INVISIBLE | effFun_SITE, 0);
	pStatus_Clear(effFun_SPECIAL_ALL, 0);
	// �M�����`
//	GetSaveData()->plrStatus = 0;
//	GetShowData()->plrStatus = 0;
//	if (GetCharData()->plrSPCFlag & (spcFLAG_GM | spcFLAG_GM2))
//	{
//		GetShowData()->plrStatus = effFun_GM;
//	}
	Status_Clear();
	//
	if (!hp_percent)
	{
		hp = GetMaxHP() / 5;
	}
	else
		hp = GetMaxHP() * hp_percent / 100;
	if (hp < 1) hp = 1;
	SetHP(hp);
	UpdatePlayerDDE_Login();
	//
	// �s�� ���A
/*	pMsg = ReplaceMsg(PROTO_MAP_GET_PLAYER_STATUS_RESULT, CMapCharMsg::SEND_TO_ALL);
	pMsg->m_nSize = sizeof(struct MAP_GET_PLAYER_STATUS_RESULT);
	pMsg->m_Msg.m_UseItemStatusChange.plrUID = GetUID();
	pMsg->m_Msg.m_UseItemStatusChange.plrStatus = GetShowData()->plrStatus;
*/
	UpdateCharStatus(1);
	//
	UpdatePlayerHPMP();
	/*
	   Carry soldiers leave the map when their master is dead/out.
	   Force an immediate soldier refresh on owner resurrect so they do not
	   remain stuck in a fake-dead/outer state until some later update.
	*/
	CarryNPC_Update();
	UpdateCarryNPCTable();
	//
	/* 强制同步复活后坐标，避免其他客户端仍用尸体旧坐标攻击。 */
	pFixMsg = ReplaceMsg(PROTO_MAP_FIX_CHAR_POS, CMapCharMsg::SEND_TO_ALL);
	pFixMsg->m_nSize = sizeof(struct MAP_FIX_CHAR_POS);
	pFixMsg->m_Msg.m_FixCharPos.m_nUID = GetUID();
	pFixMsg->m_Msg.m_FixCharPos.m_Pos.x = GetPosX();
	pFixMsg->m_Msg.m_FixCharPos.m_Pos.y = GetPosY();
}

void CMapPlayer::Resurrect(void)
{
	long	nMapID, x, y;

	if(!IsDead())
		return;

	nMapID	= GetSaveData()->plrSaveMapCode;
	x		= GetSaveData()->plrSavePosX;
	y		= GetSaveData()->plrSavePosY;

	//if (ChangeMap(nMapID, x, y))
	if (CMapServer::GetServer()->ChangeSaveMap(this, nMapID, x, y))
	{
		//SetHP(GetMaxHP() / 5);
		ResurrectImmed(0);
	}
}

void CMapPlayer::UpdateSightBlock(void)
{
	CMapBlock *	pOldBlock[MAP_BLOCK_COUNT_ALL];
	int			nOldBlocks;
	int			cnt1, cnt2;
	bool		IsFound;
	int			hasNewSightBlock;


	if(IsOutMap())
		return;

	nOldBlocks = 0;			// LRG find bug at 2004/2/16
	if(m_nSightBlocks > 0)
	{
		CopyMemory(pOldBlock, m_pSightBlock, m_nSightBlocks * sizeof(CMapBlock *));
		nOldBlocks = m_nSightBlocks;
	}

	CMapChar::UpdateSightBlock();

/*	// ��s���İ϶��q������
	if(nOldBlocks > 0)
	{
		// ���}���Ľd�򪺰϶�
		for(cnt1 = 0; cnt1 < nOldBlocks; cnt1++)
		{
			IsFound = false;
			for(cnt2 = 0; cnt2 < m_nSightBlocks; cnt2++)
			{
				if(pOldBlock[cnt1] == m_pSightBlock[cnt2])
				{
					IsFound = true;
					break;
				}
			}
			if(!IsFound)
			{
				pOldBlock[cnt1]->RemoveNotify(GetHandle());
			}
		}
	}

	// �s�i�J���Ľd�򪺰϶�
	for(cnt1 = 0; cnt1 < m_nSightBlocks; cnt1++)
	{
		IsFound = false;
		for(cnt2 = 0; cnt2 < nOldBlocks; cnt2++)
		{
			if(m_pSightBlock[cnt1] == pOldBlock[cnt2])
			{
				IsFound = true;
				break;
			}
		}
		if(!IsFound)
			m_pSightBlock[cnt1]->InsertNotify(GetHandle());
	}
*/
	// �s�i�J���Ľd�򪺰϶�
	CMapBlock * pBlk;
	hasNewSightBlock = 0;

	for(cnt1 = 0; cnt1 < m_nSightBlocks; cnt1++)
	{
		pBlk = m_pSightBlock[cnt1];
		IsFound = false;
		for(cnt2 = 0; cnt2 < nOldBlocks; cnt2++)
		{
			if(pBlk == pOldBlock[cnt2])
			{
				pOldBlock[cnt2] = NULL;
				IsFound = true;
				break;
			}
		}
		if(!IsFound)
		{
			pBlk->InsertNotify(GetHandle());
			hasNewSightBlock = 1;
		}
	}
	// ��s���İ϶��q������
	if(nOldBlocks > 0)
	{
		// ���}���Ľd�򪺰϶�
		for(cnt1 = 0; cnt1 < nOldBlocks; cnt1++)
		{
			if (pBlk = pOldBlock[cnt1])
			{
				pBlk->RemoveNotify(GetHandle());
			}
		}
	}

	/* Transform foot aura safety-sync when entering new sight blocks (map switch / cross-block enter). */
	if (hasNewSightBlock)
	{
		struct plrDATA_SAVE* pSave = GetSaveData();
		if (pSave &&
			pSave->plrTransformNpcCode > 0 &&
			pSave->plrTime_General_a > (unsigned long)CMapServer::GetServer()->GetLoopTime())
		{
			long rank = (pSave->plrTransformEquipWeapon >> 4) & 0x0F;
			if (rank < 0) rank = 0;
			if (rank > 4) rank = 4;
			{
				CMapCharMsg* pAuraMsg = InsertMsg(PROTO_MAP_USE_ITEM_RESULT_ADD_HP_MP, CMapCharMsg::SEND_TO_ALL);
				if (pAuraMsg)
				{
					pAuraMsg->m_nSize = sizeof(struct MAP_USE_ITEM_RESULT_ADD_HP_MP);
					pAuraMsg->m_Msg.m_UseItemAddHpMp.plrUID = GetUID();
					pAuraMsg->m_Msg.m_UseItemAddHpMp.add_HP_MP_Type = UIR_EFFECT;
					pAuraMsg->m_Msg.m_UseItemAddHpMp.addHP = 39000 + rank;
					pAuraMsg->m_Msg.m_UseItemAddHpMp.addMP = TRANSFORM_EFFECT_SIGNATURE_MP;
					pAuraMsg->m_Msg.m_UseItemAddHpMp.itemID = 0;
				}
			}
		}
	}
}

//***************************************** Load Data *****************************************

int CMapPlayer::ProtocoToLoadType(long nProtoco)
{
	int	cnt1;

	for(cnt1 = 0; cnt1 < DB_TOTAL_TYPES_ALL; cnt1++)
	{
		if(m_nLoadProtoco[cnt1] == nProtoco)
			return(cnt1);
	}

	return(-1);
}

long CMapPlayer::InnerCheckDupeItem(struct itemDATA_SAVE * pItem, std::map<__int64,long> & mapDupe, long fix_flags, long clear)
{
	long r = 0;
	__int64 iuid;

	if (pItem->m_Item.itemCode)
	{
		if (DataCheckXml::Test(pItem))
			goto xml_set_delete;

		iuid = *(__int64 *)&pItem->m_Item.itemUID;
		if (mapDupe.find(iuid) == mapDupe.end())
		{
			mapDupe[iuid] = 1;
			// �ˬd������d
			if (::IsItem_Card(pItem->m_Item.itemCode))
			{
				pItem->m_CardItem.itemStatus = gameVCARD_STATUS_NONE;
				AutoSaveItemData();
				//
				long hVTServer;

				hVTServer = CMapServer::GetServer()->GetVTServer();
				if ( CMapServer::GetServer()->IsConnectedToServer(hVTServer) )
				{
					struct VT_CARD_ASK_RECORD nAskVTCard;

					nAskVTCard.nMode = 2;	// �Ҧ�2�G�{��
#ifdef USE_BIG_VTCARD_ITEM
					nAskVTCard.nData = pItem->m_CardItem.itemCardFuncID;
#else
					nAskVTCard.nData = 0;
#endif
					nAskVTCard.nPlayerUID = GetUID();
					msg_strncpy(nAskVTCard.szAccount, GetSaveData()->plrAccount, sizeof(nAskVTCard.szAccount));
					msg_strncpy(nAskVTCard.szCardNo, pItem->m_CardItem.itemCardNo, sizeof(nAskVTCard.szCardNo));
					CMapServer::GetServer()->SendData(hVTServer, 0, PROTO_VT_CARD_ASK_RECORD, (char *)&nAskVTCard, sizeof(nAskVTCard), 0);
				}
			}
			else
			{
#if !(defined(GBMode) || defined(GBMode_TW))// �j�����@���ˬd
				if (!(fix_flags & 1))		// �ץ����i���|�����~���|
#endif
				{
					/* 士兵存档用 m_NPC：itemNumber 与 inpcNumber 偏移不同；误读 itemNumber 常落到 inpcAddAttack 低字节，1级兵多为0会被当成空格清掉 */
					if (pItem->m_Item.itemFlags & itemSHOW_FLAG_ALL_CNPC)
					{
						if (pItem->m_NPC.inpcNumber > 1)
						{
							r++;
							pItem->m_NPC.inpcNumber = 1;
						}
					}
					else if (!IsItemMergable(pItem->m_Item.itemCode))
					{
						if (!pItem->m_Item.itemNumber || (pItem->m_Item.itemNumber > 1))
						{
							r++;
							//
							if (!pItem->m_Item.itemNumber)
							{
								memset(pItem, 0, sizeof(struct itemDATA_SAVE));
							}
							else
								pItem->m_Item.itemNumber = 1;
						}
					}
				}
			}
		}
		else	// ���ƻs
		{
xml_set_delete:
			r++;
			//
			if (clear)
			{
				CMapServer::GetServer()->Log("ERROR: Dupe item(%d, %s, %d)!", GetUID(), GetSaveData()->plrName, pItem->m_Item.itemCode);
				CMapServer::GetServer()->SendLogMessage_Item(this, LOGTYPE_ITEM_DUPE, NULL, pItem);
				//
				memset(pItem, 0, sizeof(struct itemDATA_SAVE));
			}
		}
	}
	return(r);
}

// �ˬd�ƻs���~(���j)
void CMapPlayer::CheckDupeItem()
{
	struct plrDATA_SAVE * pSave;
	long r, cnt1, val;
	long fix_flags;
	struct plrDATASHOWSELF Self;
	struct plrCARRYITEM_SAVE * pItemSave;
	struct plrSTORAGEITEM_SAVE * pStorageSave;
	std::map<__int64,long> mapDupe;

	mapDupe.clear();
	pSave = GetSaveData();
	fix_flags = pSave->plrFixDataFlags;
	if (!(fix_flags & FIX_CHAR_DUPE_ITEM))		// �ץ��X��
	{
		pSave->plrFixDataFlags |= FIX_CHAR_DUPE_ITEM;
		AutoSaveCharData();
	}
	// ..... �˳� .....
	//
	r = 0;
	r |= InnerCheckDupeItem(&pSave->plrEquipArmor, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipFoot, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipGlove, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipHead, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipHorse, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipOther1, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipOther2, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipP, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipUnderwear, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipWeaponL, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipWeaponR, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipWeaponL2, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipWeaponR2, mapDupe, fix_flags);
	//xavi�t�����
	r |= InnerCheckDupeItem(&pSave->plrEquipJade, mapDupe, fix_flags);
	r |= InnerCheckDupeItem(&pSave->plrEquipHideWeapon, mapDupe, fix_flags);

#ifdef USE_EQUIP_CARD
	for(int i=0; i < gameMAX_EQUIP_CARD; ++i)
		r |= InnerCheckDupeItem(&pSave->plrEquipCard[i], mapDupe, fix_flags);
#endif

	r = r << 1;
	// ...... �x�έܮw ........ 2007/11/19 Bug fix
	if (m_vArmyStorageItem.size() > 0)
	{
		struct itemDATA_SAVE * pItem;
		LOGIN_ARMY_STORAGE_UPDATE_ITEM sendmsg;

		memset(&sendmsg, 0, sizeof(sendmsg));
		//
		//r = 0;
		for(cnt1=0; cnt1<gameMAX_ORGANIZE_STOREITEM; cnt1++)
		{
			if (pItem = GetArmyStorageItem(cnt1))
			{
				val = InnerCheckDupeItem(pItem, mapDupe, fix_flags);
		//		r |= val;
				//
				if (val)
				{
					sendmsg.playerUID = GetUID();
					sendmsg.itemIdx = cnt1;
					//sendmsg.itemSave = *it;

					CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_ARMY_STORAGE_UPDATE_ITEM, (char*)&sendmsg, sizeof(sendmsg), 0);
				}
			}
		}
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("DEBUG: �ˬd�έܽƻs���... ");
#endif
	}
#ifndef USE_PACKAGE_DATA
	else
	{
		CMapServer::GetServer()->Log("DEBUG: �ˬd�ƻs�S���έܸ�� !");
	}
#endif
	// ..... ���W��a .....
	if (pItemSave = GetItemData())
	{
		//for (cnt1=0; cnt1<gameMAX_CARRYITEM; cnt1++)
		val = ::gameServer_GetCarryItemMax(GetSaveData());
		for (cnt1=0; cnt1<val; cnt1++)
		{
			r |= InnerCheckDupeItem(&pItemSave->plrCarryItem[cnt1], mapDupe, fix_flags);
		}
	}
	if (r & 1)
		UpdateClientItemData();
	// ���s�p��t���P���
	if (r)
	{
		gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
		gameServer_CalcCharacterAttribute(GetCharData());
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(GetCharData(), &Self);
		::msgSendData( GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		UpdateShowData();
		//
		if (r & 2)
			AutoSaveCharData();
		if (r & 1)
			AutoSaveItemData();
	}
	// ..... �ܮw .....
	r = 0;
	if (pStorageSave = GetStorageData())
	{
		//for (cnt1=0; cnt1<gameMAX_STORAGEITEM; cnt1++)
		val = gameServer_GetStorageItemMax(GetSaveData());
		for (cnt1=0; cnt1<val; cnt1++)
		{
			r |= InnerCheckDupeItem(&pStorageSave->psStoreItem[cnt1], mapDupe, fix_flags);
		}
	}
	if (r)
		SaveStorageData();
}

// �s���ܮw���ˬd�ƻs�~
void CMapPlayer::CheckInStorageDupeItem()
{
	sdSHOPDATA * pShop = ::gameGetShopPtr( GetShopID() );
	if( pShop ) 
	{
		if( pShop->sdShopType == shopType_Storage ) //�ܮw
		{
			// �Ȯɤ��ϥ�
			AutoSaveCharData(1);
			AutoSaveItemData(1);
			AutoSaveStorageData(1);
		}
	}
}

/* 品阶石/星级：itemRandomLevel 低4位=品阶(1~6)，仅高4位参与「等级需求」；旧装备仍用整字节(可负) */
static long Inner_EquipItemRandomLevelForReq(char itemRandomLevel)
{
	unsigned char u = (unsigned char)itemRandomLevel;
	unsigned char tier = u & 0x0F;
	if (tier >= 1 && tier <= 6)
		return (long)((u >> 4) & 0x0F);
	return (long)(signed char)itemRandomLevel;
}

long InnerCheckIllegalLevelEquipItem(unsigned long lvl, struct itemDATA_SAVE * pItem1, struct itemDATA_SAVE * pItem2)
{
	long itemRandomLv,itemReduceLv;
	struct itemDATA * pItem;

	if (pItem1->m_Item.itemCode)
	{
		if (pItem = ::gameGetItemPtr(pItem1->m_Item.itemCode))
		{
			itemRandomLv = Inner_EquipItemRandomLevelForReq(pItem1->m_Item.itemRandomLevel);
			itemReduceLv = pItem1->m_Item.itemReduceLV;
			if( lvl < (unsigned long)(pItem->itemUseLevel + itemRandomLv - itemReduceLv) ) 
			{
				return(1);
			}
		}
	}
	if (pItem2->m_Item.itemCode)
	{
		if (pItem = ::gameGetItemPtr(pItem2->m_Item.itemCode))
		{
			itemRandomLv = Inner_EquipItemRandomLevelForReq(pItem2->m_Item.itemRandomLevel);
			itemReduceLv = pItem2->m_Item.itemReduceLV;
			if( lvl < (unsigned long)(pItem->itemUseLevel + itemRandomLv - itemReduceLv) ) 
			{
				return(1);
			}
		}
	}
	return(0);
}

// �ˬd�D�k�˳ƪ��A
void CMapPlayer::CheckIllegalEquipItem()
{
	struct itemDATA_SAVE * pItem1;
	struct itemDATA_SAVE * pItem2;
	long type1, type2;
	struct plrDATA * pCharData;
	//
	unsigned long lvl;

	m_nPlayerFlags &= ~PLAYER_FLAGS_EQUIP_ERR;
	pCharData = GetCharData();
	// use MapServerMenuLib.cpp
	pItem1 = ::GetPlrEquipPtr(pCharData, EQUIP_POS_WEAPONR1);
	pItem2 = ::GetPlrEquipPtr(pCharData, EQUIP_POS_WEAPONL1);
	if ((pItem1->m_Item.itemCode) && (pItem2->m_Item.itemCode))		// �D�k���˳�
	{
		type1 = ::gameGetItemTypeByID(pItem1->m_Item.itemCode);
		type2 = ::gameGetItemTypeByID(pItem2->m_Item.itemCode);
		if (!(type1 & GetHandEquipSuitType(type2)))
		{
			m_nPlayerFlags |= PLAYER_FLAGS_EQUIP_ERR;
			return;
		}
	}
	// �D�k���Ÿ˳�
	lvl = GetLevel();
	if (InnerCheckIllegalLevelEquipItem(lvl, pItem1, pItem2))
	{
		m_nPlayerFlags |= PLAYER_FLAGS_EQUIP_ERR;
		return;
	}
	//
	pItem1 = ::GetPlrEquipPtr(pCharData, EQUIP_POS_WEAPONR2);
	pItem2 = ::GetPlrEquipPtr(pCharData, EQUIP_POS_WEAPONL2);
	if ((pItem1->m_Item.itemCode) && (pItem2->m_Item.itemCode))
	{
		type1 = ::gameGetItemTypeByID(pItem1->m_Item.itemCode);
		type2 = ::gameGetItemTypeByID(pItem2->m_Item.itemCode);
		if (!(type1 & GetHandEquipSuitType(type2)))
		{
			m_nPlayerFlags |= PLAYER_FLAGS_EQUIP_ERR;
			return;
		}
	}
	// �D�k���Ÿ˳�
	if (InnerCheckIllegalLevelEquipItem(lvl, pItem1, pItem2))
	{
		m_nPlayerFlags |= PLAYER_FLAGS_EQUIP_ERR;
		return;
	}
}

long CMapPlayer::IsIllegalEquipItem()
{
	if (m_nPlayerFlags & PLAYER_FLAGS_EQUIP_ERR)
		return(1);
	return(0);
}

// �M���ª����A���
long InnerCheckNPCItemStatus(struct itemDATA_SAVE * pItem)
{
	if (pItem->m_Item.itemCode)
	{
		if (pItem->m_NPC.inpcFlags & itemSHOW_FLAG_ALL_CNPC)
		{
			if (!(pItem->m_NPC.inpcFlags & itemSHOW_FLAG_FUNCTION2))
			{
				pItem->m_NPC.inpcFlags |= itemSHOW_FLAG_FUNCTION2;
				//
				pItem->m_NPC.inpcUnUsed3 = 0;
				memset(pItem->m_NPC.inpcErrorStatusData, 0, sizeof(pItem->m_NPC.inpcErrorStatusData));
				return(1);
			}
		}
	}
	return(0);
}

// ���m�s���A���(�u����@��)
void CMapPlayer::InnerResetStatusData()
{
	struct plrDATA_SAVE * pSave;
	long i, val;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	struct plrCARRYITEM_SAVE * pItemSave;
	struct plrSTORAGEITEM_SAVE * pStorageSave;

	pSave = GetSaveData();
	if (!(pSave->plrFixDataFlags & FIX_CHAR_FUNCTION2))
	{
		pSave->plrFixDataFlags |= FIX_CHAR_FUNCTION2;
		gameEff_ClearObjectSingleStatus(0, GetCharData(), 0, -1);
		AutoSaveCharData();
		// �M�� NPC Save Data
		pNPCData = GetNPCData();
		for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
		{
			pNPCSaveData = &pNPCData->plrNPCData[i];
			if (pNPCSaveData->plrNPC_UID)
			{
				pNPCSaveData->plrNPC_Status = 0;
				pNPCSaveData->plrNPC_Status2 = 0;
				memset(pNPCSaveData->plrNPC_HelpStatusData, 0, sizeof(pNPCSaveData->plrNPC_HelpStatusData));
				memset(pNPCSaveData->plrNPC_ErrorStatusData, 0, sizeof(pNPCSaveData->plrNPC_ErrorStatusData));
				pNPCSaveData->plrNPC_Unused = 0;
				//
				AutoSaveNPCData();
			}
		}
		// �M�����W���~
		if (pItemSave = GetItemData())
		{
			val = ::gameServer_GetCarryItemMax(GetSaveData());
			for (i=0; i<val; i++)
			{
				if (InnerCheckNPCItemStatus(&pItemSave->plrCarryItem[i]))
					AutoSaveItemData();
			}
		}
		// �M���ܮw���~
		if (pStorageSave = GetStorageData())
		{
			val = gameServer_GetStorageItemMax(GetSaveData());
			for (i=0; i<val; i++)
			{
				if (InnerCheckNPCItemStatus(&pStorageSave->psStoreItem[i]))
					AutoSaveStorageData();
			}
		}
		// �M���x�έܮw���~�� db server�B�z
	}
}

#if (defined(USE_BLACKLIST_NO_MSG) || defined(CROSS_SERVER_SYSTEM))
void InnerSendBlackListData(CMapPlayer * pMapPlayer)
{
	struct plrDATA_FRIEND * pplrDATA_FRIEND;
	struct MAP_ASK_FRIEND_TABLE_RESULT sendmsg;

	if (pplrDATA_FRIEND = pMapPlayer->GetFriendData())
	{
		sendmsg.m_DataFriends = *pplrDATA_FRIEND;
		sendmsg.m_nMateInfo.nMateSubNumber = 255;		// ���ܵL�t�����

		::msgSendData( pMapPlayer->GetClientID(), 0, PROTO_MAP_ASK_FRIEND_TABLE_RESULT, (char *)&sendmsg, sizeof( sendmsg ), 0 );
	}
}
#endif

void CMapPlayer::LoadAllData(long nProtoUID, long is_first_login)
{
	LoadCharData(nProtoUID);
	//
#ifdef USE_COOL_DOWN_SYSTEM
	if (is_first_login)				// �Ĥ@���n�J�a��
	{
		CMapServer::GetServer()->GetCoolDownData(GetUID(), 1);
	}
	else
		CMapServer::GetServer()->GetCoolDownData(GetUID(), 0);
#endif
	//
	LoadSkillData(nProtoUID);
	LoadItemData(nProtoUID);
	LoadStorageData(nProtoUID);
	LoadNPCData(nProtoUID);
	LoadMissionData(nProtoUID);
	LoadFriendData(nProtoUID);
	LoadFashionData(nProtoUID);
	LoadTianshuData(nProtoUID);		// 加载天枢数据（需要在登录时加载以应用属性）
	LoadAvatarAwakenData(nProtoUID);	// 加载化身觉醒数据（用于恢复卡片觉醒等级）
	LoadDungeonClearData(nProtoUID);	// 加载副本已通关数据
	LoadCnpcItemSkillData(nProtoUID);	// 加载小兵道具技能（独立 dat，不进必载 8 类）
	LoadTowerSlashData(nProtoUID);		// 加载过关斩将进度

#if defined USE_DATA_STATUS
	m_charStatus.message = DSMSG_GET;
	m_charStatus.wparam = GetUID();
	m_charStatus.lparam = 0;
	CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetDBServer(), nProtoUID, PROTO_DB_DATASTATUS, (char *)&m_charStatus, sizeof(m_charStatus), 0);
#endif
}

void CMapPlayer::LoadCharData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_SHOWSELF	ShowSelf;
	int									hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_CHAR_DATA] > 0)
		return;
	m_nLoadTime[DB_CHAR_DATA]	= DB_TIMEOUT;

	ShowSelf.uid		= GetUID();
	ShowSelf.nAccountID	= GetAccountID();
	memset(ShowSelf.szAccount, 0, sizeof(ShowSelf.szAccount));
	m_IsLoadingData = true;
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_SHOWSELF, (char *)&ShowSelf, sizeof(ShowSelf), 0);
}

void CMapPlayer::LoadSkillData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_SKILL	SkillMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_SKILL_DATA] > 0)
		return;
	m_nLoadTime[DB_SKILL_DATA]	= DB_TIMEOUT;

	SkillMsg.nUID		= GetUID();
	SkillMsg.nAccountID	= GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_SKILL, (char *)&SkillMsg, sizeof(SkillMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadItemData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_ITEM	ItemMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_ITEM_DATA] > 0)
		return;
	m_nLoadTime[DB_ITEM_DATA]	= DB_TIMEOUT;

	ItemMsg.nUID		= GetUID();
	ItemMsg.nAccountID	= GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_ITEM, (char *)&ItemMsg, sizeof(ItemMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadStorageData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_STORAGE	StorageMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_STORAGE_DATA] > 0)
		return;
	m_nLoadTime[DB_STORAGE_DATA]	= DB_TIMEOUT;

	StorageMsg.nUID			= GetUID();
	StorageMsg.nAccountID	= GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_STORAGE, (char *)&StorageMsg, sizeof(StorageMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadNPCData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_NPC	NPCMsg;
	int							hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_NPC_DATA] > 0)
		return;
	m_nLoadTime[DB_NPC_DATA]	= DB_TIMEOUT;

	NPCMsg.nUID			= GetUID();
	NPCMsg.nAccountID	= GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_NPC, (char *)&NPCMsg, sizeof(NPCMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadMissionData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_MISSION	MissionMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_MISSION_DATA] > 0)
		return;
	m_nLoadTime[DB_MISSION_DATA]	= DB_TIMEOUT;

	MissionMsg.nUID			= GetUID();
	MissionMsg.nAccountID	= GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_MISSION, (char *)&MissionMsg, sizeof(MissionMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadFriendData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_FRIEND	FriendMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_FRIEND_DATA] > 0)
		return;
	m_nLoadTime[DB_FRIEND_DATA]	= DB_TIMEOUT;

	FriendMsg.nUID			= GetUID();
	FriendMsg.nAccountID	= GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_FRIEND, (char *)&FriendMsg, sizeof(FriendMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadTianshuData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_TIANSHU	TianshuMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_TIANSHU_DATA] > 0)
		return;
	m_nLoadTime[DB_TIANSHU_DATA] = DB_TIMEOUT;

	TianshuMsg.nUID			= GetUID();
	TianshuMsg.nAccountID	= GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_TIANSHU, (char *)&TianshuMsg, sizeof(TianshuMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadAvatarAwakenData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_AVATAR_AWAKEN loadMsg;
	int hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_AVATAR_AWAKEN_DATA] > 0)
		return;
	m_nLoadTime[DB_AVATAR_AWAKEN_DATA] = DB_TIMEOUT;

	loadMsg.nUID = GetUID();
	loadMsg.nAccountID = GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_AVATAR_AWAKEN, (char *)&loadMsg, sizeof(loadMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadDungeonClearData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_DUNGEON_CLEAR loadMsg;
	int hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_DUNGEON_CLEAR_DATA] > 0)
		return;
	m_nLoadTime[DB_DUNGEON_CLEAR_DATA] = DB_TIMEOUT;

	loadMsg.nUID = GetUID();
	loadMsg.nAccountID = GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_DUNGEON_CLEAR, (char *)&loadMsg, sizeof(loadMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadTowerSlashData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_TOWER_SLASH loadMsg;
	int hDBServer = CMapServer::GetServer()->GetDBServer();

	if (m_nLoadTime[DB_TOWER_SLASH_DATA] > 0)
		return;
	m_nLoadTime[DB_TOWER_SLASH_DATA] = DB_TIMEOUT;

	loadMsg.nUID = GetUID();
	loadMsg.nAccountID = GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_TOWER_SLASH, (char *)&loadMsg, sizeof(loadMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::SaveTowerSlashData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_TOWER_SLASH saveMsg;
	int hDBServer = CMapServer::GetServer()->GetDBServer();

	memset(&saveMsg, 0, sizeof(saveMsg));
	saveMsg.nUID = GetUID();
	memcpy(&saveMsg.nData, &m_TowerSlashData, sizeof(struct plrDATA_TOWER_SLASH_SAVE));
	if (saveMsg.nData.currentFloor < 1)
		saveMsg.nData.currentFloor = 1;

	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_TOWER_SLASH, (char *)&saveMsg, sizeof(saveMsg), 0);
}

void CMapPlayer::SaveAvatarAwakenData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_AVATAR_AWAKEN saveMsg;
	int hDBServer = CMapServer::GetServer()->GetDBServer();

	memset(&saveMsg, 0, sizeof(saveMsg));
	saveMsg.nUID = GetUID();
	memcpy(&saveMsg.nData, &m_AvatarAwakenData, sizeof(struct plrDATA_AVATAR_AWAKEN_SAVE));

	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_AVATAR_AWAKEN, (char *)&saveMsg, sizeof(saveMsg), 0);
}

void CMapPlayer::LoadCnpcItemSkillData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_CNPC_ITEM_SKILL loadMsg;
	int hDBServer = CMapServer::GetServer()->GetDBServer();

	if (m_nLoadTime[DB_CNPC_ITEM_SKILL_DATA] > 0)
		return;
	m_nLoadTime[DB_CNPC_ITEM_SKILL_DATA] = DB_TIMEOUT;

	loadMsg.nUID = GetUID();
	loadMsg.nAccountID = GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_CNPC_ITEM_SKILL, (char *)&loadMsg, sizeof(loadMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::SaveCnpcItemSkillData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_CNPC_ITEM_SKILL saveMsg;
	int hDBServer = CMapServer::GetServer()->GetDBServer();

	memset(&saveMsg, 0, sizeof(saveMsg));
	saveMsg.nUID = GetUID();
	memcpy(&saveMsg.nData, &m_CnpcItemSkillData, sizeof(struct plrDATA_CNPC_ITEM_SKILL_SAVE));
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_CNPC_ITEM_SKILL, (char *)&saveMsg, sizeof(saveMsg), 0);
}

void CMapPlayer::SyncItemSkillToNpcSave(const struct itemUIDDATA * pUID, const struct CNPC_ITEM_SKILL_SAVE * pSkills)
{
	long si;
	struct plrDATA_NPC * pNPCData;

	if (!pUID || !pSkills)
		return;
	pNPCData = GetNPCData();
	if (!pNPCData)
		return;
	for (si = 0; si < gameMAX_CARRY_SOLDIER_LIMIT; si++)
	{
		if (gameCNPC_ItemSkillDbUidEmpty(&pNPCData->plrNPCData[si].plrNPC_ItemUID))
			continue;
		if (memcmp(&pNPCData->plrNPCData[si].plrNPC_ItemUID, pUID, sizeof(struct itemUIDDATA)))
			continue;
		memcpy(pNPCData->plrNPCData[si].plrNPC_ItemSkill, pSkills, sizeof(pNPCData->plrNPCData[si].plrNPC_ItemSkill));
	}
}

void CMapPlayer::SyncCnpcItemSkillEntry(const struct itemUIDDATA * pUID)
{
	struct CNPC_ITEM_SKILL_SAVE skills[gameMAX_SPECIAL_ATTACK];

	if (!pUID)
		return;
	gameCNPC_SessionCopyTo(pUID, skills);
	gameCNPC_ItemSkillDbSetEntry(&m_CnpcItemSkillData, pUID, skills);
	SyncItemSkillToNpcSave(pUID, skills);
	SyncCnpcItemSkillsToClient(pUID);
}

void CMapPlayer::ClearCnpcItemSkillEntry(const struct itemUIDDATA * pUID)
{
	struct CNPC_ITEM_SKILL_SAVE empty[gameMAX_SPECIAL_ATTACK];
	long si;
	unsigned long fldUID;
	CMapNPC * pFldNPC;
	struct plrDATA * pFldChar;
	struct plrDATA_NPC * pNPCData;

	if (!pUID || gameCNPC_ItemSkillDbUidEmpty(pUID))
		return;

	memset(empty, 0, sizeof(empty));
	gameCNPC_SessionCopyFrom(pUID, empty);
	gameCNPC_ItemSkillDbSetEntry(&m_CnpcItemSkillData, pUID, empty);
	SyncItemSkillToNpcSave(pUID, empty);
	SyncCnpcItemSkillsToClient(pUID);
	SaveCnpcItemSkillData();

	pNPCData = GetNPCData();
	if (pNPCData)
	{
		for (si = 0; si < gameMAX_CARRY_SOLDIER_LIMIT; si++)
		{
			if (gameCNPC_ItemSkillDbUidEmpty(&pNPCData->plrNPCData[si].plrNPC_ItemUID))
				continue;
			if (memcmp(&pNPCData->plrNPCData[si].plrNPC_ItemUID, pUID, sizeof(struct itemUIDDATA)))
				continue;
			fldUID = pNPCData->plrNPCData[si].plrNPC_UID;
			if (!fldUID)
				continue;
			pFldNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUIDForce(fldUID, CMapNPC::CLASS_ID);
			if (!pFldNPC)
				continue;
			pFldChar = pFldNPC->GetCharData();
			if (!pFldChar)
				continue;
			gameCNPC_RestoreNativeSkillsToBattle(pFldChar);
			gameServer_CalcCharacterAttribute(pFldChar);
			gameServer_MakeShow(pFldChar, pFldNPC->GetShowData());
		}
	}

	if (GetClientID() && CMapServer::GetServer()->GetClientValid(GetClientID()))
		UpdateCarryNPCTable();
}

void CMapPlayer::SyncCnpcItemSkillsToClient(const struct itemUIDDATA * pUID)
{
	struct MAP_SYNC_CNPC_ITEM_SKILL_RESULT msg;
	struct plrDATA_CNPC_ITEM_SKILL_SAVE * pSave;
	long i, n, slot;

	if (!GetClientID() || !CMapServer::GetServer()->GetClientValid(GetClientID()))
		return;

	pSave = GetCnpcItemSkillData();
	if (!pSave)
		return;

	memset(&msg, 0, sizeof(msg));
	n = 0;

	if (pUID && !gameCNPC_ItemSkillDbUidEmpty(pUID))
	{
		slot = gameCNPC_ItemSkillDbFindEntry(pSave, pUID);
		if (slot >= 0 && gameCNPC_HasItemSkill(pSave->entries[slot].skills))
		{
			memcpy(&msg.entries[0].itemUID, pUID, sizeof(struct itemUIDDATA));
			memcpy(msg.entries[0].skills, pSave->entries[slot].skills, sizeof(msg.entries[0].skills));
			n = 1;
		}
		else
		{
			struct CNPC_ITEM_SKILL_SAVE skills[gameMAX_SPECIAL_ATTACK];

			gameCNPC_SessionCopyTo(pUID, skills);
			memcpy(&msg.entries[0].itemUID, pUID, sizeof(struct itemUIDDATA));
			if (gameCNPC_HasItemSkill(skills))
				memcpy(msg.entries[0].skills, skills, sizeof(msg.entries[0].skills));
			else
				memset(msg.entries[0].skills, 0, sizeof(msg.entries[0].skills));
			n = 1;
		}
	}
	else
	{
		for (i = 0; i < gameMAX_CNPC_ITEM_SKILL_ENTRY; i++)
		{
			if (gameCNPC_ItemSkillDbUidEmpty(&pSave->entries[i].itemUID))
				continue;
			if (!gameCNPC_HasItemSkill(pSave->entries[i].skills))
				continue;
			memcpy(&msg.entries[n].itemUID, &pSave->entries[i].itemUID, sizeof(struct itemUIDDATA));
			memcpy(msg.entries[n].skills, pSave->entries[i].skills, sizeof(msg.entries[n].skills));
			n++;
		}
	}

	if (!n)
		return;

	msg.entryCount = (unsigned short)n;
	::msgSendData(GetClientID(), 0, PROTO_MAP_SYNC_CNPC_ITEM_SKILL_RESULT, (char *)&msg, sizeof(msg), 0);
}

static long CMapPlayer_ResolveCnpcDeploySkills(CMapPlayer * pPlr, const struct itemUIDDATA * pItemUID,
	const struct CNPC_ITEM_SKILL_SAVE * pNpcSaveSkills, struct CNPC_ITEM_SKILL_SAVE * pOut)
{
	long si;
	struct plrDATA_NPC * pNPCData;

	if (!pPlr || !pOut)
		return 0;
	memset(pOut, 0, sizeof(struct CNPC_ITEM_SKILL_SAVE) * gameMAX_SPECIAL_ATTACK);
	if (pNpcSaveSkills && gameCNPC_HasItemSkill(pNpcSaveSkills))
	{
		memcpy(pOut, pNpcSaveSkills, sizeof(struct CNPC_ITEM_SKILL_SAVE) * gameMAX_SPECIAL_ATTACK);
		return 1;
	}
	pNPCData = pPlr->GetNPCData();
	if (pNPCData && pItemUID && !gameCNPC_ItemSkillDbUidEmpty(pItemUID))
	{
		for (si = 0; si < gameMAX_CARRY_SOLDIER_LIMIT; si++)
		{
			if (memcmp(&pNPCData->plrNPCData[si].plrNPC_ItemUID, pItemUID, sizeof(struct itemUIDDATA)))
				continue;
			if (gameCNPC_HasItemSkill(pNPCData->plrNPCData[si].plrNPC_ItemSkill))
			{
				memcpy(pOut, pNPCData->plrNPCData[si].plrNPC_ItemSkill,
					sizeof(struct CNPC_ITEM_SKILL_SAVE) * gameMAX_SPECIAL_ATTACK);
				return 1;
			}
			break;
		}
	}
#ifndef CLIENT_PART
	if (pItemUID && !gameCNPC_ItemSkillDbUidEmpty(pItemUID))
		return gameCNPC_ItemSkillCopyForDeploy(pPlr->GetUID(), pItemUID, pOut);
#endif
	return 0;
}

void CMapPlayer::ApplyCnpcItemSkillToFieldChar(struct plrDATA * pFldChar, CMapNPC * pNPC, const struct itemUIDDATA * pItemUID)
{
	struct CNPC_ITEM_SKILL_SAVE skills[gameMAX_SPECIAL_ATTACK];
	const struct CNPC_ITEM_SKILL_SAVE * pNpcSaveSkills;

	if (!pFldChar || !pItemUID)
		return;
	if (gameCNPC_ItemSkillDbUidEmpty(pItemUID))
		return;
	pNpcSaveSkills = NULL;
#ifdef SMART_PLR_DATA2
	{
		long si;
		struct plrDATA_NPC * pNPCData = GetNPCData();
		if (pNPCData)
		{
			for (si = 0; si < gameMAX_CARRY_SOLDIER_LIMIT; si++)
			{
				if (!memcmp(&pNPCData->plrNPCData[si].plrNPC_ItemUID, pItemUID, sizeof(struct itemUIDDATA)))
				{
					pNpcSaveSkills = pNPCData->plrNPCData[si].plrNPC_ItemSkill;
					break;
				}
			}
		}
	}
#endif
	if (!CMapPlayer_ResolveCnpcDeploySkills(this, pItemUID, pNpcSaveSkills, skills))
		return;

	gameCNPC_ApplyItemSkillsToBattle(pFldChar, skills);
	gameServer_CalcCharacterAttribute(pFldChar);
	gameCNPC_ApplyItemSkillsToBattle(pFldChar, skills);
	if (pNPC)
		gameServer_MakeShow(pFldChar, pNPC->GetShowData());
}

void CMapPlayer::ApplyDeployedCnpcItemSkillsFromSave(void)
{
#ifdef SMART_PLR_DATA2
	long si;
	unsigned long uid;
	CMapNPC * pFldNPC;
	struct plrDATA * pFldChar;
	struct plrDATA_NPC * pNPCData;

	pNPCData = GetNPCData();
	if (!pNPCData)
		return;

	for (si = 0; si < gameMAX_CARRY_SOLDIER_LIMIT; si++)
	{
		uid = pNPCData->plrNPCData[si].plrNPC_UID;
		if (!uid)
			continue;
		pFldNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUIDForce(uid, CMapNPC::CLASS_ID);
		if (!pFldNPC)
			continue;
		pFldChar = pFldNPC->GetCharData();
		if (!pFldChar)
			continue;
		ApplyCnpcItemSkillToFieldChar(pFldChar, pFldNPC, &pNPCData->plrNPCData[si].plrNPC_ItemUID);
	}
#endif
}

void CMapPlayer::LoadFashionData(long nProtoUID)
{
	struct DB_GETPLAYERDATA_FASHION	FashionMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	if(m_nLoadTime[DB_FASHION_DATA] > 0)
		return;
	m_nLoadTime[DB_FASHION_DATA] = DB_TIMEOUT;

	FashionMsg.nUID			= GetUID();
	FashionMsg.nAccountID	= GetAccountID();
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_GETPLAYERDATA_FASHION, (char *)&FashionMsg, sizeof(FashionMsg), 0);

	m_IsLoadingData = true;
}

void CMapPlayer::LoadResult(long nProto, long nProtoUID)
{
	struct MAP_CHECK_PLAYER_UID_RESULT	ResMsg;
	CMapCtrl *	pMap;
	int			cnt1, nType;
	long val;

	// ���� Loading �ɶ�
	if (!CMapServer::GetServer()->GetClientValid(GetClientID()))
	{
		CMapServer::GetServer()->SetClientValid( GetClientID(), false, 60 );
	}
	//
	nType = ProtocoToLoadType(nProto);
	if(nType == -1)
		return;

	if(m_nLoadTime[nType] > 0)
	{
		m_nLoadTime[nType] = 0;
	}

	m_IsLoadingData	= false;
	for(cnt1 = 0; cnt1 < DB_TOTAL_TYPES; cnt1++)
	{
		if(m_nLoadTime[cnt1] > 0)
		{
			m_IsLoadingData = true;
			break;
		}
	}

	if(nType >= 0 && nType < DB_TOTAL_TYPES && m_nLoadCount < DB_TOTAL_TYPES)
	{
		// �Ĥ@�� Load ���⧹����
		m_nLoadCount++;
		if(IsAllDataLoaded())
		{
			if (m_IsLoadingData)
				CMapServer::GetServer()->Log("SERIOUS ERROR: %d Not load all data !", GetUID());
			// ���� Load ����e�X Client UID �ˬd���T�T��
			// Virtual space instances require (mapCode, mapCopyCode).
			pMap = CMapServer::GetServer()->FindMap(GetSaveData()->plrMapCode, (unsigned short)GetSaveData()->plrMapCopyCode);
			if (pMap == NULL)
				pMap = CMapServer::GetServer()->FindMap(GetSaveData()->plrMapCode);
			if(pMap == NULL)
			{
				CMapServer::GetServer()->Disconnect_ClearAllRecord( GetUID(), 0 );
				// ��Ʀ����~, �j����
				SetExitCode(CMapPlayer::EXIT_DATA_ERROR);
				CMapServer::GetServer()->DeleteObject(GetHandle());
				return;
			}

			ResMsg.m_nOK			= 1;
			ResMsg.m_nMapCountry	= CMapServer::GetServer()->GetTownDataByID(pMap->GetID())->ptCountryID;
			ResMsg.m_nMapMode		= pMap->GetStageData()->gstgMode;
			ResMsg.m_nWarMode		= CMapServer::GetServer()->IsWar();
			ResMsg.m_nWarType		= CMapServer::GetServer()->IsWarType();
			ResMsg.m_nTime	= CMapServer::GetServer()->GetLoopTime();
			::msgSendData(GetClientID(), nProtoUID, PROTO_MAP_CHECK_PLAYER_UID_RESULT, (char *)&ResMsg, sizeof(struct MAP_CHECK_PLAYER_UID_RESULT), 0);
			
			// 自动发送天枢数据给客户端 - 已禁用，改为打开界面时才加载
			/*
			{
				struct plrDATA_TIANSHU_SAVE tianshuData;
				memset(&tianshuData, 0, sizeof(tianshuData));
				tianshuData.plrTianshuEquipSelected = 255;
				tianshuData.plrTianshuHufuSelected = 255;
				// TODO: 从数据库加载天枢数据
				
				tagMAPMSG_TIANSHU_LOAD_RESULT tianshuMsg;
				memset(&tianshuMsg, 0, sizeof(tianshuMsg));
				
				// 从位图转换为long数组
				for (int i = 0; i < 100; i++)
				{
					int byteIndex = i / 8;
					int bitIndex = i % 8;
					tianshuMsg.equipActivated[i] = (tianshuData.plrTianshuEquipActivated[byteIndex] & (1 << bitIndex)) ? 1 : 0;
					tianshuMsg.hufuActivated[i] = (tianshuData.plrTianshuHufuActivated[byteIndex] & (1 << bitIndex)) ? 1 : 0;
				}
				
				tianshuMsg.equipSelected = tianshuData.plrTianshuEquipSelected;
				tianshuMsg.hufuSelected = tianshuData.plrTianshuHufuSelected;
				::msgSendData(GetClientID(), 0, PROTO_TIANSHU_LOAD_RESULT, (char*)&tianshuMsg, sizeof(tianshuMsg), 0);
			}
			*/

			
			// 同步变身时间给客户端
			{
				#define TRANSFORM_FOOT_EFFECT_BASE_ID 39000
				struct {
					unsigned long startTime;	// 变身开始时间（Unix时间戳，兼容旧客户端）
					long cardRank;				// 卡片等级（0-4）
					unsigned long endUnix;		// 变身结束 Unix 时间（与 plrTime_General_a 一致，支持累加长计时）
				} transformState;
				
				// 变身时间使用独立槽位 plrTime_General_a，避免与武魂 plrSoulTime 冲突
				if(GetSaveData()->plrTransformNpcCode > 0
					&& GetSaveData()->plrTime_General_a > (unsigned long)CMapServer::GetServer()->GetLoopTime())
				{
					// 计算变身开始时间（当前时间 - 已经过的时间）
					long remainingTime = (long)(GetSaveData()->plrTime_General_a - (unsigned long)CMapServer::GetServer()->GetLoopTime());
					unsigned long currentTime = (unsigned long)time(NULL);
					transformState.startTime = remainingTime <= (long)AVATAR_TRANSFORM_ADD_SECONDS
						? (currentTime - (unsigned long)((long)AVATAR_TRANSFORM_ADD_SECONDS - remainingTime))
						: (currentTime - (unsigned long)remainingTime); /* 剩余超过单段时长时用线性推算 */
					transformState.endUnix = (unsigned long)GetSaveData()->plrTime_General_a;
					
					// 从保存的字段中读取卡片等级（高4位）
					transformState.cardRank = (GetSaveData()->plrTransformEquipWeapon >> 4) & 0x0F;
				}
				else
				{
					// 没有变身状态，发送清除消息
					transformState.startTime = 0;
					transformState.cardRank = -1;
					transformState.endUnix = 0;
				}
				
				::msgSendData(GetClientID(), 0, PROTO_MAP_SYNC_TRANSFORM_STATE, (char*)&transformState, sizeof(transformState), 0);

				/* Map-enter/map-switch safety sync:
				   when transformed player appears to others, broadcast persistent foot aura once. */
				if (transformState.cardRank >= 0)
				{
					CMapCharMsg* pAuraMsg = InsertMsg(PROTO_MAP_USE_ITEM_RESULT_ADD_HP_MP, CMapCharMsg::SEND_TO_ALL);
					if (pAuraMsg)
					{
						pAuraMsg->m_nSize = sizeof(struct MAP_USE_ITEM_RESULT_ADD_HP_MP);
						pAuraMsg->m_Msg.m_UseItemAddHpMp.plrUID = GetUID();
						pAuraMsg->m_Msg.m_UseItemAddHpMp.add_HP_MP_Type = UIR_EFFECT;
						pAuraMsg->m_Msg.m_UseItemAddHpMp.addHP = TRANSFORM_FOOT_EFFECT_BASE_ID + transformState.cardRank;
						pAuraMsg->m_Msg.m_UseItemAddHpMp.addMP = TRANSFORM_EFFECT_SIGNATURE_MP;
						pAuraMsg->m_Msg.m_UseItemAddHpMp.itemID = 0;
					}
				}
			}
			//
#ifdef CROSS_SERVER_SET_NAME
 //#ifdef CROSS_SERVER_SYSTEM
			if (!CMapServer::GetServer()->IsCrossServer(1))		// ���O����A��Server
			{
				GetSaveData()->plrSetID = (unsigned char)CMapServer::GetServer()->GetServerInfo()->iniSetNumber;
				GetShowData()->plrSetID = GetSaveData()->plrSetID;
			}
 //#endif
#endif
			// �]�w�s���v��(���e�������O�b���v��)
			m_nPrivilege |= (GetSaveData()->plrPrivilege & gamePRIVILEGE_TYPE_STOPTALK);
		//	// �]�w�v��, �����b plrBaseSPCFlag
		//	GetSaveData()->plrBaseSPCFlag &= ~(spcFLAG_GM | spcFLAG_GM2);
		//	if (m_nPrivilege & gamePRIVILEGE_TYPE_GM1)
		//	{
		//		GetSaveData()->plrBaseSPCFlag |= spcFLAG_GM;
		//		GetCharData()->plrSPCFlag |= spcFLAG_GM;
		//		CMapServer::GetServer()->Log("Player(%d) is GM type 1", GetUID());
		//	}
		//	else if (m_nPrivilege & gamePRIVILEGE_TYPE_GM2)
		//	{
		//		GetSaveData()->plrBaseSPCFlag |= spcFLAG_GM2;
		//		GetCharData()->plrSPCFlag |= spcFLAG_GM;
		//		CMapServer::GetServer()->Log("Player(%d) is GM type 2", GetUID());
		//	}
			// �������~�����
			if ((m_nBotCheckType > 0) && (m_nBotCheckTimes > 1))
			{
				GetSaveData()->plrBotCheckType = m_nBotCheckType;
				GetSaveData()->plrBotTimes = m_nBotCheckTimes;
				AutoSaveCharData();
				//
				m_nBotCheckType = 0;
				m_nBotCheckTimes = 0;
				m_nBotCheckDuedate = CMapServer::GetServer()->GetLoopTime() + gameBOT_UPDATE_TIMEOUT;
#ifdef SHOW_BOT_LOG
CMapServer::GetServer()->Log("DEBUG: ���� 30 ����");
#endif
			}
			else
			{
#ifdef CHECK_BOT_SO_SYSTEM
				if (GetSaveData()->plrBotTimes > 1)
				{
					m_nBotCheckDuedate = CMapServer::GetServer()->GetLoopTime() + gameBOT_UPDATE_TIMEOUT2;
 #ifdef SHOW_BOT_LOG
 CMapServer::GetServer()->Log("DEBUG: ���� 8 ����");
 #endif
				}
				else
#endif
				{
					m_nBotCheckDuedate = 0;
#ifdef SHOW_BOT_LOG
CMapServer::GetServer()->Log("DEBUG: �������ݭn");
#endif
				}
			}
			//
			// �M�����A���
			InnerResetStatusData();

			// ���`�۰ʴ_��
			if(GetHP() <= 0)
				ResurrectImmed(0);
			//
			CWar_ActiveResurrectTick(1);
			// �]�w���v�ԧЪ��A(��إߤp�L���e����)
			CMapServer::GetServer()->m_nHistoryManager.SetPlayerEnterInfo(this, GetMapCode());
			// �إߤp�L
			if (m_nPrivilege & gamePRIVILEGE_TYPE_DELETED)	// �Ĥ@���n�J�a��
			{	// �M���l���~
				CreateCarryNPC(1);
			}
			//else
			CreateCarryNPC();
			ApplyDeployedCnpcItemSkillsFromSave();
			UpdateCarryNPCTable();
			SetCarryNPC_REDStatus(0);
			// ���s�p��t��
			gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
			// ���s�p���ɳѾl��
			gameNPC_CalcLeader(GetCharData());
#ifdef TW_PVP_MODE
			if (GetSaveData()->plrCrimeVal)
			{
				pStatus_Set(effFun_RED, 1);
				SetCarryNPC_REDStatus();
 #ifndef USE_PACKAGE_DATA
				CMapServer::GetServer()->Log("DEBUG: �n�J�]�w���H���A(%s,%d)", GetName(), GetSaveData()->plrCrimeVal);
 #endif
			}
#endif
			//
			if (m_nPrivilege & gamePRIVILEGE_TYPE_DELETED)	// �Ĥ@���n�J�a��
			{
				// ��s Client NPC ���(Login �⤣�X��)
			//	struct plrDATA_NPCSHOW sendData;

			//	gameServer_MakeNPC(GetNPCData(), &sendData, this);
			//	::msgSendData(GetClientID(), 0, PROTO_MAP_GET_SOLDIER_TABLE_RESULT, (char *)&sendData, sizeof(sendData), 0 );
				//
#ifdef CROSS_SERVER_SYSTEM		// �ǰe���a������
				if (CMapServer::GetServer()->IsCrossServer(1))		// �O����A��Server
				{
					UpdateClientShowData();			// ���a���
					UpdateSkillTable();				// �ޯ�
					UpdateClientItemData();			// ���~
					UpdateClientStorageData();		// �ܮw
					//
					SendMissionDataToClient();		// ����
					//
 #ifndef USE_BLACKLIST_NO_MSG
					InnerSendBlackListData(this);	// �n��
 #endif
				}
#endif
				// ��s Cleint ���W���~�B�ܮw �p�L���~���(Login �⤣�X��)
				struct plrCARRYITEM_SAVE * pItemSave;
				struct plrSTORAGEITEM_SAVE * pStorageSave;

				if (pItemSave = GetItemData())
				{
					//for (cnt1=0; cnt1<gameMAX_CARRYITEM; cnt1++)
					val = ::gameServer_GetCarryItemMax(GetSaveData());
					for (cnt1=0; cnt1<val; cnt1++)
					{
						if (pItemSave->plrCarryItem[cnt1].m_Item.itemFlags & itemSHOW_FLAG_ALL_CNPC)
							UpdateClientSingleItem(cnt1);
					}
				}
				// �ܮw��ƭץ�
				if (pStorageSave = GetStorageData())
				{
					if (pStorageSave->psMax_StorageItem != GetSaveData()->plrMax_StorageItem)
					{
						if (pStorageSave->psMax_StorageItem >= GetSaveData()->plrMax_StorageItem)
						{
							GetSaveData()->plrMax_StorageItem = pStorageSave->psMax_StorageItem;
							AutoSaveCharData();
						}
						else
						{
							pStorageSave->psMax_StorageItem = GetSaveData()->plrMax_StorageItem;
							AutoSaveStorageData();
						}
						//
						UpdateClientMaxItem();
					}
				}
				// �ץ����ȸ��
				FixMissionRepeat();
				// �ץ��ޯ���
				UpdateSkillTable(3);

				if (GetSaveData()->plrEngineerTime)
					UpdateEngineerTime();

				// �ӤH�ө��ɶ�
				UpdatePersonalShopTime();
				UpdateAnnouncementTime();
				UpdateAutoFunctionTime(2);
				SendMailExistMsg(GetClientID());		// Client ID
				//
				if (GetSaveData()->plrEnterStageTime)
				{
					// �®ɶ�����ഫ
					if (GetSaveData()->plrEnterStageTime >= 146656000)		// 1974�~8��25��		CMapServer::GetServer()->GetLoopTime()
					{
						GetSaveData()->plrEnterStageTime -= CMapServer::GetServer()->GetLoopTime();
						if (GetSaveData()->plrEnterStageTime < 0)
							GetSaveData()->plrEnterStageTime = 0;
						AutoSaveCharData();
					}
					else if (GetSaveData()->plrEnterStageTime < 0)
					{
						GetSaveData()->plrEnterStageTime = 0;
						AutoSaveCharData();
					}
				}
				SendSpecialTimeTable();
				// �x��������ץ�
				if (GetSaveData()->plrOffice_Old)
				{
					if (GetSaveData()->plrOffice <= 1)
						GetSaveData()->plrOffice = GetSaveData()->plrOffice_Old;
					GetSaveData()->plrOffice_Old = 0;
					AutoSaveCharData();
				}
				//
#ifdef USE_TRADE_PASSWORD
				if (!(m_nWebIP_Flags & LOGIN_CHAR_FLAGS_NEED_TRADE_PSW))
					SendMsgToClientByID(24383);	// �z�|���]�w����K�X�A�Х��}�t�ο��]�w����K�X�A�O�ٱz����Ʀw���I
#endif
#ifdef USE_WEB_IP_ACCOUNT_VERIFY				// �V�n�G���@IP�Ѹ�Ʈw����
				val = m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_WEBIP_EXP | LOGIN_CHAR_FLAGS_WEBIP_SOLEXP | LOGIN_CHAR_FLAGS_WEBIP_SKILLEXP | LOGIN_CHAR_FLAGS_WEBIP_DROP);
				if (val)
				{
					//if (val & LOGIN_CHAR_FLAGS_WEBIP_EXP)		// �g��
					//if (val & LOGIN_CHAR_FLAGS_WEBIP_SOLEXP)	// �p�L�g��
					//if (val & LOGIN_CHAR_FLAGS_WEBIP_SKILLEXP)	// �ޯ��I
					//if (val & LOGIN_CHAR_FLAGS_WEBIP_DROP)		// ���_
					//
					SendMsgToClientByID(24429);	// IP Bonus �u�f�I
				}
#endif
				//
				DuedateItem_Init();
				// �ץ��\�u���~�ƥ�
				// �p�G���\�u�ɶ��A���O�ӤH�ө��ޯ൥�� < 10 ���ܬO�®ɴ���ơA�n�]�w�� (24-2)��
#ifdef ItemMode
 #ifdef USE_STALL_NEW_RULE
				struct plrDATA_SKILL * pSkillTbl;

				pSkillTbl = GetSkillData();
				if (GetSaveData()->plrPersonalShopTime)
				{
					if (pSkillTbl->plrSkillLevelMax[magic_PERISONAL_SHOP] < 10)
					{
						pSkillTbl->plrSkillLevelMax[magic_PERISONAL_SHOP] = (24-2);
						AutoSaveSkillData();
						//
						UpdateSkillTable();
					}
				}
				else
				{
					if (pSkillTbl->plrSkillLevelMax[magic_PERISONAL_SHOP] > 0)
					{
						pSkillTbl->plrSkillLevelMax[magic_PERISONAL_SHOP] = 0;
						AutoSaveSkillData();
						//
						UpdateSkillTable();
					}
				}
 #endif
#endif
				// �Ǧ^�¦W����
#ifdef USE_BLACKLIST_NO_MSG
				InnerSendBlackListData(this);
#endif
				// �t��
				if (GetSaveData()->plrMarry_UID == 0xffffffff)
					ProcDivorce();

				// �N�o
#ifdef USE_COOL_DOWN_SYSTEM
				CMapServer::GetServer()->UpdateClientCoolDownData(this);
#endif
			}

			// �O�����a�m�W�Puid��Ӫ�
			CMapServer::GetServer()->CharName_Add(GetSaveData()->plrName, GetSaveData()->plrUID);
			// ��s�����O��
			m_nStateRecord.m_nLevel = GetSaveData()->plrLevel;

			if (GetSaveData()->plrStatus & effFun_WIZARD_SUMMON)
			{
				CMapServer::GetServer()->Log("ERROR: Player(%s) has summon flag(0x08X) !!!", GetName(), GetSaveData()->plrStatus);
				GetSaveData()->plrStatus &= ~effFun_WIZARD_SUMMON;
			}
			// �p�⤧�e�L��ƪ�����
			// �Q�ʧޡG�p���O�W��
//CMapServer::GetServer()->Log("DEBUG: HP Max 1 %d", GetMaxHP());
			//::gameChar_CalcSTMax(GetCharData());
			gameServer_CalcCharacterAttribute(GetCharData());
//CMapServer::GetServer()->Log("DEBUG: HP Max 2 %d", GetMaxHP());

			CalcHPRestoreTime();
			CalcMPRestoreTime();
			CalcSTRestoreTime();

		//	if (GetSaveData()->plrBaseSPCFlag & (spcFLAG_GM | spcFLAG_GM2))
		//	{
		//		UpdateShowData();
		//	}

			if (m_nPrivilege & gamePRIVILEGE_TYPE_DELETED)	// �Ĥ@���n�J�a��(�b CB_InitClientReady() �ɲM��)
			{
				UpdateClientShowData(0);					// (Login �⤣�X��)
				//
				SetCWarInfo();								// ��Ը��
			}
			else											// ��s���A? (�p effFun2_EQUIP_NO_ATTACK �Ȯɥ��Ħ^�СA�]�����A���x�s)
				UpdateShowData(1);

			// ......... �]���a���_�u��(Server �L�k�q��)�A�ҥH�n��s�U����� ...........
			// ����p
			// �դO���p
			// �x�Ϊ��p(?)

			// �ǰe���e��n���T��
			CMapServer::GetServer()->kimSendData(this, GetUID(), GetClientID());
			//CMapServer::GetServer()->pfcUpdateData(GetUID());
			// �q�����U���A
			::gameEff_MakeDisappearStatus(GetCharData());
			SendStatusDisappear(0, GetCharData());

			// �q���S�����A
			SendSpecialStatus();
			// Bug: �D�k�˳�
			CheckIllegalEquipItem();
			if (IsIllegalEquipItem())
				SendMsgToClientByID(24403);	// ���W�˳Ʋ��`�A�L�k�����ΨϥΧޯ�I
			//
			if (CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_IsSpaceMap(GetMapCode()))
			{
				CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_UpdateClientData(GetMapCode(), this);
				//
				CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SendHotObjectHPList(GetMapCode(), this);
			}
			else if (TowerSlashServer_IsPlayerInSession(this))
				TowerSlashServer_UpdateClientData(this);
		}
	}

//	// ���s�p��t��
//	gameServer_CalcCharacterWeight(GetCharData());
}

// ==========================================================
void FixCarryNPC_UID_Data(CMapPlayer * pPlayer)
{
	long i, find, pos;
	unsigned long uid, base_uid;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
//	struct plrDATA_NPCSHOW sendData;

	pNPCData = pPlayer->GetNPCData();
	find = 0;
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		uid = pNPCSaveData->plrNPC_UID;
		if (uid)
		{
			if (IsPlayerUID(uid))		// �]�� gameMAX_ACCOUNT_UID �ܤj�A�p�L uid �|�ܦ� Player uid �d��
			{
				find++;
				break;
			}
		}
	}
	// ���s�� uid�A�Ѧ� DataProc.c �� gameNPC_GetUID
	if (find)
	{
		base_uid = gameServerNPC_GetRealUID(pPlayer->GetUID());		// �Ĥ@���p�L��UID
		pos = 0;
		for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
		{
			pNPCSaveData = &pNPCData->plrNPCData[i];
			uid = pNPCSaveData->plrNPC_UID;
			if (uid)
			{
				uid = gameServerNPC_GetSystemUID(base_uid + pos);
				pos++;
				//
				pNPCSaveData->plrNPC_UID = uid;
			}
		}
		pPlayer->AutoSaveNPCData();
		//
		// ��sClient���
		pPlayer->UpdateCarryNPCTable();
	//	gameServer_MakeNPC(pPlayer->GetNPCData(), &sendData, pPlayer);
	//	::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_GET_SOLDIER_TABLE_RESULT, (char *)&sendData, sizeof(sendData), 0 );
		CMapServer::GetServer()->Log("Fix CNPC Data: %d,%s", pPlayer->GetUID(), pPlayer->GetName());
	}
}

// �� NPC �����p�L�إߥX��
// �Ҽ{�}��
// mode = 1 �Ĥ@���n�J�A����M���l���~�ץ�
void CMapPlayer::CreateCarryNPC(long mode)
{
	unsigned long uid;
	long i, icode;
	CMapPlayer * pPlayer = this;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	struct itemDATA * pItemPtr;
	long hNPC;
	CMapNPC * pNPC;
	struct plrDATA * pCharData;
	long role_code = 0;
	long is_summon = 0;
	long chg_code, chg_code_sec;

	pNPCData = GetNPCData();
	if (mode)
	{
#ifndef NO_AUTO_DELETE_SUMMON
		for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
		{
			pNPCSaveData = &pNPCData->plrNPCData[i];
			role_code = pNPCSaveData->plrNPC_SummonCode;	// �l���~
			if (!pNPCSaveData->plrNPC_ItemCode)
			{
				uid = pNPCSaveData->plrNPC_UID;
				if (role_code)
				{
					memset(pNPCSaveData, 0, sizeof(struct plrDATA_NPC_SAVE));
					AutoSaveNPCData();
//CMapServer::GetServer()->Log("Clear summon NPC(%d).", uid);
				}
				else
				{
					if (uid)
					{
//CMapServer::GetServer()->Log("CNPC �ݯd(%d).", uid);
						memset(pNPCSaveData, 0, sizeof(struct plrDATA_NPC_SAVE));
						AutoSaveNPCData();
					}
				}
				// �R���w�s�b����
				pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(uid, CMapNPC::CLASS_ID);
				if (pNPC)
				{
					if (!pNPC->IsKindOf(CMapNPC::CLASS_ID))
						CMapServer::GetServer()->Log("ERROR: First clear not NPC uid(%08x)", uid);
					if (pNPC->GetStateProc() != CMapObject::STATE_DELETE)
						CMapServer::GetServer()->DeleteObject(pNPC->GetHandle());
				}
			}
		}
#endif
		//
		FixCarryNPC_UID_Data(this);
		return;
	}
	//
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		if (icode = pNPCSaveData->plrNPC_ItemCode)
		{
			if (pItemPtr = ::gameGetItemPtr(icode))
			{
				role_code = pItemPtr->itemUseMagicID;
				// �l���~�ץ�(����)
do_summon:		uid = pNPCSaveData->plrNPC_UID;
			//	if (!uid)		// ???
			//		continue;	// ???
				// ����w�s�b?
				pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(uid, CMapNPC::CLASS_ID);
				if (pNPC)
				{
					if (!pNPC->IsKindOf(CMapNPC::CLASS_ID))
						CMapServer::GetServer()->Log("ERROR: First create not NPC uid(%08x)", uid);
					if (pNPC->GetStateProc() != CMapObject::STATE_DELETE)
						CMapServer::GetServer()->DeleteObject(pNPC->GetHandle());
					CMapServer::GetServer()->Log("ERROR: (%d,%s)First create already exist CNPC(%d,%08x)", GetUID(), GetName(), role_code, uid);
				/*
					if (pNPC->GetStateProc() == CMapObject::STATE_DELETE)
					{
						hNPC = pNPC->GetHandle();
						if(!pNPC->IsOutMap())
							pNPC->LeaveMap();
						pNPC->SetStateProc(CMapObject::STATE_NORMAL);
						goto mrh001;
					}
					else
					{
						CMapServer::GetServer()->Log("ERROR: (%d,%s)First create already exist CNPC(%d,%08x)", GetUID(), GetName(), role_code, uid);
						//
						hNPC = pNPC->GetHandle();
						if(!pNPC->IsOutMap())
							pNPC->LeaveMap();
						goto mrh001;
					}
				*/
					continue;
				}
				//
				hNPC = CMapServer::GetServer()->CreateNPC(role_code, pNPCSaveData->plrNPC_UID, CMapNPC::TYPE_MERCENARY);
				if (hNPC != -1)
				{
					pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByHandle(hNPC);
					if(pNPC != NULL)
					{
//CMapServer::GetServer()->Log("Create NPC object item code(%d), uid(%d)", icode, pNPCSaveData->plrNPC_UID);
//	mrh001:				
						pCharData = pNPC->GetCharData();
						//gameServer_NPC_MakeFullData(pNPCSaveData, pCharData, pPlayer->GetSaveData()->plrMatrix);
						gameServer_NPC_MakeFullData(pNPCSaveData, pCharData, pNPC->GetCNPC_BA_Code(pPlayer));
						// ��l�Ƥp�L
						chg_code = pNPCSaveData->plrNPC_ChangeCode;
						chg_code_sec = pNPCSaveData->plrNPC_ChangeCodeSec;

						CMapServer::GetServer()->InnerInitCarryNPCData(pNPC, pPlayer);
						pPlayer->ApplyCnpcItemSkillToFieldChar(pCharData, pNPC, &pNPCSaveData->plrNPC_ItemUID);
						if (chg_code)
						{
							if (!chg_code_sec)
								chg_code_sec++;
							pNPCSaveData->plrNPC_ChangeCode = (unsigned short)chg_code;
							pNPCSaveData->plrNPC_ChangeCodeSec = (unsigned short)chg_code_sec;
							pCharData->plrNPC_ChangeCode = (unsigned short)chg_code;
							pCharData->plrNPC_ChangeCodeSec = (unsigned short)chg_code_sec;
							//
							gameServer_MakeShow(pNPC->GetCharData(), pNPC->GetShowData());
						}
 #ifndef USE_PACKAGE_DATA
						CMapServer::GetServer()->Log("DEBUG: CNPC change code(%s,%d,%d)(%s)", pNPC->GetName(), chg_code, chg_code_sec, GetName());
 #endif
					/*	pCharData->plrSaveData.plrMapCode = pPlayer->GetMapCode();
						pNPC->SetMapID(pCharData->plrSaveData.plrMapCode);
						pNPC->SetBornPos(0, 0);
						pNPC->SetPatrolPos(0, 0);
						pNPC->m_nTalkID = 0;
						pNPC->m_nCNPC_State = 0;			// �إ߮ɤ@�w�b���a�e������
						pNPC->m_nCNPC_UpdateFlag = 1;
						pNPC->m_nPlayerUID = pPlayer->GetUID();		// ���o�� �άO npc uid �N���O���a�� NPC(�ثe�H IsNPCUID(uid) �P�_)
						pNPC->m_nCharFlags |= CHAR_FLAG_NO_MOVE_BLOCK;
						pNPC->m_nCharFlags &= ~CHAR_NO_TRANSFER_TO_ITEM;
						//
						if (CMapServer::GetServer()->IsSpecialNPC(0, pItemPtr->itemUseMagicID))
						{
							pNPC->m_nCharFlags |= CHAR_NO_TRANSFER_TO_ITEM;
							pNPC->plrNPC_ItemCode = 0;
						}
						pNPC->SetHitObject(false);
						// ��m
						x = pPlayer->GetPosX();
						y = pPlayer->GetPosY();
						pNPC->SetPos(x, y);
						// ----- �~�Ӫ��a����� -----
						// ���y
						pCharData->plrSaveData.plrCountryID = pPlayer->GetSaveData()->plrCountryID;
						pCharData->plrShowData.plrCountryID = pCharData->plrSaveData.plrCountryID;
						pCharData->plrSetColor = (unsigned short)::gameGetCountryColor(pCharData->plrSaveData.plrCountryID);
						pCharData->plrShowData.plrSetColor = pCharData->plrSetColor;
						msg_strncpy(pCharData->plrSaveData.plrOrganizeName, pPlayer->GetSaveData()->plrName, sizeof(pCharData->plrSaveData.plrOrganizeName));
						msg_strncpy(pCharData->plrShowData.plrOrganizeName, pPlayer->GetSaveData()->plrName, sizeof(pCharData->plrSaveData.plrOrganizeName));
					*/
					//	pCharData->plrSummon_Wizard = 0;
						pNPCSaveData->plrNPC_SummonCode = 0;
						if (role_code)
						{
							//if (CMapServer::GetServer()->IsSpecialNPC(0, role_code))
							if (is_summon)
							{
					//			// ��h�l��[��
					//			if (GetSaveData()->plrJobID == jobID_WIZARD)
					//			{
					//				pCharData->plrSummon_Wizard |= 1;
					//				gameServer_CalcCharacterAttribute(pCharData);
					//			}
								pNPC->m_nCharFlags |= CHAR_NO_TRANSFER_TO_ITEM;
								//pNPC->m_nCharFlags |= CHAR_NO_LEVEL_UP;
								pNPCSaveData->plrNPC_ItemCode = 0;
								pNPCSaveData->plrNPC_SummonCode = (unsigned short)role_code;
							//	if (pNPCSaveData->plrNPC_SummonSec > 65535)				// �l��̤j�ɶ�
							//	{
							//		pNPCSaveData->plrNPC_SummonSec = 65535;
							//		AutoSaveNPCData();
							//	}
								if (pNPCSaveData->plrNPC_SummonSec == 0)
								{
									pNPCSaveData->plrNPC_SummonSec++;	// ���� 1���B�z���`
								}
								//
								pCharData->plrNPC_ItemCode = pNPCSaveData->plrNPC_ItemCode;
								pCharData->plrNPC_Loyalty = pNPCSaveData->plrNPC_Loyalty;
								pCharData->plrNPC_SummonCode = pNPCSaveData->plrNPC_SummonCode;
								pCharData->plrNPC_SummonSec = pNPCSaveData->plrNPC_SummonSec;
								//
								// �q�����A
								CMapCharMsg * pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
								
								pMsg->m_Msg.m_UpdateData2Msg.Reset();
								pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_CNPC_SUMMON_SEC, 0, pCharData->plrNPC_SummonSec, pNPC->GetUID());

								pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
								//
								is_summon = 0;		// Bug fix: 20050906 �p�L����
//if (pNPC->GetSaveData()->plrStatus & effFun_WIZARD_SUMMON)
//		CMapServer::GetServer()->Log("DEBUG: Has summon flag(0x%08X) !!!", pNPC->GetSaveData()->plrStatus);
							}
						}
						//
					//	pNPC->SetStateProc(CMapObject::STATE_NORMAL);
					//	CMapServer::GetServer()->ChangeObjectState(pNPC->GetHandle(), CMapObject::STATE_ENTER_MAP);
						::gameEff_MakeDisappearStatus(pNPC->GetCharData());
						SendStatusDisappear(uid, pNPC->GetCharData());
					}
					else
					{
						CMapServer::GetServer()->Log("ERROR: Create NPC: NPC object not found (%d) ", hNPC);
						CMapServer::GetServer()->DeleteObject(hNPC);
					}
				}
				else
					CMapServer::GetServer()->Log("Create NPC object fail(item code = %d, uid = %d)", role_code, pNPCSaveData->plrNPC_UID);
			}
		}
		else
		{
			role_code = pNPCSaveData->plrNPC_SummonCode;	// �l���~
			if (role_code)
			{
//if (pNPCSaveData->plrNPC_Status & effFun_WIZARD_SUMMON)
//		CMapServer::GetServer()->Log("DEBUG: Has summon flag(0x%08X) !!!", pNPCSaveData->plrNPC_Status);
				is_summon++;
				goto do_summon;
			}
			if (pNPCSaveData->plrNPC_UID)
			{
//CMapServer::GetServer()->Log("CNPC �ݯd2(%d).", pNPCSaveData->plrNPC_UID);
				pNPCSaveData->plrNPC_UID = 0;
				AutoSaveNPCData();
			}
		}
	}
	ApplyDeployedCnpcItemSkillsFromSave();
}

// �p�L���A�^�_
void CMapPlayer::FullRestoreCarryNPC()
{
	long i;
	long ostatus, mode;
	CMapPlayer * pPlayer = this;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	CMapNPC * pNPC;
	long ostatus2;

	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		//if (pNPCSaveData->plrNPC_ItemCode)
		if (pNPCSaveData->plrNPC_ItemCode || pNPCSaveData->plrNPC_SummonCode)
		{
			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCSaveData->plrNPC_UID, CMapNPC::CLASS_ID);
			if(pNPC != NULL)
			{
				if (!pNPC->IsDead())
				{
					ostatus = pNPC->GetSaveData()->plrStatus;
					ostatus2 = pNPC->GetSaveData()->plrStatus2;
					gameEff_ClearObjectSingleStatus(0, pNPC->GetCharData(), 0, -2);

					pNPC->GetSaveData()->plrStatus &= ~(effFUN_STATUS_ALL);
					pNPC->GetShowData()->plrStatus &= ~(effFUN_STATUS_ALL);
					//
					mode = 0;
					if ((ostatus != pNPC->GetSaveData()->plrStatus) || (ostatus2 != pNPC->GetSaveData()->plrStatus2))
					{
						::gameServer_CalcCharacterAttribute(pNPC->GetCharData());
						mode++;
					}
					//
					pNPC->SetHP(pNPC->GetMaxHP());
					pNPC->UpdateNPCPart1(mode);
					pNPC->SavePlayerNPCData(0, pPlayer);
				}
			}
		}
	}
}

// �p�L���ۦ^�_
#ifdef USE_CNPC_LOYALITY_TIME_ADD				// �w�ɼW�[�p�L����(�u��server�ݡAItemMode)
void CMapPlayer::AutoAddCarryNPCRoyality()
{
	long i;
	long add_loyal;
	CMapPlayer * pPlayer = this;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	CMapNPC * pNPC;

	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		if (pNPCSaveData->plrNPC_ItemCode || pNPCSaveData->plrNPC_SummonCode)
		{
			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCSaveData->plrNPC_UID, CMapNPC::CLASS_ID);
			if(pNPC != NULL)
			{
				if (!pNPC->IsDead())
				{
					if (!(pNPC->m_nCharFlags & CHAR_FLAG_CNPC_ENGINEER))
					{
						add_loyal = pNPC->GetCharData()->plrNPC_Loyalty;
						if (add_loyal < 100)
						{
							add_loyal++;
							if (add_loyal < 0)
								add_loyal = 0;
							if (add_loyal > 100)
								add_loyal = 100;
							//
							pNPC->GetCharData()->plrNPC_Loyalty = (unsigned char)add_loyal;
							pNPC->SavePlayerNPCData(1, pPlayer);
							pNPC->UpdateNPCPart1(0x10);
						}
					}
				}
			}
		}
	}
}
#endif

// ��s�p�L�}��(�j��)
void CMapPlayer::SetCarryNPCSetColor(long setcolor)
{
	long i;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	CMapNPC * pNPC;

	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		//if (pNPCSaveData->plrNPC_ItemCode)
		if (pNPCSaveData->plrNPC_ItemCode || pNPCSaveData->plrNPC_SummonCode)
		{
			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCSaveData->plrNPC_UID, CMapNPC::CLASS_ID);
			if(pNPC != NULL)
			{
				pNPC->GetCharData()->plrSetColor = (unsigned short)setcolor;
				if (!pNPC->IsDead())
					pNPC->UpdateShowData();
			}
		}
	}
}

// ��s�p�L���H���A(�j��)
void CMapPlayer::SetCarryNPC_REDStatus(long broadcast)
{
	long i;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	CMapNPC * pNPC;
	long status;
#ifdef TW_PVP_MODE
	long crime_val, cnt;
	CMapCharMsg * pMsg;
#endif

	status = 0;
	if (IsRedStatus())
		status++;
	//
	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		//if (pNPCSaveData->plrNPC_ItemCode)
		if (pNPCSaveData->plrNPC_ItemCode || pNPCSaveData->plrNPC_SummonCode)
		{
			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCSaveData->plrNPC_UID, CMapNPC::CLASS_ID);
			if(pNPC != NULL)
			{
				if (status)
				{
					if (!(pNPC->GetShowData()->plrStatus & effFun_RED))
						pNPC->pStatus_Set(effFun_RED, broadcast);
				}
				else
				{
					if (pNPC->GetShowData()->plrStatus & effFun_RED)
						pNPC->pStatus_Clear(effFun_RED, broadcast);
				}
				// ��s�g�@��
#ifdef TW_PVP_MODE
				crime_val = GetSaveData()->plrCrimeVal;
				if (pNPC->GetSaveData()->plrCrimeVal != crime_val)
				{
					pNPC->GetSaveData()->plrCrimeVal = crime_val;
					pNPC->GetShowData()->plrCrimeVal = (char)crime_val;
					//
				/*	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_ALL);
					pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
					pMsg->m_nSize	= pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_UPDATE_CRIME;
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = GetUID();
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData2 = crime_val;
				*/
					pMsg = ReplaceMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_ALL);
					if (!pMsg->m_nIsNewInsert)
					{
						cnt = pMsg->m_Msg.m_UpdateData2Msg.m_nCount;
						if (pMsg->m_Msg.m_UpdateData2Msg.m_nCount < (UPDATE_DATA_PART_MAX-2))
						{
							pMsg->m_Msg.m_UpdateData2Msg.m_nCount += 1;
							goto doit;
						}
						//
						pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_ALL);
					}
					pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
					cnt = 0;
				doit:
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[cnt].m_nType = UPART2_TYPE_UPDATE_CRIME;
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[cnt].m_nData1 = pNPC->GetUID();
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[cnt].m_nData2 = crime_val;
					//
					pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				}
#endif
			}
		}
	}
}

// �]�w�p�L�}��
void CMapPlayer::SetCarryNPCMatrix()
{
	long i;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	CMapNPC * pNPC;
//	long matrix;
	struct MAP_GET_SOLDIER_RESULT nRet;
	unsigned long old_hp_max;
	unsigned short old_int;
//	struct plrDATA_SKILL * pSkill;

//	matrix = GetSaveData()->plrMatrix;
	//
//	pSkill = GetSkillData();
	//
	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		if (pNPCSaveData->plrNPC_UID)
		{
			if (pNPCSaveData->plrNPC_ItemCode || pNPCSaveData->plrNPC_SummonCode)
			{
				pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCSaveData->plrNPC_UID, CMapNPC::CLASS_ID);
				if(pNPC != NULL)
				{
					old_hp_max = pNPC->GetCharData()->plrHPMax;
					old_int = pNPC->GetCharData()->plrAttrInt;
				//	if ((pNPC->m_nCharFlags & CHAR_NO_LEVEL_UP) || !pSkill->plrSkillLevelMax[magic_SOLDIER_ARRAY_ATTR])
				//	//if (pNPC->m_nCharFlags & CHAR_NO_LEVEL_UP)			// �l���~ �M �𫰧L�� �L��
				//	{
				//		pNPC->GetSaveData()->plrMatrix = 0xff;
				//	}
				//	else
				//		pNPC->GetSaveData()->plrMatrix = matrix;
					pNPC->GetSaveData()->plrMatrix = pNPC->GetCNPC_BA_Code(this);
					gameServer_CalcCharacterAttribute(pNPC->GetCharData());
					//
					// �p�G��O�W���B���O���ܡA�~�ݭn��sclient���
					if ((old_hp_max != pNPC->GetCharData()->plrHPMax) || (old_int != pNPC->GetCharData()->plrAttrInt))
					{
					//	pNPC->UpdateNPCPart1(0);
						::gameServer_NPC_MakeShowSelf(pNPC->GetCharData(), &nRet.solShowSelf);
						nRet.solIdx = i;
						::msgSendData( GetClientID(), 0, PROTO_MAP_GET_SOLDIER_RESULT, (char *)&nRet, sizeof(nRet), 0 );
					}
				}
			}
		}
	}
}

// �ˬd uid �O�_�O�ۤv�� NPC
struct plrDATA_NPC_SAVE * CMapPlayer::GetCarryNPCDataPtr(unsigned long npc_uid)
{
	struct plrDATA_NPC_SAVE * pNPC = NULL;
	struct plrDATA_NPC * plrNPC;
	long i;

	if (npc_uid && IsNPCUID(npc_uid))
	{
		plrNPC = GetNPCData();
		for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
		{
			if (npc_uid == plrNPC->plrNPCData[i].plrNPC_UID)
			{
				pNPC = &plrNPC->plrNPCData[i];
				return(pNPC);
			}
		}
	}
	return(pNPC);
}

// �ۤv�����p�ɧ�s�p�L����B�z�Ҧ�
void CMapPlayer::CarryNPC_Update(long is_delete)
{
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	CMapNPC * pNPC;
	long i;
	CMapCtrl * pMapCtrl;

	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		//if (pNPCSaveData->plrNPC_ItemCode)
		if (pNPCSaveData->plrNPC_ItemCode || pNPCSaveData->plrNPC_SummonCode)
		{
			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCSaveData->plrNPC_UID, CMapNPC::CLASS_ID);
			if(pNPC != NULL)
			{
				pNPC->m_nCNPC_UpdateFlag = 1;
				if (is_delete)
					pNPC->m_nPlayerUID = 0;		// �D�H����
				//
				pMapCtrl = CMapServer::GetServer()->FindMap(pNPC->GetMapCode(), pNPC->GetMapCopyUID());
				if (pMapCtrl)
					pMapCtrl->SetForceUpdate();
			}
		}
	}
}

void CMapPlayer::UpdateCarryNPCCountryID()
{
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	unsigned long uid;
	long i, icode;
	unsigned short player_country_id, player_country_color;
	long role_code = 0;
	long is_summon = 0;
	struct itemDATA * pItemPtr;
	CMapNPC * pNPC;
	long update_flag = 0;

	player_country_id = GetSaveData()->plrCountryID;
	player_country_color = GetCharData()->plrSetColor;
	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		if (icode = pNPCSaveData->plrNPC_ItemCode)
		{
			if (pItemPtr = ::gameGetItemPtr(icode))
			{
				role_code = pItemPtr->itemUseMagicID;
				// �l���~�ץ�(����)
do_summon:		uid = pNPCSaveData->plrNPC_UID;
				// ����w�s�b
				pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(uid, CMapNPC::CLASS_ID);
				if (pNPC)
				{
					// ��s���y
					if ((pNPC->GetSaveData()->plrCountryID != player_country_id) || (pNPC->GetCharData()->plrSetColor != player_country_color))
					{
						pNPC->GetSaveData()->plrCountryID = (unsigned char)player_country_id;
						pNPC->GetCharData()->plrSetColor = player_country_color;
						pNPC->UpdateShowData();
						//
						AutoSaveNPCData();
						update_flag |= 1;
					}
				}
			}
		}
		else
		{
			role_code = pNPCSaveData->plrNPC_SummonCode;	// �l���~
			if (role_code)
			{
				is_summon++;
				goto do_summon;
			}
		}
	}
	//
	if (update_flag)
	{
		// ��s Client NPC ���
	//	struct plrDATA_NPCSHOW sendData;

	//	gameServer_MakeNPC(GetNPCData(), &sendData, this);
	//	::msgSendData(GetClientID(), 0, PROTO_MAP_GET_SOLDIER_TABLE_RESULT, (char *)&sendData, sizeof(sendData), msgSEND_FLAG_COMPRESS );
		UpdateCarryNPCTable();
	}
}

// ��s Client NPC ���
void CMapPlayer::SyncDeployedCnpcSkillsToClient()
{
	long i;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	CMapNPC * pNPC;
	struct MAP_GET_SOLDIER_RESULT nRet;
	struct plrDATA pFullData;

	if (!GetClientID() || !CMapServer::GetServer()->GetClientValid(GetClientID()))
		return;

	pNPCData = GetNPCData();
	if (!pNPCData)
		return;

	for (i = 0; i < gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		if (!pNPCSaveData->plrNPC_UID)
			continue;
		if (!pNPCSaveData->plrNPC_ItemCode && !pNPCSaveData->plrNPC_SummonCode)
			continue;

		memset(&nRet, 0, sizeof(nRet));
		nRet.solIdx = i;
		pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCSaveData->plrNPC_UID, CMapNPC::CLASS_ID);
		if (pNPC && pNPC->GetCharData())
			gameServer_NPC_MakeShowSelf(pNPC->GetCharData(), &nRet.solShowSelf);
		else
		{
			memset(&pFullData, 0, sizeof(pFullData));
			gameServer_NPC_MakeFullData(pNPCSaveData, &pFullData, 0);
			gameServer_NPC_MakeShowSelf(&pFullData, &nRet.solShowSelf);
			if (!gameCNPC_HasItemSkill(nRet.solShowSelf.plrNPC_ItemSkill) &&
				gameCNPC_HasItemSkill(pNPCSaveData->plrNPC_ItemSkill))
			{
				memcpy(nRet.solShowSelf.plrNPC_ItemSkill, pNPCSaveData->plrNPC_ItemSkill,
					sizeof(nRet.solShowSelf.plrNPC_ItemSkill));
			}
		}
		if (!gameCNPC_HasItemSkill(nRet.solShowSelf.plrNPC_ItemSkill))
			continue;

		::msgSendData(GetClientID(), 0, PROTO_MAP_GET_SOLDIER_RESULT, (char *)&nRet, sizeof(nRet), 0);
	}
}

void CMapPlayer::UpdateCarryNPCTable()
{
	struct plrDATA_NPCSHOW sendData;

	gameServer_MakeNPC(GetNPCData(), &sendData, this);
	::msgSendData(GetClientID(), 0, PROTO_MAP_GET_SOLDIER_TABLE_RESULT, (char *)&sendData, sizeof(sendData), msgSEND_FLAG_COMPRESS );
	SyncDeployedCnpcSkillsToClient();
}

// �O�_���l���, type = 1 �ϥα�
long CMapPlayer::CheckSummonStone(long type, long summon_item)
{
	long i,val;
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA * pItem;

	if (!summon_item)
		summon_item = gameITEM_ID_SUMMON_STONE;
	//
	pItemData = GetItemData();
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(GetSaveData());
	for (i=0; i<val; i++)
	{
		if (pItemData->plrCarryItem[i].m_Item.itemCode == summon_item)
		{
			if (pItemData->plrCarryItem[i].m_Item.itemNumber > 0)
			{
				pItem = ::gameGetItemPtr(summon_item);
				if (pItem)
				{
					if (!type)
						return(1);
					//���log
					CMapServer::GetServer()->SendLogMessage_Item( (CMapPlayer *)this, LOGTYPE_ITEM_USE, 0, &pItemData->plrCarryItem[i] );
					//
					if (--pItemData->plrCarryItem[i].m_Item.itemNumber == 0)
						memset(&pItemData->plrCarryItem[i], 0, sizeof(pItemData->plrCarryItem[i]));
					//�^�ǭt��
					GetCharData()->plrWeight -= pItem->itemWeight;
					((CMapPlayer *)this)->UpdateClientGoldAndWeight();
					((CMapPlayer *)this)->AutoSaveItemData();
					//�^�Ǫ��~�����
					struct MAP_USE_ITEM_RESULT sendData;

					::memset(&sendData, 0, sizeof(sendData));
					sendData.sourcePosID = i;
					::gameServer_Item_MakeShowSelf(&pItemData->plrCarryItem[i], &sendData.itemSelf);
					::msgSendData( ((CMapPlayer *)this)->GetClientID(), 0, PROTO_MAP_USE_ITEM_RESULT, (char *)&sendData, sizeof( sendData ), 0 );
					//
				//	SendMsgToClientByID(24191);
					CMapServer::GetServer()->SendPlayerSystemMessageWithItem((CMapPlayer *)this, 0, 24191, summon_item);
					return(1);
				}
			}
		}
	}
	return(0);
}
//***************************************** Save Data *****************************************

int CMapPlayer::ProtocoToSaveType(long nProtoco)
{
	int	cnt1;

	for(cnt1 = 0; cnt1 < DB_TOTAL_TYPES_ALL; cnt1++)
	{
		if(m_nSaveProtoco[cnt1] == nProtoco)
			return(cnt1);
	}

	return(-1);
}

// �O�_����s��
long CMapPlayer::CheckPlayerNoSave()
{
/*	register long sts;

	sts = GetStateProc();
	//if ((sts == CMapObject::STATE_LEAVE_MAP) || (sts == CMapObject::STATE_DELETE))
	if (sts == CMapObject::STATE_DELETE)
	{
		CMapServer::GetServer()->Log("Player(%d) cannot save data besause been deleted", GetUID());
		return(1);
	} */
	if (m_IsLoadingData)			// ��Ʃ|��Ū������
		return(1);
	//
	//if (m_bSaveDataFlag || m_bSaveItemFlag || m_bSaveNPCFlag || m_bSaveSkillFlag || m_bSaveStorageFlag)
	//	return(0);
	//
#ifdef USE_OFFLINE_STALL
	if (m_bOfflineStallGhost)
		return(0);
#endif
	if(!IsReady())
		return(1);
	return(0);
}

void CMapPlayer::SaveAllData(long nProtoUID, long Stage_Teleport)
{
#ifdef USE_OFFLINE_STALL
	if (!IsReady() && !m_bOfflineStallGhost)
#else
	if (!IsReady())
#endif
		return;

	if (CheckPlayerNoSave())
		return;

	if (m_IsLoadingData)			// ��Ʃ|��Ū������
		return;

/*	if (SaveCharData(nProtoUID, Stage_Teleport))
	{
		SaveSkillData(nProtoUID);
		SaveItemData(nProtoUID);
		SaveStorageData(nProtoUID);
		SaveNPCData(nProtoUID);
		SaveMissionData(nProtoUID);
		SaveFriendData(nProtoUID);
	}
	*/
	if (CanSaveData())
	{
		SaveSkillData(nProtoUID);
		SaveItemData(nProtoUID);
		SaveStorageData(nProtoUID);
		SaveNPCData(nProtoUID);
		SaveMissionData(nProtoUID);
		SaveFriendData(nProtoUID);
		//
		SaveCharData(nProtoUID, Stage_Teleport);
	}
}

void CMapPlayer::SaveAllData_Lite()
{
	long nProtoUID = 0, Stage_Teleport = 0;

#ifdef USE_OFFLINE_STALL
	if (!IsReady() && !m_bOfflineStallGhost)
#else
	if (!IsReady())
#endif
		return;

	if (CheckPlayerNoSave())
		return;

	if (m_IsLoadingData)			// ��Ʃ|��Ū������
		return;

/*	if (SaveCharData(nProtoUID, Stage_Teleport))
	{
		if (m_bSaveSkillFlag)
			SaveSkillData(nProtoUID);
		//if (m_bSaveItemFlag)
			SaveItemData(nProtoUID);
		SaveStorageData(nProtoUID);
		//if (m_bSaveNPCFlag)
			SaveNPCData(nProtoUID);
	//	SaveMissionData(nProtoUID);
	//	SaveFriendData(nProtoUID);
	}
	*/
	if (CanSaveData())
	{
		if (m_bSaveSkillFlag)
			SaveSkillData(nProtoUID);
		if (m_bSaveItemFlag)
			SaveItemData(nProtoUID);
		if (m_bSaveStorageFlag)
			SaveStorageData(nProtoUID);
		//if (m_bSaveNPCFlag)
			SaveNPCData(nProtoUID);
	//	SaveMissionData(nProtoUID);
	//	SaveFriendData(nProtoUID);
		//
		SaveCharData(nProtoUID, Stage_Teleport);
	}
}

// �_�u�e�x�s���
void CMapPlayer::SaveAuotSaveData()
{
	AutoSaveCharData(1);
	AutoSaveItemData(1);
	AutoSaveSkillData(1);
	AutoSaveStorageData(1);
	AutoSaveNPCData(1);
}

void CMapPlayer::CancelSaveWaitForRemove()
{
	long i;

	m_IsSavingData = false;
	for (i = 0; i < DB_TOTAL_TYPES_ALL; i++)
		m_nSaveTime[i] = 0;
}

void CMapPlayer::AutoSaveCharData(long flush)
{
	if (flush)
	{
		if (m_bSaveDataFlag)
			SaveCharData(0);
	}
	else
		m_bSaveDataFlag = true;
}

void CMapPlayer::AutoSaveItemData(long flush)
{
	if (flush)
	{
		if (m_bSaveItemFlag)
			SaveItemData();
	}
	else
		m_bSaveItemFlag = true;
}

void CMapPlayer::AutoSaveSkillData(long flush)
{
	if (flush)
	{
		if (m_bSaveSkillFlag)
			SaveSkillData();
	}
	else
		m_bSaveSkillFlag = true;
}

void CMapPlayer::AutoSaveNPCData(long flush)
{
	if (flush)
	{
		if (m_bSaveNPCFlag)
			SaveNPCData();
	}
	else
		m_bSaveNPCFlag = true;
}

void CMapPlayer::AutoSaveStorageData(long flush)
{
	if (flush)
	{
		if (m_bSaveStorageFlag)
			SaveStorageData();
	}
	else
		m_bSaveStorageFlag = true;
}

long CMapPlayer::CanSaveData()
{
	if (CheckPlayerNoSave())
		return(0);
	if (GetSaveData()->plrUID != GetUID())	// Test: �O�_�|�o���x�s���~���D
		return(0);
	return(1);
}

long CMapPlayer::SaveCharData(long nProtoUID, long Stage_Teleport)
{
	struct DB_SAVE_PLAYER_DATA	PlayerMsg;
	int							hDBServer = CMapServer::GetServer()->GetDBServer();
	int oldMap, oldMapUID, oldX, oldY, flg;
	//
	long r = 1;
//	long i;
//	struct plrDATA_NPC_SAVE * pNPCSave;
//	CMapNPC * pNPC;

	m_bSaveDataFlag = false;
	//
	if (CheckPlayerNoSave())
	{
		m_bSaveDataFlag = true;
		return(0);
	}

//CMapServer::GetServer()->Log("Save char data (%d,%s)...", GetUID(), GetName());
	//
//	if(m_nSaveTime[DB_CHAR_DATA] > 0)
//		return(1);		// �s�ɮɶ��ӧO�p��(���}���ܡA�|���u�ɶ��L�k�s�ɰ��D)
	m_nSaveTime[DB_CHAR_DATA]	= DB_TIMEOUT;
	//
	flg = 1;
	oldMap = GetSaveData()->plrMapCode;
	oldMapUID = GetSaveData()->plrMapCopyCode;
	oldX = GetSaveData()->plrPosX;
	oldY = GetSaveData()->plrPosY;
	//
	if (Stage_Teleport)		// �p�G�O�ƥ��a�ϡA�����w���ܧ�ǰe�a�Ϧ�m
	{
		if (CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_IsSpaceMap(oldMap))
			Stage_Teleport = 0;
	}
	//
	if(GetHP() <= 0)	// �s�ɮɡA�����m�]�w�^�_���I
	{
//		SetHP(GetMaxHP() / 5);
		//
		if (Stage_Teleport)
		{
			if (!ChangeMapInStageMapPos(1))	// �̷Ӧa�ϳ]�w�ܧ�
				goto save_pos;
		}
		else
		{
save_pos:	GetSaveData()->plrMapCode = GetSaveData()->plrSaveMapCode;
			GetSaveData()->plrMapCopyCode = 0;
			GetSaveData()->plrPosX = GetSaveData()->plrSavePosX;
			GetSaveData()->plrPosY = GetSaveData()->plrSavePosY;
		}
	}
	else
	{
		if (Stage_Teleport)
			ChangeMapInStageMapPos(1);
	}
//	// �p�L���
//	for (i=0; i<gameMAX_CARRY_SOLDIER; i++)
//	{
//		pNPCSave = &GetNPCData()->plrNPCData[i];
//		if (pNPCSave->plrNPC_UID)
//		{
//			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCSave->plrNPC_UID);
//			if (pNPC)
//			{
//				gameServer_NPCSave_MakeFullData(pNPC->GetCharData(), pNPCSave);
//			}
//		}
//	}
	//
	PlayerMsg.nUID	= GetUID();
	if (GetCharData()->plrSaveData.plrUID == PlayerMsg.nUID)	// Test: �O�_�|�o���x�s���~���D
	{
		::memcpyT(&PlayerMsg.m_SaveData, &GetCharData()->plrSaveData, sizeof(struct plrDATA_SAVE));
		CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYER_DATA, (char *)&PlayerMsg, sizeof(PlayerMsg), 0);
		//
		m_IsSavingData	= true;
	}
	else
	{
		CMapServer::GetServer()->Log("SERIOUS ERROR: Save Player(%d) data uid not match, save uid(%d)!", GetUID(), GetCharData()->plrSaveData.plrUID);
		r = 0;
		m_nSaveTime[DB_CHAR_DATA] = 0;
	}

	if (flg)
	{
		GetSaveData()->plrMapCode = oldMap;
		GetSaveData()->plrMapCopyCode = oldMapUID;
		GetSaveData()->plrPosX = oldX;
		GetSaveData()->plrPosY = oldY;
	}
	return(r);
}

void CMapPlayer::SaveSkillData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_SKILL	SkillMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	m_bSaveSkillFlag = false;
	//
	if (CheckPlayerNoSave())
		return;

//	if(m_nSaveTime[DB_SKILL_DATA] > 0)
//		return;
	m_nSaveTime[DB_SKILL_DATA] = DB_TIMEOUT;

	SkillMsg.nUID	= GetUID();
//	gameServerSave_MakeSkill(GetSkillData(), &SkillMsg.nData);
	::memcpyT(&SkillMsg.nData, GetSkillData(), sizeof(struct plrDATA_SKILL_SAVE));
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_SKILL, (char *)&SkillMsg, sizeof(SkillMsg), 0);

	m_IsSavingData	= true;
}

void CMapPlayer::SaveItemData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_ITEM	ItemMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();

	m_bSaveItemFlag = false;
	//
	if (CheckPlayerNoSave())
	{
		m_bSaveItemFlag = true;		// 2007/04/03 �ѨM��������o���~
		return;
	}

//	CMapServer::GetServer()->Log("Save item data ...");

//	if(m_nSaveTime[DB_ITEM_DATA] > 0)
//		return;
	m_nSaveTime[DB_ITEM_DATA] = DB_TIMEOUT;

	ItemMsg.nUID	= GetUID();
	::memcpyT(&ItemMsg.nData, GetItemData(), sizeof(struct plrCARRYITEM_SAVE));
//	gameServerSave_MakeCarryItem(GetItemData(), &ItemMsg.nData);
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_ITEM, (char *)&ItemMsg, sizeof(ItemMsg), 0);

	m_IsSavingData	= true;
}

void CMapPlayer::SaveSingleItemData(long nItemIdx)
{
	int hDBServer = CMapServer::GetServer()->GetDBServer();
	struct DB_SAVE_PLAYERDATA_ITEM_SINGLE nReq;
	struct plrCARRYITEM_SAVE * pCarryItem;

	if (CheckPlayerNoSave())
	{
		m_bSaveItemFlag = true;
		return;
	}
//	if(m_nSaveTime[DB_ITEM_DATA] > 0)
//		return;
	//
	if (nItemIdx < gameMAX_CARRYITEM)	// �קK���s��A�ϥγ̤j��
	//if (nItemIdx < ::gameServer_GetCarryItemMax(GetSaveData()))
	{
		pCarryItem = GetItemData();
	//	if (pCarryItem->plrCarryItem[nItemIdx].m_Item.itemCode)	// ���i��M��
	//	{
			nReq.nUID = GetUID();
			nReq.itemIdx = nItemIdx;
			memcpyT(&nReq.itemData, &pCarryItem->plrCarryItem[nItemIdx], sizeof(struct itemDATA_SAVE));
			CMapServer::GetServer()->SendData(hDBServer, 0, PROTO_DB_SAVE_PLAYERDATA_ITEM_SINGLE, (char *)&nReq, sizeof(nReq), 0);
	//	}
	}
	else
		CMapServer::GetServer()->Log("ERROR: Save player(%d) carry item out of range(index = %d)", GetUID(), nItemIdx);
}

void CMapPlayer::UpdateClientWawaItem(long pos)
{
	struct plrItemSaveData * pWawaItem;
	struct MAP_UPDATE_WAWA_ITEM nWawaUpdate;
	
	pWawaItem = GetPlrWAWAEquipItemPtr(GetSaveData(), pos);
	if (pWawaItem)
	{
		nWawaUpdate.nTotal = 1;
		DeputyItem_FillUpdatePacket(&nWawaUpdate.nItemData[0], pWawaItem, (unsigned short)pos);
		::msgSendData( GetClientID(), 0, PROTO_MAP_UPDATE_WAWA_ITEM, (char *)&nWawaUpdate, nWawaUpdate.GetSize(), 0 );
	}
}

void CMapPlayer::UpdateClientDeputyItem(long pos)
{
	struct plrItemSaveData * pDeputyItem;
	struct MAP_UPDATE_DEPUTY_ITEM nDeputyUpdate;

	if (pos < 0 || pos >= EQUIP_POS_TOTAL)
		return;
	pDeputyItem = &GetSaveData()->plrDeputyEquip[pos];
	nDeputyUpdate.nTotal = 1;
	DeputyItem_FillUpdatePacket(&nDeputyUpdate.nItemData[0], pDeputyItem, (unsigned short)pos);
	::msgSendData(GetClientID(), 0, PROTO_MAP_UPDATE_DEPUTY_ITEM, (char *)&nDeputyUpdate, nDeputyUpdate.GetSize(), 0);
}

void CMapPlayer::UpdateClientAllDeputyItems(void)
{
	long pos;

	for (pos = 0; pos < gameMAX_DEPUTY_EQUIP_SLOT; pos++)
		UpdateClientDeputyItem(pos);
}

void CMapPlayer::SaveStorageData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_STORAGE	StorageMsg;
	int									hDBServer = CMapServer::GetServer()->GetDBServer();

	m_bSaveStorageFlag = false;
	//
	if (CheckPlayerNoSave())
	{
		m_bSaveStorageFlag = true;
		return;
	}

//	if(m_nSaveTime[DB_STORAGE_DATA] > 0)
//		return;
	m_nSaveTime[DB_STORAGE_DATA] = DB_TIMEOUT;

	StorageMsg.nUID			= GetUID();
	StorageMsg.nAccountID	= GetAccountID();
	::memcpyT(&StorageMsg.nData, GetStorageData(), sizeof(struct plrSTORAGEITEM_SAVE));
//	gameServerSave_MakeStorageItem(GetStorageData(), &StorageMsg.nData);
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_STORAGE, (char *)&StorageMsg, sizeof(StorageMsg), 0);

	m_IsSavingData	= true;
}

void CMapPlayer::SaveNPCData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_NPC	NPCMsg;
	int								hDBServer = CMapServer::GetServer()->GetDBServer();
	long i;
	CMapNPC * pNPC;
	struct plrDATA_NPC_SAVE * pNPCSave;

	m_bSaveNPCFlag = false;
	//
	if (CheckPlayerNoSave())
	{
		m_bSaveNPCFlag = true;
		return;
	}

//	if(m_nSaveTime[DB_NPC_DATA] > 0)
//		return;
	m_nSaveTime[DB_NPC_DATA] = DB_TIMEOUT;

	// NPC ��ƥ� CharData �ন SaveData
	for (i=0; i<gameMAX_CARRY_SOLDIER; i++)
	{
		pNPCSave = &GetNPCData()->plrNPCData[i];
		if (pNPCSave->plrNPC_UID)
		{
			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID( pNPCSave->plrNPC_UID, CMapNPC::CLASS_ID );
			if (pNPC)
			{
				if (CMapServer::GetServer()->IsObjectDeleted(pNPC->GetHandle()))
				{
					CMapServer::GetServer()->Log("ERROR: Save deleted NPC(%08x) !", pNPC->GetUID());
					continue;
				}
				{
					struct CNPC_ITEM_SKILL_SAVE tmpSkills[gameMAX_SPECIAL_ATTACK];
					struct plrDATA * pCharData = pNPC->GetCharData();

					if (!gameCNPC_HasItemSkill(pCharData->plrNPC_ItemSkill))
					{
						if (CMapPlayer_ResolveCnpcDeploySkills(this, &pNPCSave->plrNPC_ItemUID,
							pNPCSave->plrNPC_ItemSkill, tmpSkills))
							memcpy(pCharData->plrNPC_ItemSkill, tmpSkills, sizeof(pCharData->plrNPC_ItemSkill));
					}
				}
				gameServer_NPCSave_MakeFullData(pNPC->GetCharData(), pNPCSave);
			}
		}
	}

	NPCMsg.nUID	= GetUID();
	::memcpyT(&NPCMsg.nData, GetNPCData(), sizeof(struct plrDATA_NPC));
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_NPC, (char *)&NPCMsg, sizeof(NPCMsg), 0);

	m_IsSavingData	= true;
}

void CMapPlayer::SaveMissionData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_MISSION	MissionMsg;
	int									hDBServer = CMapServer::GetServer()->GetDBServer();

	if (CheckPlayerNoSave())
		return;

//	if(m_nSaveTime[DB_MISSION_DATA] > 0)
//		return;
	m_nSaveTime[DB_MISSION_DATA] = DB_TIMEOUT;

	MissionMsg.nUID	= GetUID();
//	gameServerSave_MakeMission(GetMissionData(), &MissionMsg.nData);
	::memcpyT(&MissionMsg.nData, GetMissionData(), sizeof(struct plrMISSION_SAVE));
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_MISSION, (char *)&MissionMsg, sizeof(MissionMsg), 0);

	m_IsSavingData	= true;
}

void CMapPlayer::SaveFriendData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_FRIEND	FriendMsg;
	int									hDBServer = CMapServer::GetServer()->GetDBServer();

	if (CheckPlayerNoSave())
		return;

//	if(m_nSaveTime[DB_FRIEND_DATA] > 0)
//		return;
	m_nSaveTime[DB_FRIEND_DATA] = DB_TIMEOUT;

	FriendMsg.nUID	= GetUID();
	::memcpyT(&FriendMsg.nData, &GetFriendData()->pFB, sizeof(struct plrDATA_FRIEND_SAVE));
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_FRIEND, (char *)&FriendMsg, sizeof(FriendMsg), 0);

	m_IsSavingData	= true;
}

void CMapPlayer::SaveFashionData(long nProtoUID)
{
	struct DB_SAVE_PLAYERDATA_FASHION	FashionMsg;
	int									hDBServer = CMapServer::GetServer()->GetDBServer();

	if (CheckPlayerNoSave())
		return;

	m_nSaveTime[DB_FASHION_DATA] = DB_TIMEOUT;

	FashionMsg.nUID	= GetUID();
	FashionMsg.nAccountID = GetAccountID();
	::memcpyT(&FashionMsg.nData, &m_FashionData, sizeof(struct plrDATA_FASHION_SAVE));
	CMapServer::GetServer()->SendData(hDBServer, nProtoUID, PROTO_DB_SAVE_PLAYERDATA_FASHION, (char *)&FashionMsg, sizeof(FashionMsg), 0);

	m_IsSavingData	= true;
}

void CMapPlayer::SaveResult(long nProto, long nRes)
{
	int	cnt1, nType;

	nType = ProtocoToSaveType(nProto);
	if(nType == -1)
		return;

	if(m_nSaveTime[nType] > 0)
	{
		//
		//
		//
		m_nSaveTime[nType] = 0;
	}

//CMapServer::GetServer()->Log( "Save result(%d)", nType );

	m_IsSavingData	= false;
	for(cnt1 = 0; cnt1 < DB_TOTAL_TYPES; cnt1++)
	{
		if(m_nSaveTime[cnt1] > 0)
		{
			m_IsSavingData = true;
			break;
		}
	}
}
// =======================================================
// NPC Talk
// =======================================================
void CMapPlayer::SetNPCTalk(unsigned long uid, CMapNPC * pChar, long msg_id)
{
	if (pChar)
	{
		m_nTalkNPC_UID = uid;
		pChar->m_nTalkNumber++;
	}
	//
	m_nNPCTalkMsgID = msg_id;
}

unsigned long CMapPlayer::GetNPCTalkUID()
{
	return(m_nTalkNPC_UID);
}

long CMapPlayer::IsNPCTalk()
{
	//return(m_nTalkNPC_UID);
	return(m_nNPCTalkMsgID);
}

// �@��ʦ^�ǵ��G, result = 1 ok
void CMapPlayer::SendNPCTalkResult(struct proto_COMM * pComm, long result, long msg_id, long data1)
{
	struct MAP_NPC_PROC_RESULT nRet;

	memset(&nRet, 0, sizeof(nRet));
	nRet.nResult = result;				// ok
	nRet.nSerial = data1;
	nRet.nMsgID = msg_id;
	if (pComm)
	{
		::msgSendData(GetClientID(), pComm->pcUID, PROTO_MAP_NPC_PROC_RESULT, (char *)&nRet, sizeof(nRet), 0);
	}
	else
		::msgSendData(GetClientID(), 0, PROTO_MAP_NPC_PROC_RESULT, (char *)&nRet, sizeof(nRet), 0);
}

long CMapPlayer::ExitNPCTalk()
{
	long r = 0;
	unsigned long uid;
	CMapNPC * pChar;

	uid = m_nTalkNPC_UID;
	if (uid)
	{
		if (uid != 0xffffffff)		// �S��������
		{
			r++;
			pChar = (CMapNPC *)CMapServer::GetServer()->FindAndCheckCharExistByUID(uid, CMapNPC::CLASS_ID);
			if (pChar)
			{
				if (pChar->IsKindOf(CMapNPC::CLASS_ID))
				{
					if (pChar->m_nTalkNumber > 0)
						pChar->m_nTalkNumber--;
				}
			}
		}
		m_nTalkNPC_UID = 0;
	}
	//
	//m_WaitVT_State &= ~PLAYER_NPCTALK_WAIT_DATA;	// ���� VT �^�Ǹ��
	m_WaitVT_State &= PLAYER_NPCTALK_WAIT_STATE;	// �����Ҧ��䥦�X���A�O�d��w
	//
	m_nNPCTalkMsgID = 0;
	memset(&m_nNPCSelectID, 0, sizeof(m_nNPCSelectID));
	return(r);
}

void CMapPlayer::SetMissionState(long id, long state)
{
	long i, size, exist;
	struct gameMISSION_DATA * pMissionTbl;
	struct plrMISSION_SAVE * pMIS;

	pMIS = GetMissionData();
	if (id && (id < gameMAX_MISSION))
	{
		exist = 0;
		pMissionTbl = gameGetMissionTablePtr(id);
		for (i=0; i<gameMAX_MISSION_ACCEPT; i++)
		{
			if (pMIS->pmData[i].pmdUID == id)	// �w�g�ӱ�
			{
				exist++;
				// ���e��
				//pMIS->pmData[i].pmdUID = 0;
				if (i < (gameMAX_MISSION_ACCEPT - 1))
				{
					size = ((gameMAX_MISSION_ACCEPT - 1) - i) * sizeof(struct plrMISSION_DETAIL);
					memcpyT(&pMIS->pmData[i], &pMIS->pmData[i+1], size);
				}
				pMIS->pmData[gameMAX_MISSION_ACCEPT-1].pmdUID = 0;
				//
				pMIS->pmStatus[id] = (char)state;
				if (pMissionTbl)
				{
					if (pMissionTbl->gmdRepeat)
					{
						if (state == gameMISSION_STATE_NONE)	// ���ܲM��
						{
							SetMissionRepeat(id, 0);
						}
						else
							SetMissionRepeat(id, pMissionTbl->gmdRepeat);
					}
				}
			}
		}
		//
		if (!exist)
		{
			if (pMissionTbl)
			{
				pMIS->pmStatus[id] = (char)state;
				//
				if (pMissionTbl->gmdRepeat)
				{
					if (state == gameMISSION_STATE_NONE)	// ���ܲM��
					{
						SetMissionRepeat(id, 0);
					}
					else
						SetMissionRepeat(id, pMissionTbl->gmdRepeat);
				}
			}
		}
	}
}

void CMapPlayer::SendMissionDataToClient()
{
	struct MAP_SEND_MISSION_DATA nRet;

	gameServer_MakeMission(GetMissionData(), &nRet.nData);
	::msgSendData(GetClientID(), 0, PROTO_MAP_SEND_MISSION_DATA, (char *)&nRet, sizeof(nRet), 0);
}

// ���ޭt�����D
long CMapPlayer::MissionGetDropItem(long drop_id, long gold_num, long random_flag, long log_type, long *get_id, long *get_num, unsigned long *get_ratio)
{
	long r = 0;
	long get_item_id, num;
	struct itemDATA * pItemData;
	long n;
//	CMapCharMsg *	pMsg;

	get_item_id = gameGetDropItem(gameGetRandomRange(0, gameMAX_DROPITEM_RANDOM), drop_id, 0, get_ratio);
	if (get_id)
		*get_id = get_item_id;
	//
	if (!log_type)
		log_type = LOGTYPE_ITEM_FROM_NPC_GET;
	//
	switch (get_item_id)
	{
	case item_Nothing:				// 0
#ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("��o Nothing !");
#endif
		break;
/*	case item_Money:				// 0xffffffff	��o����
		r++;		// ���@���\
		break;
	default:						// ��o���~
		num = 1;
		if (::gameGetItemTypeByID(get_item_id) & (itemTypeArrow | itemTypeThrow))
		{
			num = gameGetRandomRange(10, 21);
		}
		if (this->MakeItem(get_item_id, num, log_type, this, random_flag))
		{
			if (log_type != LOGTYPE_ITEM_FROM_USEITEM_GET)	// �ϥΪ��~��o���t�~���q���覡
			{
				pMsg			= InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
			//	pMsg->m_nSize	= sizeof(struct MAP_UPDATE_PLAYER_DATA_PART2);
				pMsg->m_nSize	= pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_GETITEM;
				pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = get_item_id;			// item_id
				pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData2 = 0;					// flag
				pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nVal = num;					// �ƥ�
			}
			//
			r++;
		}
#ifndef USE_PACKAGE_DATA
		else
		{
			CMapServer::GetServer()->Log("��o(%d, %d)���� !", get_item_id, num);
		}
#endif
		break;
		*/
	default:
		num = 1;
		//if ((get_item_id & 0x8000) == 0)			// �D�S�����~
		if (((unsigned long)get_item_id) <= 60000)	// �D�S�����~
		{
			pItemData = gameGetItemPtr(get_item_id);
			if (pItemData)
			{
				n = pItemData->itemType & ~(itemTypeNoTrade | itemTypeNoDrop | itemTypeNoMove);
				if (n & (itemTypeArrow | itemTypeThrow | itemTypeEffect))
				{
					random_flag = 0;
					num = gameGetRandomRange(10, 21);	// ���t�Ϭ��ʫ�}��A�Ϥ����@�w�ƥ�
				}
				else if ((n == itemTypeItem) || (n == (itemTypeItem | itemTypeNoUse)) || (n == (itemTypeItem | itemTypeSoldier)) || gameIsSoldierItemByPtr(pItemData, FLAG_SIT_ENGINEER))
				{		// �D��B�����B�L�� ��
					if (pItemData->itemMissionDropNum)
					{
						num = gameGetMinMaxVal(pItemData->itemMissionDropNum);
						if (!num)
							num++;
					}
				}
			}
		}
		//
		if (log_type != LOGTYPE_ITEM_FROM_USEITEM_GET)	// �ϥΪ��~��o���t�~���q���覡
		{
			PlayerDrop_Init();
			if (get_item_id == item_Money)
			{
				num = gold_num;
			}
			num = CarryItem_MakeItemOnFreeSpace(get_item_id, num, log_type, random_flag, this, 1);
			PlayerDrop_SendMissionResult(this);
		}
		else
		{
			if (get_item_id != item_Money)				// �ϥΪ��~��o���ѤW�h�B�z
				num = CarryItem_MakeItemOnFreeSpace(get_item_id, num, log_type, random_flag, this, 1);
		}
		//
		r++;
		if (get_num)
			*get_num = num;
		break;
	}
	return(r);
}

// �O�_���p�L
// type: 1=���~��; 2=�h�L��C
long CMapPlayer::CheckCarryNPC(long type)
{
	long i, val;
	struct plrCARRYITEM_SAVE * pPlrCarryItem;
	struct itemDATA_SAVE * pItem;
	//
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;

	if (type == 1)
	{
		pPlrCarryItem = GetItemData();
		//for(i=0; i<gameMAX_CARRYITEM; i++)
		val = ::gameServer_GetCarryItemMax(GetSaveData());
		for(i=0; i<val; i++)
		{
			pItem = &pPlrCarryItem->plrCarryItem[ i ];
			if (pItem->m_Item.itemCode)
			{
				if (pItem->m_NPC.inpcFlags & itemSHOW_FLAG_ALL_CNPC)
					return(1);
			}
		}
	}
	else if (type == 2)
	{
		pNPCData = GetNPCData();
		for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
		{
			pNPCSaveData = &pNPCData->plrNPCData[i];
			//if (pNPCSaveData->plrNPC_ItemCode)
			if (pNPCSaveData->plrNPC_ItemCode || pNPCSaveData->plrNPC_SummonCode)
			{
				return(1);
			}
		}
	}
	return(0);
}

void CMapPlayer::ResetAllAttribute()			// �ƥ\�G���m�Ҧ��I��
{
	struct plrDATA_SAVE * pSave;
	struct PLAYER_INIT_DATA nBaseAttr;
	long long total, leave_total;				// 计算剩余能力点数

	pSave = GetSaveData();
	if (!pSave)
		return;
	if (pSave->plrLevel < 1)
		return;
	if (serverGetPlayerAttrInitData(&nBaseAttr, pSave->plrSex, pSave->plrJobID))
	{
		// ���έp�ثe�Ҧ��`�M
		total = (long long)pSave->plrBaseAttrStr + (long long)pSave->plrBaseAttrInt + (long long)pSave->plrBaseAttrDex + (long long)pSave->plrBaseAttrMind + (long long)pSave->plrBaseAttrCon + (long long)pSave->plrBaseAttrLeader;
		total += (long long)pSave->plrAttrPoint;
		// �Ѿl�I�� = �ثe�Ҧ��`�M - ���I��
		leave_total = nBaseAttr.nStr + nBaseAttr.nInt + nBaseAttr.nDex + nBaseAttr.nMind + nBaseAttr.nCon + nBaseAttr.nLead;
		leave_total = total - leave_total;
		if (leave_total < 0)
			leave_total = 0;
		// ��l�ƤW��
		pSave->plrAttrStrMax = 50;
		pSave->plrAttrIntMax = 50;
		pSave->plrAttrDexMax = 50;
		pSave->plrAttrMindMax = 50;
		pSave->plrAttrConMax = 50;
		pSave->plrAttrLeaderMax = 50;
		// ��l���ݩ�
		pSave->plrBaseAttrStr = nBaseAttr.nStr;
		pSave->plrBaseAttrInt = nBaseAttr.nInt;
		pSave->plrBaseAttrDex = nBaseAttr.nDex;
		pSave->plrBaseAttrMind = nBaseAttr.nMind;
		pSave->plrBaseAttrCon = nBaseAttr.nCon;
		pSave->plrBaseAttrLeader = nBaseAttr.nLead;
		if ((unsigned long long)leave_total > gameMAX_ATTR_POINT)
			leave_total = (long long)gameMAX_ATTR_POINT;
		// 初始化剩余点数
		pSave->plrAttrPoint = (unsigned long long)leave_total;
	}
}

// return: < 0 �������I��
long long InnerResetAttributeCheck(long long cur_attr, long long pts, long long base_attr)
{
	long long min_pts;

	min_pts = base_attr + pts;			// �̤p�I��
	if (cur_attr < min_pts)
	{
		return(cur_attr - min_pts);		// �n�ժ��I�Ƥ���, < 0
	}
	cur_attr -= pts;
	return(cur_attr);
}

// [1:�O�q][2:���O][3:����][4:�믫][5:���][6:�αs]
// type = 0 5�I, 1 25�I
long CMapPlayer::ResetAttribute(long attr_type, long type)
{
	struct PLAYER_INIT_DATA nBaseAttr;
	struct plrDATA_SAVE * pSave;
	long val, i;
	unsigned long long pts, pts_total;
	unsigned long long cur_attr, max_attr;
	//unsigned long long max_str, max_int, max_dex, max_mind, max_con, max_leader;
	unsigned long long * pval;
	unsigned long long * pValPtr;
	unsigned long long nBaseAttrOrg[6+1];
	unsigned long long * nBaseAttrCharPtr[6+1];
	unsigned long long nMaxAttrChar[6+1];
	CMapCharMsg * pMsg;

	pSave = GetSaveData();
	if (serverGetPlayerAttrInitData(&nBaseAttr, pSave->plrSex, pSave->plrJobID))
	{
		if (type == 1)
		{
			pts = 25;							// �ƥ\�I��
		}
		else
			pts = gameRESET_ATTR_POINT;			// �ƥ\�I��
		pts_total = pts + pSave->plrAttrPoint;
		if (pts_total > gameMAX_ATTR_POINT)					// 错误：点数太多
		{
			SendMsgToClientByID(24165);
			return(0);
		}
		if ((attr_type < 1) || (attr_type > 6))	// ���~�Gid��
			return(0);
		//
		cur_attr = 0;		// �n�ժ���H����
		pval = NULL;
		//
		nMaxAttrChar[1] = pSave->plrAttrStrMax;
		nMaxAttrChar[2] = pSave->plrAttrIntMax;
		nMaxAttrChar[3] = pSave->plrAttrDexMax;
		nMaxAttrChar[4] = pSave->plrAttrMindMax;
		nMaxAttrChar[5] = pSave->plrAttrConMax;
		nMaxAttrChar[6] = pSave->plrAttrLeaderMax;
		//
		nBaseAttrOrg[1] = nBaseAttr.nStr;
		nBaseAttrOrg[2] = nBaseAttr.nInt;
		nBaseAttrOrg[3] = nBaseAttr.nDex;
		nBaseAttrOrg[4] = nBaseAttr.nMind;
		nBaseAttrOrg[5] = nBaseAttr.nCon;
		nBaseAttrOrg[6] = nBaseAttr.nLead;
		//
		nBaseAttrCharPtr[1] = &pSave->plrBaseAttrStr;
		nBaseAttrCharPtr[2] = &pSave->plrBaseAttrInt;
		nBaseAttrCharPtr[3] = &pSave->plrBaseAttrDex;
		nBaseAttrCharPtr[4] = &pSave->plrBaseAttrMind;
		nBaseAttrCharPtr[5] = &pSave->plrBaseAttrCon;
		nBaseAttrCharPtr[6] = &pSave->plrBaseAttrLeader;
		//
		for (i=1; i<=6; i++)
		{
			max_attr = nMaxAttrChar[i];
			pValPtr = nBaseAttrCharPtr[i];
			if (attr_type == i)						// �n�ժ���H
			{
				cur_attr = *pValPtr;
				pval = pValPtr;
				val = InnerResetAttributeCheck(cur_attr, pts, nBaseAttrOrg[i]);
				if (val < 0)
				{
					val = -val;
					//
					pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
					
					pMsg->m_Msg.m_UpdateData2Msg.Reset();
					pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_RESET_ATTR_ERR1, 0, val);
					
					pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					return(0);						// �n�ժ��I�Ƥ���
				}
				else
					cur_attr = val;
			}
			else
			{
				if (max_attr < pts*2)
				{
					SendMsgToClientByID(24166);
					return(0);						// �W������
				}
				max_attr -= (pts*2);
				if (max_attr < *pValPtr)
				{
					SendMsgToClientByID(24166);
					return(0);						// �W������
				}
				nMaxAttrChar[i] = max_attr;			// 2006/06/17 Bug Fix
			}
		}
		// ���G
		if (pval && cur_attr)
		{
			*pval = cur_attr;
			pSave->plrAttrStrMax = nMaxAttrChar[1];
			pSave->plrAttrIntMax = nMaxAttrChar[2];
			pSave->plrAttrDexMax = nMaxAttrChar[3];
			pSave->plrAttrMindMax = nMaxAttrChar[4];
			pSave->plrAttrConMax = nMaxAttrChar[5];
			pSave->plrAttrLeaderMax = nMaxAttrChar[6];
			//
			pSave->plrAttrPoint = pts_total;
			// �q�����G
			pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_RESET_ATTR_RESULT, (short)attr_type, cur_attr, pts);

			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			return(1);
		}
	}
	return(0);
}

long InnerCheckCarryItem_Horse(struct plrCARRYITEM_SAVE * pPlrCarryItem, struct plrDATA_SAVE * pSave)
{
	long i, itype, val;
	struct itemDATA_SAVE * pItem;

	//for(i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(pSave);
	for(i=0; i<val; i++)
	{
		pItem = &pPlrCarryItem->plrCarryItem[ i ];
		if (pItem->m_Item.itemCode)
		{
			itype = ::gameGetItemTypeByID(pItem->m_Item.itemCode);
			if( itype & itemTypeHorse )
				return(1);
		}
	}
	return(0);
}

struct itemDATA_SAVE * InnerFindSoulTicket(CMapPlayer * pPlayer, long duedate)
{
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, val;

	pItemData = pPlayer->GetItemData();
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
	for (i=0; i<val; i++)
	{
		pItem = &pItemData->plrCarryItem[i];
		if (pItem->m_Item.itemCode == gameITEM_ID_SOUL_TICKET)
		{
			if (pItem->m_Item.itemNumber > 0)
			{
				if (pItem->m_Item.itemFlags & itemSHOW_FLAG_STICKET)
				{
					if (pItem->m_STicket.itemDuedate == duedate)
					{
						return(pItem);
					}
				}
			}
		}
	}
	return(NULL);
}

// ----------------------------------------------------

unsigned long Inner_Calc_Perc_Val(unsigned long val, long perc)
{
	unsigned __int64 a;

	if (perc >= 100)
		return(val);
	a = (unsigned __int64)val * (unsigned __int64)perc / (unsigned __int64)100;
	return((unsigned long)a);
}

static void MapPlayer_RecalcAllDeployedSoldiers(CMapPlayer * pPlayer)
{
	long si;
	struct plrDATA_NPC * pNPCData;
	CMapNPC * pNPC;

	if (!pPlayer)
		return;
	pNPCData = pPlayer->GetNPCData();
	if (!pNPCData)
		return;
	for (si = 0; si < gameMAX_CARRY_SOLDIER_LIMIT; si++)
	{
		if (!pNPCData->plrNPCData[si].plrNPC_UID)
			continue;
		pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pNPCData->plrNPCData[si].plrNPC_UID, CMapNPC::CLASS_ID);
		if (pNPC)
		{
			::gameServer_CalcCharacterAttribute(pNPC->GetCharData());
			pNPC->UpdateNPCPart1(0x20);
		}
	}
}

static int MapPlayer_IsSoldierAttrSkill(unsigned short skillID)
{
	return skillID == magic_SOLDIER_ADD_HP || skillID == magic_SOLDIER_ADD_HP2
		|| skillID == magic_SOLDIER_ADD_ATK || skillID == magic_SOLDIER_ADD_ATK2
		|| skillID == magic_SOLDIER_ADD_DEF || skillID == magic_SOLDIER_ADD_DEF2;
}

// NPC Talk ��� Script �\���
// return: != 0 ���s���w talk id, -1 ���ݵ��G, -2 �������
long CMapPlayer::CB_NPCTalkCommand(long msg_id, long script_id, long * param_tbl)
{
	CMapPlayer * pPlayer = npcTalk_pPlayer;
	long i;
	long end_time;
	long id, num, step, bless,level,count,temp;
	//struct plrDATA tmpData;
	//struct itemDATA_SAVE nItemSave;
	//struct itemDATASHOWSELF nItemSF;
	unsigned long long	gold, total_gold;
	unsigned long		nExp;
	CMapCharMsg * pMsg;
	struct plrDATASHOWSELF Self;
	//
	struct plrMISSION_SAVE * pMIS;
	struct gameMISSION_DATA * pMissionTbl;
	struct plrDATA_SKILL * pSkillData;
	char tmpstr[1024];
	//
	CMapChar * pChar = NULL;
	struct plrDATA_SAVE * pSave;
	//
	int hLoginServer;
//	struct gameOFFICER_DATA * pOffice;
	//
	struct tm * ptm;
	struct tm ntm;
	//
	struct MAP_SET_REPAYMENT * pRepayData;
	unsigned long uid;
	CMapPlayer * pDest;
	struct itemDATA_SAVE * pItemSave;
	//
	struct VT_CARD_ASK_RECORD nAskVTCard;
	struct CARD_TO_ITEM_SETDATA * pGift;
	//
	long map_code,Copy_UID, x, y;
	struct itemDATA * pItemData;
	//struct plrCARRYITEM_SAVE carryItem;
	//
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	CMapNPC * pNPC;
	//
	RECT pos_rect;
	CMapCtrl * pMapCtrl;
//	struct ENTER_STAGE_RECORD * pEnterStage;
	unsigned long * pDayTimeRec;
	//
	struct MAP_CITY_DATA_PACK nCityPack;

	if (!pPlayer)
		return(0);
//	if(GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
//		return;
//	if (!pPlayer->IsNPCTalk())
//		return;

//CMapServer::GetServer()->Log("NPC talk call-back(%d)(%d, %d, %d, %d, %d)", script_id, param_tbl[0], param_tbl[1], param_tbl[2], param_tbl[3], param_tbl[4]);
	//
	switch(script_id)
	{
	case scMIS_SavePos:
		pPlayer->GetSaveData()->plrSavePosX = pPlayer->GetPosX();
		pPlayer->GetSaveData()->plrSavePosY = pPlayer->GetPosY();
		pPlayer->GetSaveData()->plrSaveMapCode = pPlayer->GetMapID();
		//pPlayer->SaveCharData(0);
		pPlayer->AutoSaveCharData();
		// ��s Client �� Save Map Code
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		break;
//	case scMIS_TakeItem:
//		break;
	case scMIS_GiveItem:	// [���Ѯɪ�tID][���~1][�ƶq1][���~2][�ƶq2][���~3][�ƶq3]	���F��G���Ѯɪ�ID���ܤ��൹�F��ɡA����s�����tID
		// ���������F��
	/*	memcpyT(&tmpData, pPlayer->GetCharData(), sizeof(tmpData));
		memcpyT(&carryItem, pPlayer->GetItemData(), sizeof(carryItem));
		total_gold = pPlayer->GetSaveData()->plrGold;
		for (i=0; i<3; i++)
		{
			id = param_tbl[i*2+1];
			num = param_tbl[i*2+2];
			if (id && (id != item_Money) && (id != item_Exp) && (id != item_SkillExp) && (num > 0))
			{
				if (::gameCreateItem(&nItemSF, id, CMapServer::GetServer()->GenerateItemUID(), num, pPlayer->GetUID(), pPlayer->GetName()))
				{
					gameServer_ItemSave_MakeShowSelf(&nItemSF, &nItemSave);
					if (InsertItemToPlayer(&nItemSave, num, &tmpData, &carryItem) != INSERT_ITEM_OK)
					{
						return(param_tbl[0]);
					}
				}
				else
				{
CMapServer::GetServer()->Log("(%s)�L�k�إߪ��~(%d,%d,%s)", pPlayer->GetName(), id, num);
					return(param_tbl[0]);
				}
			}
			else if (id == item_Money)
			{
				total_gold += (unsigned long)num;
				if (total_gold > gameMAX_GOLD)
					return(param_tbl[0]);		// �W�L������
			}
		}
	*/
		total_gold = pPlayer->GetSaveData()->plrGold;
		num = 0;
		for (i=0; i<3; i++)
		{
			if (id = param_tbl[i*2+1])
			{
				switch(id)
				{
				case item_Money:
					if (param_tbl[i*2+2] < 0)
						return(param_tbl[0]);
					total_gold += (unsigned long long)param_tbl[i*2+2];
					if (total_gold > gameMAX_GOLD)
						return(param_tbl[0]);		// �W�L������
					break;
				case item_Exp:
				case item_SkillExp:
					break;
				default:
					num++;
					break;
				}
			}
		}
		if (!pPlayer->CarryItem_CheckFreeSpace(num))
			return(param_tbl[0]);		// �W�L������
		//
		for (i=0; i<3; i++)
		{
			id = param_tbl[i*2+1];
			num = param_tbl[i*2+2];
			//
			if (id == item_Money)
			{
				gold = num;
				if (gold > 0)
				{
					total_gold = pPlayer->GetSaveData()->plrGold + gold;
					if (total_gold > gameMAX_GOLD)
						total_gold = gameMAX_GOLD;
					pPlayer->GetSaveData()->plrGold = total_gold;
					//
					pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
					
					pMsg->m_Msg.m_UpdateData2Msg.Reset();
					pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETGOLD, 0, gold, 1);
					pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GOLD, 0, pPlayer->GetUID(), total_gold);

					pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					// .... �O�� log ....
					if (pPlayer->m_nTalkNPC_UID && (pPlayer->m_nTalkNPC_UID != 0xffffffff))
						pChar = (CMapChar *)CMapServer::GetServer()->FindAndCheckCharExistByUID(pPlayer->m_nTalkNPC_UID, CMapChar::CLASS_ID);

					struct itemDATA_SAVE logItem;
					memset(&logItem, 0, sizeof(logItem));
					logItem.m_Item.itemCode = (unsigned short)item_Money;
					logItem.m_Item.itemGoldNumber = gold;
					CMapServer::GetServer()->SendLogMessage_Item(pPlayer, LOGTYPE_ITEM_FROM_NPC_GET, pChar, &logItem);
				}
			}
			else if (id ==  item_Exp)
			{
				nExp = num;
				pPlayer->AddExp(nExp, pPlayer);
				//
				wsprintf(tmpstr, MSG_ADD_EXP, nExp);
				pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
				//
				//pPlayer->SendPlayerGetExp(nExp, 0, 0, 0);
				pPlayer->UpdatePlayerDataPart1();
			}
			else if (id == item_SkillExp)
			{
				nExp = num;
				pPlayer->AddSkillExp(nExp);
				//
				pPlayer->UpdatePlayerDataPart1();
				wsprintf(tmpstr, MSG_ADD_SKILL_EXP, nExp);
				pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
				//
				//pPlayer->SendPlayerGetExp(0, nExp, 0, 0);
				pPlayer->UpdatePlayerDataPart1();
			}
			else
			{
				if (id && (num > 0))
				{
				//	if (pPlayer->MakeItem(id, num, LOGTYPE_ITEM_FROM_NPC_GET, pPlayer))
					if (pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_FROM_NPC_GET, 0, pPlayer))
					{
						pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
						
						pMsg->m_Msg.m_UpdateData2Msg.Reset();
						pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETITEM_SHOW_ON_MAINBOARD, (short)num, id, 0); // �ƥ� // item_id // flag

						pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					}
					else
						CMapServer::GetServer()->Log("(%s)�L�k�إߪ��~(%d,%d,%s)", pPlayer->GetName(), id, num);
				}
			}
		}
		pPlayer->SaveCharData();
		break;
	case scMIS_CheckQuest:		// [����ID][�S���ӱ���tID]	�ˬd�O�_�ӱ����ȡG���J�����ӱ��ɡA����S���ӱ���tID
		pMIS = pPlayer->GetMissionData();
		id = param_tbl[0];
		num = param_tbl[1];
		for (i=0; i<gameMAX_MISSION_ACCEPT; i++)
		{
			if (pMIS->pmData[i].pmdUID == id)	// �w�g�ӱ�
				return(0);
		}
		if (id < gameMAX_MISSION)
		{
			step = pMIS->pmStatus[id];
			if (step)
				return(0);
		}
		return(num);
		break;
	case scMIS_RunQuestStep:	//  [����ID][�B�J1��tID][�B�J2��tID]�K�K...[�B�J8��tID]	�̷Ӹӥ��Ȫ��B�J����s�����tID
		pMIS = pPlayer->GetMissionData();
		id = param_tbl[0];
		if (id < gameMAX_MISSION)
		{
			//num = param_tbl[1];
			for (i=0; i<gameMAX_MISSION_ACCEPT; i++)
			{
				if (pMIS->pmData[i].pmdUID == id)	// �w�g�ӱ�
				{
					step = pMIS->pmData[i].pmdStep;
					if ((step >= 1) && (step <= gameMAX_MISSION_STEP))
					{
						num = param_tbl[step];
//	CMapServer::GetServer()->Log("Mission(%d) step(%d), npc id(%d)", id, step, num);
						return(num);
					}
				}
			}
		}
		//return(0);
		CMapServer::GetServer()->Log("ERROR: NPC Talk(scMIS_RunQuestStep, %d, talk=%d)", id, msg_id);
		return(-2);					// �����~���_ 2006/12/01
		break;
	case scMIS_CheckQuestComplete:	// [����ID][���Ȧ��\��tID][���ȥ��Ѫ�tID]	�ˬd���Ȧ��\�Υ���
		pMIS = pPlayer->GetMissionData();
		id = param_tbl[0];
		if (id < gameMAX_MISSION)
		{
			step = pMIS->pmStatus[id];
			// �ץ� mission repeat �ܧ�]�w�ӨS������ repeat �����D
	/*		if (step)
			{
				pMissionTbl = gameGetMissionTablePtr(id);
				if (!pMissionTbl)
					goto mis_find_ok;
				if (!pMissionTbl->gmdRepeat)
					goto mis_find_ok;
				for (i=0; i<gameMAX_MISSION; i++)
				{
					if (!pMIS->pmRepeat[i].pmdUID)
						break;
					if (pMIS->pmRepeat[i].pmdUID == id)
						goto mis_find_ok;
				}
			}
			pMIS->pmStatus[id] = 0;
			step = 0;
	mis_find_ok:
	*/
			//
			if (step == gameMISSION_STATE_SUCCESS)				// ���\
			{
				num = param_tbl[1];
				//
report_repeat_time:
				// xxx�����ٶ�xx��xx���~��ӱ�
				for (i=0; i<gameMAX_MISSION; i++)
				{
					if (pMIS->pmRepeat[i].pmdUID == id)			// �w�]�w�L
					{
						x = pMIS->pmRepeat[i].pmdLeftTime - pPlayer->GetSaveData()->plrPlayTime;
						if (x < 1)
							x = 1;
						pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
						
						pMsg->m_Msg.m_UpdateData2Msg.Reset();
						pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_MISSION_TIME_NOTIFY, 0, id, x);
						
						pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
						break;
					}
				}
				//
				return(num);
			}
			else if (step == gameMISSION_STATE_FAIL)			// ����
			{
				num = param_tbl[2];
				//return(num);
				goto report_repeat_time;
			}
		}
		return(0);
		break;
	case scMIS_SetQuest:		//	[����ID][�ӱ����Ѯɪ�tID]	�ӱ�����
		pMIS = pPlayer->GetMissionData();		// ���G�S�ˬd���\���ѡA���W���P�_
		id = param_tbl[0];
		pMissionTbl = gameGetMissionTablePtr(id);
		if ((!pMissionTbl) || (id >= gameMAX_MISSION))
		{
CMapServer::GetServer()->Log("No Mission(%d) info", id);
			pPlayer->SendMsgToClientByID(24217);
			num = param_tbl[1];
			return(num);
		}
		//
		for (i=0; i<gameMAX_MISSION_ACCEPT; i++)
		{
			if (pMIS->pmData[i].pmdUID == id)	// �w�g�ӱ�
			{
				pPlayer->SendMsgToClientByID(24218);
				num = param_tbl[1];
				return(num);
			}
		}
		for (i=0; i<gameMAX_MISSION_ACCEPT; i++)
		{
			if (pMIS->pmData[i].pmdUID == 0)
			{
				pMIS->pmData[i].pmdUID = (unsigned short)id;
				// !!!! ���ȳ]�w
				pMIS->pmData[i].pmdStep = 1;
				pMIS->pmData[i].pmdEnemyID = (unsigned short)pMissionTbl->gmdStep[1].gmsdEnemy;
				//
				if (id < gameMAX_MISSION)
				{
					pMIS->pmStatus[id] = gameMISSION_STATE_NONE;
				}
				pPlayer->SendMissionDataToClient();
				pPlayer->SaveMissionData(0);
				return(0);
			}
		}
		pPlayer->SendMsgToClientByID(24219);
		num = param_tbl[1];
		return(num);
		break;
	case scMIS_SetQuestNextStep:	//	[����ID]	�U�@�B
		pMIS = pPlayer->GetMissionData();
		id = param_tbl[0];
		pMissionTbl = gameGetMissionTablePtr(id);
		if (pMissionTbl && (id < gameMAX_MISSION))
		{
			for (i=0; i<gameMAX_MISSION_ACCEPT; i++)
			{
				if (pMIS->pmData[i].pmdUID == id)	// �w�g�ӱ�
				{
					num = pMIS->pmData[i].pmdStep + 1;
					//if (num <= gameMAX_MISSION_STEP)
					if (num <= (gameMAX_MISSION_STEP+1))	// ���\�h 1��, ���O�L�@��
					{
						if (num > gameMAX_MISSION_STEP)
							CMapServer::GetServer()->Log("Warning: NPC talk step too large(%d, step = %d)", script_id, num );
						pMIS->pmData[i].pmdStep = (unsigned char)num;
						pMIS->pmData[i].pmdEnemyID = (unsigned short)pMissionTbl->gmdStep[num].gmsdEnemy;
						pPlayer->SendMissionDataToClient();
						pPlayer->SaveMissionData(0);
						return(0);
					}
					else
						CMapServer::GetServer()->Log("Error: NPC talk step too large(%d, step = %d)", script_id, num );
				}
			}
		}
		break;
	case scMIS_SetQuestComplete:	//	[����ID][1=���\, 2=����]	�]�w���ȧ������A
/*		pMIS = pPlayer->GetMissionData();
		id = param_tbl[0];
		if (id < gameMAX_MISSION)
		{
			pMissionTbl = gameGetMissionTablePtr(id);
			for (i=0; i<gameMAX_MISSION_ACCEPT; i++)
			{
				if (pMIS->pmData[i].pmdUID == id)	// �w�g�ӱ�
				{
					// ���e��
					//pMIS->pmData[i].pmdUID = 0;
					if (i < (gameMAX_MISSION_ACCEPT - 1))
					{
						size = ((gameMAX_MISSION_ACCEPT - 1) - i) * sizeof(struct plrMISSION_DETAIL);
						memcpyT(&pMIS->pmData[i], &pMIS->pmData[i+1], size);
					}
					pMIS->pmData[gameMAX_MISSION_ACCEPT-1].pmdUID = 0;
					//
					pMIS->pmStatus[id] = param_tbl[1];
					if (pMissionTbl)
					{
						if (pMissionTbl->gmdRepeat)
						{
						//	pMIS->pmRepeat[id] = pPlayer->GetSaveData()->plrPlayTime + pMissionTbl->gmdRepeat;
							pPlayer->SetMissionRepeat(id, pMissionTbl->gmdRepeat);
						}
					}
					return(0);
				}
			}
		}
*/
		id = param_tbl[0];
		num = param_tbl[1];
		pPlayer->SetMissionState(id, num);
		pPlayer->SendMissionDataToClient();
		pPlayer->SaveMissionData(0);
		return(0);
		break;
	case scMIS_TakeItem:		// [���~1][�ƶq1][���~2][�ƶq2][���~3][�ƶq3]	�����F��
		id = param_tbl[0];
		num = param_tbl[1];
		if (id)
			pPlayer->DelItemAndUpdateClient(id, num, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
		id = param_tbl[2];
		num = param_tbl[3];
		if (id)
			pPlayer->DelItemAndUpdateClient(id, num, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
		id = param_tbl[4];
		num = param_tbl[5];
		if (id)
			pPlayer->DelItemAndUpdateClient(id, num, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
		break;
	case scMIS_CheckItem:		// [�����ɪ�tID][���~1][�ƶq1][���~2][�ƶq2][���~3][�ƶq3]
		for (i=0; i<3; i++)
		{
			id = param_tbl[i*2 + 1];
			num = param_tbl[i*2 + 2];
			if (id)
			{
				switch(id)
				{
				case item_Money:
					if (pPlayer->GetGold() < (unsigned long long)num)
						return(param_tbl[0]);
					break;
				case item_Exp:
					return(param_tbl[0]);
					break;
				case item_SkillExp:
					if (pPlayer->GetSkillExp() < (unsigned long)num)
						return(param_tbl[0]);
					break;
				default:
					if (!pPlayer->IsItemEnough(id, num))
					{
						return(param_tbl[0]);
					}
					break;
				}
			}
		}
		break;
	case scMIS_MovePos:		// [point X] [point Y] [MAP Nomber]	���ʨ���ܬY�a�I
		map_code = param_tbl[2];
		Copy_UID = 0;
		if (!map_code)
		{
			map_code = pPlayer->GetMapCode();
			Copy_UID = pPlayer->GetCopyUID();
		}
		//pPlayer->ChangeMap(map_code, param_tbl[0], param_tbl[1]);
		CMapServer::GetServer()->ChangeSaveMap(pPlayer, map_code, param_tbl[0], param_tbl[1], 0, 0,Copy_UID);
		//pPlayer->ExitNPCTalk();
		break;
	case scMIS_CheckMoney:	// [�����ɪ�tID][gold]
		gold = param_tbl[1];
		if (pPlayer->GetSaveData()->plrGold < gold)
			return(param_tbl[0]);
		break;
	case scMIS_TakeMoney:	// [gold]
		gold = param_tbl[0];
//CMapServer::GetServer()->Log("NPC take gold: %d, %d", gold, pPlayer->GetSaveData()->plrGold);
		if (pPlayer->GetSaveData()->plrGold >= gold)
		{
			total_gold = pPlayer->GetSaveData()->plrGold - gold;
			if(total_gold > gameMAX_GOLD)
				total_gold = gameMAX_GOLD;
			pPlayer->GetSaveData()->plrGold = total_gold;
			//
			pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_LOSEGOLD, 0, gold);
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GOLD, 0, pPlayer->GetUID(), total_gold);
		
			pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			// �g log ...
			::SendGoldLog( gold, pPlayer, LOGTYPE_ITEM_FROM_NPC_LOSE, 0 ); //��
			//
			pPlayer->SaveCharData(0);
		}
		break;
	case scMIS_GiveSkill:	// [�ޯ�w�q]	�Ϩ���Ƿ|�ޯ�(����1)
		id = param_tbl[0];
		if (id <= gameMAX_SKILL_LEARN)
		{
			long target_level = param_tbl[1];
			struct magicDATA * pMagic = gameMagic_GetPointer(id, NULL);
			if (target_level <= 0)
				target_level = 1;	// 旧脚本不带第2参数时，保持原行为
			if (target_level > 255)
				target_level = 255;
			if (pMagic && pMagic->magicMaxLevel > 0 && target_level > pMagic->magicMaxLevel)
				target_level = pMagic->magicMaxLevel;

			pSkillData = pPlayer->GetSkillData();
			if (pSkillData->plrSkillLevelMax[id] < target_level)
			{
				pSkillData->plrSkillLevelMax[id] = (unsigned char)target_level;
				if (pSkillData->plrSkillLevel[id] < target_level)
					pSkillData->plrSkillLevel[id] = (unsigned char)target_level;
				//
				//pPlayer->UpdateSkillTable();
				//
				pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_LEARN_SKILL, 0, id);

				pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				//
				pPlayer->SaveSkillData();
				/* 部分客户端只靠 LEARN_SKILL 增量包不会立刻刷新技能栏，补发整张技能表。 */
				pPlayer->UpdateSkillTable();
				// 2008/07/07
				::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
				pPlayer->UpdateClientShowData(0);
				//
				if (id == magic_SOLDIER_ARRAY_ATTR)
					pPlayer->SetCarryNPCMatrix();
			}
		}
		break;
	case scMIS_CheckLevel:	//	[�p��ɸ��ܹ�ܽs��][�P�_��]	�P�_���ŬO�_�X�����
		id = param_tbl[1];
		if (pPlayer->GetSaveData()->plrLevel < id)
			return(param_tbl[0]);
		break;
	case scMIS_CheckAttribute:	//	[�p��ɸ��ܹ�ܽs��][�O�q][���O][����][�믫][���][�αs], 0 = ���P�_	�P�_�򥻤����ݩʬO�_�X�����
		if (id = param_tbl[1])
		{
			if (pPlayer->GetCharData()->plrAttrStr < id)
				return(param_tbl[0]);
		}
		if (id = param_tbl[2])
		{
			//if (pPlayer->GetCharData()->plrAttrInt < id)
			if (gameNPC_CalcLeader_GetIntVal(pPlayer->GetCharData()) < id)
				return(param_tbl[0]);
		}
		if (id = param_tbl[3])
		{
			if (pPlayer->GetCharData()->plrAttrDex < id)
				return(param_tbl[0]);
		}
		if (id = param_tbl[4])
		{
			if (pPlayer->GetCharData()->plrAttrMind < id)
				return(param_tbl[0]);
		}
		if (id = param_tbl[5])
		{
			if (pPlayer->GetCharData()->plrAttrCon < id)
				return(param_tbl[0]);
		}
		if (id = param_tbl[6])
		{
			if (pPlayer->GetCharData()->plrAttrLeader < id)
				return(param_tbl[0]);
		}
		break;
	case scMIS_CheckTeam:		//	[���Ѯɸ��ܹ�ܽs��][�ն��H��]
		id = param_tbl[1];
		if (pPlayer->PartyIsLeader())
		{
			if (pPlayer->PartyGetTotal() >= id)
				return(0);
		}
		return(param_tbl[0]);
		break;
	case scMIS_CheckSkill:		// [���Ѯɸ��ܹ�ܽs��][�֦��Y�ޯ�][����]
		id = param_tbl[1];
		num = param_tbl[2];
		pSkillData = pPlayer->GetSkillData();
		if (pSkillData->plrSkillLevelMax[id])
		{
			if (pSkillData->plrSkillLevelMax[id] >= num)
				return(0);
		}
		return(param_tbl[0]);
		break;
	case scMIS_CheckClass:		// [���Ѯɸ��ܹ�ܽs��][�O/�_���Y¾�~][�Y����]
		id = param_tbl[1];
		num = param_tbl[2];
	//	if (pPlayer->GetSaveData()->plrJobID == id)
	//	{
	//		if (pPlayer->GetSaveData()->plrLevel >= num)
	//			return(0);
	//	}
	//	return(param_tbl[0]);
		//
		if (id)	// ���]�w�~�P�_
		{
			if (pPlayer->GetSaveData()->plrJobID != id)
				return(param_tbl[0]);
		}
		if (num)
		{
			if (num < 0)
			{
				num = -num;
				if (pPlayer->GetSaveData()->plrLevel > num)
					return(param_tbl[0]);
			}
			else
			{
				if (pPlayer->GetSaveData()->plrLevel < num)
					return(param_tbl[0]);
			}
		}
		return(0);
		break;
	case scMIS_CheckForce:		// [���Ѯɸ��ܹ�ܽs��][�O/�_���Y��a]
		id = param_tbl[1];
		if (pPlayer->GetSaveData()->plrCountryID == id)
		{
			return(0);
		}
		return(param_tbl[0]);
		break;
	case scMIS_CheckSex:		// [�k�ɸ��ܹ�ܽs��]
		if (pPlayer->GetSaveData()->plrSex == sexMALE)
		{
			return(0);
		}
		return(param_tbl[0]);
		break;
	case scMIS_GiveDropItem:	// [���Ѯɸ��ܽs��][�������~���s��][gold min][gold max]
		id = param_tbl[1];
		//
		{
			long carry_max = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
			long free_slots = 0;
			struct plrCARRYITEM_SAVE * pCarry = pPlayer->GetItemData();
			if (pCarry && carry_max > 0)
			{
				for (i = 0; i < carry_max; i++)
				{
					if (!pCarry->plrCarryItem[i].m_Item.itemCode)
						free_slots++;
				}
			}
			if (free_slots > 0)
			{
				num = gameGetRandomRange(param_tbl[2], param_tbl[3] + 1);
				if (pPlayer->MissionGetDropItem(id, num))
				{
					return(0);
				}
				// 有空格但未抽到有效掉落（例如 Nothing）时，不应误提示“背包满”
				return(0);
			}
			else
			{
				wsprintf(tmpstr, gameGetResourceName(24273), 1);
				pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
			}
		}
		return(param_tbl[0]);
		break;
	case scMIS_GiveMeritPoint:	// GiveMeritPoint: plrHonor is unsigned long (see gameMAX_HONOR); do not cast to unsigned short
		step = param_tbl[0];
		if (step < 0)
			step = 0;
		{
			unsigned long curH = pPlayer->GetSaveData()->plrHonor;
			unsigned long newH = curH + (unsigned long)step;
			if (newH > (unsigned long)gameMAX_HONOR)
				newH = (unsigned long)gameMAX_HONOR;
			if (newH < curH)
				newH = (unsigned long)gameMAX_HONOR;
			pPlayer->GetSaveData()->plrHonor = newH;
			num = (long)newH;
		}
		pPlayer->AutoSaveCharData();
		//
		pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GET_HONOR, 0, step, num);
	
		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		break;
	case scMIS_CheckEquip:		// [�S���ɪ�tID][���~1][���~2][���~3][���~4][���~5]
		pSave = pPlayer->GetSaveData();
		for (num=1; num<=5; num++)
		{
			id = param_tbl[num];
			if (id)
			{
				if (pSave->plrEquipWeaponL.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipWeaponR.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipArmor.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipHead.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipFoot.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipOther1.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipOther2.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipUnderwear.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipGlove.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipP.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipHorse.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipWeaponL2.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipWeaponR2.m_Item.itemCode == id)
					return(0);
				//xavi�t�����
				if (pSave->plrEquipJade.m_Item.itemCode == id)
					return(0);
				if (pSave->plrEquipHideWeapon.m_Item.itemCode == id)
					return(0);
		//		if (pSave->plrEquipPet.m_Item.itemCode == id)
		//			return(0);
			}
		}
		//
		return(param_tbl[0]);
		break;
	case scMIS_ChangSavePoint:		// [point X] [point Y] [MAP Nomber]
		id = param_tbl[2];		// map
		i = param_tbl[0];		// x
		num = param_tbl[1];		// y
		pPlayer->GetSaveData()->plrSavePosX = i;
		pPlayer->GetSaveData()->plrSavePosY = num;
		pPlayer->GetSaveData()->plrSaveMapCode = (unsigned short)id;
		pPlayer->AutoSaveCharData();
		//
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		break;
	case scMIS_Chang_Force:			// [���y]
		i = param_tbl[0];
		if (i < 1)
		{
			CMapServer::GetServer()->Log("ERROR: NPC: Change force id = %d", i);
			i = 1;
		}
		if (i > 4)
		{
			CMapServer::GetServer()->Log("ERROR: NPC: Change force id = %d", i);
			i = 4;
		}
		//
		if (!pPlayer->GetSaveData()->plrOrganizeUID)	// 2006/05/02
		{
			// �O�_�ŦX�H�Ʊ���
			if (!CMapServer::GetServer()->CheckForcePeople(i))
			{
				pPlayer->SendMsgToClientByID(24274);
				return(-2);		// ���} NPC ���
			}
			// �q�� Login Server �ܧ���y
			struct LOGIN_CHANGE_FORCE nData;

			hLoginServer = CMapServer::GetServer()->GetLoginServer();
			nData.nUID = pPlayer->GetUID();
			nData.nOldCountryID = pPlayer->GetSaveData()->plrCountryID;
			nData.nCountryID = (unsigned char)i;
			CMapServer::GetServer()->SendData(hLoginServer, 0, PROTO_LOGIN_CHANGE_FORCE, (char *)&nData, sizeof(nData), 0);
			//
			pPlayer->GetSaveData()->plrCountryID = (unsigned char)i;
			pPlayer->GetShowData()->plrSetColor = (unsigned short)gameGetCountryColor(i);
			pPlayer->SaveCharData(0);
			// ��s�ۤv���
			::memset(&Self, 0, sizeof(plrDATASHOWSELF));
			::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
			::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
			//memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
			pPlayer->UpdateShowData();
		}
		break;
	case scMIS_CheckOffice:					// �P�_���a�x¾���šC
		id = param_tbl[1];
		if (pPlayer->GetSaveData()->plrOffice < id)
			return(param_tbl[0]);
		break;
	case scMIS_MoveSavePos:					// ���ʦܨ��⪺�����I�C
		//pPlayer->ChangeMap(pPlayer->GetSaveData()->plrSaveMapCode, pPlayer->GetSaveData()->plrSavePosX, pPlayer->GetSaveData()->plrSavePosY);
		CMapServer::GetServer()->ChangeSaveMap(pPlayer, pPlayer->GetSaveData()->plrSaveMapCode, pPlayer->GetSaveData()->plrSavePosX, pPlayer->GetSaveData()->plrSavePosY, 0);
		//pPlayer->ExitNPCTalk();
		break;
	case scMIS_CheckMeritPoint:				// [���Ѯɸ��ܽs��][�\����]	�P�_���a�\����
		num = param_tbl[1];
		i = pPlayer->GetSaveData()->plrHonor;
		if ((unsigned long)i < (unsigned long)num)
		{
			return(param_tbl[0]);
		}
		break;
	case scMIS_TakeMeritPoint:				// [�\����]	�Ѫ��a���W�����\����
		num = param_tbl[0];
		if (num < 0)
			num = 0;
		{
			unsigned long curH = pPlayer->GetSaveData()->plrHonor;
			unsigned long cost = (unsigned long)num;
			unsigned long newH = (curH >= cost) ? (curH - cost) : 0UL;
			pPlayer->GetSaveData()->plrHonor = newH;
		}
		//pPlayer->AutoSaveCharData();
		pPlayer->SaveCharData(0);
		//
		pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_HONOR, 0, pPlayer->GetSaveData()->plrHonor);
		
		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		break;
	case scMIS_CheckDayActionEx:			// [���Ѯɹ�ܽs��][���m��(0~23��)][���j�ɶ�(��)][���~ID, 0�N���������~���|�]�w�ɶ�][���~�ƶq][�ĴX��O���s��]
		pDayTimeRec = NULL;
		switch(param_tbl[5])
		{
		case 0:
		case 1:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General;
			break;
		case 2:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_2;
			break;
		case 3:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_3;
			break;
		case 4:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_4;
			break;
		case 5:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_5;
			break;
		case 6:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_6;
			break;
		case 7:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_7;
			break;
		case 8:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_8;
			break;
		case 9:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_9;
			break;
		case 10:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_a;
			break;
		case 11:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_11;
			break;
		case 12:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_12;
			break;
		case 13:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_13;
			break;
		case 14:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_14;
			break;
		case 15:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_15;
			break;
		case 16:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_16;
			break;
		case 17:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_17;
			break;
		case 18:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_18;
			break;
		case 19:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_19;
			break;
		case 20:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_20;
			break;
		case 21:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_21;
			break;
		case 22:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_22;
			break;
		case 23:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_23;
			break;
		case 24:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_24;
			break;
		case 25:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_25;
			break;
		case 26:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_26;
			break;
		case 27:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_27;
			break;
		case 28:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_28;
			break;
		case 29:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_29;
			break;
		case 30:
			pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General_30;
			break;
		}
		//
		if (pDayTimeRec)
			goto check_day_action;
		CMapServer::GetServer()->Log("ERROR: NPC talk day action id(%d)", param_tbl[5]);
		return(param_tbl[0]);
		break;
	case scMIS_ChackDayAction:				// [���Ѯɹ�ܽs��][���m��(0~23��)][���j�ɶ�(��)][���~ID, 0�N���������~���|�]�w�ɶ�][���~�ƶq]
		pDayTimeRec = &pPlayer->GetSaveData()->plrTime_General;
		//
check_day_action:
		i = CMapServer::GetServer()->GetLoopTime();
		if (i < (long)*pDayTimeRec)
			return(param_tbl[0]);
		//
		id = param_tbl[3];
		num = param_tbl[4];
		if (!id || !num)
		{
set_day_time:
			// �j�ѩw��
			ptm = ::apiGetTimeStruct(0);
			memcpyT(&ntm, ptm, sizeof(ntm));
			ntm.tm_hour = param_tbl[1];
			ntm.tm_min = 0;
			ntm.tm_sec = 0;
			if ((ntm.tm_hour > 23) || (ntm.tm_hour < 0))
			{
				CMapServer::GetServer()->Log("ERROR: NPC talk time(%d)", param_tbl[1]);
				ntm.tm_hour = 0;
			}
			i = (long)mktime(&ntm);
			step = param_tbl[2];
			if (!step)
				step++;
			if ((step < 1) || (step > 30))
			{
				CMapServer::GetServer()->Log("ERROR: NPC talk day(%d)", param_tbl[2]);
				step = 1;
			}
			i = i + (60 * 60 * 24) * step;
		//i = i + step * 10;
			if (i & 0x80000000)
				i = 0x7fffffff;
			*pDayTimeRec = i;
			//
//CMapServer::GetServer()->Log("NPC talk time(%d)(%d)", param_tbl[2], *pDayTimeRec);
			//
			pPlayer->SaveCharData(0);
		}
		else
		{
			if (!pPlayer->CarryItem_CheckFreeSpace(1))
				return(param_tbl[0]);		// �W�L������
			if (pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_FROM_NPC_GET, 0, pPlayer))
			{
				pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);

				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETITEM_SHOW_ON_MAINBOARD, (short)num, id, 0); // �ƥ� // item_id // flag
							
				pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				//
				goto set_day_time;
			}
			else
			{
				CMapServer::GetServer()->Log("(%s)�L�k�إߪ��~(%d,%d,%s)", pPlayer->GetName(), id, num);
				return(param_tbl[0]);
			}
		}
		break;
	case scMIS_AskVTData:		// [�S�����][���A��������][time out����]
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
		i = CMapServer::GetServer()->GetVTServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			id = param_tbl[2];
			if (id < 30)	// �ܤֵ� 30��
				id = 30;
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitVT_TalkID_NoData = param_tbl[0];
			pPlayer->m_WaitVT_TalkID_Error = param_tbl[1];
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			struct VT_ASK_RECORD nAskVT;

			memset(&nAskVT, 0, sizeof(nAskVT));
			nAskVT.nPlayerUID = pPlayer->GetUID();
			msg_strncpy(nAskVT.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nAskVT.szAccount));
			nAskVT.nVitemTypeFilter = 0xFF;
			CMapServer::GetServer()->SendData(i, 0, PROTO_VT_ASK_RECORD, (char *)&nAskVT, sizeof(nAskVT), 0);

//CMapServer::GetServer()->Log("Test: %s Ask VT record", pPlayer->GetSaveData()->plrName);

			//
			return(-1);		// wait next
		}
		else
			return(param_tbl[1]);
		break;
	case scMIS_AskVTDataByType:
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)
			return(0);
		i = CMapServer::GetServer()->GetVTServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			id = param_tbl[2];
			if (id < 30)
				id = 30;
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			pPlayer->m_WaitVT_TalkID_NoData = param_tbl[0];
			pPlayer->m_WaitVT_TalkID_Error = param_tbl[1];
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			struct VT_ASK_RECORD nAskVTByType;
			memset(&nAskVTByType, 0, sizeof(nAskVTByType));
			nAskVTByType.nPlayerUID = pPlayer->GetUID();
			msg_strncpy(nAskVTByType.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nAskVTByType.szAccount));
			nAskVTByType.nVitemTypeFilter = (unsigned char)param_tbl[3];
			CMapServer::GetServer()->SendData(i, 0, PROTO_VT_ASK_RECORD, (char *)&nAskVTByType, sizeof(nAskVTByType), 0);
			return(-1);
		}
		else
			return(param_tbl[1]);
		break;
	case scMIS_AskVTData_Card:		// [�S�����][���A��������][time out����]
#if (defined(ItemMode) && !defined(GBMode))
		num = 10;					// 10 ItemMode �ˬd����, 1 �ˬd����
#else
		num = 0;					// 0 �ˬd����, 1 �ˬd����
#endif
		memset(&nAskVTCard, 0, sizeof(nAskVTCard));
		goto do_card;
		break;
	case scMIS_UseVTData_CardPoint:	// [�S�����][���A��������][time out����]
		num = 4;					// 4 �����T��N��
		goto do_card2;
		break;
	case scMIS_UseVTData_Card:		// �x�ȡA[�S�����][���A��������][time out����]
		num = 1;					// 0 �ˬd����, 1 �ˬd����, 2 �{��, 3 �����]
do_card2:
		memset(&nAskVTCard, 0, sizeof(nAskVTCard));
		// �M��Ĥ@�i�d���
#ifdef USE_BIG_VTCARD_ITEM
 #ifdef ItemMode
		pItemSave = CMapServer::GetServer()->Card_FindFuncItem(pPlayer, gameITEM_ID_CARD, 2);		// type=2 350�Ȳ�
 #else
		pItemSave = CMapServer::GetServer()->Card_FindFuncItem(pPlayer, gameITEM_ID_CARD, 1);		// type=1 ��d
 #endif
#else
		pItemSave = CMapServer::GetServer()->CarryItem_FindFirstItem(pPlayer, gameITEM_ID_CARD);
#endif
		if (!pItemSave)
			return(param_tbl[0]);
		msg_strncpy(nAskVTCard.szCardNo, pItemSave->m_CardItem.itemCardNo, sizeof(nAskVTCard.szCardNo));
//CMapServer::GetServer()->Log("Use VT Card(%s,%s)", pPlayer->GetSaveData()->plrName, nAskVTCard.szCardNo);
		//
do_card:
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
		i = CMapServer::GetServer()->GetVTServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			id = param_tbl[2];
			if (id < 30)	// �ܤֵ� 30��
				id = 30;
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitVT_TalkID_NoData = param_tbl[0];
			pPlayer->m_WaitVT_TalkID_Error = param_tbl[1];
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			nAskVTCard.nMode = (unsigned char)num;
			nAskVTCard.nData = 0;
			nAskVTCard.nPlayerUID = pPlayer->GetUID();
			msg_strncpy(nAskVTCard.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nAskVTCard.szAccount));
			CMapServer::GetServer()->SendData(i, 0, PROTO_VT_CARD_ASK_RECORD, (char *)&nAskVTCard, sizeof(nAskVTCard), 0);

//CMapServer::GetServer()->Log("Test: %s Ask VT Card record", pPlayer->GetSaveData()->plrName);

			//
			return(-1);		// wait next
		}
		else
			return(param_tbl[1]);
		break;
	case scMIS_UseVTData_CardGift:	// [�S�����][���A��������][���ݮɶ�_����][§��ID]
		num = 3;					// 0 �ˬd����, 1 �ˬd����, 2 �{��, 3 �����]
		memset(&nAskVTCard, 0, sizeof(nAskVTCard));
		//// �M��Ĥ@�i�d���
		//id = 0;			// �T�w�b�Ĥ@���m
		//pItemSave = CMapServer::GetServer()->CarryItem_FindFirstItem(pPlayer, gameITEM_ID_CARD, &id, 1);
		// �M��Ĥ@�i�d���
		pItemSave = CMapServer::GetServer()->CarryItem_FindFirstItem(pPlayer, gameITEM_ID_CARD);
		if (!pItemSave)
			return(param_tbl[0]);
		// �ˬd�i�������~�]�w
		pGift = gameGetCardToItemPtr(param_tbl[3]);
		if (!pGift)
		{
no_gift_info:
			CMapServer::GetServer()->Log("ERROR: No gift info(%s,%d)", pPlayer->GetName(), param_tbl[3]);
			return(param_tbl[0]);
		}
		if (!pGift->ciItemCode)
			goto no_gift_info;
		if (!gameGetItemPtr(pGift->ciItemCode))
			goto no_gift_info;
		//
		msg_strncpy(nAskVTCard.szCardNo, pItemSave->m_CardItem.itemCardNo, sizeof(nAskVTCard.szCardNo));
//CMapServer::GetServer()->Log("Use VT Card Gift(%s,%s)", pPlayer->GetSaveData()->plrName, nAskVTCard.szCardNo);
		//
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
		i = CMapServer::GetServer()->GetVTServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			id = param_tbl[2];
			if (id < 30)	// �ܤֵ� 30��
				id = 30;
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitVT_TalkID_NoData = param_tbl[0];
			pPlayer->m_WaitVT_TalkID_Error = param_tbl[1];
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			nAskVTCard.nMode = (unsigned char)num;
			nAskVTCard.nData = param_tbl[3];
			nAskVTCard.nPlayerUID = pPlayer->GetUID();
			msg_strncpy(nAskVTCard.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nAskVTCard.szAccount));
			CMapServer::GetServer()->SendData(i, 0, PROTO_VT_CARD_ASK_RECORD, (char *)&nAskVTCard, sizeof(nAskVTCard), 0);

//CMapServer::GetServer()->Log("Test: %s Ask VT Card Gift record", pPlayer->GetSaveData()->plrName);

			//
			return(-1);		// wait next
		}
		else
			return(param_tbl[1]);
		break;
#ifdef USE_BIG_VTCARD_ITEM
	case scMIS_UseVTData_CardPoint2:	// [�S�����][���A��������][���ݮɶ�_����][����]
		num = 4;					// 4 �����T��N��
		//
		memset(&nAskVTCard, 0, sizeof(nAskVTCard));
		// �M��Ĥ@�i�d���
		pItemSave = CMapServer::GetServer()->Card_FindFuncItem(pPlayer, gameITEM_ID_CARD, param_tbl[3]);
		if (!pItemSave)
			return(param_tbl[0]);
		msg_strncpy(nAskVTCard.szCardNo, pItemSave->m_CardItem.itemCardNo, sizeof(nAskVTCard.szCardNo));
		//
		goto do_card;
		break;
	case scMIS_UsePoint_Get_VTCard:		// [�S�����][���A��������][���ݮɶ�_����][����]
		// (Map -> Login -> Account(���I) -> Login(���I���\�glog) -> VT(����p�G�����D�����I����) -> Login(�s�W���\�glog�A�_�h���I�glog) -> Map)
		//num = 5;					// 5 �����Ȳ�
		//memset(&nAskVTCard, 0, sizeof(nAskVTCard));
		// �M��Ĥ@�i�d���
		//pItemSave = CMapServer::GetServer()->Card_FindFuncItem(pPlayer, gameITEM_ID_CARD, param_tbl[3]);
		//if (!pItemSave)
		//	return(param_tbl[0]);
		//msg_strncpy(nAskVTCard.szCardNo, pItemSave->m_CardItem.itemCardNo, sizeof(nAskVTCard.szCardNo));
		//
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
		{
#ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG: Last command not finish(%d,%s)", pPlayer->GetUID(), pPlayer->GetName());
#endif
			return(0);
		}
		//
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_WEB_POINT_TO_VTCARD nWebPointToVTCard;

			id = param_tbl[2];
			if (id < 30)	// �ܤֵ� 30��
				id = 30;
			memset(&nWebPointToVTCard, 0, sizeof(nWebPointToVTCard));
			nWebPointToVTCard.nPlayerUID = pPlayer->GetUID();
			nWebPointToVTCard.nFuncID = (unsigned short)param_tbl[3];
			nWebPointToVTCard.nPoints = gameGetVTCardPointByFuncID(nWebPointToVTCard.nFuncID);
			if (!nWebPointToVTCard.nPoints)
			{
#ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG: Type error(%d,%s)(%d)", pPlayer->GetUID(), pPlayer->GetName(), nWebPointToVTCard.nFuncID);
#endif
				return(param_tbl[0]);
			}
			nWebPointToVTCard.nAccountID = pPlayer->GetSaveData()->plrAccountID;
			msg_strncpy(nWebPointToVTCard.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nWebPointToVTCard.szAccount));
			//
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitVT_TalkID_NoData = param_tbl[0];
			pPlayer->m_WaitVT_TalkID_Error = param_tbl[1];
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_WEB_POINT_TO_VTCARD, (char *)&nWebPointToVTCard, sizeof(nWebPointToVTCard), 0);

//CMapServer::GetServer()->Log("Test: %s Ask VT Card record", pPlayer->GetSaveData()->plrName);

			//
			return(-1);		// wait next
		}
		else
			return(param_tbl[1]);
		break;
#endif
	case scMIS_ChackSoldier:		// [���Ѯɸ��ܽs��][�P�_�Φ�]	�P�_����O�_���a�h�L�A0=����; 1=���~��; 2=�h�L��C
		id = 0;
		switch(param_tbl[1])
		{
		case 0:			//����
		default:
			id |= 1 | 2;
			break;
		case 1:			//���~��
			id |= 1;
			break;
		case 2:			//�h�L��
			id |= 2;
			break;
		}
		//
		if (id & 1)
		{
			if (pPlayer->CheckCarryNPC(1))
				break;
		}
		if (id & 2)
		{
			if (pPlayer->CheckCarryNPC(2))
				break;
		}
		return(param_tbl[0]);
		break;
	case scMIS_ResetAttr:			// [���Ѯɸ��ܽs��][�ݩʪ��N��]	[1:�O�q][2:���O][3:����][4:�믫][5:���][6:�αs]�C���e�n�P�_�S�w�����~�A�å��ݥ����ϥΪ̿�ܩҭn���ܪ��ݩ�(�覡���w)�C
		i = param_tbl[1];
		num = 0;
		id = 0;
		//
		if (i > 30)
		{
			i -= 30;
			num = 1;
			if (pPlayer->IsItemEnough(gameITEM_ID_RESET_ATTR_5, 1))
			{
				id = gameITEM_ID_RESET_ATTR_5;
			}
		}
		if (i > 20)
		{
			i -= 20;
			num = 1;
			if (pPlayer->IsItemEnough(gameITEM_ID_RESET_ATTR_4, 1))
			{
				id = gameITEM_ID_RESET_ATTR_4;
			}
		}
		else if (i > 10)
		{
			i -= 10;
			num = 1;
			if (pPlayer->IsItemEnough(gameITEM_ID_RESET_ATTR_3, 1))
			{
				id = gameITEM_ID_RESET_ATTR_3;
			}
		}
		else
		{
			if (pPlayer->IsItemEnough(gameITEM_ID_RESET_ATTR_2, 1))
			{
				id = gameITEM_ID_RESET_ATTR_2;
			}
			else if (pPlayer->IsItemEnough(gameITEM_ID_RESET_ATTR, 1))
			{
				id = gameITEM_ID_RESET_ATTR;
			}
		}
		//if (pPlayer->IsItemEnough(gameITEM_ID_RESET_ATTR, 1))
		if (id)
		{
			if (pPlayer->ResetAttribute(i, num))
			{
				//pPlayer->DelItemAndUpdateClient(gameITEM_ID_RESET_ATTR, 1, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
				pPlayer->DelItemAndUpdateClient(id, 1, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
				//
				::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
				// ��s self ���
				::memset(&Self, 0, sizeof(plrDATASHOWSELF));
				::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
				::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
				memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
				//
				pPlayer->SaveCharData();
				pPlayer->SaveItemData();
				break;
			}
		}
		return(param_tbl[0]);
		break;
	case scMIS_ResetAllAttr:		// [���Ѯɸ��ܽs��][�ݭn�����~][���~�ƶq]
		id = param_tbl[1];
		num = param_tbl[2];
		//
		if (pPlayer->IsItemEnough(id, num))
		{
			pPlayer->ResetAllAttribute();
			//
			pPlayer->DelItemAndUpdateClient(id, num, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
			//
			::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
			// ��s self ���
			::memset(&Self, 0, sizeof(plrDATASHOWSELF));
			::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
			::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
			memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
			//
			pPlayer->SaveCharData();
			pPlayer->SaveItemData();
			break;
		}
		return(param_tbl[0]);
		break;
	case scMIS_ChackGMTYPE:			// [���⤣�OGM�ɸ��ܽs��][�ݩʪ��N��]	�ݩʪ��N���G(0)�����B(1) �jGM �B(2)�pGM)
		id = pPlayer->GetCharData()->plrSPCFlag;
		switch(param_tbl[1])
		{
		case 0:
			if (id & (spcFLAG_GM | spcFLAG_GM2))
				goto ret001;
			break;
		case 1:
			if (id & spcFLAG_GM)
				goto ret001;
			break;
		case 2:
			if (id & spcFLAG_GM2)
				goto ret001;
			break;
		}
		return(param_tbl[0]);
		break;
	case scMIS_CheckHourse:			// [���W�S�����ܪ��s��][�������N��]	�ˬd�O�_�����F�����N���O�d�A�{��0�G�����ˬd, 1 ���~�A2 �˳�
		id = 0;
		switch(param_tbl[1])
		{
		case 0:			//����
		default:
			id |= 1 | 2;
			break;
		case 1:			//���~��
			id |= 1;
			break;
		case 2:			//�˳���
			id |= 2;
			break;
		}
		//
		pSave = pPlayer->GetSaveData();
		// �˳���O�_����
		if (id & 2)
		{
			if (pSave->plrEquipHorse.m_Item.itemCode)
				goto ret001;
		}
		// ���~��O�_����
		if (id & 1)
		{
			if (InnerCheckCarryItem_Horse(pPlayer->GetItemData(), pPlayer->GetSaveData()))
				goto ret001;
		}
		return(param_tbl[0]);
		break;
	case scMIS_GetRepay:			// ���v [�t�Τ��L���v���~��][�L�k���]
		pRepayData = CMapServer::GetServer()->GetRepayData();
		if (!pRepayData->nType)
		{
no_repay_001:
			return(param_tbl[0]);
		}
		//
		i = CMapServer::GetServer()->GetLoopTime();		// �ϥ�ID��01�}�l�C
		//
		if (i < pRepayData->nTimeBegin)
			goto no_repay_001;
		if (i > pRepayData->nTimeEnd)
			goto no_repay_001;
		if (i < (long)pPlayer->GetSaveData()->plrTime_Repay)	// �ɶ�����
			goto no_repay_001;
		// �T��s�إߨ�����
		if (pRepayData->nCharCreateTime)
		{
			if ((long)pPlayer->GetSaveData()->plrCreateTime > pRepayData->nCharCreateTime)
				goto no_repay_001;
		}
		//
		if (!pRepayData->nEveryDay)				// ���O�C�ѻ�
		{
			end_time = pRepayData->nTimeEnd + 2;
		}
		else
		{
			// �j�ѩw��
			ptm = ::apiGetTimeStruct(0);
			memcpyT(&ntm, ptm, sizeof(ntm));
			ntm.tm_hour = 0;
			ntm.tm_min = 0;
			ntm.tm_sec = 0;
			end_time = (long)mktime(&ntm);
			num = pRepayData->nEveryDay;
			if (num < 1)
				num = 1;
			if (num > 7)
				num = 7;
			end_time = end_time + (60 * 60 * 24) * num;
			//
			if (end_time > (pRepayData->nTimeEnd + 2))
				end_time = pRepayData->nTimeEnd + 2;
		}
		if (end_time & 0x80000000)
			end_time = 0x7fffffff;
		//
		switch(pRepayData->nType)
		{
		case 1:				// ���v�G�g��[����
			id = gameITEM_ID_EXP_DOUBLE;
			num = 1;
			//
give_repay_item_id:
//CMapServer::GetServer()->Log("NPC talk time(%d)", pPlayer->GetSaveData()->plrTime_General);
			//
			if (pPlayer->MakeItem(id, num, LOGTYPE_ITEM_REPAY_GET, pPlayer))
			{
				pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETITEM_SHOW_ON_MAINBOARD, (short)num, id, 0);

				pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				//
give_repay_item_ok:
				// �w��
				pPlayer->GetSaveData()->plrTime_Repay = end_time;
//CMapServer::GetServer()->Log("NPC talk time(%d)(%d)", param_tbl[2], pPlayer->GetSaveData()->plrTime_General);
				//
				pPlayer->SaveCharData(0);
			}
			else
			{
//make_item_err:
				switch(pPlayer->MakeItem_GetErrorCode())
				{
				case INSERT_ITEM_OVER_LOAD: //�L��
					pPlayer->SendMsgToClientByID( 20141 );
					break;
				case INSERT_ITEM_NO_SPACE: //�Ŷ�����
					pPlayer->SendMsgToClientByID( 20150 );
					break;
				}
				return(param_tbl[1]);
			}
			break;
		case 2:
			id = gameITEM_ID_SKILL_EXP_DOUBLE;
			num = 1;
			goto give_repay_item_id;
			break;
		case 3:
			id = gameITEM_ID_HONOR_DOUBLE;
			num = 1;
			goto give_repay_item_id;
			break;
		case 4:					// 7571		7���ӥO	�D��
			num = 3;			// �h�֪Ŷ�
			if (!pPlayer->CarryItem_CheckFreeSpace(num))
			{
				wsprintf(tmpstr, gameGetResourceName(24273), num);
				pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
				return(param_tbl[1]);
			}
			//
			PlayerDrop_Init();
			id = gameITEM_ID_TELEPORT_MARK;			// 5027	�a�I�аO�X     *3
			num = 3;
			if (pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_REPAY_GET, 1, pPlayer, 1))
			{
				char tmpstr2[1536];

				id = gameITEM_ID_TELEPORT;			// 5028	�аO�ǰe���b  *10
				num = 10;
				pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_REPAY_GET, 1, pPlayer, 1);
				//
				id = gameITEM_ID_SOLDIER_GOLD;		// ���l *10
				num = 10;
				pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_REPAY_GET, 1, pPlayer, 1);
				//
			/*	id = gameITEM_ID_EXP_DOUBLE;		// �S�O�g��ȥ[����
				num = 1;
				pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_REPAY_GET, 1, pPlayer, 1);
				//
				id = gameITEM_ID_SKILL_EXP_DOUBLE;	// �S�O�ޯ��I�[����
				num = 1;
				pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_REPAY_GET, 1, pPlayer, 1);
				//
				id = gameITEM_ID_CNPC_EXP_DOUBLE;	// �S�O�h�L�g��ȥ[����
				num = 1;
				pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_REPAY_GET, 1, pPlayer, 1);
			*/
				//
				PlayerDrop_SendResult(pPlayer, 0, 1, tmpstr);	// ��X���G��r��
				wsprintf(tmpstr2, "%s%s", gameGetResourceName(24234), tmpstr);
				pPlayer->SendMessage(gameChannel_System, NULL, tmpstr2);
				goto give_repay_item_ok;
			}
			return(param_tbl[1]);
			break;
		case 5:
			id = gameITEM_ID_BIG_UNDEAD;			// �j�j��
			num = 5;
			goto give_repay_item_id;
			break;
		case 6:
			id = gameITEM_ID_SPEC_CNPC_UNDEAD;		// �S�O�h�L�g��[����
			num = 1;
			goto give_repay_item_id;
			break;
		case 7:
			id = gameITEM_ID_GOODLUCK;				// ��B��
			num = 1;
			goto give_repay_item_id;
			break;
		case 10:			// ���v�G2005/06/15 0:0:0 �Z� (1118764800)
			pItemSave = InnerFindSoulTicket(pPlayer, 1118764800);
			if (!pItemSave)
				return(param_tbl[0]);
			// ���v: 100000��B�s�Τ�Z�x�^�~�B�\��100
			if (pPlayer->GetSaveData()->plrGold <= (gameMAX_GOLD - 100000))
			{
				if (pPlayer->GetSaveData()->plrHonor <= (gameMAX_HONOR - 100))
				{
					// ���R���ϥΪ��~�A�A�s�W��o���~
					struct itemDATA_SAVE nItemBak;

					memcpyT(&nItemBak, pItemSave, sizeof(nItemBak));
					::memset(pItemSave, 0, sizeof(struct itemDATA_SAVE));
					//
					id = pPlayer->GetSaveData()->plrJobID;
					//if ((id == jobID_WARLORD) || (id == jobID_LEADER))
					if (gameCheckJob_Warrior_Advisor(id) == JOB_TYPE_WARRIOR)
					{
						id = gameITEM_ID_REPAY_JOB1;	// �s�ΪZ�x�^�~
					}
					else
						id = gameITEM_ID_REPAY_JOB2;	// �s�Τ�x�^�~
					PlayerDrop_Init();
					if (pPlayer->CarryItem_MakeItemOnFreeSpace(id, 1, LOGTYPE_ITEM_REPAY_GET, 1, pPlayer, 1))
					{
						pPlayer->GetSaveData()->plrGold += 100000;
						pPlayer->GetSaveData()->plrHonor += 100;
						pPlayer->SendLog_GoldHonorExp(LOGTYPE_ITEM_REPAY_GET, item_Money, 100000, pPlayer);
						pPlayer->SendLog_GoldHonorExp(LOGTYPE_ITEM_REPAY_GET, item_Honor, 100, pPlayer);
						if (pPlayer->GetSaveData()->plrGold > gameMAX_GOLD)
							pPlayer->GetSaveData()->plrGold = gameMAX_GOLD;
						if (pPlayer->GetSaveData()->plrHonor > gameMAX_HONOR)
							pPlayer->GetSaveData()->plrHonor = gameMAX_HONOR;
						::PlayerDrop_SetGold(100000, pPlayer->GetSaveData()->plrGold);
						::PlayerDrop_SetHonor(100, pPlayer->GetSaveData()->plrHonor);
						//
						pPlayer->SaveCharData();
						PlayerDrop_SendMissionResult(pPlayer);
						//::gameServer_CalcCharacterAttribute(pPlayer);
						pPlayer->UpdateClientGoldAndWeight();
						//
					}
					else
					{
						::memcpyT(pItemSave, &nItemBak, sizeof(struct itemDATA_SAVE));
						return(param_tbl[1]);
					}
					pPlayer->SaveItemData();
				}
				else
					return(param_tbl[1]);
			}
			else
				return(param_tbl[1]);
			break;
		default:
			return(param_tbl[0]);
			break;
		}
		break;
	case scMIS_CheckChallenger:		// [�_�ɸ��ܽs��]	�ˬd���a�O�_�����ɪ�
		if (!CMapServer::GetServer()->Soul_IsChallenger(pPlayer, pPlayer->GetMapCode()))
			return(param_tbl[0]);
		break;
	case scMIS_MoevChallenger:		// ���ʰ��ɪ̦ܹ��������a�C
		CMapServer::GetServer()->Soul_ChallengerTeleport(pPlayer);
		break;
	case scMIS_CheckTeamLeader:		// [�_�ɸ��ܽs��]	�ˬd����O�_���ն������C
		if(!(pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_MAPSPACE))
			if (!pPlayer->PartyIsLeader())
			{
				return(param_tbl[0]);
			}
		break;
	case scMIS_MoveTeam:			// [�_�ɸ��ܽs��] [point X] [point Y] [MAP Nomber]	���ʦ����a�Ҳն���A�b���a�Ϥ���������ؼЦa�I�C
	{
		num = pPlayer->PartyGetTotal();
		if (num <= 1)
			return(param_tbl[0]);
		if (!pPlayer->IsReady() || !pPlayer->IsAvailable())
			return(param_tbl[0]);
		// �ˬd�O�_�b����
		for (i=0; i<num; i++)
		{
			uid = pPlayer->PartyGetMemberByPos(i);
			if (uid && (uid != pPlayer->GetUID()))
			{
				pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);
				if (!pDest)
					return(param_tbl[0]);
				if (!pDest->IsReady() || !pDest->IsAvailable() || pDest->IsDead())
					return(param_tbl[0]);
				if (!pPlayer->CheckInDistance(pDest->GetPosX(), pDest->GetPosY(), gameIconSize * 20))
					return(param_tbl[0]);
			}
		}
		// �ǰe����
		// If teleporting within the same map, preserve copyUID so virtual-space instances keep their shard.
		unsigned short target_copy_uid = 0;
		if ((unsigned short)param_tbl[3] == pPlayer->GetMapCode())
			target_copy_uid = (unsigned short)pPlayer->GetCopyUID();
		for (i=0; i<num; i++)
		{
			uid = pPlayer->PartyGetMemberByPos(i);
			if (uid && (uid != pPlayer->GetUID()))
			{
				pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);
				if (pDest)
				{
					if (target_copy_uid)
						CMapServer::GetServer()->ChangeSaveMap(pDest, param_tbl[3], param_tbl[1], param_tbl[2], 0, 0, target_copy_uid);
					else
						CMapServer::GetServer()->ChangeSaveMap(pDest, param_tbl[3], param_tbl[1], param_tbl[2]);
				}
			}
		}
		if (target_copy_uid)
			CMapServer::GetServer()->ChangeSaveMap(pPlayer, param_tbl[3], param_tbl[1], param_tbl[2], 0, 0, target_copy_uid);
		else
			CMapServer::GetServer()->ChangeSaveMap(pPlayer, param_tbl[3], param_tbl[1], param_tbl[2]);
		break;
	}
	case scMIS_ShipEnter:			// [�������~][�ɶ��|����F][�S���][��ɶ��]�w��ID]
		struct gameSHIPSCHEDULE_SET_DATA * pShipSet;

		pShipSet = gameGetShipScheduleSetPtr(param_tbl[3]);
		if (!pShipSet)
		{
			CMapServer::GetServer()->Log("ERROR: Ship schedule no data(%d) !", param_tbl[3]);
			return(param_tbl[0]);
		}
		// ��a�ϭn�b�@�_
		if (!CMapServer::GetServer()->GetMapManager()->FindMap(pShipSet->ssdShipBoardMapCode))
		{
			CMapServer::GetServer()->Log("ERROR: Ship schedule no board map ctrl (%d,%d) !", param_tbl[3], pShipSet->ssdShipBoardMapCode);
			return(param_tbl[0]);
		}
		if (!CMapServer::GetServer()->GetMapManager()->FindMap(pShipSet->ssdShipMapCode))
		{
			CMapServer::GetServer()->Log("ERROR: Ship schedule no map ctrl (%d,%d) !", param_tbl[3], pShipSet->ssdShipMapCode);
			return(param_tbl[0]);
		}
		// �
		if (!CMapServer::GetServer()->CarryItem_FindFirstItem(pPlayer, pShipSet->ssdTicketItemCode))
			return(param_tbl[2]);
		// �ɶ�
		if (num = CMapServer::GetServer()->m_nShipSchedule.ShipGetBoardInfo(pShipSet->ssdShipMapCode, &map_code, &x, &y, & id))
		{
			// �ɶ�����
			i = CMapServer::GetServer()->GetLoopTime();
			num -= i;
			if (num > 0)
			{
				pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_SHIP_START_TIME_REST, 0, num);

				pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			}
			return(param_tbl[1]);
		}
		// �q���}��Ѿl�ɶ�
		num = id;
		i = CMapServer::GetServer()->GetLoopTime();
		num -= i;
		if (num > 0)
		{
			pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_SHIP_START_TIME_REST, 0, num);

			pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}
		//
		pPlayer->ChangeMap(map_code, x, y);
		break;
	case scMIS_CheckGuild:		// [�_�ɸ��ܽs��][�x�Ϊ�][�x�ΤH��][�x�ε���]	�P�_���⪺�x�Ϊ��p�F[�x�Ϊ�] 1 ,���ݬ��x�Ϊ� / 0 , ���P�_�F[�x�ΤH��] 0, ���P�_�F�x�ε��� 0, ���P�_�C
		if (pPlayer->GetSaveData()->plrOrganizeUID)
		{
			if (param_tbl[1])					// �n�x�Ϊ�
			{
				if (pPlayer->GetSaveData()->plrOrganizeLeaderUID != pPlayer->GetSaveData()->plrUID)
					return(param_tbl[0]);
			}
			if (param_tbl[2] || param_tbl[3])	// �ݭn�θ��
			{
				i = CMapServer::GetServer()->GetLoginServer();
				if ( CMapServer::GetServer()->IsConnectedToServer(i) )
				{
					struct LOGIN_NPCTALK_PACK_DATA nTalkPackData;

					id = 30;	// �� 30��
					pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
					pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
					//
					pPlayer->m_WaitVT_TalkID_NoData = param_tbl[0];
					pPlayer->m_WaitVT_TalkID_Error = param_tbl[0];
					pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
					// �e�X�n�D
					memset(&nTalkPackData, 0, sizeof(nTalkPackData));
					nTalkPackData.nType = NPCTALK_PACK_TYPE_ORGDATA1;
					nTalkPackData.nData1 = param_tbl[3];
					nTalkPackData.nData2 = param_tbl[2];
					nTalkPackData.nData3 = pPlayer->GetSaveData()->plrOrganizeUID;
					nTalkPackData.player_uid = pPlayer->GetUID();
					CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackData, sizeof(nTalkPackData), 0);

		//CMapServer::GetServer()->Log("Test: %s Ask login npc talk data", pPlayer->GetName());

					//
					return(-1);		// wait next
				}
				// !!!
			}
			else
				return(0);
		}
		return(param_tbl[0]);
		break;
	case scMIS_HistoryRegister:			// [�������~�s��][�w�g���W�s��][�W�B�w���s��][�j�פ�����][�Գ��a��][�}��]
		{
			// Some scripts pass enter_map here; normalize to battle_map if needed.
			long battle_map = param_tbl[4];
			long b2 = ::gameGetHistoryBattleMapByEnterMap(battle_map);
			if (b2)
				battle_map = b2;
			id = CMapServer::GetServer()->m_nHistoryManager.PlayerRegister(pPlayer, battle_map, param_tbl[5]);
		}
		switch(id)
		{
		case -1:
			return(param_tbl[0]);
			break;
		case 0:
			return(param_tbl[1]);
			break;
		case 1:
			break;
		case 2:
			return(param_tbl[2]);
			break;
		case 3:			// �j�פ�����
			return(param_tbl[3]);
			break;
		}
		break;
	case scMIS_CheckRegister: // legacy scMIS_CheckRegister [err_talk][already_reg_talk][closed_talk][battle_map]
		{
			// This legacy command is expected to query registration status via LoginServer (KS flow),
			// not by local HistoryMgr maps (which may be world-only / different ids).
			//
			// Params:
			//  [0]=inner_error_talk
			//  [1]=no_register_talk (or "need to register")
			//  [2]=full/closed_talk (or other failure)
			//  [3]=battle_map(or enter_map) id used by LoginServer check
			if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// previous request still pending
				return(0);

			i = CMapServer::GetServer()->GetLoginServer();
			if (CMapServer::GetServer()->IsConnectedToServer(i))
			{
				struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;

				id = 300;	// wait timeout seconds
				pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
				pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);

				pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
				pPlayer->m_WaitKS_TalkID_NoReg = param_tbl[1];
				pPlayer->m_WaitKS_TalkID_Full = param_tbl[2];
				pPlayer->m_WaitKS_TalkID_NoBalance = 0;
				pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;

				memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
				nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_KS_CHECK_REGISTER;
				nTalkPackDataKS.nData1 = param_tbl[3];	// map id
				nTalkPackDataKS.nData2 = 0;
				nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
				nTalkPackDataKS.nData3 = 0;
				nTalkPackDataKS.player_uid = pPlayer->GetUID();
				msg_strncpy(nTalkPackDataKS.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nTalkPackDataKS.szAccount));

				CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, nTalkPackDataKS.GetSmartSize(), 0);
				return(-1);		// wait next
			}
			return(param_tbl[0]);
		}
		break;
	case scMIS_HistoryEnterBattle:		// [�������~�s��][�S�����W�s��][�Գ��a��]
		{
			long battle_map = param_tbl[2];
			long b2 = ::gameGetHistoryBattleMapByEnterMap(battle_map);
			if (b2)
				battle_map = b2;
		// �ɶ��O�_����
		if (!CMapServer::GetServer()->m_nHistoryManager.IsEnterHistoryBattleMap(pPlayer, battle_map))
		{
			return(param_tbl[0]);
		}
		//
		id = CMapServer::GetServer()->m_nHistoryManager.GetHistoryPos(pPlayer, battle_map, &num, &step);
//CMapServer::GetServer()->Log("HISTORY: %s Enter map 3(%d)", pPlayer->GetName(), id);
		switch(id)
		{
		case -1:	// �������~
#ifndef USE_PACKAGE_DATA
CMapServer::GetServer()->Log("HISTORY: %s enter battle -1", pPlayer->GetName());
#endif
			return(param_tbl[0]);
			break;
		case 0:		// �S���W
#ifndef USE_PACKAGE_DATA
CMapServer::GetServer()->Log("HISTORY: %s enter battle 0", pPlayer->GetName());
#endif
			return(param_tbl[1]);
			break;
		case 1:		// ���\
#ifndef USE_PACKAGE_DATA
CMapServer::GetServer()->Log("HISTORY: %s enter battle map %d(%d,%d)", pPlayer->GetName(), battle_map, num, step);
#endif
			//CMapServer::GetServer()->m_nHistoryManager.SetPlayerEnterInfo(pPlayer, param_tbl[2]);
			pPlayer->ChangeMap(battle_map, num, step);
			break;
		}
		}
		break;
	case scMIS_HistoryGetInfo:			// [�S�����][battle_map]
		{
			long battle_map = param_tbl[1];
			long b2 = ::gameGetHistoryBattleMapByEnterMap(battle_map);
			if (b2)
				battle_map = b2;
			if (!CMapServer::GetServer()->m_nHistoryManager.GetHistoryInfo(pPlayer, battle_map))
			return(param_tbl[0]);
		}
		break;
	case scMIS_UesItem:					// scMIS_UseItem [�S�����~][�ϥΥ���][�ϥΪ��~][�ϥμƶq]
		if (!pPlayer->IsItemEnough(param_tbl[2], param_tbl[3]))
			return(param_tbl[0]);
		//
		switch(param_tbl[2])			// �ϥίS�O���~
		{
		case gameITEM_ID_EXP_6POWER:
		case gameITEM_ID_EXP_POWER_01:
		case gameITEM_ID_EXP_POWER_02:
		case gameITEM_ID_EXP_POWER_03:
		case gameITEM_ID_EXP_POWER_04:
		case gameITEM_ID_EXP_POWER_05:
		case gameITEM_ID_EXP_POWER_06:
		case gameITEM_ID_EXP_POWER_07:
		case gameITEM_ID_EXP_POWER_08:
#ifndef USE_DOUBLE_ITEM_TIME_ADD
			if (pPlayer->GetSpecialStatus_AddExp(1))
			{
				pPlayer->SendMsgToClientByID(24152);
				return(param_tbl[1]);
			}
#endif
			//
			pItemData = ::gameGetItemPtr(param_tbl[2]);
			if (!pItemData)
				return(param_tbl[1]);
			//
			id = pItemData->itemUseMagicTime;
			if (!id)
				id = SPECIAL_STATUS_TIME_ADD_EXP;
			if (pPlayer->SetSpecialStatus(id, UPART2_TYPE_STATUS_ADD_EXP, pItemData->itemUseMagicLevel))
			{
				pPlayer->DelItemAndUpdateClient(param_tbl[2], param_tbl[3], LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
			}
			else
			{
#ifndef USE_PACKAGE_DATA
CMapServer::GetServer()->Log("Error: NPC talk use item(%d)(%d,%d)", param_tbl[2], id, pItemData->itemUseMagicLevel);
#endif
#ifdef USE_DOUBLE_ITEM_TIME_ADD
				pPlayer->SendMsgToClientByID(24152);
#endif
				return(param_tbl[1]);
			}
			break;
		default:
#ifndef USE_PACKAGE_DATA
CMapServer::GetServer()->Log("Error: NPC talk use item(%d)", param_tbl[2] );
#endif
			return(param_tbl[1]);
			break;
		}
		break;
	case scMIS_AttributePoint_Reset:
#ifdef HinetGame2006
		InnerResetAttribute_Player(pPlayer);
#else
		pPlayer->ResetAllAttribute();
		::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
		// ��s self ���
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char*)&Self, sizeof(struct plrDATASHOWSELF), 0);
		memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
		//
		pPlayer->SaveCharData();
		pPlayer->SaveItemData();
		break;
#endif
		break;
	case scMIS_EnterMapSpaceMinPlayer:	// [���~(���t�ΰT��)][���\(�ǳƶǰe)][�̧C����H��][����̦h�H��]
		id = CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_FindEnterMap(pPlayer, &pos_rect, param_tbl[2], param_tbl[3]);
		if ((id == -2) || (id == -3))
		{
			num = param_tbl[2];
			step = param_tbl[3];
			if (!num)
			{
				num = gameMAX_PARTY_PLAYER - 1;
				step = gameMAX_PARTY_PLAYER;
			}
			// �A�����O�����åB�պ�n�ӤH����������I
			wsprintf(tmpstr, gameGetResourceName(21340), num, step);
			pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
			return(param_tbl[0]);
		}
		//
		goto do_mapspace_player;
		break;
	case scMIS_EnterMapSpace:			// [���~(���t�ΰT��)][���\(�ǳƶǰe)]
		id = CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_FindEnterMap(pPlayer, &pos_rect);
		// >0 �a�Ͻs��, 0 �]�w���~, -1 �L�Ŷ�(�w��), -2 �����պ����ζ���, -3 �ն��Φ�����������, -4 �������H����i�J�ƥ��A�άO���b�i�J�a��(�w�o�X�T��)
do_mapspace_player:
		switch(id)
		{
		case 0:
			CMapServer::GetServer()->Log("Error: Map space setting error(%s,%d)", pPlayer->GetName(), pPlayer->GetMapCode() );
			break;
		case -1:		// �ثe�S���i�ϥΰƥ��Ŷ��A�еy��A�աI
			pPlayer->SendMsgToClientByID(24397);
			break;
		case -2:		// �A�����O�����åB�պ�6�ӤH����������I
		case -3:
			pPlayer->SendMsgToClientByID(24398);
			break;
		case -4:
			break;
		default:
			if (id > 0)
			{
				long target_map = id;
				unsigned short target_copy_uid = 0;
				struct MAPSPACE_DATA * pSet = gameGetMapSpacePtrbyMapID(id);
				// Normal enter + virtual room dispatch:
				// If configured, move party into copy_map_code's copy rooms so base room can stay available.
				if (pSet && pSet->msdCopyMapCode)
				{
					// 进入普通副本后，给当前队伍登记虚拟房间 RunMode（副本计时/回收逻辑依赖它）
					// 注意：MapSpace_FindEnterMapCopy 的返回是 copyUID；普通模式这里我们手动分配 copyUID 后启动 RunMode
					CMapManager * pMgr = CMapServer::GetServer()->GetMapManager();
					CMapCtrl * pCopyCtrl = NULL;
					std::map<int,CMapCtrl *>::iterator itCopy;
					std::map<long,queue<CMapCtrl*>>::iterator itIdle;
					long existCopyTotal = 0;
					long copyMapCode = pSet->msdCopyMapCode;

					if (!pMgr || !copyMapCode)
						return(param_tbl[0]);

					// count existing copy instances
					for (itCopy = pMgr->m_mapCopy.begin(); itCopy != pMgr->m_mapCopy.end(); ++itCopy)
					{
						if (itCopy->second && itCopy->second->GetID() == copyMapCode)
							existCopyTotal++;
					}

					// pre-create to map_num_min
					if (pSet->msdMapNumMin > 0 && existCopyTotal < pSet->msdMapNumMin)
					{
						long need = (long)pSet->msdMapNumMin - existCopyTotal;
						if (pSet->msdMapNumMax > 0 && existCopyTotal + need > pSet->msdMapNumMax)
							need = pSet->msdMapNumMax - existCopyTotal;
						if (need > 0)
							pMgr->CreateMapCopy(copyMapCode, (unsigned short)need, CMapSpace::MapSpace_OnCreateCopy);
					}

					// enforce map_num_max
					if (pSet->msdMapNumMax > 0)
					{
						existCopyTotal = 0;
						for (itCopy = pMgr->m_mapCopy.begin(); itCopy != pMgr->m_mapCopy.end(); ++itCopy)
						{
							if (itCopy->second && itCopy->second->GetID() == copyMapCode)
								existCopyTotal++;
						}
						if (existCopyTotal >= pSet->msdMapNumMax)
						{
							itIdle = pMgr->m_mapIdle.find(copyMapCode);
							if (itIdle == pMgr->m_mapIdle.end() || itIdle->second.size() == 0)
							{
								pPlayer->SendMsgToClientByID(24397);
								return(param_tbl[0]);
							}
						}
					}

					pCopyCtrl = pMgr->GetMapCopy(copyMapCode, CMapSpace::MapSpace_OnCreateCopy);
					if (!pCopyCtrl)
					{
						pPlayer->SendMsgToClientByID(24397);
						return(param_tbl[0]);
					}
					target_map = copyMapCode;
					target_copy_uid = pCopyCtrl->GetCopyUID();

					// Build party member uid array for MapSpace run-mode start.
					unsigned long member_uid[gameMAX_PARTY_PLAYER];
					::memset(member_uid, 0, sizeof(member_uid));
					num = pPlayer->PartyGetTotal();
					for (i=0; i<num && i<gameMAX_PARTY_PLAYER; i++)
						member_uid[i] = pPlayer->PartyGetMemberByPos(i);

					CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SetRunModeStart(copyMapCode, member_uid, num, target_copy_uid);
					// 普通入口只做校验；真正承载在虚拟房间，因此立即释放普通房间的 WAIT_RESULT 锁
					CMapServer::GetServer()->GetMapSpaceManager()->NotifyReadyStatus(id, 1, 0);
				}

				// �ǰe����
				num = pPlayer->PartyGetTotal();
				for (i=0; i<num; i++)
				{
					uid = pPlayer->PartyGetMemberByPos(i);
					if (uid && (uid != pPlayer->GetUID()))
					{
						pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);
						if (pDest)
						{
							GetRandomPos(&pos_rect, &x, &y);
							//
							CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SetPlayerDueTime(pPlayer->GetMapCode(), uid);
							if (target_copy_uid)
								CMapServer::GetServer()->ChangeSaveMap(pDest, target_map, x, y, 0, 0, target_copy_uid);
							else
								CMapServer::GetServer()->ChangeSaveMap(pDest, target_map, x, y);
						}
					}
				}
				GetRandomPos(&pos_rect, &x, &y);
				CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SetPlayerDueTime(pPlayer->GetMapCode(), pPlayer->GetUID());
				if (target_copy_uid)
					CMapServer::GetServer()->ChangeSaveMap(pPlayer, target_map, x, y, 0, 0, target_copy_uid);
				else
					CMapServer::GetServer()->ChangeSaveMap(pPlayer, target_map, x, y);
				//
				return(param_tbl[1]);
			}
			break;
		}
		return(param_tbl[0]);
		break;
	case scMIS_EnterMapSpaceCopyMinPlayer:	// [���~(���t�ΰT��)][���\(�ǳƶǰe)][�̧C����H��][����̦h�H��]
		// 必须走普通模式进入，再由普通入口分配虚拟房间
		id = CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_FindEnterMap(pPlayer, &pos_rect, param_tbl[2], param_tbl[3]);
		if ((id == -2) || (id == -3))
		{
			num = param_tbl[2];
			step = param_tbl[3];
			if (!num)
			{
				num = gameMAX_PARTY_PLAYER - 1;
				step = gameMAX_PARTY_PLAYER;
			}
			// �A�����O�����åB�պ�n�ӤH����������I
			wsprintf(tmpstr, gameGetResourceName(21340), num, step);
			pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
			return(param_tbl[0]);
		}
		goto do_mapspace_player;
		break;
	case scMIS_EnterMapSpaceCopy:			// [���~(���t�ΰT��)][���\(�ǳƶǰe)]
		// 必须走普通模式进入，再由普通入口分配虚拟房间
		id = CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_FindEnterMap(pPlayer, &pos_rect);
		goto do_mapspace_player;
		break;
	case scMIS_SetMapSpaceTeleport:		//	7	[���~][���\][map code][x][y][type][sec]	�]�w�ƥ��ǰe���A, map code=0���ܲM��,type=1���ܶ���]�w
		id = CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SetPlayerTeleportData(
			pPlayer, param_tbl[5], param_tbl[2], param_tbl[3], param_tbl[4], param_tbl[6], pPlayer->GetCopyUID());
		if (id)
			return(param_tbl[1]);
		return(param_tbl[0]);
		break;
	case scMIS_CheckEnterStageTime:		//	4	[����L�ت��ɶ�][�i�H�ϥζi�J][�ɶ����i�H�����i�J][map code]
		// 24480 �ثe�Ӧa�I�����a %d �H!
		map_code = param_tbl[3];
		pMapCtrl = CMapServer::GetServer()->FindMap(map_code);
		if (pMapCtrl)
		{
			wsprintf(tmpstr, gameGetResourceName(24480), pMapCtrl->GetPlayerCount());
			pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
		}
 #ifndef USE_PACKAGE_DATA
		else
			CMapServer::GetServer()->Log("ERROR: Enter stage map not exist(%d)", map_code);
 #endif
		//
#if (PAYMAP_SETTING_TYPE == 1)
#elif (PAYMAP_SETTING_TYPE == 2)
#else
		gamePayStageAddEnterStageTable(map_code, 0, 0, 0);
#endif
		//
 		if (pPlayer->GetSaveData()->plrEnterStageTime <= 0)
		{
			return(param_tbl[1]);
		}
		else
		{
			map_code = param_tbl[3];
			if (map_code == pPlayer->GetSaveData()->plrEnterStageID)
				return(param_tbl[2]);
		}
		//
		pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		if (pMsg)
		{
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ENTER_STAGE_NOTIFY, 0, pPlayer->GetSaveData()->plrEnterStageID, pPlayer->GetSaveData()->plrEnterStageTime); // data1=stage id, data2=time

			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}
		return(param_tbl[0]);
		break;
	case scMIS_SetEnterStageTime:		// 4	[���~][���\][map code][item code]
		id = param_tbl[3];
		if (pPlayer->FindItemAndDelete(id))
		{
			pItemData = ::gameGetItemPtr(id);
			if (!pItemData)
				return(param_tbl[0]);
			//
			id = param_tbl[2];
			i = pItemData->itemUseMagicTime;

			pPlayer->GetSaveData()->plrEnterStageTime = i;
			pPlayer->GetSaveData()->plrEnterStageID = (unsigned short)id;
			pPlayer->SaveCharData();
			//
			pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			if (pMsg)
			{
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ENTER_STAGE_NOTIFY, 0, pPlayer->GetSaveData()->plrEnterStageID, pPlayer->GetSaveData()->plrEnterStageTime); // data1=stage id, data2=time

				pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			}
			//
			pPlayer->SendSpecialTimeTable();
			//
 #ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("DEBUG: Set enter stage time(%s,%d,%d)", pPlayer->GetName(), id, i);
 #endif
			return(param_tbl[1]);
		}
		return(param_tbl[0]);
		break;
	case scMIS_GetEnterStageInfo:				// [map code1][map code2][map code3][map code4]
		// 24480 �ثe�Ӧa�I�����a %d �H!
		for (id=0; id<4; id++)
		{
			map_code = param_tbl[id];
			pMapCtrl = CMapServer::GetServer()->FindMap(map_code);
			if (pMapCtrl)
			{
				wsprintf(tmpstr, "%s ", gameGetResourceName(gameStageGetStageNameID(map_code)));
				wsprintf(tmpstr+strlen(tmpstr), gameGetResourceName(24480), pMapCtrl->GetPlayerCount());
				pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
			}
 #ifndef USE_PACKAGE_DATA
			else
				CMapServer::GetServer()->Log("ERROR: Enter stage map not exist(%d)", map_code);
 #endif
		}
#if (PAYMAP_SETTING_TYPE == 1)
#elif (PAYMAP_SETTING_TYPE == 2)
#else
		gamePayStageAddEnterStageTable(param_tbl[0], param_tbl[1], param_tbl[2], param_tbl[3]);
#endif
		break;
	case scMIS_CheckEnterStageTime4:			// [����L�ت��ɶ�][�i�H�ϥζi�J][�ɶ����i�H�����i�J][map code][map code2][map code3][map code4]
		if (pPlayer->GetSaveData()->plrEnterStageTime <= 0)
		{
			return(param_tbl[1]);
		}
		else
		{
			map_code = param_tbl[3];
			if (map_code == pPlayer->GetSaveData()->plrEnterStageID)
				return(param_tbl[2]);
		}
		return(param_tbl[0]);
		break;
	case scMIS_SetEnterStageTime4:				// [���~][���\][map code][item code][map code2][map code3][map code4]
		id = param_tbl[3];
		if (pPlayer->FindItemAndDelete(id))
		{
			pItemData = ::gameGetItemPtr(id);
			if (!pItemData)
				return(param_tbl[0]);
			//
			id = param_tbl[2];
			i = pItemData->itemUseMagicTime;

			pPlayer->GetSaveData()->plrEnterStageTime = i;
			pPlayer->GetSaveData()->plrEnterStageID = (unsigned short)id;
			pPlayer->SaveCharData();
			//
			pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			if (pMsg)
			{
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ENTER_STAGE_NOTIFY, 0, pPlayer->GetSaveData()->plrEnterStageID, pPlayer->GetSaveData()->plrEnterStageTime); // data1=stage id, data2=time

				pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			}
			//
			pPlayer->SendSpecialTimeTable();
			//
 #ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("DEBUG: Set enter stage time(%s,%d,%d)", pPlayer->GetName(), id, i);
 #endif
			return(param_tbl[1]);
		}
		break;
	//
	case scMIS_SetSkillLevel:			// [特殊學習失敗訊息][技能編號][等級][同步當前等級:0/1]
		id = param_tbl[1];
		if (id <= gameMAX_SKILL_LEARN)
		{
			pSkillData = pPlayer->GetSkillData();
			if (pSkillData->plrSkillLevelMax[id] != 0)
			{
				struct magicDATA * pMagicSetLv;
				long cap;

				i = param_tbl[2];
				if (i < 0)
					i = 0;
				if (MapPlayer_IsSoldierAttrSkill((unsigned short)id))
				{
					/* 士兵体/攻/防及进阶：NPC 脚本可设到 5 级，勿用 magicMaxLevel(进阶默认为1) 截断 */
					if (i > gameMAX_SKILL_LEVEL_EXTRA)
						i = gameMAX_SKILL_LEVEL_EXTRA;
				}
				else
				{
					cap = 15;
					if (pMagicSetLv = gameMagic_GetPointer(id, NULL))
					{
						if (pMagicSetLv->magicMaxLevel > 0 && pMagicSetLv->magicMaxLevel < cap)
							cap = pMagicSetLv->magicMaxLevel;
					}
					if (i > cap)
						i = cap;
				}
				pSkillData->plrSkillLevelMax[id] = (unsigned char)i;
				if (param_tbl[3])
					pSkillData->plrSkillLevel[id] = (unsigned char)i;
				else if (pSkillData->plrSkillLevel[id] > i)
					pSkillData->plrSkillLevel[id] = (unsigned char)i;
				//
				pPlayer->UpdateSkillTable();
				//
			//	pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			//	pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
			//	pMsg->m_nSize	= pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			//	pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_LEARN_SKILL;
			//	pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = id;
				//
				pPlayer->SaveSkillData();
				// 2008/07/07
				::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
				pPlayer->UpdateClientShowData(0);
				if (MapPlayer_IsSoldierAttrSkill((unsigned short)id))
					MapPlayer_RecalcAllDeployedSoldiers(pPlayer);
			}
			else
				return(param_tbl[0]);
		}
		else
			return(param_tbl[0]);
		break;
	case scMIS_CheckCityArmy:		//	[���O�T��][�O�d(��0)]	�P�_���a�Ҧb�����O�_������ӫ������x�ΡC
		pTown = CMapServer::GetServer()->GetTownDataByID(pPlayer->GetMapCode());
		if (!pTown)
			return(param_tbl[0]);
		if (!pTown->ptOrgUID)
			return(param_tbl[0]);
		if (pTown->ptOrgUID != pPlayer->GetSaveData()->plrOrganizeUID)
			return(param_tbl[0]);
		break;
	case scMIS_RunByForce:			//	[�Q][��][�d][�۳жդO]	�P�_�դO�A�L�դO�������U�@�B����
		switch(pPlayer->GetSaveData()->plrCountryID)
		{
		case ID_COUNTRY_WEI:				// �Q
			return(param_tbl[0]);
			break;
		case ID_COUNTRY_SHU:				// ��
			return(param_tbl[1]);
			break;
		case ID_COUNTRY_WU:					// �d
			return(param_tbl[2]);
			break;
		case ID_COUNTRY_NONE:				// �L
			break;
		default:
			return(param_tbl[3]);
			break;
		}
		break;
	case scMIS_CastSkill:			//	[���ѰT��][�ޯ�ID][�ޯ൥��][�O�d(��0)]	�缾�a�I��[�����A���ޯ�
		id = pPlayer->GetNPCTalkUID();
		if (!id)
			return(param_tbl[0]);
		pNPC = (CMapNPC *)CMapServer::GetServer()->FindAndCheckCharExistByUID(id, CMapNPC::CLASS_ID);
		if (!pNPC)
			return(param_tbl[0]);
		if (!pNPC->NPCCastMagic(param_tbl[1], param_tbl[2], pPlayer))
			return(param_tbl[0]);
		break;
	case scMIS_CheckTime:			//	[���b�ɶ�����][�b�ɶ�����][��(0-23)][��(0-59)][�g�L�ɶ�(����)]
		i = CMapServer::GetServer()->GetLoopTime();
		//
		ptm = ::apiGetTimeStruct(i);
		memcpyT(&ntm, ptm, sizeof(ntm));
		ntm.tm_hour = param_tbl[2];
		ntm.tm_min = param_tbl[3];
		ntm.tm_sec = 0;
		if ((ntm.tm_hour > 23) || (ntm.tm_hour < 0))
		{
			CMapServer::GetServer()->Log("ERROR: scMIS_CheckTime NPC talk time hour(%d)", param_tbl[2]);
			ntm.tm_hour = 0;
		}
		if ((ntm.tm_min > 59) || (ntm.tm_min < 0))
		{
			CMapServer::GetServer()->Log("ERROR: scMIS_CheckTime NPC talk time min(%d)", param_tbl[3]);
			ntm.tm_min = 0;
		}
		num = (long)mktime(&ntm);
		//
		id = num + param_tbl[4] * 60;
		if ((num <= i) && (i <= id))		// ����
			return(param_tbl[1]);
		num = num - (60*60*24);
		id = num + param_tbl[4] * 60;
		if ((num <= i) && (i <= id))		// �Q��
			return(param_tbl[1]);
		return(param_tbl[0]);
		break;
	case scMIS_CheckFreeItemSpace:	// [������ID][�ˬd�ƥ�]
		{
			long need_space = param_tbl[1];
			long carry_max = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
			long free_slots = 0;
			struct plrCARRYITEM_SAVE * pCarry = pPlayer->GetItemData();
			long fail_msg_id = param_tbl[0];
			// 新副本脚本参数异常时兜底，避免把 def_xxx 或脏值当成“需要格数”
			if (need_space <= 0 || (carry_max > 0 && need_space > carry_max))
				need_space = 1;
			if (pCarry && carry_max > 0)
			{
				for (i = 0; i < carry_max; i++)
				{
					if (!pCarry->plrCarryItem[i].m_Item.itemCode)
						free_slots++;
				}
			}
			// 优先使用实时空格统计，避免旧检查函数在特定副本链路误判“背包满”
			if (free_slots < need_space)
			{
				// 定向绕过：该副本奖励链使用 def_024393(24393) + 1格检查，当前环境存在误判，先放行领奖
				// 同时覆盖 exp 对话链 def_057563(57563) -> item 509948/509949 随机奖励入口
				if ((fail_msg_id == 24393 || fail_msg_id == 57563) && need_space == 1)
					break;

				FILE * fp = fopen("FreeSpaceDebug.txt", "a+");
				if (fp)
				{
					fprintf(fp, "CheckFreeItemSpace fail name=%s uid=%u map=%d copy=%u need=%d free=%d max=%d\n",
						pPlayer->GetName(), pPlayer->GetUID(), pPlayer->GetMapCode(), pPlayer->GetCopyUID(),
						(int)need_space, (int)free_slots, (int)carry_max);
					fclose(fp);
				}
				return(param_tbl[0]);
			}
		}
		break;
	case scMIS_CheckStarName:		// [�S���P�����ܽs��]	�ˬd�W�l�O�_���b�άP���}�Y
		if (pPlayer->GetName()[0] != '*')
			return(param_tbl[0]);
		break;
	//
	case scMIS_GiveExpPercent:		// [�ʤ���]	���g�� %(�̦h 100%)
		num = param_tbl[0];
		nExp = Inner_Calc_Perc_Val(pPlayer->GetCharData()->plrLevelUpExp, num);	// ::gameServerNPC_GetLevelUpExp(GetSaveData()->plrLevel);
		if (!nExp)
			nExp++;
		pPlayer->AddExp(nExp, pPlayer);
		//
		wsprintf(tmpstr, MSG_ADD_EXP, nExp);
		pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
		//pPlayer->SendPlayerGetExp(nExp, 0, 0, 0);
		pPlayer->UpdatePlayerDataPart1();
		//
		pPlayer->SaveCharData();
		break;
	case scMIS_RandomRun:			// [����1][����2][����3][����4][����5][����6]	�ü� ������O(�@7�ؾ��|)
		id = 10;
		for (i=0; i<6; i++)
		{
			if (param_tbl[i])		// ���ĳ]�w
				id += 10;
		}
		if (id > 10)
		{
			i = gameGetRandomRange(0, id-1) / 10;		// i = 0,1,2... n-1
			if (i > 6)
				i = 6;
			id = 0;
			while (i >= 0)
			{
				if (!id)
				{
					if (--i < 0)
						break;				// ����U�@��
				}
				else if (param_tbl[id-1])		// �����O���ĳ]�w
				{
					if (--i < 0)
						return(param_tbl[id-1]);
				}
				//
				id++;
				if (id > 6)
					id = 0;
			}
		}
		break;
	case scMIS_CheckLevelRange:		// [�j�p�󵥩�ɸ��ܹ�ܽs��][level min][level max]	�ˬd���Žd��
		id = param_tbl[1];
		num = param_tbl[2];
		i = pPlayer->GetSaveData()->plrLevel;
		if ((id <= i) && (i <= num))
			return(param_tbl[0]);
		break;
	case scMIS_CheckWAWAEquip:		// [�S���ɪ�tID][���~1][���~2][���~3][���~4][���~5]	�P�_�w�˳Ƨ�˪�
		pSave = pPlayer->GetSaveData();
		for (num=1; num<=5; num++)
		{
			id = param_tbl[num];
			if (id)
			{
				if (pSave->plrWawaWeaponR.nItemCode == id)
					return(0);
				if (pSave->plrWawaArmor.nItemCode == id)
					return(0);
				if (pSave->plrWawaHead.nItemCode == id)
					return(0);
				if (pSave->plrWawaFoot.nItemCode == id)
					return(0);
				if (pSave->plrWawaP.nItemCode == id)
					return(0);
				if (pSave->plrWawaHorse.nItemCode == id)
					return(0);
				if (pSave->plrWawaAllShape.nItemCode == id)
					return(0);
			}
		}

		//
		return(param_tbl[0]);
		break;
	case scMIS_CheckIsMarry:		// [�S���ɪ�tID]	�O�_���t��
		if (!pPlayer->GetSaveData()->plrMarry_UID)
			return(param_tbl[0]);
		break;
	case scMIS_CheckMarryParty:		// [�����][���H���t��][���O1�k1�k][�Ҧ�]	�O�_ 2�H����A�ն���H�Ҭ��L�t�����A�A�ն�����H�B�@�k�@�k/
		if (pPlayer->PartyGetTotal() != 2)
		{
no_find:
			// !!!
			return(param_tbl[0]);
		}
		//
		uid = pPlayer->PartyGetMemberByPos(0);
		if (uid == pPlayer->GetUID())
			uid = pPlayer->PartyGetMemberByPos(1);
		if (!uid)
			goto no_find;
		pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);
		if (!pDest)
			goto no_find;
		// �O�_�P�a��
		if (pPlayer->GetMapCode() != pDest->GetMapCode())
			goto no_find;
		//
		if ((pPlayer->GetSaveData()->plrMarry_UID) || (pDest->GetSaveData()->plrMarry_UID))
			return(param_tbl[1]);
		if (pPlayer->GetSaveData()->plrSex == pDest->GetSaveData()->plrSex)
			return(param_tbl[2]);
		//
		if (!param_tbl[3])
		{
			// �O�_�ɶ�����
			if (i = CMapServer::GetServer()->GetMerryDelay(pPlayer->GetUID()))
			{
				id = i / 60;
				i = i % 60;
				// 21370		��������%d��%d����~��A���D�B�I
				wsprintf(tmpstr, gameGetResourceName(21370), id, i);
				pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
				return(-2);									// �������
			}
			// �O�_��赥�ݨD�B�^��
			if ((pPlayer->m_nPlayerFlags & PLAYER_FLAGS_WAIT_MERRY) || (pDest->m_nPlayerFlags & PLAYER_FLAGS_WAIT_MERRY))
			{
				pPlayer->SendMsgToClientByID( 21363 );		// 21363 ��西���ݨD�B�^�����K
				return(-2);									// �������
			}
			//
			i = CMapServer::GetServer()->GetLoopTime() + MERRY_WAIT_RESPONSE_TIME;	// �^���ɶ�
			pDest->m_nPlayerFlags |= PLAYER_FLAGS_WAIT_MERRY;
			pDest->m_nWaitMerryTime = 0;
			pDest->m_nMerryPlayerUID = pPlayer->GetUID();
	 #ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG: Set merry target status(%s)", pDest->GetName());
	 #endif
			//
			pPlayer->m_nPlayerFlags |= PLAYER_FLAGS_WAIT_MERRY;
			pPlayer->m_nWaitMerryTime = i;
			pPlayer->m_nMerryPlayerUID = uid;
			msg_strncpy(pPlayer->m_szMerryPlayerName, pDest->GetName(), sizeof(pPlayer->m_szMerryPlayerName));
			pPlayer->m_nMerryPlayerOfficer = (unsigned short)gameGetCharacterOfficeID(pDest->GetCharData());
	 #ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG: Set merry status(%s)", pPlayer->GetName());
	 #endif
			//
			pPlayer->ProcAskMerry(pDest);
		}
		break;
	case scMIS_Divorce:				// [�L�t���ɪ�tID][�O�d0][�O�d0]	���B
		uid = pPlayer->GetSaveData()->plrMarry_UID;
		if (!uid)
			return(param_tbl[0]);
		pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);
		if (pDest)			// �P�@��map�A�����B�z
		{
			if (pDest->GetSaveData()->plrMarry_UID == pPlayer->GetUID())	// ������ҥ��T�~��
				pDest->ProcDivorce();
		}
		else				// �q��Login�B�z
		{
			// �ץ����t�����
			::memset( &nCityPack, 0, sizeof(nCityPack) );
			nCityPack.nType = TYPE_PACK_PLAYER_UPDATE_MATE;			// d1=player uid, d2=mate uid ��s�t��uid
			nCityPack.nData1 = uid;
			nCityPack.nData2 = 0xffffffff;				// ���ܥ��h�t��
			nCityPack.nData3 = pPlayer->GetUID();		// ������Ҧۤv����
			CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0);
		}
		//
		pPlayer->ProcDivorce();
		break;
	case scMIS_ClearMission:		// [���Ƚs��][�O�d0]	�M�����Ȫ��A
		id = param_tbl[0];
		if (id < gameMAX_MISSION)
		{
			pPlayer->SetMissionState(id, gameMISSION_STATE_NONE);
			pPlayer->SendMissionDataToClient();
			pPlayer->SaveMissionData(0);
		}
		break;
	case scMIS_CheckSameForce:		// [���P�դOtID][�a��1][�a��2][�a��3][�a��4][�a��5][�a��6]	�ˬd�����O�_�P�դO
		pTown = CMapServer::GetServer()->GetTownDataByID(param_tbl[1], 1);
		if (!pTown)
			return(param_tbl[0]);
		id = pTown->ptCountryID;
		//
		for (i=2; i<=6; i++)
		{
			num = param_tbl[i];
			if (!num)
				break;
			//
			pTown = CMapServer::GetServer()->GetTownDataByID(num, 1);
			if (!pTown)
				return(param_tbl[0]);
			if (pTown->ptCountryID != id)
				return(param_tbl[0]);
		}
		break;
	case scMIS_ForceTeleport:		// �դO�ǰe		[�դO���s�btID]
		switch(pPlayer->GetSaveData()->plrCountryID)
		{
		case ID_COUNTRY_NULL:
		case ID_COUNTRY_NONE:
			return(param_tbl[0]);
			break;
		}
		// �d�U���A
		pPlayer->m_nPlayerFlags |= PLAYER_FLAGS_WAIT_FORCE_CHG;		// ���ݦ^�����A
		// �q�� Client
		pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_FORCE_TELEPORT_MENU);

		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		break;
	case scMIS_OpenGate:			// [���~��ID][�O�d0]	�����x�ζ}����
		map_code = pPlayer->GetMapCode();
		if (!CMapServer::GetServer()->IsMapWar(map_code))
		{
  #ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG: �L��Ԥ���}����(%s)", pPlayer->GetName());
  #endif
			return(param_tbl[0]);
		}
		// �ˬd���a�ϬO�_�ݩ�ۤv�x�Φ���
		pMapCtrl = CMapServer::GetServer()->FindMap(map_code);
		if (!pMapCtrl)
		{
			return(param_tbl[0]);
		}
		else
		{
			CMapGate * pGate = pMapCtrl->GetGate();

			if (!pGate)
			{
  #ifndef USE_PACKAGE_DATA
				CMapServer::GetServer()->Log("DEBUG: �L�����i�}(%s)", pPlayer->GetName());
  #endif
				return(param_tbl[0]);
			}
			//
			if (!pGate->m_nCityCode)
			{
  #ifndef USE_PACKAGE_DATA
				CMapServer::GetServer()->Log("DEBUG: �����L�D����map id����(%s)", pPlayer->GetName());
  #endif
				return(param_tbl[0]);
			}
			// �H�D�n��������
			//pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pGate->m_nCityCode);
			pTown = CMapServer::GetServer()->GetTownDataByID(pGate->m_nCityCode);
			if (!pTown)
				return(param_tbl[0]);
			if (!pTown->ptOrgUID)
				return(param_tbl[0]);
			if (pTown->ptOrgUID != pPlayer->GetSaveData()->plrOrganizeUID)
				return(param_tbl[0]);
			// ���Ҷ���(�F���H�W)
			if (!(pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_OPEN_GATE))
			{
  #ifndef USE_PACKAGE_DATA
				CMapServer::GetServer()->Log("DEBUG: �L�}���v��(%s)", pPlayer->GetName());
  #endif
				return(param_tbl[0]);
			}
			// �q�� MapCtrl �}����
			if (pGate->IsDead())
			{
	fail_open:
				pPlayer->SendMsgToClientByID(21444);		// �����L�k�ߨ譫�s�}��!
			}
			else
			{
				if (!pGate->OpenGate())
					goto fail_open;
				pPlayer->SendMsgToClientByID(21443);		// �����}�Ҥ�!
			}
		}
		break;
	case scMIS_ForceChangePos:		// [���Ѯɹ�ܽs��][��V0=��,1=�k][�X��Z���A�qnpc��m��][���w���][���w�P���y][�O�d0]	�u�Z���ǰe�A��ԫ������k�ǰe
		map_code = pPlayer->GetMapCode();
		//
		if (param_tbl[3])		// �O�_���w���
		{
			if (!CMapServer::GetServer()->IsMapWar(map_code))
				return(param_tbl[0]);
		}
		if (param_tbl[4])		// �O�_���w�P���y
		{
			pTown = CMapServer::GetServer()->GetTownDataByID(map_code);
			if (!pTown)
				return(param_tbl[0]);
			if (pTown->ptCountryID != pPlayer->GetSaveData()->plrCountryID)
				return(param_tbl[0]);
		}
		//
		pMapCtrl = CMapServer::GetServer()->FindMap(map_code);
		if (!pMapCtrl)
		{
			return(param_tbl[0]);
		}
		//
		id = pPlayer->GetNPCTalkUID();
		if (!id)
			return(param_tbl[0]);
		pNPC = (CMapNPC *)CMapServer::GetServer()->FindAndCheckCharExistByUID(id, CMapNPC::CLASS_ID);
		if (!pNPC)
			return(param_tbl[0]);
		//
		x = pNPC->GetPosX();
		y = pNPC->GetPosY();
		if (param_tbl[1] == 0)		// ����
		{
			x -= param_tbl[2] * gameIconSize;
		}
		else
		{
			x += param_tbl[2] * gameIconSize;
		}
		//if (pMapCtrl->GetIconData(gameIconCalc_DIV(x), gameIconCalc_DIV(y)) != MAPCODE_ID_WALL)
			pPlayer->ChangeMap(map_code, x, y, 1);		// 1 = ���n�L�ļҦ��A��������
		break;
	case scMIS_CancelDoubleStatus:	// [���Ѯɹ�ܽs��][�n�������[��ID]	�����[�����A
		if (!pPlayer->CancelSpecialStatus(param_tbl[1]))
			return(param_tbl[0]);
		break;
	case scMIS_OnClassJump:			// [����][�r�N�s��][���ǽs��][�x�v�s��][��h�s��] [�äh][�����h][job�O�d1][job�O�d2]	�̷�¾�~�M���ŭ�����D�A���Ť��ũάO���D��0���|����U�@��
		num = param_tbl[0];
		if (num)
		{
			if (num < 0)
			{
				num = -num;
				if (pPlayer->GetSaveData()->plrLevel > num)
					return(0);
			}
			else
			{
				if (pPlayer->GetSaveData()->plrLevel < num)
					return(0);
			}
		}
		//
		id = 0;
		switch(pPlayer->GetSaveData()->plrJobID)
		{
		case jobID_WARLORD:				// �r�N
			id = param_tbl[1];
			break;
		case jobID_LEADER:				// ����
			id = param_tbl[2];
			break;
		case jobID_ADVISOR:				// �x�v
			id = param_tbl[3];
			break;
		case jobID_WIZARD:				// ��h
			id = param_tbl[4];
			break;
		case jobID_ASSASSIN:
			id = param_tbl[5];
			break;
		case jobID_ENGINEER:
			id = param_tbl[6];
			break;
	//	case jobID_:
	//		id = param_tbl[7];
	//		break;
	//	case jobID_:
	//		id = param_tbl[8];
	//		break;
		}
		if (id)
			return(id);
		break;
#ifdef CROSS_SERVER_SYSTEM			// scMIS_HistoryRegister ���W
	case scMIS_KS_Register:			//	[�s��:�������~][�s��:�w�g���W][�s��:�W�B�w��][�s��:�j�פ������ε��Ť���][�Գ��a��][�}��][�O�d0]	����A��:�Գ����W
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
 #ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			pPlayer->SendMsgToClientByID(5000010);		// �����~���i�ѥ[����A�����W!
			return(param_tbl[0]);
		}
 #endif
		//
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;	// �ǻ������ƨó��W

			id = 300;	// �� 300��
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_AlreadyReg = param_tbl[1];
			pPlayer->m_WaitKS_TalkID_Full = param_tbl[2];
			pPlayer->m_WaitKS_TalkID_NoBalance = param_tbl[3];
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
			nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_KS_REGISTER;
			nTalkPackDataKS.nData1 = param_tbl[4];	// map
			nTalkPackDataKS.nData2 = param_tbl[5];	// team
			nTalkPackDataKS.nData3 = 0;				// server id( login server �� )
			nTalkPackDataKS.player_uid = pPlayer->GetUID();
			//
			memcpyT(&nTalkPackDataKS.nSkill, pPlayer->GetSkillData(), sizeof(nTalkPackDataKS.nSkill));
			memcpyT(&nTalkPackDataKS.nPlrSaveData, pPlayer->GetSaveData(), sizeof(nTalkPackDataKS.nPlrSaveData));

#ifndef MAX_CARRY_ITEM_NUM_ALL
			memcpyT(nTalkPackDataKS.nCarryItem, pPlayer->GetItemData()->plrCarryItem, sizeof(nTalkPackDataKS.nCarryItem));
#endif
			//
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, sizeof(nTalkPackDataKS), 0);
//CMapServer::GetServer()->Log("Test: %s cross server register", pPlayer->GetSaveData()->plrName);
			//
			return(-1);		// wait next
		}
		return(param_tbl[0]);
		break;
	case scMIS_KS_CheckRegister:	//	[�s��:�������~][�s��:�S�����W][�Գ��a��][�O�d0][�O�d0]	����A��:�ˬd���W���A
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
 #ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			pPlayer->SendMsgToClientByID(5000010);		// �����~���i�ѥ[����A�����W!
			return(param_tbl[0]);
		}
 #endif
		//
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;

			id = 300;	// �� 300��
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_NoReg = param_tbl[1];
			pPlayer->m_WaitKS_TalkID_Full = 0;
			pPlayer->m_WaitKS_TalkID_NoBalance = 0;
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
			nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_KS_CHECK_REGISTER;
			nTalkPackDataKS.nData1 = param_tbl[2];	// map
			nTalkPackDataKS.nData2 = 0;
			nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
			nTalkPackDataKS.nData3 = 0;				// server id( login server �� )
			nTalkPackDataKS.player_uid = pPlayer->GetUID();
			msg_strncpy(nTalkPackDataKS.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nTalkPackDataKS.szAccount));
			//
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, nTalkPackDataKS.GetSmartSize(), 0);
			return(-1);		// wait next
		}
		return(param_tbl[0]);
		break;
	case scMIS_KS_EnterStage:		//	[�s��:�������~][�s��:�S�����W][�Գ��a��][�O�d0][�O�d0]	����A��:�i�J�Գ�
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
 #ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			pPlayer->SendMsgToClientByID(5000010);		// �����~���i�ѥ[����A�����W!
			return(param_tbl[0]);
		}
 #endif
		//
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;

			id = 300;	// �� 300��
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_NoReg = param_tbl[1];
			pPlayer->m_WaitKS_TalkID_Full = 0;
			pPlayer->m_WaitKS_TalkID_NoBalance = 0;
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
			nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_KS_ENTER;
			nTalkPackDataKS.nData1 = param_tbl[2];	// map
			nTalkPackDataKS.nData2 = 0;
			nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
			nTalkPackDataKS.nData3 = 0;				// server id( login server �� )
			nTalkPackDataKS.player_uid = pPlayer->GetUID();
			nTalkPackDataKS.nUserData1 = pPlayer->GetPosX();
			nTalkPackDataKS.nUserData2 = pPlayer->GetPosY();
			nTalkPackDataKS.nUserData3 = pPlayer->m_nPrivilege;
			msg_strncpy(nTalkPackDataKS.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nTalkPackDataKS.szAccount));

#ifdef MAX_CARRY_ITEM_NUM_ALL
			memcpyT(&nTalkPackDataKS.nCarryItem, pPlayer->GetItemData(), sizeof(nTalkPackDataKS.nCarryItem));

			//
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, sizeof(nTalkPackDataKS), 0);
#else
			//
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, nTalkPackDataKS.GetSmartSize(), 0);
#endif
			return(-1);		// wait next
		}
		return(param_tbl[0]);
		break;
	case scMIS_DragonGold_Check:			// [���Ѯɸ��ܽs��][�s�ȭ�]
		num = param_tbl[1];
		i = pPlayer->GetSaveData()->plrKS_Score;
		if ((unsigned long)i < (unsigned long)num)
		{
			return(param_tbl[0]);
		}
		break;
	case scMIS_DragonGold_Give:				// [�s�ȭ�]
		pPlayer->AddKSScore(param_tbl[0], 0);
		wsprintf(tmpstr, gameGetResourceName(5000247), param_tbl[0]);		//xavi�s�W��r����
		pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
		break;
	case scMIS_KS_CWar_Score_Check:			// [���Ѯɸ��ܽs��][���Q��]
		num = param_tbl[1];
		i = pPlayer->GetSaveData()->plrKS_RedScore;
		if ((unsigned long)i < (unsigned long)num)
		{
			return(param_tbl[0]);
		}
		break;
	case scMIS_KS_CWar_ScoreGive:			// [���Q��]
		pPlayer->AddKSScore(0, param_tbl[0]);
		wsprintf(tmpstr, gameGetResourceName(5000002), param_tbl[0]);		//xavi�s�W��r����
		pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
		break;
 #ifdef CROSS_SERVER_CWAR
	case scMIS_KS_CWar_Register:		// [�s��:�������~][�s��:�w�g���W][�s��:�W�B�w��(or���b�i���W�ɬqor���W����ɶ�������)][�s��:�x�Ϊ��|�����W�A�άO��´�H�Ƥw���A�߰ݬO�_�n�~����W][�Գ��a��][�}��][�O�d0]
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
  #ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			pPlayer->SendMsgToClientByID(5000010);		// �����~���i�ѥ[����A�����W!
			return(param_tbl[0]);
		}
  #endif
		//
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			// ���ަ��L�x�Ϊ��@�ֳB�z
			struct LOGIN_NPCTALK_PACK_DATA_CWAR_KS nTalkPackDataCWarKS;	// �ǻ������ƨó��W

			id = 300;	// �� 300��
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_AlreadyReg = param_tbl[1];
			pPlayer->m_WaitKS_TalkID_Full = param_tbl[2];
			pPlayer->m_WaitKS_TalkID_ArmyNoRegister = param_tbl[3];	// �Ϊ������W
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			memset(&nTalkPackDataCWarKS, 0, sizeof(nTalkPackDataCWarKS));
			nTalkPackDataCWarKS.nType = NPCTALK_PACK_TYPE_CWAR_KS_REGISTER;
			//nTalkPackDataCWarKS.nData1 = param_tbl[4];	// map
			//nTalkPackDataCWarKS.nData2 = param_tbl[5];	// team
			//nTalkPackDataCWarKS.nData3 = 0;				// server id( login server �� )
			nTalkPackDataCWarKS.player_uid = pPlayer->GetUID();
			nTalkPackDataCWarKS.org_uid = pPlayer->GetSaveData()->plrOrganizeUID;
			nTalkPackDataCWarKS.org_leader_uid = pPlayer->GetSaveData()->plrOrganizeLeaderUID;
			nTalkPackDataCWarKS.nSetNumber = CMapServer::GetServer()->GetServerInfo()->iniSetNumber;
			nTalkPackDataCWarKS.nData1 = pPlayer->GetSaveData()->plrAccountID;
			msg_strncpy(nTalkPackDataCWarKS.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nTalkPackDataCWarKS.szAccount));
			nTalkPackDataCWarKS.nData2 = param_tbl[5];		// country
			pPlayer->m_CWarKS_CountryID = param_tbl[5];		// �Ȧs
			//
			//CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataCWarKS, nTalkPackDataCWarKS.GetSmartSize(), 0);
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataCWarKS, sizeof(nTalkPackDataCWarKS), 0);
			//
			return(-1);		// wait next
		}
		return(param_tbl[0]);
		break;
	case scMIS_KS_CWar_Register_Confirm:	// [�s��:�������~][�s��:�w�g���W][�s��:�W�B�w��(or���b�i���W�ɬqor���W����ɶ�������)][�s��:�j�פ�����(or���a���Ť��žԳ�����)][�Գ��a��][�}��][�O�d0]
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
  #ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			pPlayer->SendMsgToClientByID(5000010);		// �����~���i�ѥ[����A�����W!
			return(param_tbl[0]);
		}
  #endif
		//
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;	// �ǻ������ƨó��W

			id = 300;	// �� 300��
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_AlreadyReg = param_tbl[1];
			pPlayer->m_WaitKS_TalkID_Full = param_tbl[2];
			pPlayer->m_WaitKS_TalkID_NoBalance = param_tbl[3];
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
			nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_CWAR_KS_REGISTER_CONFIRM;
			//nTalkPackDataKS.nData1 = param_tbl[4];	// map
			if (!param_tbl[5])
			{											// �o�̪��}��ѥ��e�������ӵ�
				nTalkPackDataKS.nData2 = pPlayer->m_CWarKS_CountryID;
			}
			else
				nTalkPackDataKS.nData2 = param_tbl[5];	// team
			nTalkPackDataKS.nData3 = 0;				// server id( login server �� )
			nTalkPackDataKS.player_uid = pPlayer->GetUID();
			//
			nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
			// �����x�Ϊ��A��
			nTalkPackDataKS.nUserData1 = pPlayer->GetSaveData()->plrOrganizeUID;
			//
			memcpyT(&nTalkPackDataKS.nSkill, pPlayer->GetSkillData(), sizeof(nTalkPackDataKS.nSkill));
			memcpyT(&nTalkPackDataKS.nPlrSaveData, pPlayer->GetSaveData(), sizeof(nTalkPackDataKS.nPlrSaveData));

#ifndef MAX_CARRY_ITEM_NUM_ALL
			memcpyT(nTalkPackDataKS.nCarryItem, pPlayer->GetItemData()->plrCarryItem, sizeof(nTalkPackDataKS.nCarryItem));
#endif
			//
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, sizeof(nTalkPackDataKS), 0);
//CMapServer::GetServer()->Log("Test: %s cwar cross server register", pPlayer->GetSaveData()->plrName);
			//
			return(-1);		// wait next
		}
		return(param_tbl[0]);
		break;
	case scMIS_KS_CWar_CheckRegister:	// [�s��:�������~][�s��:�S�����W][�Գ��a��][�O�d0][�O�d0]
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
 #ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			pPlayer->SendMsgToClientByID(5000010);		// �����~���i�ѥ[����A�����W!
			return(param_tbl[0]);
		}
 #endif
		//
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;

			id = 300;	// �� 300��
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_NoReg = param_tbl[1];
			pPlayer->m_WaitKS_TalkID_Full = 0;
			pPlayer->m_WaitKS_TalkID_NoBalance = 0;
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
			nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_CWAR_KS_CHECK_REGISTER;
			//nTalkPackDataKS.nData1 = param_tbl[2];	// map
			nTalkPackDataKS.nData2 = 0;
			nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
			nTalkPackDataKS.nData3 = 0;				// server id( login server �� )
			nTalkPackDataKS.player_uid = pPlayer->GetUID();
			msg_strncpy(nTalkPackDataKS.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nTalkPackDataKS.szAccount));
			//
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, nTalkPackDataKS.GetSmartSize(), 0);
			return(-1);		// wait next
		}
		return(param_tbl[0]);
		break;
	case scMIS_KS_Cwar_EnterStage:		// [�s��:�������~or�ɶ�����][�s��:�S�����W][�Գ��a��][�O�d0][�O�d0]
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		//
 #ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			pPlayer->SendMsgToClientByID(5000010);		// �����~���i�ѥ[����A�����W!
			return(param_tbl[0]);
		}
 #endif
		//
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;

			id = 300;	// �� 300��
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_NoReg = param_tbl[1];
			pPlayer->m_WaitKS_TalkID_Full = 0;
			pPlayer->m_WaitKS_TalkID_NoBalance = 0;
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
			nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_CWAR_KS_ENTER;
			//nTalkPackDataKS.nData1 = param_tbl[2];	// map
			nTalkPackDataKS.nData2 = 0;
			nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
			nTalkPackDataKS.nData3 = 0;				// server id( login server �� )
			nTalkPackDataKS.player_uid = pPlayer->GetUID();
			nTalkPackDataKS.nUserData1 = pPlayer->GetPosX();
			nTalkPackDataKS.nUserData2 = pPlayer->GetPosY();
			nTalkPackDataKS.nUserData3 = pPlayer->m_nPrivilege;
			msg_strncpy(nTalkPackDataKS.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nTalkPackDataKS.szAccount));

#ifdef MAX_CARRY_ITEM_NUM_ALL
			memcpyT(&nTalkPackDataKS.nCarryItem, pPlayer->GetItemData(), sizeof(nTalkPackDataKS.nCarryItem));

			//
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, sizeof(nTalkPackDataKS), 0);
#else
			//
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, nTalkPackDataKS.GetSmartSize(), 0);
#endif
			return(-1);		// wait next
		}
		return(param_tbl[0]);
		break;


	//�i�J��A���A��
	case scMIS_KS_CWar_EnterCrossServer:	// [�s��:�������~][�s��:�W�B�w��][�O�d0]
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)	// �W�@���S����
			return(0);
		
		i = CMapServer::GetServer()->GetLoginServer();
		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;	// �ǻ������ƨó��W
			long BagNum = gameServer_GetCarryItemMax(pPlayer->GetSaveData());
			struct itemDATA_SAVE * pItem = NULL;

			id = 300;	// �� 300��
			pPlayer->m_WaitVT_Timeout = CMapServer::GetServer()->GetLoopTime() + id;
			pPlayer->m_WaitVT_State |= (PLAYER_NPCTALK_WAIT_STATE | PLAYER_NPCTALK_WAIT_DATA);
			//
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_Full = param_tbl[1];
			pPlayer->m_WaitVT_TalkID_Ok = msg_id + 1;
			// �e�X�n�D
			memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
			nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_CWAR_KS_ENTERCROSS;

			nTalkPackDataKS.nData3 = 0;				// server id( login server �� )
			nTalkPackDataKS.player_uid = pPlayer->GetUID();
			nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
			nTalkPackDataKS.nUserData1 = pPlayer->GetPosX();
			nTalkPackDataKS.nUserData2 = pPlayer->GetPosY();
			nTalkPackDataKS.nUserData3 = pPlayer->m_nPrivilege;
			
			memcpyT(&nTalkPackDataKS.nSkill, pPlayer->GetSkillData(), sizeof(nTalkPackDataKS.nSkill));
			memcpyT(&nTalkPackDataKS.nPlrSaveData, pPlayer->GetSaveData(), sizeof(nTalkPackDataKS.nPlrSaveData));
			memcpyT(nTalkPackDataKS.nCarryItem, pPlayer->GetItemData()->plrCarryItem, sizeof(nTalkPackDataKS.nCarryItem));

			//�j���ˬd�i��a���~
			for(int j=0;j<BagNum;j++)
			{
				pItem = &nTalkPackDataKS.nCarryItem[j];
				//�u�i��a���O��Τh�L
				if((pItem->m_Item.itemCode != gameITEM_ID_KS_PASSTICKET) && !(pItem->m_Item.itemFlags & itemSHOW_FLAG_NPC) &&!(pItem->m_Item.itemFlags & itemSHOW_FLAG_SOUL)&&!(pItem->m_Item.itemFlags & itemSHOW_FLAG_ENGINEER))
					::memset(pItem, 0, sizeof(*pItem));
			}
			//�Ǧ�loginServer
			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, sizeof(nTalkPackDataKS), 0);
			return(-1);		// wait next
		}
		return(param_tbl[0]);
		break;

	//�i�J��A��Ԧa��
	case scMIS_KS_CWar_EnterMap:			// [���~(���t�ΰT��)][���\(�ǳƶǰe)][��ԩ|���}�l][��ԧY�N�����T��J��][���Ť��ŭ���][�O�d01]
		struct CWAR_KS_WORLD_INIT_DATA_MAP *CWarMapData;
		CWarMapData = CWar_KS_GetWorldSetting_Map(pPlayer->GetSaveData()->plrEnterStageID);
		if(!CMapServer::GetServer()->IsWar()) return(param_tbl[2]);//��ԩ|���}�l
		if(CMapServer::GetServer()->IsWar() == WAR_KEEPOUT) return(param_tbl[3]);//��ԧY�N�����T��J��
		if(CWarMapData)
		{
			i = CMapServer::GetServer()->GetLoginServer();
			if ( CMapServer::GetServer()->IsConnectedToServer(i) )
			{
				struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;	// �ǻ������ƨó��W
				// �e�X�n�D
				pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
				pPlayer->m_WaitKS_TalkID_NoBalance = param_tbl[4];
				pPlayer->m_WaitVT_TalkID_Ok = param_tbl[1];
				memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
				nTalkPackDataKS.nUserData4 = pPlayer->GetSaveData()->plrSetID;// ������serverID
				nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_CWAR_KS_ENTERCWARMAP;
				nTalkPackDataKS.player_uid = pPlayer->GetUID();
				nTalkPackDataKS.nUserData1 = pPlayer->GetSaveData()->plrEnterStageID + ID_COUNTRY_WEI;
				nTalkPackDataKS.nData1 = CWarMapData->nPlayerMap;
				msg_strncpy(nTalkPackDataKS.szAccount, pPlayer->GetName(), sizeof(char)*(gameMAX_PLAYER_NAME_SIZE+1));

				nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
				memcpyT(&nTalkPackDataKS.nPlrSaveData, pPlayer->GetSaveData(), sizeof(nTalkPackDataKS.nPlrSaveData));
				nTalkPackDataKS.nResult = 0;

				CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, sizeof(nTalkPackDataKS), 0);
				return(-1);
			}
		}
		else
			CMapServer::GetServer()->Log("Error: CWarMapData No data Player's Country ", pPlayer->GetSaveData()->plrEnterStageID );
		return(param_tbl[0]);
		break;

		//�i�J�Z��Գ��a��
	case scMIS_KS_CWar_EnterSoulMap:			// [���~(���t�ΰT��)][���\(�ǳƶǰe)][���Ť��ŭ���][�Z��Գ��|���}�l][mapcode][posY][posX][�w�ѥ[�L�Z��Գ�][itemID][itemNum][���~����]
		if(!CMapServer::GetServer()->IsSoulWar()) return(param_tbl[3]);		//�Z��Գ��|���}�l
		i = CMapServer::GetServer()->GetLoginServer();
#ifdef CROSS_SOUL_WAR_FUNC_FIX
		if (param_tbl[9] && param_tbl[8])									//�ˬd���~ID�μƶq
		{
			if (!pPlayer->IsItemEnough(param_tbl[8], param_tbl[9]))
			{
				return(param_tbl[10]);
			}
		}
#endif

		if ( CMapServer::GetServer()->IsConnectedToServer(i) )
		{
			struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;				// �ǻ������ƨó��W
			// �e�X�n�D
			pPlayer->m_WaitKS_TalkID_InnerError = param_tbl[0];
			pPlayer->m_WaitKS_TalkID_NoBalance = param_tbl[2];				// ���Ť���
			pPlayer->m_WaitVT_TalkID_Ok = param_tbl[1];
			pPlayer->m_WaitKS_TalkID_Full = param_tbl[7];
#ifdef CROSS_SOUL_WAR_FUNC_FIX
			pPlayer->m_WaitVT_TalkID_Error = param_tbl[10];					// �ݨD���~�ƶq����
#endif

			memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
			nTalkPackDataKS.nUserData4 = pPlayer->GetSaveData()->plrSetID;	// ������serverID
			nTalkPackDataKS.nData1 = param_tbl[4];							// �ǰe�a�IMapID�y��
			nTalkPackDataKS.nData2 = param_tbl[5]*gameIconSize;				// �ǰe�a�IMap�y��X
			nTalkPackDataKS.nData3 = param_tbl[6]*gameIconSize;				// �ǰe�a�IMap�y��Y
#ifdef CROSS_SOUL_WAR_FUNC_FIX
			nTalkPackDataKS.nData4 = param_tbl[9];							// ���~�ݨD�ƶq
			nTalkPackDataKS.nUserData2 = param_tbl[8];						// ���~ID
#endif

			nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_SWAR_KS_ENTERSWARMAP;
			nTalkPackDataKS.player_uid = pPlayer->GetUID();
			msg_strncpy(nTalkPackDataKS.szAccount, pPlayer->GetName(), sizeof(char)*(gameMAX_PLAYER_NAME_SIZE+1));

			nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
			memcpyT(&nTalkPackDataKS.nPlrSaveData, pPlayer->GetSaveData(), sizeof(nTalkPackDataKS.nPlrSaveData));
			nTalkPackDataKS.nResult = 0;

			CMapServer::GetServer()->SendData(i, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, sizeof(nTalkPackDataKS), 0);
			return(-1);
		}
		return(param_tbl[0]);
		break;


 #endif 
#endif
	case scMIS_SetPlayerRebirth:	//[���Ѯɹ�ܽs��(���t�ΰT��)],[���\�ɹ�ܽs��],[��ͦ���]
		pPlayer->GetSaveData()->plrRebirth = (unsigned char)param_tbl[2];
		::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
	
		//�e�X���
		struct plrDATASHOWSELF Self;
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );

		//
		pPlayer->UpdateShowData(); //��ۤvshowdata�s�����e���W�Ҧ��H�]�t�ۤv�^
		pPlayer->UpdatePlayerDDE_Login(); //�ն�����
		pPlayer->SaveCharData(); //���s���
		return (param_tbl[1]);
		break;

	case scMIS_CheckPlayerRebirth:	//[���Ѯɹ�ܽs��],[���\�ɹ�ܽs��],[��ͦ���]
		if(pPlayer->GetSaveData()->plrRebirth != param_tbl[2])
			return (param_tbl[0]);
		else
			return(param_tbl[1]);
		break;

	case scMIS_CheckPlayerRebirthCount:	// [fail_tID][success_tID][rebirthCount]
		if (pPlayer->GetSaveData()->plrRebirth < (unsigned char)param_tbl[2])
			return (param_tbl[0]);
		else
			return (param_tbl[1]);
		break;

	case scMIS_EnableMaterial:	// enable material system (reserved). Keep behavior as "pass through".
		// No-op: keep for script compatibility.
		// Intentionally do not change flow.
		break;

	case scMIS_CheckTeamLevel:	// [fail_tID][requiredLevel]
	{
		const unsigned short requiredLevel = (unsigned short)param_tbl[1];
		num = pPlayer->PartyGetTotal();
		long next_id = 0;

		if (param_tbl[2] > 0)
			next_id = param_tbl[2];
		else
		{
			// Compatibility for two common data layouts:
			// A) fail_tID is after current talk (e.g. msg+3): success should go to msg+1.
			// B) fail_tID is before current talk (e.g. msg-1): success should jump to msg+2.
			if (param_tbl[0] > msg_id)
				next_id = msg_id + 1;
			else
				next_id = msg_id + 2;
		}

		if (num <= 1)
		{
			if (pPlayer->GetSaveData()->plrLevel < requiredLevel)
				return (param_tbl[0]);
			if (next_id > 0)
				return(next_id);
			return(0);
		}

		for (i = 0; i < num; i++)
		{
			uid = pPlayer->PartyGetMemberByPos(i);
			if (!uid)
				continue;

			pDest = (uid == pPlayer->GetUID()) ? pPlayer : (CMapPlayer*)CMapServer::GetServer()->FindObjectByUID(uid);
			if (!pDest)
				return (param_tbl[0]);

			if (pDest->GetSaveData()->plrLevel < requiredLevel)
				return (param_tbl[0]);
		}
		if (next_id > 0)
			return(next_id);
		return(0);
	}
	break;

	case scMIS_ClearTeamMission:	// [next_tID][missionID] (fallback: clear mission for player)
	{
		long missionID = param_tbl[1];
		if (missionID > 0 && missionID < gameMAX_MISSION)
		{
			pPlayer->SetMissionState(missionID, gameMISSION_STATE_NONE);
			pPlayer->SendMissionDataToClient();
			pPlayer->SaveMissionData(0);
		}
		return (param_tbl[0]);
	}
	break;

	case scMIS_CheckOpenBattleField:	//[��ܽs��:�������~],[��ܽs��:���}�ҬO�x�Ϊ�],[��ܽs��:���}�ҬO�έ�]
		if (!pPlayer->GetSaveData()->plrOrganizeUID)
		{
			return (param_tbl[0]);
		}
		else
		{
			if (CMapServer::GetServer()->GetLoopTime() >= pPlayer->m_nArmyBattleFieldDueTime)
			{
				if (pPlayer->GetUID() == pPlayer->GetSaveData()->plrOrganizeLeaderUID)
				{
					return (param_tbl[1]);
				}
				else
				{
					return (param_tbl[2]);
				}
			}
		}
		break;

	case scMIS_OpenBattleField:	//[��ܽs��:�������~],[��ܽs��:�D�Ϊ�],[��ܽs��:�x���I�Ƥ���],[�����I��]
		if (!pPlayer->GetSaveData()->plrOrganizeUID)
		{
			return (param_tbl[0]);
		}
		else if (pPlayer->GetSaveData()->plrOrganizeLeaderUID != pPlayer->GetUID())
		{
			return (param_tbl[1]);
		}
		else
		{
			long nCostArmyPoints = param_tbl[3];
			if (nCostArmyPoints <= 0)
			{
				nCostArmyPoints = 0;
			}
			if (pPlayer->m_nArmyPoints < nCostArmyPoints)
			{
				return (param_tbl[2]);
			}
			else
			{
				i = CMapServer::GetServer()->GetLoginServer();
				if ( CMapServer::GetServer()->IsConnectedToServer(i) )
				{
					struct LOGIN_ARMY_OPENFIELD nReq;
					nReq.nOrgUID = pPlayer->GetSaveData()->plrOrganizeUID;
					nReq.nPlayerUID = pPlayer->GetUID();
					nReq.nCostArmyPoints = nCostArmyPoints;
					CMapServer::GetServer()->SendData( CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_ARMY_OPENFIELD, (char *)&nReq, sizeof(nReq), 0 );
				}
			}
		}
		break;

	case scMIS_EnterBattleField:	//[��ܽs��:�������~],[�Գ��s��],[����X],[����Y]
		if (!pPlayer->GetSaveData()->plrOrganizeUID)
		{
			return (param_tbl[0]);
		}
		else if (CMapServer::GetServer()->GetLoopTime() >= pPlayer->m_nArmyBattleFieldDueTime)
		{
			return (param_tbl[0]);
		}
		else
		{
			unsigned short map_code = gameGetForceBattleFieldMap(pPlayer->GetSaveData()->plrCountryID, (unsigned char)param_tbl[1]);
			if (map_code)
			{
				pPlayer->ChangeMap(map_code, param_tbl[2], param_tbl[3]);
			}
			else
			{
				return (param_tbl[0]);
			}
		}
		break;

	case scMIS_TakeItemBless:		// [���~1][�ƶq1][�[����1][���~2][�ƶq2][�[����2]	�����F��
		id = param_tbl[0];
		num = param_tbl[1];
		bless =  param_tbl[2];
		if (id)
			pPlayer->DelItemAndUpdateClientBless(id, num, bless, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
		id = param_tbl[3];
		num = param_tbl[4];
		bless =  param_tbl[5];
		if (id)
			pPlayer->DelItemAndUpdateClientBless(id, num, bless, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
		break;
	case scMIS_CheckItemBless:		// [�����ɪ�tID][���~1][�ƶq1][�[����1][���~2][�ƶq2][�[����2]
		for (i=0; i<2; i++)
		{
			id = param_tbl[i*3 + 1];
			num = param_tbl[i*3 + 2];
			bless =  param_tbl[i*3 + 3];
			if (id)
			{
				switch(id)
				{
				case item_Money:
					if (pPlayer->GetGold() < (unsigned long long)num)
						return(param_tbl[0]);
					break;
				case item_Exp:
					return(param_tbl[0]);
					break;
				case item_SkillExp:
					if (pPlayer->GetSkillExp() < (unsigned long)num)
						return(param_tbl[0]);
					break;
				default:
					if (!pPlayer->IsItemEnoughBless(id, num, bless))
					{
						return(param_tbl[0]);
					}
					break;
				}
			}
		}
		break;
	
	case scMIS_UpToLevel:		// ���ɨ�S�w����
		num = param_tbl[0];
		pSave = pPlayer->GetSaveData();
		level = num - pSave->plrLevel;
		if(level <= 0)
			 return(param_tbl[1]);
		pSave->plrLevel = (unsigned short)num;
		pSave->plrAttrPoint += (unsigned long long)(gameGetLevelUpAttrPointCfg() * level);
		if (pSave->plrAttrPoint > gameMAX_ATTR_POINT)
			pSave->plrAttrPoint = gameMAX_ATTR_POINT;
		pPlayer->GetCharData()->plrLevelUpExp = gameServerGetLevelUpExp(pSave->plrLevel);
		
		pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_ALL);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_LEVELUP, 0, pPlayer->GetUID());

		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();

		::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		pPlayer->UpdateShowData(); //��ۤvshowdata�s�����e���W�Ҧ��H�]�t�ۤv�^
		pPlayer->UpdatePlayerDataPart1();
		pPlayer->SaveCharData();
		return(param_tbl[2]);
		break;

	case scMIS_TakeArmyPointsWithActive:	//[��ܽs��:�������~],[��ܽs��:�D�Ϊ�],[��ܽs��:�x���I�Ƥ���],[�����I��],[���ʽs��](0:�L���ʯ¦���,1:�g�D�Z��)
		if (!pPlayer->GetSaveData()->plrOrganizeUID)
		{
			return (param_tbl[0]);
		}
		else if (pPlayer->GetSaveData()->plrOrganizeLeaderUID != pPlayer->GetUID())
		{
			return (param_tbl[1]);
		}
		else
		{
			long nCostArmyPoints = param_tbl[3];
			if (nCostArmyPoints <= 0)
			{
				nCostArmyPoints = 0;
			}
			if (pPlayer->m_nArmyPoints < nCostArmyPoints)
			{
				return (param_tbl[2]);
			}
			else
			{
				i = CMapServer::GetServer()->GetLoginServer();
				if ( CMapServer::GetServer()->IsConnectedToServer(i) )
				{
					struct LOGIN_ARMY_ACTIVE nReq;
					nReq.nOrgUID = pPlayer->GetSaveData()->plrOrganizeUID;
					nReq.nPlayerUID = pPlayer->GetUID();
					nReq.nCostArmyPoints = nCostArmyPoints;
					nReq.nActiveNum = (char)param_tbl[4];
					CMapServer::GetServer()->SendData( CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_ARMY_ACTIVE, (char *)&nReq, sizeof(nReq), 0 );
				}
			}
		}
		break;
	case scMIS_CheckArmyActive://[��ܽs��:�L�x��],[��ܽs��:�x�Ϊ��O�_�}�_����],[��ܽs��:�x�Ϊ����}�Ҭ���],[���ʽs��](0:�L���ʯ¦���,1:�g�D�Z��)
		if (!pPlayer->GetSaveData()->plrOrganizeUID)
		{
			return (param_tbl[0]);
		}
		else
		{
			switch(param_tbl[3])
			{
			case 0:
				return 0;
				break;
			case 1:
				if(CMapServer::GetServer()->GetLoopTime() >= pPlayer->m_OrgActiveData.soulTime)
				{
					if(pPlayer->GetUID() == pPlayer->GetSaveData()->plrOrganizeLeaderUID)
					{
						return (param_tbl[1]);
					}
					else
					{
						return (param_tbl[2]);
					}
				}
			}
		}
		break;
	//�Ǧ^��A�j�U
	case scMIS_KS_TeleportToInitMap:
		{
			long g_CountryPrepare[3] = {3698,3699,3700};//��A�j�U
			long idTemp  = pPlayer->GetSaveData()->plrEnterStageID;
			long map_x = 0,map_y = 0;
			if(pPlayer->GetSaveData()->plrEnterStageID <= 2 && pPlayer->GetSaveData()->plrEnterStageID >= 0)
				map_code = g_CountryPrepare[pPlayer->GetSaveData()->plrEnterStageID];
			if(param_tbl[0]) map_x = param_tbl[0]*64;
			if(param_tbl[1]) map_y = param_tbl[1]*64;
			Copy_UID = 0;
			if (!map_code)
			{
				map_code = pPlayer->GetMapCode();
				Copy_UID = pPlayer->GetCopyUID();
			}
			CMapServer::GetServer()->ChangeSaveMap(pPlayer, map_code, map_x, map_y, 0, 0,Copy_UID);
		}
		break;
	case scMIS_ItemChangeFix://���~�@��I��(�i������:���~�B�s�ȡB�C�����B�\���B�ޯ��I)
		count = 999999;
		for (i = 0; i<4; i++)
		{
			id = param_tbl[i * 2 + 1];
			num = param_tbl[i * 2 + 2];
			if (id)
			{
				switch (id)
				{
				case item_Money:
					temp = (long)(pPlayer->GetGold() / num);
					break;
				case item_SkillExp:
					temp = (long)(pPlayer->GetSkillExp() / num);
					break;
				case item_Honor:
					temp = (long)(pPlayer->GetSaveData()->plrHonor / num);
					break;
				case item_KSScore:
					temp = (long)(pPlayer->GetSaveData()->plrKS_Score / num);
					break;
				default:
					if (pPlayer->getCarryItemNum(id) >= 0)
						temp = (long)(pPlayer->getCarryItemNum(id) / num);
					else
						return(param_tbl[0]);
					break;
				}
				if (count > temp)
					count = temp;
			}
		}
		if (count <= 0)
			return(param_tbl[0]);
		else
		{
			temp = 0;
			for (i = 0; i < 4; i++)
			{
				id = param_tbl[i * 2 + 10];
				num = param_tbl[i * 2 + 11] * count;
				if (id == 0) continue;
				switch (id)
				{
					case item_Money:
					case item_Exp:
					case item_Honor:
					case item_KSScore:
					case item_SkillExp:
						break;
					default:
						temp += num / gameMAX_ITEM_STACK;
						if((num % gameMAX_ITEM_STACK) !=0)
							temp++;
						break;
				}

			}

			if (!pPlayer->CarryItem_CheckFreeSpace(temp))
				return(param_tbl[9]);		// �W�L������


			for (i = 0; i < 4; i++)
			{
				id = param_tbl[i * 2 + 1];
				num = param_tbl[i * 2 + 2] * count;
				if (id)
				{
					pPlayer->DelItemAndUpdateClient(id, num, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL);
				}
			}
			for (i = 0; i < 4; i++)
			{
				id = param_tbl[i * 2 + 10];
				num = param_tbl[i * 2 + 11] * count;
				if (id == item_Money)
				{
					gold = num;
					if (gold > 0)
					{
						total_gold = pPlayer->GetSaveData()->plrGold + gold;
						if (total_gold > gameMAX_GOLD)
							total_gold = gameMAX_GOLD;
						pPlayer->GetSaveData()->plrGold = total_gold;
						//
						pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);

						pMsg->m_Msg.m_UpdateData2Msg.Reset();
						pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETGOLD, 0, gold, 1);
						pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GOLD, 0, pPlayer->GetUID(), total_gold);

						pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
						// .... �O�� log ....
						if (pPlayer->m_nTalkNPC_UID && (pPlayer->m_nTalkNPC_UID != 0xffffffff))
							pChar = (CMapChar *)CMapServer::GetServer()->FindAndCheckCharExistByUID(pPlayer->m_nTalkNPC_UID, CMapChar::CLASS_ID);

						struct itemDATA_SAVE logItem;
						memset(&logItem, 0, sizeof(logItem));
						logItem.m_Item.itemCode = (unsigned short)item_Money;
						logItem.m_Item.itemGoldNumber = gold;
						CMapServer::GetServer()->SendLogMessage_Item(pPlayer, LOGTYPE_ITEM_FROM_NPC_GET, pChar, &logItem);
					}
				}
				else if (id == item_Exp)
				{
					nExp = num;
					pPlayer->AddExp(nExp, pPlayer);
					wsprintf(tmpstr, MSG_ADD_EXP, nExp);
					pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
					pPlayer->UpdatePlayerDataPart1();
				}
				else if (id == item_SkillExp)
				{
					nExp = num;
					pPlayer->AddSkillExp(nExp);
					pPlayer->UpdatePlayerDataPart1();
					wsprintf(tmpstr, MSG_ADD_SKILL_EXP, nExp);
					pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
					pPlayer->UpdatePlayerDataPart1();
				}
				else if (id == item_Honor)
				{
					pPlayer->AddHonor(num);
					wsprintf(tmpstr, gameGetResourceName(24216), num);
					pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
					pPlayer->UpdatePlayerDataPart1();
				}
				else if (id == item_KSScore)
				{
					pPlayer->AddKSScore(num,0);
					wsprintf(tmpstr, gameGetResourceName(5000247), num);
					pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
					pPlayer->UpdatePlayerDataPart1();
				}
				else
				{
					if (id && (num > 0))
					{
						if (pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_FROM_NPC_GET, 0, pPlayer))
						{
							pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);

							pMsg->m_Msg.m_UpdateData2Msg.Reset();
							pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETITEM_SHOW_ON_MAINBOARD, (short)num, id, 0);

							pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
						}
						else
							CMapServer::GetServer()->Log("(%s)�L�k�إߪ��~(%d,%d,%s)", pPlayer->GetName(), id, num);
					}
				}
				pPlayer->SaveCharData();
			}
		}
		break;

	case scMIS_OpenBattleExchangePage:
		{
			struct MAP_BATTLE_EXCHANGE_ITEM_RESULT res;
			long pageCode;
			long directParam;
			memset(&res, 0, sizeof(res));
			res.result = 1;
			res.msgID = 0;
			res.gold = pPlayer->GetGold();
			// Official behavior: scripts must pass explicit exchange code.
			// Some tables may put the value in param_tbl[0] instead of param_tbl[1].
			directParam = (param_tbl[1] > 0) ? param_tbl[1] : ((param_tbl[0] > 0) ? param_tbl[0] : 0);
			if (directParam > 0)
				pageCode = directParam;
			else
			{
				// Compatibility: some NPC talk scripts pass 0 and rely on server-side inference.
				// Try to resolve by NPC + last button index (ExchangeData.txt pos mapping).
				pageCode = ResolveExchangeCodeByNpc(pPlayer->m_nTalkNPC_UID, pPlayer->m_nNPCTalkLastBtnIndex);
			}
			if (pageCode <= 0)
			{
				// No code -> signal client to close exchange UI.
				res.load = 0;
				::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_BATTLE_EXCHANGE_RESULT, (char*)&res, sizeof(res), 0);
				break;
			}
			res.load = (unsigned long)pageCode;
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_BATTLE_EXCHANGE_RESULT, (char*)&res, sizeof(res), 0);
		}
		break;

	default:
		CMapServer::GetServer()->Log(
			"Error: NPC talk not exist script command id(%d), msg_id(%d), npc_uid(%u), btn_idx(%d), subid(%d), params=[%d,%d,%d,%d,%d,%d,%d,%d]",
			script_id,
			msg_id,
			(unsigned int)(pPlayer ? pPlayer->m_nTalkNPC_UID : 0),
			(int)(pPlayer ? pPlayer->m_nNPCTalkLastBtnIndex : 0),
			(int)CMapServer::GetServer()->m_nMapServerSubID,
			param_tbl[0], param_tbl[1], param_tbl[2], param_tbl[3],
			param_tbl[4], param_tbl[5], param_tbl[6], param_tbl[7]
		);
		break;
	}
ret001:
	return(0);
}

void CMapPlayer::SetEnterStageTimeData()
{
	struct gs_StageData * pStage;
	long is_mark = 0;

	if (TowerSlashServer_IsPlayerInSession(this))
		return;

	// �����аO���O�a��
	if (m_nPlayerFlags & PLAYER_FLAGS_ENTER_STAGE)
	{
		is_mark++;
		m_nPlayerFlags &= ~PLAYER_FLAGS_ENTER_STAGE;
	}
	//
	//if (GetSaveData()->plrEnterStageID == GetMapID())
	if (gamePayStageIsEnterStageMap(GetSaveData()->plrEnterStageID, GetMapID()))
	{
// #ifndef USE_PACKAGE_DATA
//	CMapServer::GetServer()->Log("DEBUG: Enter stage time(%s,%d)", GetName(), GetSaveData()->plrEnterStageTime);
// #endif
		{
// #ifndef USE_PACKAGE_DATA
//			CMapServer::GetServer()->Log("DEBUG: Enter stage id(%s,%d)", GetName(), GetSaveData()->plrEnterStageID);
// #endif
			pStage = gameStageGetPtr(GetMapID());
			//
			if (GetSaveData()->plrEnterStageTime > 0)
			{
				long map_code, x, y, xrange, yrange;
				CMapCharMsg * pMsg;

				//pStage = gameStageGetPtr(GetMapID());
				if (pStage->gstMapPos[0])
				{
					// �]�wclient��ܭ˼ƭp��
					pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
					if (pMsg)
					{
						x = GetSaveData()->plrEnterStageTime;
						if (x < 1)
							x = 1;
						//
						pMsg->m_Msg.m_UpdateData2Msg.Reset();
						pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_MAPSPACE_NOTIFY, 1, x, GetMapID()); // Client �۰ʰ���˼ƭp��

						pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					}
					//
					m_nTeleportDueTime = CMapServer::GetServer()->GetLoopTime() + GetSaveData()->plrEnterStageTime;
					//
set_teleport:
					map_code = (pStage->gstMapPos[0] & ~0x80000000);
					x = (pStage->gstMapPos[3] + pStage->gstMapPos[1]) / 2;
					y = (pStage->gstMapPos[4] + pStage->gstMapPos[2]) / 2;
					xrange = pStage->gstMapPos[3] - pStage->gstMapPos[1] + 1;
					yrange = pStage->gstMapPos[4] - pStage->gstMapPos[2] + 1;
					//
					x = x + gameGetRandomRange(0, xrange) - (xrange/2);
					y = y + gameGetRandomRange(0, yrange) - (yrange/2);
					//
					m_nTeleportMapCode = (unsigned short)map_code;
					m_nTeleportPosX = x;
					m_nTeleportPosY = y;
					// �аO���O�a��
					m_nPlayerFlags |= PLAYER_FLAGS_ENTER_STAGE;
					if (!is_mark)
					{
						is_mark++;
					}
					else
						is_mark = 0;
				}
				else
					CMapServer::GetServer()->Log("ERROR: Player(%s) enter stage time but no teleport setting(%d) !", GetName(), GetMapID());
			}
			else
			{
				if (!IsGM(0))	// gm�i�H�d�b�̭�
				{
					GetSaveData()->plrEnterStageTime = 0;
					AutoSaveCharData();
					// Bug: �Q�ζi�J�ɶ��t�y���ɶ��L��
					// �]�w�ǥX�a��
					if (pStage->gstMapPos[0])
					{
						m_nTeleportDueTime = CMapServer::GetServer()->GetLoopTime();
						goto set_teleport;
					}
				}
			}
		}
	}
	// �i�J/���}���O�a�Ϯɧ�s�ɶ�
	if (is_mark)
		EnterStage_SendClientTimeNotify(this);
}

// �H NPC Talk ID �i�J�A�^�ǤU�@�� NPC Talk ID, 0 ���ܵ���, -1 ���ݵ��G, -2 �������
long CMapPlayer::gameServer_NPCTalkProcess1(long npc_talk_id)
{
	long r;

	npcTalk_pPlayer = this;
	r = gameServerNPCTalkProcess1(npc_talk_id, (long *)&m_nNPCSelectID, CB_NPCTalkCommand);
	return(r);
}

// �H NPC Talk ID �i�J�A�^�ǤU�@�� NPC Talk ID, 0 ���ܵ���, -1 ���ݵ��G, -2 �������
long CMapPlayer::gameServer_NPCTalkProcess2(long npc_talk_id, long select_id)
{
	long r;
	int i2;

	npcTalk_pPlayer = this;
	g_NpcTalk_CurrentSelectId = select_id;
	g_NpcTalk_CurrentBtnIndex = 0;
	m_nNPCTalkLastBtnIndex = 0;
	if (select_id > 0)
	{
		for (i2 = 0; i2 < 4; i2++)
		{
			if (select_id == m_nNPCSelectID[i2])
			{
				g_NpcTalk_CurrentBtnIndex = i2 + 1;	// 1-based
				m_nNPCTalkLastBtnIndex = g_NpcTalk_CurrentBtnIndex;
				break;
			}
		}
	}
	r = gameServerNPCTalkProcess2(select_id, npc_talk_id, (long *)&m_nNPCSelectID, CB_NPCTalkCommand);
	g_NpcTalk_CurrentSelectId = 0;
	g_NpcTalk_CurrentBtnIndex = 0;
	return(r);
}

// �]�w�Y���ȥi���Ʊ�, repeat_time = 0 �M��
void CMapPlayer::SetMissionRepeat(long id, long repeat_time)
{
	long i, size;
	struct plrMISSION_SAVE * pMIS;

	if (id)
	{
		pMIS = GetMissionData();
		if (!repeat_time)
		{
			for (i=0; i<gameMAX_MISSION; i++)
			{
				if (pMIS->pmRepeat[i].pmdUID == id)
				{
					pMIS->pmRepeat[i].pmdUID = 0;
					// ���e��
					if (i < (gameMAX_MISSION - 1))
					{
						size = ((gameMAX_MISSION - 1) - i) * sizeof(struct plrMISSION_DETAIL2);
						memcpyT(&pMIS->pmRepeat[i], &pMIS->pmRepeat[i+1], size);
					}
					pMIS->pmRepeat[gameMAX_MISSION-1].pmdUID = 0;
					return;
				}
			}
		}
		else
		{
			for (i=0; i<gameMAX_MISSION; i++)
			{
				if (pMIS->pmRepeat[i].pmdUID == id)		// �w�]�w�L����
					return;
				if (!pMIS->pmRepeat[i].pmdUID)
				{
					pMIS->pmRepeat[i].pmdUID = (unsigned short)id;
					pMIS->pmRepeat[i].pmdLeftTime = GetSaveData()->plrPlayTime + repeat_time;
					return;
				}
			}
		}
	}
}

// �ץ����e���x�s���~, �̷ӳ]�w�ˬd repeat ���
void CMapPlayer::FixMissionRepeat()
{
	long i, cnt, id, find, flag;
	struct gameMISSION_DATA * pMissionTbl;
	struct plrMISSION_SAVE * pMIS;

	flag = 0;
	pMIS = GetMissionData();
	for (cnt=1; cnt<=gameMAX_MISSION; cnt++)
	{
		pMissionTbl = gameGetMissionTablePtr(cnt);
		if (pMissionTbl)
		{
			if (pMissionTbl->gmdRepeat)		// ���Ȧ� repeat �]�w
			{
				find = 0;
				for (i=0; i<gameMAX_MISSION; i++)
				{
					id = pMIS->pmRepeat[i].pmdUID;
					if (!id)
						break;
					//
					if (id == cnt)
					{
						find++;
						break;
					}
				}
				if (!find)					// �M�����A
				{
					pMIS->pmStatus[cnt] = 0;
					flag |= 1;
				}
			}
		}
	}
	//
	if (flag)
	{
		SaveMissionData(0);		// �x�s
		//
		struct plrMISSION sendData;

		::memset(&sendData, 0, sizeof(sendData));
		::gameServer_MakeMission(pMIS, &sendData);
		::msgSendData( GetClientID(), 0, PROTO_MAP_SEND_MISSION_DATA, (char *)&sendData, sizeof(sendData), 0 );
	}
}

// �B�z�i���Ʊ����Ȯɶ�
void CMapPlayer::ProcessMissionRepeat()
{
	long i, size;
	long update_flag = 0;
	unsigned long cur_play_time;
	unsigned long dest_time;
	struct plrMISSION_SAVE * pMIS;

	cur_play_time = GetSaveData()->plrPlayTime;
	pMIS = GetMissionData();
	for (i=0; i<gameMAX_MISSION; i++)
	{
rpt:	if (!pMIS->pmRepeat[i].pmdUID)
			break;
		dest_time = pMIS->pmRepeat[i].pmdLeftTime;
		if (!((dest_time ^ cur_play_time) & 0x80000000))
		{
			if (cur_play_time > dest_time)
			{
				pMIS->pmStatus[pMIS->pmRepeat[i].pmdUID] = 0;
				update_flag |= 1;
				// ���e��
				if (i < (gameMAX_MISSION - 1))
				{
					size = ((gameMAX_MISSION - 1) - i) * sizeof(struct plrMISSION_DETAIL2);
					memcpyT(&pMIS->pmRepeat[i], &pMIS->pmRepeat[i+1], size);
				}
				pMIS->pmRepeat[gameMAX_MISSION-1].pmdUID = 0;
				goto rpt;
			}
		}
	}
	// ��s����
	if (update_flag)
	{
		SaveMissionData(0);		// �x�s
		//
		struct plrMISSION sendData;

		::memset(&sendData, 0, sizeof(sendData));
		::gameServer_MakeMission(pMIS, &sendData);
		::msgSendData( GetClientID(), 0, PROTO_MAP_SEND_MISSION_DATA, (char *)&sendData, sizeof(sendData), 0 );
	}
}

// �Y�ĤH�O�_�O�������Ȫ��~
long CMapPlayer::CheckDropMissionItem(unsigned long enemy_id)
{
	long i;
	struct plrMISSION_SAVE * pMIS;
	unsigned long boss_code;

	pMIS = GetMissionData();
	for (i=0; i<gameMAX_MISSION_ACCEPT; i++)
	{
		if (pMIS->pmData[i].pmdUID)
		{
			if (boss_code = pMIS->pmData[i].pmdEnemyID)
			{
				if (boss_code == enemy_id)
				{
					return(1);
				}
				else
				{
					if (CMapServer::GetServer()->GetMapManager()->CheckArmyMissionBoss(boss_code, enemy_id))
						return(1);
				}
			}
		}
	}
	return(0);
}

// �缾�a�e�X�T��(gameChannel_System)
void CMapPlayer::SendMessage(long channel, char *talker, char *msg)
{
	//struct MAP_SEND_MESSAGE_TO_CLIENT nData;
	struct MAP_SYSTEM_MESSAGE nData;

	if (msg && *msg)
	{
	//	memset(&nReq, 0, sizeof(nReq));
		nData.m_nUID = GetUID();
		nData.m_nTalkerUID = 0;
		nData.m_nChannel = channel;	// �ثe�@�w�O�t�ΩΤ��i�W�D
	//	nData.m_nChannel = gameChannel_System;
		if (talker)
		{
			msg_strncpy(nData.m_TalkerName, talker, sizeof(nData.m_TalkerName));
		}
		else
			nData.m_TalkerName[0] = 0;
		msg_strncpy(nData.m_Message, msg, sizeof(nData.m_Message));
		nData.m_nMessageSize = strlen(nData.m_Message);
		//
		//::msgSendData( GetClientID(), 0, PROTO_MAP_SEND_MESSAGE_TO_CLIENT, (char *)&nData, nData.GetSize(), 0 );
		::msgSendData( GetClientID(), 0, PROTO_MAP_SYSTEM_MESSAGE, (char *)&nData, nData.GetSize(), 0 );
	}
}

// ����ĸS
// return: 0 �ɶ�����
//		   1 �ǰe�� Login Server �P�_
//		   2 �L�x��
long CMapPlayer::GetPrebend()
{
	long i;
	long hLoginServer;
	struct LOGIN_CITY_DATA_PACK nData;

	i = CMapServer::GetServer()->GetLoopTime();
	if ((unsigned long)i < (unsigned long)GetSaveData()->plrTime_OfficeGift)
		return(0);
	if (!GetSaveData()->plrOrganizeUID)
		return(2);
	//
	memset(&nData, 0, sizeof(nData));
	nData.nType = TYPE_PACK_GET_PREBEND;
	nData.nData1 = GetUID();
	nData.nData2 = GetMapCode();
	//
	hLoginServer = CMapServer::GetServer()->GetLoginServer();
	CMapServer::GetServer()->SendData(hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nData, sizeof(nData), 0);
	return(1);
}

// return: 0 �����`���~
long CMapPlayer::GetPrebendResult(long num_force)
{
	long i, num, id;
	struct gameOFFICER_DATA * pOffice;
	CMapPlayer * pPlayer;
	CMapCharMsg * pMsg;
	struct tm * ptm;
	struct tm ntm;

	i = CMapServer::GetServer()->GetLoopTime();
	if ((unsigned long)i < (unsigned long)GetSaveData()->plrTime_OfficeGift)
		return(1);
	pPlayer = this;
	// �o��X�~�x
	num = 0;
	pOffice = gameGetOfficerPtr(pPlayer->GetSaveData()->plrOffice);
	if (pOffice)
		num = pOffice->odPin;
	//
	switch(num)
	{
	case 1:
		id = 5591; // �@�~
		break;
	case 2:
		id = 5592;
		break;
	case 3:
		id = 5593;
		break;
	case 4:
		id = 5594;
		break;
	case 5:
		id = 5595;
		break;
	case 6:
		id = 5596;
		break;
	case 7:
		id = 5597;
		break;
	case 8:
		id = 5598;
		break;
	case 9:
		id = 5599;
		break;
	case 0:
	default:
		id = 5600;
		break;
	}

	int check_count = 0;
	POINT objex[6];
	check_count = PrevendExXml::GetObjectForOffice(num, objex, sizeof(objex) / sizeof(objex[0]));
	//
#ifdef USE_PREBEND_MANY_MODE	// �ĸS 2�ӡA�ۥ߶դO 3��
	num = 2;
	//
	i = pPlayer->GetCountryID();
	if (i >= ID_COUNTRY_FORCE01)
	{
		i -= ID_COUNTRY_FORCE01;
		if (i < gameMAX_PLAYER_COUNTRY_ID)
			num = 3;
	}
#else
	num = 1;
#endif
	//
	if (num_force)
		num = num_force;
	//
	int j = check_count + 1;
	for(i = 0 ; (i < gameServer_GetCarryItemMax(GetSaveData())) && j; i++)
	{
		if (GetItemData()->plrCarryItem[i].m_Item.itemCode == 0)
			j--;
	}

	if (j)
	{
		pPlayer->SendMsgToClientByID(20150); //�Ŷ�����
		return 0;
	}
	//
	if (pPlayer->MakeItem(id, num, LOGTYPE_ITEM_FROM_NPC_GET, pPlayer))
	{
		pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETITEM_SHOW_ON_MAINBOARD, (short)num, id, 0); // �ƥ� // item_id // flag

		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		// �j�ѭ��
		ptm = ::apiGetTimeStruct(0);
		memcpyT(&ntm, ptm, sizeof(ntm));
		ntm.tm_hour = 0;
		ntm.tm_min = 0;
		ntm.tm_sec = 0;
		i = (long)mktime(&ntm);
		i = i + (60 * 60 * 24);
		pPlayer->GetSaveData()->plrTime_OfficeGift = i;
		//
		//if (pPlayer->GetSaveData()->plrTime_OfficeGift & 0x80000000)
		//	pPlayer->GetSaveData()->plrTime_OfficeGift = 0x7fffffff;
//CMapServer::GetServer()->Log("NPC talk time(%d)(%d)", param_tbl[2], pPlayer->GetSaveData()->plrTime_General);

		for(i = 0 ; i < check_count ; i++)
		{
			pPlayer->MakeItem(objex[i].x, objex[i].y, LOGTYPE_ITEM_FROM_NPC_GET, pPlayer);
			//
			pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETITEM_SHOW_ON_MAINBOARD, (short)objex[i].y, objex[i].x, 0); // �ƥ� // item_id // flag

			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}

		pPlayer->SaveCharData();
	}
	else
	{
		switch(pPlayer->MakeItem_GetErrorCode())
		{
		case INSERT_ITEM_OVER_LOAD: //�L��
			pPlayer->SendMsgToClientByID( 20141 );
			break;
		case INSERT_ITEM_NO_SPACE: //�Ŷ�����
			pPlayer->SendMsgToClientByID( 20150 );
			break;
		}
		return(0);
	}
	return(1);
}

// ���ޭt���A�u�ަ��S���Ŷ�
long CMapPlayer::CarryItem_CheckFreeSpace(long number)
{
	struct plrCARRYITEM_SAVE * pItem;
	long i, total;
	long val;

	pItem = GetItemData();
	total = 0;
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(GetSaveData());
	for (i=0; i<val; i++)
	{
		if (!pItem->plrCarryItem[i].m_Item.itemCode)
		{
			total++;
			if (total >= number)
				return(1);
		}
	}
	return(0);
}

void CMapPlayer::SendLog_GoldHonorExp(long log_type, long item_id, long val, CMapChar * Target)
{
	// ���� Log Server
	struct itemDATA_SAVE logItem;

	switch(item_id)
	{
	case item_Money:
	case item_Exp:
	case item_SkillExp:
	case item_Honor:
		memset(&logItem, 0, sizeof(logItem));
		logItem.m_Item.itemCode = (unsigned short)item_id;
		logItem.m_Item.itemGoldNumber = val;
		CMapServer::GetServer()->SendLogMessage_Item(this, log_type, Target, &logItem);
		break;
	}
}

// �s�W���~�|�s�ɡA����t�����s��
// mode = �S���@��: 1 �˳��� +5 ~ +10
long CMapPlayer::CarryItem_MakeItemOnFreeSpace(unsigned long nItemID, long nCount, long log_type, long is_random_val, CMapPlayer * pLogFrom, long is_record_result, long carry_idx, long mode)
{
	struct plrCARRYITEM_SAVE * pItemTbl;
	long sourcePosID;
	unsigned long long total_gold;
	//
	struct itemDATA * pItem;
	struct itemDATASHOWSELF newItem;
	struct itemDATA_SAVE * pNewCarryItem;
	long cell_max, val;
	//
//	unsigned long weight;
	CMapCharMsg * pMsg;
	long duedate;

	if (!nItemID)
		return(0);
	if (nCount <= 0)
		return(0);

//CMapServer::GetServer()->Log("VT: Get code %d, %d", nItemID, nCount);

	if ((nItemID & 0xffff) == (item_Money & 0xffff))
	{
		total_gold = GetSaveData()->plrGold + (unsigned long long)nCount;
		if (total_gold > gameMAX_GOLD)
			total_gold = gameMAX_GOLD;
		GetSaveData()->plrGold = total_gold;
		//
		if (is_record_result)
		{
			PlayerDrop_SetGold(nCount, total_gold);
		}
		// ���� Log Server
		if (log_type != -1)
		{
			struct itemDATA_SAVE logItem;
			memset(&logItem, 0, sizeof(logItem));
			logItem.m_Item.itemCode = (unsigned short)item_Money;
			logItem.m_Item.itemGoldNumber = nCount;
			CMapServer::GetServer()->SendLogMessage_Item(this, log_type, (CMapChar *)this, &logItem);
		}
		//
		SaveCharData();
	}
	else	// �j����~
	{
		pItem = ::gameGetItemPtr(nItemID);
		if (!pItem)
			return(0);
		// �ƶq����
		if ( ::IsItemMergable( nItemID ) )
		{
			cell_max = gameMAX_ITEM;
		}
		else
			cell_max = 1;
		if (nCount > cell_max)
			nCount = cell_max;
		//
		pItemTbl = GetItemData();
		//for (sourcePosID=0; sourcePosID<gameMAX_CARRYITEM; sourcePosID++)
		val = ::gameServer_GetCarryItemMax(GetSaveData());
		// ���w��m
		if (carry_idx != -1)
		{
			if ((carry_idx >= 0) && (carry_idx < val))
			{
				pNewCarryItem = &pItemTbl->plrCarryItem[carry_idx];
				if (!pNewCarryItem->m_Item.itemCode)
				{
					sourcePosID = carry_idx;
					goto do_make;
				}
			}
		}
		//
		for (sourcePosID=0; sourcePosID<val; sourcePosID++)
		{
			pNewCarryItem = &pItemTbl->plrCarryItem[sourcePosID];
			if (!pNewCarryItem->m_Item.itemCode)
			{
do_make:
#ifdef DEBUG_TRACE_DUPE_ITEM_PLAYER
				char * name = GetName();

				if ((name[0] == ' ') || _mbsstr((const unsigned char *)GetName(), (const unsigned char *)"�"))
					CMapServer::GetServer()->Log("Notice: Special player make item(%s,%s)(%d,%d) !!!", GetName(), GetSaveData()->plrAccount, nItemID, nCount);
#endif
				::memset(&newItem, 0, sizeof(newItem));
				//
				//if( pItem->itemType & (itemTypeSoldier | itemTypeSiegeWeapon) ) //�L��
				if (gameIsSoldierItemByPtr(pItem, FLAG_SIT_ALL))
				{
				//	if(szSoldierName)
				//		::gameCreateItem_NPC(&newItem, itemID, pMapServer->GenerateItemUID(), pPlayer->plrSaveData.plrUID, szSoldierName);
				//	else
					::gameCreateItem_NPC(&newItem, nItemID, CMapServer::GetServer()->GenerateItemUID(), GetSaveData()->plrUID, "");
				}
				else if( pItem->itemType & itemTypeSoul )	// �Z��
				{
				//	duedate = CMapServer::GetServer()->Soul_GetSoulItemDuedate();
				//	if (!duedate)
					{
						duedate = (long)::time(NULL) + SOUL_ITEM_DEFAULT_TIME;	// �w�]�T��
					}
				//	else
				//	{
				//		// ���ѨM�ɿ�ɶ����D�A�o�̱j�w�H���Ѻ�_��7��12�I
				//		struct tm * ptm;
				//		struct tm ntm;

				//		ptm = ::apiGetTimeStruct(0);
				//		memcpyT(&ntm, ptm, sizeof(ntm));
				//		//
				//		ntm.tm_hour = 12;		// �Z��~�G���Ĥ鬰����
				//		ntm.tm_min = 0;
				//		ntm.tm_sec = 0;
				//		duedate = mktime(&ntm);
				//		duedate += (60*60*24*7);	// 7 �ѫ�
				//	}
					::gameCreateItem_Soul(&newItem, nItemID, CMapServer::GetServer()->GenerateItemUID(), duedate, GetSaveData()->plrUID, GetSaveData()->plrName);
				}
				else if( pItem->itemType2 & itemType2Tiger )	// ���
				{
					::gameCreateItem_Tiger(&newItem, nItemID, CMapServer::GetServer()->GenerateItemUID(), GetSaveData()->plrUID, GetSaveData()->plrName);
				}
				else
				{
					::gameCreateItem(&newItem, nItemID, CMapServer::GetServer()->GenerateItemUID(), nCount, GetSaveData()->plrUID, GetSaveData()->plrName);
					if (is_random_val)
						::gameSetCreateItemRandomVal(&newItem); //�]�w�ü��ݩ�
				}
				//
				if( newItem.m_Item.itemShowData.itemCode == 0 ) 
					return(NULL); //�إߪ��~����
				//
				if (mode == 1)		// �˳��� +5 ~ +10
				{
					if (pItem->itemType & itemTypeALL_ARMOR)
					{
						if (!(pItem->itemType & itemTypeOther))
							::gameSetCreateItemRandomBless(&newItem, 3, 8); //�]�w�[��
					}
					else if (pItem->itemType & itemTypeALL_WEAPON)
					{
						::gameSetCreateItemRandomBless(&newItem, 5, 10);	//�]�w�[��
					}
				}
				//
				::gameServer_ItemSave_MakeShowSelf(&newItem, pNewCarryItem);
				// �t������
				//weight = ::gameItem_CalcWeight(nItemID, nCount);
				//GetCharData()->plrWeight += weight;
				gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
				//�^�ǭt��
				pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_WEIGHT, 0, GetCharData()->plrWeight);

				pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				// �s��
				//SaveCharData();
				SaveSingleItemData( sourcePosID );
				// �^�� Client ��s
				struct MAP_GET_PLAYER_ITEM_RESULT sendData2;

				sendData2.itemIdx = sourcePosID;
				::gameServer_Item_MakeShowSelf(pNewCarryItem, &newItem);
				::memcpyT( &sendData2.itemShowSelf, &newItem, sizeof(sendData2.itemShowSelf) );
				::msgSendData( GetClientID(), 0, PROTO_MAP_GET_PLAYER_ITEM_RESULT, (char *)&sendData2, sizeof(sendData2), 0 );
				//
				if (is_record_result)
					PlayerDrop_SetItem(nItemID, nCount);
				//
				// �glog
				if ( log_type != -1 ) 
				{
					struct itemDATA_SAVE itemSave;
					::memset( &itemSave, 0, sizeof(itemSave) );
					::memcpyT( &itemSave, pNewCarryItem, sizeof(itemSave) );
					itemSave.m_Item.itemNumber = (unsigned short)nCount;
					CMapServer::GetServer()->SendLogMessage_Item( pLogFrom, log_type, this, &itemSave );
				}
				//
				if (IsWawaTypeItem(nItemID) & 2)
				{
					DuedateItem_Init();
				}
 #ifdef USE_DUEDATE_ITEM
				else
				{
					if (gameGetItemType2ByID(nItemID) & itemType2DUEDATE)
						DuedateItem_Init();
				}
 #endif
				//return(1);
				if (!nCount)
					nCount++;
				return(nCount);
			}
		}
	}
	return(0);
}

// �i���|�B���O���q(�����y���~��)
long CMapPlayer::CarryItem_MakeItemNoWeight(unsigned long nItemID, long nCount, long log_type, long is_random_val, CMapPlayer * pLogFrom)
{
	struct plrCARRYITEM_SAVE * pItemTbl;
	struct itemDATA * pItem;
	long i, val, n, is_update, is_emul, old_count;
	struct itemDATA_SAVE * pNewCarryItem;
	struct itemDATA_SAVE * pNewEmptyCarryItem;
	long nEmptyIdx;
	struct itemDATASHOWSELF newItem;
	struct MAP_GET_PLAYER_ITEM_RESULT sendItemData;
	CMapCharMsg * pMsg;

	pItem = ::gameGetItemPtr(nItemID);
	if (!pItem || (nCount <= 0))
		return(0);
	// �ƶq����
	if ( !::IsItemMergable( nItemID ) )
		return(CarryItem_MakeItemOnFreeSpace(nItemID, nCount, log_type, is_random_val, pLogFrom, 0));
	//
	if (nCount > gameMAX_ITEM)
		nCount = gameMAX_ITEM;
	//
	pItemTbl = GetItemData();
	//for (sourcePosID=0; sourcePosID<gameMAX_CARRYITEM; sourcePosID++)
	val = ::gameServer_GetCarryItemMax(GetSaveData());
	pNewEmptyCarryItem = NULL;
	nEmptyIdx = -1;
	is_update = 0;
	is_emul = 1;			// �������A�קK����
	old_count = nCount;		// �O�d
	//
redo:
	for (i=0; i<val; i++)
	{
		pNewCarryItem = &pItemTbl->plrCarryItem[i];
		if (!pNewCarryItem->m_Item.itemCode)
		{
			if (!pNewEmptyCarryItem)
			{
				pNewEmptyCarryItem = pNewCarryItem;		// ����
				nEmptyIdx = i;
			}
		}
		else
		{
			if ((unsigned long)pNewCarryItem->m_Item.itemCode == nItemID)
			{
				if (pNewCarryItem->m_Item.itemNumber < gameMAX_ITEM)
				{
					n = pNewCarryItem->m_Item.itemNumber + nCount;
					if (n > gameMAX_ITEM)
					{
						nCount -= (n - gameMAX_ITEM);
					}
					else
						nCount = 0;
					if (!is_emul)
					{
						pNewCarryItem->m_Item.itemNumber = (unsigned short)n;
						SaveSingleItemData( i );
						is_update |= 1;
						// Log
						CMapServer::GetServer()->SendLogMessage_Item( pLogFrom, log_type, this, pNewCarryItem );
						// �^�� Client ��s
						sendItemData.itemIdx = i;
						::gameServer_Item_MakeShowSelf(pNewCarryItem, &newItem);
						::memcpyT( &sendItemData.itemShowSelf, &newItem, sizeof(sendItemData.itemShowSelf) );
						::msgSendData( GetClientID(), 0, PROTO_MAP_GET_PLAYER_ITEM_RESULT, (char *)&sendItemData, sizeof(sendItemData), 0 );
					}
				}
			}
		}
		//
		if (nCount <= 0)
			break;
	}
	//
	if (is_emul)
	{
		if (nCount > 0)						// ���ѤU��
		{
			if (!pNewEmptyCarryItem)		// �L�Ŧ�
				goto dok;
		}
		//
		is_emul = 0;
		nCount = old_count;			// �_��
		goto redo;
	}
	//
	if (nCount > 0)
	{
		if (pNewEmptyCarryItem)		// ���Ŧ�
		{
			::gameCreateItem(&newItem, nItemID, CMapServer::GetServer()->GenerateItemUID(), nCount, GetSaveData()->plrUID, GetSaveData()->plrName);
			if (is_random_val)
				::gameSetCreateItemRandomVal(&newItem);			//�]�w�ü��ݩ�
			if ( newItem.m_Item.itemShowData.itemCode != 0 )
			{
				::gameServer_ItemSave_MakeShowSelf(&newItem, pNewEmptyCarryItem);
				//
				SaveSingleItemData( nEmptyIdx );
				is_update |= 1;
				// Log
				CMapServer::GetServer()->SendLogMessage_Item( pLogFrom, log_type, this, pNewEmptyCarryItem );
				// �^�� Client ��s
				sendItemData.itemIdx = nEmptyIdx;
				::gameServer_Item_MakeShowSelf(pNewEmptyCarryItem, &newItem);
				::memcpyT( &sendItemData.itemShowSelf, &newItem, sizeof(sendItemData.itemShowSelf) );
				::msgSendData( GetClientID(), 0, PROTO_MAP_GET_PLAYER_ITEM_RESULT, (char *)&sendItemData, sizeof(sendItemData), 0 );
			}
		}
	}
	//
	if (is_update)
	{
		// �t������
		gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
		//�^�ǭt��
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_WEIGHT, 0, GetCharData()->plrWeight);

		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		//
		return(1);
	}
dok:
	return(0);
}

long CMapPlayer::CarryItem_CheckMakeItemEnoughSpace(unsigned long nItemID, long nCount, long & nReason)
{
	struct plrCARRYITEM_SAVE * pItemTbl;
	struct itemDATA * pItem;
	long i, carry_item_max, old_count, empty_stackspace_count;
	long can_stack_count = 0;
	long item_index = 0;
	struct itemDATA_SAVE * pNewCarryItem;
	
	queue<long> exist_and_nonfull_item_index;
	queue<long> empty_item_index;

	pItemTbl = GetItemData();
	carry_item_max = ::gameServer_GetCarryItemMax(GetSaveData());
	old_count = nCount;
	empty_stackspace_count = 0;
	nReason = 0;

	pItem = ::gameGetItemPtr(nItemID);
	if (!pItem || (nCount <= 0))
	{
		// ���~��ƿ��~
		nReason = 1;
		return(0);
	}

#ifdef NO_CHECK_WEIGHT
#else
	if ((GetCharData()->plrWeight+nCount*pItem->itemWeight)>(GetCharData()->plrWeightMax))
	{
		// �t���L��
		nReason = 2;
		return(0);
	}
#endif

	// �ƶq����
	// ���i���|���~
	if ( !::IsItemMergable( nItemID ) )
	{
		for (i=0; i<carry_item_max; i++)
		{
			pNewCarryItem = &pItemTbl->plrCarryItem[i];
			if (!pNewCarryItem->m_Item.itemCode)
			{
				empty_item_index.push(i);
				if (empty_item_index.size() >= (unsigned long)nCount)
				{
					return (1);
				}
			}
		}
	}
	// �i���|���~
	else
	{
		for (i=0; i<carry_item_max; i++)
		{
			pNewCarryItem = &pItemTbl->plrCarryItem[i];
			if (!pNewCarryItem->m_Item.itemCode)
			{
				empty_item_index.push(i);
				if ((empty_stackspace_count+empty_item_index.size()*gameMAX_ITEM) >= (unsigned long)nCount)
				{
					return(1);
				}
			}
			else if (pNewCarryItem->m_Item.itemCode == nItemID && pNewCarryItem->m_Item.itemNumber < gameMAX_ITEM)
			{
				can_stack_count = gameMAX_ITEM - pNewCarryItem->m_Item.itemNumber;
				if (can_stack_count >= 1 && can_stack_count <= gameMAX_ITEM-1)
				{
					empty_stackspace_count += (can_stack_count);
					exist_and_nonfull_item_index.push(i);
					if ((empty_stackspace_count+empty_item_index.size()*gameMAX_ITEM) >= (unsigned long)nCount)
					{
						return(1);
					}
				}
				else
				{
					nReason = 3;
					return(0);
				}
			}
		}
	}
	nReason = 4;
	return (0);
}

long CMapPlayer::CarryItem_MakeItemMerge(unsigned long nItemID, long nCount, long log_type, long is_random_val, CMapPlayer * pLogFrom, long is_record_result, long mode)
{
	struct plrCARRYITEM_SAVE * pItemTbl;
	struct itemDATA * pItem;
	long i, carry_item_max, old_count, empty_stackspace_count;
	long can_stack_count = 0;
	long item_index = 0;
	long nEmptyIdx;
	struct itemDATA_SAVE * pNewCarryItem;
	struct itemDATA_SAVE * pNewEmptyCarryItem;
	
	struct itemDATASHOWSELF newItem;
	struct MAP_GET_PLAYER_ITEM_RESULT sendItemData;
	CMapCharMsg * pMsg;
	queue<long> exist_and_nonfull_item_index;
	queue<long> empty_item_index;
	queue<long> update_item_index;

	pItem = ::gameGetItemPtr(nItemID);
	if (!pItem || (nCount <= 0))
		return(0);

	// �ƶq����
	if ( !::IsItemMergable( nItemID ) )
		return(CarryItem_MakeItemOnFreeSpace(nItemID, nCount, log_type, is_random_val, pLogFrom, 0));

	pItemTbl = GetItemData();
	carry_item_max = ::gameServer_GetCarryItemMax(GetSaveData());
	pNewEmptyCarryItem = NULL;
	nEmptyIdx = -1;
	old_count = nCount;		// �O�d
	empty_stackspace_count = 0;

	for (i=0; i<carry_item_max; i++)
	{
		pNewCarryItem = &pItemTbl->plrCarryItem[i];
		if (!pNewCarryItem->m_Item.itemCode)
		{
			empty_item_index.push(i);
		}
		else if (pNewCarryItem->m_Item.itemCode == nItemID && pNewCarryItem->m_Item.itemNumber < gameMAX_ITEM)
		{
			can_stack_count = gameMAX_ITEM - pNewCarryItem->m_Item.itemNumber;
			if (can_stack_count >= 1 && can_stack_count <= gameMAX_ITEM-1)
			{
				empty_stackspace_count += (can_stack_count);
				exist_and_nonfull_item_index.push(i);
				if (empty_stackspace_count >= nCount)
				{
					break;
				}
			}
			else
			{
				return(0);
			}
		}
	}

	while (nCount > 0)
	{
		if (exist_and_nonfull_item_index.size() > 0)
		{
			item_index = exist_and_nonfull_item_index.front();
			exist_and_nonfull_item_index.pop();

			pNewCarryItem = &pItemTbl->plrCarryItem[item_index];
			can_stack_count = gameMAX_ITEM - pNewCarryItem->m_Item.itemNumber;
			if (can_stack_count > nCount)
			{
				can_stack_count = nCount;
			}
			pNewCarryItem->m_Item.itemNumber += (unsigned short)can_stack_count;
			nCount -= can_stack_count;

			update_item_index.push(item_index);
		}
		else if (empty_item_index.size() > 0)
		{
			item_index = empty_item_index.front();
			empty_item_index.pop();

			pNewCarryItem = &pItemTbl->plrCarryItem[item_index];
			can_stack_count = gameMAX_ITEM;
			if (can_stack_count > nCount)
			{
				can_stack_count = nCount;
			}
			::gameCreateItem(&newItem, nItemID, CMapServer::GetServer()->GenerateItemUID(), can_stack_count, GetSaveData()->plrUID, GetSaveData()->plrName);
			if (is_random_val)
			{
				::gameSetCreateItemRandomVal(&newItem);			//�]�w�ü��ݩ�
			}
			if ( newItem.m_Item.itemShowData.itemCode != 0 )
			{
				::gameServer_ItemSave_MakeShowSelf(&newItem, pNewCarryItem);
			}
			nCount -= can_stack_count;

			update_item_index.push(item_index);
		}
		else
		{
			break;
		}
	}

	if (update_item_index.size() > 0)
	{
		while (update_item_index.size() > 0)
		{
			nEmptyIdx = update_item_index.front();
			update_item_index.pop();

			SaveSingleItemData( nEmptyIdx );
			// Log
			CMapServer::GetServer()->SendLogMessage_Item( pLogFrom, log_type, this, pNewEmptyCarryItem );
			// �^�� Client ��s
			sendItemData.itemIdx = nEmptyIdx;
			pNewEmptyCarryItem = &pItemTbl->plrCarryItem[nEmptyIdx];
			::gameServer_Item_MakeShowSelf(pNewEmptyCarryItem, &newItem);
			::memcpyT( &sendItemData.itemShowSelf, &newItem, sizeof(sendItemData.itemShowSelf) );
			::msgSendData( GetClientID(), 0, PROTO_MAP_GET_PLAYER_ITEM_RESULT, (char *)&sendItemData, sizeof(sendItemData), 0 );
		}


		// �t������
		gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
		// �^�ǭt��
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_WEIGHT, 0, GetCharData()->plrWeight);

		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		//
		return(1);
	}
	return (0);
}

// ==========================================================
// �O�����a�Q��𪬺A, �u���Ǫ��缾�a��������
// ==========================================================
bool CMapPlayer::Hurt(long hAttacker, long long * pHurt, long is_X2Damage, long is_Block_Damage_Act)
{
	CMapChar * pAttacker;

	if(!CMapChar::Hurt(hAttacker, pHurt, is_X2Damage, is_Block_Damage_Act))
		return(false);

#ifndef NO_ROUND_ATTACK_EFFECT
	if (*pHurt >= 0)
	{
		pAttacker = CMapServer::GetServer()->FindAndCheckCharExist(hAttacker);
		if (pAttacker)
		{
			RoundAttacker_Add(pAttacker);
		}
	}
#endif
	return(true);
}

// ���a���ˮ`�ɧ�s
void CMapPlayer::RoundAttacker_Add(CMapChar * pChar)
{
	unsigned long curr_time;
	unsigned long uid, cuid;
	long i, free;

	if (pChar->IsKindOf(CMapNPC::CLASS_ID))
	{
		// �p�����[��
		if (pChar->m_ParentHandle == pChar->GetHandle())
		{
			RoundAttacker_Clear();
			return;
		}
		//
		curr_time = CMapServer::GetServer()->GetLoopTime();
		if (m_nRoundAttackerTime < (long)curr_time)
		{
			m_nRoundAttackerNumber = 1;
			m_nRoundAttackerTime = curr_time + gameROUND_ENEMY_RECORD_TIME;
			//
			uid = pChar->GetUID();
			memset(&m_nRoundAttacker, 0, sizeof(m_nRoundAttacker));
			m_nRoundAttacker[0] = uid;
		}
		else
		{
			m_nRoundAttackerTime = curr_time + gameROUND_ENEMY_RECORD_TIME;
			if (m_nRoundAttackerNumber < gameMAX_ROUND_ENEMY)
			{
				uid = pChar->GetUID();
				free = -1;
				for (i=0; i<gameMAX_ROUND_ENEMY; i++)
				//for (i=0; i<m_nRoundAttackerNumber; i++)
				{
					cuid = m_nRoundAttacker[i];
					if (cuid == uid)		// �P�@�ؼĤH���W�[�ƥ�
						return;
					if (!cuid)
						free = i;
				}
				//
				if (free != -1)
				{
					m_nRoundAttacker[free] = uid;
					m_nRoundAttackerNumber++;
				}
			}
		}
	}
}

// �ĤH���`�ɧ�s
void CMapPlayer::RoundAttacker_Del(CMapChar * pChar)
{
	unsigned long uid;
	long i;

	uid = pChar->GetUID();
	for (i=0; i<gameMAX_ROUND_ENEMY; i++)
	{
		if (m_nRoundAttacker[i] == uid)		// �P�@�ؼĤH���W�[�ƥ�
		{
			m_nRoundAttacker[i] = 0;
			if (m_nRoundAttackerNumber > 1)
				m_nRoundAttackerNumber--;
			break;
		}
	}
}

void CMapPlayer::RoundAttacker_Clear()
{
	m_nRoundAttackerNumber = 0;
	m_nRoundAttackerTime = 0;
	memset(&m_nRoundAttacker, 0, sizeof(m_nRoundAttacker));
}

long CMapPlayer::RoundAttacker_GetNumber()
{
//	CMapServer::GetServer()->Log("Round num = %d", m_nRoundAttackerNumber);
	return(m_nRoundAttackerNumber);
}

// �~���B�z
void CMapPlayer::SetBotRecord(long reason, long force, long time_mode)
{
#ifdef NO_AUTO_KICK_BOT
	return;
#endif
	struct LOGIN_MAP_FIND_BOT_NOTIFY nData;

	if (reason == BOT_REASON_ATTACK_BOT_ENEMY)
		CMapServer::GetServer()->Log("SERIOUS: (%d,%s)attack bot enemy !", GetUID(), GetName());
//CMapServer::GetServer()->Log("�~���O�� !");

	if (m_nBotCount < 3)		// �~�t
	{
		m_nBotReason[m_nBotCount] = (unsigned char)reason;
		//
		// �g�� Log Server
		if (!m_nBotCheckErrCount)
			CMapServer::GetServer()->SendLogMessage_System(this, LOGTYPE_SYS_BOT_COUNT);
		//
		if (force)
		{
			if (m_nBotCount < 2)
			{
				nData.nBotCheckType = GetSaveData()->plrBotCheckType;
				nData.nBotType = (unsigned char)reason;
				nData.nMapCode = GetMapCode();
				nData.uid = GetUID();
				msg_strncpy(nData.szName, GetName(), sizeof(nData.szName));
				msg_strncpy(nData.szAccount, GetSaveData()->plrAccount, sizeof(nData.szAccount));
				CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_MAP_FIND_BOT_NOTIFY, (char *)&nData, sizeof(nData), 0);
			}
			m_nBotCount = 3;
		}
		else
		{
			m_nBotCount++;
			//
			if (m_nBotCount == 2)	// �ĤG���~�q��
			{
			//	if (reason != BOT_REASON_PING_ERR2)
				{
notify_gm:			nData.nBotCheckType = GetSaveData()->plrBotCheckType;
					nData.nBotType = (unsigned char)reason;
					nData.nMapCode = GetMapCode();
					nData.uid = GetUID();
					msg_strncpy(nData.szName, GetName(), sizeof(nData.szName));
					msg_strncpy(nData.szAccount, GetSaveData()->plrAccount, sizeof(nData.szAccount));
					CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_MAP_FIND_BOT_NOTIFY, (char *)&nData, sizeof(nData), 0);
				}
			}
			return;
		}
	}
	else
	{
		if (reason == BOT_REASON_ATTACK_BOT_ENEMY)
			goto notify_gm;
	}
	//
// #ifdef LOCK_BOT_PLAYER
	if (!m_nBotKickTime)
	{
		if (!time_mode)
		{
			m_nBotKickTime = gameGetRandomRange(5, 30) * 1000;	// 5 ���� 30 ��
		}
		else
			m_nBotKickTime = gameGetRandomRange(30, 60) * 1000;	// 30 ���� 60 ��
		//
		m_nBotCheckDuedate = 0;
		// �{�w�� bot, �����ˬd
		//GetSaveData()->plrBotCheckType = 0;
		//AutoSaveCharData();
		m_nBotCheckErrCount = 1;
	}
// #else
//	m_nBotCheckDuedate = 0;
//	m_nBotCheckErrCount = 1;
// #endif
}

// �~���P�_
void CMapPlayer::CheckBOT(unsigned long rnd_code)
{
#if defined(CHECK_BOT_SO_SYSTEM)
	unsigned long ctype, ctimes;
	long newtimes;
//	long elapse_time, ok_time;

//	// ����B�z
//	if ((GetUID() & (DIV_PROCESS_MAGIC_NUM-1)) != CMapServer::GetServer()->nOptSerial_NPC)
//		return;
	if (!IsReady())				// ���}���ܡA���B�zOK
		return;					// ���b�n�J���ܡA�]OK
	ctype = GetSaveData()->plrBotCheckType;		// �O�_�n�ϥ��ˬd
	if (m_nBotCheckErrCount || !ctype)
		return;
	ctimes = GetSaveData()->plrBotTimes;
	//
	if (m_nBotCheckDuedate)
	{
		// �ץ�
		if (ctimes <= 1)
		{
			if (m_nBotLastCode == 0xffffffff)		// �O�_�M�W�����P
				m_nBotLastCode = gameGetB_Table(ctype, 1);
			goto clr;
		}
		// �����W�@������
		if (m_nBotLastCode == 0xffffffff)
		{
			if (!rnd_code)
			{
				m_nBotLastCode = rnd_code;
			}
			else
			{
				m_nBotLastCode = gameGetB_Table(ctype, ctimes);
				//
				m_nBotCheckDuedate = CMapServer::GetServer()->GetLoopTime() + gameBOT_UPDATE_TIMEOUT2;
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: ���� 8 ����(%s,%d)", GetName(), rnd_code);
  #endif
			}
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: �Ĥ@������(%s,%d)(%d)", GetName(), rnd_code, ctype);
  #endif
			if (m_nBotLastCode == rnd_code)
			{
				m_nBotCheckLastUpdateTime = 0;	// �Ĥ@�����L
				return;
			}
		}
		// �O�_�M�W�����P
		if (m_nBotLastCode == rnd_code)
		{
			// �O�_�U�ɥ�����
			if (CMapServer::GetServer()->GetLoopTime() > m_nBotCheckDuedate)
			{
				if (rnd_code == gameGetB_Table(ctype, 1))
					goto clr;
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: �U�ɥ�����(%s,%d)", GetName(), rnd_code);
  #endif
				m_nBotCheckDuedate = CMapServer::GetServer()->GetLoopTime() + gameBOT_UPDATE_TIMEOUT2;
				SetBotRecord(BOT_REASON_TIMEOUT);
				return;
			}
			return;
		}
		else
		{
			if (!m_nBotCheckLastUpdateTime)
			{
				m_nBotCheckLastUpdateTime = CMapServer::GetServer()->GetLoopTime();
			}
			else
			{
				// �O�_�ӧ֧���(1��������)
				if (abs(CMapServer::GetServer()->GetLoopTime() - m_nBotCheckLastUpdateTime) < 60)
				{
  #ifdef SHOW_BOT_LOG	// !!! �L�a�Ϯɷ|�o�ͤ@��
  CMapServer::GetServer()->Log("DEBUG: �u�ɶ�����(%s,%d)", GetName(), rnd_code);
  #endif
					SetBotRecord(BOT_REASON_CHANGE_FAST);
				}
			}
		}
		m_nBotLastCode = rnd_code;
		//
		newtimes = gameCheckB_Table(ctype, ctimes, rnd_code);
		if (newtimes == -1)		// ������ƿ��~�A�M���ˬd
		{
	clr:
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: �M��(%s)", GetName());
  #endif
			//GetSaveData()->plrBotCheckType = 0;
			GetSaveData()->plrBotTimes = 0;
			m_nBotCheckDuedate = 0;
			AutoSaveCharData();
		}
		else if (!newtimes)		// �䤣�� rnd_code
		{
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: �䤣��A�����~��(%s,%d,%d,%d)", GetName(), ctype, ctimes, rnd_code);
  #endif
			SetBotRecord(BOT_REASON_CODE_NOT_FOUND);
			// ���s�M��s��m(���ƨ�F�ɤ�����)
			if (!m_nBotCheckReFind)
			{
				m_nBotCheckReFind++;
				//
				newtimes = gameCheckB_Table(ctype, gameBOT_RNDTABLE_W, rnd_code);
				if (newtimes > 0)
				{
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: ���s�w��m(%d)", newtimes);
  #endif
					GetSaveData()->plrBotTimes = newtimes;
					AutoSaveCharData();
					goto redo;
				}
			}
		}
		else
		{
			// ���ƻ~�t�O�_�L�j(���ӭn�ߨ���)
			if (abs(ctimes - newtimes) > 2)
			{
				// �ɶ��O�_�X�z
				// ���i��GClient ���|�ۤv���L�üƦC��
			//	elapse_time = CMapServer::GetServer()->GetLoopTime() - (m_nBotCheckDuedate - gameBOT_UPDATE_TIMEOUT2);
			//	ok_time = 60 * abs(ctimes - newtimes);	// �̤֮ɶ�
			//	ok_time = gameBOT_UPDATE_TIMEOUT2 * abs(ctimes - newtimes);	// �̤j�ɶ�
			////	if (elapse_time > ok_time)
				{
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: ���ƻ~�t�L�j(%s,%d,%d,%d)", GetName(), rnd_code, ctimes, newtimes);
  #endif
					if (newtimes <= 1)	// ����̫�@�ӤF
					{
						CMapServer::GetServer()->Log("SERIOUS: ���ƻ~�t�L�j(%d,%d,%d)", rnd_code, ctimes, newtimes);
						SetBotRecord(BOT_REASON_CODE_SEQUENCE, 1);
					}
					else
						SetBotRecord(BOT_REASON_CODE_SEQUENCE);
				}
			}
			//
			if (newtimes == 1)		// �̫�@�ӤF
			{
				//if (ctimes > 1)		// �ثe�������ӭn�j�� 1(�W���ˬd�ߨ���)
					goto clr;
			}
			GetSaveData()->plrBotTimes = newtimes;
			AutoSaveCharData();
			//
redo:		m_nBotCheckDuedate = CMapServer::GetServer()->GetLoopTime() + gameBOT_UPDATE_TIMEOUT2;
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: ���� 8 ����(%s,%d)", GetName(), rnd_code);
  #endif
		}
	}
	else
	{
		if (m_nBotLastCode == 0xffffffff)		// �O�_�M�W�����P
			m_nBotLastCode = gameGetB_Table(ctype, 1);
		if (rnd_code != m_nBotLastCode)
		{
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: �üƸ�����(%s,%d,%d)", GetName(), rnd_code, m_nBotLastCode);
  #endif
			SetBotRecord(BOT_REASON_CODE_ERROR);
		}
  #ifdef SHOW_BOT_LOG
  CMapServer::GetServer()->Log("DEBUG: �üƸ�OK(%s,%d)", GetName(), rnd_code);
  #endif
	}
#endif
}

void CMapPlayer::SetMemoryCheckFlag(struct MAP_PLAYER_WALK_TO_CHECK * pData)
{
	if (m_nBotMemoryCheck & 1)
		return;
  #ifndef USE_PACKAGE_DATA
  CMapServer::GetServer()->Log("(%s)Magic check(%d,0x%08x,%d,0x%08x)", GetName(), pData->m_nType1, pData->m_nVal1, pData->m_nType2, pData->m_nVal2);
/*	if (BotCheckMemoryCheckVal(pData->m_nType1, pData->m_nVal1, 0))
		return;
	sts++;
	if (BotCheckMemoryCheckVal(pData->m_nType2, pData->m_nVal2, 0))
		return;
*/
	if ((!pData->m_nVal1) && (!pData->m_nVal2))
		return;
	//
	char tmpstr[1024];

	wsprintf(tmpstr, "Magic check code error");
	SendMessage(gameChannel_System, NULL, tmpstr);
  #else
/*	if (BotCheckMemoryCheckVal(pData->m_nType1, pData->m_nVal1, 1))
		return;
	sts++;
	if (BotCheckMemoryCheckVal(pData->m_nType2, pData->m_nVal2, 1))
		return;
*/
	if ((!pData->m_nVal1) && (!pData->m_nVal2))
		return;
  #endif
	// ���~
	CMapManager * pMapMgr = CMapServer::GetServer()->GetMapManager();
	if (pMapMgr)
	{
		if (!pMapMgr->CheckBotEnemyMapCode(GetMapCode()))
		{
			if (GetSaveData()->plrLevel <= 29)		// ���\����
				return;
		}
	}
	if (GetSaveData()->plrLevel <= 18)		// ���\����
		return;
	// ���~
	SetIsBotPlayer();
	// ��x�q��
	CMapServer::GetServer()->BOT_NotifyGM(this, BOT_REASON_MEM_CHECKSUM_ERR);
	//
	// �g�� Log Server
	if (m_nBotCount < 3)
	{
		m_nBotReason[m_nBotCount] = (unsigned char)BOT_REASON_MEM_CHECKSUM_ERR;
		CMapServer::GetServer()->SendLogMessage_System(this, LOGTYPE_SYS_BOT_COUNT);
		m_nBotReason[m_nBotCount] = 0;
	}
}

void CMapPlayer::SetIsBotPlayer()
{
	m_nBotMemoryCheck |= 1;
}

long CMapPlayer::IsBotPlayer()
{
	return(m_nBotMemoryCheck & 1);
}

void CMapPlayer::ClearLastActionTick(long type)
{
	m_nLastActionTick[type] = 0;
	m_nLastActionTickErrCount = 0;
}

// type = 0, 1, 2...
void CMapPlayer::CheckLastActionTick(long type, unsigned long ntick, unsigned long min_tick)
{
#ifdef CHECK_ATTACK_FREQUENCE
 #ifndef GBMode		// ²�骩�ثe���ˬd�����ɶ�
	long r, ntime;

	if (!IsBotPlayer())
	{
		if (min_tick)		// = 0 ���ˬd
		{
			r = 0;
			// �@�w�n����j
			ntime = CMapServer::GetServer()->GetLoopTime();
			if (ntime > m_nLastActionTickClearTime)
			{
				m_nLastActionTickClearTime = ntime + 60*10;		// 10 �����M���@������
				m_nLastActionTickErrCount = 0;
			}
			//
  #ifdef USE_PACKAGE_DATA
			if (ntime > m_nBotFakeProtocoLockTime)
  #endif
			{
				if (!ntick)
				{
	set_bot:		if (++m_nLastActionTickErrCount >= 20)
					{
						SetIsBotPlayer();
						CMapServer::GetServer()->Log("SERIOUS:(%s,%s) is bot player !", GetName(), GetSaveData()->plrAccount);
						return;
					}
  #ifndef USE_PACKAGE_DATA
  CMapServer::GetServer()->Log("Test SERIOUS:(%s,%s)(%d,%d,%d) set bot !", GetName(), GetSaveData()->plrAccount, r, min_tick, ntick);
  #endif
				}
				else
				{
					if (m_nLastActionTick[type])
					{
						if (ntick >= m_nLastActionTick[type])
						{
							r = ntick - m_nLastActionTick[type];
							if (r < min_tick)
								goto set_bot;
						}
						else
							goto set_bot;
					}
				}
			}
			// �M���䥦�����ȡA�קK�����P�I�k�椬�ʧ@�~�P���D
			// type �ثe�u�� 0,1
			m_nLastActionTick[type ^ 1] = 0;
			//
			m_nLastActionTick[type] = ntick;
		}
	}
 #endif
#endif
}

// ��Դ_�����`����
// ���`�ɬ����ɶ��s��
void CMapPlayer::CWar_SetResurrectTick()
{
	GetSaveData()->plrCWarDeadWaitTime = (unsigned short)gameGetCWarResurrectDelaySecCfg();
	AutoSaveCharData();
}

// �Ұʭp��
// type = 1 �W�������`
void CMapPlayer::CWar_ActiveResurrectTick(long type)
{
	unsigned long sec;
	char tmpstr[512];

	sec = GetSaveData()->plrCWarDeadWaitTime;
	if (sec)
	{
		long maxSec = gameGetCWarResurrectDelaySecCfg();
		if (sec > (unsigned long)maxSec)
		{
			sec = (unsigned long)maxSec;
			GetSaveData()->plrCWarDeadWaitTime = (unsigned short)sec;
			AutoSaveCharData();
		}
		m_nCWarResurrectTick = sec * 1000;
		//
		wsprintf(tmpstr, MSG_CWAR_RESURRECT_TIME, GetSaveData()->plrCWarDeadWaitTime);
		SendMessage(gameChannel_System, NULL, tmpstr);
		// �p�G���ͩR�ȡA���ܤ����`����
		m_nCWarResurrectType = false;
		pStatus_Set(effFun_INVISIBLE, 1);
		if (type)
		{
			m_nCWarResurrectType = true;
			SetHP(0);
		}
	}
}

// �Q�O�H�_���P�N��
void CMapPlayer::CWar_ClearResurrectTick()
{
//	if (m_nCWarResurrectTick)
	{
		GetSaveData()->plrCWarDeadWaitTime = 0;
		AutoSaveCharData();
		m_nCWarResurrectTick = 0;
//GetServer()->Log( "Clear resurrect wait time !" );
	}
}

// �p�ɳB�z
void CMapPlayer::CWar_ProcResurrectTick()
{
	unsigned long loop_tick;
	unsigned short sec;
	char tmpstr[512];

	if (m_nCWarResurrectTick)
	{
		if (!IsReady())
			return;
		loop_tick = CMapServer::GetServer()->GetLoopTickCount();
		if (m_nCWarResurrectTick >= loop_tick)
		{
			m_nCWarResurrectTick -= loop_tick;
		}
		else
			m_nCWarResurrectTick = 0;
		//
		sec = (unsigned short)(m_nCWarResurrectTick / 1000);
		if (sec != GetSaveData()->plrCWarDeadWaitTime)
		{
			GetSaveData()->plrCWarDeadWaitTime = sec;
			AutoSaveCharData();
			//
			if (sec)
			{
				if (!(sec % 5))		// ��Դ������`�An ����۰ʭ���
				{
					wsprintf(tmpstr, MSG_CWAR_RESURRECT_TIME, sec);
					SendMessage(gameChannel_System, NULL, tmpstr);
				}
			}
		}
		//
		if (!m_nCWarResurrectTick)	// �_���B�z
		{
			//if (GetCharData()->plrTempStatus & effFun_INVISIBLE)
			if (m_nCWarResurrectType)
			{
				pStatus_Clear(effFun_INVISIBLE, 1);
				if (IsDead())
					ResurrectImmed(0);
			}
			else
				CMapServer::GetServer()->PlayerResurrectToSavePos(this);
		}
	}
}

// �O�_����(�������ʨϥάI�k�۰ʦ^�_�T��)(�_���T��)
long CMapPlayer::CWar_IsResurrectWait()
{
	if (m_nCWarResurrectTick)
		return(1);
	return(0);
}

void CMapPlayer::PlayerCastResultNotify(long retry)
{
	long nMsgSize;
	struct MAP_UPDATE_PLAYER_DATA_PART2 nMsg;

	nMsg.Reset();
	nMsg.Add(UPART2_TYPE_RETRY_CAST_MAGIC, (unsigned short)retry, 0, 0);

	nMsgSize = nMsg.GetSize();
	::msgSendData(GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsgSize, 0);
}

long CMapPlayer::PlayerActErrorLogCount(long reason, long send_immed)
{
	long r = 0;
	long curtime;
	struct LOGIN_MAP_FIND_BOT_NOTIFY nData;

	curtime = CMapServer::GetServer()->GetLoopTime();

	if (reason && send_immed)
	{
		nData.nBotType = (unsigned char)reason;
		goto send_notify;
	}
	//
	if (curtime >= m_nPAErrorTime)
	{
clear_reset:
		m_nPAErrorTime = curtime + PLAYER_ACT_ERROR_CHECK_TIME;
		m_nPAErrorCount = 0;
	}
	else
	{
		m_nPAErrorCount++;
		if (m_nPAErrorCount >= PLAYER_ACT_ERROR_MAX_COUNT)
		{
			// ��x�q��
			//struct LOGIN_MAP_FIND_BOT_NOTIFY nData;

			if (reason)
			{
				nData.nBotType = (unsigned char)reason;
			}
			else
				nData.nBotType = (unsigned char)BOT_REASON_GM_NOTICE;
send_notify:
			nData.nBotCheckType = 0;
			nData.nMapCode = GetMapCode();
			nData.uid = GetUID();
			msg_strncpy(nData.szName, GetName(), sizeof(nData.szName));
			msg_strncpy(nData.szAccount, GetSaveData()->plrAccount, sizeof(nData.szAccount));
			CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_MAP_FIND_BOT_NOTIFY, (char *)&nData, sizeof(nData), 0);
			//
			r = 1;
#ifdef USE_PLAYER_ERROR_KICK
			CMapServer::GetServer()->KickPlayer(this);
#endif
			goto clear_reset;
		}
	}
	return(r);
}

// �O���u�ɶ��D�k����
long CMapPlayer::PlayerAttackErrorCount(long log_id)
{
	long cur_time;

	cur_time = CMapServer::GetServer()->GetLoopTime();
	if (cur_time >= m_nPlayerAttackErrorTime)		// �U�ɲM��
	{
		m_nPlayerAttackErrorTime = cur_time + PLAYER_ATTACK_ERR_COUNT_TIME;
		if (m_nPlayerAttackErrorCount < PLAYER_ATTACK_ERR_COUNTER)
			m_nPlayerAttackErrorCount = 0;
	}
	//
	if (++m_nPlayerAttackErrorCount == PLAYER_ATTACK_ERR_COUNTER)
	{
		CMapServer::GetServer()->Log("SERIOUS: Player attack err-count(%d)(%d,%s,%s)", log_id, GetUID(), GetName(), GetSaveData()->plrAccount);
		// �Ȯɤ���
#ifdef USE_PLAYER_ERROR_KICK
		CMapServer::GetServer()->KickPlayer(this);
#endif
	}
	if (m_nPlayerAttackErrorCount >= PLAYER_ATTACK_ERR_COUNTER)
		return(1);
	return(0);
}

// �Ĥ@���n�J���ˬd��Ԭ���
void CMapPlayer::SetCWarInfo()
{
	long cur_time, i;
	struct tm * ptm;
	struct tm ntm;

	if (CMapServer::GetServer()->IsMapWar(GetMapCode(), 1))
	{
		cur_time = CMapServer::GetServer()->GetLoopTime();
		if (cur_time >= GetSaveData()->plrCWar_Duedate)	// �W�@����Ԭ������Įɶ�
		{
#ifdef USE_NEW_CWAR_LIST_TIME_GIFT
			GetSaveData()->plrCWar_Gift_Tick = 0;
#endif
#if (defined(WAR_ADD_ARMY_POINT) || defined(CWAR_ADD_RED_SCORE))
			GetSaveData()->plrCWar_ArmyPoint_Tick = 0;
#endif
			GetSaveData()->plrCWar_Honor = 0;
			GetSaveData()->plrCWar_KillCount = 0;
			// ��s���Įɶ�
			ptm = ::apiGetTimeStruct(cur_time);
			memcpyT(&ntm, ptm, sizeof(ntm));
			ntm.tm_hour = 0;
			ntm.tm_min = 0;
			ntm.tm_sec = 0;
			i = (long)mktime(&ntm);
			i = i + (60 * 60 * 24) + (60*60*3);		// �j��3�I
			GetSaveData()->plrCWar_Duedate = i;
			//
			AutoSaveCharData();
		}
		//else		// �q�� Client �H�Q
		//{
			UpdateCWarInfo();
		//}
	}
}

// ���a�������a�H�Q�O��
void CMapPlayer::SendCWarInfo(long add_honor)
{
	struct plrDATA_SAVE * plrData;
	struct LOGIN_UPDATE_CWAR_RECORD nReq;
	long val;
	long i, cur_time;
	struct tm * ptm;
	struct tm ntm;
	CMapCharMsg * pMsg;

	cur_time = CMapServer::GetServer()->GetLoopTime();
	plrData = GetSaveData();
	if (cur_time >= plrData->plrCWar_Duedate)	// �W�@����Ԭ������Įɶ�
	{
		plrData->plrCWar_Honor = 0;
		plrData->plrCWar_KillCount = 0;
		// ��s���Įɶ�
		ptm = ::apiGetTimeStruct(cur_time);
		memcpyT(&ntm, ptm, sizeof(ntm));
		ntm.tm_hour = 0;
		ntm.tm_min = 0;
		ntm.tm_sec = 0;
		i = (long)mktime(&ntm);
		i = i + (60 * 60 * 24) + (60*60*3);		// �j��3�I
		plrData->plrCWar_Duedate = i;
	}
	//
	val = plrData->plrCWar_Honor + add_honor;	// �W�@����Ԭ���: �\��
	if (val > 65535)
		val = 65535;
	plrData->plrCWar_Honor = (unsigned short)val;
	val = plrData->plrCWar_KillCount + 1;		// �W�@����Ԭ���: ����
	if (val > 65535)
		val = 65535;
	if (val != plrData->plrCWar_KillCount)
	{
		plrData->plrCWar_KillCount = (unsigned short)val;
		// �e�X�H�Q���W�A�C 100�H
		KillCount_SendSPMessage(plrData->plrCWar_KillCount, 1);
	}
	//
	AutoSaveCharData();
	//
	UpdateCWarInfo();
	// �q�� Login Server
	if( CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->GetLoginServer()) )
	{
		memset(&nReq, 0, sizeof(nReq));
		//
		nReq.nPlayerUID = GetUID();
		msg_strncpy(nReq.nData.pwcwName, GetName(), sizeof(nReq.nData.pwcwName));
		nReq.nData.pwcwLevel = plrData->plrLevel;
		nReq.nData.pwcwJobID = plrData->plrJobID;
		nReq.nData.pwcwRebirth = plrData->plrRebirth;
		nReq.nData.pwcwCountryID = plrData->plrCountryID;
		nReq.nData.pwcwHonor = plrData->plrCWar_Honor;
		nReq.nData.pwcwKillCount = plrData->plrCWar_KillCount;
		nReq.nData.pwcwOrgUID = plrData->plrOrganizeUID;
		nReq.nData.pwcwSex = plrData->plrSex;
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_UPDATE_CWAR_RECORD, (char *)&nReq, sizeof(nReq), 0);
	}
	// �q�� Client�\��
	if (CMapServer::GetServer()->IsCrossServer(1))		// �O����A��Server
	{
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_CWAR_HONOR, 0, plrData->plrCWar_Honor, 0);

		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
	}
}

// ���a�������a�H�Q�O��
void CMapPlayer::SendCWarInfo2(long add_honor)//��A�ɶ��n����(���[�H�Q��)
{
	struct plrDATA_SAVE * plrData;
	struct LOGIN_UPDATE_CWAR_RECORD nReq;
	long val;
	long i, cur_time;
	struct tm * ptm;
	struct tm ntm;
	CMapCharMsg * pMsg;

	cur_time = CMapServer::GetServer()->GetLoopTime();
	plrData = GetSaveData();
	if (cur_time >= plrData->plrCWar_Duedate)	// �W�@����Ԭ������Įɶ�
	{
		plrData->plrCWar_Honor = 0;
		plrData->plrCWar_KillCount = 0;
		// ��s���Įɶ�
		ptm = ::apiGetTimeStruct(cur_time);
		memcpyT(&ntm, ptm, sizeof(ntm));
		ntm.tm_hour = 0;
		ntm.tm_min = 0;
		ntm.tm_sec = 0;
		i = (long)mktime(&ntm);
		i = i + (60 * 60 * 24) + (60*60*3);		// �j��3�I
		plrData->plrCWar_Duedate = i;
	}
	//
	val = plrData->plrCWar_Honor + add_honor;	// �W�@����Ԭ���: �\��
	if (val > 65535)
		val = 65535;
	plrData->plrCWar_Honor = (unsigned short)val;
	val = plrData->plrCWar_KillCount;		// �W�@����Ԭ���: ����
	if (val > 65535)
		val = 65535;
	if (val != plrData->plrCWar_KillCount)
	{
		plrData->plrCWar_KillCount = (unsigned short)val;
		// �e�X�H�Q���W�A�C 100�H
		KillCount_SendSPMessage(plrData->plrCWar_KillCount, 1);
	}
	//
	AutoSaveCharData();
	//
	UpdateCWarInfo();
	// �q�� Login Server
	if( CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->GetLoginServer()) )
	{
		memset(&nReq, 0, sizeof(nReq));
		//
		nReq.nPlayerUID = GetUID();
		msg_strncpy(nReq.nData.pwcwName, GetName(), sizeof(nReq.nData.pwcwName));
		nReq.nData.pwcwLevel = plrData->plrLevel;
		nReq.nData.pwcwJobID = plrData->plrJobID;
		nReq.nData.pwcwRebirth = plrData->plrRebirth;
		nReq.nData.pwcwCountryID = plrData->plrCountryID;
		nReq.nData.pwcwHonor = plrData->plrCWar_Honor;
		nReq.nData.pwcwKillCount = plrData->plrCWar_KillCount;
		nReq.nData.pwcwOrgUID = plrData->plrOrganizeUID;
		nReq.nData.pwcwSex = plrData->plrSex;
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_UPDATE_CWAR_RECORD, (char *)&nReq, sizeof(nReq), 0);
	}
	// �q�� Client�\��
	if (CMapServer::GetServer()->IsCrossServer(1))		// �O����A��Server
	{
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_CWAR_HONOR, 0, plrData->plrCWar_Honor, 0);
		
		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
	}
}

void CMapPlayer::UpdateLoginPlayerOffice()
{
	struct LOGIN_UPDATE_PLAYER_OFFICE nReq;
	struct plrDATA_SAVE * pSave;

	if( CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->GetLoginServer()) )
	{
		nReq.nPlayerUID = GetUID();
		pSave = GetSaveData();
		nReq.nOffice = pSave->plrOffice;
		if (pSave->plrSoulOffice)
			nReq.nOffice = pSave->plrSoulOffice;
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_UPDATE_PLAYER_OFFICE, (char *)&nReq, sizeof(nReq), 0);
	}
}

#ifdef USE_SPCMODE_CN_ADULT_LIMIT
// mode = 0����, 1�n�X�g����
void CMapPlayer::SpecModeCN_UpdateTimeRecord(long mode)
{
	struct plrDATA_SAVE * pSave;
	struct LOGIN_UPDATE_SPCMODE_TIME nReq;
	CMapCharMsg * pMsg;

	pSave = GetSaveData();
	//
	//memset(&nReq, 0, sizeof(nReq));
	nReq.nMode = (char)mode;
	nReq.nPlayerUID = GetUID();
	nReq.nOnlineTime = pSave->plrSPCMode_CN_OnLineTime;
	nReq.nOfflineTime = pSave->plrSPCMode_CN_OffLineTime;
	if (mode)
	{
		if (m_nPlayerFlags & PLAYER_FLAGS_UPDATE_SPCMODE_OK)	// ����Ƴq��
			return;
		nReq.nLogoutTime = (long)::time(NULL);
		m_nPlayerFlags |= PLAYER_FLAGS_UPDATE_SPCMODE_OK;
	}
	else
	{
		nReq.nLogoutTime = 0;
		// ���q�� Login(�L�N�q)�A�אּ�q�� Client ��s���
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		if (pMsg)
		{
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_SPCMODE_TIME_NOTIFY, 0, pSave->plrSPCMode_CN_OnLineTime, pSave->plrSPCMode_CN_OffLineTime);
			
			pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}
		SendMsgToPlayer();
		//
	}
	CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_UPDATE_SPCMODE_TIME, (char *)&nReq, sizeof(nReq), 0);
}
#endif

// �Ĥ@���n�J�ɳq�����A�A��l��
void CMapPlayer::SpecModeCN_FirstLoginNotify()
{
	struct plrDATA_SAVE * pSave;
	long cur_time;
	long val, offline_time, online_time;

	pSave = GetSaveData();
	//
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	if (m_nPlayerFlags & PLAYER_FLAGS_UPDATE_SPCMODE)
	{
		m_nPlayerFlags &= ~PLAYER_FLAGS_UPDATE_SPCMODE;
		//
		pSave->plrSPCMode_CN_OnLineTime = m_nOnlineTime;
		pSave->plrSPCMode_CN_OffLineTime = m_nOfflineTime;
		pSave->plrSPCMode_CN_LastLogoutTime = m_nLastLogoutTime;
		AutoSaveCharData();
	}
#endif
	//
	cur_time = CMapServer::GetServer()->GetLoopTime();
#ifdef VietnamMode		// �V�n���S���U�u�ɶ��A�u���j�ѲM���ɶ�
	if (CMapServer::GetServer()->m_nResetSpecCNModeTime)	// �����m�ɶ�
	{
		if (pSave->plrSPCMode_CN_LastLogoutTime == CMapServer::GetServer()->m_nResetSpecCNModeTime)
		{
			pSave->plrSPCMode_CN_LastLogoutTime += 1;
			pSave->plrSPCMode_CN_OnLineTime = 0;
			m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
			AutoSaveCharData();
			goto dok;
		}
	}
	if (cur_time >= pSave->plrSPCMode_CN_LastLogoutTime)
#else
	if (CMapServer::GetServer()->m_nResetSpecCNModeTime)	// �����m�ɶ�
	{
		if (CMapServer::GetServer()->m_nResetSpecCNModeTime > pSave->plrSPCMode_CN_LastLogoutTime)
		{
			pSave->plrSPCMode_CN_LastLogoutTime = cur_time;
			pSave->plrSPCMode_CN_OffLineTime = 0;
			pSave->plrSPCMode_CN_OnLineTime = 0;
			m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
			AutoSaveCharData();
			goto dok;
		}
	}
	// �M�w�W�����n�J�֭p�ɶ�
	if (pSave->plrSPCMode_CN_LastLogoutTime)
	{
		val = cur_time - pSave->plrSPCMode_CN_LastLogoutTime;
		if (val < 0)
			val = 0;
	}
	else
		val = 0;
	offline_time = (long)pSave->plrSPCMode_CN_OffLineTime + val;
	if (offline_time > 60000)								// �̪�1000����
		offline_time = 60000;
 #ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("Player(%s) off-line time(%d,%d) !", GetName(), pSave->plrSPCMode_CN_OffLineTime, val);
 #endif
	//
	if (offline_time >= SPECMODE_CN_OFFLINE_CLEAR_TIME)		// �U�u�ɶ��W�L 5�p�ɭ��m
#endif
	{
		offline_time = 0;
		pSave->plrSPCMode_CN_OnLineTime = 0;
		m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
		//
#ifdef VietnamMode		// �V�n���S���U�u�ɶ��A�u���j�ѲM���ɶ�
		// �]�w�M���ɶ�, �j�ѭ��
		struct tm * ptm;
		struct tm ntm;

		ptm = ::apiGetTimeStruct(0);
		memcpyT(&ntm, ptm, sizeof(ntm));
		ntm.tm_hour = 0;
		ntm.tm_min = 0;
		ntm.tm_sec = 0;
		val = (long)mktime(&ntm);
		val = val + (60 * 60 * 24);
		pSave->plrSPCMode_CN_LastLogoutTime = val;
		AutoSaveCharData();
#endif
	}
	else
	{
		online_time = pSave->plrSPCMode_CN_OnLineTime;
		if (online_time >= SPECMODE_CN_ONLINE_WARN_TIME_2)		// �W�u�ɶ��W�L 5�p��: �L
		{
			m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_3;
			//
			if (!(m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			{
				m_nWebIP_Flags |= LOGIN_CHAR_FLAGS_SPCMODE_CN_NO;
				SendMsgToClientByID(24358);
			}
		}
		else if (online_time >= SPECMODE_CN_ONLINE_WARN_TIME_1)	// �W�u�ɶ��W�L 3�p��: ��b
		{
			m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_2;
			//
			if (!(m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN_HF))
			{
				m_nWebIP_Flags |= LOGIN_CHAR_FLAGS_SPCMODE_CN_HF;
				SendMsgToClientByID(24359);
			}
		}
		else
			m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
 #ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("Player(%s) on-line time(%d) !", GetName(), online_time);
 #endif
	}
#ifndef VietnamMode
	// ��s�̫�n�X�ɶ�
	pSave->plrSPCMode_CN_LastLogoutTime = cur_time;
	// ��s�U�u�ɶ�
	pSave->plrSPCMode_CN_OffLineTime = (unsigned short)offline_time;
	//
	AutoSaveCharData();
#endif
	//
dok:
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	SpecModeCN_UpdateTimeRecord();		// ��s����
#else
	return;
#endif
}

// ���I�g�B�z
void CMapPlayer::SpecModeCN_Process(long seconds)
{
	struct plrDATA_SAVE * pSave;
	long cur_time;
	long online_time;
	char tmpstr[256];

	pSave = GetSaveData();
	//
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	if (m_nPlayerFlags & PLAYER_FLAGS_UPDATE_SPCMODE)
	{
		m_nPlayerFlags &= ~PLAYER_FLAGS_UPDATE_SPCMODE;
		//
		pSave->plrSPCMode_CN_OnLineTime = m_nOnlineTime;
		pSave->plrSPCMode_CN_OffLineTime = m_nOfflineTime;
		pSave->plrSPCMode_CN_LastLogoutTime = m_nLastLogoutTime;
		AutoSaveCharData();
		//
		SpecModeCN_UpdateTimeRecord();		// ��s����
	}
#endif
	//
	cur_time = CMapServer::GetServer()->GetLoopTime();
#ifdef VietnamMode		// �V�n���S���U�u�ɶ��A�u���j�ѲM���ɶ�
	if (cur_time >= pSave->plrSPCMode_CN_LastLogoutTime)
	{
		pSave->plrSPCMode_CN_OnLineTime = 0;
		m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
		//
		// �]�w�M���ɶ�, �j�ѭ��
		struct tm * ptm;
		struct tm ntm;
		long val;

		ptm = ::apiGetTimeStruct(0);
		memcpyT(&ntm, ptm, sizeof(ntm));
		ntm.tm_hour = 0;
		ntm.tm_min = 0;
		ntm.tm_sec = 0;
		val = (long)mktime(&ntm);
		val = val + (60 * 60 * 24);
		pSave->plrSPCMode_CN_LastLogoutTime = val;
		AutoSaveCharData();
	}
#else
	// ��s�̫�n�X�ɶ�
	pSave->plrSPCMode_CN_LastLogoutTime = cur_time;
#endif
//	if (!(m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))	// ���եήɡA��������
	{
		// ��s�u�W�ɶ�
		online_time = (long)pSave->plrSPCMode_CN_OnLineTime + seconds;
		if (online_time > 60000)							// �̪�1000����
			online_time = 60000;
		pSave->plrSPCMode_CN_OnLineTime = (unsigned short)online_time;
		// ����F���q��
		if (online_time >= SPECMODE_CN_ONLINE_WARN_TIME_2)		// �W�u�ɶ��W�L 5�p��: �L
		{
			if (!(m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
			{
				m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_3;
				//
				m_nWebIP_Flags |= LOGIN_CHAR_FLAGS_SPCMODE_CN_NO;
				SendMsgToClientByID(24358);
				//
#ifdef VIETNAM_SPCMODE_CN_NO_LIMIT
				CancelTrade();			// �������
				CancelStall();			// �����\�u
#endif
			}
			else
			{
				// �C 15���������@��
				if (cur_time >= m_nSpecModeCN_NotifyTime)
				{
					m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_3;
					SendMsgToClientByID(24358);
				}
			}
		}
		else if (online_time >= SPECMODE_CN_ONLINE_WARN_TIME_1)	// �W�u�ɶ��W�L 3�p��: ��b
		{
			if (!(m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN_HF))
			{
				m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_2;
				//
				m_nWebIP_Flags |= LOGIN_CHAR_FLAGS_SPCMODE_CN_HF;
				SendMsgToClientByID(24361);
			}
			else
			{
				// �C 30���������@��
				if (cur_time >= m_nSpecModeCN_NotifyTime)
				{
					m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_2;
					SendMsgToClientByID(24359);
				}
			}
		}
		else
		{
			// �C 1�p�ɴ����@��
			if (cur_time >= m_nSpecModeCN_NotifyTime)
			{
				m_nSpecModeCN_NotifyTime = cur_time + SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
				//
				cur_time = online_time / SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
			//	SendMsgToClientByID(24360);
				wsprintf(tmpstr, gameGetResourceName(24360), cur_time);
				SendMessage(gameChannel_System, NULL, tmpstr);
			}
		}
	}
	//
	AutoSaveCharData();
}
// ==========================================================
struct INNER_DROP_RECORD
{
	struct itemDATA_SAVE * pItem;
	unsigned short item_idx;
	unsigned short item_type;
};

#if defined(TW_PVP_MODE)
long PVP_CheckItemDrop(CMapPlayer * pMapPlayer, struct itemDATA_SAVE * pItem)
{
 #ifdef TW_PVP_MODE
	struct itemDATA * pItemSetData;
	long itype;

	if (pItem->m_Item.itemCode)
	{
		if (pItemSetData = ::gameGetItemPtr(pItem->m_Item.itemCode))
		{
			itype = pItemSetData->itemType;
			if (!(itype & (itemTypeNoTrade | itemTypeNoDrop)))			// �ư����i�c������
			{
				if (!(pItemSetData->itemType2 & itemType2NoSell))			// ���i��ө�
					return(1);
			}
		}
	}
	return(0);
 #else
	if (!pMapPlayer->IsNoTradeItem(pItem))
		return(1);
 #endif
	return(0);
}
#endif

#ifdef TW_PVP_MODE
// ��s�o�c��
void CMapPlayer::UpdatePlayerCrime(long val)
{
	CMapCharMsg * pMsg;

	if (GetSaveData()->plrCrimeVal != val)
	{
		GetSaveData()->plrCrimeVal = val;
		AutoSaveCharData();
		//
		m_nCrimeClearTick = 0;		// �]�w�p��
		//
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_ALL);
		pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_UPDATE_CRIME;
		pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = GetUID();
		pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData2 = val;
		// ��s�p�L�g�@��
		SetCarryNPC_REDStatus();
	}
}

// 81��(�t)�H�W�A�զW����\���[���A�D�զW����g��ȥ[���C
void CMapPlayer::SetCrimeAddVal(unsigned long * add_exp, unsigned long * add_honor)
{
	struct plrDATA_SAVE * pSave;
	long val, p;

	pSave = GetSaveData();
	if (pSave->plrLevel >= PLAYER_USE_FORCE_PK_LEVEL)
	{
	/*	���H�g���
		2~12�G50%
		13~24�G75%
		25~36�G100%
		37~48�G150%
	*/
		val = pSave->plrCrimeVal;
		if (val >= 2)
		{
			if (val <= 12)
			{
				p = 50;
			}
			else if (val <= 24)
			{
				p = 75;
			}
			else if (val <= 36)
			{
				p = 100;
			}
			else
			{
				p = 150;
			}
			//
			*add_exp += (*add_exp * p / 100);
		}
		else
		{
			*add_honor += *add_honor;
		}
	}
}
#endif

// �j��pk�G
//		mode = 0 �D���H����
// �x�Wpk�G
//		mode = 0 �@����������, 1 �˳Ƥ��]�t�Z��
// �p�G pItemIdx = 65535 ���ܸ˳�
struct itemDATA_SAVE * CMapPlayer::PVP_FindDropItem(long mode, unsigned short * pItemIdx)
{
#if defined(TW_PVP_MODE)
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, imax, itype, size;
	struct itemDATA * pItemSetData;
	struct INNER_DROP_RECORD nRec;
//	struct plrDATA_SAVE * pSave;
	std::vector<struct INNER_DROP_RECORD> vDropItem;

 #ifdef TW_PVP_MODE
	struct plrDATA_SAVE * pSave;

	pItemData = GetItemData();
	size = 0;
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	//if ((mode == 0) || (mode == 1))				// �Q�Ǫ����άO�զW�Qpk
	{
		imax = ::gameServer_GetCarryItemMax(GetSaveData());
		for (i=0; i<imax; i++)
		{
			pItem = &pItemData->plrCarryItem[i];
			if (!pItem->m_Item.itemFlags)					// �ư��S�����~
			{
				if (PVP_CheckItemDrop(this, pItem))
				{
					if (pItemSetData = ::gameGetItemPtr(pItem->m_Item.itemCode))
					{
						itype = pItemSetData->itemType;
						//
						// �������B���Y���B�����ī~
						if (mode > 1)
							goto skip_equip;
						if ((itype & itemTypeThrow) || (itype == itemTypeItem) || (itype == (itemTypeItem | itemTypeNoUse)))
						{
	skip_equip:				nRec.pItem = pItem;
							nRec.item_idx = (unsigned short)i;
							nRec.item_type = 0;
							vDropItem.push_back(nRec);
							size++;
						}
					}
				}
			}
		}
	}
	//
	if (mode)			// �]�t�˳ƪ��~
	{
		pSave = GetSaveData();
		//
		nRec.item_idx = 65535;
		nRec.item_type = 1;				// �˳���
		//
		pItem = &pSave->plrEquipArmor;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		//
		pItem = &pSave->plrEquipFoot;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		//
		pItem = &pSave->plrEquipGlove;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		//
		pItem = &pSave->plrEquipHead;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		//
		pItem = &pSave->plrEquipHorse;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		//
		pItem = &pSave->plrEquipOther1;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		//
		pItem = &pSave->plrEquipOther2;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		//
		pItem = &pSave->plrEquipP;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		//
		pItem = &pSave->plrEquipUnderwear;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		// ����
		pItem = &pSave->plrEquipWeaponL;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		pItem = &pSave->plrEquipWeaponL2;
		if (PVP_CheckItemDrop(this, pItem))
		{
			nRec.pItem = pItem;
			vDropItem.push_back(nRec);
			size++;
		}
		// �k��
		if (mode != 1)	// �զW���t�Z��
		{
			pItem = &pSave->plrEquipWeaponR;
			if (PVP_CheckItemDrop(this, pItem))
			{
				nRec.pItem = pItem;
				vDropItem.push_back(nRec);
				size++;
			}
			pItem = &pSave->plrEquipWeaponR2;
			if (PVP_CheckItemDrop(this, pItem))
			{
				nRec.pItem = pItem;
				vDropItem.push_back(nRec);
				size++;
			}
		}
	}
 #endif
	//
	if (size)
	{
 #ifndef USE_PACKAGE_DATA
		// DEBUG: �⪫�~�C�X��
		std::map<long, long> mapItemRec;
		char tmpstr[4096];

		tmpstr[0] = 0;
		for (i=0; i<size; i++)
		{
			pItem = vDropItem[i].pItem;
			if (pItemSetData = ::gameGetItemPtr(pItem->m_Item.itemCode))
			{
				if (mapItemRec.find(pItem->m_Item.itemCode) == mapItemRec.end())
				{
					mapItemRec[pItem->m_Item.itemCode] = 1;
					//
					if (tmpstr[0])
					{
						strcat(tmpstr, ",");
					}
					else
						strcat(tmpstr, "DEBUG_DROP: ");
					strcat(tmpstr, gameGetResourceName(pItemSetData->itemNameID));
				}
			}
		}
		SendMessage(gameChannel_System, NULL, tmpstr);
 #endif
		//
		i = gameGetRandomRange(0, size);
		while (i >= size)
			i -= size;
		pItem = vDropItem[i].pItem;
		if (pItemIdx)
			*pItemIdx = vDropItem[i].item_idx;
		return(pItem);
	}
#endif
	return(NULL);
}
// ==========================================================
// Start Add By R61

int CMapPlayer::CheckShopType(int p_shoptype)
{
    unsigned int nowShopID = GetShopID();

    sdSHOPDATA *psdSHOPDATA = gameGetShopPtr(nowShopID);    
    if(!psdSHOPDATA)
    {
        CMapServer::GetServer()->Log("Error: Player(%d) enter unknown storeID(%d)", GetUID(), nowShopID );
        return 0;
    }

    if( p_shoptype == psdSHOPDATA->sdShopType )
        return 1;

    return 0;
}

void CMapPlayer::SetShopID(unsigned int p_ShopID)
{
    m_ShopID = p_ShopID;
}

unsigned int CMapPlayer::GetShopID()
{
    return m_ShopID;
}

// �Y�ǰө� ID �|�ھګ�����
unsigned long CMapPlayer::EnterShop_GetID(unsigned long p_ShopID)
{
	struct plrDATA_WORLD_TOWN_DETAIL * pTownDetail;

	sdSHOPDATA * psdSHOPDATA = gameGetShopPtr(p_ShopID);
	if (psdSHOPDATA)
	{
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(GetMapCode());
		if (pTownDetail)
		{
			if (pTownDetail->ptStatueLevel >= 6)
			{
				if (!psdSHOPDATA->sdAdvID3)
					goto do002;
				if (gameGetShopPtr(psdSHOPDATA->sdAdvID3))
					p_ShopID = psdSHOPDATA->sdAdvID3;
			}
			else if (pTownDetail->ptStatueLevel >= 4)
			{
	do002:		if (!psdSHOPDATA->sdAdvID2)
					goto do001;
				if (gameGetShopPtr(psdSHOPDATA->sdAdvID2))
					p_ShopID = psdSHOPDATA->sdAdvID2;
			}
			else if (pTownDetail->ptStatueLevel >= 2)
			{
	do001:		if (psdSHOPDATA->sdAdvID1)
				{
					if (gameGetShopPtr(psdSHOPDATA->sdAdvID1))
						p_ShopID = psdSHOPDATA->sdAdvID1;
				}
			}
//CMapServer::GetServer()->Log("Statue %d, %d, %d", pTownDetail->ptStatueLevel, psdSHOPDATA->sdAdvID1, psdSHOPDATA->sdAdvID2);
		}
	}
	return(p_ShopID);
}

void CMapPlayer::EnterShop(unsigned int p_ShopID)
{
    if(!p_ShopID)
    {
        return;
    }

	if (IsInTrade())		// 2008/04/02
		return;

    HConnect aHConnect = GetClientID();
    if(!aHConnect)
    {
        return; 
    }

    proto_COMM pcm;
    memset( &pcm , 0 , sizeof(pcm) );

    long result = -1;

    sdSHOPDATA *psdSHOPDATA = gameGetShopPtr(p_ShopID);    
    if(!psdSHOPDATA)
    {
        CMapServer::GetServer()->Log("Error: Player(%d) enter unknown storeID(%d)", GetUID(), p_ShopID );
        return;
    }

    long type = psdSHOPDATA->sdShopType;
    switch(type)
    {
        case shopType_Storage:
            // �ӤH�ܮw
            {
                MAP_ENTER_SHOP_STORAGE sendmsg;
                sendmsg.m_Type = type;
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_STORAGE( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
            break;

        case shopType_Hotel:			// �ȩ�
			p_ShopID = EnterShop_GetID(p_ShopID);
			//
            {
                MAP_ENTER_SHOP_STORAGE sendmsg;
                sendmsg.m_Type = type;
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_STORAGE( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
            break;

        case shopType_WeaponArmor:		// �Z����
        case shopType_Market:			// ����
        case shopType_BookStore:		// �ѩ�
        case shopType_Item:				// �D�㩱
		case shopType_Factory:			// �u�{
			p_ShopID = EnterShop_GetID(p_ShopID);
			//
            {
                MAP_ENTER_SHOP_GENERAL sendmsg;
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_GENERAL( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
            break;

        case shopType_Gamble:
            // ���
//#ifndef USE_PACKAGE_DATA		// �������
#if !(defined(GBMode) || defined(GBMode_TW))		// �j���������A�s���n�}
			{
                MAP_ENTER_SHOP_GAMBLE sendmsg;
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_GAMBLE( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
#endif
            break;

        case shopType_Army:
            // �L��
            {
                MAP_ENTER_SHOP_ARMY_CAMP sendmsg;
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_ARMY_CAMP( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
            break;

        case shopType_Lab:
            // �X����
            {
                MAP_ENTER_SHOP_LAB sendmsg;
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_LAB( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
            break;

		case shopType_CityHall:
			// �c��
            {
                MAP_ENTER_SHOP_CITY_HALL sendmsg;
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_CITY_HALL( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
			break;

		case shopType_Arena:
			// ��Z��
            {
                MAP_ENTER_SHOP_GENERAL sendmsg; //�ɥ�
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_ARENA( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
			break;

		case shopType_OrganizePVP:
			// �v�޳�
            {
                MAP_ENTER_SHOP_GENERAL sendmsg; //�ɥ�
                sendmsg.m_Id = p_ShopID;
                result = PreShop::Callback_MAP_ENTER_SHOP_ORGANIZE_PVP( (char*)&sendmsg, sizeof(sendmsg) , aHConnect , &pcm );
            }
			break;

        default:
            CMapServer::GetServer()->Log("Error: Player(%d) enter unknown storetype(%d)", GetUID(), type );
            return;
    }



    if( result == 0 )
    {
        // ���\�i�J�ө�

        SetShopID(p_ShopID);
	    // �]�w���A
	    pStatus_Set(effFun_INVISIBLE, 1);
	    StopMoving();
    }
}

void CMapPlayer::ExitShop(long setVisibleBroadcast, long force_exit_shop)
{
    SetShopID(0);
	//
//	if (setVisibleBroadcast)
//	{
//		if (!(GetCharData()->plrTempStatus & effFun_INVISIBLE))
//		{
//			if (!(GetShowData()->plrStatus & effFun_INVISIBLE))
//				setVisibleBroadcast = 0;
//		}
//	}
	pStatus_Clear(effFun_INVISIBLE, setVisibleBroadcast);
	// �ץ��G�������}�a�������A���|�ǰT��
	// ���ΡG�b LeaveMapBeforeChangeMap() �ɰ���
//	if (setVisibleBroadcast)
//		SendMsgToPlayer();
	//
	// xiun : �j��a���}�ө�
	if (force_exit_shop)
	{
		unsigned long sendmsg = GetUID();
		// client����q����۰ʰe���}�ө���protocol
		::msgSendData(GetClientID(), 0, PROTO_MAP_FORCE_LEAVE_SHOP_NOTIFY, (char *)&sendmsg, sizeof(sendmsg), 0);
	}
}
// End Add By R61

void CMapPlayer::CancelOrgPVP(void)
{
	CMapServer * pServer = CMapServer::GetServer();
	char * szFunc = "CancelOrgPVP";
#ifdef _DEBUG
	pServer->Log( "MapPlayer %s:Enter %s", GetName(), szFunc );
#endif
	if( GetUID() != GetArmyLeaderUID() )
		return;
	//�}�����ˬd
	CMapPlayer * pChallenger = pServer->GetHistoryOrgPVPManager()->HiOP_GiveUpOpenBattle( this );
		//�q���D�Ԫ�
	if( pChallenger ) 
	{
		pChallenger->SendMsgToClientByID( 21004 ); //�������ɡI
		long sendmsg = OrgPVP_Result_GiveUpOpenBattle;
		::msgSendData( pChallenger->GetClientID(), 0, PROTO_MAP_ORG_PVP_RESULT_NOTIFY, (char *)&sendmsg, sizeof( sendmsg ), 0 );
	}
	//�D�Ԫ��ˬd
	CMapPlayer * pHostPlayer = pServer->GetHistoryOrgPVPManager()->HiOP_GiveUpJoinBattle( this );
		//�q���}����
	if( pHostPlayer )
	{
		pHostPlayer->SendMsgToClientByID( 21005 ); //���ڵ����ɡI
		long sendmsg = OrgPVP_Result_GiveUpJoinBattle;
		::msgSendData( pHostPlayer->GetClientID(), 0, PROTO_MAP_ORG_PVP_RESULT_NOTIFY, (char *)&sendmsg, sizeof( sendmsg ), 0 );
	}
}

// xiun : trade 04/03/19
void CMapPlayer::ResetTradeData(void)
{
	m_nTraderUID = 0;
	m_nTradeState = TRADE_STATE_FREE;
	m_nTradeGold = 0;
	m_vTradeItem.clear(); 
	m_vSendReqToTrader.clear();
}

void CMapPlayer::CancelTrade(void)
{
	CMapServer * pMapServer = CMapServer::GetServer();
#ifdef _DEBUG
	pMapServer->Log( "MapPlayer %s:Enter CancelTrade", GetName() );
#endif

	CMapPlayer * pTrader = (CMapPlayer *)pMapServer->FindObjectByUID( m_nTraderUID );
	
	if( pTrader )
	{
		struct proto_COMM pcm; //DDE �~����
		memset( &pcm , 0 , sizeof(pcm) );

		unsigned long uid = GetUID();
		CMapServer::CB_TradeCancel( (char*)&uid, sizeof(uid), GetClientID(), &pcm);
	}

	ResetTradeData();
}

bool IsTraderTimeOut( CMapPlayer::sendReqTrader trader )
{
	if( (::GetTickCount() - trader.time) > CMapPlayer::TRADE_TIMEOUT ) //�^���O��
		return true;
	else 
		return false;
}

void CMapPlayer::RemoveTimeOutTrader(void)
{
	CMapServer * pMapServer = CMapServer::GetServer();
#ifdef _DEBUG
	pMapServer->Log( "MapPlayer %s:Enter RemoveTimeOutTrader", GetName() );
#endif

	vector< CMapPlayer::sendReqTrader >::iterator it = 
		remove_if( m_vSendReqToTrader.begin(), m_vSendReqToTrader.end(), IsTraderTimeOut ); 

#ifdef _DEBUG
if(it != m_vSendReqToTrader.end())
	pMapServer->Log( "MapPlayer %s: RemoveTimeOutTrader: find timeout trader!", GetName() );
#endif

	m_vSendReqToTrader.erase(it, m_vSendReqToTrader.end());
}
// end xiun : trade

//xiun : stall 04/06/02
	//���󪬺A�n�^��STALL_STATE_FREE�����̰��u���v
bool CMapPlayer::SetStallState(unsigned char state)
{
	CMapServer * pMapServer = CMapServer::GetServer();
#ifdef _DEBUG
	pMapServer->Log( "MapPlayer %s:Enter SetStallState", GetName() );
#endif

	if(state == m_nStallState)
		return true;

	bool r = true;
	
	if(m_nStallState == STALL_STATE_FREE)
	{
		if(state == STALL_STATE_OPERATING)
			r = false;
		else 
			m_nStallState = state;
	}
	else if(m_nStallState == STALL_STATE_PREPARING)
	{
		if(state == STALL_STATE_BUY)
			r = false;
		else 
			m_nStallState = state;
	}
	else if(m_nStallState == STALL_STATE_OPERATING)
	{
		if(state != STALL_STATE_FREE)
			r = false;
		else 
			m_nStallState = state;
	}
	else if(m_nStallState == STALL_STATE_BUY)
	{
		if(state != STALL_STATE_FREE)
			r = false;
		else 
			m_nStallState = state;
	}

	return r;
}

void CMapPlayer::SetStallTitle(char * szTitle)
{
	if( !szTitle )
		return;

	::memcpyT( &m_szStallTitle, szTitle, STALL_TITLE_MAX_CHAR + 1 );
}

#ifdef USE_OFFLINE_STALL
bool CMapPlayer::WantOfflineStallLogout(void)
{
	if (m_bOfflineStallLicense)
		return true;
	/* 不依賴 ItemMode：GAMEFLIER_NORMAL 會 #undef ItemMode，但 plrPersonalShopTime 仍可由月令牌寫入 */
	unsigned long shopExpire = GetSaveData()->plrPersonalShopTime;
	if (shopExpire != 0 && (unsigned long)CMapServer::GetServer()->GetLoopTime() < shopExpire)
		return true;
	return false;
}
#endif

void CMapPlayer::ResetStallData(void)
{
	m_vStallSellItem.clear();
	m_vStallBuyItem.clear();
	m_vStallCustomerUID.clear();
	m_nStallState = STALL_STATE_FREE;
	SetStallTitle("");
	m_ulStallHostUID = 0;
}

void CMapPlayer::CancelStall(void)
{
	CMapServer * pMapServer = CMapServer::GetServer();
#ifdef _DEBUG
	pMapServer->Log( "MapPlayer %s:Enter CancelStall", GetName() );
#endif

	if( IsInStall() ) //�R/��
	{
		struct proto_COMM pcm; //DDE �~����
		memset( &pcm , 0 , sizeof(pcm) );

		unsigned long uid = GetUID();
		if( GetStallHostUID() ) //�U��
			CMapServer::CB_StallLeave( (char*)&uid, sizeof(uid), GetClientID(), &pcm);
		else //�D�H
			CMapServer::CB_StallClose( (char*)&uid, sizeof(uid), GetClientID(), &pcm);
	}
}
//end xiun : stall

//xiun : �x�έܮw
itemDATA_SAVE * CMapPlayer::GetArmyStorageItem(long nItemIdx)
{
	CMapServer * pMapServer = CMapServer::GetServer();
#ifdef _DEBUG
	pMapServer->Log( "MapPlayer %s:Enter GetArmyStorageItem", GetName() );
#endif

	long num = m_vArmyStorageItem.size();
	if( num == 0 || nItemIdx < 0 || nItemIdx >= num )
		return 0;

	return &m_vArmyStorageItem[ nItemIdx ];
}

void CMapPlayer::ClearArmyStorageData(void)
{
	//m_nPlayerFlags &= ~PLAYER_FLAGS_GET_ARMY_STORAGE;
	m_nPlayerFlags &= ~PLAYER_FLAGS_USE_ARMY_STORAGE;
	m_vArmyStorageItem.clear();
#ifdef USE_ARMY_STORAGE_GOLD
	m_nArmyStorageGoldFlag = 0;
	m_nArmyStorageGold = 0;
#endif
}
//end xiun : �x�έܮw

//xiun : state
long CMapPlayer::GetState(void)
{
	CMapServer * pMapServer = CMapServer::GetServer();
#ifdef _DEBUG
	pMapServer->Log( "MapPlayer %s:Enter GetState", GetName() );
#endif

	if( IsInShop() )
		return STATE_IN_SHOP;
	else if( IsInTrade() )
		return STATE_IN_TRADE;
	else if( IsInStall() )
		return STATE_IN_STALL;
	else 
		return STATE_FREE;
}

bool CMapPlayer::IsAvailable(void)
{
#ifdef _DEBUG
	CMapServer * pMapServer = CMapServer::GetServer();
//	pMapServer->Log( "MapPlayer %s:Enter IsAvailable, GetName()" ); //always call
#endif
	bool r = true;

	if( IsInShop() || IsInTrade() || IsInStall() || CWar_IsResurrectWait())
		r = false;

	return r;
}

void CMapPlayer::UpdateClientItemData( long msgUID )
{
	/* plrCARRYITEM 约 400 格 × itemDATASHOWSELF，栈上分配易与炼造等路径叠加导致栈溢出(0xC00000FD) */
	struct plrCARRYITEM *pSend = (struct plrCARRYITEM *)::malloc(sizeof(struct plrCARRYITEM));
	if (!pSend)
		return;
	::memset(pSend, 0, sizeof(*pSend));
	::gameServer_MakeCarryItem(GetItemData(), pSend);
	::msgSendData(GetClientID(), msgUID, PROTO_MAP_GET_PLAYER_CARRY_ITEM_RESULT, (char *)pSend, (long)sizeof(*pSend), 0);
	::free(pSend);
}

void CMapPlayer::UpdateClientSingleItem(long itemIdx, long msgUID)
{
	if( itemIdx < 0 || itemIdx >= gameMAX_CARRYITEM )	// �קK�S��s�A�ϥγ̤j��
		return;

	struct MAP_GET_PLAYER_ITEM_RESULT sendData;

	sendData.itemIdx = itemIdx;
	::gameServer_Item_MakeShowSelf( &GetItemData()->plrCarryItem[ itemIdx ], &sendData.itemShowSelf );
	::msgSendData( GetClientID(), msgUID, PROTO_MAP_GET_PLAYER_ITEM_RESULT, (char *)&sendData, sizeof(sendData), 0 );
}

void CMapPlayer::UpdateClientStorageData(long msgUID)
{
	struct plrSTORAGEITEM nStoreItem;

	gameServer_MakeStorageItem(GetStorageData(), &nStoreItem);
	::msgSendData( GetClientID(), msgUID, PROTO_LOGIN_GETPLAYERDATA_STORAGE_RESULT, (char *)&nStoreItem, sizeof(nStoreItem), msgSEND_FLAG_COMPRESS );
}

void CMapPlayer::UpdateClientStorageData2(long msgUID)
{
	struct plrSTORAGEITEM nStoreItem;

	gameServer_MakeStorageItem(GetStorageData(), &nStoreItem);
	::msgSendData( GetClientID(), msgUID, PROTO_MAP_GET_PLAYER_STORAGE_RESULT, (char *)&nStoreItem, sizeof(nStoreItem), msgSEND_FLAG_COMPRESS );
}

void CMapPlayer::UpdateClientGoldAndWeight(void)
{
	CMapCharMsg *	pMsg;

	pMsg			= ReplaceMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	if (!pMsg->m_nIsNewInsert)
	{
		if (pMsg->m_Msg.m_UpdateData2Msg.GetCount() < (UPDATE_DATA_PART_MAX-2))
		{
			goto doit;
		}
		//
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	}
	pMsg->m_Msg.m_UpdateData2Msg.Reset();

doit:
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GOLD, 0, GetUID(), GetGold());
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_WEIGHT, 0, GetWeight());
	//
	pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

// �s�@ showself �æ^��
void CMapPlayer::UpdateClientShowData( long msgUID )
{
	struct plrDATASHOWSELF Self;

	::memset( &Self, 0, sizeof(struct plrDATASHOWSELF) );
	::gameServer_MakeShowSelf( GetCharData(), &Self );
	::msgSendData( GetClientID(), msgUID, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), msgSEND_FLAG_COMPRESS );

	// 补发衣柜外观同步，防止变身/技能等操作覆盖客户端显示的衣柜外观
	extern struct plrDATA_FASHION_SAVE * GetPlayerFashionData(unsigned long uid);
	struct plrDATA_FASHION_SAVE * pFashionData = GetPlayerFashionData(GetCharData()->plrSaveData.plrUID);
	if (pFashionData)
	{
		// 检查是否有任何衣柜外观设置
		bool hasDisplay = false;
		for (int i = 0; i < 8; i++)
		{
			if (pFashionData->plrDisplayedFashions[i] > 0) { hasDisplay = true; break; }
		}
		if (hasDisplay)
		{
			struct PROTOCOL_FASHION_DISPLAY_SYNC displaySync;
			memset(&displaySync, 0, sizeof(displaySync));
			memcpy(displaySync.displayedFashions, pFashionData->plrDisplayedFashions, sizeof(displaySync.displayedFashions));
			::msgSendData(GetClientID(), GetUID(), PROTO_FASHION_DISPLAY_SYNC,
				(char*)&displaySync, sizeof(displaySync), 0);
		}
	}
}

void CMapPlayer::UpdateClientMaxItem()
{
	CMapCharMsg * pMsg;

	pMsg = ReplaceMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	if (!pMsg->m_nIsNewInsert)
	{
		if (pMsg->m_Msg.m_UpdateData2Msg.GetCount() < (UPDATE_DATA_PART_MAX-2))
		{
			goto doit;
		}
		//
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	}
	pMsg->m_Msg.m_UpdateData2Msg.Reset();

doit:
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_CHANGE_MAX_ITEM, 0, GetSaveData()->plrMax_CarryItem, GetSaveData()->plrMax_StorageItem);
	//
	pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

// return: 0x2 �ʦL���A
long CMapPlayer::IsNoTradeItem(struct itemDATA_SAVE * pItemSave)
{
	long cur_time;
	struct itemDATA * pItemData;

	pItemData = ::gameGetItemPtr(pItemSave->m_Item.itemCode);
	if (pItemData)
	{
		if ((pItemData->itemType & itemTypeSEAL) || ((pItemData->itemType2 & itemType2Engineer) && (pItemData->itemType2 & itemTypeSEAL)))
		{
			if(pItemSave->m_NPC.inpcSealDuedate && pItemSave->m_Item.itemFlags& itemSHOW_FLAG_ALL_CNPC)
			{
				cur_time = (long)time(NULL);
				if (pItemSave->m_NPC.inpcSealDuedate == 0xffffffff)
				{
					return(2);
				}
				if (pItemSave->m_NPC.inpcSealDuedate >= (unsigned long)cur_time)
				{
					return(2);
				}
			}
			else if (pItemSave->m_Item.itemSealDuedate)
			{
				if (pItemSave->m_Item.itemSealDuedate == 0xffffffff)
					return(2);
				cur_time = CMapServer::GetServer()->GetLoopTime();
				if (pItemSave->m_Item.itemSealDuedate >= cur_time)
					return(2);
			}
		}
		//
		if (pItemData->itemType & itemTypeNoTrade)
			return(1);
		return(0);
	}
	return(1);
}

bool CMapPlayer::DelItemAndUpdateClient(long itemID, long num,
										long logType, CMapChar * pTarget) //�R�����~�æ^��
{
	unsigned long long val;
	CMapCharMsg * pMsg;
	char tmpstr[256];
	struct itemDATA_SAVE nItem;

	switch(itemID)
	{
	case item_Money:
		val = GetGold();
		if (val >= (unsigned long long)num)
		{
			val -= (unsigned long long)num;
		}
		else
			val = 0;
		GetSaveData()->plrGold = val;
		//
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);

		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_LOSEGOLD, 0, num);
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GOLD, 0, GetUID(), val);
		
		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		// �g log ...
		::SendGoldLog( num, this, logType, 0 ); //��
		//
		SaveCharData(0);
		return(true);
		break;
	case item_Exp:
		return(true);
		break;
	case item_SkillExp:
		val = GetSkillExp();
		if (val >= (unsigned long)num)
		{
			val -= num;
		}
		else
			val = 0;
		GetSaveData()->plrSkillExp = (unsigned long)val;
		//
		UpdatePlayerDataPart1();
		wsprintf(tmpstr, MSG_LOSE_SKILL_EXP, num);
		SendMessage(gameChannel_System, NULL, tmpstr);
		// �g log ...
		
		memset(&nItem, 0, sizeof(nItem));
		nItem.m_Item.itemCode = (unsigned short)item_SkillExp;
		nItem.m_Item.itemGoldNumber = num;
		CMapServer::GetServer()->SendLogMessage_Item( this, logType, pTarget, &nItem );
		//
		SaveCharData(0);
		return(true);
		break;
	case item_Honor:
		{
			unsigned long dec = (num < 0) ? 0UL : (unsigned long)num;
			unsigned long curH = GetSaveData()->plrHonor;
			GetSaveData()->plrHonor = (curH >= dec) ? (curH - dec) : 0UL;
			pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_HONOR, 0, GetSaveData()->plrHonor);
			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}
		memset(&nItem, 0, sizeof(nItem));
		nItem.m_Item.itemCode = (unsigned short)item_Honor;
		nItem.m_Item.itemGoldNumber = num;
		CMapServer::GetServer()->SendLogMessage_Item(this, logType, pTarget, &nItem);
		break;
	case item_KSScore:
		AddKSScore(-num, 0);
		memset(&nItem, 0, sizeof(nItem));
		nItem.m_Item.itemCode = (unsigned short)item_KSScore;
		nItem.m_Item.itemGoldNumber = num;
		CMapServer::GetServer()->SendLogMessage_Item(this, logType, pTarget, &nItem);
		break;
	}


	struct plrCARRYITEM_SAVE carryItemBak = *GetItemData();
	bool r = DelItem(itemID, num);
	
	if(r)
	{
		UpdateClientItemData();
		gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
		UpdateClientGoldAndWeight();
		//
		SaveItemData();
		//�glog
		if( logType != -1 )
		{
			if( !::CompareItemAndSendLog( &carryItemBak, this, logType, pTarget ) )
				CMapServer::GetServer()->Log( "ERROR : ���a(%s)�R�����~�ɼglog���~!!", GetName() );
		}
	}

	return r;
}

bool CMapPlayer::DelItemAndUpdateClientBless(long itemID, long num, long bless,
										long logType, CMapChar * pTarget) //�R�����~�æ^��
{
	unsigned long long val;
	CMapCharMsg * pMsg;
	char tmpstr[256];

	switch(itemID)
	{
	case item_Money:
		val = GetGold();
		if (val >= (unsigned long long)num)
		{
			val -= (unsigned long long)num;
		}
		else
			val = 0;
		GetSaveData()->plrGold = val;
		//
		pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_LOSEGOLD, 0, num);
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GOLD, 0, GetUID(), val);
		
		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		
		// �g log ...
		::SendGoldLog( num, this, logType, 0 ); //��
		//
		SaveCharData(0);
		return(true);
		break;
	case item_Exp:
		return(true);
		break;
	case item_SkillExp:
		val = GetSkillExp();
		if (val >= (unsigned long)num)
		{
			val -= num;
		}
		else
			val = 0;
		GetSaveData()->plrSkillExp = (unsigned long)val;
		//
		UpdatePlayerDataPart1();
		wsprintf(tmpstr, MSG_LOSE_SKILL_EXP, num);
		SendMessage(gameChannel_System, NULL, tmpstr);
		// �g log ...
		struct itemDATA_SAVE nItem;

		memset(&nItem, 0, sizeof(nItem));
		nItem.m_Item.itemCode = (unsigned short)item_SkillExp;
		nItem.m_Item.itemGoldNumber = num;
		CMapServer::GetServer()->SendLogMessage_Item( this, logType, pTarget, &nItem );
		//
		SaveCharData(0);
		return(true);
		break;
	case item_Honor:
		{
			unsigned long dec = (num < 0) ? 0UL : (unsigned long)num;
			unsigned long curH = GetSaveData()->plrHonor;
			GetSaveData()->plrHonor = (curH >= dec) ? (curH - dec) : 0UL;
			pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_HONOR, 0, GetSaveData()->plrHonor);
			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}
		{
			struct itemDATA_SAVE nItem;
			memset(&nItem, 0, sizeof(nItem));
			nItem.m_Item.itemCode = (unsigned short)item_Honor;
			nItem.m_Item.itemGoldNumber = num;
			CMapServer::GetServer()->SendLogMessage_Item(this, logType, pTarget, &nItem);
		}
		break;
	}


	struct plrCARRYITEM_SAVE carryItemBak = *GetItemData();
	bool r = DelItemBless(itemID, num, bless);
	
	if(r)
	{
		UpdateClientItemData();
		gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
		UpdateClientGoldAndWeight();
		//
		SaveItemData();
		//�glog
		if( logType != -1 )
		{
			if( !::CompareItemAndSendLog( &carryItemBak, this, logType, pTarget ) )
				CMapServer::GetServer()->Log( "ERROR : ���a(%s)�R�����~�ɼglog���~!!", GetName() );
		}
	}

	return r;
}

void CMapPlayer::NotifyLoginCountryChanged(long oldCountryID)
{
	CMapServer * pServer = CMapServer::GetServer();

	if( !pServer )
		return;

	long hLoginServer = pServer->GetLoginServer();

	if( !pServer->IsConnectedToServer(hLoginServer) )
		return;

	if( GetCountryID() == oldCountryID ) //���y�S��
		return;

	struct LOGIN_CHANGE_FORCE nData;
	
	nData.nUID = GetUID();
	nData.nOldCountryID = (unsigned char)oldCountryID;
	nData.nCountryID = GetCountryID();
	
	pServer->SendData(hLoginServer, 0, PROTO_LOGIN_CHANGE_FORCE, (char *)&nData, sizeof(nData), 0);
}

// no_send_login = 0 �q��Login, �䥦: ���q��Login, 2 ���n���[�J�T��
void CMapPlayer::ChangeCountryAndUpdateClient(unsigned char newCountryID, long msgUID, long no_send_login)
{
	unsigned char oldCountryID = GetCountryID();

	if (oldCountryID == newCountryID)
	{
		CMapServer::GetServer()->Log("WARNING: ChangeCountryAndUpdateClient Change the same country(%s,%d)", GetName(), newCountryID);
		return;
	}
	GetSaveData()->plrCountryID = newCountryID;
	//���y�C��
	GetCharData()->plrSetColor = (unsigned short)::gameGetCountryColor( newCountryID );
	//�s��
	if (!(no_send_login & 2))		// Login�q���Ҧ��H�դO�ܧ�
	{
		SaveCharData();
	}
	else
		AutoSaveCharData();
	//�^��
	UpdateClientShowData( msgUID ); //��s�ۤv
	UpdateShowData();				//��ۤvshowdata�s�����e���W�Ҧ��H�]���t�ۤv�^
	UpdateCarryNPCCountryID();		// ��s�p�L���y

	if (!(no_send_login & 2))
	{
		char * szCountryName = ::gameGetCountryNameByID( newCountryID, CMapServer::GetServer()->GetForceDataArray() );
		if( szCountryName )
		{	// �A�[�J%s��C
			char tmpstr[256];
			::wsprintf( tmpstr, ::gameGetResourceName(20511), szCountryName );
			SendMessage( gameChannel_System, NULL, tmpstr );
		}
	}
	// �q�� Login Server ���s�έp�U��H��
	if (!no_send_login)
		NotifyLoginCountryChanged( oldCountryID );
}

void CMapPlayer::SendGetGoldMsgToCleint(unsigned long long gold, long type)
{
	if(type != UPART2_TYPE_GETGOLD && type != UPART2_TYPE_STALL_GETGOLD && type != UPART2_TYPE_STALL_PAYGOLD)
		return;

	CMapCharMsg *	pMsg;

	pMsg			= InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);

	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add((unsigned short)type, 1, 0, (long long)gold); // UPART2_TYPE_GETGOLD:�j����ܦb�t�ΰT��

	pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

void CMapPlayer::SendGetItemMsgToCleint(long itemID, long num, long showOnBoard)
{
	CMapCharMsg *	pMsg;

	pMsg			= InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);

	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	
	if( showOnBoard ) //�@�w��ܦb�����W
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETITEM_SHOW_ON_MAINBOARD, (short)num, itemID, 0);
	else
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GETITEM, (short)num, itemID, 0);

	pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
}

void CMapPlayer::SendSpecialTimeTable()
{
	long sec, c_time;
	struct plrDATA_SAVE * pSave;
	struct MAP_SEND_SPECIAL_TIME nRet;

	memset(&nRet, 0, sizeof(nRet));
	c_time = CMapServer::GetServer()->GetLoopTime();
	//
	pSave = GetSaveData();
	//
	sec = pSave->plrStsTime_GoodLuck;
	if (sec)
		nRet.nGoodLuck = c_time + sec;
	//
	sec = pSave->plrStsTime_AddExp;
	if (sec)
		nRet.nAddExp = c_time + sec;
	//
	sec = pSave->plrStsTime_AddSkillExp;
	if (sec)
		nRet.nAddSkillExp = c_time + sec;
	//
	sec = pSave->plrStsTime_AddHonor;
	if (sec)
		nRet.nAddHonor = c_time + sec;
	//
	sec = pSave->plrStsTime_AddCNPCExp;
	if (sec)
		nRet.nAddCNPCExp = c_time + sec;
	//
	sec = pSave->plrStsTime_Undead;
	if (sec)
		nRet.nUndead = c_time + sec;
	//
	sec = pSave->plrStsTime_SoldierUndead;
	if (sec)
		nRet.nSoldierUndead = c_time + sec;

	// �Z��ɶ�
	if (pSave->plrSoulWID && pSave->plrSoulWID != 0xFFFF)
	{
		sec = pSave->plrSoulTime;
		if (sec > 0)
			nRet.nSoulTime = sec;
	}
	//
	nRet.nEnterStageTime = pSave->plrEnterStageTime;
	nRet.nEnterStageID = pSave->plrEnterStageID;
	// �]��
	sec = pSave->plrStsTime_Month_Exp;
	if (sec)
		nRet.nAddExp = sec;
	sec = pSave->plrStsTime_Month_Skill_Exp;
	if (sec)
		nRet.nAddSkillExp = sec;
	sec = pSave->plrStsTime_Month_CNPC_Exp;
	if (sec)
		nRet.nAddCNPCExp = sec;
	sec = pSave->plrStsTime_Month_GoodLuck;
	if (sec)
		nRet.nGoodLuck = sec;
	sec = plrStsTime_Month_AddHonor_field(pSave);
	if (sec)
		nRet.nAddHonor = sec;
	//
	::msgSendData( GetClientID(), 0, PROTO_MAP_SEND_SPECIAL_TIME, (char *)&nRet, sizeof(nRet), 0 );
}

// (�n�J)(���)
void CMapPlayer::DuedateItem_Init()
{
	struct plrDATA_SAVE * pSave;
	struct plrCARRYITEM_SAVE * pItemSave;
	struct plrSTORAGEITEM_SAVE * pStorageSave;
	long r, val, cnt1;
	struct plrDATASHOWSELF Self;

	m_mapDuedateItem.clear();
	//
	r = 0;
	pSave = GetSaveData();
	// ..... �˳� .....
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaWeaponR.nItemCode, &pSave->plrWawaWeaponR.nUID, 1, (char *)&pSave->plrWawaWeaponR, sizeof(pSave->plrWawaWeaponR));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaArmor.nItemCode, &pSave->plrWawaArmor.nUID, 1, (char *)&pSave->plrWawaArmor, sizeof(pSave->plrWawaArmor));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaHead.nItemCode, &pSave->plrWawaHead.nUID, 1, (char *)&pSave->plrWawaHead, sizeof(pSave->plrWawaHead));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaFoot.nItemCode, &pSave->plrWawaFoot.nUID, 1, (char *)&pSave->plrWawaFoot, sizeof(pSave->plrWawaFoot));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaP.nItemCode, &pSave->plrWawaP.nUID, 1, (char *)&pSave->plrWawaP, sizeof(pSave->plrWawaP));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaHorse.nItemCode, &pSave->plrWawaHorse.nUID, 1, (char *)&pSave->plrWawaHorse, sizeof(pSave->plrWawaHorse));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaAllShape.nItemCode, &pSave->plrWawaAllShape.nUID, 1, (char *)&pSave->plrWawaAllShape, sizeof(pSave->plrWawaAllShape));

	r |= DuedateItem_CheckSingleItem(pSave->plrWawaShowWeaponR.nItemCode, &pSave->plrWawaShowWeaponR.nUID, 1, (char *)&pSave->plrWawaShowWeaponR, sizeof(pSave->plrWawaShowWeaponR));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaShowArmor.nItemCode, &pSave->plrWawaShowArmor.nUID, 1, (char *)&pSave->plrWawaShowArmor, sizeof(pSave->plrWawaShowArmor));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaShowHead.nItemCode, &pSave->plrWawaShowHead.nUID, 1, (char *)&pSave->plrWawaShowHead, sizeof(pSave->plrWawaShowHead));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaShowFoot.nItemCode, &pSave->plrWawaShowFoot.nUID, 1, (char *)&pSave->plrWawaShowFoot, sizeof(pSave->plrWawaShowFoot));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaShowP.nItemCode, &pSave->plrWawaShowP.nUID, 1, (char *)&pSave->plrWawaShowP, sizeof(pSave->plrWawaShowP));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaShowHorse.nItemCode, &pSave->plrWawaShowHorse.nUID, 1, (char *)&pSave->plrWawaShowHorse, sizeof(pSave->plrWawaShowHorse));
	r |= DuedateItem_CheckSingleItem(pSave->plrWawaShowAllShape.nItemCode, &pSave->plrWawaShowAllShape.nUID, 1, (char *)&pSave->plrWawaShowAllShape, sizeof(pSave->plrWawaShowAllShape));

 #ifdef USE_DUEDATE_ITEM
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipWeaponR.m_Item.itemCode, &pSave->plrEquipWeaponR.m_Item.itemUID, 1, (char *)&pSave->plrEquipWeaponR, sizeof(pSave->plrEquipWeaponR));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipWeaponL.m_Item.itemCode, &pSave->plrEquipWeaponL.m_Item.itemUID, 1, (char *)&pSave->plrEquipWeaponL, sizeof(pSave->plrEquipWeaponL));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipWeaponR2.m_Item.itemCode, &pSave->plrEquipWeaponR2.m_Item.itemUID, 1, (char *)&pSave->plrEquipWeaponR2, sizeof(pSave->plrEquipWeaponR2));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipWeaponL2.m_Item.itemCode, &pSave->plrEquipWeaponL2.m_Item.itemUID, 1, (char *)&pSave->plrEquipWeaponL2, sizeof(pSave->plrEquipWeaponL2));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipHorse.m_Item.itemCode, &pSave->plrEquipHorse.m_Item.itemUID, 1, (char *)&pSave->plrEquipHorse, sizeof(pSave->plrEquipHorse));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipArmor.m_Item.itemCode, &pSave->plrEquipArmor.m_Item.itemUID, 1, (char *)&pSave->plrEquipArmor, sizeof(pSave->plrEquipArmor));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipHead.m_Item.itemCode, &pSave->plrEquipHead.m_Item.itemUID, 1, (char *)&pSave->plrEquipHead, sizeof(pSave->plrEquipHead));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipFoot.m_Item.itemCode, &pSave->plrEquipFoot.m_Item.itemUID, 1, (char *)&pSave->plrEquipFoot, sizeof(pSave->plrEquipFoot));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipP.m_Item.itemCode, &pSave->plrEquipP.m_Item.itemUID, 1, (char *)&pSave->plrEquipP, sizeof(pSave->plrEquipP));
	//
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipUnderwear.m_Item.itemCode, &pSave->plrEquipUnderwear.m_Item.itemUID, 1, (char *)&pSave->plrEquipUnderwear, sizeof(pSave->plrEquipUnderwear));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipGlove.m_Item.itemCode, &pSave->plrEquipGlove.m_Item.itemUID, 1, (char *)&pSave->plrEquipGlove, sizeof(pSave->plrEquipGlove));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipOther1.m_Item.itemCode, &pSave->plrEquipOther1.m_Item.itemUID, 1, (char *)&pSave->plrEquipOther1, sizeof(pSave->plrEquipOther1));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipOther2.m_Item.itemCode, &pSave->plrEquipOther2.m_Item.itemUID, 1, (char *)&pSave->plrEquipOther2, sizeof(pSave->plrEquipOther2));
	//xavi�t�����
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipJade.m_Item.itemCode, &pSave->plrEquipJade.m_Item.itemUID, 1, (char *)&pSave->plrEquipJade, sizeof(pSave->plrEquipJade));
	r |= DuedateItem_CheckSingleItem(pSave->plrEquipHideWeapon.m_Item.itemCode, &pSave->plrEquipHideWeapon.m_Item.itemUID, 1, (char *)&pSave->plrEquipHideWeapon, sizeof(pSave->plrEquipHideWeapon));

  #ifdef USE_EQUIP_CARD
	for(int i=0; i < gameMAX_EQUIP_CARD; ++i)
		r |= DuedateItem_CheckSingleItem(pSave->plrEquipCard[i].m_Item.itemCode, &pSave->plrEquipCard[i].m_Item.itemUID, 1, (char *)&pSave->plrEquipCard[i], sizeof(pSave->plrEquipCard[i]));
  #endif

 #endif
	// ..... ���W��a .....
	r = r << 1;
	if (pItemSave = GetItemData())
	{
		//for (cnt1=0; cnt1<gameMAX_CARRYITEM; cnt1++)
		val = ::gameServer_GetCarryItemMax(pSave);
		for (cnt1=0; cnt1<val; cnt1++)
		{
			r |= DuedateItem_CheckSingleItem(pItemSave->plrCarryItem[cnt1].m_Item.itemCode, &pItemSave->plrCarryItem[cnt1].m_Item.itemUID, 1, (char *)&pItemSave->plrCarryItem[cnt1], sizeof(pItemSave->plrCarryItem[cnt1]));
		}
	}
	// ���s�p��t���P���
	if (r)
	{
		gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
		gameServer_CalcCharacterAttribute(GetCharData());
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(GetCharData(), &Self);
		::msgSendData( GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		UpdateShowData();
		//
		if (r & 2)
			AutoSaveCharData();
		if (r & 1)
		{
			AutoSaveItemData();
			//
			UpdateClientItemData();		//��sclient��carry item
		}
	}
	// ..... �ܮw .....
	r = 0;
	if (pStorageSave = GetStorageData())
	{
		//for (cnt1=0; cnt1<gameMAX_STORAGEITEM; cnt1++)
		val = gameServer_GetStorageItemMax(GetSaveData());
		for (cnt1=0; cnt1<val; cnt1++)
		{
			r = DuedateItem_CheckSingleItem(pStorageSave->psStoreItem[cnt1].m_Item.itemCode, &pStorageSave->psStoreItem[cnt1].m_Item.itemUID, 1, (char *)&pStorageSave->psStoreItem[cnt1], sizeof(pStorageSave->psStoreItem[cnt1]));
		}
	}
	if (r)
	{
		AutoSaveStorageData();
		//
		UpdateClientStorageData();
	}
}

void CMapPlayer::DuedateItem_DeleteItem(long item_code, struct itemUIDDATA * pItemUID)
{
	struct plrDATA_SAVE * pSave;
	struct plrCARRYITEM_SAVE * pItemSave;
	struct plrSTORAGEITEM_SAVE * pStorageSave;
	long r, val, cnt1, item_idx;
	struct plrDATASHOWSELF Self;
	//
	CMapCharMsg * pMsg;
	char * dataPtr;
	long dataSize;
	struct itemDATA_SAVE nItemSave;

	pSave = GetSaveData();
	// ..... �˳� .....
	if ((pSave->plrWawaWeaponR.nItemCode == item_code) && (*(double *)&pSave->plrWawaWeaponR.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaWeaponR;
		dataSize = sizeof(pSave->plrWawaWeaponR);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaArmor.nItemCode == item_code) && (*(double *)&pSave->plrWawaArmor.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaArmor;
		dataSize = sizeof(pSave->plrWawaArmor);
		r = 2;
		//
mrh001:
 #ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("DEBUG: Duedate Item delete detail(%d,%d)", item_code, r);
 #endif
		// �M��
		memset(dataPtr, 0, dataSize);
		// �q�����a
		pMsg = ReplaceMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		if (pMsg->m_nIsNewInsert)
		{
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
		}
		else
		{
			if (pMsg->m_Msg.m_UpdateData2Msg.GetCount() >= UPDATE_DATA_PART_MAX)
			{
				pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
			}
		}
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ITEM_TIMEOUT, 0, item_code);

		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		// �gLog
		memset(&nItemSave, 0, sizeof(nItemSave));
		memcpyT(&nItemSave.m_Item.itemUID, pItemUID, sizeof(nItemSave.m_Item.itemUID));
		nItemSave.m_Item.itemCode = (unsigned short)item_code;
		nItemSave.m_Item.itemNumber = 1;
		CMapServer::GetServer()->SendLogMessage_Item( (CMapPlayer *)this, LOGTYPE_ITEM_TIMEOUT_LOST, 0, &nItemSave );
		// ���s�p��t���P���
		if (r)
		{
			if (r & (1 | 2))
			{
				gameServer_CalcCharacterWeight(GetCharData(), GetItemData());
				gameServer_CalcCharacterAttribute(GetCharData());
				::memset(&Self, 0, sizeof(plrDATASHOWSELF));
				::gameServer_MakeShowSelf(GetCharData(), &Self);
				::msgSendData( GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
				UpdateShowData();
				//
				if (r & 2)
					AutoSaveCharData();
				if (r & 1)
				{
					AutoSaveItemData();
					//
					//UpdateClientItemData();		//��sclient��carry item
					UpdateClientSingleItem(item_idx);
				}
			}
			if (r & 4)
			{
				AutoSaveStorageData();
				//
				UpdateClientStorageData();
			}
		}
		return;
	}
	if ((pSave->plrWawaHead.nItemCode == item_code) && (*(double *)&pSave->plrWawaHead.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaHead;
		dataSize = sizeof(pSave->plrWawaHead);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaFoot.nItemCode == item_code) && (*(double *)&pSave->plrWawaFoot.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaFoot;
		dataSize = sizeof(pSave->plrWawaFoot);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaP.nItemCode == item_code) && (*(double *)&pSave->plrWawaP.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaP;
		dataSize = sizeof(pSave->plrWawaP);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaHorse.nItemCode == item_code) && (*(double *)&pSave->plrWawaHorse.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaHorse;
		dataSize = sizeof(pSave->plrWawaHorse);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaAllShape.nItemCode == item_code) && (*(double *)&pSave->plrWawaAllShape.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaAllShape;
		dataSize = sizeof(pSave->plrWawaAllShape);
		r = 2;
		goto mrh001;
	}
	//��˯���ܳ���
	if ((pSave->plrWawaShowWeaponR.nItemCode == item_code) && (*(double *)&pSave->plrWawaShowWeaponR.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaShowWeaponR;
		dataSize = sizeof(pSave->plrWawaShowWeaponR);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaShowArmor.nItemCode == item_code) && (*(double *)&pSave->plrWawaShowArmor.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaShowArmor;
		dataSize = sizeof(pSave->plrWawaShowArmor);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaShowHead.nItemCode == item_code) && (*(double *)&pSave->plrWawaShowHead.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaShowHead;
		dataSize = sizeof(pSave->plrWawaShowHead);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaShowFoot.nItemCode == item_code) && (*(double *)&pSave->plrWawaShowFoot.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaShowFoot;
		dataSize = sizeof(pSave->plrWawaShowFoot);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaShowP.nItemCode == item_code) && (*(double *)&pSave->plrWawaShowP.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaShowP;
		dataSize = sizeof(pSave->plrWawaShowP);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaShowHorse.nItemCode == item_code) && (*(double *)&pSave->plrWawaShowHorse.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaShowHorse;
		dataSize = sizeof(pSave->plrWawaShowHorse);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrWawaShowAllShape.nItemCode == item_code) && (*(double *)&pSave->plrWawaShowAllShape.nUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrWawaShowAllShape;
		dataSize = sizeof(pSave->plrWawaShowAllShape);
		r = 2;
		goto mrh001;
	}
	// �˳�
#ifdef USE_DUEDATE_ITEM
	if ((pSave->plrEquipWeaponR.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipWeaponR.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipWeaponR;
		dataSize = sizeof(pSave->plrEquipWeaponR);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipWeaponR2.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipWeaponR2.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipWeaponR2;
		dataSize = sizeof(pSave->plrEquipWeaponR2);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipWeaponL.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipWeaponL.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipWeaponL;
		dataSize = sizeof(pSave->plrEquipWeaponL);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipWeaponL2.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipWeaponL2.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipWeaponL2;
		dataSize = sizeof(pSave->plrEquipWeaponL2);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipHorse.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipHorse.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipHorse;
		dataSize = sizeof(pSave->plrEquipHorse);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipArmor.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipArmor.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipArmor;
		dataSize = sizeof(pSave->plrEquipArmor);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipHead.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipHead.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipHead;
		dataSize = sizeof(pSave->plrEquipHead);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipFoot.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipFoot.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipFoot;
		dataSize = sizeof(pSave->plrEquipFoot);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipP.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipP.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipP;
		dataSize = sizeof(pSave->plrEquipP);
		r = 2;
		goto mrh001;
	}
	//
	if ((pSave->plrEquipUnderwear.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipUnderwear.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipUnderwear;
		dataSize = sizeof(pSave->plrEquipUnderwear);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipGlove.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipGlove.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipGlove;
		dataSize = sizeof(pSave->plrEquipGlove);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipOther1.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipOther1.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipOther1;
		dataSize = sizeof(pSave->plrEquipOther1);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipOther2.m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipOther2.m_Item.itemUID == *(double *)pItemUID))
	{
		dataPtr = (char *)&pSave->plrEquipOther2;
		dataSize = sizeof(pSave->plrEquipOther2);
		r = 2;
		goto mrh001;
	}
	//xavi�t�����
	if ((pSave->plrEquipJade.m_Item.itemCode == item_code) && (*(double*)&pSave->plrEquipJade.m_Item.itemUID == *(double*)pItemUID))
	{
		dataPtr = (char*)&pSave->plrEquipJade;
		dataSize = sizeof(pSave->plrEquipJade);
		r = 2;
		goto mrh001;
	}
	if ((pSave->plrEquipHideWeapon.m_Item.itemCode == item_code) && (*(double*)&pSave->plrEquipHideWeapon.m_Item.itemUID == *(double*)pItemUID))
	{
		dataPtr = (char*)&pSave->plrEquipHideWeapon;
		dataSize = sizeof(pSave->plrEquipHideWeapon);
		r = 2;
		goto mrh001;
	}

 #ifdef USE_EQUIP_CARD
	for(int i=0; i < gameMAX_EQUIP_CARD; ++i)
	{
		if ((pSave->plrEquipCard[i].m_Item.itemCode == item_code) && (*(double *)&pSave->plrEquipCard[i].m_Item.itemUID == *(double *)pItemUID))
		{
			dataPtr = (char *)&pSave->plrEquipCard[i];
			dataSize = sizeof(pSave->plrEquipCard[i]);
			r = 2;
			goto mrh001;
		}
	}
#endif

#endif
	// ..... ���W��a .....
	if (pItemSave = GetItemData())
	{
		//for (cnt1=0; cnt1<gameMAX_CARRYITEM; cnt1++)
		val = ::gameServer_GetCarryItemMax(pSave);
		for (cnt1=0; cnt1<val; cnt1++)
		{
			if ((pItemSave->plrCarryItem[cnt1].m_Item.itemCode == item_code) && (*(double *)&pItemSave->plrCarryItem[cnt1].m_Item.itemUID == *(double *)pItemUID))
			{
				dataPtr = (char *)&pItemSave->plrCarryItem[cnt1];
				dataSize = sizeof(pItemSave->plrCarryItem[cnt1]);
				r = 1;
				item_idx = cnt1;
				goto mrh001;
			}
		}
	}
	// ..... �ܮw .....
	if (pStorageSave = GetStorageData())
	{
		//for (cnt1=0; cnt1<gameMAX_STORAGEITEM; cnt1++)
		val = gameServer_GetStorageItemMax(GetSaveData());
		for (cnt1=0; cnt1<val; cnt1++)
		{
			if ((pStorageSave->psStoreItem[cnt1].m_Item.itemCode == item_code) && (*(double *)&pStorageSave->psStoreItem[cnt1].m_Item.itemUID == *(double *)pItemUID))
			{
				dataPtr = (char *)&pStorageSave->psStoreItem[cnt1];
				dataSize = sizeof(pStorageSave->psStoreItem[cnt1]);
				r = 4;
				goto mrh001;
			}
		}
	}
}

void CMapPlayer::DuedateItem_Process()
{
	static unsigned long time_update_tick = 0;
	long cur_time, i, size;

	if (time_update_tick == 0)
	{
		time_update_tick = apiGetTickCounter();
		return;
	}
	//time_update_tick += CMapServer::GetServer()->GetLoopTickCount();
	//if (time_update_tick >= 5000)
	if (abs((long)(apiGetTickCounter() - time_update_tick)) >= 5000)
	{
		std::map<double, struct DUEDATE_ITEM_DATA>::iterator iter;
		std::map<double, struct DUEDATE_ITEM_DATA>::iterator iter_end;
		std::vector<double> vDel;

	//	time_update_tick = 0;
		//
		iter = m_mapDuedateItem.begin();
		iter_end = m_mapDuedateItem.end();
		if (iter != iter_end)
		{
			cur_time = CMapServer::GetServer()->GetLoopTime();
			while(iter != iter_end)
			{
				if (cur_time >= iter->second.nDuedate)
				{
					// �R�����~
 #ifndef USE_PACKAGE_DATA
					CMapServer::GetServer()->Log("DEBUG: Duedate Item delete(%s,%d)", GetName(), iter->second.nItemCode);
 #endif
					DuedateItem_DeleteItem(iter->second.nItemCode, (struct itemUIDDATA *)&iter->first);
					//
					vDel.push_back(iter->first);
				}
				iter++;
			}
			//
			size = vDel.size();
			if (size)
			{
				for (i=0; i<size; i++)
					m_mapDuedateItem.erase(vDel[i]);
			}
		}
		//
		time_update_tick = apiGetTickCounter();
	}
}

void CMapPlayer::TimedMapPass_ClearMarkIfSameTp(unsigned short tpMapCode)
{
	if (!tpMapCode || GetSaveData()->plrMarkMapCode != tpMapCode)
		return;
	GetSaveData()->plrMarkMapCode = 0;
	GetSaveData()->plrMarkPosX = 0;
	GetSaveData()->plrMarkPosY = 0;
	CMapCharMsg * pMk = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	if (pMk)
	{
		pMk->m_Msg.m_UpdateData2Msg.Reset();
		pMk->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_SET_MARK_INFO, 0, 0, 0);
		pMk->m_nSize = (short)pMk->m_Msg.m_UpdateData2Msg.GetSize();
	}
	AutoSaveCharData();
}

namespace {

bool map_IsTimedTpExclusiveDestination(unsigned short mapCode)
{
	long id;

	if (!mapCode)
		return false;
	for (id = 1; id <= gameItemMaxNumber; id++)
	{
		struct itemDATA * pDef = gameGetItemPtr(id);
		if (!pDef)
			continue;
		if (pDef->itemUsageMark != itemUsageMark_TimedTeleportPass)
			continue;
		if (pDef->itemUseDuedate <= 0 || !pDef->itemTPMapCode)
			continue;
		if (pDef->itemTPMapCode == mapCode)
			return true;
	}
	return false;
}

} // namespace

void CMapPlayer::TimedMapPass_EnforceExclusiveMap()
{
	long curTime;
	unsigned short curMap;
	struct plrCARRYITEM_SAVE * pItemData;
	long j;

	if (!IsReady() || !GetMapCtrl())
		return;
	if (IsDead())
		return;
	if (GetSaveData()->plrBaseSPCFlag & (spcFLAG_GM | spcFLAG_GM2))
		return;

	curMap = GetMapCode();
	if (!curMap || !map_IsTimedTpExclusiveDestination(curMap))
		return;

	curTime = CMapServer::GetServer()->GetLoopTime();
	pItemData = GetItemData();
	if (!pItemData)
		return;

	for (j = 0; j < gameMAX_CARRYITEM; j++)
	{
		struct itemDATA_SAVE * slot = &pItemData->plrCarryItem[j];
		struct itemDATA * pDef;

		if (!slot->m_Item.itemCode)
			continue;
		if (slot->m_Item.itemFlags & (itemSHOW_FLAG_SOUL | itemSHOW_FLAG_STICKET | itemSHOW_FLAG_CARD | itemSHOW_FLAG_ALL_CNPC))
			continue;
		pDef = gameGetItemPtr(slot->m_Item.itemCode);
		if (!pDef || pDef->itemUsageMark != itemUsageMark_TimedTeleportPass)
			continue;
		if (pDef->itemTPMapCode != curMap)
			continue;
		if (slot->m_Item.itemTimedMapExpireAt > curTime)
			return;
	}

	CMapServer::GetServer()->ChangeSaveMap(this, GetSaveData()->plrSaveMapCode, GetSaveData()->plrSavePosX, GetSaveData()->plrSavePosY);
}

void CMapPlayer::TimedMapPass_Process()
{
	long cur_time, i;
	long any = 0;
	unsigned long nowTick = apiGetTickCounter();

	if (m_nTimedMapPassLastProcTick == 0)
	{
		m_nTimedMapPassLastProcTick = nowTick;
		return;
	}
	if (abs((long)(nowTick - m_nTimedMapPassLastProcTick)) < 5000)
		return;

	struct plrCARRYITEM_SAVE * pItemData = GetItemData();
	if (!pItemData || !CanSaveData())
	{
		m_nTimedMapPassLastProcTick = nowTick;
		return;
	}

	cur_time = CMapServer::GetServer()->GetLoopTime();
	for (i = 0; i < gameMAX_CARRYITEM; i++)
	{
		struct itemDATA_SAVE * slot = &pItemData->plrCarryItem[i];
		if (!slot->m_Item.itemCode)
			continue;
		if (slot->m_Item.itemFlags & (itemSHOW_FLAG_SOUL | itemSHOW_FLAG_STICKET | itemSHOW_FLAG_CARD | itemSHOW_FLAG_ALL_CNPC))
			continue;
		if (slot->m_Item.itemTimedMapExpireAt == 0)
			continue;
		if (slot->m_Item.itemTimedMapExpireAt > cur_time)
			continue;

		struct itemDATA * pDef = gameGetItemPtr(slot->m_Item.itemCode);
		if (!pDef || pDef->itemUsageMark != itemUsageMark_TimedTeleportPass)
			continue;

		unsigned short kickTpMap = pDef->itemTPMapCode;
		long w = pDef->itemWeight * slot->m_Item.itemNumber;

		struct MAP_USE_ITEM_RESULT sendData;
		memset(&sendData, 0, sizeof(sendData));
		sendData.sourcePosID = i;
		::gameServer_Item_MakeShowSelf(slot, &sendData.itemSelf);

		memset(slot, 0, sizeof(struct itemDATA_SAVE));
		GetCharData()->plrWeight -= w;
		if (GetCharData()->plrWeight < 0)
			GetCharData()->plrWeight = 0;
		any = 1;

		TimedMapPass_ClearMarkIfSameTp(kickTpMap);
		if (kickTpMap && GetSaveData()->plrMapCode == kickTpMap)
			CMapServer::GetServer()->ChangeSaveMap(this, GetSaveData()->plrSaveMapCode, GetSaveData()->plrSavePosX, GetSaveData()->plrSavePosY);

		::msgSendData(GetClientID(), 0, PROTO_MAP_USE_ITEM_RESULT, (char *)&sendData, sizeof(sendData), 0);
		UpdateClientSingleItem(i);
	}

	m_nTimedMapPassLastProcTick = nowTick;
	if (any)
	{
		AutoSaveItemData();
		UpdateClientGoldAndWeight();
	}
}

void CMapPlayer::TimedMapPass_HUDNotify()
{
	unsigned long now = apiGetTickCounter();

	if (m_nTimedMapHudLastNotifyTick && abs((long)(now - m_nTimedMapHudLastNotifyTick)) < 1000)
		return;

	struct plrCARRYITEM_SAVE * pItemData = GetItemData();
	if (!pItemData || !CanSaveData())
	{
		m_nTimedMapHudLastNotifyTick = now;
		return;
	}

	long cur_time = CMapServer::GetServer()->GetLoopTime();
	unsigned short curMap = GetMapCode();
	long best_left = 0;

	for (long j = 0; j < gameMAX_CARRYITEM; j++)
	{
		struct itemDATA_SAVE * slot = &pItemData->plrCarryItem[j];
		if (!slot->m_Item.itemCode)
			continue;
		if (slot->m_Item.itemFlags & (itemSHOW_FLAG_SOUL | itemSHOW_FLAG_STICKET | itemSHOW_FLAG_CARD | itemSHOW_FLAG_ALL_CNPC))
			continue;
		struct itemDATA * pDef = gameGetItemPtr(slot->m_Item.itemCode);
		if (!pDef || pDef->itemUsageMark != itemUsageMark_TimedTeleportPass)
			continue;
		if (!pDef->itemTPMapCode || pDef->itemTPMapCode != curMap)
			continue;
		if (slot->m_Item.itemTimedMapExpireAt <= cur_time)
			continue;
		long left = slot->m_Item.itemTimedMapExpireAt - cur_time;
		if (left > 0 && (best_left == 0 || left < best_left))
			best_left = left;
	}

	if (best_left > 0 && curMap)
	{
		CMapCharMsg * pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		if (pMsg)
		{
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_MAPSPACE_NOTIFY, 0, best_left, curMap);
			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}
	}
	/* 不主動發 0 清倒計時，避免衝到 MapSpace 等系統；換圖時客戶端會清 nMapSpaceTime */

	m_nTimedMapHudLastNotifyTick = now;
}

// �ˬd�÷s�W����(�խܩ�쨭�W��)
// return: 1 �w���( dataPtr & dataSize = �۰ʲM��, ���s�� )
long CMapPlayer::DuedateItem_CheckSingleItem(long item_code, struct itemUIDDATA * pItemUID, long is_add_rec, char * dataPtr, long dataSize)
{
	struct itemDATA * pItem;
	long cur_time;
	CMapCharMsg * pMsg;
	struct itemDATA_SAVE nItemSave;
	struct DUEDATE_ITEM_DATA nRec;

	if (item_code)
	{
		pItem = gameGetItemPtr(item_code);
		if (pItem)
		{
 #ifdef USE_DUEDATE_ITEM
			if (pItem->itemType2 & (itemType2WAWA | itemType2DUEDATE))
 #else
			if (pItem->itemType2 & itemType2WAWA)		// ���w�˳���
 #endif
			{
				/* usage_mark 时效传送：use_duedate 表示地图内秒数，勿按 UID+到期删除 */
				if (pItem->itemUsageMark == itemUsageMark_TimedTeleportPass)
					return 0;
				if (pItem->itemUseDuedate)				// ���]�w����ɶ�
				{
					cur_time = CMapServer::GetServer()->GetLoopTime();
					long n = pItemUID->iUID_Time + pItem->itemUseDuedate;
					if (cur_time >= n)					// �L��
					{
						if (dataPtr && dataSize)
						{
							// �M��
							memset(dataPtr, 0, dataSize);
							// �q�����a
							pMsg = ReplaceMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
							if (pMsg->m_nIsNewInsert)
							{
								pMsg->m_Msg.m_UpdateData2Msg.Reset();
							}
							else
							{
								if (pMsg->m_Msg.m_UpdateData2Msg.GetCount() >= UPDATE_DATA_PART_MAX)
								{
									pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
									pMsg->m_Msg.m_UpdateData2Msg.Reset();
								}
							}
							pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ITEM_TIMEOUT, 0, item_code);
							
							pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
							// �gLog
							memset(&nItemSave, 0, sizeof(nItemSave));
							memcpyT(&nItemSave.m_Item.itemUID, pItemUID, sizeof(nItemSave.m_Item.itemUID));
							nItemSave.m_Item.itemCode = (unsigned short)item_code;
							nItemSave.m_Item.itemNumber = 1;
							CMapServer::GetServer()->SendLogMessage_Item( (CMapPlayer *)this, LOGTYPE_ITEM_TIMEOUT_LOST, 0, &nItemSave );
							//
 #ifndef USE_PACKAGE_DATA
							CMapServer::GetServer()->Log("DEBUG: Delete duedate Item(%s,%d)", GetName(), item_code);
 #endif
						}
						//
						return(1);
					}
					else
					{
						nRec.nDuedate = n;			// n = ����ɶ�
						nRec.nItemCode = (unsigned short)item_code;
						m_mapDuedateItem[*(double *)pItemUID] = nRec;
 #ifndef USE_PACKAGE_DATA
						CMapServer::GetServer()->Log("DEBUG: Duedate Item record(%s,%d)(%d)", GetName(), item_code, n);
 #endif
					}
				}
			}
		}
	}
	return(0);
}

// �i�J�i�[��������γ]�w(CB_InitClientReady �ɩI�s�]�w)
void CMapPlayer::SetInvisibleEnterStage()
{
#ifdef USE_INVISIBLE_ENTER_MAP
	long map_code;
	struct gameHISTORYBATTLE_SET_DATA * p;

	if (!IsGM(0))
	{
		map_code = GetMapCode();
		// OrgPVP �Գ�
		if (p = gameGetHistoryBattleSetPtrByMap(map_code))
		{
			if (p->hsdType == gameHISTORY_TYPE_10)
			{
				if (!CMapServer::GetServer()->GetHistoryOrgPVPManager()->HiOP_InnerGetPlayerTeamID(this))
				{
					if (!(GetSaveData()->plrBaseSPCFlag & spcFLAG_INVISIBLE))
					{
						GetSaveData()->plrBaseSPCFlag |= spcFLAG_INVISIBLE;
						GetCharData()->plrSPCFlag |= spcFLAG_INVISIBLE;
						m_nCharFlags |= CHAR_FLAG_GM_NO_APPEAR;
						//
						UpdateShowData();
						CarryNPC_Update();
					}
					return;
				}
			}
		}
		// ��L�����۰�����
		//
		if (GetSaveData()->plrBaseSPCFlag & spcFLAG_INVISIBLE)
		{
			GetSaveData()->plrBaseSPCFlag &= ~(spcFLAG_INVISIBLE | spcFLAG_GODMODE);
			GetCharData()->plrSPCFlag &= ~(spcFLAG_INVISIBLE | spcFLAG_GODMODE);
			m_nCharFlags &= ~CHAR_FLAG_GM_NO_APPEAR;
			//
			UpdateShowData();
			CarryNPC_Update();
		}
	}
#endif
}

void CMapPlayer::ProcWaitMerryResponseFail()
{
	CMapPlayer * pDest;
	struct gameOFFICER_DATA * pOffice;
	long off_id;
	char * off_name, * off_name2;
	char tmpstr[512];

	if (m_nPlayerFlags & PLAYER_FLAGS_WAIT_MERRY)
	{
		if (m_nWaitMerryTime)
		{
			if (!GetSaveData()->plrMarry_UID)		// �|�����B
			{
				off_name = gameGetResourceName(0);
				off_name2 = gameGetResourceName(0);
				if (off_id = gameGetCharacterOfficeID(GetCharData()))
				{
					if (pOffice = gameGetOfficerPtr(off_id))
					{
						off_name = gameGetResourceName(pOffice->odNameID);
					}
				}
				if (pOffice = gameGetOfficerPtr(m_nMerryPlayerOfficer))
				{
					off_name2 = gameGetResourceName(pOffice->odNameID);
				}
				// 21367		@6%s@5%s@1�V@6%s@5%s@1���D�B���ѤF�I
				wsprintf(tmpstr, gameGetResourceName(21367), off_name, GetName(), off_name2, m_szMerryPlayerName);
				//
				CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(-2, tmpstr, gameSPMSG_WAV_GENTALK, NULL, 0);
				// �����U���D�B����
				CMapServer::GetServer()->SetMerryDelay(GetUID());
			}
			//
			if (m_nMerryPlayerUID)
			{
				pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(m_nMerryPlayerUID);
				if (pDest)
				{
					if (pDest->m_nMerryPlayerUID == GetUID())
					{
						pDest->m_nPlayerFlags &= ~PLAYER_FLAGS_WAIT_MERRY;
						pDest->m_nMerryPlayerUID = 0;
						// �q������
						pDest->ProcAskMerry(NULL);
					}
 #ifndef USE_PACKAGE_DATA
					else
						CMapServer::GetServer()->Log("DEBUG ERROR: Dest merry player uid not match(%d,%d)", pDest->m_nMerryPlayerUID, GetUID());
 #endif
				}
			}
		}
		//
		m_nPlayerFlags &= ~PLAYER_FLAGS_WAIT_MERRY;
		m_nWaitMerryTime = 0;
		m_nMerryPlayerUID = 0;
		m_szMerryPlayerName[0] = 0;
		m_nMerryPlayerOfficer = 0;
	}
}

void CMapPlayer::ProcAskMerry(CMapPlayer * pDest)
{
	struct MAP_ASK_MERRY nAskMerry;
	struct gameOFFICER_DATA * pOffice;
	long off_id;
	char * off_name, * off_name2;
	char tmpstr[512];

	if (!pDest)
	{
		memset(&nAskMerry, 0, sizeof(nAskMerry));
		::msgSendData( GetClientID(), 0, PROTO_MAP_ASK_MERRY, (char *)&nAskMerry, sizeof(nAskMerry), 0 );
	}
	else
	{
		nAskMerry.m_nOffice = gameGetCharacterOfficeID(GetCharData());
		msg_strncpy(nAskMerry.szPlayerName, GetName(), sizeof(nAskMerry.szPlayerName));
		::msgSendData( pDest->GetClientID(), 0, PROTO_MAP_ASK_MERRY, (char *)&nAskMerry, sizeof(nAskMerry), 0 );
		//
		off_name = gameGetResourceName(0);
		off_name2 = gameGetResourceName(0);
		if (off_id = gameGetCharacterOfficeID(GetCharData()))
		{
			if (pOffice = gameGetOfficerPtr(off_id))
			{
				off_name = gameGetResourceName(pOffice->odNameID);
			}
		}
		if (off_id = gameGetCharacterOfficeID(pDest->GetCharData()))
		{
			if (pOffice = gameGetOfficerPtr(off_id))
			{
				off_name2 = gameGetResourceName(pOffice->odNameID);
			}
		}
		// 21364		@6%s@5%s@1�V@6%s@5%s@1���X�D�B���ШD�I
		wsprintf(tmpstr, gameGetResourceName(21364), off_name, GetName(), off_name2, pDest->GetName());
		//
		CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(-2, tmpstr, gameSPMSG_WAV_GENTALK, NULL, 0);
	}
}

void CMapPlayer::ProcMerrySuccess(CMapPlayer * pDest)
{
	CMapCharMsg * pMsg;
	//
	struct gameOFFICER_DATA * pOffice;
	long off_id, m_time;
	char * off_name, * off_name2;
	char tmpstr[512];

	off_name = gameGetResourceName(0);
	off_name2 = gameGetResourceName(0);
	if (off_id = gameGetCharacterOfficeID(GetCharData()))
	{
		if (pOffice = gameGetOfficerPtr(off_id))
		{
			off_name = gameGetResourceName(pOffice->odNameID);
		}
	}
	if (off_id = gameGetCharacterOfficeID(pDest->GetCharData()))
	{
		if (pOffice = gameGetOfficerPtr(off_id))
		{
			off_name2 = gameGetResourceName(pOffice->odNameID);
		}
	}
	// 21366		@6%s@5%s@1�P@6%s@5%s@1���Q�����s�z�A�ЦU�쵹�����֡I
	wsprintf(tmpstr, gameGetResourceName(21366), off_name, GetName(), off_name2, pDest->GetName());
	CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(-2, tmpstr, gameSPMSG_WAV_GENTALK, NULL, 0);
	// 21368		�b���@�����l���I
	CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(-2, gameGetResourceName(21368), gameSPMSG_WAV_PLAYERTALK, pDest, 1);
	// 21369		�b�a�@���s�z�K�I
	CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(-2, gameGetResourceName(21369), gameSPMSG_WAV_PLAYERTALK, this, 1);
	// �����U���D�B����
	CMapServer::GetServer()->ClearMerryDelay(GetUID());
	CMapServer::GetServer()->ClearMerryDelay(pDest->GetUID());
	// �������
	m_time = CMapServer::GetServer()->GetLoopTime();
	GetSaveData()->plrMarry_UID = pDest->GetUID();
	//GetSaveData()->plrMarry_CreateTime = m_time;
	SaveCharData();
	pDest->GetSaveData()->plrMarry_UID = GetUID();
	//pDest->GetSaveData()->plrMarry_CreateTime = m_time;
	pDest->SaveCharData();
	// �M���S������
	SetMissionState(MARRY_SPECIAL_MISSION_1, gameMISSION_STATE_NONE);
	SetMissionState(MARRY_SPECIAL_MISSION_2, gameMISSION_STATE_NONE);
	SendMissionDataToClient();
	SaveMissionData(0);
	pDest->SetMissionState(MARRY_SPECIAL_MISSION_1, gameMISSION_STATE_NONE);
	pDest->SetMissionState(MARRY_SPECIAL_MISSION_2, gameMISSION_STATE_NONE);
	pDest->SendMissionDataToClient();
	pDest->SaveMissionData(0);
	// ��s����t�����
	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	if (pMsg)
	{
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_MATE_NOTIFY, 0, GetSaveData()->plrMarry_UID); // data1=�t��uid��s

		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
	}
	pMsg = pDest->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	if (pMsg)
	{
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_MATE_NOTIFY, 0, pDest->GetSaveData()->plrMarry_UID); // data1=�t��uid��s
		
		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
	}
	// ��s Login Server �t�����
	// ���ݭn�G�ĥΧY�ɸ߰ݰt���W�u���p
	//
	m_nPlayerFlags &= ~PLAYER_FLAGS_WAIT_MERRY;
	m_nWaitMerryTime = 0;
	m_nMerryPlayerUID = 0;
	m_szMerryPlayerName[0] = 0;
	m_nMerryPlayerOfficer = 0;
	pDest->m_nPlayerFlags &= ~PLAYER_FLAGS_WAIT_MERRY;
	pDest->m_nWaitMerryTime = 0;
	pDest->m_nMerryPlayerUID = 0;
	pDest->m_szMerryPlayerName[0] = 0;
	pDest->m_nMerryPlayerOfficer = 0;
	// Log
	struct LOG_INSERT_MSG_ACT nLogAct;

	memset(&nLogAct, 0, sizeof(nLogAct));
	nLogAct.nType = LOGTYPE_ACT_MARRY;
	nLogAct.nMapCode = GetMapCode();
	nLogAct.nPosX = GetPosX();
	nLogAct.nPosY = GetPosY();
	::msg_strncpy(nLogAct.nName, GetName(), sizeof(nLogAct.nName));
	nLogAct.nData1 = GetUID();
	nLogAct.nData2 = pDest->GetUID();
	::msg_strncpy(nLogAct.nStr1, pDest->GetName(), sizeof(nLogAct.nStr1));
	CMapServer::GetServer()->SendLogMessage_Act(&nLogAct);
	//
	memset(&nLogAct, 0, sizeof(nLogAct));
	nLogAct.nType = LOGTYPE_ACT_MARRY;
	nLogAct.nMapCode = pDest->GetMapCode();
	nLogAct.nPosX = pDest->GetPosX();
	nLogAct.nPosY = pDest->GetPosY();
	::msg_strncpy(nLogAct.nName, pDest->GetName(), sizeof(nLogAct.nName));
	nLogAct.nData1 = pDest->GetUID();
	nLogAct.nData2 = GetUID();
	::msg_strncpy(nLogAct.nStr1, GetName(), sizeof(nLogAct.nStr1));
	CMapServer::GetServer()->SendLogMessage_Act(&nLogAct);
}

void CMapPlayer::ProcDivorce()
{
	CMapCharMsg * pMsg;
	unsigned long marry_uid;

	marry_uid = GetSaveData()->plrMarry_UID;
	//
	GetSaveData()->plrMarry_UID = 0;
	SaveCharData();
	SendMsgToClientByID( 21382 );	// �A�w�짴���B����A�ثe���L�t�����A
	// ��s�t�����
	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	if (pMsg)
	{
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_MATE_NOTIFY, 0, 0); // data1=�t��uid��s

		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
	}
	// Log
	struct LOG_INSERT_MSG_ACT nLogAct;

	memset(&nLogAct, 0, sizeof(nLogAct));
	nLogAct.nType = LOGTYPE_ACT_DIVORCE;
	nLogAct.nMapCode = GetMapCode();
	nLogAct.nPosX = GetPosX();
	nLogAct.nPosY = GetPosY();
	::msg_strncpy(nLogAct.nName, GetName(), sizeof(nLogAct.nName));
	nLogAct.nData1 = GetUID();
	nLogAct.nData2 = marry_uid;
	//::msg_strncpy(nLogAct.nStr1, GetName(), sizeof(nLogAct.nStr1));
	CMapServer::GetServer()->SendLogMessage_Act(&nLogAct);
}

void CMapPlayer::UpdateForceTownInfo()
{
	// �ǰe�դO���
	struct MAP_ASK_WORLD_FORCE_RESULT nRet;
	struct plrDATA_WORLD_FORCE * pForce = CMapServer::GetServer()->GetForceDataArray();

	::memcpyT( nRet.pwForce, pForce, sizeof(nRet.pwForce) );
	nRet.pwfOrgTotal_WEI = CMapServer::GetServer()->pwfOrgTotal_WEI;
	nRet.pwfOrgTotal_SHU = CMapServer::GetServer()->pwfOrgTotal_SHU;
	nRet.pwfOrgTotal_WU = CMapServer::GetServer()->pwfOrgTotal_WU;
	::msgSendData(GetClientID(), 0, PROTO_MAP_ASK_WORLD_FORCE_RESULT, (char*)&nRet, sizeof(nRet), 0);
	//�^�ǫ����դO���
	struct MAP_ASK_WORLD_TOWN_RESULT townMsg;
	struct plrDATA_WORLD_TOWN * pTown = CMapServer::GetServer()->GetTownDataArray();

	::memcpyT( &townMsg.pwTown, pTown, sizeof(townMsg.pwTown) );
	::msgSendData(GetClientID(), 0, PROTO_MAP_ASK_WORLD_TOWN_RESULT, (char*)&townMsg, sizeof(townMsg), 0);
}
// ...........................................
bool CMapPlayer::IsItemEnough(long itemID, long num)
{
	long val;

	if(!gameGetItemPtr(itemID) || num <= 0)
		return false;

	struct plrCARRYITEM_SAVE * pPlrCarryItem = GetItemData();
	long itemNum = 0;

	//���`��
	//for(int i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(GetSaveData());
	for(int i=0; i<val; i++)
	{
		struct itemDATA_SAVE * pItem = &pPlrCarryItem->plrCarryItem[ i ];
		if(itemID == pItem->m_Item.itemCode)
			itemNum += pItem->m_Item.itemNumber;
	}

	//�䤣��
	if(itemNum == 0)
		return false;

	//�p��
	if(num <= itemNum)
		return true;
	else //�ƶq����
		return false;
}

//���o���~�`��
long CMapPlayer::getCarryItemNum(long itemID)
{
	long val;

	if (!gameGetItemPtr(itemID))
		return -1;

	struct plrCARRYITEM_SAVE * pPlrCarryItem = GetItemData();
	long itemNum = 0;

	//���`��
	val = ::gameServer_GetCarryItemMax(GetSaveData());
	for (int i = 0; i<val; i++)
	{
		struct itemDATA_SAVE * pItem = &pPlrCarryItem->plrCarryItem[i];
		if (itemID == pItem->m_Item.itemCode)
			itemNum += pItem->m_Item.itemNumber;
	}

	return itemNum;
}

bool CMapPlayer::IsItemEnoughBless(long itemID, long num, long bless)
{
	long val;

	if(!gameGetItemPtr(itemID) || num <= 0)
		return false;

	struct plrCARRYITEM_SAVE * pPlrCarryItem = GetItemData();
	long itemNum = 0;

	//���`��
	//for(int i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(GetSaveData());
	for(int i=0; i<val; i++)
	{
		struct itemDATA_SAVE * pItem = &pPlrCarryItem->plrCarryItem[ i ];
		if(itemID == pItem->m_Item.itemCode)
			if(gameGetItemBless(pItem->m_Item) >= bless)
				itemNum += pItem->m_Item.itemNumber;
	}

	//�䤣��
	if(itemNum == 0)
		return false;

	//�p��
	if(num <= itemNum)
		return true;
	else //�ƶq����
		return false;
}

bool CMapPlayer::DelItem(long itemID, long num)
{
	long val;

	if( !IsItemEnough(itemID, num) )
		return false;

	struct plrCARRYITEM_SAVE * pPlrCarryItem = GetItemData();

	//for(int i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(GetSaveData());
	for(int i=0; i<val; i++)
	{
		struct itemDATA_SAVE * pItem = &pPlrCarryItem->plrCarryItem[ i ];
		long itemNum = pItem->m_Item.itemNumber;
		if(itemID == pItem->m_Item.itemCode)
		{
			if(num >= itemNum) 
			{
				num -= itemNum;
				pItem->m_Item.itemNumber = 0;
			}
			else 
			{
				pItem->m_Item.itemNumber -= (unsigned short)num;
				num = 0;
				break;
			}

			if(pItem->m_Item.itemNumber == 0)
				::memset(pItem, 0, sizeof(*pItem));
		}
	}

	//�^�Ǫ��~
/*	struct plrCARRYITEM sendData;
	::memset(&sendData, 0, sizeof(sendData));
	::gameServer_MakeCarryItem(pPlrCarryItem, &sendData);
	::msgSendData( ((CMapPlayer *)this)->GetClientID(), 0, PROTO_MAP_GET_PLAYER_CARRY_ITEM_RESULT, (char *)&sendData, sizeof(sendData), 0 ); */

	return true;
}

bool CMapPlayer::DelItemBless(long itemID, long num, long bless)
{
	long val;

	if( !IsItemEnoughBless(itemID, num, bless))
		return false;

	struct plrCARRYITEM_SAVE * pPlrCarryItem = GetItemData();

	val = ::gameServer_GetCarryItemMax(GetSaveData());
	for(int i=0; i<val; i++)
	{
		struct itemDATA_SAVE * pItem = &pPlrCarryItem->plrCarryItem[ i ];
		long itemNum = pItem->m_Item.itemNumber;
		if(itemID == pItem->m_Item.itemCode)
		{
			if(gameGetItemBless(pItem->m_Item) >= bless)
			{
				if(num >= itemNum) 
				{
					num -= itemNum;
					pItem->m_Item.itemNumber = 0;
				}
				else 
				{
					pItem->m_Item.itemNumber -= (unsigned short)num;
					num = 0;
					break;
				}
			}
			if(pItem->m_Item.itemNumber == 0)
				::memset(pItem, 0, sizeof(*pItem));
		}
	}
	return true;
}
	//04/06/10
struct itemDATA_SAVE * CMapPlayer::GetItemPtr(struct itemUIDDATA * pItemUidData,
											unsigned short * pItemIdx)
{
	return ::GetItemPtr(pItemUidData, GetItemData(), pItemIdx);
}

	//04/11/05
struct itemDATA_SAVE * CMapPlayer::GetItemPtr(long itemID, unsigned short * pItemIdx)
{
	return ::GetItemPtr(itemID, GetItemData(), pItemIdx);
}

struct itemDATA_SAVE * CMapPlayer::GetFreeItemPtr(unsigned short * pItemIdx)
{
	struct plrCARRYITEM_SAVE * pItemData;
	long i, imax;
	struct itemDATA_SAVE * pItem;

	pItemData = GetItemData();
	imax = ::gameServer_GetCarryItemMax(GetSaveData());
	for(i=0; i<imax; i++)
	{
		pItem = &pItemData->plrCarryItem[i];
		if (pItem->m_Item.itemCode == 0)
		{
			if (pItemIdx)
				*pItemIdx = (unsigned short)i;
			return pItem;
		}
	}
	return(NULL);
}

struct itemDATA_SAVE * CMapPlayer::GetSingleItemData(long itemIndex)
{
	//if( itemIndex < 0 || itemIndex >= gameMAX_CARRYITEM )
	if( (itemIndex < 0) || (itemIndex >= ::gameServer_GetCarryItemMax(GetSaveData())) )
		return 0;

	return &GetItemData()->plrCarryItem[ itemIndex ];
}

//end xiun : state

struct itemDATA_SAVE* CMapPlayer::GetItemDATA_SAVE(struct itemUIDDATA *uid, int* is_eqwa)
{
	if (is_eqwa)
		*is_eqwa = 0;

	itemDATA_SAVE* r = GetItemPtr(uid, NULL);
	if (r)
		return r;

	struct plrSTORAGEITEM_SAVE* pss = GetStorageData();
	int imax = gameServer_GetStorageItemMax(GetSaveData());
	int i;
	for(i = 0 ; i < imax ; i++)
	{
		if (pss->psStoreItem[i].m_Item.itemUID.iUID_Serial == uid->iUID_Serial &&
			pss->psStoreItem[i].m_Item.itemUID.iUID_Time == uid->iUID_Time)
			return &pss->psStoreItem[i];
	}

	r = &GetSaveData()->plrEquipWeaponL;
	itemDATA_SAVE* ed = &GetSaveData()->plrEquipWeaponR2;

	while(r <= ed)
	{
		if (r->m_Item.itemUID.iUID_Serial == uid->iUID_Serial &&
			r->m_Item.itemUID.iUID_Time == uid->iUID_Time)
			return r;
		r++;
	}

	// �s�ȫ����˳ƪ��~
	struct plrItemSaveData* pwa_beg = &GetSaveData()->plrWawaArmor;
	struct plrItemSaveData* pwa_ed = &GetSaveData()->plrWawaAllShape;
	while(pwa_beg <= pwa_ed)
	{
		//if (pwa_beg->nItemCode == uid->iUID_Serial && pwa_beg->nItemCode == uid->iUID_Time)
		if (pwa_beg->nItemCode == uid->iUID_Serial)
		{
			if (is_eqwa) *is_eqwa = 1;
			return (itemDATA_SAVE*)1;
		}
		pwa_beg++;
	}

	if (GetSaveData()->plrWawaWeaponR.nItemCode == uid->iUID_Serial)
	{
		if (is_eqwa) *is_eqwa = 1;
		return (itemDATA_SAVE*)1;
	}

	return NULL;
}

/* itemDATA_SAVE 为 union：武魂/虎符/魂票/点卡/带兵 NPC 的数量不在 m_Item.itemNumber */
static int AutoSort_IsSpecialCarrySlot(struct itemDATA_SAVE * iobj)
{
	unsigned short f;

	if (!iobj || !iobj->m_Item.itemCode)
		return 0;
	f = iobj->m_Item.itemFlags;
	return (f & (itemSHOW_FLAG_SOUL | itemSHOW_FLAG_STICKET | itemSHOW_FLAG_CARD | itemSHOW_FLAG_ALL_CNPC)) ? 1 : 0;
}

struct PushCarryTest
{
	plrCARRYITEM_SAVE d;
	int index;

	PushCarryTest()
	{
		index = 0;
	}

	void Push(itemDATA_SAVE* obj)
	{
		memcpyT(&d.plrCarryItem[index], obj, sizeof(itemDATA_SAVE));
		index++;
	}

	void PushMergable(itemDATA_SAVE* obj, PushCarryTest* sing_p)
	{
		int i;
		unsigned int count;
		itemDATA_SAVE* iobj;

		for(i = 0 ; i < index ; i++)
		{
			iobj = &d.plrCarryItem[i];
			if (iobj->m_Item.itemCode != obj->m_Item.itemCode)
				continue;

			count = iobj->m_Item.itemNumber + obj->m_Item.itemNumber;
			if (count >= gameMAX_ITEM_STACK)
			{
				count -= gameMAX_ITEM_STACK;
				iobj->m_Item.itemNumber = gameMAX_ITEM_STACK;
				sing_p->Push(iobj);

				if (count == 0)
				{
					memset(iobj, 0, sizeof(itemDATA_SAVE));
				}
				else
				{
					memcpyT(iobj, obj, sizeof(itemDATA_SAVE));
					iobj->m_Item.itemNumber = count;
				}
			}
			else
			{
				iobj->m_Item.itemNumber = (unsigned short)count;
			}
			return;
		}
		//
		for(i = 0 ; i < index ; i++)
		{
			iobj = &d.plrCarryItem[i];

			if (iobj->m_Item.itemCode)
				continue;

			memcpyT(iobj, obj, sizeof(itemDATA_SAVE));
			return;
		}
		//
		Push(obj);
	}
};

void CMapPlayer::AutoSortCarry()
{
	int i, imax;
	plrCARRYITEM_SAVE* s_ptr;
	itemDATA_SAVE* iobj;
	PushCarryTest d_sing, d_multi; // ���������A�i���|�M���i���|

	imax = gameServer_GetCarryItemMax(GetSaveData());
	s_ptr = GetItemData();

	for(i = 0 ; i < imax ; i++)
	{
		iobj = &s_ptr->plrCarryItem[i];

		if (iobj->m_Item.itemCode == 0)
			continue;

		if (AutoSort_IsSpecialCarrySlot(iobj))
		{
			d_sing.Push(iobj);
			continue;
		}

		if (iobj->m_Item.itemNumber == 0)
			continue;

		if (!IsItemMergable(iobj->m_Item.itemCode))
		{
			d_sing.Push(iobj);
			continue;
		}

		// �i���|
		if (iobj->m_Item.itemNumber == gameMAX_ITEM_STACK)
		{
			d_sing.Push(iobj);
			continue;
		}

		d_multi.PushMergable(iobj, &d_sing);
	}

	memset(s_ptr, 0, sizeof(plrCARRYITEM_SAVE));
	memcpyT(&s_ptr->plrCarryItem[0], &d_sing.d.plrCarryItem[0], sizeof(itemDATA_SAVE) * d_sing.index);
	memcpyT(&s_ptr->plrCarryItem[d_sing.index], &d_multi.d.plrCarryItem[0], sizeof(itemDATA_SAVE) * d_multi.index);
	AutoSaveItemData();
}

struct PushStorageTest
{
	plrSTORAGEITEM_SAVE d;
	int index;

	PushStorageTest()
	{
		index = 0;
	}

	void Push(itemDATA_SAVE* obj)
	{
		memcpyT(&d.psStoreItem[index], obj, sizeof(itemDATA_SAVE));
		index++;
	}

	void PushMergable(itemDATA_SAVE* obj, PushStorageTest* sing_p)
	{
		int i;
		unsigned int count;
		itemDATA_SAVE* iobj;

		for(i = 0 ; i < index ; i++)
		{
			iobj = &d.psStoreItem[i];
			if (iobj->m_Item.itemCode != obj->m_Item.itemCode)
				continue;

			count = iobj->m_Item.itemNumber + obj->m_Item.itemNumber;
			if (count >= gameMAX_ITEM_STACK)
			{
				count -= gameMAX_ITEM_STACK;
				iobj->m_Item.itemNumber = gameMAX_ITEM_STACK;
				sing_p->Push(iobj);

				if (count == 0)
				{
					memset(iobj, 0, sizeof(itemDATA_SAVE));
				}
				else
				{
					memcpyT(iobj, obj, sizeof(itemDATA_SAVE));
					iobj->m_Item.itemNumber = count;
				}
			}
			else
			{
				iobj->m_Item.itemNumber = (unsigned short)count;		//xavi�ק� unsigned char-->unsigned short
			}
			return;
		}
		//
		for(i = 0 ; i < index ; i++)
		{
			iobj = &d.psStoreItem[i];

			if (iobj->m_Item.itemCode)
				continue;

			memcpyT(iobj, obj, sizeof(itemDATA_SAVE));
			return;
		}
		//
		Push(obj);
	}
};

void CMapPlayer::AutoSortStorage()
{
	int i, imax;
	plrSTORAGEITEM_SAVE* s_ptr;
	itemDATA_SAVE* iobj;
	PushStorageTest d_sing, d_multi; // ���������A�i���|�M���i���|

	imax = gameServer_GetStorageItemMax(GetSaveData());
	s_ptr = GetStorageData();

	for(i = 0 ; i < imax ; i++)
	{
		iobj = &s_ptr->psStoreItem[i];

		if (iobj->m_Item.itemCode == 0)
			continue;

		if (AutoSort_IsSpecialCarrySlot(iobj))
		{
			d_sing.Push(iobj);
			continue;
		}

		if (iobj->m_Item.itemNumber == 0)
			continue;

		if (!IsItemMergable(iobj->m_Item.itemCode))
		{
			d_sing.Push(iobj);
			continue;
		}

		// �i���|
		if (iobj->m_Item.itemNumber == gameMAX_ITEM_STACK)
		{
			d_sing.Push(iobj);
			continue;
		}

		d_multi.PushMergable(iobj, &d_sing);
	}

	memset(s_ptr->psStoreItem, 0, sizeof(s_ptr->psStoreItem));
	memcpyT(&s_ptr->psStoreItem[0], &d_sing.d.psStoreItem[0], sizeof(itemDATA_SAVE) * d_sing.index);
	memcpyT(&s_ptr->psStoreItem[d_sing.index], &d_multi.d.psStoreItem[0], sizeof(itemDATA_SAVE) * d_multi.index);
	AutoSaveStorageData();
}
	
	
	void CMapPlayer::AddNPCExp(unsigned long nExp)
{
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	unsigned long uid;
	long i, icode;
	struct itemDATA * pItemPtr;
	CMapNPC * pNPC;

	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		pNPCSaveData = &pNPCData->plrNPCData[i];
		if (icode = pNPCSaveData->plrNPC_ItemCode)
		{
			if (pItemPtr = ::gameGetItemPtr(icode))
			{
				uid = pNPCSaveData->plrNPC_UID;
				// ����w�s�b
				pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(uid, CMapNPC::CLASS_ID);
				if (pNPC)
				{
					pNPC->AddExp(nExp, pNPC, this);		// �t�ɯ�
				}
			}
		}
	}
}


long CMapPlayer::CheckSuperSoldier(void)
{
	long i,k;
	CMapPlayer * pPlayer = this;
	struct plrDATA_NPC * pNPCData;
	struct gameCNPC_SKILL* pNpcSkill = NULL;
	struct itemDATA * pItemPtr;

	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		if (pNPCData->plrNPCData[i].plrNPC_UID == 0)
			continue;
		pItemPtr = ::gameGetItemPtr(pNPCData->plrNPCData[i].plrNPC_ItemCode);
		if(!pItemPtr)
			continue;
		for(k = 0 ; k < gameMAX_CNPC_Skill_SETTING; k++)
		{
			pNpcSkill = gameGetCNPC_SkillPtr(k);
			if (pNpcSkill && pNpcSkill->csSolCode == pItemPtr->itemUseMagicID)
				return pNpcSkill->csSolCode;
		}
	}
	return 0;
}

long CMapPlayer::CheckSuperSoldierNotDead(void)
{
	long i,k;
	CMapPlayer * pPlayer = this;
	struct plrDATA_NPC * pNPCData;
	struct gameCNPC_SKILL* pNpcSkill = NULL;
	struct itemDATA * pItemPtr;

	pNPCData = GetNPCData();
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		if (pNPCData->plrNPCData[i].plrNPC_UID == 0)
			continue;
		if (pNPCData->plrNPCData[i].plrNPC_HP == 0)
			continue;
		pItemPtr = ::gameGetItemPtr(pNPCData->plrNPCData[i].plrNPC_ItemCode);		
		if(!pItemPtr)
			continue;
		for(k = 0 ; k < gameMAX_CNPC_Skill_SETTING; k++)
		{
 			pNpcSkill = gameGetCNPC_SkillPtr(k);
			if (pNpcSkill && pNpcSkill->csSolCode == pItemPtr->itemUseMagicID)
				return pNpcSkill->csSolCode;
		}
	}
	return 0;
}

void CMapPlayer::SoulCardMerge(char *pBuffer)
{

	struct MAP_SOULCARD_MERGE* MergeData = (struct MAP_SOULCARD_MERGE*)pBuffer;
	long i, val, MergeExp;
	unsigned long long MergeGold;
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * MergeItem = NULL;
	struct gameEquipCard * cData = NULL;
	CMapCharMsg * pMsg;
	CMapPlayer * pPlayer = this;


	val = ::gameServer_GetCarryItemMax(GetSaveData());
	pItemData = GetItemData();
	for (i=0; i<val; i++)
	{
		if (memcmp(&pItemData->plrCarryItem[i].m_Item.itemUID,&MergeData->MergeCardUID,sizeof(struct itemUIDDATA)) == 0)
		{
			MergeItem = &pItemData->plrCarryItem[i];
			break;
		}
	}
	cData = gameGetEquipCardPtr(MergeItem->m_Item.itemCode);
	if(cData == NULL)// �䤣��Z�N�d���
		return;
	MergeExp = 0;
	MergeGold = 0;
	for (i=0; i<32; i++)
	{
		if(MergeData->item[i].iUID_Serial!=0)
			MergeGold += getSoulCardGold(MergeData->item[i]);
	}
	if(GetSaveData()->plrGold < MergeGold)// ��������
		return;

	for (i=0; i<32; i++)
	{
		if(MergeData->item[i].iUID_Serial!=0)
			MergeExp += getSoulCardExp(MergeData->item[i]);
	}
	MergeItem->m_Item.itemCardExp += MergeExp;
	if(MergeItem->m_Item.itemCardExp >= cData->next_exp && cData->next_code != 0)
	{
		MergeItem->m_Item.itemCardExp -= cData->next_exp;
		MergeItem->m_Item.itemCode = cData->next_code;
	}
	GetSaveData()->plrGold -= MergeGold; //����

	pMsg = InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
	
	pMsg->m_Msg.m_UpdateData2Msg.Reset();
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_LOSEGOLD, 0, MergeGold);
	pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GOLD, 0, GetUID(), GetSaveData()->plrGold);

	pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
	// �g log ...
	::SendGoldLog( MergeGold, pPlayer, LOGTYPE_ITEM_SOULCARD_MERGE_LOSE, 0 ); //��
	//
	AutoSaveCharData();

	//�s��
	UpdateClientItemData();
	AutoSaveItemData();
}


long CMapPlayer::getSoulCardGold(itemUIDDATA UID)
{
	int i,val;
	struct plrCARRYITEM_SAVE * pItemData;
	struct gameEquipCard * cData = NULL;

	val = ::gameServer_GetCarryItemMax(GetSaveData());
	pItemData = GetItemData();
	for (i=0; i<val; i++)
	{
		if (memcmp(&pItemData->plrCarryItem[i].m_Item.itemUID,&UID,sizeof(struct itemUIDDATA)) == 0)
		{
			cData = gameGetEquipCardPtr(pItemData->plrCarryItem[i].m_Item.itemCode);
			if(cData != NULL)
			{				
				return cData->gold;
			}
		}
	}
	return 0;
}

long CMapPlayer::getSoulCardExp(itemUIDDATA UID)
{
	int i,val;
	struct plrCARRYITEM_SAVE * pItemData;
	struct gameEquipCard * cData = NULL;

	val = ::gameServer_GetCarryItemMax(GetSaveData());
	pItemData = GetItemData();
	for (i=0; i<val; i++)
	{
		if (memcmp(&pItemData->plrCarryItem[i].m_Item.itemUID,&UID,sizeof(struct itemUIDDATA)) == 0)
		{
			cData = gameGetEquipCardPtr(pItemData->plrCarryItem[i].m_Item.itemCode);
			if(cData != NULL)
			{
				CMapServer::GetServer()->SendLogMessage_Item(this, LOGTYPE_ITEM_SOULCARD_MERGE_LOSE, NULL, &(pItemData->plrCarryItem[i]));
				::memset(&pItemData->plrCarryItem[i], 0, sizeof(struct itemDATA_SAVE));				
				return cData->exp;
			}
		}
	}
	return 0;
}

bool CMapPlayer::GetArmyDataByKey(unsigned long key)
{
	struct LOGIN_GET_ARMY_DATA_SINGLE nReq;
	int hLogin = CMapServer::GetServer()->GetLoginServer();
	if (CMapServer::GetServer()->IsConnectedToServer(hLogin))
	{
		if (GetSaveData()->plrOrganizeUID)
		{
			nReq.nPlayerUID = GetUID();
			nReq.nDataKey = key;

			CMapServer::GetServer()->SendData(hLogin, 0, PROTO_LOGIN_GET_ARMY_DATA_SINGLE, (char *)&nReq, sizeof(nReq), 0);
			return true;
		}
	}
	return false;
}

void CMapPlayer::ToLogin_AddArmyPointTemp(unsigned int point)
{	
	CMapServer * pServer = CMapServer::GetServer();
	struct LOGIN_ADD_ARMY_POINT nData;

	if( !pServer )
		return;
	
	long hLoginServer = pServer->GetLoginServer();

	if( !pServer->IsConnectedToServer(hLoginServer) )
		return;

	nData.nPlayerUID = GetUID();
	nData.addPoint = point;

	pServer->SendData(hLoginServer, 0, PROTO_LOGIN_ARMY_ADDPOINTTEMP, (char *)&nData, sizeof(nData), 0);
}

void CMapPlayer::ToLogin_AddArmyPoint(unsigned int point)
{	
	CMapServer * pServer = CMapServer::GetServer();
	struct LOGIN_ADD_ARMY_POINT nData;

	if( !pServer )
		return;

	long hLoginServer = pServer->GetLoginServer();

	if( !pServer->IsConnectedToServer(hLoginServer) )
		return;

	nData.nPlayerUID = GetUID();
	nData.addPoint = point;

	pServer->SendData(hLoginServer, 0, PROTO_LOGIN_ARMY_ADDPOINT, (char *)&nData, sizeof(nData), 0);
}
void CMapPlayer::UpdateEightGateTime(unsigned long Time,unsigned char CanOpen)
{
	struct MAP_OPEN_GATE_TIME sendData2;
	sendData2.CheckCanOpen = CanOpen;
	sendData2.OpenGateTime = Time;

	::msgSendData( GetClientID(), 0, PROTO_MAP_OPEN_GATE_TIME, (char *)&sendData2, sizeof(sendData2), 0 );
}
