
// GACHA_SYSTEM_VERSION_20260305_2109
#include "Stdafx.h"
#include "MapServer.h"
#include "MapBossRtRank.h"

// Preload CNPCWEIGHT.TXT (defined in MapServerShop.cpp) for soldier random upgrade.
extern "C" void gameServerPreloadCNPCWeightForUpgrade(void);
extern "C" long gameGetMaxSoldierNumByJobCfg(long jobID);
#include "MapServerMenuLib.h"
#include "MapNPC.h"
#include "MapPlayer.h"
#include "MapMagic.h"
#ifndef CONSOL_MODE
 #include "15MapServer.h"
 #include "MapServerDlg.h"
#endif
#include "MapServerTradePsw.h"
//
//#include "..\\Common\\Limits.h"
//#include "..\\Common\\DataProc.h"
#include "..\\Common\\DataRes.h"
#include "..\\Common\\ItemProc.h"
#include "..\\IncServer\\CriticalSection.h"
#include "..\\Common\\DataProc.h"
#include "..\\Common\\gameServer_Cultivate.h"
#include "..\\CultivateAmuletUpgrade.h"
#include "..\\CultivateWingUpgrade.h"
#include "..\\Common\\MagicProc.h"
#include "..\\Common\\ShopProc.h"
#include "..\\Common\\DataProc_BattleSkill.h"
#include "..\\Common\\Stage.h"
//#include "..\\Common\\Walk_SLG.h"
#include "..\\Common\\random.h"
#include "..\\Common\\Effect_Server.h"
#include "..\\IncServer\\apiwin.h"
#include "..\\IncServer\\Scribe.h"
#include "..\\Common\\R61_Common.h"
#include "..\\Data\\magic_exp2.h"
#include "..\\common\\protocoGM.h"
#include "..\\common\\utility.hpp"
//#include "..\\IncServer\\services.h"
#include "R61_ServerMsgRegister.h"
#include <time.h>

#include <winsock2.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#include "..\\Common\\netapi.h"

//#include "..\\Common\\Base64.h"
//#include "..\\Common\\Random.h"
//#include "..\\Common\\Effect_Server.h"

#include "C_Interface.h"
#include "..\\zip.h"

#include "..\\13LogServer\\Lang_Msg.h"

#ifdef USE_COOL_DOWN_SYSTEM
	#include "..\\Common\\CoolDownProc.h"
#endif

#include <algorithm>
//void InnerProcGenBossDieTeleport(CMapNPC * pNPC);


#define NO_BAN_IP							// ?O?_???n??? Ban IP
//#define USE_CLIENT_DIV_PROCESS				// ????: ????????send ????, ????code(5)

#ifdef USE_PACKAGE_DATA
#include "..\\zippak.h"
#include "..\\IncServer\\Scribe.h"
#define mapOpenFileLocal(szFile)						zpakOpenFile(szFile, 0)
#define mapReadFileLocal(nHandle, pBuffer, nBytes)		zpakReadFile(nHandle, pBuffer, nBytes)
#define mapCloseFileLocal(nHandle)						zpakCloseFile(nHandle)
#define mapGetFileSizeLocal(nHandle)					zpakGetFileSize(nHandle)
#else
#define mapOpenFileLocal(szFile)						apiOpenFile(szFile, 0)
#define mapReadFileLocal(nHandle, pBuffer, nBytes)		apiReadFile(nHandle, pBuffer, nBytes)
#define mapCloseFileLocal(nHandle)						apiCloseFile(nHandle)
#define mapGetFileSizeLocal(nHandle)					apiGetFileSize(nHandle)
#endif

#include "MailProc.h"

#if defined USE_FAQ_SYSTEM
#include "faq.h"
#endif
#include "hDoc.h"
#include "ver.h"

// ????????
extern void InitGachaSystem();

// ??????????
extern void LoadExchangeData();
extern void LoadLegionExchangeData(void);
extern void JinshanDigServer_LoadConfig(void);
extern void TowerSlashServer_LoadConfig(void);
extern void TowerSlashServer_RequestRankFromLogin(void);
extern void TowerSlashServer_SetRankData(const struct TOWER_SLASH_RANK_SAVE* pData);
extern void TowerSlashServer_Update(void);
extern void TowerSlashServer_UpdateClientData(CMapPlayer* pPlayer);
extern long TowerSlashServer_IsPlayerInSession(CMapPlayer* pPlayer);
extern long TowerSlashServer_IsCopyInSession(unsigned short copyUID);

// ?????????????AccessoryBlessProb.txt??
extern "C" void gameLoadAccessoryBlessProbTXT();
// ?????????BlessRule.txt??
extern "C" void gameLoadBlessRuleTXT();
// ????????????CombatPowerRankReward.txt??
extern "C" void gameLoadCombatPowerRankRewardTXT();
extern "C" void gameLoadCombatPowerRankTitlesTXT(void);
// ??????/??????WeaponHorseRankRule.txt??
extern "C" void gameLoadWeaponHorseRankRuleTXT(void);
// ????????????ACCESSORYBLESSDATA.TXT??
extern "C" void gameLoadAccessoryBlessDataTXT();
// ?????????????SoldierEquipAttrConfig.txt??
extern void gameServer_PreloadSoldierEquipAttrConfigTXT(void);
// ???CultivateAmuletUpgrade ?? Map ???????zpak/????
extern "C" void MapServer_RegisterCultivateAmuletFullReader(void);

// BlessRule.txt??????????????
static long g_BlessRuleEnhanceMaxNormal = gameMAX_ITEM_BLESS_NUM;
static long g_BlessRuleEnhanceMaxLoaded = 0;

extern "C" long apiOpenFile(char *filename, long mode);
extern "C" long apiCloseFile(long handle);

static void MapServer_MakeExeOverridePath(char * out, const char * rel)
{
	char exe[512];
	::ZeroMemory(exe, sizeof(exe));
	if (::GetModuleFileName(NULL, exe, sizeof(exe) - 1) > 0)
	{
		char * p = strrchr(exe, '\\');
		if (p) *(p + 1) = 0;
		wsprintf(out, "%s%s", exe, rel);
		return;
	}
	strcpy(out, rel);
}

static long MapServer_BlessRuleScribeLoadFile(const char * filename)
{
	return scribeLoadFile((char *)filename);
}

static void MapServer_LoadBlessEnhanceMaxRule()
{
	if (g_BlessRuleEnhanceMaxLoaded)
		return;

	char attempt0[512], attempt1[512], attempt2[512], attempt3[512];
	char attempt4[512], attempt5[512], attempt6[512], attempt7[512];
	// ??????MapServer ????Map ?????????????? pak??
	char override0[512], override1[512], override2[512], override3[512];
	MapServer_MakeExeOverridePath(override0, "BlessRule.txt");
	MapServer_MakeExeOverridePath(override1, "BlessRule.TXT");
	MapServer_MakeExeOverridePath(override2, "Map\\BlessRule.txt");
	MapServer_MakeExeOverridePath(override3, "Map\\BlessRule.TXT");
	long node;

	strcpy(attempt0, baseMakeDataFileName("Data\\BlessRule.txt"));
	strcpy(attempt1, baseMakeDataFileName("Data\\BlessRule.TXT"));
	strcpy(attempt2, baseMakeDataFileName("data\\BlessRule.txt"));
	strcpy(attempt3, baseMakeDataFileName("data\\BlessRule.TXT"));
	strcpy(attempt4, baseMakeDataFileName("DATA\\BlessRule.txt"));
	strcpy(attempt5, baseMakeDataFileName("DATA\\BlessRule.TXT"));
	strcpy(attempt6, baseMakeDataFileName("BlessRule.txt"));
	strcpy(attempt7, baseMakeDataFileName("BlessRule.TXT"));

	if (!MapServer_BlessRuleScribeLoadFile(override0) &&
		!MapServer_BlessRuleScribeLoadFile(override1) &&
		!MapServer_BlessRuleScribeLoadFile(override2) &&
		!MapServer_BlessRuleScribeLoadFile(override3) &&
		!MapServer_BlessRuleScribeLoadFile(attempt0) &&
		!MapServer_BlessRuleScribeLoadFile(attempt1) &&
		!MapServer_BlessRuleScribeLoadFile(attempt2) &&
		!MapServer_BlessRuleScribeLoadFile(attempt3) &&
		!MapServer_BlessRuleScribeLoadFile(attempt4) &&
		!MapServer_BlessRuleScribeLoadFile(attempt5) &&
		!MapServer_BlessRuleScribeLoadFile(attempt6) &&
		!MapServer_BlessRuleScribeLoadFile(attempt7))
	{
		g_BlessRuleEnhanceMaxLoaded = 1;
		return;
	}

	node = scribeGetNodeSectionPosition("bless_rule", 1);
	if (node)
	{
		g_BlessRuleEnhanceMaxNormal = gameReadScribeFileNumber("enhance_max_normal", 1, node, g_BlessRuleEnhanceMaxNormal);
	}
	scribeFreeFile();

	if (g_BlessRuleEnhanceMaxNormal < 0) g_BlessRuleEnhanceMaxNormal = 0;
	if (g_BlessRuleEnhanceMaxNormal > 20000) g_BlessRuleEnhanceMaxNormal = 20000;

	g_BlessRuleEnhanceMaxLoaded = 1;
}

extern "C" long gameGetBlessEnhanceMaxNormalCfg()
{
	MapServer_LoadBlessEnhanceMaxRule();
	return g_BlessRuleEnhanceMaxNormal;
}

// ?????????????CardUpgrade.txt??
extern "C" void LoadServerCardUpgradeTXT(void);

// ??AI??
#include "BotManager.h"

// ????
#include "MapServer_TianshuProtocol.h"

#define DEFAULT_CLIENT_CONNECT_TIMEOUT			15
#ifdef NO_SPEED_LIMIT
 #define SPEED_REPORT_MIN						10000000
#else
 #define SPEED_REPORT_MIN						20 // 80
#endif

/* Mail/gamble not used on this shard: skip idle MailLoop + R61_Gamble_Run each frame. */
#define NO_R61_GAMBLE_LOOP

#define CHECK_LOGIN_FLAG_FIRST_LOGIN			0x00000001
#define CHECK_LOGIN_FLAG_COUNTRY_WAR_TELEPORT	0x00000002

#if NET_SERVER_DIAG
static void MapServer_NetDiagLog(const char *msg)
{
	if (CMapServer::GetServer() && msg)
		CMapServer::GetServer()->Log("%s", msg);
}
#endif

CMapServer *	CMapServer::m_pServer	= NULL;
struct plrDATA_WORLD_TOWN_DATA g_nSafeTownData;
struct plrDATA_WORLD_TOWN_DETAIL g_nSafeTownDetailData;
struct plrDATA_WORLD_FORCE g_nForceData[3];

#define NO_LOG_TO_UI_FRAMERATIO				10
long g_nFrameRatio = 0;		// び篊玥ぃ陪ボ癟 UI
long g_Server_Setting = 0;

#ifdef CONSOL_MODE
long m_csDBMessage = 0;
long g_nShowLog = 0;
#else
extern long m_csDBMessage;
extern long g_nShowLog;
#endif

// ??????????
extern void SendFashionDataOnLogin(CMapPlayer * pMapPlayer);
extern void AddFashionAttrToPlayer(struct plrDATA * pData, struct plrCALCDATA * pCData);
extern void NormalizeFashionActivatedCountOnLoad(struct plrDATA_FASHION_SAVE * d);

// ??????????
extern void AddTransformAttrToPlayer(struct plrDATA * pData, struct plrCALCDATA * pCData);

// ??????????????
extern void AddCardCollectionAttrToPlayer(struct plrDATA * pData, struct plrCALCDATA * pCData);

// ??????????
extern void AddTianshuAttrToPlayer(struct plrDATA * pData, struct plrCALCDATA * pCData);

char g_ServerINI_Dir[256] = "";
char * g_szDebug_LastFormat = NULL;		// 魁程 Log 篈﹃夹(狦Τ杠)

char mapInvalidNameChar[] = "~`!@#$%^&*()=+{}\\|/?:;\"\'<>, ";

// ?}???????a??ID
//unsigned char g_town_id[] = {1,2,4,5,7,11,13,14,15,17,21,22,24,25,26,27,29,30,0};
unsigned char g_town_id[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,33,35,0};
// ???L??O??e?a??
#define CWAR_NONE_FORCE_TELEPORT_CITY			14

unsigned short g_CWarTeleport[CWAR_NONE_FORCE_TELEPORT_CITY] = {260,262,264,267,274,276,285,291,293,323,325,327,329,331};

unsigned long g_nObjectCounter = 0;

#define gameTOWN_LOW_GOOD_NUMBER			4	// 计ぶΤ纔磃

#ifdef USE_NEW_PLAYER_MSG_BROADCAST
	std::map<long, CProtocoInfo>& ProcSendPlayerMsg();

	bool compCProtocoInfoSize (const CProtocoInfo& first, const CProtocoInfo& second)
	{
		return first.size < second.size;
	}

	bool compCProtocoInfoCount (const CProtocoInfo& first, const CProtocoInfo& second)
	{
		return first.count < second.count;
	}
#endif

//	long NAI_CheckTickTime(unsigned long cur_tick, unsigned long due_tick);

CSpeedRecord::CSpeedRecord(void)
{
	m_nTickStart = 0;
	m_nTickStart2 = 0;
	m_nFrame = 0;
	m_nLastFrame = 0;
	m_nElapseTickStart = 0;
}

CSpeedRecord::~CSpeedRecord(void)
{
}

void CSpeedRecord::SetFrameRatioStart()
{
	m_nLastFrame = 0;
	m_nFrame = 0;
	m_nTickStart = apiGetTickCounter();
	m_nTickStart2 = m_nTickStart;
}

long CSpeedRecord::GetFrameRatio(unsigned long *ratio)
{
	long r = 0;
	unsigned long nTick, nElapseTick;

	m_nLastFrame++;
	nTick = apiGetTickCounter();
	if (nTick < m_nTickStart)
	{
		nElapseTick = nTick + ((unsigned long)0xffffffff - m_nTickStart);
	}
	else
		nElapseTick = nTick - m_nTickStart;
	if (nElapseTick >= 1000)			// – 1 穝Ω
	{
		m_nTickStart = nTick;
		m_nFrame = m_nLastFrame;
		m_nLastFrame = 0;
		r++;
	}
	//
	if (m_nCountDown)
	{
		if (nTick < m_nTickStart2)
		{
			nElapseTick = nTick + ((unsigned long)0xffffffff - m_nTickStart2);
		}
		else
			nElapseTick = nTick - m_nTickStart2;
		if (m_nCountDown >= nElapseTick)
		{
			m_nCountDown -= nElapseTick;
		}
		else
			m_nCountDown = 0;
	}
	m_nTickStart2 = nTick;
	//
	*ratio = m_nFrame;
	return(r);
}

void CSpeedRecord::SetElapseTickStart()
{
	m_nElapseTickStart = apiGetTickCounter();
}

unsigned long CSpeedRecord::GetElapseTick()
{
	unsigned long nTick, nElapseTick;

	nTick = apiGetTickCounter();
	if (nTick < m_nElapseTickStart)
	{
		nElapseTick = nTick + ((unsigned long)0xffffffff - m_nElapseTickStart);
	}
	else
		nElapseTick = nTick - m_nElapseTickStart;
	if (!nElapseTick)
		nElapseTick = 1;
	return(nElapseTick);
}

void CSpeedRecord::SetCountDownTick(unsigned long tick)	// ?p???
{
	m_nCountDown = tick;
}

unsigned long CSpeedRecord::GetCountDownTick()
{
	return(m_nCountDown);
}
// ==============================================================
// ?????X?k?T??????X?
void cbInvalidMessageNotify(char *buffer, int len, int id)
{
	CMapPlayer * pPlayer;

	pPlayer = CMapServer::GetServer()->FindPlayerByClientID(id);
	if (pPlayer)
	{
		CMapServer::GetServer()->Log("ERROR: Data notify! %d(uid=%d,name='%s')", id, pPlayer->GetUID(), pPlayer->GetName());
		CMapServer::GetServer()->KickPlayer(pPlayer);
	}
	else
	{
		if (!CMapServer::GetServer()->GetClientValid( id ))
		{
			CMapServer::GetServer()->Log( "WARNING: Invalid login data, kick out(%d) !", id );
			if (id)
				CMapServer::GetServer()->KickClient( id );
		}
	}
}

// ?O?_ PK ???P?????a?P?h?L
// target_uid ?@?w?O???a, player_uid ???@?w
long IsFreePKStagePartyTarget(unsigned long player_uid, unsigned long target_uid)
{
	CMapChar * pChar;
	CMapNPC * pNPC;
	CMapPlayer * pMapPlayer;

	if (!player_uid || !target_uid)
		return(0);
	if (!IsPlayerUID(target_uid))
		return(0);
	pChar = (CMapChar *)CMapServer::GetServer()->FindObjectByUID(player_uid, CMapChar::CLASS_ID);
	if (pChar)
	{
		if (IsPlayerUID(player_uid))
		{
			pMapPlayer = (CMapPlayer *)pChar;
			if (pMapPlayer->PartyFind(target_uid))
				return(1);
		}
		else
		{
			pNPC = (CMapNPC *)pChar;
			if (pNPC->m_nPlayerUID)
			{
				pMapPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(pNPC->m_nPlayerUID);
				if (pMapPlayer)
				{
					if (pMapPlayer->PartyFind(target_uid))
						return(1);
				}
			}
		}
	}
	return(0);
}

CMapServer::CMapServer(void)
{
	m_pServer				= NULL;
	m_nItemUIDMin			= 0;
	m_nItemUIDMax			= 0xffffffffL;
	m_nItemUID				= 0;

	m_pMapObject			= NULL;
	m_nFreeHandle			= 0;

	m_nLastTickCount		= 0;
	//m_nLoopTickCount		= 0;
	m_nLoopTickCount		= ::GetTickCount();
	m_nLoopTime				= (long)::time(NULL);
	m_nLastCombatPowerRankTitlePoll = 0;

	m_nLoadWorldDataTime	= 0;
	m_nLocalBaseTime		= 0;
	m_nWorldBaseTime		= 0;
	m_IsWorldDataReady		= false;
	m_IsWorldTimeReady		= false;

	memset(&m_KingStatue, 0, sizeof(m_KingStatue));
	memset(&m_KingStatue2, 0, sizeof(m_KingStatue2));
	memset(&m_KingStatueNPC, 0, sizeof(m_KingStatueNPC));
	memset(&m_KingStatueFlag, 0, sizeof(m_KingStatueFlag));
	memset(&m_KingStatueTime, 0, sizeof(m_KingStatueTime));

	m_IsCreating			= false;
	m_IsWar					= 0;
	m_IsSoulBattle			= 0;
	m_CanEnterMode			= 0;

	m_nConnectCount = 0;
	m_mCharToUID.clear();

	m_mConnectIPCount.clear();
	m_nPlayerFace.clear();
	m_nFaceCacheNum = 0;

	nOptSerial_NPC = 0;

	m_nKeepImportantData.clear();

	m_IsNPCCountryUpdateFlag = 0;
	m_IsNPCCountryUpdate = 0;

	memset(&m_TownData, 0, sizeof(m_TownData));
	memset(&m_ForceData, 0, sizeof(m_ForceData));

	pwfTownTotal_WEI = 0;
	pwfTownTotal_SHU = 0;
	pwfTownTotal_WU = 0;

	m_nIsLoginAskID = 0;

	IsBotEnemyCheck = false;
	memset(&m_nRepayData, 0, sizeof(m_nRepayData));
	memset(&m_nActivityData, 0, sizeof(m_nActivityData));
	memset(&m_nSoulSetData, 0, sizeof(m_nSoulSetData));
	memset(&m_nSoulRecordData, 0, sizeof(m_nSoulRecordData));
	memset(&m_nHistorySetData, 0, sizeof(m_nHistorySetData));

	memset(&nCWarList, 0, sizeof(nCWarList));
	memset(&nCWarKList, 0, sizeof(nCWarKList));

	m_nSoulItemDuedate = 0;
	m_nSoulTicketDuedate = 0;

	m_nKeepChangeForceTime = 0;
	m_nResetSpecCNModeTime = 0;

	::Global_AddSoldierAttrSS = getPlayerSolSoulToAddSoldierAttr;
	::Global_AddSoldierAttrFP = getPlayerSkillToAddSoldierAttr;
	::Global_AddSoldierAttrSFFP = getPlayerSkillToAddSoldierAttrSF;
	::Global_AddFashionAttr = AddFashionAttrToPlayer;
	::Global_AddTransformAttr = AddTransformAttrToPlayer;
	::Global_AddCardCollectionAttr = AddCardCollectionAttrToPlayer;
	::Global_AddTianshuAttr = AddTianshuAttrToPlayer;

#ifdef SMART_PLR_DATA2
	memset(&plrEnemySkillTable, 0, sizeof(plrEnemySkillTable));
	memset(&plrEnemyNPC, 0, sizeof(plrEnemyNPC));
#endif

	m_nSelfBoomTime = 0;

	memset(&m_rtRankList, 0, sizeof(struct plrDATA_WORLD_COUNTRY_WAR_LIST_REAL_TIME_SEND)*10);
	memset(&nCWarKListShow, 0, sizeof(nCWarKListShow));
}

CMapServer::~CMapServer(void)
{
	m_pServer	= NULL;
}

// Broadcast message to all clients on this MapServer
void CMapServer::SendToAllClients(long protoco, char* buffer, long size, long send_flag)
{
	CMapPlayer* pPlayer;
	std::map<int, CMapPlayer*>::iterator i;
	
	for (i = m_ClientMap.begin(); i != m_ClientMap.end(); i++)
	{
		pPlayer = i->second;
		if (pPlayer && !IsObjectDeleted(pPlayer->GetHandle()))
		{
			::msgSendData(i->first, 0, protoco, buffer, size, send_flag);
		}
	}
}

bool CMapServer::IsCreating(void)
{
	return(m_IsCreating);
}

void CMapServer::LoadSetting(char *szProfile)
{
	long lNode;
	char szTmpStr[1024];

	if ( !::scribeLoadFile( szProfile ) )
		return;
	//
	if (lNode = ::scribeGetNodeSectionPosition( "MapServer", 1 ))
	{
		if (scribeReadString("server_ini_dir", 1, lNode, szTmpStr))
		{
			strcpy(g_ServerINI_Dir, szTmpStr);
		}
	}
	::scribeFreeFile();
	return;
}

long CMapServer::gameLoadTextResourceFiles()
{
	long r = 0;
	long k, j, node;
	char tmpstr[10240];

	if (scribeLoadFile(baseMakeDataFileName("Data\\BootUp.txt")))
	{

#ifdef USE_LOGIN_EXTRA_RESOURCE
		if (m_ServerInfo.iniServerLang)
		{
			Log("正在加载资源 [BOOT_EN]...");
			node = scribeGetNodeSectionPosition("BOOT_EN", 1);
		}
		else
		{
			Log("正在加载资源 [BOOT]...");
			node = scribeGetNodeSectionPosition("BOOT", 1);
		}
#else
		node = scribeGetNodeSectionPosition("BOOT", 1);
#endif
		if (node)
		{
			j = scribeGetNodeItemNumber("resource", node);
			if (j)
			{
				k = 1;
				while (j--)
				{
					if (scribeReadString("resource", k, node, tmpstr))
					{
						scribePush();
						if (k == 1)
						{
							gameLoadResourceTXT(baseMakeDataFileName(tmpstr));
						}
						else
						{
							gameLoadResourceTXT_Append(baseMakeDataFileName(tmpstr));
						}
						scribePop();
					}
					k++;
				}
			}
			else
				Log("ERROR: Load text-resource not found setting !");
		}
		scribeFreeFile();
		r++;
		//
#ifdef USE_INNER_CODE_RESOURCE
		gameMakeGameResourceNameTrans_Japan();
#endif
	}
	else
		Log("Load BootUp.txt error !");
	return(r);
}

#ifdef __cplusplus
extern "C" {
#endif
unsigned char MapServer_GetCNPC_BA_Code(unsigned long cnpc_uid, unsigned long player_uid)
{
	CMapNPC *pNPC;
	CMapPlayer *pPlayer;

	if (!cnpc_uid || !player_uid)
		return 0;
	pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(cnpc_uid, CMapNPC::CLASS_ID);
	if (!pNPC)
		return 0;
	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(player_uid, CMapPlayer::CLASS_ID);
	if (!pPlayer)
		return 0;
	return pNPC->GetCNPC_BA_Code(pPlayer);
}
#ifdef __cplusplus
}
#endif

void gameServer_MakeNPC(struct plrDATA_NPC * pSave, struct plrDATA_NPCSHOW * pData, CMapPlayer * pPlayer)
{
	long i;
	CMapNPC * pNPC;
	struct plrDATA pFullData;
	struct plrDATA * plrData;

//	if (pPlayerSave)
//	{
//		matrix = pPlayerSave->plrMatrix;
//	}
//	else
//		matrix = -1;
	for (i=0; i<gameMAX_CARRY_SOLDIER; i++)
	{
	//	memset(&pFullData, 0, sizeof(pFullData));
		//pSave->plrNPCData[i].plrNPC_UID
		if (pSave->plrNPCData[i].plrNPC_ItemCode || pSave->plrNPCData[i].plrNPC_SummonCode)
		{
			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByUID(pSave->plrNPCData[i].plrNPC_UID, CMapNPC::CLASS_ID);
			if (pNPC)
			{
				gameServer_NPC_MakeFullData(&pSave->plrNPCData[i], &pFullData, pNPC->GetCNPC_BA_Code(pPlayer));
				//gameEff_MakeDisappearStatus(&pFullData);
				gameServer_NPC_MakeShowSelf(&pFullData, &pData->plrNPCData[i]);
				//pData->plrNPCData[i].plrNPC_StatusDisappear = MapServer_GetCharStatusDisappear(pSave->plrNPCData[i].plrNPC_UID);
				//
				if (plrData = pNPC->GetCharData())
				{
					pData->plrNPCData[i].plrNPC_StatusDisappear = plrData->plrStatusDisappear;
					pData->plrNPCData[i].plrNPC_Status2Disappear = plrData->plrStatus2Disappear;
					pData->plrNPCData[i].plrNPC_HP = ::gameClampPlrHPMPVal(plrData->plrHP);
					pData->plrNPCData[i].plrNPC_HPMax = ::gameClampPlrHPMPVal(plrData->plrHPMax);
				}
			}
			else
			{
				memset(&pFullData, 0, sizeof(pFullData));
				gameServer_NPC_MakeFullData(&pSave->plrNPCData[i], &pFullData, 0);
				gameServer_NPC_MakeShowSelf(&pFullData, &pData->plrNPCData[i]);
				if (!gameCNPC_HasItemSkill(pData->plrNPCData[i].plrNPC_ItemSkill) &&
					gameCNPC_HasItemSkill(pSave->plrNPCData[i].plrNPC_ItemSkill))
				{
					memcpy(pData->plrNPCData[i].plrNPC_ItemSkill, pSave->plrNPCData[i].plrNPC_ItemSkill,
						sizeof(pData->plrNPCData[i].plrNPC_ItemSkill));
				}
			}
		}
		else
			memset(&pData->plrNPCData[i], 0, sizeof(pData->plrNPCData[i]));
	}
}
// =====================================================================
// ?Z??
// =====================================================================
/*
long CMapServer::MakeTime(long year, long mon, long day, long hour, long min, long sec)
{
	struct tm ntm;

	memset(&ntm, 0, sizeof(ntm));
	ntm.tm_year = year - 1900;
	ntm.tm_mon = mon - 1;
	ntm.tm_mday = day;
	ntm.tm_hour = hour;
	ntm.tm_min = min;
	ntm.tm_sec = sec;
	//
	if (ntm.tm_year < 0)
		return(-1);
	if ((ntm.tm_mon < 0) || (ntm.tm_mon > 11))
		return(-1);
	if ((day < 1) || (day > 31))
		return(-1);
	if ((hour < 0) || (hour > 23))
		return(-1);
	if ((min < 0) || (min > 59))
		return(-1);
	if ((sec < 0) || (sec > 59))
		return(-1);
	//
	return(mktime(&ntm));
}
*/
// ??????]?w????A?]?w?Z??map code???????A?? Login ??s
// ?]?w?Z??Z?N?t?m
void CMapServer::InnerSoulRecordInit()
{
	long i, total;
	struct gameSOUL_SET_DATA * pSoulSet;

	memset(&m_nSoulRecordData, 0, sizeof(m_nSoulRecordData));
	total = 0;
	for (i=1; i<=gameMAX_SOUL_SETTING; i++)
	{
		pSoulSet = gameGetSoulSetPtr(i);
		if (pSoulSet)
		{
			if (pSoulSet->sdID)
			{
				m_nSoulRecordData.nData[total].nMapCode = (unsigned short)i;
				//m_nSoulRecordData.nData[total].nCount
				//
				total += 1;
			}
		}
	}
	m_nSoulRecordData.nTotal = (unsigned short)total;
	// ?]?w?Z??Z?N?t?m
	// ?u????@???y
	if (GetServer()->m_nMapServerSubID == WORLD_MAPSERVER_SUBID)
		GetMapManager()->CreateWorldSoulSetManager();
}

// ???s?p??Z?????????
void CMapServer::InnerSoulCalcData()
{
/*	struct MAP_SET_SOUL_DATA * pSoulSet;
	struct tm * ptm;
	struct tm ntm;
	long day, ntime;

	pSoulSet = GetSoulSetData();
	//
	ptm = ::apiGetTimeStruct(0);
	memcpyT(&ntm, ptm, sizeof(ntm));
	// .......................................
	ntm.tm_hour = 0;		// ???G??????e?@??
	ntm.tm_min = 0;
	ntm.tm_sec = 0;
	ntime = mktime(&ntm);
	// ???G?p??o????????
	day = pSoulSet->nTicketWeekDay;					// 把辽布アら琍戳碭(1 - 7)
	if (day >= 7)
		day = 0;
	if (day < 0)
		day = 0;
	day = day + 7 - ntm.tm_wday;
	ntime += (60*60*24*day);
	// ?U???????
	//ntime += (60*60*24*7);							// ?U?P??
	Soul_SetSoulTicketDuedate(ntime);
	// .......................................
	ntm.tm_hour = 12;		// 猌活珇Τらいと
	ntm.tm_min = 0;
	ntm.tm_sec = 0;
	ntime = mktime(&ntm);
	// ?Z???~?G?p??o????????
	day = pSoulSet->nBattleWeekDay;					// 猌活アら琍戳碭(1 - 7)
	if (day >= 7)
		day = 0;
	if (day < 0)
		day = 0;
	day = day + 7 - ntm.tm_wday;
	ntime += (60*60*24*day);
	// ?U???????
	//ntime += (60*60*24*7);							// ?U?P??
	Soul_SetSoulItemDuedate(ntime);
*/
}

struct MAP_SOUL_RECORD_INNER_DATA * CMapServer::GetSoulRecordDetail(long map_code)
{
	long i, total;

	total = m_nSoulRecordData.nTotal;
	for (i=0; i<total; i++)
	{
		if (m_nSoulRecordData.nData[i].nMapCode == map_code)
			return(&m_nSoulRecordData.nData[i]);
	}
	return(NULL);
}

// ?Z??: ?]?w?Z???~????????]?O makeitem ????]?w??)
void CMapServer::Soul_SetSoulItemDuedate(long duedate)
{
	m_nSoulItemDuedate = duedate;
}

long CMapServer::Soul_GetSoulItemDuedate()
{
	return(m_nSoulItemDuedate);
}

void CMapServer::Soul_SetSoulTicketDuedate(long duedate)
{
	m_nSoulTicketDuedate = duedate;
}

long CMapServer::Soul_GetSoulTickDuedate()
{
	return(m_nSoulTicketDuedate);
}

// ?]?w?Z??????
long CMapServer::SetSoulBattleStatus(long status, long broadcast)
{
	// ?u????@???y
	if (GetServer()->m_nMapServerSubID != WORLD_MAPSERVER_SUBID)
		return(0);
//	CMapPlayer * pPlayer;
//	CMapCharMsg * pMsg;
//	//
//	std::map<int, CMapPlayer *> *mMapPlayer;
//	std::map<int, CMapPlayer *>::iterator	iter;
//	std::map<int, CMapPlayer *>::iterator	iter_end;

	if (m_IsSoulBattle != status)
	{
		m_IsSoulBattle = (unsigned char)status;
		// ?Z??@?~?B?z
		if(CMapServer::GetServer()->IsCrossServer())
		{
			GetMapManager()->SetWorldKSSoulSetManagerState(m_IsSoulBattle);
		}
		else
		{
			GetMapManager()->SetWorldSoulSetManagerState(m_IsSoulBattle);
		}
/*		// ?q????????a??s???
		if (broadcast)
		{
			mMapPlayer = &CMapServer::GetServer()->GetClientPlayerMap();
			iter_end = mMapPlayer->end();
			for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
			{
				pPlayer	= iter->second;
				if (pPlayer)
				{
					if (!IsObjectDeleted(pPlayer->GetHandle()))
					{
						pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
						pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
					//	pMsg->m_nSize	= sizeof(struct MAP_UPDATE_PLAYER_DATA_PART2);
						pMsg->m_nSize	= pMsg->m_Msg.m_UpdateData2Msg.GetSize();
						pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_WAR_MODE;
						pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = IsWar();
						pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData2 = IsWarType();
						//
						Disconnect_ClearAllRecord( pPlayer->GetUID(), 3 );
					}
				}
			}
		}	*/
		return(1);
	}
	return(0);
}

// ????R??
long CMapServer::Soul_CanBuyTicket()
{
	struct MAP_SET_SOUL_DATA * pSoulSet;
	struct tm * ptm;
	//struct tm ntm;
	long wday;

	pSoulSet = GetSoulSetData();
	//
	ptm = ::apiGetTimeStruct(0);
	//memcpyT(&ntm, ptm, sizeof(ntm));

//Log("Ticket: %d,%d,%d,%d,%d,%d,%d", pSoulSet->nTicketSell[0], pSoulSet->nTicketSell[1], pSoulSet->nTicketSell[2], pSoulSet->nTicketSell[3], pSoulSet->nTicketSell[4], pSoulSet->nTicketSell[5], pSoulSet->nTicketSell[6]);

	//
	wday = ptm->tm_wday;
	if (!wday)
		wday = 7;
	if (pSoulSet->nTicketSell[wday])
		return(1);
	return(0);
}

// ??_?J??
// return: 0 ????A0x80000000 = ?????
long CMapServer::Soul_CheckCanEnter(CMapPlayer * pPlayer, long goto_map_code)
{
	if (m_CanEnterMode)
	{
		// ?u????@???y
		if (GetServer()->m_nMapServerSubID == WORLD_MAPSERVER_SUBID)
		{
			// ?u????????J??
			if (m_CanEnterMode == 1)
			{
				if (Soul_IsChallenger(pPlayer, goto_map_code))
					return(1 | 0x80000000);
			}
			else if (m_CanEnterMode == 2)
			{
				if (!Soul_IsChallenger(pPlayer, goto_map_code))	// 獶把辽ぃ盿皑
				{
					// ?????O?_????
					if (pPlayer->GetSaveData()->plrEquipHorse.m_Item.itemCode)
					{
	//err_horse:
						pPlayer->SendMsgToClientByID(24187);
						return(0);
					}
				//	// ???~??O?_????
				//	if (InnerCheckCarryItem_Horse(pPlayer->GetItemData()))
				//		goto err_horse;
					// ?O?_???p?L
					if (pPlayer->CheckCarryNPC(2))	// ?h?L??
					{
	//err_cnpc:
						pPlayer->SendMsgToClientByID(24188);
						return(0);
					}
				//	if (pPlayer->CheckCarryNPC(1))	// ???~??
				//		goto err_cnpc;
				}
				else
					return(1 | 0x80000000);
				return(1);
			}
		}
	}

	pPlayer->SendMsgToClientByID(20687); //瞷ぃ初

	return(0);
}

// ??d???O??x??O?Z?x
// return: -1 error, 0, 1 ?Z, 2, 3 ??
long CMapServer::Soul_CheckTicketGenType(struct itemDATA_SAVE * pItem)
{
	struct gameSOUL_SET_DATA * pSoulSet;
	long i, role_id;

	if (pItem->m_Item.itemCode == gameITEM_ID_SOUL_TICKET)
	{
		if (pItem->m_Item.itemFlags & itemSHOW_FLAG_STICKET)
		{
			pSoulSet = gameGetSoulSetPtr(pItem->m_STicket.itemMapCode);
			if (pSoulSet)
			{
				role_id = pItem->m_STicket.itemSoulRoleID;
				for (i=0; i<gameMAX_SOUL_SETTING_ROLE; i++)
				{
					if (pSoulSet->nSetRole[i].srdRoleID == role_id)
					{
						return(i);
					}
				}
			}
		}
	}
	return(-1);
}

// ?O?_?O?D???
// return: 0 ???O
long CMapServer::Soul_IsChallenger(CMapPlayer * pPlayer, long goto_map_code, long * soul_job_type, struct gameSOUL_SET_DATA ** pSoulSetPtr, long hope_gen_pos)
{
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, curr_time;	// , p_mapcode;
	struct gameSOUL_SET_DATA * pSoulSet;
	long gen_type, job_type;
	long val;
	long r = 0;

	if(CMapServer::GetServer()->IsCrossServer())
		return ++r;

	if (pPlayer->GetSaveData()->plrLevel < gameBUY_SOUL_TICKET_LEVEL)
		return(r);
	// ?O?_?????T???????
	curr_time = GetLoopTime();
	//p_mapcode = pPlayer->GetMapCode();
	pItemData = pPlayer->GetItemData();
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
	for (i=0; i<val; i++)
	{
		pItem = &pItemData->plrCarryItem[i];
		if (pItem->m_Item.itemCode == gameITEM_ID_SOUL_TICKET)
		{
			if (::gameGetItemPtr(gameITEM_ID_SOUL_TICKET))
			{
				if (pItem->m_Item.itemNumber > 0)
				{
					if (pItem->m_Item.itemFlags & itemSHOW_FLAG_STICKET)
					{						// ?O?_???]?w
						pSoulSet = gameGetSoulSetPtr(pItem->m_STicket.itemMapCode);
						if (pSoulSet)
						{					// ?O?_?L??
							if ((long)pItem->m_STicket.itemDuedate > curr_time)
							{				// ?O?_??~???T
								gen_type = Soul_CheckTicketGenType(pItem);
								job_type = pPlayer->GetSaveData()->plrJobID;
								//
								if (gen_type <= 1)			// ???r
								{
									//if ((job_type != jobID_WARLORD) && (job_type != jobID_LEADER))
									if (gameCheckJob_Warrior_Advisor(job_type) != JOB_TYPE_WARRIOR)
										continue;
								}
								else if (gen_type <= 3)		// ?x??
								{
									//if ((job_type != jobID_ADVISOR) && (job_type != jobID_WIZARD))
									if (gameCheckJob_Warrior_Advisor(job_type) != JOB_TYPE_ADVISOR)
										continue;
								}
								// ?O?_?a????T
								if (pItem->m_STicket.itemMapCode == goto_map_code)
								{
		chk_ok:						if (soul_job_type)
										*soul_job_type = gen_type;
									if (pSoulSetPtr)
										*pSoulSetPtr = pSoulSet;
									//
									r |= 1;
									//
									if (hope_gen_pos != -1)
									{
										if (gen_type != hope_gen_pos)
											continue;
									}
									return(r);
									//pPlayer->SendMsgToClientByID(24160);
								}
								else
								{
									if (gen_type <= 1)
									{
										if (pSoulSet->sdBattleMapCode1 == goto_map_code)
											goto chk_ok;
									}
									else if (gen_type <= 3)
									{
										if (pSoulSet->sdBattleMapCode2 == goto_map_code)
											goto chk_ok;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return(r);
}

// ?D???b???????e?????a?I
void CMapServer::Soul_ChallengerTeleport(CMapPlayer * pPlayer)
{
	long soul_job_type;
	long side_flag;
	struct gameSOUL_SET_DATA * pSoulSet;
	long map_code, map_x, map_y;

	if (m_CanEnterMode == 0)
		return;
	map_code = pPlayer->GetMapCode();
	if (Soul_IsChallenger(pPlayer, map_code, &soul_job_type, &pSoulSet))
	{
		// ??d?i?????(15????~???e?i???}???)
#ifdef USE_SOUL_ENTER_TIME_LIMIT
		map_code |= (soul_job_type << 16);
		if (m_EnterDueTime[map_code] >= GetLoopTime())
		{
 #ifndef USE_PACKAGE_DATA
			Log("DEBUG: Cannot enter now(%s)", pPlayer->GetName());
 #endif
			// ??e?X?h
			pPlayer->ChangeMapInStageMapPos();
			return;
		}
#endif
		// ???Z?????e
		map_code = 0;
		if (soul_job_type <= 1)				// 0,1
		{
			map_code = pSoulSet->sdBattleMapCode1;
		}
		else if (soul_job_type <= 3)		// 2,3
		{
			map_code = pSoulSet->sdBattleMapCode2;
		}
		//
		side_flag = 0;
		if (!(soul_job_type & 1))			// 虫┪蛮
		{
			map_x = pSoulSet->sdBattlePosX1;
			map_y = pSoulSet->sdBattlePosY1;
		}
		else
		{
			side_flag |= 8;
			map_x = pSoulSet->sdBattlePosX2;
			map_y = pSoulSet->sdBattlePosY2;
		}
		//
		if (map_code)
		{
//Log("Soul: Teleport to %s,%d,%d,%d", pPlayer->GetName(), map_code, map_x, map_y);
			//ChangeSaveMap(pPlayer, map_code, map_x, map_y);
			//
			//pPlayer->m_nIsChallenger &= ~8;					// 2009/05/25 ???M??????
			pPlayer->m_nIsChallenger |= (2 | side_flag);
		//	pPlayer->m_nIsChallenger |= 2;
			pPlayer->ChangeMap(map_code, map_x, map_y, 2);
		}
	}
}

// ???`????e?^?m???????a: ?e???A?w?g?B?z?L??o???~?A?^?m???w?g???L
void CMapServer::Soul_BossDieTeleport(CMapNPC * pNPC)
{
	long total;
	struct NPC_HURT_RECORD nHurt[100];		// 100 莱赣ì镑
	struct NPC_HURT_RECORD * pHurtRec;
	//
	long no_ticket = 0;
	long cnt1;
	long map_code;
	long soul_job_type;
	struct gameSOUL_SET_DATA * pSoulSet;
	unsigned long player_uid;
	//
	CMapPlayer * pPlayer;
	CObjectManager * pObjMgr;
	CMapCtrl * pCtrl;
	std::map<long, CMapObject *>::iterator	iObject;

	//pHurtRec = (struct NPC_HURT_RECORD *)&pNPC->m_HurtRecord;
	//total = CMapNPC::MAX_NPC_HURT_RECORD;
	pHurtRec = (struct NPC_HURT_RECORD *)&nHurt;
	total = pNPC->SortPlayerHurtRecord(pHurtRec, 100);
#ifdef USE_SOUL_HURT_LOW_LIMIT		// 2009/04/22
	// ???s??????`??W
	pNPC->m_mapPlayerHurtRecord.clear();
#else
	if (total > 20)
		total = 20;				// ?e 20??
#endif
	//
	map_code = pNPC->GetMapCode();
	pCtrl = CMapServer::GetServer()->FindMap(map_code);
	if (pCtrl)
	{
		pObjMgr = pCtrl->GetAllPlayerList();
		// ........ ???a??????a .........
		for (iObject = pObjMgr->GetBegin(); iObject != pObjMgr->GetEnd(); iObject++)
		{
			pPlayer	= (CMapPlayer *)iObject->second;
			if (pPlayer)
			{
				if (pPlayer->m_nIsChallenger & 1)	// 初いΤ厨
				{
					player_uid = pPlayer->GetUID();
					// ?P?_?O???o??Z?????????
					// ?H????@???u??		(?n?????A??i?J???)
					if (Soul_IsChallenger(pPlayer, pPlayer->GetMapCode(), &soul_job_type, &pSoulSet, pNPC->m_nSoulData_GenPos))
					{
						if (soul_job_type == pNPC->m_nSoulData_GenPos)
						{
							// ???`????e?^?m???????a
							for(cnt1 = 0; cnt1 < total; cnt1++)
							{
								if (player_uid == pHurtRec[cnt1].m_nPlayerUID)
								{
#ifdef USE_SOUL_HURT_LOW_LIMIT		// 2009/04/22
 #ifndef USE_PACKAGE_DATA
									Log("DEBUG: 猌活產端甡κだゑ(%d,%d)", player_uid, pHurtRec[cnt1].m_nHurtPerc);
 #endif
									if (pHurtRec[cnt1].m_nHurtPerc >= 5)
									{
										pNPC->m_mapPlayerHurtRecord[player_uid] = nHurt[cnt1];
										goto skip_teleport;
									}
#else
									goto skip_teleport;
#endif
								}
							}
							if (!pPlayer->ChangeMapInStageMapPos())
								break;
						}
						else			// ??????(?o??|??t?@???a??e?X?h)
						{
					//		pPlayer->ChangeMapInStageMapPos();
							if (pNPC->m_nSoulData_GenPos & 1)			// 虫┪蛮
							{
								if (pPlayer->m_nIsChallenger & 8)		// ?P?@??
								{
									if (!pPlayer->ChangeMapInStageMapPos())
									break;
								}
							}
							else
							{
								if (!(pPlayer->m_nIsChallenger & 8))	// ?P?@??
								{
									if (!pPlayer->ChangeMapInStageMapPos())
									break;
								}
							}
						}
					}
					else
					{
					//	if (no_ticket)	// ?S??
							pPlayer->ChangeMapInStageMapPos();
					}
				}
skip_teleport:	;
			}
		}
	}
	// ?]?w?i?????(15????~???e?i???}???)
#ifdef USE_SOUL_ENTER_TIME_LIMIT
	map_code |= (pNPC->m_nSoulData_GenPos << 16);
	total = GetLoopTime() + SOUL_DIE_ENTER_BATTLE_TIME;
	m_EnterDueTime[map_code] = total;
#endif
}

// mode 0 = ??, 1 = ???~?Z??Z?N
void CMapServer::Gen_BossDieTeleport(CMapNPC * pNPC, long mode)
{
	long total;
	struct NPC_HURT_RECORD nHurt[300];		// 300 莱赣ì镑
	struct NPC_HURT_RECORD * pHurtRec;
	//
	long cnt1;
	long map_code;
	unsigned long player_uid;
	//
	CMapPlayer * pPlayer;
	CObjectManager * pObjMgr;
	CMapCtrl * pCtrl;
	std::map<long, CMapObject *>::iterator	iObject;

	if (!mode)
	{
		switch(pNPC->GetSaveData()->plrCode)
		{
		case GEN_ENEMY_CODE01:		// Τ疭﹚à︹
		case GEN_ENEMY_CODE02:
			break;
		default:
			return;
			break;
		}
	}
	//pHurtRec = (struct NPC_HURT_RECORD *)&pNPC->m_HurtRecord;
	//total = CMapNPC::MAX_NPC_HURT_RECORD;
	pHurtRec = (struct NPC_HURT_RECORD *)&nHurt;
	total = pNPC->SortPlayerHurtRecord(pHurtRec, 300);
	if (!mode)
	{
		if (total > 50)
			total = 50;				// ?e 50??
		// ???s??????`??W
		pNPC->m_mapPlayerHurtRecord.clear();
		for (cnt1=0; cnt1<total; cnt1++)
		{
			player_uid = nHurt[cnt1].m_nPlayerUID;
			if (player_uid)
			{
				pNPC->m_mapPlayerHurtRecord[player_uid] = nHurt[cnt1];
			}
		}
	}
	else
	{
		if (total > gameGEN_BOX_PLAYER_REC)
			total = gameGEN_BOX_PLAYER_REC;
		// ???s??????`??W
		pNPC->m_mapPlayerHurtRecord.clear();
		for (cnt1=0; cnt1<total; cnt1++)
		{
			player_uid = nHurt[cnt1].m_nPlayerUID;
			if (player_uid)
			{
#ifdef GEN_BOSS_HURT_LOW_LIMIT
 #ifndef USE_PACKAGE_DATA
				Log("DEBUG: 產端甡κだゑ(%d,%d)", player_uid, nHurt[cnt1].m_nHurtPerc);
 #endif
				//if ((unsigned long)nHurt[cnt1].m_nHurtTotal >= (unsigned long)nMaxHP)
				if (nHurt[cnt1].m_nHurtPerc >= 5)
#endif
					pNPC->m_mapPlayerHurtRecord[player_uid] = nHurt[cnt1];
			}
		}
	}
	//
	map_code = pNPC->GetMapCode();
	pCtrl = CMapServer::GetServer()->FindMap(map_code);
	if (pCtrl)
	{
		pObjMgr = pCtrl->GetAllPlayerList();
		// ........ ???a??????a .........
		for (iObject = pObjMgr->GetBegin(); iObject != pObjMgr->GetEnd(); iObject++)
		{
			pPlayer	= (CMapPlayer *)iObject->second;
			if (pPlayer)
			{
				player_uid = pPlayer->GetUID();
				// ???`????e?^?m???????a
			/*	for(cnt1 = 0; cnt1 < total; cnt1++)
				{
					if (player_uid == pHurtRec[cnt1].m_nPlayerUID)
						goto skip_teleport;
				}
			*/
				if (pNPC->m_mapPlayerHurtRecord.find(player_uid) != pNPC->m_mapPlayerHurtRecord.end())
					goto skip_teleport;
				//
				if (!pPlayer->ChangeMapInStageMapPos())
					break;
skip_teleport:	;
			}
		}
	}
}
// =====================================================================
// ?j?M???~
// =====================================================================
struct itemDATA_SAVE * CMapServer::PlayerFindCarryItem_ByUID(CMapPlayer * pPlayer, struct itemUIDDATA * piUID, long * index)
{
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, val;

	if (index)
		*index = -1;
	pItemData = pPlayer->GetItemData();
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
	for (i=0; i<val; i++)
	{
		pItem = &pItemData->plrCarryItem[i];
		if (!memcmp(&pItem->m_Item.itemUID, piUID, sizeof(struct itemUIDDATA)))
		{
			if (index)
				*index = i;
			return(pItem);
		}
	}
	return(NULL);
}
// =====================================================================
// ??d
// =====================================================================
// gameITEM_ID_CARD
// ?M???d???~
// assign_item_pos = 1 ??? item_pos ???w??m
struct itemDATA_SAVE * CMapServer::CarryItem_FindFirstItem(CMapPlayer * pPlayer, long item_id, long * item_pos, long assign_item_pos)
{
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, val;

	pItemData = pPlayer->GetItemData();
	if (assign_item_pos)
	{
		if (item_pos)
		{
			pItem = &pItemData->plrCarryItem[*item_pos];
			if (pItem->m_Item.itemCode == item_id)
				return(pItem);
		}
	}
	else
	{
		//for (i=0; i<gameMAX_CARRYITEM; i++)
		val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
		for (i=0; i<val; i++)
		{
			pItem = &pItemData->plrCarryItem[i];
			if (pItem->m_Item.itemCode == item_id)
			{
				if (item_pos)
					*item_pos = i;
				return(pItem);
			}
		}
	}
	return(NULL);
}

struct itemDATA_SAVE * CMapServer::Card_FindItem(CMapPlayer * pPlayer, long item_id, char * CardNo, long * index)
{
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, val;

	if (index)
		*index = -1;
	pItemData = pPlayer->GetItemData();
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
	for (i=0; i<val; i++)
	{
		pItem = &pItemData->plrCarryItem[i];
		if (pItem->m_Item.itemCode == item_id)
		{
			// ??j?p?g?
			if (!strcmp(pItem->m_CardItem.itemCardNo, CardNo))
			{
				if (index)
					*index = i;
				return(pItem);
			}
		}
	}
	return(NULL);
}

/*
// ???s???d??O?L??
void Card_FindItemAndMarkFail(CMapPlayer * pPlayer, long item_id, char * CardNo)
{
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, val;
	struct plrSTORAGEITEM_SAVE * pStorageSave;

	pItemData = pPlayer->GetItemData();
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
	for (i=0; i<val; i++)
	{
		pItem = &pItemData->plrCarryItem[i];
		if (pItem->m_Item.itemCode == item_id)
		{
			// ??j?p?g?
			if (!strcmp(pItem->m_CardItem.itemCardNo, CardNo))
			{
				pItem->m_CardItem.itemStatus = gameVCARD_STATUS_FAIL;
				pPlayer->UpdateClientSingleItem( i, 0 );
				pPlayer->AutoSaveItemData();
			}
		}
	}
	// ??w
	if (pStorageSave = pPlayer->GetStorageData())
	{
		//for (i=0; i<gameMAX_STORAGEITEM; i++)
		val = gameServer_GetStorageItemMax(pPlayer->GetSaveData());
		for (i=0; i<val; i++)
		{
			pItem = &pStorageSave->psStoreItem[i];
			if (pItem->m_Item.itemCode == item_id)
			{
				// ??j?p?g?
				if (!strcmp(pItem->m_CardItem.itemCardNo, CardNo))
				{
					pItem->m_CardItem.itemStatus = gameVCARD_STATUS_FAIL;
					// !!!
					// ??s??w???~
					MAP_EXCHANGE_ITEM_RESULT sendmsg;
					sendmsg.m_UpdatePos = posOWN_STORAGE;
					sendmsg.m_UpdatePosID = i;
					::gameServer_Item_MakeShowSelf( pItem, &sendmsg.m_UpdateItem );
					if( pPlayer->GetClientID() )
						::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_EXCHANGE_ITEM_RESULT, (char*)&sendmsg , sizeof(sendmsg), 0 );
					// !!!
					pPlayer->AutoSaveStorageData();
				}
			}
		}
	}
}
*/
// ?]?w??d???~?{??
void CMapServer::Card_SetItemStatus(CMapPlayer * pPlayer, long item_id, char * CardNo, long status)
{
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, val;
	struct plrSTORAGEITEM_SAVE * pStorageSave;
	long is_find = 0;

	// ??a???~
	if (pItemData = pPlayer->GetItemData())
	{
		//for (i=0; i<gameMAX_CARRYITEM; i++)
		val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
		for (i=0; i<val; i++)
		{
			pItem = &pItemData->plrCarryItem[i];
			if (pItem->m_Item.itemCode == item_id)
			{
				// ??j?p?g?
				if (!strcmp(pItem->m_CardItem.itemCardNo, CardNo))
				{
					if (is_find)
						status = gameVCARD_STATUS_FAIL;					// ??s?H
					pItem->m_CardItem.itemStatus = (unsigned char)status;
					pPlayer->UpdateClientSingleItem( i, 0 );
					pPlayer->AutoSaveItemData();
					//
					is_find |= 1;
				}
			}
		}
	}
	// ??w
	if (pStorageSave = pPlayer->GetStorageData())
	{
		//for (i=0; i<gameMAX_STORAGEITEM; i++)
		val = gameServer_GetStorageItemMax(pPlayer->GetSaveData());
		for (i=0; i<val; i++)
		{
			pItem = &pStorageSave->psStoreItem[i];
			if (pItem->m_Item.itemCode == item_id)
			{
				// ??j?p?g?
				if (!strcmp(pItem->m_CardItem.itemCardNo, CardNo))
				{
					if (is_find)
						status = gameVCARD_STATUS_FAIL;					// ??s?H
					//
					pItem->m_CardItem.itemStatus = (unsigned char)status;
					// !!!
					// ??s??w???~
					MAP_EXCHANGE_ITEM_RESULT sendmsg;
					sendmsg.m_UpdatePos = posOWN_STORAGE;
					sendmsg.m_UpdatePosID = i;
					::gameServer_Item_MakeShowSelf( pItem, &sendmsg.m_UpdateItem );
					if( pPlayer->GetClientID() )
						::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_EXCHANGE_ITEM_RESULT, (char*)&sendmsg , sizeof(sendmsg), 0 );
					// !!!
					pPlayer->AutoSaveStorageData();
					//
					is_find |= 1;
				}
			}
		}
	}
}
// ??s?????
// pNewPlayer = ?s???????
// return: 0 ?|???s?u?A??????
long CMapServer::Card_SetOwner(CMapPlayer * pPlayer, char * CardNo, CMapPlayer * pNewPlayer)
{
	long sid;
	struct VT_CARD_UPDATE_RECORD nUpdateVT;

	// ??s???w
	sid = GetVTServer();
	if ( IsConnectedToServer(sid) )
	{
		memset(&nUpdateVT, 0, sizeof(nUpdateVT));
		nUpdateVT.nPlayerUID = pPlayer->GetUID();
		nUpdateVT.nMode = 2;		// 家Α穝局Τ
		msg_strncpy(nUpdateVT.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nUpdateVT.szAccount));
		msg_strncpy(nUpdateVT.szCardNo, CardNo, sizeof(nUpdateVT.szCardNo));
		msg_strncpy(nUpdateVT.szCharName, pPlayer->GetSaveData()->plrName, sizeof(nUpdateVT.szCharName));
		msg_strncpy(nUpdateVT.szOwnAccount, pNewPlayer->GetSaveData()->plrAccount, sizeof(nUpdateVT.szOwnAccount));
		CMapServer::GetServer()->SendData(sid, 0, PROTO_VT_CARD_UPDATE_RECORD, (char *)&nUpdateVT, sizeof(nUpdateVT), 0);
		return(1);
	}
	return(0);
}

#ifdef USE_BIG_VTCARD_ITEM
struct itemDATA_SAVE * CMapServer::Card_FindFuncItem(CMapPlayer * pPlayer, long item_id, long func_id, long * index)
{
	struct plrCARRYITEM_SAVE * pItemData;
	struct itemDATA_SAVE * pItem;
	long i, val;

	if (index)
		*index = -1;
	pItemData = pPlayer->GetItemData();
	//for (i=0; i<gameMAX_CARRYITEM; i++)
	val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
	for (i=0; i<val; i++)
	{
		pItem = &pItemData->plrCarryItem[i];
		if (pItem->m_Item.itemCode == item_id)
		{
			if (pItem->m_CardItem.itemCardFuncID == func_id)
			{
				if (index)
					*index = i;
				return(pItem);
			}
		}
	}
	return(NULL);
}
#endif
// =====================================================================
// ??? Trace
// =====================================================================
void InnerCB_MonitorPlayer_Recv(unsigned long monitor_data, char *buffer, int len, int id)
{
	struct MAP_TRACE_PLAYER_SERVER_FULL_RESULT nRet;
	long size;
	const long bufCap = (long)sizeof(nRet.nMsgData);
	const long maxInput = MAX_PACKET_LEN - (long)sizeof(struct proto_COMM) - 100;

//CMapServer::GetServer()->Log("Trace: Recv data(%d)", len);
	if (CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hLoginServer))
	{
		if (len < 0 || len > maxInput)
		{
			if (len > maxInput)
				CMapServer::GetServer()->Log("Trace: WARNING: Recv data too large(%d)", len);
			return;
		}
		//
		nRet.TargetConnectID = id;
		nRet.MonitorUID = monitor_data;
		nRet.nMsgTime = CMapServer::GetServer()->GetLoopTime();
		nRet.nMsgSize = len;
		nRet.nMsgCompressSize = 0;
		if (!buffer || !len)
		{
			nRet.nMsgData[0] = 0;
			nRet.nResult = TRACE_RESULT_TARGET_LOGOUT;
			size = nRet.GetRawSize() + nRet.nMsgSize;
		}
		else
		{
			if (len >= 30)
			{
				nRet.nMsgCompressSize = (unsigned short)CompressMemoryToMemoryZIP(nRet.nMsgData, buffer, len, bufCap);
				if (!nRet.nMsgCompressSize)
				{
					if (len > bufCap)
					{
						CMapServer::GetServer()->Log("Trace: WARNING: Recv uncompressed too large(%d > %d), drop", len, bufCap);
						return;
					}
					memcpyT(nRet.nMsgData, buffer, len);
				}
			}
			else
			{
				if (len > bufCap)
				{
					CMapServer::GetServer()->Log("Trace: WARNING: Recv payload too large(%d > %d), drop", len, bufCap);
					return;
				}
				memcpyT(nRet.nMsgData, buffer, len);
			}
			nRet.nResult = TRACE_RESULT_DATA_RECV_OK;
			//
			if (nRet.nMsgCompressSize)
			{
				size = nRet.GetRawSize() + nRet.nMsgCompressSize;
			}
			else
				size = nRet.GetRawSize() + nRet.nMsgSize;
		}
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
	}
}

void InnerCB_MonitorPlayer_Send(unsigned long monitor_data, char *buffer, int len, int id)
{
	struct MAP_TRACE_PLAYER_SERVER_FULL_RESULT nRet;
	long size;
	const long bufCap = (long)sizeof(nRet.nMsgData);
	const long maxInput = MAX_PACKET_LEN - (long)sizeof(struct proto_COMM) - 100;

//CMapServer::GetServer()->Log("Trace: Send data(%d)", len);
	if (CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hLoginServer))
	{
		if (len < 0 || len > maxInput)
		{
			if (len > maxInput)
				CMapServer::GetServer()->Log("Trace: WARNING: Send data too large(%d)", len);
			return;
		}
		//
		nRet.TargetConnectID = id;
		nRet.MonitorUID = monitor_data;
		nRet.nMsgTime = CMapServer::GetServer()->GetLoopTime();
		nRet.nMsgSize = len;
		nRet.nMsgCompressSize = 0;
		if (len >= 30)
		{
			nRet.nMsgCompressSize = (unsigned short)CompressMemoryToMemoryZIP(nRet.nMsgData, buffer, len, bufCap);
			if (!nRet.nMsgCompressSize)
			{
				if (len > bufCap)
				{
					CMapServer::GetServer()->Log("Trace: WARNING: Send uncompressed too large(%d > %d), drop", len, bufCap);
					return;
				}
				memcpyT(nRet.nMsgData, buffer, len);
			}
		}
		else
		{
			if (len > bufCap)
			{
				CMapServer::GetServer()->Log("Trace: WARNING: Send payload too large(%d > %d), drop", len, bufCap);
				return;
			}
			memcpyT(nRet.nMsgData, buffer, len);
		}
		nRet.nResult = TRACE_RESULT_DATA_SEND_OK;
		if (nRet.nMsgCompressSize)
		{
			size = nRet.GetRawSize() + nRet.nMsgCompressSize;
		}
		else
			size = nRet.GetRawSize() + nRet.nMsgSize;
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
	}
}

struct MAP_OBJECT_IN_FULL
{
	struct proto_COMM	m_nComm;
	short				m_nCount;
	struct plrDATASHOW	m_DataShow[MAP_OBJECT_IN_MAX_COUNT];

#ifdef __cplusplus
	long	GetSize(void)	{ return(sizeof(short) + sizeof(struct plrDATASHOW) * m_nCount); }
#endif
};

// ???e??????s????i???d?????????Trace??)
void CMapServer::Trace_MonitorSendSightObjects(unsigned long monitor_uid, CMapChar * pTarget)
{
	struct MAP_TRACE_PLAYER_SERVER_FULL_RESULT nRet;
	const long bufCap = (long)sizeof(nRet.nMsgData);
	long i;
	std::map<long, CMapObject *>::iterator iter;
	std::map<long, CMapObject *>::iterator iter_end;
	CMapBlock * pBlock;
	long hObject;
	CMapChar * pChar;
	struct plrDATASHOW * pShow;
	//struct plrDATASHOW_2 * pShow_2;
	struct proto_COMM * pComm;
	long len, size;
	struct MAP_OBJECT_IN * pin;
	struct MAP_OBJECT_IN_2 * pin_2;
	struct MAP_OBJECT_IN_FULL pinData;
	
	if (!pTarget)
		return;
	if(pTarget->IsOutMap())
		return;
	if (!pTarget->m_pSightBlock || pTarget->m_nSightBlocks <= 0)
		return;
	//
	nRet.MonitorUID = monitor_uid;
	nRet.nMsgTime = CMapServer::GetServer()->GetLoopTime();
	// ???q???[?K
	memset(&pinData, 0, sizeof(pinData));
	pComm = &pinData.m_nComm;
	pin = (struct MAP_OBJECT_IN *)&pinData.m_nCount;
	pin_2 = (struct MAP_OBJECT_IN_2 *)pin;
	//
	for (i=0; i<pTarget->m_nSightBlocks; i++)
	{
		pBlock = pTarget->m_pSightBlock[i];
		if (!pBlock)
			continue;
		iter_end = pBlock->GetEnd();
		for(iter = pBlock->GetBegin(); iter != iter_end; iter++)
		{
			hObject = iter->first;
			pChar = CMapServer::GetServer()->FindAndCheckCharExist(hObject);
			if (pChar == NULL)
				continue;
			if (pChar->m_nCharFlags & CHAR_FLAG_BOT_INVISIBLE)
				continue;
			pShow = pChar->GetShowData();
			memcpyT(pin->m_DataShow, pShow, sizeof(struct plrDATASHOW));
			if (IsPlayerUID(pShow->plrUID) || IsNPCUID(pShow->plrUID))
			{
full02:			pin->m_nCount = 1;
				pComm->pcProtoco = PROTO_MAP_OBJECT_IN;
				pComm->pcSize = (unsigned long)pin->GetSize();
				len = pComm->pcSize + sizeof(struct proto_COMM);
				//
				if (len > bufCap)
				{
#ifndef USE_PACKAGE_DATA
					CMapServer::GetServer()->Log("Trace: WARNING: sight OBJECT_IN too large(%d > %d), skip", len, bufCap);
#endif
					continue;
				}
				nRet.nMsgSize = (unsigned short)len;
				nRet.nMsgCompressSize = (unsigned short)CompressMemoryToMemoryZIP(nRet.nMsgData, &pinData, len, bufCap);
				if (!nRet.nMsgCompressSize)
				{
					memcpyT(nRet.nMsgData, &pinData, len);
				}
				nRet.nResult = TRACE_RESULT_UPDATE_IN;
			}
			else
			{
				if (pChar->m_nCharFlags & CHAR_FLAG_SHOWFULL)
					goto full02;
				pin_2->m_nCount = 1;
				pComm->pcProtoco = PROTO_MAP_OBJECT_IN_2;
				pComm->pcSize = (unsigned long)pin_2->GetSize();
				len = pComm->pcSize + sizeof(struct proto_COMM);
				//
				if (pShow->plrSex == sexFEMALE)
				{
					pin_2->m_DataShow[0].plrStatus |= effFun_FEMALE;
				}
				else if (pShow->plrSex == sexMALE)
				{
					pin_2->m_DataShow[0].plrStatus |= effFun_MALE;
				}
				//
				if (len > bufCap)
				{
#ifndef USE_PACKAGE_DATA
					CMapServer::GetServer()->Log("Trace: WARNING: sight OBJECT_IN_2 too large(%d > %d), skip", len, bufCap);
#endif
					continue;
				}
				nRet.nMsgSize = (unsigned short)len;
				nRet.nMsgCompressSize = (unsigned short)CompressMemoryToMemoryZIP(nRet.nMsgData, &pinData, len, bufCap);
				if (!nRet.nMsgCompressSize)
				{
					memcpyT(nRet.nMsgData, &pinData, len);
				}
				nRet.nResult = TRACE_RESULT_UPDATE_IN;
			}
			//
			if (nRet.nMsgCompressSize)
			{
				size = nRet.GetRawSize() + nRet.nMsgCompressSize;
			}
			else
				size = nRet.GetRawSize() + nRet.nMsgSize;
			CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
		}
	}
}

// ?q?? Login ??e???a????
void CMapServer::Trace_SendPlayerFullData(unsigned long uid, CMapPlayer * pTarget_Player)
{
	struct plrDATA * plr;
	CMapPlayer * pMonitor;
	struct MAP_TRACE_PLAYER_SERVER_FULL_RESULT nRet;
	long size;
	struct proto_COMM * pComm;
	char * buffer;
	//
	CMapPlayer * pPlayer;
	long client_id = 0;

	pMonitor = (CMapPlayer *)GetServer()->FindObjectByUID(uid, CMapPlayer::CLASS_ID);
	if (pMonitor)								// 菏跌
		client_id = pMonitor->GetClientID();
	//
	pPlayer = pTarget_Player;
	plr = pTarget_Player->GetCharData();		// 菏跌ヘ夹
	//
	if (IsConnectedToServer(m_hLoginServer))
	{
		// ???q???[?K
		pComm = (struct proto_COMM *)nRet.nMsgData;
		buffer = nRet.nMsgData + sizeof(struct proto_COMM);
		memset(pComm, 0, sizeof(struct proto_COMM));
		const size_t nTraceMsgCap = sizeof(nRet.nMsgData);
		const size_t nTraceHdr = sizeof(struct proto_COMM);
		//
		nRet.nMsgCompressSize = 0;
		nRet.nMsgTime = GetServer()->GetLoopTime();
		// ??H????
		struct plrDATASHOWSELF ShowSelf;

		if (nTraceHdr + sizeof(ShowSelf) > nTraceMsgCap)
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("Trace_SendPlayerFullData: ShowSelf need=%u cap=%u", (unsigned)(nTraceHdr + sizeof(ShowSelf)), (unsigned)nTraceMsgCap);
#endif
		}
		else
		{
			pComm->pcProtoco = PROTO_LOGIN_GETPLAYERDATA_SHOWSELFRESULT;
			pComm->pcSize = sizeof(ShowSelf);
			//
			memset(&ShowSelf, 0, sizeof(ShowSelf));
			gameServer_MakeShowSelf(plr, &ShowSelf);
			strcpy(ShowSelf.plrShowData.plrName, plr->plrSaveData.plrName);
			memcpyT(buffer, &ShowSelf, sizeof(ShowSelf));
			nRet.MonitorUID = uid;
			nRet.nMsgSize = sizeof(ShowSelf) + sizeof(struct proto_COMM);
			nRet.nResult = TRACE_RESULT_RESTORE_DATA;
			size = nRet.GetRawSize() + nRet.nMsgSize;
			if (client_id)
			{
				::msgSendData(client_id, 0, PROTO_MAP_TRACE_PLAYER_RESULT, (char *)&nRet, size, 0);
			}
			else
				SendData(m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
		}
		// ???
		struct plrDATA_SKILL nSkillData;

		if (nTraceHdr + sizeof(nSkillData) > nTraceMsgCap)
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("Trace_SendPlayerFullData: Skill need=%u cap=%u", (unsigned)(nTraceHdr + sizeof(nSkillData)), (unsigned)nTraceMsgCap);
#endif
		}
		else
		{
			pComm->pcProtoco = PROTO_LOGIN_GETPLAYERDATA_SKILL_RESULT;
			pComm->pcSize = sizeof(nSkillData);
			//
#ifdef SMART_PLR_DATA2
			::gameServer_MakeSkill(plr->plrSkillTable, &nSkillData);
#else
			::gameServer_MakeSkill(&plr->plrSkillTable, &nSkillData);
#endif
			memcpyT(buffer, &nSkillData, sizeof(nSkillData));
			nRet.MonitorUID = uid;
			nRet.nMsgSize = sizeof(nSkillData) + sizeof(struct proto_COMM);
			nRet.nResult = TRACE_RESULT_RESTORE_DATA;
			size = nRet.GetRawSize() + nRet.nMsgSize;
			if (client_id)
			{
				::msgSendData(client_id, 0, PROTO_MAP_TRACE_PLAYER_RESULT, (char *)&nRet, size, 0);
			}
			else
				SendData(m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
		}
		// ???~
		struct plrCARRYITEM nCarryItem;

		if (nTraceHdr + sizeof(nCarryItem) > nTraceMsgCap)
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("Trace_SendPlayerFullData: CarryItem need=%u cap=%u", (unsigned)(nTraceHdr + sizeof(nCarryItem)), (unsigned)nTraceMsgCap);
#endif
		}
		else
		{
			pComm->pcProtoco = PROTO_LOGIN_GETPLAYERDATA_ITEM_RESULT;
			pComm->pcSize = sizeof(nCarryItem);
			//
			//gameServer_MakeCarryItem(&plr->plrCarryItem, &nCarryItem);
			gameServer_MakeCarryItem(pPlayer->GetItemData(), &nCarryItem);
			memcpyT(buffer, &nCarryItem, sizeof(nCarryItem));
			nRet.MonitorUID = uid;
			nRet.nMsgSize = sizeof(nCarryItem) + sizeof(struct proto_COMM);
			nRet.nResult = TRACE_RESULT_RESTORE_DATA;
			size = nRet.GetRawSize() + nRet.nMsgSize;
			if (client_id)
			{
				::msgSendData(client_id, 0, PROTO_MAP_TRACE_PLAYER_RESULT, (char *)&nRet, size, 0);
			}
			else
				SendData(m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
		}
		// ??w
		struct plrSTORAGEITEM nStoreItem;

		if (nTraceHdr + sizeof(nStoreItem) > nTraceMsgCap)
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("Trace_SendPlayerFullData: Storage need=%u cap=%u", (unsigned)(nTraceHdr + sizeof(nStoreItem)), (unsigned)nTraceMsgCap);
#endif
		}
		else
		{
			pComm->pcProtoco = PROTO_LOGIN_GETPLAYERDATA_STORAGE_RESULT;
			pComm->pcSize = sizeof(nStoreItem);
			//
			//gameServer_MakeStorageItem(&plr->plrStorageItem, &nStoreItem);
			gameServer_MakeStorageItem(pPlayer->GetStorageData(), &nStoreItem);
			memcpyT(buffer, &nStoreItem, sizeof(nStoreItem));
			nRet.MonitorUID = uid;
			nRet.nMsgSize = sizeof(nStoreItem) + sizeof(struct proto_COMM);
			nRet.nResult = TRACE_RESULT_RESTORE_DATA;
			size = nRet.GetRawSize() + nRet.nMsgSize;
			if (client_id)
			{
				::msgSendData(client_id, 0, PROTO_MAP_TRACE_PLAYER_RESULT, (char *)&nRet, size, 0);
			}
			else
				SendData(m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
		}
		// ?p?L
		struct plrDATA_NPCSHOW nNPCData;

		if (nTraceHdr + sizeof(nNPCData) > nTraceMsgCap)
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("Trace_SendPlayerFullData: NPC need=%u cap=%u", (unsigned)(nTraceHdr + sizeof(nNPCData)), (unsigned)nTraceMsgCap);
#endif
		}
		else
		{
			//pComm->pcProtoco = PROTO_LOGIN_GETPLAYERDATA_NPC_RESULT;
			pComm->pcProtoco = PROTO_MAP_GET_SOLDIER_TABLE_RESULT;
			pComm->pcSize = sizeof(nNPCData);
			//
#ifdef SMART_PLR_DATA2
			gameServer_MakeNPC(plr->plrNPC, &nNPCData, pTarget_Player);
#else
			gameServer_MakeNPC(&plr->plrNPC, &nNPCData, pTarget_Player);
#endif
			memcpyT(buffer, &nNPCData, sizeof(nNPCData));
			nRet.MonitorUID = uid;
			nRet.nMsgSize = sizeof(nNPCData) + sizeof(struct proto_COMM);
			nRet.nResult = TRACE_RESULT_RESTORE_DATA;
			size = nRet.GetRawSize() + nRet.nMsgSize;
			if (client_id)
			{
				::msgSendData(client_id, 0, PROTO_MAP_TRACE_PLAYER_RESULT, (char *)&nRet, size, 0);
			}
			else
				SendData(m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
		}
		// ????
		struct plrMISSION nMissionData;

		if (nTraceHdr + sizeof(nMissionData) > nTraceMsgCap)
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("Trace_SendPlayerFullData: Mission need=%u cap=%u", (unsigned)(nTraceHdr + sizeof(nMissionData)), (unsigned)nTraceMsgCap);
#endif
		}
		else
		{
			pComm->pcProtoco = PROTO_LOGIN_GETPLAYERDATA_MISSION_RESULT;
			pComm->pcSize = sizeof(nMissionData);
			//
			//gameServer_MakeMission(&plr->plrMission, &nMissionData);
			gameServer_MakeMission(pPlayer->GetMissionData(), &nMissionData);
			memcpyT(buffer, &nMissionData, sizeof(nMissionData));
			nRet.MonitorUID = uid;
			nRet.nMsgSize = sizeof(nMissionData) + sizeof(struct proto_COMM);
			nRet.nResult = TRACE_RESULT_RESTORE_DATA;
			size = nRet.GetRawSize() + nRet.nMsgSize;
			if (client_id)
			{
				::msgSendData(client_id, 0, PROTO_MAP_TRACE_PLAYER_RESULT, (char *)&nRet, size, 0);
			}
			else
				SendData(m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
		}
		// ??????
		// ?V Login Server ?n?D???o??e??????, Login ?|??? monitor ??e
		struct LOGIN_GET_PARTY_INFO nParty;

		nParty.m_nUID = plr->plrSaveData.plrUID;
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_GET_PARTY_INFO, (char *)&nParty, sizeof(nParty), 0);
		// ?x?????Login Server ??s)
		struct LOGIN_ARMY_GET_MEMBER_LIST nArmyMemberData;

		nArmyMemberData.nPlayerUID = uid;
		nArmyMemberData.nTargetPlayerUID = pPlayer->GetUID();
		SendData(m_hLoginServer, 0, PROTO_LOGIN_ARMY_GET_MEMBER_LIST, (char *)&nArmyMemberData, sizeof(nArmyMemberData), 0);
		// ?n??W??(Client ?}?????|?Q?}?a)
	/*	struct MAP_ASK_FRIEND_TABLE_RESULT nFriend;

		pComm->pcProtoco = PROTO_MAP_ASK_FRIEND_TABLE_RESULT;
		pComm->pcSize = sizeof(nFriend);
		//
		memcpyT(&nFriend.m_DataFriends, &plr->plrFriend, sizeof(nFriend.m_DataFriends));
		memcpyT(buffer, &nFriend, sizeof(nFriend));
		nRet.MonitorUID = uid;
		nRet.nMsgSize = sizeof(nFriend) + sizeof(struct proto_COMM);
		nRet.nResult = TRACE_RESULT_RESTORE_DATA;
		size = nRet.GetRawSize() + nRet.nMsgSize;
		if (client_id)
		{
			::msgSendData(client_id, 0, PROTO_MAP_TRACE_PLAYER_RESULT, (char *)&nRet, size, 0);
		}
		else
			SendData(m_hLoginServer, 0, PROTO_MAP_TRACE_PLAYER_SERVER_RESULT, (char *)&nRet, size, 0);
		*/
	}
}
/*
// ?q?? Client ???a????
void CMapServer::Trace_SendPlayerFullData2(CMapPlayer * pPlayer, long type, char * buffer, long len)
{
	char * pData;
	long proto = 0;
	long size;

	pData = buffer;
	size = len;
	switch(type)
	{
	case 1:
		proto = PROTO_LOGIN_GETPLAYERDATA_SHOWSELFRESULT;
		break;
	case 2:
		proto = PROTO_LOGIN_GETPLAYERDATA_SKILL_RESULT;
		break;
	case 3:
		proto = PROTO_LOGIN_GETPLAYERDATA_ITEM_RESULT;
		break;
	case 4:
		proto = PROTO_LOGIN_GETPLAYERDATA_STORAGE_RESULT;
		break;
	//case 5:
	//	proto = PROTO_LOGIN_GETPLAYERDATA_NPC_RESULT;
	//	break;
	case 6:
		proto = PROTO_LOGIN_GETPLAYERDATA_MISSION_RESULT;
		break;
	}
	//
	if (proto)
		::msgSendData(pPlayer->GetClientID(), 0, proto, pData, size, 0);
}
*/

// ??x?q??
void CMapServer::BOT_NotifyGM(CMapPlayer * pPlayer, long type)
{
	struct LOGIN_MAP_FIND_BOT_NOTIFY nData;

	//nData.nBotCheckType = GetSaveData()->plrBotCheckType;
	nData.nBotCheckType = 0;
	nData.nBotType = (unsigned char)type;
	nData.nMapCode = pPlayer->GetMapCode();
	nData.uid = pPlayer->GetUID();
	msg_strncpy(nData.szName, pPlayer->GetName(), sizeof(nData.szName));
	msg_strncpy(nData.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nData.szAccount));
	SendData(GetLoginServer(), 0, PROTO_LOGIN_MAP_FIND_BOT_NOTIFY, (char *)&nData, sizeof(nData), 0);
}
// =====================================================================
// ?S??????
// type: 1=???_?[??, 2=?p?L?g???[??, 3=?\???[??, 4=?g??[??
long CMapServer::GetActivityStatus(long type)
{
	long cur_time;

	if (type == m_nActivityData.nType)
	{
		cur_time = GetLoopTime();
		if (cur_time <= m_nActivityData.nTimeEnd)
		{
			if (cur_time >= m_nActivityData.nTimeBegin)
			{
				return(m_nActivityData.nPower);
			}
		}
	}
	return(0);
}
// =====================================================================
//unsigned long CalcPercVal(unsigned long val, unsigned long perc);

// ----- Login Server -----
char* MSG_EXPIRE_WARNING;
char* MSG_PLAYER_NOT_EXIST_WARNING;
char* MSG_PLAYER_RELOGIN_LOCK;
char* MSG_PLAYER_DISABLE_LOCK;
char* MSG_LOGIN_SYSTEM_DISCONNECT;
char* MSG_LOGIN_SYSTEM_CONNECT_EXCEED;
char* MSG_ORG_STORAGE_IN_USE;
char* MSG_ORG_STORAGE_LEVEL_ERROR;
char* MSG_ORG_INVITE_FULL;
char* MSG_ORG_INVITE_ALREADY_HAVE;
char* MSG_ORG_INVITE_ONLY_LEADER;
char* MSG_ORG_NAME_DUP;
char* MSG_ORG_NEW_LEADER;
char* MSG_ORG_CHANGE_LEADER;
char* MSG_PARTY_CHANGE_LEADER;
char* MSG_FORCE_DESTROYED;
char* MSG_ARMY_CHANNEL_SETUP;
char* MSG_ARMY_CHANNEL_CANCEL;
char* MSG_CHATROOM_AREA01;
char* MSG_CHATROOM_AREA02;
char* MSG_CHATROOM_AREA03;
char* MSG_CHATROOM_AREA04;
char* MSG_ORG_REACH_MAX;
// ----- Map Server -----
char* MSG_PARTY_INVIT_LEADER_ONLY;
char* MSG_PARTY_INVIT_FREE_ONLY;
char* MSG_PARTY_INVIT_FULL;
char* MSG_SHOP_SPACE_NOT_ENOUGH;
char* MSG_ADD_EXP;
char* MSG_ADD_SKILL_EXP;
char* MSG_LOSE_SKILL_EXP;
char* MSG_SKILL_BLOCK_BY_WALL;
char* MSG_CWAR_RESURRECT_TIME;
char* MSG_CWAR_ABANDON_CITY;
char* MSG_JN_LORD_SET_AUTO_ALLOW;
char* MSG_JN_LORD_SET_NO_AUTO_ALLOW;
char* MSG_JN_LORD_ALLOW_NOTIFY;
char* MSG_JN_LORD_PLAYER_WAIT_NOTIFY;
char* MSG_JN_LORD_EXPEL_PLAYER;
char* MSG_CROSS_CWAR_WINNER_COUNTRY;
char* MSG_CROSS_CWAR_NO_WINNER_COUNTRY;
char* MSG_ADD_GOOD_FRIEND_OK;
char* MSG_SOMEONE_ADD_YOU_INTO_GOOD_FRIEND;
char* MSG_ADD_GOOD_FRIEND_FAILED;
char* MSG_MOVE_ITEM_OVERWEIGHT_FAILED;

#define MAP_CREATE_FAIL(msg)      do { Log("MapCreate FAIL: %s", (msg)); return(false); } while(0)
#define MAP_LOAD_STEP(name, call) (call)


bool CMapServer::Create(int nServerType)
{
	char * p;
	long i, node;
	char tmpstr[512];
	//
	struct iniSERVERMAPDATA *	pMapData;
	int							cnt1;
//	long vmin, vmax;

	gameServer_SetTargetCallBack(IsFreePKStagePartyTarget);
	//
	m_csDBMessage = csCreate();
	msgSetSendDataDefaultCompressMode(1);	// 箇砞ぃ溃罽戈家Α

#ifndef CONSOL_MODE
	m_pDialog		= (CMapServerDlg *)AfxGetMainWnd();
#endif
	m_IsCreating	= true;

//	char * p = NEW2 char[128];
//	cnt1 = gameGetB_Table(102, 1);

// ???? Map new ?I?s?y?{
//m_UIDMap[0] = 0;

//	if (gameCheckHitRatio((15 << 16) + 30))
//		i = 0;

//	FindObjectByUID(1);

//	CalcPercVal(50000000, 50);
//	NAI_CheckTickTime(0x8AC00146, 0);
//	NAI_CheckTickTime(0xFAC00146, 0);

/*	// ???? Base64
	struct itemDATA_SAVE nItem;
	uint dlen;

	dlen = sizeof(tmpstr);
	base64_encode( (uchar *)tmpstr, &dlen, (uchar *)&nItem, sizeof(nItem) );
*/
/*	strcpy(tmpstr, "1,2,,4");
	p = (char *)_mbstok((unsigned char *)tmpstr, (const unsigned char *)",");
	while(p)
		p = (char *)_mbstok(NULL, (const unsigned char *)",");
	//
	strcpy(tmpstr, "1,2,,4");
	i = (long)&tmpstr;
	p = (char *)_mbschr((const unsigned char *)i, ',');
	if (p)
	{
		*p = 0;
		i = (long)(p+1);
		//
		p = (char *)_mbschr((const unsigned char *)i, ',');
		if (p)
		{
			*p = 0;
			i = (long)(p+1);
			p = (char *)_mbschr((const unsigned char *)i, ',');
			if (p)
			{
				*p = 0;
				i = (long)(p+1);
				//
				p = (char *)i;
			}
		}
	}
*/

	// ???? CoolDown
/*	struct COOLDOWN_SAVE_DATA buffer;

	CoolDown_AddItem(100, 7, 60000, apiGetTickCounter());
	i = CoolDown_MakeSaveData(100, apiGetTickCounter(), &buffer);
*/

	LoadSetting("MapServer.ini");

#ifdef ItemMode
	gameServerStatus_SetStatus(SERVER_STATUS_ITEM_MODE);
#else
	gameServerStatus_SetStatus(0);
#endif
	gameServerType_SetType(CMapServer::GetServer()->GetServerVersion());

	if(!CBaseServer::Create(nServerType, true, g_ServerINI_Dir))
		MAP_CREATE_FAIL("CBaseServer::Create (Server.ini / network / napi)");

	// ?????D
#ifndef CONSOL_MODE
	if (m_pDialog)
	{
		if (m_nMapServerSubID > 1)
		{
 #if (defined(GAMEFLIER_NORMAL) || defined(GAMEFLIER_ITEMMALL) || defined(GAMEFLIER_CLASSIC))
			wsprintf(tmpstr, "MapServer%d(???y%d, %s)", m_nMapServerID, m_nMapServerSubID, m_ServerInfo.iniServerName);
 #else
			wsprintf(tmpstr, "MapServer%d(Sub%d, %s)", m_nMapServerID, m_nMapServerSubID, m_ServerInfo.iniServerName);
			
 #endif
		}
		else
			wsprintf(tmpstr, "MapServer%d(%s)", m_nMapServerID, m_ServerInfo.iniServerName);
 #ifdef ItemMode
  #if defined(TW_PVP_MODE)
		strcat(tmpstr, "(ItemMall-PVP)");
  #elif defined(GBMode_LIGHT)
		strcat(tmpstr, "(ItemMall-Light)");
  #elif defined(GAMEFLIER_CLASSIC)
		strcat(tmpstr, "(ItemMall-Classic)");
  #elif defined(USE_BLESS_MAX_20)
		strcat(tmpstr, "(ItemMall_CN_Bless)");
  #else
		strcat(tmpstr, "(ItemMall)");
  #endif
 #endif
		wsprintf(tmpstr+strlen(tmpstr), "(%s %s)", __DATE__, __TIME__);
		m_pDialog->SetWindowText(tmpstr);
	}
	//
	Log("服务器信息: %s", tmpstr);
#endif

//	CheckLockSelfIP();

#ifdef USE_PACKAGE_DATA
	{
		char pak_try[5][512];
		long pak_handle = 0;
		int pak_i;

		zpakCloseAllPackageFile();
		strcpy(pak_try[0], baseMakeDataFileName(mapPACKAGE_FILENAME));
		MapServer_MakeExeOverridePath(pak_try[1], mapPACKAGE_FILENAME);
		wsprintf(pak_try[2], "%s%s", apiGetCurrentPath(), mapPACKAGE_FILENAME);
		wsprintf(pak_try[3], "%sData\\%s", apiGetCurrentPath(), mapPACKAGE_FILENAME);
		strcpy(pak_try[4], mapPACKAGE_FILENAME);

		for (pak_i = 0; pak_i < 5; pak_i++)
		{
			if (!pak_try[pak_i][0])
				continue;
			pak_handle = zpakOpenPackageFile(pak_try[pak_i], 0 | 0x80000000);
			if (pak_handle)
				break;
		}
		if (!pak_handle)
			MAP_CREATE_FAIL("zpakOpenPackageFile(data.pak) - file missing or unreadable (OD/PK zip)");
	}
	//
#ifdef _WIN64
	scribeSetFileFunctionCall((long *)(intptr_t)zpakOpenFile, (long *)(intptr_t)zpakGetFileSize, (long *)(intptr_t)zpakReadFile, (long *)(intptr_t)zpakCloseFile, (long *)(intptr_t)zpakWriteFile);
	gameSetFileFunctionCall((long *)(intptr_t)zpakOpenFile, (long *)(intptr_t)zpakGetFileSize, (long *)(intptr_t)zpakReadFile, (long *)(intptr_t)zpakCloseFile, (long *)(intptr_t)zpakWriteFile);
#else
	scribeSetFileFunctionCall((long *)zpakOpenFile, (long *)zpakGetFileSize, (long *)zpakReadFile, (long *)zpakCloseFile, (long *)zpakWriteFile);
	gameSetFileFunctionCall((long *)zpakOpenFile, (long *)zpakGetFileSize, (long *)zpakReadFile, (long *)zpakCloseFile, (long *)zpakWriteFile);
#endif
	baseClearDataDir();						// 睲埃戈旧砞﹚
#endif

	m_pServer	= this;

//	SetSleepTimer(0);						// ?]?w?t?? Loop ?????
	SetSleepTimer(10);

	//m_nLastTickCount	= ::GetTickCount();
	//m_nLoopTickCount	= 0;

	// ??????]?w??
	if (!gameLoadTextResourceFiles())
		MAP_CREATE_FAIL("gameLoadTextResourceFiles (Data\\BootUp.txt)");
// ----- Login Server -----
	MSG_EXPIRE_WARNING = gameGetResourceName(5000201);
	MSG_PLAYER_NOT_EXIST_WARNING = gameGetResourceName(5000202);
	MSG_PLAYER_RELOGIN_LOCK = gameGetResourceName(5000203);
	MSG_PLAYER_DISABLE_LOCK = gameGetResourceName(5000205);
	MSG_LOGIN_SYSTEM_DISCONNECT = gameGetResourceName(5000206);
	MSG_LOGIN_SYSTEM_CONNECT_EXCEED = gameGetResourceName(5000207);
	MSG_ORG_STORAGE_IN_USE = gameGetResourceName(5000208);
	MSG_ORG_STORAGE_LEVEL_ERROR = gameGetResourceName(5000209);
	MSG_ORG_INVITE_FULL = gameGetResourceName(5000210);
	MSG_ORG_INVITE_ALREADY_HAVE = gameGetResourceName(5000211);
	MSG_ORG_INVITE_ONLY_LEADER = gameGetResourceName(5000212);
	MSG_ORG_NAME_DUP = gameGetResourceName(5000213);
	MSG_ORG_NEW_LEADER = gameGetResourceName(5000214);
	MSG_ORG_CHANGE_LEADER = gameGetResourceName(5000215);
	MSG_PARTY_CHANGE_LEADER = gameGetResourceName(5000216);
	MSG_FORCE_DESTROYED = gameGetResourceName(5000217);
	MSG_ARMY_CHANNEL_SETUP = gameGetResourceName(5000218);
	MSG_ARMY_CHANNEL_CANCEL = gameGetResourceName(5000219);
	MSG_CHATROOM_AREA01 = gameGetResourceName(5000220);
	MSG_CHATROOM_AREA02 = gameGetResourceName(5000221);
	MSG_CHATROOM_AREA03 = gameGetResourceName(5000222);
	MSG_CHATROOM_AREA04 = gameGetResourceName(5000223);
	MSG_PARTY_INVIT_LEADER_ONLY = gameGetResourceName(5000224);
	MSG_PARTY_INVIT_FREE_ONLY = gameGetResourceName(5000225);
	MSG_PARTY_INVIT_FULL = gameGetResourceName(5000226);
	MSG_SHOP_SPACE_NOT_ENOUGH = gameGetResourceName(5000227);
	MSG_ADD_EXP = gameGetResourceName(5000228);
	MSG_ADD_SKILL_EXP = gameGetResourceName(5000229);
	MSG_LOSE_SKILL_EXP = gameGetResourceName(5000230);
	MSG_SKILL_BLOCK_BY_WALL = gameGetResourceName(5000231);
	MSG_CWAR_RESURRECT_TIME = gameGetResourceName(5000232);
	MSG_CWAR_ABANDON_CITY = gameGetResourceName(5000233);
	MSG_JN_LORD_SET_AUTO_ALLOW = gameGetResourceName(5000234);
	MSG_JN_LORD_SET_NO_AUTO_ALLOW = gameGetResourceName(5000235);
	MSG_JN_LORD_ALLOW_NOTIFY = gameGetResourceName(5000236);
	MSG_JN_LORD_PLAYER_WAIT_NOTIFY = gameGetResourceName(5000237);
	MSG_JN_LORD_EXPEL_PLAYER = gameGetResourceName(5000238);
	MSG_CROSS_CWAR_WINNER_COUNTRY = gameGetResourceName(5000239);
	MSG_CROSS_CWAR_NO_WINNER_COUNTRY = gameGetResourceName(5000240);
	MSG_ADD_GOOD_FRIEND_OK = gameGetResourceName(5000241);
	MSG_SOMEONE_ADD_YOU_INTO_GOOD_FRIEND = gameGetResourceName(5000242);
	MSG_ADD_GOOD_FRIEND_FAILED = gameGetResourceName(5000243);
	MSG_MOVE_ITEM_OVERWEIGHT_FAILED = gameGetResourceName(5000244);
	MSG_ORG_REACH_MAX = gameGetResourceName(21496);
	//
	MAP_LOAD_STEP("Data\\Stage.txt", gameLoadStageTXT(baseMakeDataFileName("Data\\Stage.txt")));
	MAP_LOAD_STEP("Data\\Players.txt", gameServerLoadCharacterTXT(baseMakeDataFileName("Data\\Players.txt")));
	MAP_LOAD_STEP("Data\\Table_LevelUp.txt", gameLoadLevelUpTXT(baseMakeDataFileName("Data\\Table_LevelUp.txt")));
	MAP_LOAD_STEP("Data\\Table_AttrLevel.txt", gameLoadEnemyLevelUpTXT(baseMakeDataFileName("Data\\Table_AttrLevel.txt")));
	MAP_LOAD_STEP("Data\\Item.txt", gameLoadItemTXT(baseMakeDataFileName("Data\\Item.txt")));
	MAP_LOAD_STEP("Data\\Item2.txt", gameLoadItemTXT_Append(baseMakeDataFileName("Data\\Item2.txt")));
	MAP_LOAD_STEP("Data\\Magic.txt", gameLoadMagicTXT(baseMakeDataFileName("Data\\Magic.txt")));
	MAP_LOAD_STEP("Data\\Magic_Exp.txt", gameLoadMagicTXT_Exp(baseMakeDataFileName("Data\\Magic_Exp.txt")));
	MAP_LOAD_STEP("AccessoryBlessProb.txt", gameLoadAccessoryBlessProbTXT());
	// ?????????BlessRule.txt??
	gameLoadBlessRuleTXT();
	MapServer_LoadBlessEnhanceMaxRule();
	// ????????????CombatPowerRankReward.txt??
	gameLoadCombatPowerRankRewardTXT();
	gameLoadCombatPowerRankTitlesTXT();
	// ??????/??????WeaponHorseRankRule.txt??
	gameLoadWeaponHorseRankRuleTXT();
	// ????????????ACCESSORYBLESSDATA.TXT??
	gameLoadAccessoryBlessDataTXT();
#ifdef NEW_AI_METHOD
	gameLoadMagicGroupTXT(baseMakeDataFileName("Data\\Magic_Group.txt"));
#endif
	gameLoadCharacterAITXT(baseMakeDataFileName("Data\\AI_Table.txt"));
	gameLoadShopTXT(baseMakeDataFileName("Data\\ShopData.txt"));
	gameLoadShopTXT_Append(baseMakeDataFileName("Data\\ShopData2.txt"));
	gameLoadShopTXT_Append(baseMakeDataFileName("Data\\ShopData3.txt"));
	gameLoadShopTXT_Append(baseMakeDataFileName("Data\\ShopData4.txt"));
	gameLoadTeleportTXT(baseMakeDataFileName("Data\\Teleport.txt"));
#ifdef _DEBUG
	gameLoadDropItemTXT(baseMakeDataFileName("Data\\DropItem.txt"), FALSE);
#else
	gameLoadDropItemTXT(baseMakeDataFileName("Data\\DropItem.txt"), FALSE);
#endif
	//
	MAP_LOAD_STEP("Data\\ScriptFunc.txt", gameLoadScriptFuncTXT(baseMakeDataFileName("Data\\ScriptFunc.txt")));
	MAP_LOAD_STEP("Data\\Mission.txt", gameLoadMissionTableTXT(baseMakeDataFileName("Data\\Mission.txt")));
	MAP_LOAD_STEP("Data\\npc_talk.txt", gameLoadNPCTalkTXT(baseMakeDataFileName("Data\\npc_talk.txt")));
	MAP_LOAD_STEP("Data\\npc_talk_exp.txt", gameLoadNPCTalkTXT_Append(baseMakeDataFileName("Data\\npc_talk_exp.txt")));
	MAP_LOAD_STEP("Data\\ScriptTable.txt", gameLoadScriptTableTXT(baseMakeDataFileName("Data\\ScriptTable.txt")));
	// Preload ExchangeData.txt for NPC exchange UI mapping
	extern void PreloadExchangeDataForNpcTalk();
	PreloadExchangeDataForNpcTalk();
	//
	gameLoadMatrixTXT(baseMakeDataFileName("Data\\Matrix.txt"));
	gameLoadArmyTableTXT(baseMakeDataFileName("Data\\Army.txt"));
	gameLoadForceChangeTableTXT(baseMakeDataFileName("DATA\\ForceChange.txt"));
	gameLoadCityAreaTXT(baseMakeDataFileName("Data\\Stage_Info.txt"));
	MAP_LOAD_STEP("Data\\GenTalk.txt", gameLoadGenTalkTXT(baseMakeDataFileName("Data\\GenTalk.txt")));
	gameLoadOfficerTXT(baseMakeDataFileName("Data\\Officer.txt"));
	gameLoadCompositeTXT(baseMakeDataFileName("Data\\Composite_Table.txt"));
#ifdef USE_COMPOSITE_FREE
	// ?????t??X????
	gameLoadCompositeFreeTXT(baseMakeDataFileName("Data\\CompositeFree_Table.txt"));
#endif
	gameLoadCNPCJobUpTXT(baseMakeDataFileName("data\\CNPCJobUp.txt"));
	// Preload CNPCWEIGHT.TXT for soldier random upgrade on map server.
	gameServerPreloadCNPCWeightForUpgrade();
	gameLoadCNPC_ChangeItemTXT(baseMakeDataFileName("data\\CNPC_ChangeItem.txt"));
#ifdef SOLDIER_SOUL
	gameLoadCNPC_ConvertItemTXT(baseMakeDataFileName("data\\CNPC_ConvertItem.txt"));
#endif
	gameLoadCNPC_AttrLevelTXT(baseMakeDataFileName("data\\Table_SoldierAttrLevel.txt"));
#ifdef SUPER_SOLDIER
	gameLoadCNPC_SkillTXT(baseMakeDataFileName("data\\Table_SoldierSkill.txt"));
#endif
	gameLoadSoulSetTXT(baseMakeDataFileName("data\\SoulSet.txt"));
	gameLoadShipScheduleSetTXT(baseMakeDataFileName("data\\ShipSchedule.txt"));
	gameLoadCityGuardSetTXT(baseMakeDataFileName("data\\CityGuard.txt"));
	gameLoadHistoryBattleSetTXT(baseMakeDataFileName("data\\HistoryBattle.txt"));
#ifdef ItemMode
	gameLoadIModeShopSetTXT(baseMakeDataFileName("DATA\\ItemModeShop.txt"));
	IMode_MakeQuotaInfo();				// 弄 ItemModeShop 砞﹚郎ぇ㊣
#endif
#if defined CROSS_SCORE_ITEM
	gameLoadIModeShopSetTXT(baseMakeDataFileName("DATA\\ScoreModeShop.txt"));
#endif
	// ??
	gameLoadSetItemTXT(baseMakeDataFileName("data\\SetItem.txt"));
	gameLoadSetEffectTXT(baseMakeDataFileName("data\\SetEffect.txt"));
	// ?????????????Data\SoldierEquipAttrConfig.txt????KAP ??
	gameServer_PreloadSoldierEquipAttrConfigTXT();
	// ??????????????
	extern void LoadSvrWardrobeData();
	LoadSvrWardrobeData();
	// ??????????
	extern void LoadPowerUpConfig();
	LoadPowerUpConfig();
	// ??????????
	extern void LoadSvrFurnaceRules();
	LoadSvrFurnaceRules();
	gameLoadMapCmdTXT(baseMakeDataFileName("data\\MapCmd.txt"));
	gameLoadMapSpaceTXT(baseMakeDataFileName("data\\MapSpace.txt"));
	gameLoadMapSpaceTableTXT(baseMakeDataFileName("data\\MapSpaceTable.txt"));
 	gameLoadHistoryCMDTXT(baseMakeDataFileName("data\\HistoryCmd.txt"));
	gameLoadHistoryCMDTableTXT(baseMakeDataFileName("data\\HistoryTable.txt"));
	gameLoadCardToItemTXT(baseMakeDataFileName("data\\CardToItem.txt"));
	gameLoadCardCollectionTXT(baseMakeDataFileName("data\\CARDCOLLECTION.TXT"));
	extern void LoadGachaDrawPoolsFromFile(void);
	LoadGachaDrawPoolsFromFile();
	gameLoadCardGroupTXT(baseMakeDataFileName("data\\CARDGROUP.TXT"));  // ????????

	// ?????????????CardUpgrade.txt??
	LoadServerCardUpgradeTXT();
	
	// ????????
	extern void LoadServerEquipBookData();
	extern void LoadServerEquipBookActiveData();
	extern void LoadServerEquipBookAchieveData();
	LoadServerEquipBookData();
	LoadServerEquipBookActiveData();
	LoadServerEquipBookAchieveData();

	MapServer_RegisterCultivateAmuletFullReader();
	MAP_LOAD_STEP("CultivateTables_Init", gameServer_CultivateTables_Init());

	// ????????
	extern void LoadPowerUpConfig();
	LoadPowerUpConfig();

	// ????????????BattleExchangeRegisterMsg ??????????
	gameLoadEngineerTypeTXT(baseMakeDataFileName("data\\Engineer_Type.txt"));
	gameLoadEngineerSkillTXT(baseMakeDataFileName("data\\Engineer_Skill.txt"));
#ifdef USE_KS_HISTORY_ADD_REDSCORE
	gameLoadKSHistoryAddRedScoreTXT(baseMakeDataFileName("data\\KSHistoryAddScore.txt"));
#endif
#ifdef USE_KS_HISTORY_ADD_SCORE
	gameLoadKSHistoryAddScoreTXT(baseMakeDataFileName("data\\KSHistoryAddScore.txt"));
#endif
	//?Z?N?d
#ifdef USE_EQUIP_CARD
	gameLoadEquipCardTXT(baseMakeDataFileName("data\\EquipCard.txt"));
#endif
#ifdef EIGHT_GATES
	//?K???P??
	gameLoadEightGatesTXT(baseMakeDataFileName("data\\EightGates1.txt"));
	gameLoadEightGatesTXT_append(baseMakeDataFileName("DATA\\EightGates2.txt"));
	gameLoadEightGatesTXT_append(baseMakeDataFileName("DATA\\EightGates3.txt"));
	gameLoadEightGatesNeedTXT(baseMakeDataFileName("data\\EightGatesNeed.txt"));
	gameLoadEightGatesSetTXT(baseMakeDataFileName("data\\EightGatesSet.txt"));
#endif
	// ?W?N??
#ifdef GENERAL
	gameLoadGenTXT(baseMakeDataFileName("data\\General.txt"));
	gameLoadGenSetTXT(baseMakeDataFileName("data\\General_Set.txt"));
	gameLoadGenCollectionTXT(baseMakeDataFileName("data\\General_Collection.txt"));
#endif
	// ??y?t??
#ifdef EXTRA
	gameLoadExtraTXT(baseMakeDataFileName("Data\\Extra_Table.txt"));
#endif

	CWar_KS_LoadWorldSetting_Map(baseMakeDataFileName("data\\LoginKSCWarWorld.ini"));
	gameLoadKSSoulSetTXT(baseMakeDataFileName("data\\KSSWar_Soul.txt"));

#ifdef ALCHEMYSING_NOLEVEL
	gameLoadDismentleItemTXT(baseMakeDataFileName("data\\Dismantling_table.txt"));
#endif
#ifdef STAGE_EFFECT
	gameLoadMapEffectTXT(baseMakeDataFileName("data\\stage_effect.txt"));
#endif
#ifdef USE_BATTLE_SKILL
	gameLoadBattleSkillTXT(baseMakeDataFileName("data\\BattleSkill.txt"));
	gameLoadBattleSkillBookTXT(baseMakeDataFileName("data\\BattleSkillBook.txt"));
	gameLoadBattleSkillExpTXT(baseMakeDataFileName("data\\Table_BattleSkillLevelUp.txt"));
	gameLoadBattleSkillBookDismantlingTXT(baseMakeDataFileName("data\\BattleSkillDismantling.txt"));
#endif
	//
	// ?]?w?w????????????
	memset(&g_nSafeTownData, 0, sizeof(g_nSafeTownData));
	memset(&g_nSafeTownDetailData, 0, sizeof(g_nSafeTownDetailData));
	// ?]?w?T???O????
	memset(&g_nForceData, 0, sizeof(g_nForceData));
	msg_strncpy(g_nForceData[0].pwfName, gameGetResourceName(20074), sizeof(g_nForceData[0].pwfName));
	msg_strncpy(g_nForceData[0].pwfKingName, gameGetResourceName(24253), sizeof(g_nForceData[0].pwfKingName));
	g_nForceData[0].pwfCapital = gameCAPITAL_WEI;		// 锭
	msg_strncpy(g_nForceData[1].pwfName, gameGetResourceName(20075), sizeof(g_nForceData[1].pwfName));
	msg_strncpy(g_nForceData[1].pwfKingName, gameGetResourceName(24254), sizeof(g_nForceData[1].pwfKingName));
	g_nForceData[1].pwfCapital = gameCAPITAL_SHU;		// Θ常
	msg_strncpy(g_nForceData[2].pwfName, gameGetResourceName(20076), sizeof(g_nForceData[2].pwfName));
	msg_strncpy(g_nForceData[2].pwfKingName, gameGetResourceName(24255), sizeof(g_nForceData[2].pwfKingName));
	g_nForceData[2].pwfCapital = gameCAPITAL_WU;		// ??~
	//
	// .... ?O?d?W?l?P?????W?l ....
	m_nPNameReverse.SetStringToLowerCase(1);	// mode = 1 锣Θ糶ゑ耕

	p = baseMakeDataFileName("Data\\Name_Reverse.txt");
	if (scribeLoadFile(p))
	{
		if (node = scribeGetNodeSectionPosition("reverse_name", 1))
		{
			i = 1;
			while (scribeReadString("item", i++, node, tmpstr))
			{
				m_nPNameReverse.AddString(tmpstr);
			}
		}
		//
		scribeFreeFile();
	}
	m_nPNameDirty.SetStringToLowerCase(1);	// mode = 1 锣Θ糶ゑ耕
	p = baseMakeDataFileName("Data\\Name_Dirty.txt");
	if (scribeLoadFile(p))
	{
		if (node = scribeGetNodeSectionPosition("dirty_name", 1))
		{
			i = 1;
			while (scribeReadString("item", i++, node, tmpstr))
			{
				m_nPNameDirty.AddString(tmpstr);
			}
		}
		//
		scribeFreeFile();
	}
	m_nPNameCountry.SetStringToLowerCase(1);	// mode = 1 锣Θ糶ゑ耕
	p = baseMakeDataFileName("Data\\Name_Country.txt");
	if (scribeLoadFile(p))
	{
		if (node = scribeGetNodeSectionPosition("country_name", 1))
		{
			i = 1;
			while (scribeReadString("item", i++, node, tmpstr))
			{
				m_nPNameCountry.AddString(tmpstr);
			}
		}
		//
		scribeFreeFile();
	}

	// ???~??
	MAP_LOAD_STEP("gameBuildB__Table", gameBuildB__Table());

	// ?b??l?????I?O?a??s??????????C
	MAP_LOAD_STEP("gamePayStageScanTalkAndSetStageTable", gamePayStageScanTalkAndSetStageTable());

	// ===== ????? =====
//	i = (long)gameMagic_GetPointer(502);
//	struct itemDATASHOWSELF nItemSF;

//	gameCreateItem_NPC(&nItemSF, 6557, 1, 123, "");
//	i = gameCheckB_Table(1, gameBOT_RNDTABLE_W, 0xf99);

//	i = gameGetMinMaxVal((10000 << 16) + 40000);

//	SetWarStatus(1);						// ?}????
//	CMapPlayer * pPlayer = new CMapPlayer;
//	pPlayer->gameServer_NPCTalkProcess1(25);
//	pPlayer->gameServer_NPCTalkProcess2(25, 0);

//	TradePsw_AskPlayerPassword(pPlayer, 0);

/*	struct plrDATA_NPC_SAVE nSave;
	struct plrDATA nData;
	struct plrDATA_NPC_SHOWSELF nSelf;

	memset(&nSave, 0, sizeof(nSave));
	strcpy(nSave.plrNPC_Name, "Tester 1");
	nSave.plrNPC_Level = 1;
	nSave.plrNPC_HP = 70;
	nSave.plrNPC_UID = 0x12345678;
	nSave.plrNPC_CountryID = 2;
	nSave.plrNPC_Loyalty = 30;
	nSave.plrNPC_ItemCode = 6501;
	nSave.plrNPC_BaseAttrStr = 10;
	nSave.plrNPC_BaseAttrInt = 10;
	nSave.plrNPC_BaseAttrMind = 10;
	nSave.plrNPC_BaseAttrCon = 10;
	nSave.plrNPC_BaseAttrDex = 10;
	nSave.plrNPC_BaseAttrLeader = 0;

	gameServer_NPC_MakeFullData(&nSave, &nData, BA_ODD_BOX, 0xff);
	//nData.plrSaveData.plrEquipHead.m_Item.itemCode = 2401;	// ???U
	//nData.plrSaveData.plrEquipFoot.m_Item.itemCode = 2801;	// ???c
	gameServer_CalcCharacterAttribute(&nData);
	gameServer_NPC_MakeShowSelf(&nData, &nSelf);
*/

//	::gameGetSkillDamage(43, 10, 10, &vmin, &vmax);
//	cnt1 = gameGetRandomRange(vmin, vmax+1);
	//
	//struct effectDATA * pEffect = ::gameGetItemEffectPtr(5010);
//	struct magicDATA *	pMagicData;
//	struct plrDATA pPlayer;
//	long effFlag, effResult;
//
//	pMagicData = gameMagic_GetPointer(109);		// ?C?P??R
//	//pPlayer.plrSaveData.plrStatus = effFun_POISON;
//	//pPlayer.plrShowData.plrStatus = effFun_POISON;
//	//pPlayer.plrAntiStatus = 0;
//	memset(&pPlayer, 0, sizeof(pPlayer));
//	pPlayer.plrSkillTable = &plrEnemySkillTable;
//	pPlayer.plrNPC = &plrEnemyNPC;
//	pPlayer.plrSaveData.plrHP = 100;
//	effFlag = ::gameCalcEffect(&pMagicData->magicEffect, &pPlayer, &pPlayer, pMagicData->magicEffect.effFunction, 0, 0, &effResult);
//	effFlag = ::gameCalcEffect(&pMagicData->magicEffect, &pPlayer, &pPlayer, pMagicData->magicEffect.effFunction, 0, 0, &effResult);
	//
// ????????? GameResource ID ????, 9 = ?????????\?G%s.JPG
//	wsprintf(tmpstr, "Test: %s", gameGetResourceName(9));
//	wsprintf(tmpstr, gameGetResourceName(9), 0xffffffff);
//	wsprintf(tmpstr, "Test: %s", NULL);
// ????O??????
//	p = new char[4096000000];
	// ==================

	// ????K?X?t??
	if (!TradePsw_Init())
		MAP_CREATE_FAIL("TradePsw_Init (trade password images)");
	pMapData = m_nMapServerIdx;
	if (!pMapData)
		MAP_CREATE_FAIL("m_nMapServerIdx is NULL (MapServer.ini id / Server.ini map list)");

	// ?]?w UID ?d??
	m_nItemUIDMin	= pMapData->iniUID_Min;
	m_nItemUIDMax	= pMapData->iniUID_Max;
	m_nItemUID		= m_nItemUIDMin;

	// ?t?m???? handle ???
	m_nFreeHandle	= 0;
	m_pMapObject	= new CMapObject *[MAX_SERVER_OBJECTS];
	if(m_pMapObject == NULL)
		MAP_CREATE_FAIL("new CMapObject*[MAX_SERVER_OBJECTS] memory allocation");
	ZeroMemory(m_pMapObject, sizeof(CMapObject *) * MAX_SERVER_OBJECTS);
	if(!m_MapManager.Create(pMapData->iniMapTotal, (int *)pMapData->iniMapCode))
		MAP_CREATE_FAIL("MapManager.Create (Level*.wrd / obs / bin map files)");
	// ???????`??
	Log("总计物件 = %d", m_nFreeHandle);

	// ???????????i?J?a???
	for(cnt1 = 0; cnt1 < MAX_SERVER_OBJECTS; cnt1++)
	{
		if(m_pMapObject[cnt1] != NULL)
		{
			m_pMapObject[cnt1]->EnterMap();
			// ?? TempMap ?????M??
			if (m_TempMap.find(cnt1) != m_TempMap.end())
				m_TempMap.erase(cnt1);
		}
	}
	// ?Z??]?w
	InnerSoulRecordInit();
	// ???]?w
	m_nShipSchedule.ShipScheduleInit();
	// ???X??e?i???
	{ MEMORYSTATUSEX _ms; _ms.dwLength=sizeof(_ms); GlobalMemoryStatusEx(&_ms); }
	// ?u???Z?N?]?w
	m_nCityGuard.CityGuardInit();
	// ???v???]?w
	m_nHistoryManager.HistoryInit();
	// ????]?w
	GetMapSpaceManager()->MapSpace_Init();
 	GetHistoryManager()->HistoryCMD_Init();
	// ????]?w(??A)
	GetMapSpaceManager()->CreateCopy();//ミ捌セ
	MAP_LOAD_STEP("initialize_hdoc", initialize_hdoc());
	MAP_LOAD_STEP("InitGachaSystem", InitGachaSystem());
	// ???? / ???????????initialize_hdoc ??????scribe ??????
	MAP_LOAD_STEP("LoadExchangeData", LoadExchangeData());
	MAP_LOAD_STEP("LoadLegionExchangeData", LoadLegionExchangeData());
	MAP_LOAD_STEP("JinshanDigServer_LoadConfig", JinshanDigServer_LoadConfig());
	MAP_LOAD_STEP("TowerSlashServer_LoadConfig", TowerSlashServer_LoadConfig());
	MAP_LOAD_STEP("BotPlayer.ini", BotManager::Instance()->Init(baseMakeDataFileName("BotPlayer.ini")));
//	Log("Block = %d,%d", MAP_BLOCK_WIDTH, MAP_BLOCK_HEIGHT);

	// Test 2
//	CMapCtrl * pMapCtrl = CMapServer::GetServer()->GetMapManager()->FindMap(45);
//	if (pMapCtrl)
//	{
//		pMapCtrl->CheckMoveLineFast(100, 20, 110, 20);		// ?k??
//		pMapCtrl->CheckMoveLineFast(100, 20, 90, 20);		// ????
//		pMapCtrl->CheckMoveLineFast(100, 20, 100, 10);		// ?W??
//		pMapCtrl->CheckMoveLineFast(100, 20, 100, 30);		// ?U??
//		pMapCtrl->CheckMoveLineFast(100, 20, 110, 10);		// ?k?W
//		pMapCtrl->CheckMoveLineFast(100, 20, 110, 30);		// ?k?U
//		pMapCtrl->CheckMoveLineFast(100, 20, 90, 10);		// ???W
//		pMapCtrl->CheckMoveLineFast(100, 20, 90, 30);		// ???U
//	}
	//
//	slgDetectAttackRange(gameWEAPON_ATTACK_RANGE_TYPE, 100, 100, 10, 10, 4, 1, 2);
	//
//	long hPlayer = CreatePlayer(1, 18);
/*	CMapPlayer * pPlayer = new CMapPlayer;
	if(pPlayer->Create(m_nFreeHandle, 1707, 1000000))
	{
		m_nFreeHandle++;
		//
		//CreateNPCFromSummon(1707, pPlayer, 0);

		RECT dRect;
		GetMapSpaceManager()->MapSpace_FindEnterMap(pPlayer, &dRect);
	}
	*/
//	gameGetDropItem(20000000, 1, 2);
//	struct itemDATA_SAVE nItem;

//	memset(&nItem, 0, sizeof(nItem));
//	nItem.m_Item.itemCode = 1;
//	CompositeEnchanceItem(&nItem, 7);
	//
/*	CMapNPC * pNPC = CreateEnemy(1000, 1000, 1104, 1);		// ???

	pNPC->m_HurtRecord[0].m_nHurtTotal = 1000000;
	pNPC->m_HurtRecord[0].m_nPlayerUID = 1;
	pNPC->m_HurtRecord[0].m_hAttacker = 1;
	pNPC->m_HurtRecord[0].m_nTime = time(NULL) + 100000;
	pNPC->m_HurtRecord[1].m_nHurtTotal = 100;
	pNPC->m_HurtRecord[1].m_nPlayerUID = 2;
	pNPC->m_HurtRecord[1].m_hAttacker = 2;
	pNPC->m_HurtRecord[1].m_nTime = time(NULL) + 100000;
	//
	pNPC->DistributeExpAndItem(NULL);
	InnerProcGenBossDieTeleport(pNPC);
*/

	//CMapNPC * pNPC2 = CreateEnemy(1000, 1000, 180, 1);		// ????j?n
	//CMapNPC * pNPC3 = CreateEnemy(1000, 1000, 801, 1);			// xxx_test ?W?????
	//pNPC->GetCharData()->plrMagicPower = 20;
	//gameCalcFinalDamage_Magic(27, 1, pNPC->GetCharData(), pNPC2->GetCharData());
	//i = gameATK_GetSkillMagicDamageLevel(pNPC->GetCharData());
//	pNPC = CreateEnemy(1000, 1000, 1201, 1);				// ?P??
//	pNPC->MagicDamageProcess(1, 2, pNPC, gameMagic_GetPointer(1), (long *)&cnt1);
//	CMapServer::GetServer()->ChangeEnemyCodeData(pNPC, 1000, 1000, 1104, 5);

//	GetServer()->m_nHistoryManager.SendSPMessage_PlayerGetFlag(NULL, (CMapPlayer *)pNPC, 1);

//	char tmpstr[1024];
//	CMapServer::GetServer()->GetMapManager()->MakeSPMessage_Flee(pNPC, pNPC, tmpstr);
//	CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(pNPC->GetMapID(), tmpstr);

//	struct gameCITY_AREA * pCityArea = gameGetCityAreaPtr(1);

//	CMapCtrl * pMapCtrl = CMapServer::GetServer()->GetMapManager()->FindMap(1);
//	if (pMapCtrl)
//	{
//		char * name = gameGetResourceName(pMapCtrl->GetStageData()->gstgNameID);
//	}

/*	struct gameARMY_DATA * pArmy = gameGetArmyTablePtr(4);
	for (cnt1=0; cnt1<gameMAX_ARMY_MAPPOS; cnt1++)
	{
		long map_id = pArmy->adMapPos[cnt1*3];
		if (!map_id)
			break;
		long x = pArmy->adMapPos[cnt1*3+1];
		long y = pArmy->adMapPos[cnt1*3+2];
		long code = m_MapManager.Army_GetGenCode((long*)pArmy->adList, gameMAX_ARMY_LIST);
		if (code)
		{
			m_MapManager.Army_RecordAdd(code, 1);
		}
	}
*/
//	GetMapManager()->MakeSPMessage_Talk(pNPC, 0, tmpstr);
//	CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(pNPC->GetMapID(), tmpstr);

	// ?s?u?w??
#ifdef USE_CLIENT_DIV_PROCESS
	::napiServer_SetInputQueueProcNumber(1, 100);	// 100
	::napiServer_SetClientDivProcess(1);		// ?_???B?z?[?t
#else
	::napiServer_SetClientDivProcess(0);
	::napiServer_SetInputQueueProcNumber(2, 120);
#endif
	::smsSetMessageNotify(cbInvalidMessageNotify);
	::napiServer_SetSendQueueMaxMemorySize(10240000);		// ?w?]??10MB
#if NET_SERVER_DIAG
	::napiServer_SetNetDiagLogFn(MapServer_NetDiagLog);
#endif
	// ?s?u?? Login server
	Log("连接登录服务器: %s:%d", m_ServerInfo.iniIP_Login, m_ServerInfo.iniPort_Login);
	m_hLoginServer = JoinServer(m_ServerInfo.iniIP_Login, m_ServerInfo.iniPort_Login, 0, 5);
	if (!m_hLoginServer)
		MAP_CREATE_FAIL("JoinServer Login (napiH_JoinServer returned 0)");
	// ?s?u?? DB server
	Log("连击数据服务器: %s:%d", m_ServerInfo.iniIP_DB, m_ServerInfo.iniPort_DB);
	m_hDBServer = JoinServer(m_ServerInfo.iniIP_DB, m_ServerInfo.iniPort_DB, 0, 5, SERVER_CHECKMODE_RECV_ALL | SERVER_CHECKMODE_SEND_ALL);
	if (!m_hDBServer)
		MAP_CREATE_FAIL("JoinServer DB (napiH_JoinServer returned 0)");
	TowerSlashServer_RequestRankFromLogin();
	// ?s?u?? Log server
	m_hLogServer = 0;
	if (m_ServerInfo.iniPort_Log)
	{
		Log("连接日志服务器: %s:%d", m_ServerInfo.iniIP_Log, m_ServerInfo.iniPort_Log);
		m_hLogServer = JoinServer(m_ServerInfo.iniIP_Log, m_ServerInfo.iniPort_Log, 0, 30);
		if (!m_hLogServer)
		{
			Log("! 加入日志服务器错误");
			//return(false);
		}
	}
	else
		Log("跳过: 没有日志服务器设置 !");
	// 硈絬 VT server
	m_hVTServer = 0;
	if (m_ServerInfo.iniPort_VT)
	{
		Log("连接商城服务器: %s:%d", m_ServerInfo.iniIP_VT, m_ServerInfo.iniPort_VT);
		m_hVTServer = JoinServer(m_ServerInfo.iniIP_VT, m_ServerInfo.iniPort_VT, 0, 30);
		if (!m_hVTServer)
		{
			Log("! 加入商城服务器错误");
			//return(false);
		}
	}
	else
		Log("跳过: 没有商城服务器设置 !");
	// 硈絬 Mail Server
	MailStartup();
	if (m_ServerInfo.iniPort_MAIL)		// ???]?w???~?n
	{
		if (!SendMailServerConnect(m_ServerInfo.iniIP_MAIL, m_ServerInfo.iniPort_MAIL))
			Log("! 加入邮件服务器错误!");
	}

	// ?t???p
	m_SpeedRecord.SetFrameRatioStart();
	//m_SpeedRecord.SetCountDownTick(1000*60*5);	// ?C5????
	m_SpeedRecord.SetCountDownTick(0);

	m_nLastTickCount	= ::GetTickCount();
	m_nLoopTickCount	= 0;

	m_IsCreating			= false;

	m_nLoadWorldDataTime	= 0;
	m_nLocalBaseTime		= 0;
	m_nWorldBaseTime		= 0;
	m_IsWorldDataReady		= false;
	m_IsWorldTimeReady		= false;
	SetClientConnectTimeout(DEFAULT_CLIENT_CONNECT_TIMEOUT);



//	CreateEnemy(1000, 1000, 70, 123);

	//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);	// ???CUI?t??
	SetFlag( FLAG_PAUSE, false );
	return(true);
}

void CMapServer::EnterMap(long map_code, long copy_uid)
{
	int cnt1;

	// ???????????i?J?a???
	for(cnt1 = 0; cnt1 < MAX_SERVER_OBJECTS; cnt1++)
	{
		if(m_pMapObject[cnt1] != NULL)
		{
			if(m_pMapObject[cnt1]->GetMapID() == map_code && m_pMapObject[cnt1]->GetMapCopyUID() == copy_uid)
			{
				m_pMapObject[cnt1]->EnterMap();
				// ?? TempMap ?????M??
				if (m_TempMap.find(cnt1) != m_TempMap.end())
					m_TempMap.erase(cnt1);
			}
		}
	}
}

void CMapServer::Destroy(void)
{
	if(m_pMapObject != NULL)
	{
		delete [] m_pMapObject;
		m_pMapObject = NULL;
	}

	m_UIDMap.clear();
	m_ClientMap.clear();

	m_MapManager.Release();

	CBaseServer::Destroy();

	MailCleanup();
}

void CMapServer::Shutdown_SaveAllData()
{
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;
	CMapPlayer * pPlayer;

	m_nHistoryManager.History_SaveAllData();
	//
	mMapPlayer = &GetClientPlayerMap();
	iter_end = mMapPlayer->end();
	for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
	{
		pPlayer	= iter->second;
		if (pPlayer)
		{
		/*	if (pPlayer->m_bSaveDataFlag)
			{
				pPlayer->SaveCharData();
			}
			if (pPlayer->m_bSaveNPCFlag)
			{
				pPlayer->SaveNPCData();
			} */
			pPlayer->AutoSaveCharData(1);
			pPlayer->AutoSaveItemData(1);
			pPlayer->AutoSaveSkillData(1);
			pPlayer->AutoSaveStorageData(1);
			pPlayer->AutoSaveNPCData(1);
		}
	}
	//Save player Max number -- chenyin
#ifdef NEW_PLAYER_STATISTICS
	SavePlayerMaxNumber();
#endif
}

void CMapServer::Shutdown_ClearOnly(HWND hWnd, long wait_sec, long no_close_server)
{
	Shutdown_SaveAllData();
	KickAllClient();
	//
	CBaseServer::Shutdown_ClearOnly(hWnd, wait_sec, no_close_server);
}

#ifndef USE_PACKAGE_DATA
void InnerMakeMemorySizeString(DWORD size, char * buffer)
{
	DWORD size_dot;

	if (size > 1024)						// K
	{
		if (size > 1024*1024)				// M
		{
			if (size > 1024*1024*1024)		// G
			{
				//size_dot = ((size & 0x3fffffff) * 100) >> 30;		// ?|????
				size_dot = (((size & 0x3fffffff) >> 8) * 100) >> 22;
				size = size >> 30;
				wsprintf(buffer, "%d.%02dGB", size, size_dot);
			}
			else
			{
				size_dot = ((size & 0xfffff) * 100) >> 20;
				size = size >> 20;
				wsprintf(buffer, "%d.%02dMB", size, size_dot);
			}
		}
		else
		{
			size_dot = ((size & 0x3ff) * 100) >> 10;
			size = size >> 10;
			wsprintf(buffer, "%d.%02dKB", size, size_dot);
		}
	}
	else
	{
		wsprintf(buffer, "%dBytes", size);
	}
}
#endif

// ?O??: Loop ?? frame
//		 Tick ??
bool CMapServer::Loop(void)
{
	unsigned long	nTickCount;
	long			nData;
	static long connect_to_login_state = 0;
	static unsigned long time_update_tick = 0;
	static unsigned long time_update_txt_tick = 5000;
	//
#ifndef USE_PACKAGE_DATA
	char memstr1[64];
	char memstr2[64];
#endif
	char tmpstr[2048];
	long nDoSpeedRecord;
	unsigned long nTick;
	unsigned long n;
	static long nUpdateCount = -1;
	static unsigned long nRecFrame = 0;
	static unsigned long nRecT1 = 0;
	static unsigned long nRecT2 = 0;
	static unsigned long nRecT3 = 0;
	static unsigned long nRecT4 = 0;
	static unsigned long nRecT5 = 0;
	static unsigned long nRecT6 = 0;
	static unsigned long nRecT10 = 0;
	static unsigned long nRecT11 = 0;
	static unsigned long nRecT_flush = 0;

	nTickCount = ::GetTickCount();
	if(nTickCount < m_nLastTickCount)
	{
		m_nLoopTickCount = nTickCount + ((unsigned long)0xffffffff - m_nLastTickCount);
		if (m_nLoopTickCount & 0x80000000)
			m_nLoopTickCount = m_nLastTickCount - nTickCount;
	}
	else
		m_nLoopTickCount = nTickCount - m_nLastTickCount;
	m_nLastTickCount = nTickCount;
	//
	time_update_tick += m_nLoopTickCount;
	//
	if (time_update_tick >= 1000)
	{
		unsigned long nSecMs = (unsigned long)time_update_tick;
		time_update_tick = 0;
		m_nLoopTime = (long)::time(NULL);
		m_nDayState = gameDayNight_GetState(GetWorldTime());
		PollCombatPowerRankTitleFromLogin();
		MapBossRt_Tick(this, nSecMs);
		
		/* Check if nation war has ended */
		if (m_IsWar && m_nWarType == WAR_TYPE_NATION && m_nWarEndTime > 0)
		{
			if ((unsigned long)m_nLoopTime >= m_nWarEndTime)
			{
				/* Use SetWarStatus to properly end war and broadcast to all clients */
				SetWarStatus(0, 1);  /* 0 = war ended, 1 = broadcast */
				m_nWarType = 0;
				m_nWarEndTime = 0;
				m_nNationWarAttacker = 0;
				m_nNationWarDefender = 0;
				Log("Nation War Ended - War mode disabled");
			}
		}
	}
	//
// ?????(?L?X??e?????`??)
#ifndef USE_PACKAGE_DATA
	time_update_txt_tick += m_nLoopTickCount;
	if (time_update_txt_tick >= 5000)
	{
		time_update_txt_tick = 0;
		// ?????D
 #ifndef CONSOL_MODE
		if (m_pDialog)
		{
			DWORD PhysicalTotal, PhysicalAvail, VirtualTotal, VirtualAvail;

			apiGetWindowsFreeMemorySize(&PhysicalTotal, &PhysicalAvail, &VirtualTotal, &VirtualAvail);
			//
			//PhysicalAvail = 537012300;		// 512.13M
			//PhysicalAvail = 2917602851;	// 2.71G
			InnerMakeMemorySizeString(PhysicalAvail, memstr1);
			InnerMakeMemorySizeString(VirtualTotal - VirtualAvail, memstr2);
			//
			if (m_nMapServerSubID > 1)
			{
  #if (defined(GBMode) || defined(GBMode_TW))
				wsprintf(tmpstr, "Map%d(Sub%d,%s)%d(%s,%s)", m_nMapServerID, m_nMapServerSubID, m_ServerInfo.iniServerName, g_nObjectCounter, memstr1, memstr2);		// VirtualAvail
  #else
				wsprintf(tmpstr, "Map%d(???y%d,%s)%d(%s,%s)", m_nMapServerID, m_nMapServerSubID, m_ServerInfo.iniServerName, g_nObjectCounter, memstr1, memstr2);		// VirtualAvail
  #endif
			}
			else
				wsprintf(tmpstr, "Map%d(%s)%d(%s,%s)", m_nMapServerID, m_ServerInfo.iniServerName, g_nObjectCounter, memstr1, memstr2);		// VirtualAvail
			//
  #ifdef ItemMode
   #if defined(TW_PVP_MODE)
		strcat(tmpstr, "(ItemMall-PVP)");
   #elif defined(GBMode_LIGHT)
		strcat(tmpstr, "(ItemMall-Light)");
   #elif defined(GAMEFLIER_CLASSIC)
		strcat(tmpstr, "(ItemMall-Classic)");
   #elif defined(USE_BLESS_MAX_20)
		strcat(tmpstr, "(ItemMall_CN_Bless)");
   #else
			strcat(tmpstr, "(ItemMall)");
   #endif
  #endif
			//wsprintf(tmpstr+strlen(tmpstr), "(%s %s)", __DATE__, __TIME__);
			wsprintf(tmpstr+strlen(tmpstr), "(SVN:%u)", SVN_VER);
			m_pDialog->SetWindowText(tmpstr);
		}
 #endif
	}
#endif

	if(IsCreating())
		return(true);

	m_IsNPCCountryUpdate = m_IsNPCCountryUpdateFlag;
	m_IsNPCCountryUpdateFlag = 0;
	// ?t???p
	if (nDoSpeedRecord = m_SpeedRecord.GetFrameRatio(&nTick))
	{
		if (nUpdateCount == -1)
		{
			nDoSpeedRecord = 0;
			goto do001;
		}
	//	// ?p?G??t?U???A?n???????
	//	if (nRecFrame > nTick)
	//	{
	//		if (nRecFrame > (nTick+10))
	//			nUpdateCount = 10;
	//	}
		//
		if (nUpdateCount >= 10)
		{
			g_nFrameRatio = nTick;
			if (!g_nFrameRatio)
				g_nFrameRatio++;
			//
do001:		nUpdateCount = 0;
			nRecFrame = nTick;
#ifdef CONSOL_MODE
			//Log("SPEED: Frame = %d", nRecFrame);
			wsprintf(tmpstr, "Map Server %d(Frame=%d, Connect=%d)", m_nMapServerID, nRecFrame, m_nConnectCount);
			SetConsoleTitle(tmpstr);
#else
			if (m_pDialog)
			{
				wsprintf(tmpstr, "%d", nRecFrame);
				m_pDialog->m_Frame.SetWindowText(tmpstr);
			}
#endif
			//
#ifdef USE_GMTOOL_MAPSERVER_SPEED
			if (IsConnectedToServer(m_hLoginServer))
			{
				struct LOGIN_MAP_FRAME_SPEED_NOTIFY nFS;

				nFS.nMapServerID = (unsigned short)GetServer()->m_nMapServerID;
				nFS.nMapServerSubID = (unsigned short)GetServer()->m_nMapServerSubID;
				nFS.nMapFrameSpeed = (unsigned short)nRecFrame;
				SendData(m_hLoginServer, 0, PROTO_LOGIN_MAP_FRAME_SPEED_NOTIFY, (char *)&nFS, sizeof(nFS), 0);
			}
#endif
		}
		else
		{
			nUpdateCount++;
			nDoSpeedRecord = 0;
		}
	}

	m_SpeedRecord.SetElapseTickStart();
	if(CBaseServer::Loop() == false)
		return(false);
	if (nDoSpeedRecord)
	{
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT10)
			nRecT10 = n;
	}

#if 0
	{
		m_SpeedRecord.SetElapseTickStart();
		if(CBaseServer::Loop() == false)		// ?????T???P?B?z
			return(false);
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT10)
			nRecT10 = n;
		//
		Log( "低速: %d(%d)", nRecFrame, m_nConnectCount);
	}
	else
	{
		if(CBaseServer::Loop() == false)		// ?????T???P?B?z
			return(false);
	}

	// ?@????????
#endif
	if(!IsWorldDataReady() || !IsWorldTimeReady())
	{
		if(m_nLoadWorldDataTime > 0)
		{
			if(m_nLoadWorldDataTime > (int)GetLoopTickCount())
				m_nLoadWorldDataTime -= GetLoopTickCount();
			else
				m_nLoadWorldDataTime = 0;
		}

		if(m_nLoadWorldDataTime == 0)
		{
			if(IsConnectedToServer(m_hLoginServer))
			{
				nData = 1;
				if(!IsWorldDataReady())
					SendData(m_hLoginServer, 0, PROTO_LOGIN_GET_MAP_DATA, (char *)&nData, sizeof(nData), 0);
				if(!IsWorldTimeReady())
					SendData(m_hLoginServer, 0, PROTO_LOGIN_GET_MAP_GENERAL_DATA, (char *)&nData, sizeof(nData), 0);
				TowerSlashServer_RequestRankFromLogin();
			}

			m_nLoadWorldDataTime = 10000;
		}
	}

	// Mail Server Loop (only when mail server port is configured)
	if (m_ServerInfo.iniPort_MAIL)
	{
		if (nDoSpeedRecord)
			m_SpeedRecord.SetElapseTickStart();
		MailLoop();
		if (nDoSpeedRecord)
		{
			n = m_SpeedRecord.GetElapseTick();
			if (n > nRecT11)
				nRecT11 = n;
		}
	}

	//if (nDoSpeedRecord && (nRecFrame <= SPEED_REPORT_MIN))
	// !!! ????t???G
	if (nDoSpeedRecord)
	{
		m_SpeedRecord.SetElapseTickStart();
		m_MapManager.Update();
		BotManager::Instance()->Update(::GetTickCount());
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT1)
			nRecT1 = n;

		m_SpeedRecord.SetElapseTickStart();
		OuterObjectProc();
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT2)
			nRecT2 = n;
		m_SpeedRecord.SetElapseTickStart();
		TempObjectProc();
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT3)
			nRecT3 = n;
		m_SpeedRecord.SetElapseTickStart();
		LoginCheckProc();
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT4)
			nRecT4 = n;
		m_SpeedRecord.SetElapseTickStart();
		WorldTimeProc();
#ifdef USE_NEW_PLAYER_MSG_BROADCAST
		std::map<long, CProtocoInfo>& info = ProcSendPlayerMsg();
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT5)
			nRecT5 = n;
		m_SpeedRecord.SetElapseTickStart();
		FlushAllClientSendQueue();
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT_flush)
			nRecT_flush = n;
#else
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT5)
			nRecT5 = n;
#endif

		// ?H????????{??(?????D??L)
#ifndef NO_R61_GAMBLE_LOOP
		m_SpeedRecord.SetElapseTickStart();
		R61_Gamble_Run();
		n = m_SpeedRecord.GetElapseTick();
		if (n > nRecT6)
			nRecT6 = n;
#endif

		if (!m_SpeedRecord.GetCountDownTick())
		{
//			m_SpeedRecord.SetCountDownTick(1000*60*5);	// ?C5????
			m_SpeedRecord.SetCountDownTick(1000*20);	// ?C20s
			//
#if NET_SERVER_DIAG
			{
				struct napiNET_LOOP_DIAG loopDiag;
				long bi, bcnt, bbytes;
				std::vector<CProtocoInfo> top_vec;

				memset(&loopDiag, 0, sizeof(loopDiag));
				loopDiag.map_update_ms = nRecT1;
				loopDiag.outer_ms = nRecT2;
				loopDiag.temp_ms = nRecT3;
				loopDiag.login_check_ms = nRecT4;
				loopDiag.world_time_ms = 0;
				loopDiag.bcast_ms = nRecT5;
				loopDiag.flush_ms = nRecT_flush;
				loopDiag.gamble_ms = nRecT6;
				loopDiag.net_loop_ms = nRecT10;
				loopDiag.mail_ms = nRecT11;
				loopDiag.online_players = (long)m_ClientMap.size();
				loopDiag.connect_count = (long)m_nConnectCount;
#ifdef USE_NEW_PLAYER_MSG_BROADCAST
				bi = 0;
				bcnt = 0;
				bbytes = 0;
				for (std::map<long, CProtocoInfo>::iterator it = info.begin(); it != info.end(); ++it)
				{
					loopDiag.bcast_proto_count++;
					bcnt += it->second.count;
					bbytes += it->second.size * it->second.count;
					top_vec.push_back(it->second);
				}
				loopDiag.bcast_pkt_count = bcnt;
				loopDiag.bcast_bytes = bbytes;
				if (top_vec.size() > 0)
				{
					std::sort(top_vec.begin(), top_vec.end(), compCProtocoInfoCount);
					for (bi = 0; bi < 5 && bi < (long)top_vec.size(); bi++)
					{
						loopDiag.top_proto[bi] = top_vec[bi].protoco;
						loopDiag.top_proto_count[bi] = top_vec[bi].count;
					}
				}
#endif
				napiServer_NetDiagLogSummary(&loopDiag);
			}
#else
				//Log( "???: (%d,%d,%d,%d,%d,%d)(%d,%d)", nRecT1, nRecT2, nRecT3, nRecT4, nRecT5, nRecT6, nRecT10, nRecT11);
#endif

#ifdef USE_NEW_PLAYER_MSG_BROADCAST
				size_t info_size = info.size();

				if(info_size > 0)
				{
					//??????]??p
					std::vector<CProtocoInfo> info_vec;
					long info_size = 0;

					for(std::map<long, CProtocoInfo>::iterator it=info.begin(); it != info.end(); ++it)
					{
						info_vec.push_back(it->second);
						info_size += it->second.size;
					}

					//Log ("网络 %d", info_size / 1024);

					std::sort(info_vec.begin(),info_vec.end(), compCProtocoInfoSize);
					//Log ("网络大小 %d(%d), %d(%d), %d(%d), %d(%d), %d(%d)", info_vec[0].protoco, info_vec[0].size, info_size > 1 ? info_vec[1].protoco : 0, info_size > 1 ? info_vec[1].size : 0, info_size > 2 ? info_vec[2].protoco : 0, info_size > 2 ? info_vec[2].size : 0, info_size > 3 ? info_vec[3].protoco : 0, info_size > 3 ? info_vec[3].size : 0, info_size > 4 ? info_vec[4].protoco : 0, info_size > 4 ? info_vec[4].size : 0);

					std::sort(info_vec.begin(),info_vec.end(), compCProtocoInfoSize);
					//Log ("网络总数 %d(%d), %d(%d), %d(%d), %d(%d), %d(%d)", info_vec[0].protoco, info_vec[0].count, info_size > 1 ? info_vec[1].protoco : 0, info_size > 1 ? info_vec[1].count : 0, info_size > 2 ? info_vec[2].protoco : 0, info_size > 2 ? info_vec[2].count : 0, info_size > 3 ? info_vec[3].protoco : 0, info_size > 3 ? info_vec[3].count : 0, info_size > 4 ? info_vec[4].protoco : 0, info_size > 4 ? info_vec[4].count : 0);
				}
#endif

			/* SPEED ms/20s ??????????*/
			nRecT1 = 0;
			nRecT2 = 0;
			nRecT3 = 0;
			nRecT4 = 0;
			nRecT5 = 0;
			nRecT6 = 0;
			nRecT10 = 0;
			nRecT11 = 0;
			nRecT_flush = 0;
		}
	}
	else
	{
		m_MapManager.Update();
		BotManager::Instance()->Update(::GetTickCount());

		OuterObjectProc();
		TempObjectProc();
		LoginCheckProc();
		WorldTimeProc();
#ifdef USE_NEW_PLAYER_MSG_BROADCAST
		ProcSendPlayerMsg();
		FlushAllClientSendQueue();
#endif

		// ?H????????{??(?????D??L)
#ifndef NO_R61_GAMBLE_LOOP
		R61_Gamble_Run();
#endif
	}

	m_nShipSchedule.ShipScheduleProcess();
	m_nHistoryManager.HistoryProcess();

	TowerSlashServer_Update();
	GetMapSpaceManager()->MapSpace_Loop();
 	GetHistoryManager()->HistoryCMD_Loop();

#ifdef USE_ORGANIZE_MAP_PK
	OrgMapPK_CountDown();
#endif

	// ???n?T??
	//kimLoop();
	pfcLoopProcess();

	ProcSelfBoom();

	// ?P Login ?_?u???_????
	if (!connect_to_login_state)
	{
		if ( IsConnectedToServer( m_hLoginServer ) )
		{
			if (m_nIsLoginAskID)
				connect_to_login_state++;
		}
	}
	else if (connect_to_login_state == 1)
	{
		if ( !IsConnectedToServer( m_hLoginServer ) )
		{
			m_nIsLoginAskID = 0;
			connect_to_login_state++;
			//
			Log( "系统：断开与登录服务器的连接 ......" );
			//
			DisconnectToLoginServer();
		}
	}
	else
	{
		if ( IsConnectedToServer( m_hLoginServer ) )
		{
			if (m_nIsLoginAskID)
			{
				UpdateLoginServerRecord();
				connect_to_login_state--;
				Log( "系统：重新连接到登录服务器，更新登录玩家UID表 ......" );
			}
		}
		else
			m_nIsLoginAskID = 0;
	}

	return(true);
}

void CMapServer::OuterObjectProc(void)
{
	std::map<long, CMapObject *>::iterator	oi;
	CMapObject *	pObject;
//	long			hObject;

	// ?a??~????B?z
	for(oi = m_OuterMap.begin(); oi != m_OuterMap.end(); oi++)
	{
//		hObject	= oi->first;
		pObject = oi->second;
		pObject->OuterUpdate();
	}
	// ????B?z?y????
	if (++nOptSerial_NPC >= DIV_PROCESS_MAGIC_NUM)		// 0, 1, 2, 3
		nOptSerial_NPC = 0;
}

void CMapServer::TempObjectProc(void)
{
	std::map<long, CMapObject *>::iterator			ti;
	std::map<int, CMapPlayer *>::iterator			ci;
	std::map<unsigned long, CMapObject *>::iterator	ui;
	std::vector<long>::iterator						ri;
	std::vector<long>								RemoveList;

	struct LOGIN_LOGOUT_FROM_MAP	LogoutMsg;
	struct MAP_KICK_CLIENT_RESULT	KickMsg;

	CMapObject *	pObject;
	CMapPlayer *	pPlayer;
	long			hObject;
	long			nState;
	CMapNPC *		pNPC;
	long nClientID;
	long force_keep;

	RemoveList.clear();
	for(ti = m_TempMap.begin(); ti != m_TempMap.end(); ti++)
	{
		hObject = ti->first;
		pObject = ti->second;
		nState = pObject->GetStateProc();
		pObject->SetStateProc(CMapObject::STATE_NORMAL);
		switch(nState)
		{
		case CMapObject::STATE_CREATE:
			// ???????
//GetServer()->Log("Insert Outer ...");
			InsertOuterObject(hObject);
			RemoveList.push_back(hObject);
			break;

		case CMapObject::STATE_DELETE:
			// ????R??
			if(!pObject->IsOutMap())
				pObject->LeaveMap();

			if (pObject->IsKindOf(CMapPlayer::CLASS_ID))
			{
				pPlayer = (CMapPlayer *)pObject;
				// ?M???a????w????
				pPlayer->MapCtrl_UnLock(1);
				// ?O?d????B?z
				if (pPlayer->m_nDeleteKeepTime > 0)
				{
					pPlayer->m_nDeleteKeepTime -= GetLoopTickCount();
					if (pPlayer->m_nDeleteKeepTime > 0)
					{
						pObject->SetStateProc(nState);
						continue;
					}
					pPlayer->m_nDeleteKeepTime = 0;
				}
				// ?Y????|???????s?????, ????R??
				if(pPlayer->IsSavingData())
				{
					long ec = pPlayer->GetExitCode();
					if (ec == CMapPlayer::EXIT_CLIENT_DISCONNECTED || ec == CMapPlayer::EXIT_KICK)
						pPlayer->CancelSaveWaitForRemove();
					else
					{
						pObject->SetStateProc(nState);
						continue;
					}
				}

				// 2004/10/26 ?M??????
				//Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );
				pPlayer->FlushCombatPowerRankToLogin();

				Disconnect_ClearAllRecord( pPlayer->GetUID(), 6 );

				// ?]?w????_?u
				nClientID = pPlayer->GetClientID();
				if (nClientID > 0)
					SetClientValid( nClientID, false );

				// ?O???s?u??
				m_nConnectCount--;
#ifndef CONSOL_MODE
				if (m_pDialog)
				{
					char tmpstr[2048];

					wsprintf(tmpstr, "%d", m_nConnectCount);
					m_pDialog->m_Connect.SetWindowText(tmpstr);
				}
#endif
				CharName_Del(pPlayer->GetSaveData()->plrName);

				// ?w?s?????, ?e?X?n?X?? Kick ?T???? LoginServer
				switch(pPlayer->GetExitCode())
				{
				case CMapPlayer::EXIT_LOGOUT:
					LogoutMsg.m_nUID		= pPlayer->GetUID();
					LogoutMsg.m_nAccountID	= pPlayer->GetAccountID();
					LogoutMsg.m_IsNormal	= 1;
					SendData(m_hLoginServer, 0, PROTO_LOGIN_LOGOUT_FROM_MAP, (char *)&LogoutMsg, sizeof(struct LOGIN_LOGOUT_FROM_MAP), 0);
//GetServer()->Log("Client logout");
					break;

				case CMapPlayer::EXIT_CHANGE_KS_SERVER:
#ifdef CROSS_SERVER_SYSTEM
					// ?q?????a?? MapServer
					if (nClientID > 0)
					{
						struct MAP_CHANGE_KS_MAP_SERVER nChangeKSMap;

						memset(&nChangeKSMap, 0, sizeof(nChangeKSMap));
						nChangeKSMap.nMode = 0;			// 0=???KS, 1=?qKS??^??
						nChangeKSMap.nPlayerUID = pPlayer->m_KS_NewPlayerUID;
						memcpyT(&nChangeKSMap.nChangeMapData, pPlayer->GetChangeMapServerData(), sizeof(nChangeKSMap.nChangeMapData));
						::msgSendData(nClientID, 0, PROTO_MAP_CHANGE_KS_MAP_SERVER, (char *)&nChangeKSMap, sizeof(nChangeKSMap), msgSEND_FLAG_COMPRESS);
					//	// ????Client?_?u TimeOut ???
					//	SetClientValid( nClientID, false, 30);
						//
						// ???_????A????e?X?_?u?T??
						::smsDelMessageMonitor(nClientID);
					}
#endif
					break;
				case CMapPlayer::EXIT_KS_SERVER_RETURN:
#ifdef CROSS_SERVER_SYSTEM
					// ?q?????a?? MapServer
					if (nClientID > 0)
					{
						struct MAP_CHANGE_KS_MAP_SERVER nChangeKSMap;

						memset(&nChangeKSMap, 0, sizeof(nChangeKSMap));
						nChangeKSMap.nMode = 1;			// 0=???KS, 1=?qKS??^??
						nChangeKSMap.nPlayerUID = pPlayer->m_KS_NewPlayerUID;
						memcpyT(&nChangeKSMap.nChangeMapData, pPlayer->GetChangeMapServerData(), sizeof(nChangeKSMap.nChangeMapData));
						::msgSendData(nClientID, 0, PROTO_MAP_CHANGE_KS_MAP_SERVER, (char *)&nChangeKSMap, sizeof(nChangeKSMap), msgSEND_FLAG_COMPRESS);
					//	// ????Client?_?u TimeOut ???
					//	SetClientValid( nClientID, false, 30);
						//
						// ???_????A????e?X?_?u?T??
						::smsDelMessageMonitor(nClientID);
						//
						// ?o?? KS?????q?? Login Server ?n?X???a????????
						LogoutMsg.m_nUID		= pPlayer->GetUID();
						LogoutMsg.m_nAccountID	= pPlayer->GetAccountID();
						LogoutMsg.m_IsNormal	= 100;	// 埃魁
						SendData(m_hLoginServer, 0, PROTO_LOGIN_LOGOUT_FROM_MAP, (char *)&LogoutMsg, sizeof(struct LOGIN_LOGOUT_FROM_MAP), 0);
					}
#endif
					break;
				case CMapPlayer::EXIT_CHANGE_SERVER:
					// ?q?????a?? MapServer
					if (nClientID > 0)
					{
						::msgSendData(nClientID, 0, PROTO_MAP_CHANGE_MAP_SERVER, (char *)pPlayer->GetChangeMapServerData(), sizeof(struct MAP_CHANGE_MAP_SERVER), msgSEND_FLAG_COMPRESS);
					//	// ????Client?_?u TimeOut ???
					//	SetClientValid( nClientID, false, 30);
						//
						// ???_????A????e?X?_?u?T??
						::smsDelMessageMonitor(nClientID);
					}
					break;

				case CMapPlayer::EXIT_TO_LOGIN_SERVER:
					if (nClientID > 0)
						::msgSendData(nClientID, pPlayer->m_ReloginCommUID, PROTO_MAP_LOGOUT_TO_LOGIN_RESULT, (char *)&pPlayer->m_ReLoginData, sizeof(struct MAP_LOGOUT_TO_LOGIN_RESULT), msgSEND_FLAG_COMPRESS);
//					GetServer()->Log("Send relogin result to client, account id = %d, menu uid = %d.", pPlayer->m_ReLoginData.nAccountID, pPlayer->m_ReloginCommUID);
					break;

				case CMapPlayer::EXIT_KICK:
					KickMsg.m_nUID		= pPlayer->GetUID();
					KickMsg.m_nResult	= MAP_KICK_CLIENT_RESULT_OK;
					SendData(m_hLoginServer, 0, PROTO_MAP_KICK_CLIENT_RESULT, (char *)&KickMsg, sizeof(struct MAP_KICK_CLIENT_RESULT), 0);
					if (nClientID > 0)
						KickClient(nClientID);
					break;

				case CMapPlayer::EXIT_DATA_ERROR:
					LogoutMsg.m_nUID		= pPlayer->GetUID();
					LogoutMsg.m_nAccountID	= pPlayer->GetAccountID();
					LogoutMsg.m_IsNormal	= 0;
					SendData(m_hLoginServer, 0, PROTO_LOGIN_LOGOUT_FROM_MAP, (char *)&LogoutMsg, sizeof(struct LOGIN_LOGOUT_FROM_MAP), 0);
					if (nClientID > 0)
						KickClient(nClientID);
					break;

				case CMapPlayer::EXIT_CLIENT_DISCONNECTED:
					LogoutMsg.m_nUID		= pPlayer->GetUID();
					LogoutMsg.m_nAccountID	= pPlayer->GetAccountID();
					LogoutMsg.m_IsNormal	= 0;
					SendData(m_hLoginServer, 0, PROTO_LOGIN_LOGOUT_FROM_MAP, (char *)&LogoutMsg, sizeof(struct LOGIN_LOGOUT_FROM_MAP), 0);
					break;
				}
				
				//?????a??H?? --chenyin
#ifdef NEW_PLAYER_STATISTICS
				unsigned short mapCode = pPlayer->GetMapID();
				unsigned short players = this->MinusMapPlayer(mapCode);
 #ifdef _DEBUG
				CMapServer::GetServer()->Log("登出地图服务器玩家%hu 还有%hu玩家",mapCode,players);
 #endif
#endif
				if (nClientID > 0)
				{
					ci = m_ClientMap.find(nClientID);
					if(ci != m_ClientMap.end())
						m_ClientMap.erase(nClientID);
				}
			}
			else if (pObject->IsKindOf(CMapNPC::CLASS_ID))
			{
				pNPC = (CMapNPC *)pObject;
				// ?O?d????B?z
				if (pNPC->GetType() == CMapNPC::TYPE_MERCENARY)
				{
					if (pNPC->m_nDeleteKeepTime > 0)
					{
						pNPC->m_nDeleteKeepTime -= GetLoopTickCount();
						if (pNPC->m_nDeleteKeepTime > 0)
						{
							pObject->SetStateProc(nState);
							continue;
						}
						pNPC->m_nDeleteKeepTime = 0;
					}
				}
			}

			ui = m_UIDMap.find(pObject->GetUID());
			if(ui != m_UIDMap.end())
			{
				if (m_UIDMap[pObject->GetUID()] == pObject)
				{
					m_UIDMap.erase(pObject->GetUID());
				}
				else
					GetServer()->Log("错误：删除对象UID表，但不匹配(%d).", pObject->GetUID());
			}

			RemoveOuterObject(hObject);
			pObject->Release();
			delete pObject;
			m_pMapObject[hObject] = NULL;

			g_nObjectCounter--;
			// LRG: ???? handle ???F??????
			if (m_nFreeHandle == -1)
			{
				m_nFreeHandle = hObject;
			}

			RemoveList.push_back(hObject);
			break;

		case CMapObject::STATE_ENTER_MAP:
			// ????i?J?a??
/*
if (pObject->IsKindOf(CMapNPC::CLASS_ID))
{
	g_nShowLog = 1;
	if (((CMapNPC *)pObject)->GetSaveData()->plrCode == 1309)
		Log("Object state: Object(%s) enter map(%d,%d,%d) ...", ((CMapNPC *)pObject)->GetName(), ((CMapNPC *)pObject)->GetMapCode(), ((CMapNPC *)pObject)->GetPosX(), ((CMapNPC *)pObject)->GetPosY());
	g_nShowLog = 0;
}
*/
			if (pObject->IsKindOf(CMapNPC::CLASS_ID))
			{
				if (((CMapNPC *)pObject)->m_nEnterMapDelay > 0)
				{
					((CMapNPC *)pObject)->m_nEnterMapDelay--;
					pObject->SetStateProc(nState);
					continue;
				}
			}
//Log("Object state: Object(%d) enter map ...", hObject);
			pObject->EnterMap();
			RemoveList.push_back(hObject);
			break;

		case CMapObject::STATE_CHANGE_MAP:		//  Map Server ち传瓜
			// ???????}?a??
			force_keep = 0;
			if(!pObject->IsOutMap())
			{
				pObject->LeaveMap();
				force_keep = 1;			// ?? Leave Map?A?n?j???d?U?A?H?K MapCtrl->Update() ???~
			}
			// ?O?d????B?z
			if(pObject->IsKindOf(CMapPlayer::CLASS_ID))
			{
				pPlayer = (CMapPlayer *)pObject;
				if (pPlayer->m_nDeleteKeepTime > 0)
				{
					pPlayer->m_nDeleteKeepTime -= GetLoopTickCount();
					if ((pPlayer->m_nDeleteKeepTime > 0) || force_keep)
					{
						pObject->SetStateProc(nState);
						continue;
					}
					pPlayer->m_nDeleteKeepTime = 0;
//Log("Object state: Object(%d) leave map ...", hObject);
				}
				else
				{
					if (force_keep)
					{
						pObject->SetStateProc(nState);
						continue;
					}
				}
				//
				struct MAP_CHANGE_MAP	ClientMsg;
				CMapCtrl * pMap;

				pMap = FindMap(pPlayer->GetMapID());
				if (pMap)
				{
					ClientMsg.m_nMapCode	= pPlayer->GetMapID();
					ClientMsg.m_Pos.x		= pPlayer->GetPosX();
					ClientMsg.m_Pos.y		= pPlayer->GetPosY();
					ClientMsg.m_nMapCountry	= CMapServer::GetServer()->GetTownDataByID(pMap->GetID())->ptCountryID;
					ClientMsg.m_nMapMode	= pMap->GetStageData()->gstgMode;
					::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_CHANGE_MAP, (char *)&ClientMsg, sizeof(struct MAP_CHANGE_MAP), 0);
				}
				else
					GetServer()->Log("ERROR: (%d)Self change map but no pointer(%d)", pPlayer->GetUID(), pPlayer->GetMapID());
			}
			RemoveList.push_back(hObject);
			break;

		case CMapObject::STATE_LEAVE_MAP:
			// ???????}?a??
			if(!pObject->IsOutMap())
			{
				pObject->LeaveMap();
			}
			// ?O?d????B?z
			if(pObject->IsKindOf(CMapPlayer::CLASS_ID))
			{
				pPlayer = (CMapPlayer *)pObject;
				if (pPlayer->m_nDeleteKeepTime > 0)
				{
					pPlayer->m_nDeleteKeepTime -= GetLoopTickCount();
					if (pPlayer->m_nDeleteKeepTime > 0)
					{
						pObject->SetStateProc(nState);
						continue;
					}
					pPlayer->m_nDeleteKeepTime = 0;
//Log("Object state: Object(%d) leave map ...", hObject);
				}
			}
			RemoveList.push_back(hObject);
			break;
		default:
			// ??? !!!
			Log("未知对象进程状态: %d", nState);
		case CMapObject::STATE_NORMAL:
			RemoveList.push_back(hObject);
			break;
		}
	}

	for(ri = RemoveList.begin(); ri != RemoveList.end(); ri++)
		m_TempMap.erase(*ri);
}

// ??d?w?w?n?J??????O?_?U??
void CMapServer::LoginCheckProc(void)
{
	static unsigned long time_update_tick = 0;
	std::map<unsigned long, CLoginCheck>::iterator	li;
	std::vector<unsigned long>::iterator				ri;
	std::vector<unsigned long>						RemoveList;
	struct LOGIN_LOGOUT_FROM_MAP					LogoutMsg;
	CLoginCheck *	pLoginCheck;
	unsigned long nTick;

	time_update_tick += CMapServer::GetServer()->GetLoopTickCount();
	if (time_update_tick >= 1000)
	{
		time_update_tick = 0;
		//
		nTick = ::GetTickCount();
		// ??????LoginServer ?i?J?U??B?z
		RemoveList.clear();
		for(li = m_LoginCheckMap.begin(); li != m_LoginCheckMap.end(); li++)
		{
			pLoginCheck = &(li->second);
			if(nTick >= pLoginCheck->m_nCancelTime)
			{
				// ????n?J?U??
				Log(MSG_WARN_LOGIN_TIMEOUT, pLoginCheck->m_nMapCode, li->first, pLoginCheck->m_nCheckCode);

				// ?q?? LoginServer ??X????
				CMapPlayer * pPlayer;
				struct LOGIN_MAP_KICK_PLAYER nReq;

				pPlayer = (CMapPlayer *)FindObjectByUID(li->first, CMapPlayer::CLASS_ID);
				if (pPlayer)
				{
					memset(&nReq, 0, sizeof(nReq));
					//nReq.uid = pPlayer->GetUID();
					nReq.uid = 0;
					nReq.nMinutes = 0;
					nReq.nType = 0;
					msg_strncpy(nReq.m_szName, pPlayer->GetName(), sizeof(nReq.m_szName));
					GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_MAP_KICK_PLAYER, (char *)&nReq, sizeof(nReq), 0);
					GetServer()->Log("错误: 玩家登陆超时(%d,%s)", pPlayer->GetUID(), pPlayer->GetName());
				}

				// ?q?? LoginServer ?????_?u
				LogoutMsg.m_nUID		= li->first;
				LogoutMsg.m_nAccountID	= pLoginCheck->m_nAccountID;
				LogoutMsg.m_IsNormal	= 0;
				SendData(m_hLoginServer, 0, PROTO_LOGIN_LOGOUT_FROM_MAP, (char *)&LogoutMsg, sizeof(struct LOGIN_LOGOUT_FROM_MAP), 0);

				RemoveList.push_back(li->first);

				kimClear(LogoutMsg.m_nUID);
				pfcClearData(LogoutMsg.m_nUID);
			}
		}

		for(ri = RemoveList.begin(); ri != RemoveList.end(); ri++)
			m_LoginCheckMap.erase(*ri);
	}
}

void CMapServer::WorldTimeProc(void)
{
/*	std::map<int, CMapPlayer *>::iterator	i;
	struct MAP_CHANGE_TIME	TimeMsg;
	CMapPlayer *			pPlayer;
	bool					IsSendDayMsg;

	// ??]?P?_
	IsSendDayMsg = false;
	if(((GetWorldTime() / 60 * 60) & 1) == 0)
	{
		if(!IsDay())
		{
			// ?]???
			IsSendDayMsg	= true;
		}
	}
	else
	{
		if(IsDay())
		{
			// ????]
			IsSendDayMsg	= true;
		}
	}
	if(IsSendDayMsg)
	{
		// ********** ?`?N!! ?i??y?????@???e?X??h?????D, ?i??{????o?X ********
		// ?q???? Server ?????a
		TimeMsg.m_nTime	= GetWorldTime();
		for(i = m_ClientMap.begin(); i != m_ClientMap.end(); i++)
		{
			pPlayer = i->second;
			if(!IsObjectDeleted(pPlayer->GetHandle()))
				::msgSendData(i->first, 0, PROTO_MAP_CHANGE_TIME, (char *)&TimeMsg, sizeof(TimeMsg), 0);
		}
	} */
}

void CMapServer::Log( char *szFormat, ... )
{
	long log_count = 0;
	char	szTemp[10240];
	char	szTemp2[10240];
	va_list		vl;

	//if (g_nShowLog)
		csThreadLock(m_csDBMessage);
	//CBaseServer::BaseThreadLock();
	//
	g_szDebug_LastFormat = szFormat;
	//
	struct tm *ptm = ::apiGetTimeStruct(0);
	::wsprintf( szTemp2, "<%04d-%02d-%02d %02d:%02d:%02d> ", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec );

//	strcpy(szTemp2, szFormat);
	va_start( vl, szFormat );
	::vsprintf( szTemp2+strlen(szTemp2), szFormat, vl );
	va_end( vl );

	//::wsprintf( szTemp, "(MAP)%s \n", szTemp2 );
	::wsprintf( szTemp, "%s \n", szTemp2 );

	if (!g_nShowLog)
		goto skip_ui;
	// ??C?h???L?? UI
	if (g_nFrameRatio)
	{
		if (g_nFrameRatio <= NO_LOG_TO_UI_FRAMERATIO)
			goto skip_ui;
	}
	//
#ifdef CONSOL_MODE
	printf( szTemp );
#else
	if(m_pDialog)
	{
	//	log_count = m_pDialog->m_LogList.GetCount();
	//	if(log_count >= 1000)
	//		m_pDialog->m_LogList.DeleteString(log_count - 1);
	//
	//	m_pDialog->m_LogList.InsertString(0, szTemp);
		//
		m_pDialog->m_LogList.InsertString( 0, szTemp );
		log_count = m_pDialog->m_LogList.GetCount();
		if (log_count > 100)
		{
			m_pDialog->m_LogList.DeleteString(log_count - 1);
		}
	}
#endif

skip_ui:
	CBaseServer::Log( szTemp2 );
	//
	//
	g_szDebug_LastFormat = NULL;
	//CBaseServer::BaseThreadUnlock();
	//if (g_nShowLog)
		csThreadUnLock(m_csDBMessage);
}

// ???g?? MFC UI ?? Log
void CMapServer::Log2(char *szFormat, ...)
{
	long log_count = 0;
	char	szTemp[4096];
	char	szTemp2[4096];
	va_list		vl;

	csThreadLock(m_csDBMessage);
	//CBaseServer::BaseThreadLock();
	//
	struct tm *ptm = ::apiGetTimeStruct(0);
	::wsprintf( szTemp2, "<%d-%d-%d %d:%d:%d> ", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec );

	va_start( vl, szFormat );
	::vsprintf( szTemp2+strlen(szTemp2), szFormat, vl );
	va_end( vl );

	::wsprintf( szTemp, "%s \n", szTemp2 );

	CBaseServer::Log( szTemp2 );
	//
	//CBaseServer::BaseThreadUnlock();
	csThreadUnLock(m_csDBMessage);
}

void CMapServer::FileLog(long nLogType, char * szFrom, char * szTo, char * szDescribe, ...)
{
	union uniLOG
	{
		struct DB_SAVE_LOGFILE_RECORD	m_Log;
		struct
		{
			long	m_nLogType;
			char	m_szMsg[4096];
		};
	} LogMsg;

	char		szTemp[4096];
	struct tm *	ptm;
	va_list		vl;
	int			nLen;

	if(!IsConnectedToServer(m_hDBServer))
		return;

	ptm = ::apiGetTimeStruct(0);

	LogMsg.m_nLogType	= nLogType;
	LogMsg.m_szMsg[0]	= 0;

	va_start(vl, szDescribe);
	::vsprintf(szTemp, szDescribe, vl);
	va_end(vl);

	::wsprintf(LogMsg.m_szMsg, "%d, %d/%d/%d %d:%d:%d, %s, %s, %s", nLogType, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, szFrom, szTo, szTemp);

	nLen = sizeof(long) + ::strlen(LogMsg.m_szMsg) + 1;
	SendData(m_hDBServer, 0, PROTO_DB_SAVE_LOGFILE_RECORD, (char *)&LogMsg, nLen, 0);
}

// ?|?q????x???@????Error
void CMapServer::PlayerErrLog(CMapPlayer * pPlayer, char *szFormat, ...)
{
	char	szTemp[4096];
	va_list		vl;

	if (pPlayer)
		pPlayer->PlayerActErrorLogCount();
	//
	va_start( vl, szFormat );
	::vsprintf( szTemp, szFormat, vl );
	va_end( vl );

	Log(szTemp);
}

// ??????T????A??d???
long cbMessageChecker(char *buffer, int len, int id, struct proto_COMM * pc)
{
	// id = 0 ????O??v?s?u?X?h??
	if (!id)
		return(1);
	// ?s?u??o????u???{?i?? GM Tools(GM Tools ?????d)

	CMapServer::GetServer()->Log( "警告：丢弃数据: 原始(%d),大小(%d),ip(%s)", pc->pcProtoco, pc->pcSize, ::napiServer_GetClientIP(id) );
	return(0);
}

// ???D?k?? Protoco?A?p???q????x
void cbErrorProtocoChecker(char *buffer, int len, int id, struct proto_COMM * pc)
{
	CMapPlayer * pPlayer;

	pPlayer = CMapServer::GetServer()->FindPlayerByClientID(id);
	if (pPlayer)
	{
		if (pPlayer->PlayerActErrorLogCount(BOT_REASON_GM_NOTICE_UNKNOWN_PROTOCO))
		{
			CMapServer::GetServer()->Log( "警告: 协议错误(%s,%s,%d)(%d,%d)", pPlayer->GetSaveData()->plrAccount, pPlayer->GetName(), pPlayer->GetUID(), pc->pcProtoco, pc->pcSize );
			CMapServer::GetServer()->KickPlayer(pPlayer);
		}
	}
//	else
//		CMapServer::GetServer()->Log( "WARNING: Error Protoco" );
}

bool CMapServer::InitProtocol(void)
{
	if(!CBaseServer::InitProtocol())
		return(false);

	::smsSetMessageClientIDChecker(cbMessageChecker);
	::smsSetErrorProtocoCallBack(cbErrorProtocoChecker);

	SETMSGFUNC(PROTO_LOGIN_GET_MAP_DATA_RESULT,			CB_GetMapDataResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_GET_MAP_GENERAL_DATA_RESULT,	CB_GetMapGeneralDataResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_BOSS_STATUS_SNAPSHOT_RESULT,	CB_LoginBossStatusSnapshotResult, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_LOGIN_ENTER_MAP,				CB_LoginEnterMap, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_CLIENT_LOGIN,					CB_ClientLogin, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CHECK_PLAYER_UID,				CB_CheckPlayerUID, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_SHOWSELFRESULT,	CB_GetPlayerDataShowSelfResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_SKILL_RESULT,		CB_GetPlayerDataSkillResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_ITEM_RESULT,		CB_GetPlayerDataItemResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_STORAGE_RESULT,	CB_GetPlayerDataStorageResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_MATERIALLIB_RESULT, CB_GetPlayerDataMaterialLibResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_NPC_RESULT,		CB_GetPlayerDataNPCResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_MISSION_RESULT,	CB_GetPlayerDataMissionResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_FRIEND_RESULT,	CB_GetPlayerDataFriendResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_FASHION_RESULT,	CB_GetPlayerDataFashionResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_TIANSHU_RESULT,	CB_GetPlayerDataTianshuResult, PROTO_FROM_SERVER);	// ??????
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_AVATAR_AWAKEN_RESULT,	CB_GetPlayerDataAvatarAwakenResult, PROTO_FROM_SERVER);	// ????????
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_DUNGEON_CLEAR_RESULT,	CB_GetPlayerDataDungeonClearResult, PROTO_FROM_SERVER);	// ????????
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_TOWER_SLASH_RESULT,	CB_GetPlayerDataTowerSlashResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_GET_TOWER_SLASH_RANK_RESULT,	CB_LoginGetTowerSlashRankResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_SYNC_TOWER_SLASH_RANK,			CB_LoginSyncTowerSlashRank, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_DB_GETPLAYERDATA_CNPC_ITEM_SKILL_RESULT,	CB_GetPlayerDataCnpcItemSkillResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_INIT_CLIENT_READY,				CB_InitClientReady, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CLIENT_LOGOUT,					CB_ClientLogout, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_KICK_CLIENT,					CB_KickClient, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_CLIENT_SPEED_CHECK,			CB_ClientSpeedCheck, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_GET_FACE,					CB_GetFace, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_DB_GET_FACE_RESULT,				CB_GetFaceResult, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_LOGIN_TO_MAP,					CB_LoginToMap, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_MAP_TO_LOGIN_RESULT,			CB_MapToLoginResult, PROTO_FROM_SERVER);

#if (defined(USE_RELOGIN) || defined(CROSS_SERVER_SYSTEM))
	SETMSGFUNC(PROTO_MAP_LOGOUT_TO_LOGIN,				CB_MapLogoutToLogin, PROTO_FROM_CLIENT);
#endif
	SETMSGFUNC(PROTO_MAP_LOGOUT_2LOGIN_FROM_MAP_RESULT,	CB_MapLogout2LoginFromMapResult, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_PLAYER_WALK_TO,				CB_PlayerWalkTo, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_PLAYER_ATTACK,					CB_PlayerAttack, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_PLAYER_CAST_MAGIC,				CB_PlayerCastMagic, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_PLAYER_CANCEL_MAGIC,			CB_PlayerCancelMagic, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_PLAYER_RESURRECT,				CB_PlayerResurrect, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_RESURRECT_CONFIRM,				CB_ResurrectConfirm, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_REBIRTH,						CB_PlayerRebirth, PROTO_FROM_CLIENT);

	SETMSGFUNC(PROTO_MAP_SEND_MESSAGE_TO_CLIENT,		CB_SendMessageToClient, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_SEND_MESSAGE_TO_CLIENT_GROUP,	CB_SendMessageToClientGroup, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_SEND_CHAT_MESSAGE_TO_CLIENT,	CB_SendChatMessageToClient, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_SEND_MESSAGE_FROM_CLIENT,		CB_SendMessageFromClient, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_SEND_CHAT_MESSAGE_FROM_CLIENT,	CB_SendChatMessageFromClient, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_GM_COMMAND,					CB_GMCommand, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_DB_SAVE_PLAYERDATA_RESULT,			CB_SavePlayerDataResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_SEND_BROADCAST_MESSAGE,		CB_SendBroadcastMessage, PROTO_FROM_CLIENT);

	SETMSGFUNC(PROTO_LOGIN_ASK_MAP_PLAYER_POS,			CB_AskMapPlayerPos, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_MAP_GOTO_PLAYER_RESULT,		CB_MapGotoPlayerResult, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_LOGIN_ASK_MAPSERVER_ID,			CB_LoginAskMapServerID, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_BROADCAST_ALLMAP_MESSAGE,		CB_MapBossBroadcastAllMapMessage, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_BROADCAST_MAP_MESSAGE,			CB_MapBossBroadcastMapMessage, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_BROADCAST_PLAYER_MESSAGE,		CB_MapBossBroadcastPlayerMessage, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_UPDATE_FORCE_ORGANIZE_TOTAL,	CB_MapUpdateForceOrganizeTotal, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_UPDATE_FORCE_DATA,				CB_MapUpdateForceData, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_LOGIN_UPDATE_PEOPLE_DATA,		CB_LoginUpdatePeopleData, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_UPDATE_ARMY_CHANNEL_DATA_GROUP,	CB_UpdateArmyChannelGroup, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_UPDATE_ARMY_CHANNEL_DATA,		CB_UpdateArmyChannel, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_USE_ITEM_ON_ITEM,				CB_UseItemOnItem, PROTO_FROM_CLIENT);

	// ????????
	SETMSGFUNC(PROTO_MAP_GACHA_DRAW,					CB_GachaDraw, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_AVATAR_TRANSFORM,				CB_AvatarTransform, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_AVATAR_HIDE_APPEARANCE,		CB_AvatarHideAppearance, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_AVATAR_REFINE,					CB_AvatarRefine, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_AVATAR_CARD_ACTIVATE,			CB_AvatarCardActivate, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_AVATAR_AWAKEN_INJECT,			CB_AvatarAwakenInject, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_AVATAR_AWAKEN_UPGRADE,			CB_AvatarAwakenUpgrade, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_AVATAR_AWAKEN_QUERY,			CB_AvatarAwakenQuery, PROTO_FROM_CLIENT);

	// ????
	SETMSGFUNC(PROTO_FASHION_ACTIVATE,					CB_FashionActivate, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_FASHION_SET_DISPLAY,				CB_FashionSetDisplay, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_FASHION_POWERUP,					CB_FashionPowerUp, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_FASHION_SET_ABILITY,				CB_FashionSetAbility, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_FASHION_FURNACE,					CB_FashionFurnace, PROTO_FROM_CLIENT);

	// Trace: ????F Pass Login ?????Client ??
	SETMSGFUNC(PROTO_LOGIN_GET_ARMY_DATA_RESULT,		CB_LoginGetArmyDataResult, PROTO_FROM_SERVER);

	// ?????
	SETMSGFUNC(PROTO_LOGIN_GET_CHATROOM_LIST_RESULT,	CB_LoginGetChatroomListResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_OPEN_CHATROOM_RESULT,		CB_LoginOpenChatroomResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_JOIN_CHATROOM_RESULT,		CB_LoginJoinChatroomResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_QUIT_CHATROOM_RESULT,		CB_LoginQuitChatroomResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_CHATROOM_INVITE,				CB_LoginChatroomInvite, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_LOGIN_JOIN_CHATROOM_NOTIFY,		CB_LoginJoinChatroomNotify, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_QUIT_CHATROOM_NOTIFY,		CB_LoginQuitChatroomNotify, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_GET_CHATROOM_LIST,				CB_ClientGetChatroomList, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_OPEN_CHATROOM,					CB_ClientOpenChatroom, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_JOIN_CHATROOM,					CB_ClientJoinChatroom, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_QUIT_CHATROOM,					CB_ClientQuitChatroom, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CHATROOM_INVITE,				CB_ClientChatroomInvite, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CHATROOM_KICK,					CB_ClientChatroomKick, PROTO_FROM_CLIENT);

	SETMSGFUNC(PROTO_MAP_CANCEL_OPEN_CHATROOM,			CB_ClientCancelOpenChatroom, PROTO_FROM_CLIENT);

	// ????
	SETMSGFUNC(PROTO_LOGIN_JOIN_PARTY_RESULT,			CB_LoginJoinPartyResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_INVITE_BY_PARTY,				CB_LoginInviteByParty, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_JOIN_PARTY_NOTIFY,			CB_LoginJoinPartyNotify, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_QUIT_PARTY_NOTIFY,			CB_LoginQuitPartyNotify, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_JOIN_PARTY,					CB_ClientJoinParty, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_QUIT_PARTY,					CB_ClientQuitParty, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_PARTY_INVITE,					CB_ClientPartyInvite, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_PARTY_KICK,					CB_ClientPartyKick, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_DISMISS_PARTY,					CB_ClientDismissParty, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_PARTY_PLAYER_STATUS,			CB_PartyPlayerStatus, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_PARTY_PLAYER_STATUS_MAP_POS,	CB_PartyPlayerStatusMapPos, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_ASSIGN_PARTY_VICE_LEADER,		CB_AssignPartyViceLeader, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_ASSIGN_PARTY_VICE_LEADER_RESULT, CB_AssignPartyViceLeaderResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_GET_PARTY_INFO_RESULT,		CB_LoginGetPartyInfoResult, PROTO_FROM_SERVER);

	// NPC ????
	SETMSGFUNC(PROTO_MAP_TALK_TO_NPC,					CB_TalkToNPC, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_TALK_TO_NPC_END,				CB_TalkToNPCEnd, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_NPC_SAVE_POS,					CB_NPCSavePos, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_TALK_TO_NPC_NEXT_STEP,			CB_NPCNextStep, PROTO_FROM_CLIENT);

	SETMSGFUNC(PROTO_MAP_Hotel_ResetBornPoint,			CB_NPCHotelSavePos, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_Hotel_TakeRest,				CB_NPCHotelRest, PROTO_FROM_CLIENT);

	// ???_
	SETMSGFUNC(PROTO_VT_ASK_RECORD_RESULT,				CB_NPCVTAskRecordResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_VT_CARD_ASK_RECORD_RESULT,			CB_NPCVTCardAskRecordResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_NPCTALK_PACK_DATA_RESULT,		CB_NPCTalkPackDataResult, PROTO_FROM_SERVER);

#ifdef ItemMode
	SETMSGFUNC(PROTO_MAP_ITEM_MALL_GET_LIST_RESULT,		CB_ItemMallGetListResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_ITEM_MALL_GET_ITEM_RESULT,		CB_ItemMallGetItemResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_ITEM_MALL_RECEIVE_GIFT,		CB_ItemMallReceiveGift, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_ADD_CHAR_SLOT_RESULT,			CB_ItemMallAddCharSlotResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_ADD_MALL_POINT_RESULT,			CB_ItemMallAddMallPointResult, PROTO_FROM_SERVER);
 	SETMSGFUNC(PROTO_MAP_IMODE_SHOP_QUOTA_NOTIFY,		CB_ItemMall_QuotaNotify, PROTO_FROM_SERVER);
#endif

	// ?p?L
	SETMSGFUNC(PROTO_MAP_CNPC_WALK_TO,					CB_CNPCWalkTo, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CNPC_ATTACK,					CB_CNPCAttack, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CNPC_CAST_MAGIC,				CB_CNPCCastMagic, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CNPC_IN_OUT,					CB_CNPCInOut, PROTO_FROM_CLIENT);

	SETMSGFUNC(PROTO_MAP_ASK_CLIENT_NUMBER,				CB_AskClientNumber, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_ASK_CLIENT_NUMBER_SERVER_RESULT,	CB_AskClientNumberServerResult, PROTO_FROM_SERVER);

	// ????
	SETMSGFUNC(PROTO_MAP_COUNTRY_WAR_COUNTDOWN_NOTIFY,	CB_CountryWar_CountdownNotify, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_UPDATE_COUNTRY_WAR_INFO,		CB_CountryWar_InfoUpdate, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_UPDATE_MAP_FORCE_AND_TOWN_DATA,	CB_CountryWar_UpdateForceTownData, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TELEPORT_OTHER_COUNTRY,		CB_CountryWar_TeleportOtherCountry, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_STATUE_BROKEN_MESSAGE,			CB_CountryWar_StatueBrokenMessage, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_LOGIN_UPDATE_TOWN_FORCE_DATA,		CB_LoginUpdateTownForceData, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_CITY_DATA_PACK,				CB_MapCityDataPack, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_SET_REPAYMENT,					CB_MapSetRepayment, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_SET_SOUL_TIME,					CB_MapSetSoulTime, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_LOGIN_UPDATE_STICKET_COUNT,	CB_MapLoginUpdateSTicketCount, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_SET_HISTORYDATA,				CB_MapSetHistoryTimeData, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_WAR_TOWNTAX_ADD,				CB_MapWarTownTaxAdd, PROTO_FROM_SERVER);

	// ???@?n??(?? Login Server ?q??)
//	SETMSGFUNC(PROTO_MAP_DELETE_FRIEND_LIST,			CB_MapDeleteFriendList, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_GM_CALL_PLAYER,				CB_MapGMCallPlayer, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_GM_CALL_PLAYER_RESULT,			CB_MapGMCallPlayerResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRACE_PLAYER_SERVER_RESULT,	CB_MapTraceServerResult, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_LOGIN_SEND_MSG_BY_ID_TO_CLIENT, CB_MapSendMsgToClientByID, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_SEND_MSG_BY_ID_AND_TIME_TO_CLIENT, CB_MapSendMsgToClientByIDAndTime, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_MAP_UPDATE_COUNTRY_WAR_LIST_RESULT, CB_MapUpdateCountryWarListResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_KWAR_LIST_RESULT,				 CB_MapUpdateCWarKListResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_AUTOSORT,						 CB_MapAutoSort, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_AUTOSORTSTORAGE,				 CB_MapAutoSortStorage, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_GET_MATERIALLIB,				 CB_MapGetMaterialLib, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_SAVE_MATERIALLIB,				 CB_MapSaveMaterialLib, PROTO_FROM_CLIENT);

	SETMSGFUNC(PROTO_LOGIN_MAP_UPDATE_AUTO_SO_RESULT,	CB_MapUpdateAutoSO, PROTO_FROM_SERVER);

#ifdef USE_TRADE_PASSWORD
	SETMSGFUNC(PROTO_MAP_TRADE_PSW_ASK_CLIENT_RESULT,	CB_MapTradePswAskClientResult, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_TRADE_PSW_CLIENT_ASK_ANSWER,	CB_MapTradePswClientAskAnswer, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_TRADE_PSW_VERIFY_RESULT,		CB_MapLoginTradePswVerifyResult, PROTO_FROM_SERVER);
#endif

#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	SETMSGFUNC(PROTO_MAP_UPDATE_SPCMODE_TIME,			CB_MapUpdateSPCModeTime, PROTO_FROM_SERVER);
#endif

	SETMSGFUNC(PROTO_MAP_USE_RENAME_ITEM,				CB_MapUseRenameItem, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_DB_PLAYER_RENAME_RESULT,			CB_MapUseRenameItemResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_USE_GET_PLAYER_POS_ITEM,		CB_MapUseGetPlayerPosItem, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_GET_PLAYER_POS_RESULT,		CB_MapUseGetPlayerPosItemResult, PROTO_FROM_SERVER);

	SETMSGFUNC(PROTO_LOGIN_BROADCAST_GET_DROP_ITEM_MESSAGE, CB_BroadcastGetDropItemMessage, PROTO_FROM_SERVER);

#ifdef USE_BIG_VTCARD_ITEM
	SETMSGFUNC(PROTO_LOGIN_WEB_POINT_TO_VTCARD_RESULT,	CB_NPCPointToVTCardResult, PROTO_FROM_SERVER);
#endif
	SETMSGFUNC(PROTO_MAP_PUT_WAWA_ITEM,					CB_MapPutWAWAItem, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CHANGE_WAWA_ITEM,				CB_MapChangeWAWAItem, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_PUT_DEPUTY_ITEM,				CB_MapPutDeputyItem, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_CHANGE_DEPUTY_ITEM,			CB_MapChangeDeputyItem, PROTO_FROM_CLIENT);
#ifdef USE_ARMY_FRIEND_ONLINE_FLAG
	SETMSGFUNC(PROTO_MAP_UPDATE_ONLINE_STATUS,			CB_MapUpdateOnlineStatus, PROTO_FROM_CLIENT);
#endif
	SETMSGFUNC(PROTO_MAP_GET_CWAR_REWARD,				CB_GetWarReward, PROTO_FROM_CLIENT);
	//
	SETMSGFUNC(PROTO_MAP_TRADE_GET_ITEM_LIST,				CB_TradeGetItemList, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_GET_ITEM_LIST_RESULT,			CB_TradeGetItemListResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRADE_GET_ITEM_BY_TYPE_ID,			CB_TradeGetItemByTypeID, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_GET_ITEM_BY_TYPE_ID_RESULT,		CB_TradeGetItemByTypeIDResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRADE_BUY_ITEM,					CB_TradeBuyItem, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_BUY_ITEM_RESULT,					CB_TradeBuyItemResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRADESERVER_BUY_ITEM_NOTIFY,		CB_TradeBuyItemNotify, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRADE_SELL_ITEM,					CB_TradeSellItem, PROTO_FROM_CLIENT);
	//SETMSGFUNC(PROTO_MAP_TRADE_SELL_ITEM_RESULT,			CB_TradeSellItemResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_LOGIN_SELL_ITEM_RESULT,				CB_TradeSellItemResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRADE_GET_TREASURE_BOX_ITEM,		CB_TradeGetTreasureBoxItem, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_GET_TREASURE_BOX_ITEM_RESULT,	CB_TradeGetTreasureBoxItemResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRADE_GET_SELF_SELL_ITEM_LIST,		CB_TradeGetSelfSellItemList, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_GET_SELF_SELL_ITEM_LIST_RESULT,	CB_TradeGetSelfSellItemListResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRADE_CANCEL_SELF_SELL_ITEM,		CB_TradeCancelSelfSellItem, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_LOGIN_CANCEL_SELF_SELL_ITEM_RESULT,	CB_TradeCancelSelfSellItemResult, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_TRADE_ADD_SUB_GOLD,				CB_TradeAddSubGold, PROTO_FROM_CLIENT);
 #if (defined(ITEMMODE_USE_SF_TRADE_SELL) || (defined(USE_NF_SERVER_TRADE) && !defined(ItemMode)))
	SETMSGFUNC(PROTO_MAP_TRADE_UPDATE_ADD_SUB_GOLD,			CB_TradeUpdateAddSubGold, PROTO_FROM_SERVER);
 #endif
	//
#ifdef TW_PVP_MODE
	SETMSGFUNC(PROTO_MAP_ASK_TW_PVP_LIST_RESULT,			CB_AskTWPVPListResult, PROTO_FROM_SERVER);
#endif
	// ???B?????X?\??
	SETMSGFUNC(PROTO_MAP_CLIENT_PACK_DATA,					CB_ClientPackData, PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_LOGIN_TO_MAP_MSG_INFO,				CB_LoginToMapMsgInfo, PROTO_FROM_SERVER);
	//
#ifdef USE_COOL_DOWN_SYSTEM
	SETMSGFUNC(PROTO_DB_UPDATE_COOLDOWN_RESULT,				CB_DBUpdateCoolDownResult, PROTO_FROM_SERVER);
#endif
#ifdef SETUP_FORCE_CONTROL
	SETMSGFUNC(PROTO_MAP_PACK_GET_JN_PLAYER_LIST,			CB_LoginPackGetJNPlayerList, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_EXPEL_FORCE_PLAYER,				CB_ExpelForcePlayer, PROTO_FROM_CLIENT);
#endif
	//
#ifdef CROSS_SERVER_SYSTEM			// ?{???SERVER????
 #ifdef CROSS_SERVER_CWAR
	SETMSGFUNC(PROTO_DB_DATASTATUS,							CB_OnDataStatus, PROTO_FROM_SERVER);
 #endif
#endif
	SETMSGFUNC( PROTO_MAP_GMTOOL_GET_FILE,					CB_GMToolGetFile, PROTO_FROM_SERVER );

	//?Z?N?d
#ifdef USE_EQUIP_CARD
	SETMSGFUNC(PROTO_MAP_SOULCARD_MERGE,					CB_SoulCardMerge, PROTO_FROM_CLIENT);
#endif

	SETMSGFUNC(PROTO_LOGIN_GET_ARMY_DATA_SINGLE_RESULT,		CB_GetArmyDataSingleResult, PROTO_FROM_SERVER);

	// ????N?o??T
#ifdef MAPSPACE_COOLDOWN_INFO
	// ????N?oclient to server
	SETMSGFUNC(PROTO_MAP_DUNGEON_COOLDOWN_FROMCLIENT,		GetDungeonCoolDown,PROTO_FROM_CLIENT);
	SETMSGFUNC(PROTO_MAP_DUNGEON_SWEEP,					CB_MapDungeonSweep,PROTO_FROM_CLIENT);
	//????N?osync from login -- chenyin
	SETMSGFUNC(PROTO_LOGIN_MAP_GET_DUNGEON_COOLDOWN_RESULT,		SyncDungeonCoolDown,PROTO_FROM_SERVER);
#endif

#ifdef CROSS_SERVER_SYSTEM			// ?{???SERVER????
 #ifdef CROSS_SERVER_CWAR
	SETMSGFUNC(PROTO_MAP_UPDATE_KSCWAR_LIST_REALTIME,		CB_MapUpdateCountryKSCWarListRealTime, PROTO_FROM_SERVER);
	SETMSGFUNC(PROTO_MAP_KWAR_LIST_SHOW_RESULT,				CB_MapUpdateCWarKListShowResult, PROTO_FROM_SERVER);
 #endif
#endif

	InitMenuProtocol();
	MailMapServerProtocol();
	TianshuMapServerProtocol();

#if defined USE_FAQ_SYSTEM
	FaqRegisterProtocol();
#endif
	return(true);
}

// ???O???o??????e??m
void CMapServer::GetCapitalTeleportMapPos(long country_id, long * map_code, long * map_x, long * map_y,long jobID)
{
	struct plrDATA_WORLD_FORCE * pForce;
	long x1, y1, x2, y2, x, y;
	long xrange, yrange;
	long map;

	// ?O?_?L??O
	if (!country_id || (country_id == ID_COUNTRY_NONE))
	{
no_force:
#ifdef CROSS_SERVER_CWAR
		if (GetServer()->IsCrossCWarServer())
		{
	no_city:
			map = gameCWAR_KS_DEFAULT_CAPITAL;
		}
		else
#endif
		{
#ifdef NEW_CWAR_TELEPORT
			switch(jobID)
			{
				case jobID_WARLORD:
					map = DEF_NEW_NOOB_START_WARLORD_MAPCODE;
					break;
				case jobID_LEADER:
					map = DEF_NEW_NOOB_START_LEADER_MAPCODE;
					break;
				case jobID_ADVISOR:
					map = DEF_NEW_NOOB_START_ADVISOR_MAPCODE;
					break;
				case jobID_WIZARD:
					map = DEF_NEW_NOOB_START_WIZARD_MAPCODE;
					break;
				case jobID_ASSASSIN:
					map = DEF_NEW_NOOB_START_ASSASSIN_MAPCODE;
					break;
				case jobID_ENGINEER:
					map = DEF_NEW_NOOB_START_ENGINEER_MAPCODE;
					break;
			}
#else
			map = gameGetRandomRange(0, CWAR_NONE_FORCE_TELEPORT_CITY);
			if (map >= CWAR_NONE_FORCE_TELEPORT_CITY)
				map = CWAR_NONE_FORCE_TELEPORT_CITY - 1;
			map = g_CWarTeleport[map];
#endif
		}
		//map = 285;
		//
set_pos:
		//
		switch(map)
		{
		default:
#ifdef CROSS_SERVER_CWAR
			if (map >= gameCWAR_KS_STAGE_BEGIN)
			{
				if (map != gameCWAR_KS_DEFAULT_CAPITAL)
					goto no_city;
			}
			else
#endif
				map = 285;
		case 285:						// ???L
			x1 = 57;
			y1 = 30;
			x2 = 105;
			y2 = 40;
			break;
		case 260:
			x1 = 960; y1 = 10; x2 = 990; y2 = 40;
			break;
		case 262:
			x1 = 960; y1 = 25; x2 = 990; y2 = 40;
			break;
		case 264:
			x1 = 10; y1 = 30; x2 = 40; y2 = 40;
			break;
		case 267:
			x1 = 10; y1 = 25; x2 = 60; y2 = 40;
			break;
		case 274:
			x1 = 40; y1 = 25; x2 = 100; y2 = 40;
			break;
		case 276:
			x1 = 30; y1 = 10; x2 = 70; y2 = 25;
			break;
		case 291:
			x1 = 730; y1 = 15; x2 = 770; y2 = 25;
			break;
		case 293:
			x1 = 980; y1 = 10; x2 = 990; y2 = 40;
			break;
		case 323:
			x1 = 30; y1 = 30; x2 = 70; y2 = 40;
			break;
		case 325:
			x1 = 10; y1 = 10; x2 = 20; y2 = 30;
			break;
		case 327:
			x1 = 10; y1 = 30; x2 = 20; y2 = 40;
			break;
		case 329:
			x1 = 980; y1 = 10; x2 = 990; y2 = 40;
			break;
		case 331:
			x1 = 10; y1 = 10; x2 = 20; y2 = 40;
			break;
		//
		case 1:				// ????		// ????
		case 11:						// ????
		case 21:						// ??~
		//
		case 3701:
		case 3702:
		case 3718:
			if (gameGetRandomRange(0, 100) > 50)
			{
				x1 = 15;
				y1 = 10;
				x2 = 55;
				y2 = 40;
			}
			else
			{
				x1 = 545;
				y1 = 10;
				x2 = 585;
				y2 = 40;
			}
			break;			// ?s?W?????n???o??
		case 2:				// ?j??		// ???w
		case 4:							// ?s??
		case 9:							// ?_??
		case 13:						// ???{
		case 15:						// ????
		case 18:						// ?~??
		case 26:						// ???L
		case 27:						// ???n
		case 28:						// ???
		//
		case 3703:
		case 3704:
		case 3705:
		case 3708:
		case 3710:
		case 3713:
			if (gameGetRandomRange(0, 100) > 50)
			{
				x1 = 15;
				y1 = 10;
				x2 = 55;
				y2 = 40;
			}
			else
			{
				x1 = 505;
				y1 = 10;
				x2 = 545;
				y2 = 40;
			}
			break;
		case 3:				// ????		// ??
		case 5:							// ?{
		case 14:						// ?W?e
		case 16:						// ???
		case 17:						// ????
		case 19:						// ???
		case 20:						// ?Z??
		case 25:						// ??K
		case 35:						// ???d
		case 37:						// ??F
		case 43:						// ???y?R
		case 50:						// ???
		//
		case 3707:
		case 3709:
		case 3711:
		case 3714:
			if (gameGetRandomRange(0, 100) > 50)
			{
				x1 = 1;
				y1 = 1;
				x2 = 50;
				y2 = 15;
			}
			else
			{
				x1 = 400;
				y1 = 1;
				x2 = 445;
				y2 = 15;
			}
			break;
		case 6:				// ?p??		// ???
		case 7:							// ?\??
		case 8:							// ????
		case 22:						// ?c??
		case 23:						// ?s??
		case 24:						// ???F
		case 29:						// ?}?{
		//
		case 10:						// ?D?{
		case 12:						// ?w?w
		case 30:						// ?_??
		case 31:						// ???
		case 33:						// ?|?n
		//
		case gameCWAR_KS_DEFAULT_CAPITAL:
		case 3706:
		case 3712:
		case 3715:
		case 3716:
		case 3717:
			if (gameGetRandomRange(0, 100) > 50)
			{
				x1 = 1;
				y1 = 1;
				x2 = 30;
				y2 = 15;
			}
			else
			{
				x1 = 320;
				y1 = 1;
				x2 = 345;
				y2 = 15;
			}
			break;
#ifdef NEW_CWAR_TELEPORT
		case DEF_NEW_NOOB_START_WARLORD_MAPCODE:
		case DEF_NEW_NOOB_START_LEADER_MAPCODE:
		case DEF_NEW_NOOB_START_ADVISOR_MAPCODE:
		case DEF_NEW_NOOB_START_WIZARD_MAPCODE:
		case DEF_NEW_NOOB_START_ASSASSIN_MAPCODE:
		case DEF_NEW_NOOB_START_ENGINEER_MAPCODE:
			if (gameGetRandomRange(0, 100) > 50)
			{
				x1 = DEF_NEW_NOOB_START_A1X1/64;
				y1 = DEF_NEW_NOOB_START_A1Y1/64;
				x2 = DEF_NEW_NOOB_START_A1X2/64;
				y2 = DEF_NEW_NOOB_START_A1Y2/64;
			}
			else
			{
				x1 = DEF_NEW_NOOB_START_A2X1/64;
				y1 = DEF_NEW_NOOB_START_A2Y1/64;
				x2 = DEF_NEW_NOOB_START_A2X2/64;
				y2 = DEF_NEW_NOOB_START_A2Y2/64;
			}
#endif
		}
		//
		x1 = x1 * gameIconSize;
		y1 = y1 * gameIconSize;
		x2 = x2 * gameIconSize;
		y2 = y2 * gameIconSize;
		//
		x = (x1 + x2) / 2;
		y = (y1 + y2) / 2;
		xrange = x2 - x1 + 1;
		yrange = y2 - y1 + 1;
		//
		x = x + gameGetRandomRange(0, xrange) - (xrange/2);
		y = y + gameGetRandomRange(0, yrange) - (yrange/2);
		//
		*map_x = x;
		*map_y = y;
		*map_code = map;
	}
	else
	{
#ifdef CROSS_SERVER_CWAR
		if (GetServer()->IsCrossCWarServer())
		{
			map = gameCWAR_KS_DEFAULT_CAPITAL;
			switch (country_id)
			{
			case ID_COUNTRY_WEI:
				map = gameCWAR_KS_STAGE_BEGIN + 18;
				break;
			case ID_COUNTRY_SHU:
				map = gameCWAR_KS_STAGE_BEGIN + 1;
				break;
			case ID_COUNTRY_WU:
				map = gameCWAR_KS_STAGE_BEGIN + 2;
				break;
			}
			goto set_pos;

		}
#endif

		pForce = GetForceDataByID(country_id);
		if (!pForce)
			goto no_force;
		if (!pForce->pwfCapital)
			goto no_force;
		map = pForce->pwfCapital;
			goto set_pos;
	}
}

// ???G??^?x?s?I???O?I
bool CMapServer::ChangeSaveMap(CMapPlayer * pPlayer, long map_code, long map_x, long map_y, long force_msg, long emul_war,long copy_uid)
{
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	long country_id;
	char tmpstr[1024];

	if (force_msg)
	{
		strcpy(tmpstr, gameGetResourceName(24256));
		pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
	}
	//
	if (IsWar())
	{
		emul_war = 0;
		//
emul:
		if (IsMapWar(map_code, 1))		//  3 常
		{
			// ?p?G?x?s?I?a???O???O??v???A?????
			pTown = GetTownDataByID(map_code);
			if (pTown == &g_nSafeTownData)
				goto force;
			if (pTown->ptCountryID && (pPlayer->GetSaveData()->plrCountryID != pTown->ptCountryID))
			{
	force:		if (!force_msg)
				{
					strcpy(tmpstr, gameGetResourceName(24256));
					pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
				}
				//
				GetCapitalTeleportMapPos(pPlayer->GetSaveData()->plrCountryID, &map_code, &map_x, &map_y, pPlayer->GetSaveData()->plrJobID);
			}
		}
		else	// ?S???a?I????e
		{
			if (IsSpecialMapWar(map_code, &country_id))
			{
				if (pPlayer->GetSaveData()->plrCountryID != country_id)
					goto force;
			}
		}
		//
		if (emul_war)
			m_IsWar = 0;
	}
	else
	{
		if (emul_war)
		{
			m_IsWar = 1;
			goto emul;
		}
	}
	return(pPlayer->ChangeMap(map_code, map_x, map_y,0,0,copy_uid));		// ???a???|???Q?R??
}

void CMapServer::ResetAllPlayer_SpecCNModeTime()
{
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;
	CMapPlayer * pPlayer;
	struct plrDATA_SAVE * pSave;

	mMapPlayer = &GetClientPlayerMap();
	iter_end = mMapPlayer->end();
	for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
	{
		pPlayer	= iter->second;
		if (pPlayer)
		{
			if (!IsObjectDeleted(pPlayer->GetHandle()))
			{
				if (pPlayer->IsReady())
				{
					pSave = pPlayer->GetSaveData();
					//
#ifdef VietnamMode		// 禫玭⊿Τ絬丁Τ筳ぱ睲埃丁
					if (pSave->plrSPCMode_CN_LastLogoutTime == m_nResetSpecCNModeTime)
					{
						pSave->plrSPCMode_CN_LastLogoutTime += 1;
						pSave->plrSPCMode_CN_OnLineTime = 0;
						pPlayer->m_nSpecModeCN_NotifyTime = GetLoopTime() + SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
						pPlayer->m_nWebIP_Flags &= ~(LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO);
						pPlayer->AutoSaveCharData();
					}
#else
					if (m_nResetSpecCNModeTime > pSave->plrSPCMode_CN_LastLogoutTime)
					{
						pSave->plrSPCMode_CN_LastLogoutTime = GetLoopTime();
						pSave->plrSPCMode_CN_OffLineTime = 0;
						pSave->plrSPCMode_CN_OnLineTime = 0;
						pPlayer->m_nSpecModeCN_NotifyTime = GetLoopTime() + SPECMODE_CN_ONLINE_NOTIFY_TIME_1;
						pPlayer->m_nWebIP_Flags &= ~(LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO);
						pPlayer->AutoSaveCharData();
					}
#endif
				}
			}
		}
	}
}

// ???W: ??????e????? Client(all map, all player)
// ?q?`?? Login ?^???q????
void CMapServer::SendMessageToAllPlayers(long protoco, char * msg, long msg_size, long if_keep)
{
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;
	CMapPlayer * pPlayer;

	mMapPlayer = &GetClientPlayerMap();
	iter_end = mMapPlayer->end();
	for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
	{
		pPlayer	= iter->second;
		if (pPlayer)
		{
			if (!IsObjectDeleted(pPlayer->GetHandle()))
				::msgSendData(pPlayer->GetClientID(), 0, protoco, msg, msg_size, 0);
		}
	}
	//
	if (if_keep)
		kimKeepDataToCheckLogin(0, protoco, msg, msg_size);
}

// ???W: ??????e????? Client(single map, all player)
// ?q?`?? Login ?^???q????
void CMapServer::SendMessageToAllPlayers_Map(long map_code, long protoco, char * msg, long msg_size, long if_keep, unsigned short nCopyUID)
{
	CObjectManager * pObjMgr;
	CMapCtrl * pCtrl;
	std::map<long, CMapObject *>::iterator	iObject;
	CMapPlayer * pPlayer;

	if (map_code)
	{
		pCtrl = CMapServer::GetServer()->FindMap(map_code,nCopyUID);
		if (pCtrl)
		{
			pObjMgr = pCtrl->GetAllPlayerList();
			// ........ ???a??????a .........
			for (iObject = pObjMgr->GetBegin(); iObject != pObjMgr->GetEnd(); iObject++)
			{
				pPlayer	= (CMapPlayer *)iObject->second;
				if (pPlayer)
				{
					if (!IsObjectDeleted(pPlayer->GetHandle()))
						::msgSendData(pPlayer->GetClientID(), 0, protoco, msg, msg_size, 0);
				}
			}
		}
		//
		if (if_keep)
			kimKeepDataToCheckLogin(map_code, protoco, msg, msg_size);
	}
}

// ??????H???y(??O???????Login?q??)
// type = 0 ???x????H???~(?w?B?z?L)
void CMapServer::ChangeAllPlayersForce(long old_country, long country_id, long type)
{
	struct KEEP_CHANGE_FORCE nKeepData;
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;
	CMapPlayer * pPlayer;

	if (old_country == country_id)
		return;
	nKeepData.m_nType = type;
	nKeepData.m_nOldCountryID = old_country;
	nKeepData.m_nCountryID = country_id;
	//
	mMapPlayer = &GetClientPlayerMap();
	iter_end = mMapPlayer->end();
	for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
	{
		pPlayer	= iter->second;
		if (pPlayer)
		{
			if (!pPlayer->IsLoadingData())
			{
				if (!IsObjectDeleted(pPlayer->GetHandle()))
				{
					if (pPlayer->GetSaveData()->plrCountryID == old_country)
					{
						if (!type)
						{
							if (!pPlayer->GetSaveData()->plrOrganizeUID)
								goto do_it;
						}
						else
						{
	do_it:				//	pPlayer->GetSaveData()->plrCountryID = country_id;
							// ?]?t?s??
							pPlayer->ChangeCountryAndUpdateClient((unsigned char)country_id, 0, 2);
						}
					}
				}
			}
			else
				pfcAdd(pPlayer->GetUID(), &nKeepData);
		}
	}
	// ?O??????n?J?????a
	unsigned long uid;
	std::map<unsigned long, CLoginCheck>::iterator	li;

	for(li = m_LoginCheckMap.begin(); li != m_LoginCheckMap.end(); li++)
	{
		uid = li->first;
		if (uid)
		{
			pfcAdd(uid, &nKeepData);
		}
	}
}
// ======================================================
// ?_?u??n?X??A?M?????????????
// ======================================================
// type = 0 ?_?u ?? ?n?X ?? ???n
//		  1 ?????a??(??Server)
//		  2 ???`
//		  3 ???}?l/????
//		  4 ???????? 
//		  5 goto ?P?@?? Map Server ??z???a??
//		  6 ???????M??
void CMapServer::Disconnect_ClearAllRecord( unsigned long uid, long type )
{
	CMapPlayer * pPlayer;
	//
	long country_id, force_flg, emul_war;
	long map_code, map_x, map_y;
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	long force_flg2, need_update_cn_flg;
	//
	CMapCtrl * pCtrl;

	pPlayer = (CMapPlayer *)FindObjectByUID(uid);
	if (!pPlayer)
		return;

	need_update_cn_flg = 1;
	if (type == 6)
	{
		type = 0;		// ???O?? 0?B?z
		need_update_cn_flg = 0;
	}

	if (pPlayer->GetClientID() > 0)
		SendMailClearTmpData(pPlayer->GetClientID());

	if (type == 0)
	{
#if defined USE_DATA_STATUS
		pPlayer->m_charStatus.message = DSMSG_SAVE;
		pPlayer->m_charStatus.wparam = pPlayer->GetUID();
		pPlayer->m_charStatus.lparam = 0;

		SendData(GetDBServer(), 0, PROTO_DB_DATASTATUS, (char *)&pPlayer->m_charStatus, sizeof(DataStatusNotify), 0);
#endif
	}

#if (defined(ORGANIZE_PVP_MODE) || defined(USE_ORGANIZE_MAP_PK))
 #ifdef ORGANIZE_PVP_MODE
	pPlayer->CancelOrgPVP();
 #endif
	if ((type == 0) || (type == 1) || (type == 5))
	{
 #ifdef ORGANIZE_PVP_MODE
		GetHistoryOrgPVPManager()->HiOP_DisconnectProcess(pPlayer);
 #endif
 #ifdef USE_ORGANIZE_MAP_PK
		OrgMapPK_SetLoser(pPlayer);
 #endif
	}
#endif
	//
	if ((type == 0) || (type == 1) || (type == 5))
	{
		// ???v???B?z
		if (pPlayer->m_nCharFlags & CHAR_HISTORY_BATTLE)
		{
			if (CMapServer::GetServer()->m_nHistoryManager.IsVIP_Flag_Player(pPlayer))
			{
				CMapServer::GetServer()->m_nHistoryManager.SetVIP_Flag_Player(pPlayer, 0, 1 | 2);
				// ?|?v?T????t??A?p???O
				if (type == 5)
					::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
			}
		}
		// ???B?B?z
		if (pPlayer->m_nPlayerFlags & PLAYER_FLAGS_WAIT_MERRY)
		{
			CMapPlayer * pDest;

			if (pPlayer->m_nWaitMerryTime)
			{
				pPlayer->ProcWaitMerryResponseFail();
			}
			else	// ?t?@??A??X?D?B??
			{
				pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(pPlayer->m_nMerryPlayerUID);
				if (pDest)
				{
					if (pDest->m_nPlayerFlags & PLAYER_FLAGS_WAIT_MERRY)
					{
						if (pDest->m_nWaitMerryTime && (pDest->m_nMerryPlayerUID == pPlayer->GetUID()))
						{
							pDest->ProcWaitMerryResponseFail();
						}
					}
				}
			}
		}
	}
	//
	emul_war = 0;
	if (type == 3)
	{
//// ???????????
//#ifndef USE_PACKAGE_DATA
		if (!IsWar())		// 瓣驹挡: ぃ墩肚癳
		{
			force_flg = 0;
			// ???]?w???X???A?A??^??
			m_IsWar = 1;
			//
			map_code = pPlayer->GetMapCode();
			if (CMapServer::GetServer()->IsMapWar(map_code, 1))		// ?D???a??A???????q??(?]?t 3 ??????)
			{
				pTown = CMapServer::GetServer()->GetTownDataByID(map_code);
				if (pTown == &g_nSafeTownData)
					goto force01;
	//CMapServer::GetServer()->Log("Player(%s) enter map(%d)(%d) ... ", pPlayer->GetSaveData()->plrName, map_code, pTown->ptCountryID);
				if (pTown->ptCountryID && (pPlayer->GetSaveData()->plrCountryID != pTown->ptCountryID))
				{
	force01:		force_flg++;
					emul_war++;
				}
			}
			else
			{
				if (CMapServer::GetServer()->IsSpecialMapWar(map_code, &country_id))
				{
					if (pPlayer->GetSaveData()->plrCountryID != country_id)
						goto force01;
				}
			}
			//
			m_IsWar = 0;
			if (force_flg)
				goto force;
			//??A???????M??
			memset(&m_rtRankList, 0, sizeof(struct plrDATA_WORLD_COUNTRY_WAR_LIST_REAL_TIME_SEND)*10);
			return;
		}
		else
		{
			pPlayer->GetSaveData()->plrCWar_Honor = 0;
			pPlayer->GetSaveData()->plrCWar_KillCount = 0;
			pPlayer->AutoSaveCharData();
		}
	}
	//
	if ((type == 0) || (type == 1))
	{
		if (uid)
		{
			kimClear(uid);
			pfcClearData(uid);
		}
		//
#ifdef USE_COOL_DOWN_SYSTEM
		if (need_update_cn_flg)	// 6 ???p
			CMapServer::GetServer()->SaveCoolDownData(pPlayer);
#endif
		// ??d??s?~(??w??)
		if (!pPlayer->CheckPlayerNoSave())
		{
			pPlayer->CheckInStorageDupeItem();
		}
		// MapCtrl ???a??????d(?]??????S?R????????)
		if (pPlayer->MapCtrl_IsLock())
		{
			//pCtrl = GetMapManager()->FindMap(pPlayer->GetMapCode());
			pCtrl = pPlayer->MapCtrl_GetMapCtrl();
			if (pCtrl)
			{
			//	if (pPlayer->MapCtrl_GetMapCtrl() == pCtrl)
					pCtrl->DeletePlayerRecord(pPlayer->GetHandle(), uid);
			}
		}
		//
		// ?j?????I?g
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
			if (type == 0)
			{
				pPlayer->SpecModeCN_UpdateTimeRecord(1);
			}
			else
				pPlayer->SpecModeCN_UpdateTimeRecord();
		}
		else
		{
			if (type == 0)
			{
				if (need_update_cn_flg)
					pPlayer->SpecModeCN_UpdateTimeRecord(2);
			}
		}
#endif
	}
	//
	//pPlayer = (CMapPlayer *)FindObjectByUID(uid);
	//if (pPlayer)
	{
		if (type == 3)					// ?p?G?O???}?l
		{
			force_flg = 0;
			force_flg2 = 0;
			map_code = pPlayer->GetMapCode();
			if (!IsMapWar(map_code))	// 獶瓣驹瓜ぃノヴ硄
			{
				// ?S???a?I????e
				if (IsSpecialMapWar(map_code, &country_id))
				{
					if (pPlayer->GetSaveData()->plrCountryID != country_id)
					{
						force_flg++;
						goto force;
					}
				}
			//	// ???(????
			//	pPlayer->ExitShop();
			//	// ?x???w
			//	pPlayer->ClearArmyStorageData();
			//	// xiun : ?j???a???}???
			//	unsigned long sendmsg = pPlayer->GetUID();
			//		// client????q??????e???}?????protocol
			//	::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_FORCE_LEAVE_SHOP_NOTIFY, (char *)&sendmsg, sizeof(sendmsg), 0);

				return;
			}
			else
				force_flg2++;
force:
			pTown = GetTownDataByID(map_code);
			// .....................................
			// ?@?w??????????
			// .....................................
			// ????
			pPlayer->CancelTrade();
			// ???(????
			pPlayer->ExitShop();
			// ?x???w
			pPlayer->ClearArmyStorageData();
			// xiun : ?j???a???}???
			unsigned long sendmsg = pPlayer->GetUID();
				// client????q??????e???}?????protocol
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_FORCE_LEAVE_SHOP_NOTIFY, (char *)&sendmsg, sizeof(sendmsg), 0);
			//xiun : ?q??login???}?x???w
			struct LOGIN_LEAVE_ARMY_STORAGE sendmsg2;

			::memset( &sendmsg2, 0, sizeof(sendmsg2) );

			sendmsg2.playerUID = pPlayer->GetUID();

			::msgSendData(pPlayer->GetClientID(), 0, PROTO_LOGIN_LEAVE_ARMY_STORAGE, (char *)&sendmsg2, sizeof(sendmsg2), 0);
			// NPC????
			if (pPlayer->ExitNPCTalk())
			{
				pPlayer->SendNPCTalkResult(NULL, 0, 0, 0);
			}
			// .....................................
			// ?????(??O)??????????
			// .....................................
			// ?\?u
			if (force_flg)
				goto force2;
			if (force_flg2)							// ?O???a??
			{
				if (pTown == &g_nSafeTownData)		// ?a??s?????b??O?O???d????
					goto force2;
			}
			if (pTown->ptCountryID && (pPlayer->GetSaveData()->plrCountryID != pTown->ptCountryID))
			{
force2:			pPlayer->CancelStall();
				// .....................................
				// ???}?l????a??l??
				// .....................................
				pPlayer->SendMsgToPlayer();
				// ?p?G??e?a???O???O??v???A????x?s?I
				map_code = pPlayer->GetSaveData()->plrSaveMapCode;
				map_x = pPlayer->GetSaveData()->plrSavePosX;
				map_y = pPlayer->GetSaveData()->plrSavePosY;
				ChangeSaveMap(pPlayer, map_code, map_x, map_y, 1, emul_war);
			}
		}
		else
		{
			// Add By R61 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
        
			// ??????i??)
			// ????
			pPlayer->CancelTrade();
			// ????
			// ???
			pPlayer->ExitShop();		// ?o??|???A?s???A???D????? 0
			R61_DisconnectClient(uid);
			// ?\?u
#ifdef USE_OFFLINE_STALL
			{
				OfflineStallEntry * const pOff = CMapServer::GetServer()->m_OfflineStallMgr.Find(pPlayer->GetUID());
				const bool wantOff = pPlayer->WantOfflineStallLogout();
				const unsigned char stallSt = pPlayer->GetStallState();
				if (!pOff && wantOff && stallSt == STALL_STATE_OPERATING)
				{
					pPlayer->SaveCharData();
					pPlayer->SaveItemData();
					/* true = ????????????????????????*/
					(void)CMapServer::GetServer()->m_OfflineStallMgr.RegisterFromHost(pPlayer, true);
				}
			}
#endif
#ifdef USE_OFFLINE_STALL
			if (!CMapServer::GetServer()->m_OfflineStallMgr.Find(pPlayer->GetUID()) && !pPlayer->IsOfflineStallGhost())
#endif
				pPlayer->CancelStall();

			//?x???w
			pPlayer->ClearArmyStorageData();

			// End Add By R61
			// ????
			if ((type == 0) || (type == 1))
			{
				pPlayer->PartyClear();
				pPlayer->CarryNPC_Update(1);
			}
			//
			pPlayer->ExitNPCTalk();
			//
			if (type == 0)
			{
				if (pPlayer->m_nCharFlags & CHAR_FLAG_GM_TRACE)
				{
					pPlayer->m_nCharFlags &= ~CHAR_FLAG_GM_TRACE;
					pPlayer->GetSaveData()->plrPosX = pPlayer->m_nTraceLastPosX;
					pPlayer->GetSaveData()->plrPosY = pPlayer->m_nTraceLastPosY;
				}
			}
		}
	}
}

// ?P Login Server ?_?u??M???@?????
void CMapServer::DisconnectToLoginServer()
{
	CMapPlayer * pPlayer;
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;

	mMapPlayer = &CMapServer::GetServer()->GetClientPlayerMap();
	iter_end = mMapPlayer->end();
	for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
	{
		pPlayer	= iter->second;
		// ?M????????i??) !!!
		// ?M???????q??

		// 2005/02/18 xiun : ???}?x???w
		sdSHOPDATA * pShop = ::gameGetShopPtr( pPlayer->GetShopID() );
		if( pShop ) 
		{
			if( pShop->sdShopType == shopType_Storage ) //??w
			{
				if( pPlayer->GetArmyStorageItem().size() != 0 ) //?x???w
				{
					unsigned long uid = pPlayer->GetUID();
					// client????q??????e???}?????protocol
					::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_FORCE_LEAVE_SHOP_NOTIFY, (char *)&uid, sizeof(uid), 0);
					//
					pPlayer->ClearArmyStorageData();
				}
			}
		}
		//end xiun : ???}?x???w
	}	
}

// ?P Login Server ?_?u??s?u??A?n?O?n?J??
void CMapServer::UpdateLoginServerRecord()
{
	struct LOGIN_UPDATE_MAP_PLAYER nReq;
	CMapObject * pObject;
	CMapPlayer * pPlayer;
	std::map<unsigned long, CMapObject *>::iterator oi = m_UIDMap.begin();
	std::map<unsigned long, CMapObject *>::iterator oi_end = m_UIDMap.end();

	if (IsConnectedToServer(m_hLoginServer))
	{
		memset(&nReq, 0, sizeof(nReq));
		while(oi != oi_end)
		{
			pObject = oi->second;
			if (pObject->IsKindOf(CMapPlayer::CLASS_ID))
			{
				pPlayer = (CMapPlayer *)pObject;
				//
				nReq.m_nUID = pPlayer->GetUID();
				nReq.nAccountID = pPlayer->GetAccountID();
				nReq.nMapCode = pPlayer->GetSaveData()->plrMapCode;
				nReq.lPrivilege = pPlayer->m_nPrivilege;
				nReq.nLevel = pPlayer->GetSaveData()->plrLevel;
				nReq.nSex = pPlayer->GetSaveData()->plrSex;
				nReq.nJob = pPlayer->GetSaveData()->plrJobID;
				nReq.nRebirth = pPlayer->GetSaveData()->plrRebirth;
				nReq.nCountryID = pPlayer->GetSaveData()->plrCountryID;
				nReq.nOffice = pPlayer->GetSaveData()->plrOffice;
				nReq.nOrgUID = pPlayer->GetSaveData()->plrOrganizeUID;
				nReq.nServerSubID = m_nMapServerSubID;
				//
				if (pPlayer->GetSaveData()->plrSoulWID && pPlayer->GetSaveData()->plrSoulWID != 0xFFFF )
				{
					if (pPlayer->GetSaveData()->plrSoulOffice)
						nReq.nOffice = pPlayer->GetSaveData()->plrSoulOffice;
				}
				//
				msg_strncpy(nReq.szCharName, pPlayer->GetSaveData()->plrName, sizeof(nReq.szCharName));
				msg_strncpy(nReq.szAccountName, pPlayer->GetSaveData()->plrAccount, sizeof(nReq.szAccountName));
				msg_strncpy(nReq.szClientIP, ::napiServer_GetClientIP(pPlayer->GetClientID()), sizeof(nReq.szClientIP));
				SendData(m_hLoginServer, 0, PROTO_LOGIN_UPDATE_MAP_PLAYER, (char *)&nReq, sizeof(nReq), 0);
			}
			oi++;
		}
	}
}

void CMapServer::FlushAllClientSendQueue(void)
{
	::napiServer_FlushAllClientsSendQueue();
}

long CMapServer::ConnectClient(int nClientID)
{
	char * ciptr;
	std::string cip;
	unsigned long cnt;

	// check_mode = 1 ??d Client ???]????
 #if !(defined(GBMode) || defined(GBMode_TW))
	CBaseServer::SetClientSafeCheck( nClientID, SERVER_CHECKMODE_SAFE | SERVER_CHECKMODE_INPUT_QUEUE | SERVER_CHECKMODE_SEND_QUEUE | SERVER_CHECKMODE_SEND_ALL );
 #else
	// 2009/06/04
	CBaseServer::SetClientSafeCheck( nClientID, SERVER_CHECKMODE_SAFE | SERVER_CHECKMODE_INPUT_QUEUE | SERVER_CHECKMODE_SEND_QUEUE | SERVER_CHECKMODE_SEND_QUEUE_SIZE | SERVER_CHECKMODE_SEND_ALL );
 #endif
	//
	if (!(CBaseServer::ConnectClient(nClientID)))
		return(0);

	ciptr = ::napiServer_GetClientIP(nClientID);
	cip = ciptr;
	if (m_mConnectIPCount.find(cip) != m_mConnectIPCount.end())
	{
		cnt = m_mConnectIPCount[cip] + 1;
		m_mConnectIPCount[cip] = cnt;
		if (cnt > 200)		// ?O?_?P IP ??h?s?u
		{
			KickClient(nClientID, 0);
			Log2("WARNING: Client %d connected, IP = %s (connect too much)", nClientID, ciptr);
			return(0);
		}
	}
	else
	{
		cnt = 1;
		m_mConnectIPCount[cip] = cnt;
	}
	//
#ifndef NO_DETAIL_LOG
	if (cnt < 10)
	{
		Log("Client %d connected(%d), IP = %s", nClientID, cnt, ciptr);
	}
	else
		Log("Client %d(notice) connected(%d), IP = %s", nClientID, cnt, ciptr);
#endif
	return(1);
}

// Tell LoginServer to drop online character record immediately (do not wait for save/delete delay in TempObjectProc).
static void MapServer_SendLogoutToLogin(CMapServer * pSrv, CMapPlayer * pPlayer, long isNormal)
{
	struct LOGIN_LOGOUT_FROM_MAP LogoutMsg;

	if (!pSrv || !pPlayer || !pSrv->m_hLoginServer)
		return;
	LogoutMsg.m_nUID		= pPlayer->GetUID();
	LogoutMsg.m_nAccountID	= pPlayer->GetAccountID();
	LogoutMsg.m_IsNormal	= isNormal;
	pSrv->SendData(pSrv->m_hLoginServer, 0, PROTO_LOGIN_LOGOUT_FROM_MAP, (char *)&LogoutMsg, sizeof(LogoutMsg), 0);
}

void CMapServer::DisconnectClient(int nClientID)
{
	char * ciptr;
	std::string cip;
	long i, err_type;
	CMapPlayer *	pPlayer;
	//
//	long data_protoco, data_size, data_1, data_2;
	//
	std::map<int, CMapPlayer *>::iterator			ci;

	ciptr = ::napiServer_GetClientIP(nClientID);
	cip = ciptr;
	i = 0;
	if (m_mConnectIPCount.find(cip) != m_mConnectIPCount.end())
	{
		i = m_mConnectIPCount[cip] - 1;
		if (i <= 0)
		{
			m_mConnectIPCount.erase(cip);
		}
		else
		{
			m_mConnectIPCount[cip] = i;
		}
	}

	pPlayer = GetServer()->FindPlayerByClientIDForce(nClientID);

	CBaseServer::DisconnectClient(nClientID);

	if (i < 20)
	{
#ifndef NO_DETAIL_LOG
		Log("Client %d(%d) disconnected.", nClientID, i);
#endif
	}
	else
	{
		if (i < 200)
		{
#ifndef NO_DETAIL_LOG
			Log2("Client %d(%d) disconnected.", nClientID, i);
#endif
		}
		else
		{
			Log2("WARNING: Client %d(%d) disconnected(too much).", nClientID, i);
#ifndef NO_BAN_IP
			CBaseServer::SetAutoBanIP_BanIP((char *)cip.c_str(), 60*5);	// きだ牧
#endif
		}
	}

	if(pPlayer != NULL)
	{
		if (!IsObjectDeleted(pPlayer->GetHandle()))
		{
			// ??d?w?????~?X
			if (i = napiServerMCB_GetSafeErrorCode())
			{
				err_type = 0;
				switch(i)
				{
				case napiMCB_ERROR_DATA:					// 钵盽戈
					err_type = LOGTYPE_SYS_PACKET_CHECKSUM_ERR;
					break;
				case napiMCB_ERROR_PACKET_TOO_MUCH:			// ???`??]?q
					err_type = LOGTYPE_SYS_MASS_PACKET;
					break;
				case napiMCB_ERROR_USER_ABORT:				// ㄏノ浪琩岿粇
					err_type = LOGTYPE_SYS_PROTOCO_ERR;
					break;
				case napiMCB_ERROR_QUEUE_FULL:
					err_type = LOGTYPE_SYS_QUEUE_FULL;		// Queue ?w??
					break;
				case napiMCB_ERROR_SEND_QUEUE_TIMEOUT:
					//Log( "SERIOUS: Account(%s) net serious error, code(%d), IP(%s), player uid(%d)", pPlayer->GetSaveData()->plrAccount, i, ciptr, pPlayer->GetUID() );
					err_type = LOGTYPE_SYS_NO_SEND_TIMEOUT;
#if NET_SERVER_DIAG
					napiServer_NetDiagLogClientSnapshot(nClientID, "DISCONNECT code5");
#endif
					break;
				case napiMCB_ERROR_SEND_QUEUE_EXCEED:
					err_type = LOGTYPE_SYS_SEND_QUEUE_EXCEED;
					break;
				default:
					Log( "SERIOUS: Account(%s) net serious error, code(%d), IP(%s), player uid(%d)", pPlayer->GetSaveData()->plrAccount, i, ciptr, pPlayer->GetUID() );
					break;
				}
				//
				if (err_type)
				{
				//	if (smsServer_GetClientLastQueueData(nClientID, &data_protoco, &data_size, &data_1, &data_2))
				//	{
				//		Log( "SERIOUS: Account(%s) net serious error, code(%d), IP(%s), player uid(%d), (%d,%d,%08x,%08x)", pPlayer->GetSaveData()->plrAccount, i, ciptr, pPlayer->GetUID(), data_protoco, data_size, data_1, data_2 );
				//	}
				//	else
						Log( "SERIOUS: Account(%s) net serious error, code(%d), IP(%s), player uid(%d)", pPlayer->GetSaveData()->plrAccount, i, ciptr, pPlayer->GetUID() );
					SendLogMessage_System(pPlayer, err_type);
				}
			}
			//
			// ?????`?_?u??
			Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );

			// ??????s??, ?R??????
#ifdef USE_OFFLINE_STALL
			if (pPlayer->IsOfflineStallGhost())
			{
				/* keep Login m_mapCharacter (same as normal online); only drop client link */
				pPlayer->SaveCharData();
				pPlayer->SaveItemData();
				pPlayer->m_nBotKickTime = 0;
				pPlayer->SetReady(false);
			}
			else
#endif
			{
			MapServer_SendLogoutToLogin(GetServer(), pPlayer, 0);
			pPlayer->SaveAllData(0, 1);
			pPlayer->m_nBotKickTime = 0;
			pPlayer->SetReady(false);			// By LRG 2005/06/04
			pPlayer->SetExitCode(CMapPlayer::EXIT_CLIENT_DISCONNECTED);
			if (!pPlayer->IsOutMap())
				pPlayer->LeaveMap();
			DeleteObject(pPlayer->GetHandle());
			pPlayer->m_nDeleteKeepTime = 0;
			}
//Log( "%s disconnect !", pPlayer->GetName() );
		}
		else
		{
#ifdef USE_OFFLINE_STALL
			if (!pPlayer->IsOfflineStallGhost())
#endif
			{
			MapServer_SendLogoutToLogin(GetServer(), pPlayer, 0);
			pPlayer->CancelSaveWaitForRemove();
			if (!pPlayer->IsOutMap())
				pPlayer->LeaveMap();
			pPlayer->m_nDeleteKeepTime = 0;
			}
		}
		// bug fix by LRG: ?????? client id ?O??
		// ?_?h???????h?|???? Client ID
		if (pPlayer->IsKindOf(CMapPlayer::CLASS_ID))
		{
			if (pPlayer->GetClientID() > 0)
			{
				ci = m_ClientMap.find(pPlayer->GetClientID());
				if(ci != m_ClientMap.end())
					m_ClientMap.erase(pPlayer->GetClientID());
				pPlayer->SetClientID(-1);
			}
		}
	}
}

// ?t?m ItemUID
unsigned long CMapServer::GenerateItemUID(void)
{
	unsigned long	nUID;

	nUID = m_nItemUID;

	m_nItemUID++;
	if(m_nItemUID > m_nItemUIDMax)
		m_nItemUID = m_nItemUIDMin;

	return(nUID);
}

// ??????~
//bool CMapServer::MakeItem(CMapPlayer * pCreator, long nItemID, long nCount, struct itemDATA_SAVE * pItem)
//{
//	struct itemDATASHOWSELF	Show;
//
//	::memset(&Show, 0, sizeof(struct itemDATASHOWSELF));
//	::gameCreateItem(&Show, nItemID, GenerateItemUID(), nCount, pCreator->GetUID(), pCreator->GetName());
//	::gameServer_ItemSave_MakeShowSelf(&Show, pItem);
//
//	return(true);
//}

// ?j??????G
long CMapServer::CompositeEnchanceItem(struct itemDATA_SAVE * pItem, long composite_id, long player_uid, long super_mode)
{
	struct COMPOSITE_DATA * pComp;
	long val, pos, oval;
#if (defined(USE_BLESS_ENHANCE_JAPAN) || defined(USE_BLESS_ENHANCE_LIMIT))
	long bless_max;
#endif
	long ctimes, ctimes_max;
	long i, type;
	struct itemDATA * pItemData;
	//
	short imod[gameMAX_ITEM_MOD*2];
#ifndef COMPOSITE_NAME_USE_ID
	char tmpstr[5];
#endif

//Log("?j??ID: %d", composite_id);
	pComp = gameGetCompositePtr(composite_id);
	if (!pComp)
		return(COMP_ENH_NO_SET_DATA);
//	if (pComp->cdType & itemTypeItem)		// ?j?????S?? Item ???
//		return(COMP_ENH_NO_SET_DATA);
	// ??d?O?_?O????
	if (val = pComp->cdExtra_Bless)			// 0 ~ val 褐
	{
		oval = 0;
		pos = gameItemComp_FindEnchanceMod(iMOD_TYPE_BLESS, (short *)&pItem->m_Item.itemMod);
		if (pos == -1)
		{
			pos = gameItemComp_FindEnchanceFreeMod((short *)&pItem->m_Item.itemMod);
			if (pos == -1)
				return(COMP_ENH_NO_MOD_SPACE);		// ?S???i?H?O??
			pItem->m_Item.itemMod[pos*2] = iMOD_TYPE_BLESS;
		}
		else
		{
			oval = pItem->m_Item.itemMod[pos*2 + 1];
		}
		// ???M?w?????
		if (super_mode)
		{
			if (HIWORDPTR(val) < LOWORDPTR(val))
				HIWORDPTR(val) = LOWORDPTR(val) - 1;
		}
		//
		val = gameGetMinMaxVal_Max(val, 0) + oval;
#if (defined(USE_BLESS_ENHANCE_JAPAN) || defined(USE_BLESS_ENHANCE_LIMIT))
		MapServer_LoadBlessEnhanceMaxRule();
		// ???????????enhance_max_normal??????????
		bless_max = g_BlessRuleEnhanceMaxNormal;
		//
		if (val > bless_max)
		{
			val = bless_max;
		}
#else
		if (val > 200)
		{
			val = 200;
		}
#endif
		else if (val < -200)
		{
			val = -200;
		}
		pItem->m_Item.itemMod[pos*2 + 1] = (short)val;
	}
	else if (pComp->cdRandomAttr)		// 琌琌睹计妮┦
	{
#ifndef NO_USE_RANDOM_DEC_SOUL_TIME
		i = ::gameServerGetItemSoulTimes(pItem);
		if (!i)
			return(COMP_ENH_NO_EQUIP_TIME);
		if (i == 1)
		{
			if (pItem->m_Item.itemOwnerUID != player_uid)	// 局Τ琌ㄏノ 0
				return(COMP_ENH_NO_EQUIP_TIME);
		}
		if (gameServerDecItemSoulTimes(pItem, 0))
		{
#endif
			struct itemDATASHOWSELF nShowSelf;

			gameServer_Item_MakeShowSelf(pItem, &nShowSelf);
			gameSetCreateItemSuperRandom(2);	// 国家Α
			gameSetCreateItemRandomVal(&nShowSelf);
			gameServer_ItemSave_MakeShowSelf(&nShowSelf, pItem);
			return(COMP_ENH_OK);
#ifndef NO_USE_RANDOM_DEC_SOUL_TIME
		}
		else
			return(COMP_ENH_NO_EQUIP_TIME);
#endif
	}
	else if (pComp->cdAddEquipTimes)	// 琌琌確杆称Ω计
	{
		if (!gameServerGetItemSoulMaxTimes(pItem))
			return(COMP_ENH_NO_MAX_EQUIP_TIME);
		if (gameServerAddItemSoulTimes(pItem))
		{
			return(COMP_ENH_OK);
		}
		else
			return(COMP_ENH_NO_ADD_MAX_TIME);
	}
	else if (pComp->cdClearEquipTimes)	// 琌琌睲埃杆称Ω计
	{
		i = gameServerClearItemSoulTimes(pItem, player_uid);
		if (i == 1)
		{
			return(COMP_ENH_OK);
		}
		else if (i == 2)
		{
			return(COMP_ENH_MUST_SELF_USED);
		}
		else
			return(COMP_ENH_NO_CLEAR_TIME);
	}
#ifdef USE_COMPOSITE_FAIL_BROKEN
	else if (pComp->cdFixBroken)
	{
		SetServerItemBrokenStatus(pItem, 0);
		return(COMP_ENH_OK);
	}
#endif
	else if (pComp->cdDismount)
	{
		// ???|?????o??
		return(COMP_ENH_TIMES_EXCEED);		// ?H?K?????
	}
	else
	{
		pItemData = gameGetItemPtr(pItem->m_Item.itemCode);
		if (!pItemData)
			return(COMP_ENH_NO_SET_DATA);

#if defined USE_EQUIP_GEM
	#if !defined USE_EQUIP_GEM2
		// ?????\?^?O???Z??i??o?????B?z
		if (pItem->m_Item.itemFlags & itemSHOW_FLAG_GEM)
			return(COMP_ENH_TIMES_EXCEED);	// ?H?K?????
	#endif
#endif
		// ??d?O?_?j??????F?W???F
		i = ((pItem->m_Item.itemCompTimes & 0xf0) >> 4);
		if (i > gameMAX_COMPOSITE_TIMES_CMP)
		{
			i = i - 16;
		}
		ctimes_max = pItemData->itemCompTimes + i;
		if (ctimes_max <= 0)
			return(COMP_ENH_TIMES_EXCEED);
		ctimes = pItem->m_Item.itemCompTimes & 0x0f;
		if (ctimes >= ctimes_max)
			return(COMP_ENH_TIMES_EXCEED);
		//
		// ?]?w?j?????
//Log("眏てID: %d", composite_id);
		memcpyT(&imod, &pItem->m_Item.itemMod, sizeof(imod));
		{
			long long imodVal64[gameMAX_ITEM_MOD];
			long long min64, max64, oval64, roll64;

			memcpy(imodVal64, pItem->m_Item.itemModVal64, sizeof(imodVal64));
			gameItemMod_MigrateLegacyVal64(imod, imodVal64);
			for (i=0; i<gameMAX_ITEM_MOD; i++)
			{
				if (type = pComp->cdResultItemMod[i*2])
				{
					if (pComp->cdResultItemModMin[i] != 0 || pComp->cdResultItemModMax[i] != 0)
					{
						min64 = pComp->cdResultItemModMin[i];
						max64 = pComp->cdResultItemModMax[i];
						if (!min64)
							min64 = max64;
					}
					else
					{
						val = pComp->cdResultItemMod[i*2 + 1];
						min64 = (long long)(long)HIWORDPTR(val);
						max64 = (long long)(long)LOWORDPTR(val);
						if (!min64)
							min64 = max64;
					}
					oval64 = 0;
					pos = gameItemComp_FindEnchanceMod(type, (short *)&imod);
					if (pos == -1)
					{
						pos = gameItemComp_FindEnchanceFreeMod((short *)&imod);
						if (pos == -1)
							return(COMP_ENH_NO_MOD_SPACE);
						imod[pos*2] = (short)type;
					}
					else
						oval64 = gameItemMod_GetValue(imod, imodVal64, pos);
					if (super_mode && max64 > min64)
						max64 = max64 - 1;
					if (gameItemMod_IsVal64Type(type))
					{
						roll64 = gameGetMinMaxVal_Max64(min64, max64, 0) + oval64;
						gameItemMod_SetValue(imod, imodVal64, pos, roll64);
					}
					else
					{
						val = pComp->cdResultItemMod[i*2 + 1];
						if (super_mode)
						{
							if (HIWORDPTR(val) < LOWORDPTR(val))
								HIWORDPTR(val) = LOWORDPTR(val) - 1;
						}
						val = gameGetMinMaxVal_Max(val, 0) + (long)oval64;
						if (val > 60000)
							val = 60000;
						else if (val < -60000)
							val = -60000;
						imod[pos*2 + 1] = (short)val;
						if (imodVal64[pos] != 0)
							imodVal64[pos] = (long long)(short)val;
					}
				}
			}
			memcpyT(&pItem->m_Item.itemMod, &imod, sizeof(imod));
			memcpy(pItem->m_Item.itemModVal64, imodVal64, sizeof(imodVal64));
		}
		// ???s?R?W
#ifdef COMPOSITE_NAME_USE_ID
		pItem->m_Item.itemCompNameID = pComp->cdCompNameID;
#else
		msg_strncpy(tmpstr, gameGetResourceName(pComp->cdCompNameID), 5);
		memcpyT(pItem->m_Item.itemCompName, tmpstr, 4);
#endif
		//gameMakeSaveItemName(pItem, buffer);
		// ?W?[????
		ctimes++;
		pItem->m_Item.itemCompTimes = (unsigned char)((pItem->m_Item.itemCompTimes & 0xf0) | ctimes);
	}
	return(COMP_ENH_OK);
}

void CMapServer::CharName_Add(char * name, unsigned long uid)
{
	char tmpstr[512];

	msg_strupr(tmpstr, name, sizeof(tmpstr));
	if (!CharName_FindUID(tmpstr))
	{
		std::string sCharName = tmpstr;

		m_mCharToUID[sCharName] = uid;
	}
}

void CMapServer::CharName_Del(char * name)
{
	char tmpstr[512];

	msg_strupr(tmpstr, name, sizeof(tmpstr));
	std::string sCharName = tmpstr;

	std::map<std::string,unsigned long>::iterator iter_end = m_mCharToUID.end();
	std::map<std::string,unsigned long>::iterator iter = m_mCharToUID.find(sCharName);
	if (iter != iter_end)
	{
		m_mCharToUID.erase(sCharName);
	}
}

unsigned long CMapServer::CharName_FindUID(char * name)
{
	char tmpstr[512];

	msg_strupr(tmpstr, name, sizeof(tmpstr));
	std::string sCharName = tmpstr;

	std::map<std::string,unsigned long>::iterator iter_end = m_mCharToUID.end();
	std::map<std::string,unsigned long>::iterator iter = m_mCharToUID.find(sCharName);
	if (iter != iter_end)
	{
		return(iter->second);
	}
	return(0);
}

/* ???{???X
// ??t?M???????a??
unsigned long CMapServer::CharName_FindFirst()
{
	std::map<std::string,unsigned long>::iterator	iter = m_mCharToUID.begin();
	std::map<std::string,unsigned long>::iterator iter_end = m_mCharToUID.end();

	if (iter != iter_end)
	{
		return(iter->second);
	}
	return(0);
}
*/
//===================================================================================
//
// ????P?????z functions
//
//===================================================================================
// ???a?????
long CMapServer::CreateObject(unsigned short nClassID, long nCode, unsigned long nUID)
{
	CMapObject *	pObject;
	long			hObject;
	bool			IsFound;
	std::map<unsigned long, CMapObject *>::iterator	ui;

//if (nUID == 0x7f000002)
//	nUID = nUID;

	if(m_nFreeHandle == -1)
	{
		Log("WARNING: !!! Cannot create new object because no free handle  !!!");
		return(-1);
	}

	hObject = m_nFreeHandle;
	if(hObject < 0 || hObject >= MAX_SERVER_OBJECTS)
	{
		Log("WARNING: !!! Invalid free handle (%ld), cannot create object class=%u code=%ld uid=%lu !!!",
			hObject, (unsigned)nClassID, nCode, nUID);
		return(-1);
	}
	pObject = NULL;

	switch(nClassID)
	{
	case CMapWarpPoint::CLASS_ID:
		pObject = new CMapWarpPoint;
		break;
/*
	case CMapGate::CLASS_ID:
	//	ui = m_UIDMap.find(nUID);
	//	if (ui != m_UIDMap.end())
	//	{
	//		Log("WARNING: !!! Create gate object already exist(%d)  !!!", nUID);
	//		return(-1);
	//	}
		pObject = new CMapGate;
		break;
*/
	case CMapStatue::CLASS_ID:
		ui = m_UIDMap.find(nUID);
		if (ui != m_UIDMap.end())
		{
			Log("WARNING: !!! Create Statue object already exist(%d)  !!!", nUID);
			return(-1);
		}
		pObject = new CMapStatue;
		break;
	case CMapGate::CLASS_ID:
		ui = m_UIDMap.find(nUID);
		if (ui != m_UIDMap.end())
		{
			Log("WARNING: !!! Create Gate object already exist(%d)  !!!", nUID);
			return(-1);
		}
		pObject = new CMapGate;
		break;
	case CMapNPC::CLASS_ID:
		ui = m_UIDMap.find(nUID);
		if (ui != m_UIDMap.end())
		{
			Log("WARNING: !!! Create NPC object already exist(%d)  !!!", nUID);
			return(-1);
		}
		pObject = new CMapNPC;
		break;

	case CMapPlayer::CLASS_ID:
		ui = m_UIDMap.find(nUID);
		if (ui != m_UIDMap.end())
		{
			Log("WARNING: !!! Create player object already exist(%d)  !!!", nUID);
			return(-1);
		}
		pObject = new CMapPlayer;
		break;

	case CMapShopPoint::CLASS_ID:
		pObject = new CMapShopPoint;
		break;

	case CMapMagic::CLASS_ID:
		pObject = new CMapMagic;
		break;

	default:
		GetServer()->Log("WARNING: !!! Unknow object class-id (%8X)  !!!", nClassID);
		return(-1);
		break;
	}
	//
	if (pObject == NULL)
		return(-1);
	if(pObject->Create(hObject, nCode, nUID))
	{
		m_pMapObject[hObject] = pObject;

		g_nObjectCounter++;
		// ??U?@???? handle
		IsFound = false;
		for(m_nFreeHandle = hObject + 1; m_nFreeHandle < MAX_SERVER_OBJECTS; m_nFreeHandle++)
		{
			if(m_pMapObject[m_nFreeHandle] == NULL)
			{
				IsFound = true;
				break;
			}
		}
		if(!IsFound)
		{
			for(m_nFreeHandle = 0; m_nFreeHandle < hObject; m_nFreeHandle++)
			{
				if(m_pMapObject[m_nFreeHandle] == NULL)
				{
					IsFound = true;
					break;
				}
			}
		}
		if(!IsFound)
		{
			m_nFreeHandle = -1;
			GetServer()->Log("SYSTEM: Object handle is out of range !!!");
		}
	}
	else
	{
		delete pObject;
		pObject	= NULL;
		hObject	= -1;
	}

	if(pObject != NULL)
	{
		pObject->SetStateProc(CMapObject::STATE_NORMAL);

		ChangeObjectState(hObject, CMapObject::STATE_CREATE);
		m_UIDMap[nUID]	= pObject;
	}

	return(hObject);
}

/*
// ?]?w???????? NPC uid, uid = ?s???????L??, old_uid = ??????
long CMapServer::SetNPC_UID(unsigned long uid, unsigned long old_uid)
{
	std::map<unsigned long, CMapObject *>::iterator	ui;
	CMapObject * pObj;
	long r = 0;

	if (uid && old_uid)
	{
		if (uid != old_uid)
		{
			r = 1;
			ui = m_UIDMap.find(uid);
			if (!IsNPCUID(uid) || (ui != m_UIDMap.end()))	// ?s?? uid ?S?H??L
			{
				r = 0;
			}
			ui = m_UIDMap.find(old_uid);
			if (!IsNPCUID(old_uid) || (ui == m_UIDMap.end()))	// ??? uid ?n?s?b
			{
				r = 0;
			}
			//
			if (r)
			{
				pObj = m_UIDMap[old_uid];
				pObj->SetUID(uid);
				m_UIDMap[uid] = pObj;
				m_UIDMap.erase(old_uid);
			}
		}
	}
	return(r);
}

// ?????????? NPC uid
long CMapServer::ChangeNPC_UID(unsigned long uid1, unsigned long uid2)
{
	std::map<unsigned long, CMapObject *>::iterator	ui;
	CMapObject * pObj1;
	CMapObject * pObj2;
	long r = 0;

	if (uid1 && uid2)
	{
		if (uid1 != uid2)
		{
			r = 1;
			ui = m_UIDMap.find(uid1);
			if (!IsNPCUID(uid1) || (ui == m_UIDMap.end()))
			{
				r = 0;
			}
			ui = m_UIDMap.find(uid2);
			if (!IsNPCUID(uid2) || (ui == m_UIDMap.end()))
			{
				r = 0;
			}
			//
			if (r)
			{
				pObj1 = m_UIDMap[uid1];
				pObj2 = m_UIDMap[uid2];
				pObj1->SetUID(uid2);
				pObj2->SetUID(uid1);
				m_UIDMap[uid1] = pObj2;
				m_UIDMap[uid2] = pObj1;
			}
		}
	}
	return(r);
}
*/

// ????e?I????
// set_color_only = ?u?????}????~??e
long CMapServer::CreateWarpPoint(int nMapID, long nTargetX, long nTargetY, long set_color_only, long copy_uid)
{
	CMapWarpPoint *	pWarp;
	long			hWarp;

	hWarp = CreateObject(CMapWarpPoint::CLASS_ID, 1, MAP_WARP_POINT_UID);
	if(hWarp == -1)
		return(-1);

	pWarp = (CMapWarpPoint *)FindObjectByHandle(hWarp);
	pWarp->SetTarget(nMapID, nTargetX, nTargetY, copy_uid);
	pWarp->SetSetColor(set_color_only);

	return(hWarp);
}

// ???????e?I
long CMapServer::CreateShopPoint(int nShopID)
{
	CMapShopPoint *	pShop;
	long			hShop;

	hShop = CreateObject(CMapShopPoint::CLASS_ID, 1, MAP_SHOP_POINT_UID);
	if(hShop == -1)
		return(-1);

	pShop = (CMapShopPoint *)FindObjectByHandle(hShop);
	pShop->SetShopID(nShopID);

	return(hShop);
}

// ??? Statue ??????(?W?h????w uid, type)
long CMapServer::CreateStatue(long nCode, unsigned long nUID)
{
	CMapStatue * pStatue;
	long		hStatue;
//	struct plrDATA_ORIGIN * pOrg;

	hStatue = CreateObject(CMapStatue::CLASS_ID, nCode, nUID);
	if(hStatue == -1)
		return(-1);
	pStatue = (CMapStatue *)FindObjectByHandle(hStatue);
	pStatue->SetType(CMapNPC::TYPE_STATUE);
	//
	pStatue->CalcHPRestoreTime();
	pStatue->CalcMPRestoreTime();
	pStatue->CalcSTRestoreTime();
	//pStatue->GetSaveData()->plrBaseSPCFlag |= spcFLAG_STATUE;
	//pStatue->GetCharData()->plrSPCFlag |= spcFLAG_STATUE;
	//pStatue->GetShowData()->plrStatus |= effFun_STATUE;
	return(hStatue);
}

// ??????????(?@??a???h?@??)
long CMapServer::CreateGate(long nCode, unsigned long nUID)
{
	//return(CreateObject(CMapGate::CLASS_ID, 1, MAP_GATE_UID));
	CMapGate * pGate;
	long		hGate;
//	struct plrDATA_ORIGIN * pOrg;

	hGate = CreateObject(CMapGate::CLASS_ID, nCode, nUID);
	if(hGate == -1)
		return(-1);
	pGate = (CMapGate *)FindObjectByHandle(hGate);
	pGate->SetType(CMapNPC::TYPE_GATE);
	//
	pGate->CalcHPRestoreTime();
	pGate->CalcMPRestoreTime();
	pGate->CalcSTRestoreTime();
	//pGate->GetSaveData()->plrBaseSPCFlag |= spcFLAG_GATE;
	//pGate->GetCharData()->plrSPCFlag |= spcFLAG_GATE;
	//pGate->GetShowData()->plrStatus |= effFun_GATE;
	return(hGate);
}

// ????A?]?w?S???]?w????
void SetNPCSpecialData(CMapNPC * pNPC, long nCode)
{
	struct plrDATA_ORIGIN * pOrg;
	unsigned long long hp_low;

	// ?]?w???????(???B??W??)
	pNPC->m_RebornType = 0;
	pNPC->m_BossRtRank = 0;
	pNPC->m_BOSS_Attack_Spec_Ratio = 0;			// 砰计癸ю竟ю阑诀瞯
	pNPC->m_BOSS_HP_Low = 0;					// 砰硂计癸ю竟ю阑诀瞯
	if (IsEnemyUID(pNPC->GetUID()))
	{
		if (pOrg = gameServerGetCharacterPtr(nCode))	// Τà︹﹚竡
		{
			pNPC->m_RebornType = pOrg->plrSetData.plrRebornType;
			pNPC->m_BossRtRank = pOrg->plrSetData.plrBossRtRank;
			//
			if (pOrg->plrSetData.plrBOSS_HP_Low_Ratio)
			{
				pNPC->m_BOSS_Attack_Spec_Ratio = pOrg->plrSetData.plrBOSS_Attack_Spec_Ratio;
				hp_low = pNPC->GetMaxHP();
				hp_low = hp_low / 100 * pOrg->plrSetData.plrBOSS_HP_Low_Ratio;
				if (hp_low < 0)
					hp_low = 0;
				pNPC->m_BOSS_HP_Low = hp_low;
				//
#ifndef USE_PACKAGE_DATA
				if (pNPC->m_BOSS_HP_Low > pNPC->GetMaxHP())
					CMapServer::GetServer()->Log("ERROR: BOSS hp low(%s,%d)", pNPC->GetName(), pOrg->plrSetData.plrBOSS_HP_Low_Ratio);
#endif
			}
		}
	}
}

// ??? NPC ??????(?W?h????w uid, type)
long CMapServer::CreateNPC(long nCode, unsigned long nUID, int nType)
{
	CMapNPC *	pNPC;
	long		hNPC;
	//struct plrDATA_ORIGIN * pOrg;

	hNPC = CreateObject(CMapNPC::CLASS_ID, nCode, nUID);
	if(hNPC == -1)
		return(-1);

	pNPC = (CMapNPC *)FindObjectByHandle(hNPC);

	//if (pNPC)		// ?????? 0?A???o??????a
	{
		pNPC->SetType(nType);

		SetNPCSpecialData(pNPC, nCode);
	//	// ?]?w???????(???B??W??)
	//	if (IsEnemyUID(nUID))
	//	{
	//		if (pOrg = gameServerGetCharacterPtr(nCode))	// ??????w?q
	//		{
	//			pNPC->m_RebornType = pOrg->plrSetData.plrRebornType;
	//		}
	//	}

		pNPC->CalcHPRestoreTime();
		pNPC->CalcMPRestoreTime();
		pNPC->CalcSTRestoreTime();

		pNPC->KillCount_NPC_Clear();
	}

	return(hNPC);
}

// ?{??????H
CMapNPC * CMapServer::CreateEnemy(long x, long y, long code, long map_id, long is_enter_map, long is_die_delete,long copyUID)
{
	unsigned long uid;
	long		hNPC;
	CMapNPC * pNPC;

	uid = gameServerNPC_Allocate_EnemyUID(gameMAX_PLAYER_UID + 1, gameMAX_ENEMY_UID);
	if(uid == 0)
	{
		GetServer()->Log("WARNING: No UID avail ");
		return(0);
	}
	hNPC = CreateNPC(code, uid, CMapNPC::TYPE_ENEMY);
	if (hNPC == -1)
	{
		// ??? ?n???? UID ?
		gameServerNPC_FreeLast_EnemyUID();
		//
		GetServer()->Log("WARNING: No Handle avail ");
		return(0);
	}
	//
	pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByHandle(hNPC);
	if(pNPC != NULL)
	{
		pNPC->KillCount_NPC_Clear();
		//
		if (is_die_delete)
			pNPC->m_DieDelete = 1;			// ???`??R??
		pNPC->SetMapID((unsigned short)map_id,(unsigned short)copyUID);
		pNPC->SetBornPos(x, y);
		pNPC->SetPatrolPos(x, y);
		pNPC->SetPos(x, y);
	//	ChangeObjectState(hNPC, CMapObject::STATE_ENTER_MAP);
		if (is_enter_map)
		{
			//pNPC->EnterMap();
//		InsertOuterObject(hNPC);
pNPC->SetStateProc(CMapObject::STATE_NORMAL);
			GetServer()->ChangeObjectState(pNPC->GetHandle(), CMapObject::STATE_ENTER_MAP);
		}
	}
	return(pNPC);
}

// ????Y???H?????t?~?@??
void CMapServer::ChangeEnemyCodeData(CMapNPC * pNPC, long x, long y, long code, long map_id, long is_enter_map, long is_die_delete)
{
	struct plrDATA * pData;
	struct plrDATASHOW * pShow;
	struct plrDATA_ORIGIN * pOrg;
	long ai_type = 0;

	if(!pNPC->IsOutMap())
		pNPC->LeaveMap();
	//
	pData = pNPC->GetCharData();
#ifdef SMART_PLR_DATA2
	pData->plrSkillTable = &plrEnemySkillTable;
	pData->plrNPC = &plrEnemyNPC;
#endif
#ifdef DEBUG_SMART_PLR_DATA
	//pData->plrAIPtr = gameGetCharacterAIPtr(ai_type);
	pData->plrOrg = gameServerGetCharacterPtr(code);
	if (pData->plrOrg)
	{
		// 2011/08/23 ??? ?sAI?A?o??????code ?]?w?n
		pData->plrSaveData.plrCode = (unsigned short)code;
		//
		ai_type = pData->plrOrg->plrSetData.plrAI_Type;
		gameServer_SetCharacterAIType(pData, ai_type, 0);
	}
#endif
	//
	::gameServer_NPC_MakeFullData2(code, pNPC->GetUID(), pData, 0xff);
	//
#ifndef DEBUG_SMART_PLR_DATA
	ai_type = pData->plrAI_Type;
#endif
	//
#ifdef NEW_AI_METHOD
	pData->plrOrgAI_Type = (unsigned short)ai_type;
	pData->plrNewAISet.plrUseNewAIFlag &= ~gameNEW_AI_FLAG_PATROL;		// ǖ呸翴砞﹚
	pNPC->ResetNewAIData();
#endif
	//
	pNPC->m_nCharFlags &= ~CHAR_FLAG_SHOWFULL;
	pShow = pNPC->GetShowData();
	if (pShow->plrEquipWeaponR.itemCode || pShow->plrEquipHorse.itemCode)
		pNPC->m_nCharFlags |= CHAR_FLAG_SHOWFULL;
#ifdef NPC_ATOOLS_NAME_BY_ID
	pShow->plrHandle = 0;
#endif
/*	if (pShow->plrCode == BOT_ENEMY_CODE)	// 琩本留┣
	{
		m_nCharFlags |= CHAR_FLAG_BOT_INVISIBLE;
		// ??? code
		pShow->plrCode = gameGetRandomRange(0, 102) + 1;
		pShow->plrWID = BOT_ENEMY_CODE;
		GetSaveData()->plrCode = pShow->plrCode;
		GetCharData()->plrWID = pShow->plrWID;
	} */
	//pNPC->m_IsEscape	= false;
	// ?M????`??????
	//pNPC->ClearHurtRecord();
	// ???i???NPC ?
	pNPC->m_nTalkID = 0;
	pOrg = gameServerGetCharacterPtr(code);
	if (pOrg)
	{
		pNPC->m_nTalkID = pOrg->plrSetData.plrTalk;
		pNPC->GetCharData()->plrTalkID = (unsigned short)pNPC->m_nTalkID;
		pNPC->GetShowData()->plrTalkID = (unsigned short)pNPC->m_nTalkID;
	}
	else
	{
		pNPC->GetCharData()->plrTalkID = 0;
		pNPC->GetShowData()->plrTalkID = 0;
	}
	//
	pNPC->m_nSoulData_ItemID = 0;
	pNPC->m_nBoxData_ItemID = 0;
	//
	SetNPCSpecialData(pNPC, code);
	// ...................
	// ???? reborn ?B?z
	pNPC->ResetRebornData();
	// ...................
	if (is_die_delete)
		pNPC->m_DieDelete = 1;			// ???`??R??
	pNPC->SetMapID((unsigned short)map_id,0);
	pNPC->SetBornPos(x, y);
	pNPC->SetPatrolPos(x, y);
	pNPC->SetPos(x, y);
	if (is_enter_map)
	{
	//	pNPC->EnterMap();
//		InsertOuterObject(pNPC->GetHandle());
	//	if (pNPC->GetStateProc() == CMapObject::STATE_CREATE)
	//	{
	//		InsertOuterObject(pNPC->GetHandle());
	//	}
		pNPC->SetStateProc(CMapObject::STATE_NORMAL);
		GetServer()->ChangeObjectState(pNPC->GetHandle(), CMapObject::STATE_ENTER_MAP);
	}
}

// ?????a??????
long CMapServer::CreatePlayer(int nClientID, unsigned long nPlayerUID)
{
	CMapPlayer *	pPlayer;
	long			hPlayer;

	hPlayer	= CreateObject(CMapPlayer::CLASS_ID, 1, nPlayerUID);
	if(hPlayer == -1)
		return(-1);

	pPlayer = (CMapPlayer *)FindObjectByHandle(hPlayer);
	pPlayer->SetClientID(nClientID);

	m_ClientMap[nClientID]	= pPlayer;

	return(hPlayer);
}

// ????]?k????
long CMapServer::CreateMagic(unsigned long nRange)
{
	CMapMagic *	pMagic;
	long		hMagic;

	hMagic = CreateObject(CMapMagic::CLASS_ID, 1, MAP_MAGIC_UID);
	if(hMagic == -1)
		return(-1);

	pMagic = (CMapMagic *)FindObjectByHandle(hMagic);
	pMagic->SetRange(nRange);



	return(hMagic);
}

void CMapServer::KickPlayer(CMapPlayer * pPlayer)
{
	if (!IsObjectDeleted(pPlayer->GetHandle()))
	{
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("DEBUG: Kick player(%d,%s)", pPlayer->GetUID(), pPlayer->GetName());
#endif
#ifdef USE_OFFLINE_STALL
		if (pPlayer->IsOfflineStallGhost())
		{
			GetServer()->m_OfflineStallMgr.RemoveByHostUID(pPlayer->GetUID());
			pPlayer->SetOfflineStallGhost(false);
			pPlayer->CancelStall();
		}
#endif
		Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );
		//
		pPlayer->SaveAuotSaveData();
		//SetClientValid( pPlayer->GetClientID(), false );
		pPlayer->SetReady(false);
		//
		pPlayer->SetExitCode(CMapPlayer::EXIT_KICK);
		DeleteObject(pPlayer->GetHandle());
		//KickClient(nClientID);
	}
}

// ?R??????
void CMapServer::DeleteObject(long hObject)
{
	CMapObject *	pObject;

	pObject = FindObjectByHandle(hObject);
	if(pObject == NULL)
		return;

	ChangeObjectState(hObject, CMapObject::STATE_DELETE);

	if (pObject->IsKindOf(CMapChar::CLASS_ID))
	{
		((CMapChar *)pObject)->m_nDeleteKeepTime = PLAYER_DELETE_KEEP_TIME;
		//
		if (pObject->IsKindOf(CMapPlayer::CLASS_ID))
		{
			((CMapPlayer *)pObject)->CarryNPC_Update(1);
			if (((CMapPlayer *)pObject)->IsReady())			// By LRG 2005/06/04
				((CMapPlayer *)pObject)->SetReady(false);
			//
			// ?M???a????w????
			((CMapPlayer *)pObject)->MapCtrl_UnLock(1);	// By LRG 2005/06/06
		}
	}
}

// ????O?_?w?g?Q?R??
bool CMapServer::IsObjectDeleted(long hObject)
{
	std::map<long, CMapObject *>::iterator	i;
	CMapObject *	pObject;

/*	if(FindObjectByHandle(hObject) == NULL)
		return(true);

	i = m_TempMap.find(hObject);
	if(i != m_TempMap.end())
	{
		pObject = i->second;
		if(pObject->GetStateProc() == CMapObject::STATE_DELETE)
			return(true);
	}
*/
	pObject = FindObjectByHandle(hObject);
	if (pObject == NULL)
		return(true);
	if (pObject->GetStateProc() == CMapObject::STATE_DELETE)
		return(true);

	return(false);
}

// ??????A?B?z
void CMapServer::ChangeObjectState(long hObject, long nStateProc)
{
	CMapObject *	pObject;

	pObject = FindObjectByHandle(hObject);
	if(pObject == NULL)
		return;

	if (pObject->GetStateProc() != CMapObject::STATE_NORMAL)
	{
//		GetServer()->Log("WARNING: !!! Change object state but not ready !!! current state = %d", pObject->GetStateProc());
		//
		if (pObject->GetStateProc() == CMapObject::STATE_DELETE)
		{
			GetServer()->Log("ERROR: !!! Change object state but object is deleted !!!");
		}
	}

	pObject->SetStateProc(nStateProc);
	m_TempMap[hObject] = pObject;
}

// ?? Handle ???o????????
CMapObject * CMapServer::FindObjectByHandle(long hObject)
{
	if(hObject < 0 || hObject >= MAX_SERVER_OBJECTS)
		return(NULL);

	return(m_pMapObject[hObject]);
}

// ?? UID ???o????????
CMapObject * CMapServer::FindObjectByUIDForce(unsigned long nUID, long class_id)
{
	std::map<unsigned long, CMapObject *>::iterator	i;
	CMapObject *	pObject;

	pObject	= NULL;
	i		= m_UIDMap.find(nUID);
	if(i != m_UIDMap.end())
	{
		pObject = i->second;
		if (class_id)
		{
			if (!pObject->IsKindOf((unsigned short)class_id))
				pObject = NULL;
		}
	}
	return(pObject);
}

// ?? UID ???o????????
CMapObject * CMapServer::FindObjectByUID(unsigned long nUID, long class_id, long * is_delete)
{
	std::map<unsigned long, CMapObject *>::iterator	i;
	CMapObject *	pObject;
	long del_flag = 0;

	pObject	= NULL;
	if (nUID)
	{
		i		= m_UIDMap.find(nUID);
		if(i != m_UIDMap.end())
		{
			pObject = i->second;
			//
			//if (IsObjectDeleted(pObject->GetHandle()))	// By LRG 2005/06/09
			if (pObject->GetStateProc() == CMapObject::STATE_DELETE)
			{
				pObject = NULL;
				del_flag++;
			}
			else
			{
				if (class_id)
				{
					if (!pObject->IsKindOf((unsigned short)class_id))
						pObject = NULL;
				}
			}
		}
	}

	if (is_delete)
		*is_delete = del_flag;

	return(pObject);
}

// ?? ClientID ???o???a????????
CMapPlayer * CMapServer::FindPlayerByClientID(int nClientID, long * is_deleted)
{
	std::map<int, CMapPlayer *>::iterator	i;
	CMapPlayer *	pPlayer;
	long del_flg = 0;

	pPlayer	= NULL;
	i = m_ClientMap.find(nClientID);
	if(i != m_ClientMap.end())
		pPlayer = i->second;

	// ??d???a?O?_?Q?R?? New by LRG
	if (pPlayer)
	{
		//if(IsObjectDeleted(pPlayer->GetHandle()))
		if (pPlayer->GetStateProc() == CMapObject::STATE_DELETE)
		{
			pPlayer = NULL;
			del_flg++;
		}
	}
	if (is_deleted)
		*is_deleted = del_flg;
	return(pPlayer);
}

// ?? ClientID ???o???a???????? ???????O?_?R??
CMapPlayer * CMapServer::FindPlayerByClientIDForce(int nClientID)
{
	std::map<int, CMapPlayer *>::iterator	i;
	CMapPlayer *	pPlayer;

	pPlayer	= NULL;
	i = m_ClientMap.find(nClientID);
	if(i != m_ClientMap.end())
		pPlayer = i->second;
	return(pPlayer);
}

// ?? Handle ???o?????????P????d????O?_?s?b
CMapChar * CMapServer::FindAndCheckCharExist(long hChar, CMapChar ** pCharPtr)
{
	CMapObject *	pObject;
	CMapChar *		pChar;

	if (pCharPtr)
		*pCharPtr = NULL;

	pObject = FindObjectByHandle(hChar);
	if(pObject == NULL)
		return(NULL);

	if(!pObject->IsKindOf(CMapChar::CLASS_ID))
		return(NULL);

	if(IsObjectDeleted(hChar))
		return(NULL);

	pChar = (CMapChar *)pObject;

	if (pCharPtr)
		*pCharPtr = pChar;

	if(pChar->IsOutMap() || pChar->IsDead())
		return(NULL);

	return(pChar);
}

// ?? UID ???o?????????P????d????O?_?s?b
CMapChar * CMapServer::FindAndCheckCharExistByUID(unsigned long nUID, long class_id)
{
	CMapObject *	pObject;
	CMapChar *		pChar;

	if (!class_id)
		class_id = CMapChar::CLASS_ID;
	//
	pObject = FindObjectByUID(nUID, class_id);
	if(pObject == NULL)
		return(NULL);

	//if(!pObject->IsKindOf(CMapChar::CLASS_ID))
	//	return(NULL);
//	if (class_id)
	{
		if (!pObject->IsKindOf((unsigned short)class_id))
		{
//CMapServer::GetServer()->Log("(%d,%08x,%08x) kind not match", nUID, class_id, pObject->GetClassID());
			return(NULL);
		}
	}
//	else
//	{
//		if(!pObject->IsKindOf(CMapChar::CLASS_ID))
//			return(NULL);
//	}

	pChar = (CMapChar *)pObject;
	if(pChar == NULL || pChar->IsOutMap() || pChar->IsDead())
		return(NULL);

	if(IsObjectDeleted(pChar->GetHandle()))
		return(NULL);

	return(pChar);
}

// ?N????[?J?a??~????B?z?{??
void CMapServer::InsertOuterObject(long hObject)
{
	CMapObject *	pObject;

	pObject = FindObjectByHandle(hObject);
	if(pObject != NULL)
	{
	//	if (!pObject->IsOutMap())
			m_OuterMap[hObject] = pObject;
			//
		if (pObject->m_nMapSpaceMode & (1|2))
		{
			GetMapSpaceManager()->MapSpace_InsertOuterObject(pObject);
			GetHistoryManager()->HistoryCMD_InsertOuterObject(pObject);
		}
	}
}

// ?N?????X?a??~????B?z?{??
void CMapServer::RemoveOuterObject(long hObject)
{
	std::map<long, CMapObject *>::iterator	i;

	i = m_OuterMap.find(hObject);
	if(i != m_OuterMap.end())
	{
		CMapObject * pObject;

		pObject = m_OuterMap[hObject];
		if (pObject->m_nMapSpaceMode & (1|2))
		{
			GetMapSpaceManager()->MapSpace_RemoveOuterObject(pObject);
			GetHistoryManager()->HistoryCMD_RemoveOuterObject(pObject);
		}

		m_OuterMap.erase(hObject);
	}
}

//===================================================================================
//
// Callback Functions
//
//===================================================================================

// **************************** MapServer ??l?? ****************************
long CMapServer::CB_GetMapGeneralDataResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_GET_MAP_GENERAL_DATA_RESULT *	pResult = (struct LOGIN_GET_MAP_GENERAL_DATA_RESULT *)pBuffer;
	int i;

	GetServer()->Log("从登陆服务器获取地图常规数据");
	GetServer()->SetLocalBaseTime((unsigned long)::time(NULL));
	GetServer()->SetWorldBaseTime(pResult->nTime);
	GetServer()->m_IsWorldTimeReady = true;
	
	memcpyT(&GetServer()->m_KingStatue, &pResult->KingStatue, sizeof(GetServer()->m_KingStatue)); //戮穨材戈
	for(i=0;i<6;i++)
	{
		GetServer()->m_KingStatueFlag[i]++;
		if(GetServer()->m_KingStatueNPC[i])
			if(pResult->KingStatue[i].pwcwSex == 2)
				GetServer()->m_KingStatueNPC[i]->GetShowData()->plrWID = 1028+i*2+1;
	}


	return(0);
}

// ?p?????`??
void CMapServer::CalcTownTotalData()
{
	long i;
	//unsigned short count;
	struct plrDATA_WORLD_TOWN_DATA * pTown;

	pwfTownTotal_WEI = 0;				// ?????`??
	pwfTownTotal_SHU = 0;
	pwfTownTotal_WU = 0;
	//
	i = 0;
//	for (i=0; i<gameMAX_WORLD_TOWN; i++)
	while(1)
	{
		//pTown = &m_TownData.pwTown[i];
		if (!g_town_id[i])
			break;
		pTown = &m_TownData.pwTown[g_town_id[i]];
//Log("Town data:(%d,%d,%d)", g_town_id[i],pTown->ptDetailID, pTown->ptCountryID);
		i++;
		//
		if (pTown->ptDetailID)			// Τ
		{
			switch(pTown->ptCountryID)
			{
			case ID_COUNTRY_WEI:
				pwfTownTotal_WEI++;
				break;
			case ID_COUNTRY_SHU:
				pwfTownTotal_SHU++;
				break;
			case ID_COUNTRY_WU:
				pwfTownTotal_WU++;
				break;
			}
		}
	}
/*	// ??X??j??
	count = pwfTownTotal_WEI;
	if (pwfTownTotal_WEI > pwfTownTotal_SHU)
	{
		pwfTownMax_CountryID = ID_COUNTRY_WEI;
	}
	else if (pwfTownTotal_WEI < pwfTownTotal_SHU)
	{
		pwfTownMax_CountryID = ID_COUNTRY_SHU;
		count = pwfTownTotal_SHU;
	}
	if (pwfTownTotal_WU > count)
		pwfTownMax_CountryID = ID_COUNTRY_WU;
	*/
	Log("城镇数据:(%d,%d,%d)", pwfTownTotal_WEI, pwfTownTotal_SHU, pwfTownTotal_WU);
}

// ???o????????
long CMapServer::CB_GetMapDataResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	long ui;
	struct LOGIN_GET_MAP_DATA_RESULT *	pResult = (struct LOGIN_GET_MAP_DATA_RESULT *)pBuffer;

	ui = g_nShowLog;
	g_nShowLog = 1;

	GetServer()->Log("从登录服务器获取地图数据");

	::memcpyT(&GetServer()->m_TownData, &pResult->pwTown, sizeof(GetServer()->m_TownData));
	::memcpyT(GetServer()->m_ForceData, pResult->pwForce, sizeof(GetServer()->m_ForceData));
	//
	GetServer()->pwfOrgTotal_WEI = pResult->pwfOrgTotal_WEI;
	GetServer()->pwfOrgTotal_SHU = pResult->pwfOrgTotal_SHU;
	GetServer()->pwfOrgTotal_WU = pResult->pwfOrgTotal_WU;
	//
	GetServer()->pwTotalPlayer_WEI = pResult->pwTotalPlayer_WEI;		// ?Q???`?H??
	GetServer()->pwTotalPlayer_SHU = pResult->pwTotalPlayer_SHU;		// 妇瓣羆计
	GetServer()->pwTotalPlayer_WU = pResult->pwTotalPlayer_WU;			// ?d???`?H??
	GetServer()->pwTotalPlayer_NONE = pResult->pwTotalPlayer_NONE;		// ?L???y?`?H??

	GetServer()->CalcTownTotalData();

	GetServer()->m_IsWorldDataReady = true;

	// ???????NPC ?C???]?w
	GetServer()->SetNPCSetColor();
	// ????????????J?? ???]?w
	GetServer()->GetMapManager()->Statue_Gate_Restore(1);
	//
	GetServer()->m_nCityGuard.CityGuardInit_Create();

	g_nShowLog = ui;

//#ifndef USE_PACKAGE_DATA
	GetServer()->Log("收到城市势力资料(%s,%s) !", GetServer()->m_ForceData[0].pwfName, GetServer()->m_ForceData[1].pwfName);
//#endif

	return(0);
}

void CMapServer::CB_LoginUpdatePeopleData(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_LOGIN_UPDATE_PEOPLE_DATA * pData;

	pData = (struct MAP_LOGIN_UPDATE_PEOPLE_DATA *)pBuffer;
	if (nLen == sizeof(struct MAP_LOGIN_UPDATE_PEOPLE_DATA))
	{
		GetServer()->pwTotalPlayer_WEI = pData->pwTotalPlayer_WEI;		// ?Q???`?H??
		GetServer()->pwTotalPlayer_SHU = pData->pwTotalPlayer_SHU;		// 妇瓣羆计
		GetServer()->pwTotalPlayer_WU = pData->pwTotalPlayer_WU;		// ?d???`?H??
		GetServer()->pwTotalPlayer_NONE = pData->pwTotalPlayer_NONE;	// ?L???y?`?H??
		//
//GetServer()->Log( "World Force Number: WEI = %d, SHU = %d, WU = %d, None = %d", pData->pwTotalPlayer_WEI, pData->pwTotalPlayer_SHU, pData->pwTotalPlayer_WU, pData->pwTotalPlayer_NONE);
	}
}

// ?????y??A??d?H??O?_???\
// ??h : ??? <= 1.2 : 1
// return: 1 ?i?H?????y
long CMapServer::CheckForcePeople(long country_id)
{
	unsigned long vmax, vmin;
	unsigned long max_num;
	long min_id;

	switch(country_id)
	{
	case ID_COUNTRY_WEI:
		vmax = pwTotalPlayer_WEI;
		break;
	case ID_COUNTRY_SHU:
		vmax = pwTotalPlayer_SHU;
		break;
	case ID_COUNTRY_WU:
		vmax = pwTotalPlayer_WU;
		break;
	default:
		return(1);			// ?L??????
		break;
	}
	// ?H??O?_??F??p?]?w
	if (vmax <= 100)
		return(1);
	// ???p
	if (pwTotalPlayer_WEI <= pwTotalPlayer_SHU)
	{
		vmin = pwTotalPlayer_WEI;
		min_id = ID_COUNTRY_WEI;
	}
	else
	{
		vmin = pwTotalPlayer_SHU;
		min_id = ID_COUNTRY_SHU;
	}
	if (vmin > pwTotalPlayer_WU)
	{
		vmin = pwTotalPlayer_WU;
		min_id = ID_COUNTRY_WU;
	}
	if (country_id == min_id)	// ?O???H???
		return(1);
/*	// ???j
	if (pwTotalPlayer_WEI >= pwTotalPlayer_SHU)
	{
		vmax = pwTotalPlayer_WEI;
	}
	else
		vmax = pwTotalPlayer_SHU;
	if (vmax < pwTotalPlayer_WU)
		vmax = pwTotalPlayer_WU;
*/
	// ????O?H????O?_??F
	if (!vmin) vmin++;
	//max_num = vmin + (vmin >> 1);
	max_num = vmin + (vmin * 20 / 100);
	if (vmax > max_num)
		return(0);
	return(1);
}

// ?????y??A??d?O?_?K?O
// return: 1 ?K?O
long CMapServer::CheckForceForFree(long country_id)
{
	switch(country_id)
	{
	case ID_COUNTRY_WEI:
		if (pwfTownTotal_WEI <= gameTOWN_LOW_GOOD_NUMBER)
			return(1);
		break;
	case ID_COUNTRY_SHU:
		if (pwfTownTotal_SHU <= gameTOWN_LOW_GOOD_NUMBER)
			return(1);
		break;
	case ID_COUNTRY_WU:
		if (pwfTownTotal_WU <= gameTOWN_LOW_GOOD_NUMBER)
			return(1);
		break;
	}
	return(0);
}
// **************************** ?n?J?{?? Login To Map ****************************
// Map Server ?? Client ?????P Map Server ??A?? Login ?q??
// ?]?t??????
//
// ?? Login Server ?q?????a???L Map Server ?i?J?? Map Server
long CMapServer::CB_LoginToMap(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	std::map<unsigned long, CLoginCheck>::iterator	i;

	struct MAP_LOGIN_TO_MAP_RESULT	Res;
	struct MAP_LOGIN_TO_MAP *		pCheck = (struct MAP_LOGIN_TO_MAP *)pBuffer;
	CLoginCheck *					pLoginCheck;
	long is_delete;

	// LoginServer ?q?? MapServer ?????a?N?i?J
	GetServer()->Log("登陆: 玩家(%d)到地图(%d). (%d, %d)", pCheck->m_nUID, pCheck->m_nMapCode, pCheck->nAccountID, pCheck->m_nCheckCode);

	CMapPlayer * pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pCheck->m_nUID, CMapPlayer::CLASS_ID, &is_delete);
	if ((pPlayer != NULL) || is_delete)
	{
		GetServer()->Log("Warning: ENTER_MAP: ???(%d) will enter map but already in map. AccountID = %d, CheckCode = %d", pCheck->m_nUID, pCheck->nAccountID, pCheck->m_nCheckCode);
		//
		Res.nRet			= MAP_LOGIN_RESULT_ERROR;
		goto err001;
	}

	if (!GetServer()->GetMapManager()->FindMap(pCheck->m_nMapCode))
	{
		GetServer()->Log("警告:玩家(%d)进入不存在的地图(%d)", pCheck->m_nUID, pCheck->m_nMapCode);
		Res.nRet = MAP_LOGIN_RESULT_ERROR_MAPCODE;
		goto err001;
	}

	// ??d?s?u??O?_?w??
	if(GetServer()->m_ServerInfo.iniMaxConnect <= (long)(GetServer()->m_ClientMap.size() + GetServer()->m_LoginCheckMap.size()))
	{
		Res.nRet			= MAP_LOGIN_RESULT_CONNECT_FULL;
		// ?q?? LoginServer ?s?u?w?????a???i?A?n?J
err001:	Res.m_nUID			= pCheck->m_nUID;
		Res.nAccountID		= pCheck->nAccountID;
		Res.m_nCheckCode	= pCheck->m_nCheckCode;
		Res.m_nMapCode		= pCheck->m_nMapCode;
		GetServer()->SendData(GetServer()->m_hLoginServer, pComm->pcUID, PROTO_MAP_LOGIN_TO_MAP_RESULT, (char *)&Res, sizeof(struct MAP_LOGIN_TO_MAP_RESULT), 0);
		return(0);
	}

	// ?????n?J???a
	i = GetServer()->m_LoginCheckMap.find(pCheck->m_nUID);
	if(i != GetServer()->m_LoginCheckMap.end())
		GetServer()->m_LoginCheckMap.erase(pCheck->m_nUID);

	pLoginCheck = &(GetServer()->m_LoginCheckMap[pCheck->m_nUID]);
	pLoginCheck->m_nCheckCode	= pCheck->m_nCheckCode;
	pLoginCheck->m_nAccountID	= pCheck->nAccountID;
	pLoginCheck->m_nCancelTime	= ::GetTickCount() + CLoginCheck::CANCEL_TIME_LONG;
	pLoginCheck->m_nPrivilege	= pCheck->nPrivilege;
	pLoginCheck->m_nMapFlags = pCheck->nMapFlags;
	pLoginCheck->m_nIsFirstFromLogin = 0;
	pLoginCheck->m_nMapCode = pCheck->m_nMapCode;
	//
	pLoginCheck->m_nBotCheckTimes = 0;
	pLoginCheck->m_nBotCheckType = 0;

	// ?^??T???? LoginServer
	Res.m_nUID			= pCheck->m_nUID;
	Res.nAccountID		= pCheck->nAccountID;
	Res.m_nCheckCode	= pCheck->m_nCheckCode;
	Res.m_nMapCode		= pCheck->m_nMapCode;
	Res.nRet			= MAP_LOGIN_RESULT_OK;
	GetServer()->SendData(GetServer()->m_hLoginServer, pComm->pcUID, PROTO_MAP_LOGIN_TO_MAP_RESULT, (char *)&Res, sizeof(struct MAP_LOGIN_TO_MAP_RESULT), 0);

/*	// kim ????
	long n, nTestSize;
	struct MAP_SEND_MESSAGE_TO_CLIENT nTest;

	nTest.m_nChannel = gameChannel_System;
	nTest.m_nTalkerUID = pCheck->m_nUID;
	nTest.m_nUID = pCheck->m_nUID;
	nTest.m_TalkerName[0] = 0;
	for (n=1; n<=5; n++)
	{
		wsprintf(nTest.m_Message, "代刚﹃ %d", n);
		nTest.m_nMessageSize = strlen(nTest.m_Message);
		nTestSize = nTest.GetSize();
		GetServer()->kimKeepData(pCheck->m_nUID, PROTO_MAP_SYSTEM_MESSAGE, (char *)&nTest, nTestSize);
	}
*/
	return(0);
}

long CMapServer::CB_MapToLoginResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_MAP_TO_LOGIN_RESULT *	pResult = (struct MAP_MAP_TO_LOGIN_RESULT *)pBuffer;
	struct MAP_CHANGE_MAP_SERVER *		pChangeMsg;
	CMapPlayer *	pPlayer;
	unsigned long uid;

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pResult->m_nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("ERROR: CB_MapToLoginResult: Player(%d) not found", pResult->m_nUID);
		return(-1);
	}
	// bug fix: ??d????O?_?w?g?R??
	if (GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
	{
		GetServer()->Log("ERROR: CB_MapToLoginResult: Player(%d) was deleted !", pResult->m_nUID);
		return(-1);
	}
	// 2009/08/18
	if (!pPlayer->CanSaveData())
	{
	//	if (pResult->nRet == MAP_LOGIN_RESULT_OK)			// 2010/03/10 ?w?g???a??????A?d??]?S??A?i??|?|?s????
		{
			GetServer()->KickPlayer(pPlayer);
		}
	//	else
	//	{
	//		if (pPlayer->m_nTeleportMapCode)				// 2010/01/12
	//		{
	//			pPlayer->m_nTeleportDueTime = CMapServer::GetServer()->GetLoopTime() + 5;	// ?Y?u??????
	//		}
	//	}
		//
		GetServer()->Log("ERROR: CB_MapToLoginResult: Player(%d) cannot save data !", pResult->m_nUID);
		return(-1);
	}

	switch(pResult->nRet)
	{
	case MAP_LOGIN_RESULT_OK:
		break;
	case MAP_LOGIN_RESULT_ERROR_MAPCODE:
		GetServer()->Log("CB_MapToLoginResult: Map not exist(Player=%d, MapCode=%d)", pResult->m_nUID, pResult->m_nMapCode);
		// ?p?G?O?_???A?n?q?? Client ?_?u
do001:	if(pPlayer->GetHP() <= 0)
		{
			// ??????s??, ?R??????
			pPlayer->SaveAllData();
			pPlayer->SetReady(false);							// 2008/07/07
			pPlayer->SetExitCode(CMapPlayer::EXIT_LOGOUT);
			GetServer()->DeleteObject(pPlayer->GetHandle());
		//	GetServer()->KickClient(pPlayer->GetClientID());	// 2004/10/26 ?H?K???n?X
		}
		return(0);
		break;
	case MAP_LOGIN_RESULT_ERROR:
		GetServer()->Log("ERROR: CB_MapToLoginResult: Undefined error(uid=%d)", pResult->m_nUID);
		return(-1);
		break;
	case MAP_LOGIN_RESULT_CONNECT_FULL:
		GetServer()->Log("CB_MapToLoginResult: MapServer connection full(uid=%d, MapCode=%d)", pResult->m_nUID, pResult->m_nMapCode);
		//
		pPlayer->SendMessage(gameChannel_System, NULL, gameGetResourceName(24275));
		goto do001;
		return(0);
		break;
	case MAP_LOGIN_RESULT_ERROR_LIMIT:
		GetServer()->Log("CB_MapToLoginResult: MapServer connection limit(%d)", pResult->m_nMapCode);
		//
		pPlayer->SendMessage(gameChannel_System, NULL, gameGetResourceName(24275));
		// ???}????BNPC????
		//if (pPlayer->GetShopID())	// ???e ChangeMap ??w?g ExitShop ?F
		{
			pPlayer->ExitShop();
			uid = pPlayer->GetUID();
			// client????q??????e???}?????protocol
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_FORCE_LEAVE_SHOP_NOTIFY, (char *)&uid, sizeof(uid), 0);
		}
		if (pPlayer->ExitNPCTalk())
			pPlayer->SendNPCTalkResult(NULL, 0, 0, 0);
		return(0);
		break;
	case MAP_LOGIN_RESULT_ERROR_NO_MAP:
		pPlayer->SendMsgToClientByID(24411);		// ???a??|???}??
		return(0);
		break;
	default:
		GetServer()->Log("CB_MapToLoginResult: Unknown(%d) !!!", pResult->nRet);
		return(0);
		break;
	}

//	GetServer()->Log("Change MapServer to %s:%d, PlayerUID = %d, MapCode = %d, ConnectID = %d", pResult->m_IP, pResult->m_nPort, pResult->m_nUID, pResult->m_nMapCode, pPlayer->GetClientID());

	// ?T?w?a??i?H????, ?x?s???a????]?w???u?T??
	pChangeMsg = pPlayer->GetChangeMapServerData();
	pChangeMsg->m_nCheckCode	= pResult->m_nCheckCode;
	pChangeMsg->m_nPort			= pResult->m_nPort;
	::memcpyT(pChangeMsg->m_IP, pResult->m_IP, sizeof(pResult->m_IP));

	// ???`???_??
	if(pPlayer->GetHP() <= 0)
		pPlayer->ResurrectImmed(0);

	pPlayer->m_nTeleportMapCode = 0;			// ?M????e?]?w	2010/01/12
	pPlayer->m_nTeleport2MapCode = 0;

#ifndef USE_PACKAGE_DATA
	GetServer()->Log("DEBUG: Change map to other map server(%d,%s)(%d)", pPlayer->GetUID(), pPlayer->GetName(), pResult->m_nMapCode);
#endif
	GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 1 );

	// By LRG
	pPlayer->LeaveMapBeforeChangeMap();		// ?b?????m?e??????????

	pPlayer->SetMapID((unsigned short)pResult->m_nMapCode,0);
	pPlayer->SetPos(pChangeMsg->m_Pos.x, pChangeMsg->m_Pos.y);
	//pPlayer->SaveAllData();
	pPlayer->SaveAllData_Lite();

	pPlayer->SetReady(false);

	pPlayer->SetExitCode(CMapPlayer::EXIT_CHANGE_SERVER);
	GetServer()->DeleteObject(pPlayer->GetHandle());

	return(0);
}

// **************************** ?n?J?{?? Client ****************************
// ??@???n?J??A?? Login ?q??
long CMapServer::CB_LoginEnterMap(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	std::map<unsigned long, CLoginCheck>::iterator	i;

	struct MAP_LOGIN_ENTER_MAP_RESULT	Res;
	struct MAP_LOGIN_ENTER_MAP *		pCheck = (struct MAP_LOGIN_ENTER_MAP *)pBuffer;
	CLoginCheck *						pLoginCheck;
	CMapPlayer * pPlayer;
	long is_delete;

	// LoginServer ?q?? MapServer ?????a?N?i?J
	GetServer()->Log("Warning: ENTER_MAP: 玩家(%d) will enter map but already in map. AccountID = %d, CheckCode = %d", pCheck->m_nUID, pCheck->nAccountID, pCheck->m_nCheckCode);

//	if (nLen != sizeof(struct MAP_LOGIN_ENTER_MAP))
//	{
//		GetServer()->Log("Wrong buffer size");
//		return -1;
//	}

	if (!GetServer()->GetMapManager()->FindMap(pCheck->m_nMapCode))
	{
		GetServer()->Log("警告:玩家(%d)进入不存在的地图(%d)", pCheck->m_nUID, pCheck->m_nMapCode);
		Res.nRet = MAP_LOGIN_RESULT_ERROR_MAPCODE;
		goto err001;
	}

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pCheck->m_nUID, CMapPlayer::CLASS_ID, &is_delete);
	if ((pPlayer != NULL) || is_delete)
	{
		GetServer()->Log("Warning: ENTER_MAP: Player(%d) will enter map but already in map. AccountID = %d, CheckCode = %d", pCheck->m_nUID, pCheck->nAccountID, pCheck->m_nCheckCode);
		// By LRG 2005/06/14
		Res.nRet			= MAP_LOGIN_RESULT_ERROR;
		goto err001;
	}

	// ??d?s?u??O?_?w??
	if(GetServer()->m_ServerInfo.iniMaxConnect <= (long)(GetServer()->m_ClientMap.size() + GetServer()->m_LoginCheckMap.size()))
	{
		// ?q?? LoginServer ?s?u?w?????a???i?A?n?J
		Res.nRet			= MAP_LOGIN_RESULT_CONNECT_FULL;
err001:	Res.m_nUID			= pCheck->m_nUID;
		Res.nAccountID		= pCheck->nAccountID;
		Res.m_nCheckCode	= pCheck->m_nCheckCode;
		GetServer()->SendData(GetServer()->m_hLoginServer, pComm->pcUID, PROTO_MAP_LOGIN_ENTER_MAP_RESULT, (char *)&Res, sizeof(struct MAP_LOGIN_ENTER_MAP_RESULT), 0);
		return(0);
	}

	// ?O?@?G??d??? IP
	GetServer()->CheckLockSelfIP();

	// ?????n?J???a
	i = GetServer()->m_LoginCheckMap.find(pCheck->m_nUID);
	if(i != GetServer()->m_LoginCheckMap.end())
		GetServer()->m_LoginCheckMap.erase(pCheck->m_nUID);

	pLoginCheck = &(GetServer()->m_LoginCheckMap[pCheck->m_nUID]);
	pLoginCheck->m_nCheckCode	= pCheck->m_nCheckCode;
	pLoginCheck->m_nAccountID	= pCheck->nAccountID;
	pLoginCheck->m_nCancelTime	= ::GetTickCount() + CLoginCheck::CANCEL_TIME;
	pLoginCheck->m_nPrivilege	= pCheck->nPrivilege;
	//pLoginCheck->m_nMapFlags = 0;
	pLoginCheck->m_nMapFlags = pCheck->nMapFlags;
	pLoginCheck->m_nIsFirstFromLogin = CHECK_LOGIN_FLAG_FIRST_LOGIN;
	pLoginCheck->m_nMapCode = pCheck->m_nMapCode;
	// ???~??
	pLoginCheck->m_nBotCheckType = pCheck->m_nBotCheckType;
	pLoginCheck->m_nBotCheckTimes = pCheck->m_nBotCheckTimes;
	// ???I?g
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	pLoginCheck->m_nOnlineTime = pCheck->nOnlineTime;
	pLoginCheck->m_nOfflineTime = pCheck->nOfflineTime;
	pLoginCheck->m_nLastLogoutTime = pCheck->nLastLogoutTime;
 #ifndef USE_PACKAGE_DATA
	GetServer()->Log("字符标志(0x%08x)", pCheck->nMapFlags);
 #endif
#endif

	// ?^??T???? LoginServer
	Res.m_nUID			= pCheck->m_nUID;
	Res.nAccountID		= pCheck->nAccountID;
	Res.m_nCheckCode	= pCheck->m_nCheckCode;
	Res.nRet			= MAP_LOGIN_RESULT_OK;
	GetServer()->SendData(GetServer()->m_hLoginServer, pComm->pcUID, PROTO_MAP_LOGIN_ENTER_MAP_RESULT, (char *)&Res, sizeof(struct MAP_LOGIN_ENTER_MAP_RESULT), 0);
/*
	// kim ????
	long n, nTestSize;
	struct MAP_SEND_MESSAGE_TO_CLIENT nTest;

	nTest.m_nChannel = gameChannel_System;
	nTest.m_nTalkerUID = pCheck->m_nUID;
	nTest.m_nUID = pCheck->m_nUID;
	nTest.m_TalkerName[0] = 0;
	for (n=1; n<=5; n++)
	{
		wsprintf(nTest.m_Message, "代刚﹃ %d", n);
		nTest.m_nMessageSize = strlen(nTest.m_Message);
		nTestSize = nTest.GetSize();
		GetServer()->kimKeepData(pCheck->m_nUID, PROTO_MAP_SYSTEM_MESSAGE, (char *)&nTest, nTestSize);
	}
*/
#ifdef NEW_PLAYER_STATISTICS
	//?n?J??K?[??a??H?? -- chenyin
	CMapServer *m_Server = CMapServer::GetServer();	
	unsigned short players = m_Server->AddMapPlayer((unsigned short)pCheck->m_nMapCode);	
 #ifdef _DEBUG
	m_Server->Log("玩家进入地图%hu 还有%hu人", pCheck->m_nMapCode,players);
 #endif
#endif
	return(0);
}

// Client ??@???n?J
long CMapServer::CB_ClientLogin(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	unsigned long	dwCode = *(unsigned long *)pBuffer;
	long			nResult;

	if(nLen != sizeof(unsigned long))
	{
		GetServer()->Log("ERROR: CB_ClientLogin: Wrong buffer length!");
		return(-1);
	}

//	GetServer()->Log("Client %d Login", nClientID);

	// 2006/04/13 Bug Fix
	CMapPlayer * pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	// ?D???`?s?u, ?j?????_?s?u
	if (pPlayer)
	{
#ifndef USE_PACKAGE_DATA
		GetServer()->Log("DEBUG: CB_ClientLogin kick (%d,%s)", pPlayer->GetUID(), pPlayer->GetName());
#endif
		GetServer()->KickPlayer(pPlayer);
		return(0);
	}

	// ??d?n?J??O?_?X?k
	if(dwCode != MAP_LOGIN_CHECK_CODE)
	{
		// ?D???`?s?u, ?j?????_?s?u
		GetServer()->KickClient(nClientID);		// ?i?g??n?J Timeout ?n?X
		GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
		return(-1);
	}

	// ??d??e?O?_?i???\?n?J
	//

	GetServer()->SetClientValid( nClientID, false, 60);	// ?W?[?w?]?_?u???

	nResult = MAP_CLIENT_LOGIN_RESULT_OK;
	::msgSendData(nClientID, pComm->pcUID, PROTO_MAP_CLIENT_LOGIN_RESULT, (char *)&nResult, sizeof(long), 0);

//	// ?O???s?u??
//	GetServer()->m_nConnectCount++;
//#ifndef CONSOL_MODE
//	if (GetServer()->m_pDialog)
//	{
//		char tmpstr[2048];
//
//		wsprintf(tmpstr, "%d", GetServer()->m_nConnectCount);
//		GetServer()->m_pDialog->m_Connect.SetWindowText(tmpstr);
//	}
//#endif

	return(0);
}

long CMapServer::CB_CheckPlayerUID(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	std::map<unsigned long, CLoginCheck>::iterator	i;

	struct MAP_CHECK_PLAYER_UID *	pCheckUID = (struct MAP_CHECK_PLAYER_UID *)pBuffer;
	CLoginCheck *	pLoginCheck;
	CMapPlayer *	pPlayer;
	long			hPlayer;
	unsigned long	nAccountID;
	bool			IsOK;
	long			nPrivilege, nIsFirstFromLogin, nMapFlags;
	unsigned long uid;
	//
	unsigned char bot_check_type;
	unsigned char bot_check_times;
	long is_delete;
#ifdef USE_OFFLINE_STALL
	long killed_offline_stall_ghost;
#endif
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	unsigned short	online_time;
	unsigned short	offline_time;
	long			last_logout_time;
#endif

	if(nLen != sizeof(struct MAP_CHECK_PLAYER_UID))
	{
		GetServer()->Log("ERROR: CB_CheckPlayerUID: Wrong buffer length!");
		return(-1);
	}

//	GetServer()->Log("Check player UID, UID = %d, CheckCode = %d", pCheckUID->m_nUID, pCheckUID->m_nCheckCode);

	// ?d??O?_?? LoginServer ???q??
	uid = pCheckUID->m_nUID;
	pLoginCheck	= NULL;
	IsOK		= true;
	i			= GetServer()->m_LoginCheckMap.find(pCheckUID->m_nUID);
	if(i != GetServer()->m_LoginCheckMap.end())
	{
		pLoginCheck = &(i->second);
		if(pLoginCheck->m_nCheckCode != pCheckUID->m_nCheckCode)
		{
			IsOK = false;
			GetServer()->m_LoginCheckMap.erase(pCheckUID->m_nUID);
		}
	}
	else
		IsOK = false;

	if(!IsOK)
	{
		pPlayer = (CMapPlayer *)GetServer()->FindPlayerByClientID(nClientID);
		if (pPlayer)
		{
			GetServer()->KickPlayer(pPlayer);
		}
		else
		{
			// ?????X??W?L???, ?????\?n?J
			GetServer()->Log("ERROR: CB_CheckPlayerUID: Data error or timeout(%d,%d,%d)", pCheckUID->m_nUID, pCheckUID->m_nCheckCode, nClientID);
			//
err:		GetServer()->kimClear(uid);
			GetServer()->pfcClearData(uid);
			GetServer()->KickClient(nClientID);		// ???n?J???? ???????n?X
		}
		return(-1);
	}

	if (!GetServer()->IsWorldDataReady() || !GetServer()->IsWorldTimeReady())
	{	// Map Server ?|?????n
		GetServer()->Log("ERROR: CB_CheckPlayerUID: Data not ready!(%d, %d)", pCheckUID->m_nUID, pCheckUID->m_nCheckCode);
		goto err;
	}

	nAccountID = pLoginCheck->m_nAccountID;
	nPrivilege = pLoginCheck->m_nPrivilege;
	nIsFirstFromLogin = pLoginCheck->m_nIsFirstFromLogin;
	nMapFlags = pLoginCheck->m_nMapFlags;
	//
	bot_check_type = pLoginCheck->m_nBotCheckType;
	bot_check_times = pLoginCheck->m_nBotCheckTimes;
	//
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	online_time = pLoginCheck->m_nOnlineTime;
	offline_time = pLoginCheck->m_nOfflineTime;
	last_logout_time = pLoginCheck->m_nLastLogoutTime;
#endif

	pLoginCheck = NULL;
	GetServer()->m_LoginCheckMap.erase(pCheckUID->m_nUID);

//	GetServer()->Log("Player(%d) login to MapServer", pCheckUID->m_nUID);

#ifdef USE_OFFLINE_STALL
	killed_offline_stall_ghost = 0;
	{
		CMapPlayer * pGhost = (CMapPlayer *)GetServer()->FindObjectByUIDForce(uid, CMapPlayer::CLASS_ID);
		if (pGhost && pGhost->IsOfflineStallGhost() && !GetServer()->IsObjectDeleted(pGhost->GetHandle()))
		{
			GetServer()->m_OfflineStallMgr.RemoveByHostUID(uid);
			pGhost->SetOfflineStallGhost(false);
			pGhost->CancelStall();
			GetServer()->DeleteObject(pGhost->GetHandle());
			killed_offline_stall_ghost = 1;
		}
	}
#endif

	// x64??? STATE_DELETE ?????????? is_delete ??
	{
		CMapPlayer * pGhost = (CMapPlayer *)GetServer()->FindObjectByUIDForce(uid, CMapPlayer::CLASS_ID);
		if (pGhost && pGhost->GetStateProc() == CMapObject::STATE_DELETE && !pGhost->IsSavingData())
		{
			pGhost->m_nDeleteKeepTime = 0;
			GetServer()->ChangeObjectState(pGhost->GetHandle(), CMapObject::STATE_DELETE);
			GetServer()->TempObjectProc();
		}
	}

	// ??d?O?_???????, ?p?G??????G
	// 1.???? Map ?s?b?? Server ???A??????e?]???s??L?[?A
	//   ?????N?????^???A????????s??C
	//
	// 2.???? Map ???s?b?? Server ???A??????e?L???I??s??L?[?A
	//   ?S?i?J?? Server?A????????s??C
	//
	// ??????ALogin Server ????|?? Map Server ?????~?|?n?X?b??
	// ?~??A??H?@??????~?B?z
	is_delete = 0;
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(uid, CMapPlayer::CLASS_ID, &is_delete);
	if (pPlayer)
	{
		long b_l, b_s;

		b_l = 0; b_s = 0;
		if (pPlayer->IsLoadingData())
			b_l++;
		if (pPlayer->IsSavingData())
			b_s++;
		GetServer()->Log("ERROR: Player(%d) login but player already exist(L=%d, S=%d) !", uid, b_l, b_s);
		goto err;
	}
#ifdef USE_OFFLINE_STALL
	if (is_delete && !killed_offline_stall_ghost)
#else
	if (is_delete)
#endif
	{
		GetServer()->Log("ERROR: Player(%d) login but deleted !", uid);
		goto err;
	}

	// ?????a????, ?V DBServer ?n?D???a????
	hPlayer	= GetServer()->CreatePlayer(nClientID, uid);
	if(hPlayer == -1)
	{
		// ???a???????????? ?j?????_?s?u
		GetServer()->Log("创建角色失败(ClientID=%d, PlayerUID=%d)", nClientID, uid);
		goto err;
	}

	GetServer()->Log("创建角色(%d,%d,%s)", uid, nClientID, napiServer_GetClientIP(nClientID));

	// ?O???s?u??
	GetServer()->m_nConnectCount++;
#ifndef CONSOL_MODE
	if (GetServer()->m_pDialog)
	{
		char tmpstr[2048];

		wsprintf(tmpstr, "%d", GetServer()->m_nConnectCount);
		GetServer()->m_pDialog->m_Connect.SetWindowText(tmpstr);
	}
#endif

	//GetServer()->ClearBanIP( nClientID );				// ?M?? Ban IP
	GetServer()->SetClientValid( nClientID, false, 60);	// ???s?p??

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByHandle(hPlayer);
	pPlayer->SetAccountID(nAccountID);
	//pPlayer->SetReady(true, 1);		// ???v?T?W???n???n?L????A
	pPlayer->SetReady(true, 3);		// ぃ紇臫Ω璶ぃ璶礚寄篈		// 2008/02/21
	pPlayer->LoadAllData(pComm->pcUID, nIsFirstFromLogin & CHECK_LOGIN_FLAG_FIRST_LOGIN);
	pPlayer->m_nPrivilege = (unsigned char)nPrivilege;		// 既魁舦ノ程魁 plrSPCFlag 籔穝 Login Server 魁ノ
	// ?~?? MapFlags
	if (nMapFlags)
	{
//#ifndef USE_PACKAGE_DATA
//CMapServer::GetServer()->Log("Player(%s) special falgs = 0x%08x !", pPlayer->GetName(), nMapFlags);
//#endif
		if (nMapFlags & LOGIN_CHAR_FLAGS_IS_BOT)
			pPlayer->SetIsBotPlayer();
		pPlayer->m_nWebIP_Flags |= (nMapFlags & ~LOGIN_CHAR_FLAGS_IS_BOT);
	}
	//
	if ((bot_check_type > 0) && (bot_check_times > 1))
	{
		pPlayer->m_nBotCheckType = bot_check_type;
		pPlayer->m_nBotCheckTimes = bot_check_times;
		pPlayer->m_nBotCheckDuedate = 0;
//GetServer()->Log("DEBUG: Login say Bot data: %d, %d", bot_check_type, bot_check_times);
	}
	else
	{
		pPlayer->m_nBotCheckType = 0;
		pPlayer->m_nBotCheckTimes = 0;
	}
	// ?]?w???I?g????
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	pPlayer->m_nOnlineTime = online_time;
	pPlayer->m_nOfflineTime = offline_time;
	pPlayer->m_nLastLogoutTime = last_logout_time;
#endif

	// ?O?_?n???? ??e??????]?w?a?I
	if (nIsFirstFromLogin & CHECK_LOGIN_FLAG_FIRST_LOGIN)
	{
		pPlayer->m_nPrivilege |= gamePRIVILEGE_TYPE_DELETED;	// ノ硂篨腹肚患材Ω祅
		//
		pPlayer->m_nHonorVal_Max = 0xffff;
	/*	// ??e??O????
		struct MAP_ASK_WORLD_FORCE_RESULT nRet;
		struct plrDATA_WORLD_FORCE * pForce = CMapServer::GetServer()->GetForceDataArray();

		::memcpyT( &nRet.pwForce, pForce, sizeof(nRet.pwForce) );
		nRet.pwfOrgTotal_WEI = GetServer()->pwfOrgTotal_WEI;
		nRet.pwfOrgTotal_SHU = GetServer()->pwfOrgTotal_SHU;
		nRet.pwfOrgTotal_WU = GetServer()->pwfOrgTotal_WU;
		::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_ASK_WORLD_FORCE_RESULT, (char*)&nRet, sizeof(nRet), 0);
		//?^???????O????
		struct MAP_ASK_WORLD_TOWN_RESULT townMsg;
		struct plrDATA_WORLD_TOWN * pTown = CMapServer::GetServer()->GetTownDataArray();

		::memcpyT( &townMsg.pwTown, pTown, sizeof(townMsg.pwTown) );
		::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_ASK_WORLD_TOWN_RESULT, (char*)&townMsg, sizeof(townMsg), 0);
	*/
		pPlayer->UpdateForceTownInfo();
	}
	if (nIsFirstFromLogin & CHECK_LOGIN_FLAG_COUNTRY_WAR_TELEPORT)
		pPlayer->m_nPrivilege |= gamePRIVILEGE_TYPE_CWAR;

	// ??e???e??n???T??
	//CMapServer::GetServer()->kimSendData(pPlayer->GetUID(), nClientID);

//	GetServer()->Log("Player(%d) begin load data, account id(%d)", uid, nAccountID);

	return(0);
}

void CMapServer::CB_LoginGetPartyInfoResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	long i;
	struct LOGIN_GET_PARTY_INFO_RESULT * pReq;
	//
	long map_id;
	unsigned long uid;
	struct MAP_PARTY_PLAYER_STATUS_MAP_POS_CLIENT nUpdMsg;
	CMapPlayer * pPlayer;
	CMapPlayer * pDest;
	//
	long member_exist = 0;

	pReq = (struct LOGIN_GET_PARTY_INFO_RESULT *)pBuffer;
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pReq->m_nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("Warning: Get player party info result but player not exist, uid = %d", pReq->m_nUID);
		return;
	}
	//
	if (pReq->m_nTotal > 0)
	{
		// ????s????????
		struct MAP_GET_PARTY_INFO_RESULT nRet;

		memset(&nRet, 0, sizeof(nRet));
		nRet.m_nTotal = pReq->m_nTotal;
		memcpyT(&nRet.m_MemberList, &pReq->m_MemberList, sizeof(nRet.m_MemberList));
		nRet.m_nLeaderUID = pReq->m_nLeaderUID;
		memcpyT(&nRet.m_MemberMapCode, &pReq->m_MemberMapCode, sizeof(nRet.m_MemberMapCode));
 		nRet.m_nViceLeaderUID = pReq->m_nViceLeaderUID;
 		memcpyT(&nRet.m_MemberSubNumber, &pReq->m_MemberSubNumber, sizeof(nRet.m_MemberSubNumber));
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PARTY_INFO_RESULT, (char *)&nRet, nRet.GetSize(), 0 );
		//
		map_id = pPlayer->GetMapCode();
		//pPlayer->PartyClear();
		pPlayer->PartySetType(pReq->m_nType);
		pPlayer->PartySetLeader(pReq->m_nLeaderUID);
		for (i=0; i<pReq->m_nTotal; i++)
		{
			uid = pReq->m_MemberList[i].m_nUID;
			pPlayer->PartyAdd(uid);
			// ??v??s??L??????m
			if (uid != pPlayer->GetUID())
			{
				pDest = (CMapPlayer *)GetServer()->FindObjectByUID(uid);
				if (pDest)
				{
					// 2011/10/21 Bug Fix: ?c????@???n?J?????A???a?A??K????????y???D(??????t ?L??A?|?~?P)
					//if (!(pDest->m_nPrivilege & gamePRIVILEGE_TYPE_DELETED))
					{
						member_exist++;
						//
						nUpdMsg.nUID = uid;
						nUpdMsg.nMapCode = pDest->GetMapCode();
						if (nUpdMsg.nMapCode == map_id)
						{
							nUpdMsg.nPosX = pDest->GetPosX();
							nUpdMsg.nPosY = pDest->GetPosY();
							nUpdMsg.nSubNumber = 255;		// ぃ穝
							if (nUpdMsg.nMapCode)
								::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_PARTY_PLAYER_STATUS_MAP_POS_CLIENT, (char *)&nUpdMsg, sizeof(nUpdMsg), 0 );
						}
					}
				}
			}
		}
		// ?]?w??v??m??s?q?????????
		if (!pPlayer->m_nSendPartyPosTivk)
		{
			pPlayer->m_nSendPartyPosTivk = PARTY_UPDATE_POS_TICK;
		}
		// ??d???W?O?_??????
		if (pPlayer->m_nPlayerFlags & PLAYER_FLAGS_CHECK_PARTY)
		{
			if (!member_exist)
				goto do_tele;
			//
			pPlayer->m_nPlayerFlags &= ~PLAYER_FLAGS_CHECK_PARTY;
		}
	}
	else
	{
		// ?M???????q??
		if (pPlayer->PartyGetTotal())
			pPlayer->PartyClear();
		//
		struct LOGIN_QUIT_PARTY_NOTIFY nQuitParty;

		nQuitParty.m_nUID = pPlayer->GetUID();		// 硄癸禜 UID
		nQuitParty.m_nLeaderUID = 0;				// 钉 UID
		nQuitParty.m_nQuitPlayerUID = pPlayer->GetUID();
		nQuitParty.m_szLeaderName[0] = 0;
		::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_QUIT_PARTY_NOTIFY, (char *)&nQuitParty, sizeof(nQuitParty), 0);
		//
		if (pPlayer->m_nPlayerFlags & PLAYER_FLAGS_CHECK_PARTY)
			pPlayer->m_nPlayerFlags &= ~PLAYER_FLAGS_CHECK_PARTY;
do_tele:
		CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SetQuitPartyTeleport(pPlayer);
	}
//	GetServer()->Log("Get player party info result from login, uid = %d", pReq->m_nUID);
}

long CMapServer::CB_GetPlayerDataShowSelfResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_SHOWSELFRESULT *	pShowSelf = (struct DB_GETPLAYERDATA_SHOWSELFRESULT *)pBuffer;
	struct plrDATA_SAVE *	pDataSave = &pShowSelf->Data;
	struct plrDATA			PlayerData;
	CMapCtrl *				pMap;
	CMapPlayer *			pPlayer;
	unsigned long			nPlayerUID;
	//
	unsigned long long		old_hp;
	unsigned long long		old_mp;
#if (defined(SMART_PLR_DATA2) || defined(DEBUG_SMART_PLR_DATA))
	struct plrDATA * pData;
#endif

	if(nLen != sizeof(struct DB_GETPLAYERDATA_SHOWSELFRESULT))
	{
		GetServer()->Log("ERROR: CB_GetPlayerDataShowSelfResult: Wrong buffer length!");
		return(-1);
	}

	// ?N??????s?J CMapPlayer ??
	nPlayerUID	= pDataSave->plrUID;
	pPlayer		= (CMapPlayer *)GetServer()->FindObjectByUID(nPlayerUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataShowSelfResult: Wrong player UID:%d, %s", pDataSave->plrUID, pDataSave->plrName);
		return(-1);
	}
	if (!pPlayer->IsWaitingCharData() && !pPlayer->IsLoadingData())
		return(-1);

	// ???s?????????????????????????????????n?A?p??@???A?p ?Q??????
	// magic_EH_MAX_HP
	old_hp = pDataSave->plrHP;
	old_mp = pDataSave->plrMP;
#if (defined(SMART_PLR_DATA2) || defined(DEBUG_SMART_PLR_DATA))
	 pData = pPlayer->GetCharData();
#endif
#ifdef SMART_PLR_DATA2
	PlayerData.plrSkillTable = pData->plrSkillTable;	// ?O?d pointer
	PlayerData.plrNPC = pData->plrNPC;
#endif
#ifdef DEBUG_SMART_PLR_DATA
	PlayerData.plrAIPtr = pData->plrAIPtr;
	PlayerData.plrOrg = pData->plrOrg;
#endif
	gameServer_MakeFullData(pDataSave, &PlayerData);
	
	/* ????????HP/MP???gameServer_MakeFullData??????????????????? */
	if (old_hp > 0 && old_hp <= PlayerData.plrHPMax)
		PlayerData.plrHP = PlayerData.plrSaveData.plrHP = old_hp;
	if (old_mp <= PlayerData.plrMPMax)
		PlayerData.plrMP = PlayerData.plrSaveData.plrMP = old_mp;
	
	pPlayer->SetCharData(&PlayerData);
	
	// ??????????
	// ??????0~400 ???????????????????????
	// ????<80 ??>400????????80
	unsigned short carryMax = pPlayer->GetSaveData()->plrMax_CarryItem;
	if (carryMax < 80 || carryMax > 400)
	{
		pPlayer->GetSaveData()->plrMax_CarryItem = 80;
		pPlayer->SaveCharData(0);
		// UpdateClientMaxItem will send the updated value to client
		pPlayer->UpdateClientMaxItem();
		GetServer()->Log("Normalize player(%d) bag size to 80 (old value=%hu)", pPlayer->GetUID(), carryMax);
	}
	
	// ?O?_?O?s??????
	if (pDataSave->plrLevel == 1)
	{
		if (pDataSave->plrMarkMapCode == 0xffff)
		{
			pPlayer->GetSaveData()->plrMarkMapCode = 0;
			pPlayer->SetHP(pPlayer->GetMaxHP());
			pPlayer->SetMP(pPlayer->GetMaxMP());
			pPlayer->SaveCharData(0);
		}
	}
	//
	// ??d?M???Y?? Cache(??K???a?w?g??s?Y??)
	if (GetServer()->m_nPlayerFace.find(nPlayerUID) != GetServer()->m_nPlayerFace.end())
	{
		struct PIC_PLAYER_HEAD * pHead;

		pHead = &GetServer()->m_nPlayerFace[nPlayerUID];
		if (pHead->time != pPlayer->GetSaveData()->plrBigHead_Time)
		{
			GetServer()->m_nPlayerFace.erase(nPlayerUID);
			if (GetServer()->m_nFaceCacheNum > 0)
				GetServer()->m_nFaceCacheNum--;
		}
	}
	// ?]?w?v??, ?????b plrBaseSPCFlag
	pPlayer->GetSaveData()->plrBaseSPCFlag &= ~(spcFLAG_GM | spcFLAG_GM2);
	pPlayer->GetCharData()->plrSPCFlag &= ~(spcFLAG_GM | spcFLAG_GM2);
	if ((pPlayer->m_nPrivilege & gamePRIVILEGE_TYPE_GM3) == gamePRIVILEGE_TYPE_GM3)
	{
		pPlayer->GetSaveData()->plrBaseSPCFlag |= spcFLAG_GM | spcFLAG_GM2;
		pPlayer->GetCharData()->plrSPCFlag |= spcFLAG_GM | spcFLAG_GM2;
		GetServer()->Log("错误: 玩家登陆超时(%d,%s)", pPlayer->GetUID(), pPlayer->GetName());
		//
		if (pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_INVISIBLE)
			pPlayer->m_nCharFlags |= CHAR_FLAG_GM_NO_APPEAR;
	}
	else if (pPlayer->m_nPrivilege & gamePRIVILEGE_TYPE_GM1)
	{
		pPlayer->GetSaveData()->plrBaseSPCFlag |= spcFLAG_GM;
		pPlayer->GetCharData()->plrSPCFlag |= spcFLAG_GM;
		CMapServer::GetServer()->Log("玩家(%d)是小管理员", pPlayer->GetUID());
		//
		if (pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_INVISIBLE)
			pPlayer->m_nCharFlags |= CHAR_FLAG_GM_NO_APPEAR;
	}
	else if (pPlayer->m_nPrivilege & gamePRIVILEGE_TYPE_GM2)
	{
		pPlayer->GetSaveData()->plrBaseSPCFlag |= spcFLAG_GM2;
		pPlayer->GetCharData()->plrSPCFlag |= spcFLAG_GM2;
		CMapServer::GetServer()->Log("玩家(%d)是小管理员", pPlayer->GetUID());
		//
		if (pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_INVISIBLE)
			pPlayer->m_nCharFlags |= CHAR_FLAG_GM_NO_APPEAR;
	}
	else
	{
		pPlayer->GetSaveData()->plrBaseSPCFlag &= ~(spcFLAG_INVISIBLE | spcFLAG_GODMODE);
		pPlayer->GetCharData()->plrSPCFlag &= ~(spcFLAG_INVISIBLE | spcFLAG_GODMODE);
	}

//CMapServer::GetServer()->Log("Player(%d) spc flag = %08x", pPlayer->GetUID(), pPlayer->GetCharData()->plrSPCFlag);
//::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
//CMapServer::GetServer()->Log("Player(%d) spc flag = %08x", pPlayer->GetUID(), pPlayer->GetCharData()->plrSPCFlag);

	pMap = GetServer()->FindMap(PlayerData.plrSaveData.plrMapCode);
	if(pMap == NULL)
	{
		GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );

		// Server ???S?????a??, Client ??????n?J?? Server
		pPlayer->SetExitCode(CMapPlayer::EXIT_DATA_ERROR);
		// ???m??????x?s?I
		pPlayer->GetSaveData()->plrMapCode = pPlayer->GetSaveData()->plrSaveMapCode;
		pPlayer->GetSaveData()->plrPosX = pPlayer->GetSaveData()->plrSavePosX;
		pPlayer->GetSaveData()->plrPosY = pPlayer->GetSaveData()->plrSavePosY;
		pPlayer->SaveCharData();
		//
		GetServer()->DeleteObject(pPlayer->GetHandle());
		GetServer()->Log("错误：没有此地图代码(%d,ClientID=%d)", PlayerData.plrSaveData.plrMapCode, pPlayer->GetClientID());
		return(-1);
	}

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_SHOWSELFRESULT, pComm->pcUID);

	return(0);
}

long CMapServer::CB_GetPlayerDataSkillResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_SKILL_RESULT *	pRes = (struct DB_GETPLAYERDATA_SKILL_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;
	int				cnt1;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: à︹ Skill 戈眔ア毖, PlayerUID = %d, AccountID = %d!", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	// ?R?????????A??K?W?????x?s???Q??\
//	GetServer()->Log("Player(%d) loading skill data ok", pRes->nUID);

	// ???s??????????????
	for(cnt1 = 0; cnt1 < gameMAX_SKILL_LEARN; cnt1++)
	{
		if(pRes->nData.plrSkillLevel[cnt1 + 1] == 0)
			pRes->nData.plrSkillLevel[cnt1 + 1] = 1;
	}
	gameServer_MakeSkill(&pRes->nData, pPlayer->GetSkillData());

	GetServer()->SetClientValid( pPlayer->GetClientID(), false, 60 );	// ???s?p??

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_SKILL_RESULT, pComm->pcUID);

	return(0);
}

long CMapServer::CB_GetPlayerDataItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_ITEM_RESULT *	pRes = (struct DB_GETPLAYERDATA_ITEM_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;

	if (nLen != (int)sizeof(struct DB_GETPLAYERDATA_ITEM_RESULT))
	{
		GetServer()->Log("ERROR: CB_GetPlayerDataItemResult: Wrong buffer length!");
		return(-1);
	}

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: à︹ Skill 戈眔ア毖, PlayerUID = %d, AccountID = %d!", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataItemResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	// ?R?????????A??K?W?????x?s???Q??\
//	GetServer()->Log("Player(%d) loading carry item data ok", pRes->nUID);

	// ???s??????????????
//	gameServer_MakeCarryItem(&pRes->nData, pPlayer->GetItemData());
	::memcpyT(pPlayer->GetItemData(), &pRes->nData, sizeof(struct plrCARRYITEM_SAVE));

	GetServer()->SetClientValid( pPlayer->GetClientID(), false, 60 );	// ???s?p??

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_ITEM_RESULT, pComm->pcUID);

	return(0);
}

long CMapServer::CB_GetPlayerDataStorageResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_STORAGE_RESULT *	pRes = (struct DB_GETPLAYERDATA_STORAGE_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: à︹ Skill 戈眔ア毖, PlayerUID = %d, AccountID = %d!", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataStorageResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	// ?R?????????A??K?W?????x?s???Q??\
//	GetServer()->Log("Player(%d) loading storage item data ok", pRes->nUID);

	// ???s??????????????
//	gameServer_MakeStorageItem(&pRes->nData, pPlayer->GetStorageData());
	::memcpyT(pPlayer->GetStorageData(), &pRes->nData, sizeof(struct plrSTORAGEITEM_SAVE));

	GetServer()->SetClientValid( pPlayer->GetClientID(), false, 60 );	// ???s?p??

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_STORAGE_RESULT, pComm->pcUID);

	return(0);
}

long CMapServer::CB_GetPlayerDataMaterialLibResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_MATERIALLIB_RESULT * pRes = (struct DB_GETPLAYERDATA_MATERIALLIB_RESULT *)pBuffer;
	CMapPlayer * pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: à︹ Skill 戈眔ア毖, PlayerUID = %d, AccountID = %d!", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataMaterialLibResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	
	// ?????????
	// ??????
	::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_GET_MATERIALLIB_RESULT, (char *)&pRes->nData, sizeof(pRes->nData), msgSEND_FLAG_COMPRESS);

	return(0);
}

long CMapServer::CB_GetPlayerDataNPCResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_NPC_RESULT *	pRes = (struct DB_GETPLAYERDATA_NPC_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: à︹ Skill 戈眔ア毖, PlayerUID = %d, AccountID = %d!", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataNPCResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	// ?R?????????A??K?W?????x?s???Q??\
//	GetServer()->Log("Player(%d) loading NPC data ok", pRes->nUID);

	// ???s??????????????
//	gameServer_MakeNPC(&pRes->nData, pPlayer->GetNPCData());
	::memcpyT(pPlayer->GetNPCData(), &pRes->nData, sizeof(struct plrDATA_NPC));

	GetServer()->SetClientValid( pPlayer->GetClientID(), false, 60 );	// ???s?p??

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_NPC_RESULT, pComm->pcUID);

	return(0);
}

long CMapServer::CB_GetPlayerDataMissionResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_MISSION_RESULT *	pRes = (struct DB_GETPLAYERDATA_MISSION_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: à︹ Skill 戈眔ア毖, PlayerUID = %d, AccountID = %d!", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataMissionResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	// ?R?????????A??K?W?????x?s???Q??\
//	GetServer()->Log("Player(%d) loading mission data ok", pRes->nUID);

	// ???s??????????????
	//gameServer_MakeMission(&pRes->nData, pPlayer->GetMissionData());
	memcpyT(pPlayer->GetMissionData(), &pRes->nData, sizeof(struct plrMISSION_SAVE));

	GetServer()->SetClientValid( pPlayer->GetClientID(), false, 60 );	// ???s?p??

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_MISSION_RESULT, pComm->pcUID);

	return(0);
}

long CMapServer::CB_GetPlayerDataFriendResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_FRIEND_RESULT *	pRes = (struct DB_GETPLAYERDATA_FRIEND_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: à︹ Skill 戈眔ア毖, PlayerUID = %d, AccountID = %d!", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataFriendResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	// ?R?????????A??K?W?????x?s???Q??\
//	GetServer()->Log("Player(%d) loading friend data ok", pRes->nUID);

	// ???s??????????????
	::memcpyT(&pPlayer->GetFriendData()->pFB, &pRes->nData, sizeof(struct plrDATA_FRIEND_SAVE));

	GetServer()->SetClientValid( pPlayer->GetClientID(), false, 60 );	// ???s?p??

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_FRIEND_RESULT, pComm->pcUID);

	return(0);
}

long CMapServer::CB_GetPlayerDataFashionResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_FASHION_RESULT *	pRes = (struct DB_GETPLAYERDATA_FASHION_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataSkillResult: à︹ Skill 戈眔ア毖, PlayerUID = %d, AccountID = %d!", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataFashionResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	// ?R?????????A??K?W?????x?s???Q??\
	// ???s??????????????
	::memcpyT(pPlayer->GetFashionData(), &pRes->nData, sizeof(struct plrDATA_FASHION_SAVE));
	{
		struct plrDATA_FASHION_SAVE * f = pPlayer->GetFashionData();
		short nBefore = f->plrActivatedFashionCount;
		NormalizeFashionActivatedCountOnLoad(f);
		if (f->plrActivatedFashionCount != nBefore)
			pPlayer->SaveFashionData(pComm->pcUID);
	}

	// ????????????????????????
	gameServer_CalcCharacterAttribute(pPlayer->GetCharData());

	GetServer()->SetClientValid( pPlayer->GetClientID(), false, 60 );	// ???s?p??

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_FASHION_RESULT, pComm->pcUID);

	return(0);
}

// ??????????
long CMapServer::CB_GetPlayerDataTianshuResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_TIANSHU_RESULT *	pRes = (struct DB_GETPLAYERDATA_TIANSHU_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataTianshuResult: Read error, PlayerUID=%d, AccountID=%d", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataTianshuResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	// ?????????
	// ????????????
	struct plrDATA_TIANSHU_SAVE* pTianshuData = pPlayer->GetTianshuData();
	if(pTianshuData)
	{
		memcpy(pTianshuData, &pRes->nData, sizeof(struct plrDATA_TIANSHU_SAVE));
	}
	
	// ????????????????
	struct plrDATA* pCharData = pPlayer->GetCharData();
	if(pCharData)
	{
		// ???????????????
		if(pRes->nData.plrTianshuActiveEquipID != 0 || pRes->nData.plrTianshuActiveHufuID != 0)
		{
			extern void RestoreTianshuSkillForPlayer(CMapPlayer* pPlayer, long equipID, long hufuID);
			RestoreTianshuSkillForPlayer(pPlayer, pRes->nData.plrTianshuActiveEquipID, pRes->nData.plrTianshuActiveHufuID);
		}
		
		// ??????????????
		extern void gameServer_CalcCharacterAttribute(struct plrDATA * pData);
		gameServer_CalcCharacterAttribute(pCharData);
		
		// ??????
		pPlayer->UpdateShowData();
		pPlayer->UpdateClientShowData(0);
	}
	
	// ???????????
	struct tagMAPMSG_TIANSHU_LOAD_RESULT tianshuMsg;
	memset(&tianshuMsg, 0, sizeof(tianshuMsg));
	
	// ??????????unsigned char -> unsigned char??
	for (int i = 0; i < 100; i++)
	{
		tianshuMsg.equipActivated[i] = pRes->nData.plrTianshuEquipActivated[i];
		tianshuMsg.hufuActivated[i] = pRes->nData.plrTianshuHufuActivated[i];
	}
	
	tianshuMsg.equipSelected = pRes->nData.plrTianshuEquipSelected;
	tianshuMsg.hufuSelected = pRes->nData.plrTianshuHufuSelected;
	
	::msgSendData(pPlayer->GetClientID(), 0, PROTO_TIANSHU_LOAD_RESULT, (char*)&tianshuMsg, sizeof(tianshuMsg), 0);
	
	// ??????
	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_TIANSHU_RESULT, pComm->pcUID);

	return(0);
}

long CMapServer::CB_GetPlayerDataAvatarAwakenResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_AVATAR_AWAKEN_RESULT * pRes = (struct DB_GETPLAYERDATA_AVATAR_AWAKEN_RESULT *)pBuffer;
	CMapPlayer * pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataAvatarAwakenResult: Read error, PlayerUID=%d, AccountID=%d", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataAvatarAwakenResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	if(pPlayer->GetAvatarAwakenData())
	{
		memcpy(pPlayer->GetAvatarAwakenData(), &pRes->nData, sizeof(struct plrDATA_AVATAR_AWAKEN_SAVE));
	}

	{
		extern void ServerImportAvatarAwakenDataFromDB(CMapPlayer* pMapPlayer, struct plrDATA_AVATAR_AWAKEN_SAVE* pSaveData);
		ServerImportAvatarAwakenDataFromDB(pPlayer, &pRes->nData);
	}

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_AVATAR_AWAKEN_RESULT, pComm->pcUID);
	return(0);
}

long CMapServer::CB_GetPlayerDataDungeonClearResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_DUNGEON_CLEAR_RESULT * pRes = (struct DB_GETPLAYERDATA_DUNGEON_CLEAR_RESULT *)pBuffer;
	CMapPlayer * pPlayer;

	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataDungeonClearResult: Read error, PlayerUID=%d, AccountID=%d", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataDungeonClearResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	GetServer()->GetMapSpaceManager()->LoadPlayerDungeonClearData(pRes->nUID, &pRes->nData);
	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_DUNGEON_CLEAR_RESULT, pComm->pcUID);
	return(0);
}

long CMapServer::CB_GetPlayerDataTowerSlashResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_GETPLAYERDATA_TOWER_SLASH_RESULT * pRes = (struct DB_GETPLAYERDATA_TOWER_SLASH_RESULT *)pBuffer;
	CMapPlayer * pPlayer;
	struct plrDATA_TOWER_SLASH_SAVE * pSave;

	(void)nID;
	(void)nLen;
	if(pRes->nOK == 0)
	{
		GetServer()->Log("CB_GetPlayerDataTowerSlashResult: Read error, PlayerUID=%d, AccountID=%d", pRes->nUID, pRes->nAccountID);
		return(-1);
	}

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pRes->nUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("CB_GetPlayerDataTowerSlashResult: Wrong player UID:%d", pRes->nUID);
		return(-1);
	}
	pSave = pPlayer->GetTowerSlashData();
	if (pSave)
	{
		memcpy(pSave, &pRes->nData, sizeof(struct plrDATA_TOWER_SLASH_SAVE));
		if (pSave->currentFloor < 1)
			pSave->currentFloor = 1;
	}

	pPlayer->LoadResult(PROTO_DB_GETPLAYERDATA_TOWER_SLASH_RESULT, pComm->pcUID);
	return(0);
}

long CMapServer::CB_LoginGetTowerSlashRankResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_GET_TOWER_SLASH_RANK_RESULT * pRes;

	(void)nID;
	(void)pComm;
	if (nLen != (int)sizeof(struct LOGIN_GET_TOWER_SLASH_RANK_RESULT))
		return -1;
	pRes = (struct LOGIN_GET_TOWER_SLASH_RANK_RESULT *)pBuffer;
	TowerSlashServer_SetRankData(&pRes->nData);
	return 0;
}

long CMapServer::CB_LoginSyncTowerSlashRank(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_SYNC_TOWER_SLASH_RANK * pSync;

	(void)nID;
	(void)pComm;
	if (nLen != (int)sizeof(struct LOGIN_SYNC_TOWER_SLASH_RANK))
		return -1;
	pSync = (struct LOGIN_SYNC_TOWER_SLASH_RANK *)pBuffer;
	TowerSlashServer_SetRankData(&pSync->nData);
	return 0;
}

// Client ??????a????A???n?n?J
long CMapServer::CB_InitClientReady(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	struct LOGIN_CLIENT_LOGIN_TO_MAP_OK	LoginMsg;
//	struct MAP_UPDATE_PLAYER_DATA_PART1	UpdateMsg;
//	struct MAP_UPDATE_PLAYER_DATA_PART2	UpdateMsg2;
	struct plrDATASHOWSELF Self;
	CMapPlayer *	pPlayer;
	//
	struct MAP_UPDATE_PLAYER_DATA_PART2 nMsg;
	long nMsgSize;
	long is_teleport;
	long country_id;
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	//
	struct MAP_SET_REPAYMENT * pRepayData;
	struct MAP_SET_ACTIVITY * pActivityData;
	long i;
	long no_teleport_in_space = 0;

	pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		GetServer()->Log("ERROR: CB_InitClientReady: Connect error(ClientID=%d)", nClientID);
		GetServer()->KickClient(nClientID);
		return(-1);
	}

	if (pPlayer->GetStateProc() == CMapObject::STATE_DELETE)
	{
		GetServer()->Log("SERIOUS: CB_InitClientReady: Player is deleted !");
		return(-1);
	}

	if (pPlayer->IsLoadingData())
		return(-1);

	is_teleport = 0;
	//if (pPlayer->IsReady())
	if (GetServer()->GetClientValid(nClientID))
	{
		is_teleport++;						// ㄏノ俐丁簿笆, ┪琌 goto 
		pPlayer->m_TeleportKickTime = 0;	// 睲埃丁浪琩
		// ?M???a????w????
		pPlayer->MapCtrl_UnLock(1);
//GetServer()->Log("Player teleport ok !");
		//
		if (pPlayer->IsReady())
		{
		//	pPlayer->SaveAuotSaveData();
		//	GetServer()->SetClientValid( nClientID, false );
		//	pPlayer->SetReady(false);
			// ?D???`?s?u, ?j?????_?s?u
			GetServer()->Log("ERROR: CB_InitClientReady: Already Ready ok, ClientID = %d, uid = %d, name = '%s'", nClientID, pPlayer->GetUID(), pPlayer->GetSaveData()->plrName);
		//	pPlayer->SetExitCode(CMapPlayer::EXIT_KICK);
		//	GetServer()->DeleteObject(pPlayer->GetHandle());
		//	//GetServer()->KickClient(nClientID);
			GetServer()->KickPlayer(pPlayer);
			return(-1);
		}
		// ?q???S?????A(???O????????)
		if (!pPlayer->m_bIsTeleport)
		{
			pPlayer->SendSpecialStatus();
			// ?]?w???v?????A
			CMapServer::GetServer()->m_nHistoryManager.SetPlayerEnterInfo(pPlayer, pPlayer->GetMapCode());
		}
	}
//	else
//		GetServer()->Log("Player(%d, %s)(%d) ready(%d)", pPlayer->GetUID(), pPlayer->GetSaveData()->plrName, nClientID, pPlayer->GetMapID());

	//
#ifdef USE_INVISIBLE_ENTER_MAP
	if (!pPlayer->m_bIsTeleport)
		pPlayer->SetInvisibleEnterStage();
#endif
	// ????i?J?a??
	//pPlayer->m_nCharFlags &= ~CHAR_FLAG_STOP_DAMAGE;
	pPlayer->ExitNPCTalk();
#ifdef USE_OFFLINE_STALL
	GetServer()->m_OfflineStallMgr.RemoveByHostUID(pPlayer->GetUID());
#endif
	pPlayer->SetReady(true);
	GetServer()->ChangeObjectState(pPlayer->GetHandle(), CMapObject::STATE_ENTER_MAP);
	GetServer()->SetClientValid( nClientID, true );

	pPlayer->CalcHPRestoreTime();
	pPlayer->CalcMPRestoreTime();
	pPlayer->CalcSTRestoreTime();

	// ??K??e?a??L?{???A?S???Q??X?h
	if (CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_IsSpaceMap(pPlayer->GetMapCode())
		&& !TowerSlashServer_IsPlayerInSession(pPlayer)
		&& !TowerSlashServer_IsCopyInSession(pPlayer->GetCopyUID()))
	{
		// ??d?O?_?????
		if (pPlayer->m_nPrivilege & gamePRIVILEGE_TYPE_DELETED)	// ??@???n?J?a??
		{
			if (!pPlayer->m_bIsTeleport)
			{
				if (CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_CheckReLogin(pPlayer, pPlayer->GetMapCode(), pPlayer->GetCopyUID()))
				{
  #ifndef USE_PACKAGE_DATA
					CMapServer::GetServer()->Log("DEBUG: Map space relogin(%s)", pPlayer->GetName());
  #endif
					no_teleport_in_space++;
				}
			}
		}
		// ??s???
		CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_UpdateClientData(pPlayer->GetMapCode(), pPlayer,pPlayer->GetCopyUID());
		//
		// If relogin check passed, keep player in current space copy.
		// Only schedule fallback teleport when relogin is NOT valid.
		if (!no_teleport_in_space)
			CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SetPlayerTeleportData(pPlayer, -1, -1, 0, 0, 3,pPlayer->GetCopyUID());
	}

	TowerSlashServer_UpdateClientData(pPlayer);

	if (!pPlayer->m_bIsTeleport)
	{
		pPlayer->SetSoulBattleStatus();
		pPlayer->m_nCharFlags &= ~CHAR_FLAG_STOP_DAMAGE;
		//
		// ??s???O????????
		pPlayer->SetEnterStageTimeData();

#ifdef USE_ORGANIZE_MAP_PK
		CMapServer::GetServer()->OrgMapPK_InitEnterMapPlayer(pPlayer);
#endif
	}
	else
	{
		pPlayer->m_bIsTeleport = false;
		//
		pPlayer->m_nOuterDueTime = 0;		// 睲埃 outer time 筄魁
	}

	if (is_teleport)
	{
	//	if(pPlayer->GetHP() <= 0)
	//		pPlayer->ResurrectImmed(0);
		/* Teleport/change-map used to return here without pushing show self; client never refreshed plrShowData (e.g. plrCPRankForTitle). */
		CMapServer::GetServer()->pfcUpdateData(pPlayer->GetUID());
		::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
		pPlayer->CheckIllegalEquipItem();
		if (pPlayer->IsIllegalEquipItem())
			pPlayer->SendMsgToClientByID(24403);
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof(struct plrDATASHOWSELF), 0);
		pPlayer->UpdateClientAllDeputyItems();
		memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
		SendFashionDataOnLogin(pPlayer);
		GetServer()->m_nLastCombatPowerRankTitlePoll = 0;
		return(0);
	}

	// ??s??O???q??
	CMapServer::GetServer()->pfcUpdateData(pPlayer->GetUID());

	// ?q?? LoginServer ????n?J???\
	LoginMsg.m_nUID			= pPlayer->GetUID();
	LoginMsg.m_nAccountID	= pPlayer->GetAccountID();
	LoginMsg.m_nMapCode		= pPlayer->GetMapID();
	GetServer()->SendData(GetServer()->m_hLoginServer, pComm->pcUID, PROTO_LOGIN_CLIENT_LOGIN_TO_MAP_OK, (char *)&LoginMsg, sizeof(struct LOGIN_CLIENT_LOGIN_TO_MAP_OK), 0);

	::memset(&Self, 0, sizeof(plrDATASHOWSELF));
	::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
	::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
	pPlayer->UpdateClientAllDeputyItems();
	memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));

	// ??????????
	SendFashionDataOnLogin(pPlayer);

	// ?V Login Server ?n?D???o??e??????
	GetServer()->m_nLastCombatPowerRankTitlePoll = 0;

	long map_code;
	if (!is_teleport)
	{
		struct LOGIN_GET_PARTY_INFO nParty;

		nParty.m_nUID = pPlayer->GetSaveData()->plrUID;
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_GET_PARTY_INFO, (char *)&nParty, sizeof(nParty), 0);
//CMapServer::GetServer()->Log("Player(%d) enter map and get party info... ", GetUID());
	
		///////////////////////////////////////////////////////////////
#ifdef WAR_ADD_ARMY_POINT
		pPlayer->GetArmyDataByKey(ORG_DATA_KEY_ARMY_POINTS);
#endif
#ifdef FORCE_BATTLE_FIELD
		pPlayer->GetArmyDataByKey(ORG_DATA_KEY_BATTLEFIELD_DUETIME);
		pPlayer->m_nArmyBattleFieldCheckTick = 10000;
#endif
		pPlayer->GetArmyDataByKey(ORG_DATA_KEY_ACTIVEDATA);
		///////////////////////////////////////////////////////////////
	}

	// ??d?O?_?B??S???????A???e
	if (pPlayer->m_nPrivilege & gamePRIVILEGE_TYPE_DELETED)	// ??@???n?J?a??
	{
		// ?j?????I?g
		if (pPlayer->m_nWebIP_Flags & LOGIN_CHAR_FLAGS_SPCMODE_CN)
		{
#ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("Player(%s) is special mode CN !", pPlayer->GetName());
#endif
			//
// ?]?w???I?g????
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
			pPlayer->GetSaveData()->plrSPCMode_CN_OnLineTime = pPlayer->m_nOnlineTime;
			pPlayer->GetSaveData()->plrSPCMode_CN_OffLineTime = pPlayer->m_nOfflineTime;
			pPlayer->GetSaveData()->plrSPCMode_CN_LastLogoutTime = pPlayer->m_nLastLogoutTime;
			//pPlayer->AutoSaveCharData();
 #ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("Player(%s) time data(%d,%d)(%d)", pPlayer->GetName(), pPlayer->m_nOnlineTime, pPlayer->m_nOfflineTime, pPlayer->m_nLastLogoutTime);
 #endif
#endif
			pPlayer->SpecModeCN_FirstLoginNotify();
		}
		// ??d??s???~
		pPlayer->CheckDupeItem();
		// ?q?? Client ??????
		nMsg.Reset();
		nMsg.Add(UPART2_TYPE_WAR_TIME, CMapServer::GetServer()->m_nWarWeekDay, CMapServer::GetServer()->m_nWarHour, CMapServer::GetServer()->m_nWarMin);

		// ?q?? Client ????T
		if (CMapServer::GetServer()->IsWar())
		{
			nMsg.Add(UPART2_TYPE_WAR_MODE, 0, CMapServer::GetServer()->IsWar(), CMapServer::GetServer()->IsWarType());
			nMsg.Add(UPART2_TYPE_WAR_KING_SCORE, 0, 1, CMapServer::GetServer()->m_nWarKingScore.nValue[1]);
			nMsg.Add(UPART2_TYPE_WAR_KING_SCORE, 0, 2, CMapServer::GetServer()->m_nWarKingScore.nValue[2]);
			nMsg.Add(UPART2_TYPE_WAR_KING_SCORE, 0, 3, CMapServer::GetServer()->m_nWarKingScore.nValue[3]);
		}

		// ?q?? Client ?Z??????
		struct MAP_SET_SOUL_DATA * pSoulSet = CMapServer::GetServer()->GetSoulSetData();
		nMsg.Add(UPART2_TYPE_SOUL_BATTLE_TIME, pSoulSet->nBattleWeekDay, pSoulSet->nBattleTime_Hour, pSoulSet->nBattleTime_Min);

		//
		nMsg.Add(UPART2_TYPE_SOUL_BATTLE_TIICKET_TIME, 0, *(long *)&pSoulSet->nTicketSell[0], *(long *)&pSoulSet->nTicketSell[0+4]); // ???P???X

		// ?q?????v???A
		pRepayData = CMapServer::GetServer()->GetRepayData();
		//
		i = CMapServer::GetServer()->GetLoopTime();
		if (pRepayData->nType)
		{
			if ((i >= pRepayData->nTimeBegin) && (i <= pRepayData->nTimeEnd))
			{
				// ?T??s????????
				if (pRepayData->nCharCreateTime)
				{
					if ((long)pPlayer->GetSaveData()->plrCreateTime > pRepayData->nCharCreateTime)
						goto no_repay;
				}
				//
				nMsg.Add(UPART2_TYPE_REPAYMENT, (short)pRepayData->nEveryDay, pRepayData->nTimeBegin, pRepayData->nTimeEnd);
			}
		}
no_repay:
		// ?q????????A
		pActivityData = CMapServer::GetServer()->GetActivityData();
		if (pActivityData->nType)
		{
			if ((i >= pActivityData->nTimeBegin) && (i <= pActivityData->nTimeEnd))
			{
				nMsg.Add(UPART2_TYPE_ACTIVITY, (short)pActivityData->nType, pActivityData->nTimeBegin, pActivityData->nTimeEnd);
			}
		}
		//
		nMsgSize = nMsg.GetSize();
		::msgSendData(nClientID, 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsgSize, 0);
		//
		pPlayer->m_nPrivilege &= ~gamePRIVILEGE_TYPE_DELETED;
		// ???B?z: ???P??O??e
		map_code = pPlayer->GetMapCode();
		if (CMapServer::GetServer()->IsMapWar(map_code, 1))		// ?D???a??A???????q??(?]?t 3 ??????)
		{
			pTown = CMapServer::GetServer()->GetTownDataByID(map_code);
			if (pTown == &g_nSafeTownData)
				goto force01;
#ifndef USE_PACKAGE_DATA
CMapServer::GetServer()->Log("Player(%s) enter cwar map(%d,%d) ... ", pPlayer->GetSaveData()->plrName, map_code, pTown->ptCountryID);
#endif
			if (pTown->ptCountryID && (pPlayer->GetSaveData()->plrCountryID != pTown->ptCountryID))
			{
force01:		long map_code = pPlayer->GetSaveData()->plrSaveMapCode;
				long map_x = pPlayer->GetSaveData()->plrSavePosX;
				long map_y = pPlayer->GetSaveData()->plrSavePosY;
				CMapServer::GetServer()->ChangeSaveMap(pPlayer, map_code, map_x, map_y, 1);
				return(0);
			}
		}
		else
		{
			if (CMapServer::GetServer()->IsSpecialMapWar(map_code, &country_id))
			{
#ifndef USE_PACKAGE_DATA
CMapServer::GetServer()->Log("Player(%s) enter special cwar map(%d) ... ", pPlayer->GetSaveData()->plrName, map_code);
#endif
				if (pPlayer->GetSaveData()->plrCountryID != country_id)
					goto force01;
			}
		}
		//
		if (!no_teleport_in_space)
			pPlayer->ChangeMapInStageMapPos();					// ?n?J??e
	}
	else if (pPlayer->m_nPrivilege & gamePRIVILEGE_TYPE_CWAR)
	{
		pPlayer->m_nPrivilege &= ~gamePRIVILEGE_TYPE_CWAR;
		// ???B?z: ???P??O??e
//// ???????????
//#ifndef USE_PACKAGE_DATA
		map_code = pPlayer->GetMapCode();
		if (CMapServer::GetServer()->IsMapWar(map_code, 1))		// ?D???a??A???????q??(?]?t 3 ??????)
		{
			struct plrDATA_WORLD_TOWN_DATA * pTown = CMapServer::GetServer()->GetTownDataByID(map_code);
			if (pTown == &g_nSafeTownData)
				goto force02;
//CMapServer::GetServer()->Log("Player(%s) enter map(%d)(%d) ... ", pPlayer->GetSaveData()->plrName, map_code, pTown->ptCountryID);
			if (pTown->ptCountryID && (pPlayer->GetSaveData()->plrCountryID != pTown->ptCountryID))
			{
force02:		long map_code = pPlayer->GetSaveData()->plrSaveMapCode;
				long map_x = pPlayer->GetSaveData()->plrSavePosX;
				long map_y = pPlayer->GetSaveData()->plrSavePosY;
				CMapServer::GetServer()->ChangeSaveMap(pPlayer, map_code, map_x, map_y, 1);
				return(0);
			}
		}
//#endif
	}

#if defined USE_FAQ_SYSTEM
	FaqClientAdd(nClientID, 0);
#endif

	if (pPlayer && pPlayer->IsReady())
	{
		pPlayer->UpdateCarryNPCTable();
		pPlayer->SyncCnpcItemSkillsToClient();
	}

	return(0);
}

long CMapServer::CB_ClientLogout(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	CMapPlayer *	pPlayer;
	long			nResult;

	pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		GetServer()->KickClient(nClientID);
		GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
		return(-1);
	}

//	GetServer()->Log("Player(%d, %s) logging out.", pPlayer->GetUID(), pPlayer->GetSaveData()->plrName);

#ifndef USE_PACKAGE_DATA
	Log("DEBUG: Cannot enter now(%s)", pPlayer->GetName());
#endif
	GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );

	// ??????s??, ???}?a??
	pPlayer->SaveAllData(0, 1);
	pPlayer->SetReady(false);			// 2008/07/07
#ifdef USE_OFFLINE_STALL
	if (!pPlayer->IsOfflineStallGhost())
#endif
	{
		MapServer_SendLogoutToLogin(GetServer(), pPlayer, 1);
		pPlayer->SetExitCode(CMapPlayer::EXIT_LOGOUT);
		GetServer()->DeleteObject(pPlayer->GetHandle());
	}
	// ?q?? Client ?i?H?_?u
	nResult = MAP_CLIENT_LOGOUT_RESULT_OK;
	::msgSendData(nClientID, pComm->pcUID, PROTO_MAP_CLIENT_LOGOUT_RESULT, (char *)&nResult, sizeof(long), 0);

	return(0);
}

long CMapServer::CB_KickClient(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_KICK_CLIENT *	pKick = (struct MAP_KICK_CLIENT *)pBuffer;
	CMapPlayer *				pPlayer;
	long is_delete;

	GetServer()->Log("玩家(%d) 由登录服务器踢出", pKick->m_nUID);

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pKick->m_nUID, CMapPlayer::CLASS_ID, &is_delete);
	if (!pPlayer)
		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUIDForce(pKick->m_nUID, CMapPlayer::CLASS_ID);
	if (is_delete)
		goto in_delete;
#ifdef USE_OFFLINE_STALL
	if (pPlayer && pPlayer->IsOfflineStallGhost())
	{
		GetServer()->m_OfflineStallMgr.RemoveByHostUID(pPlayer->GetUID());
		pPlayer->SetOfflineStallGhost(false);
		pPlayer->CancelStall();
	}
#endif
	if(pPlayer == NULL)
	{
		// ??? ?e?^ Login ??X???\
		if (GetServer()->m_LoginCheckMap.find(pKick->m_nUID) != GetServer()->m_LoginCheckMap.end())
		{
			GetServer()->m_LoginCheckMap.erase(pKick->m_nUID);
			//
	//		struct MAP_KICK_CLIENT_RESULT	KickMsg;
	//		KickMsg.m_nUID		= pKick->m_nUID;
	//		KickMsg.m_nResult	= MAP_KICK_CLIENT_RESULT_OK;
	//		GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_MAP_KICK_CLIENT_RESULT, (char *)&KickMsg, sizeof(struct MAP_KICK_CLIENT_RESULT), 0);
		}
		// ?????s?b??????????~
		GetServer()->Log("玩家(%d) 由登录服务器踢出", pKick->m_nUID);
		//
		struct MAP_KICK_CLIENT_RESULT	KickMsg;
		KickMsg.m_nUID		= pKick->m_nUID;
		KickMsg.m_nResult	= MAP_KICK_CLIENT_RESULT_OK;
		GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_MAP_KICK_CLIENT_RESULT, (char *)&KickMsg, sizeof(struct MAP_KICK_CLIENT_RESULT), 0);
		return(-1);
	}

	// ?p?G???b?R???B?z?h????(????timeout??|?q?? Login ?n?X)
	if (GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
	{
in_delete:
		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUIDForce(pKick->m_nUID, CMapPlayer::CLASS_ID);
		if (pPlayer)
		{
			MapServer_SendLogoutToLogin(GetServer(), pPlayer, 0);
			pPlayer->CancelSaveWaitForRemove();
			if (!pPlayer->IsOutMap())
				pPlayer->LeaveMap();
			pPlayer->m_nDeleteKeepTime = 0;
		}
		{
			struct MAP_KICK_CLIENT_RESULT	KickMsg;
			KickMsg.m_nUID		= pKick->m_nUID;
			KickMsg.m_nResult	= MAP_KICK_CLIENT_RESULT_OK;
			GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_MAP_KICK_CLIENT_RESULT, (char *)&KickMsg, sizeof(struct MAP_KICK_CLIENT_RESULT), 0);
		}
		return(-1);
	}

	GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );
	// ??????s??, ???}?a??
	//pPlayer->SaveAllData();
	pPlayer->SaveAuotSaveData();
	pPlayer->SetReady(false);			// By LRG 2005/06/06
	MapServer_SendLogoutToLogin(GetServer(), pPlayer, 0);	/* kick: clear login + leave map */
	if (!pPlayer->IsOutMap())
		pPlayer->LeaveMap();
	pPlayer->m_nDeleteKeepTime = 0;
	pPlayer->m_nBotKickTime = 0;
	pPlayer->SetExitCode(CMapPlayer::EXIT_KICK);
	pPlayer->CancelSaveWaitForRemove();
	GetServer()->DeleteObject(pPlayer->GetHandle());
	pPlayer->m_nDeleteKeepTime = 0;
	//GetServer()->KickClient(pPlayer->GetClientID());

	return(0);
}

long CMapServer::CB_ClientSpeedCheck(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
#ifndef NO_CLIENT_SPEED_CHECK
	long serial;
	long is_deleted;
#endif

	struct MAP_CLIENT_SPEED_CHECK *	pSpeedCheck = (struct MAP_CLIENT_SPEED_CHECK *)pBuffer;

	if(nLen != sizeof(struct MAP_CLIENT_SPEED_CHECK))
	{
		GetServer()->Log("错误: CB_ClientSpeedCheck: 缓冲区长度错误!");
		return(-1);
	}

//	GetServer()->Log("Receive speed check(%d) from client(%d)", pSpeedCheck->m_nSerial, nClientID);

#ifndef NO_CLIENT_SPEED_CHECK
	CMapPlayer * pPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		if (!is_deleted)
		{
		//	GetServer()->KickClient(nClientID);
		//	GetServer()->Log("ERROR: CB_ClientSpeedCheck: ?D???`?s?u, ClientID = %d", nClientID);
		}
		return(-1);
	}

	serial = pPlayer->GetSpeedCheckSerial() & 0xfffff;
	if (!serial)
		goto skip_first;
	if (serial == gameMAX_PING_SERIAL)
	{
		serial = 1;
	}
	else
		serial++;
	//if(pPlayer->GetSpeedCheckSerial() == pSpeedCheck->m_nSerial)
	if(serial != (pSpeedCheck->m_nSerial & 0xfffff))
	{
		// ?[?t??d?y????????
		//GetServer()->Log("Ping serial no repeat(ClientID=%d, Serial=%d)", nClientID, pSpeedCheck->m_nSerial);
		GetServer()->Log("Ping serial error(ClientID=%d, Serial=%08x,%08x)", nClientID, pPlayer->GetSpeedCheckSerial(), pSpeedCheck->m_nSerial);

	//	GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );
	//	// ??????s??, ???}?a??
	//	pPlayer->SaveAllData();
	//	pPlayer->SetExitCode(CMapPlayer::EXIT_LOGOUT);
	//	GetServer()->DeleteObject(pPlayer->GetHandle());
	//	//GetServer()->KickClient(pPlayer->GetClientID());		// 2004/10/26 ?H?K???n?X
		GetServer()->KickPlayer(pPlayer);
		return(0);
	}
skip_first:
	//
	if (!(pSpeedCheck->m_nSerial & gameBOT_MARK_PING_FLAG))	// ?~??
		pPlayer->SetBotRecord(BOT_REASON_PING_ERR, 0, 1);
	if (pSpeedCheck->m_nSerial & gameBOT_MARK_PING_FLAG2)	// ?~??
	{
		pPlayer->SetBotRecord(BOT_REASON_PING_ERR2);
	}

	int nRes = pPlayer->AccelerateCheck();
	if(nRes > 0)
	{
		// ?o?{???a?[?t
		GetServer()->Log("Find speed-fast(ClientID=%d, Player(%d, %s), type=%d)", nClientID, pPlayer->GetUID(), pPlayer->GetSaveData()->plrName, nRes);
		GetServer()->SendLogMessage_System(pPlayer, LOGTYPE_SYS_ADD_SPEED);
		//
		pPlayer->SendMsgToClientByID(95);
speed_err:
		//
		struct LOGIN_MAP_KICK_PLAYER nKick;

		memset(&nKick, 0, sizeof(nKick));
		nKick.nMinutes = 1;		// ??w???, ????
		nKick.uid = 0;
		nKick.nType = 2;
		msg_strncpy(nKick.m_szName, pPlayer->GetName(), sizeof(nKick.m_szName));
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->GetLoginServer(), 0, PROTO_LOGIN_MAP_KICK_PLAYER, (char *)&nKick, sizeof(nKick), 0);
	}
	else if(nRes < 0)
	{
		// ?o?{???a??t
		GetServer()->Log("Find speed-slow(ClientID=%d, Player(%d, %s), type=%d)", nClientID, pPlayer->GetUID(), pPlayer->GetSaveData()->plrName, nRes);
		GetServer()->SendLogMessage_System(pPlayer, LOGTYPE_SYS_SUB_SPEED);
		//
		//pPlayer->SendMsgToClientByID(94);
		goto speed_err;
	}

	if(nRes != 0)
	{
		GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );
		// ??????s??, ???}?a??
		pPlayer->SaveAllData();
		//pPlayer->SetExitCode(CMapPlayer::EXIT_LOGOUT);
		pPlayer->SetReady(false);								// 2008/07/07
		pPlayer->SetExitCode(CMapPlayer::EXIT_KICK);
		GetServer()->DeleteObject(pPlayer->GetHandle());
		//GetServer()->KickClient(pPlayer->GetClientID());		// 2004/10/26 ?H?K???n?X
		return(0);
	}

	pPlayer->SetSpeedCheckSerial(pSpeedCheck->m_nSerial);
	pPlayer->IncSpeedCheckCount();
#endif

	return(0);
}

void CMapServer::DeletePlayerFaceCache(unsigned long uid)
{
	if (m_nPlayerFace.find(uid) != m_nPlayerFace.end())
	{
		if (m_nFaceCacheNum > 0)
			m_nFaceCacheNum--;
		m_nPlayerFace.erase(uid);
#ifndef USE_PACKAGE_DATA
		Log("DEBUG: Delete player face cache ok(%d)", uid);
#endif
	}
}

long CMapServer::CB_GetFace( char *pBuffer, int nLen, int nClientID, proto_COMM *pComm )
{
	struct DB_GET_FACE	nReq;
	unsigned long		nUID;
	CMapPlayer *		pPlayer;
//	CMapPlayer *		pFacePlayer;

	if(nLen != sizeof(unsigned long))
	{
		GetServer()->Log("ERROR: CB_GetFace: Wrong buffer length!");
		return(-1);
	}

	nUID = *(unsigned long *)pBuffer;
//	GetServer()->Log("Player(%d) get face", nUID);

	if (!IsPlayerUID(nUID))
		return(-1);

	pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	if(pPlayer == NULL)
	{
	//	// ?D???`?s?u, ?j?????_?s?u
	//	GetServer()->KickClient(nClientID);
	GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
		return(-1);
	}

//	pFacePlayer = (CMapPlayer *)GetServer()->FindObjectByUID(nUID);
//	if(pFacePlayer == NULL)
//	{
//		// ?n?D???????s?b
//		GetServer()->Log("CB_GetFace: 璶―à︹ぃ, UID = %d", nUID);
//		return(0);
//	}

	if (GetServer()->m_nPlayerFace.find(nUID) != GetServer()->m_nPlayerFace.end())
	{
		struct LOGIN_GET_FACE_RESULT	nRet;
		struct PIC_PLAYER_HEAD * pHead;

		pHead = &GetServer()->m_nPlayerFace[nUID];
		if (pHead->status == FACE_STATUS_OK)
		{
			nRet.nUID = nUID;
			nRet.isOK = 1;
			nRet.time = pHead->time;
			memcpyT(&nRet.pic, &pHead->pic, sizeof(nRet.pic));
			::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_LOGIN_GET_FACE_RESULT, (char *)&nRet, sizeof(nRet), 0);
			return(0);
		}
		else	// ??a DB Pass ?T??
		{
			nReq.nType = 2;
			nReq.nUID		= nUID;
			nReq.nSenderUID	= pPlayer->GetUID();
			GetServer()->SendData(GetServer()->m_hDBServer, pComm->pcUID, PROTO_DB_GET_FACE, (char *)&nReq, sizeof(nReq), 0);
		}
	}
	else
	{
		if (GetServer()->m_nFaceCacheNum >= 25000)		// 4Kb * 25000 = 102MB
		{
			GetServer()->m_nPlayerFace.clear();			// 禬筁甧砛睲埃ㄓ
			GetServer()->m_nFaceCacheNum = 0;
		}
		//
		GetServer()->m_nPlayerFace[nUID].status = FACE_STATUS_ASK_TO_DB;
		GetServer()->m_nPlayerFace[nUID].time = 0;
		GetServer()->m_nFaceCacheNum++;
		//
		nReq.nType = 1;
		nReq.nUID		= nUID;
		nReq.nSenderUID	= pPlayer->GetUID();
		GetServer()->SendData(GetServer()->m_hDBServer, pComm->pcUID, PROTO_DB_GET_FACE, (char *)&nReq, sizeof(nReq), 0);
	}
	return(0);
}

long CMapServer::CB_GetFaceResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_GET_FACE_RESULT	nRet;
	struct DB_GET_FACE_RESULT *		upf;
	CMapPlayer *					pPlayer;

	upf = (struct DB_GET_FACE_RESULT *)pBuffer;

//	GetServer()->Log("Player(%d) face return from DB", upf->nUID);

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(upf->nSenderUID);
	if(pPlayer == NULL)
	{
		GetServer()->Log("ERROR: CB_GetFaceResult: Wrong sender UID:%d", upf->nSenderUID);
		return(-1);
	}

	// ?O?_?O Pass ?? DB ??????
	if (upf->isOK == 2)
	{
		if (GetServer()->m_nPlayerFace.find(upf->nUID) != GetServer()->m_nPlayerFace.end())
		{
			struct PIC_PLAYER_HEAD * pHead;

			pHead = &GetServer()->m_nPlayerFace[upf->nUID];
			if (pHead->status == FACE_STATUS_OK)
			{
				nRet.nUID = upf->nUID;
				nRet.isOK = 1;
				nRet.time = pHead->time;
				memcpyT(&nRet.pic, &pHead->pic, sizeof(nRet.pic));
				::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_LOGIN_GET_FACE_RESULT, (char *)&nRet, sizeof(nRet), 0);
			}
		}
		return(0);
	}
	else
	{
		nRet.nUID = upf->nUID;
		nRet.isOK = upf->isOK;
		nRet.time = upf->time;
		memcpyT(&nRet.pic, &upf->pic, sizeof(nRet.pic));
		::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_LOGIN_GET_FACE_RESULT, (char *)&nRet, sizeof(nRet), 0);
	}

	// ?????? Cache
	if (GetServer()->m_nPlayerFace.find(upf->nUID) != GetServer()->m_nPlayerFace.end())
	{
		GetServer()->m_nPlayerFace[upf->nUID].status = FACE_STATUS_OK;
		GetServer()->m_nPlayerFace[upf->nUID].time = upf->time;
		memcpyT(&GetServer()->m_nPlayerFace[upf->nUID].pic, &upf->pic, sizeof(GetServer()->m_nPlayerFace[upf->nUID].pic));
	}
	return(0);
}

// **************************** ?????? ****************************

long CMapServer::CB_PlayerWalkTo(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	CVector2D					PathPoint[MAP_PLAYER_WALK_TO_MAX_POINTS];
	struct MAP_FIX_CHAR_POS		FixMsg;
	struct MAP_PLAYER_WALK_TO *	pWalkTo = (struct MAP_PLAYER_WALK_TO *)pBuffer;
	CMapPlayer *				pPlayer;
	unsigned long				dx, dy;
//	long						lx, ly, nx, ny;
//	int							cnt1;
	long is_deleted;
	CMapCtrl *					pMapCtrl;

	pPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		if (!is_deleted)
		{
		//	GetServer()->KickClient(nClientID);
		//	GetServer()->Log("ERROR: CB_PlayerWalkTo: ?D???`?s?u, ClientID = %d", nClientID);
		}
		return(-1);
	}

	if (pWalkTo->m_nFlags == 0xffff)
	{
		//pPlayer->SetMemoryCheckFlag((struct MAP_PLAYER_WALK_TO_CHECK *)pWalkTo);
		return(-1);
	}

	if (pPlayer->m_nMoveInputDelay)
		return(-1);
	pPlayer->m_nMoveInputDelay = PLAYER_MOVE_DELAY_TIME;

	if (!(pWalkTo->m_nFlags & gameBOT_MARK_FLAG))	// ?~??
	{
		pPlayer->SetBotRecord(BOT_REASON_WALK_ERR);
	}

	if (pPlayer->GetAttackTime())		// ??@??? !?
		return(-1);

	if (pPlayer->IsCasting() || pPlayer->IsBlocking() || pPlayer->IsOutMap())
		return(-1);

	if(pWalkTo->m_nPointCount < 2 || pWalkTo->m_nPointCount > MAP_PLAYER_WALK_TO_MAX_POINTS)
	{
		pPlayer->m_nMoveInputDelay = PLAYER_MOVE_DELAY_TIME_LONG;
		// Point ??q???b?d??, ?L?k???????
		GetServer()->Log("Walk: Point data error, PlayerUID = %d", pPlayer->GetUID());
		return(-1);
	}

	if (pPlayer->GetSaveData()->plrStatus2 & effFun2_NO_MOVE)
		return(-1);

	if (!pPlayer->CheckCan_Attack_Cast_Move(1 | 0x80000000))
		return(-1);

//	pPlayer->ClearSuperMode();
//	pPlayer->ExitNPCTalk();
	dx = abs((long)(pWalkTo->m_Point[0].x - pPlayer->GetPosX()));
	dy = abs((long)(pWalkTo->m_Point[0].y - pPlayer->GetPosY()));
	if(dx > gameFIX_POS_DIST || dy > gameFIX_POS_DIST)
	{
		if( dx > gameERROR_POS_DIST || dy > gameERROR_POS_DIST)
			return(-1);
		// ??m?~?t?L?j, ????????m
GetServer()->Log("Walk: Point data error, PlayerUID = %d", pPlayer->GetUID());

		FixMsg.m_nUID	= pPlayer->GetUID();
		FixMsg.m_Pos.x	= pPlayer->GetPosX();
		FixMsg.m_Pos.y	= pPlayer->GetPosY();
		::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_FIX_CHAR_POS, (char *)&FixMsg, sizeof(struct MAP_FIX_CHAR_POS), 0);

		//return(0);	// ?~??
	}

	// ???????, ??d?Z??(??d??????u???)
/*	lx = pPlayer->GetPosX();
	ly = pPlayer->GetPosY();
	for(cnt1 = 0; cnt1 < pWalkTo->m_nPointCount; cnt1++)
	{
		//PathPoint[cnt1].x = pWalkTo->m_Point[cnt1].x;
		//PathPoint[cnt1].y = pWalkTo->m_Point[cnt1].y;
		//
		nx = pWalkTo->m_Point[cnt1].x;
		ny = pWalkTo->m_Point[cnt1].y;
		dx = abs(nx - lx);
		dy = abs(ny - ly);
		if ( dx > gameERROR_POS_DIST || dy > gameERROR_POS_DIST)
		{
			pPlayer->m_nMoveInputDelay = PLAYER_MOVE_DELAY_TIME_LONG;
			return(-1);
		}
		PathPoint[cnt1].x = nx;
		PathPoint[cnt1].y = ny;
		lx = nx;
		ly = ny;
	}
*/
	// Bug fix: ???? 2011/08/01
	pMapCtrl = pPlayer->GetMapCtrl();
	if (!pMapCtrl)
		return(-1);
	if (pMapCtrl->GetIconData(gameIconCalc_DIV(pPlayer->GetPosX()), gameIconCalc_DIV(pPlayer->GetPosY())) == MAPCODE_ID_WALL)
		return(-1);
	//
	dx = sizeof(CVector2D) * pWalkTo->m_nPointCount;
	memcpy(PathPoint, pWalkTo->m_Point, dx);
	//pPlayer->SetHitObject(false);
	if (!pPlayer->WalkTo(pWalkTo->m_nPointCount, PathPoint))
	{
		// ??e???????T??
		struct MAP_MOVE_CHAR_TO nRet;

		memset(&nRet, 0, sizeof(nRet));	// speed = 0 ??????~
		nRet.m_nUID	= pPlayer->GetUID();
		::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_MOVE_CHAR_TO, (char *)&nRet, sizeof(nRet), 0);
	}

//GetServer()->Log("(%s)Walk: (%d,%d)(%d,%d)", pPlayer->GetName(), pPlayer->GetPosX(), pPlayer->GetPosY(), pWalkTo->m_Point[0].x, pWalkTo->m_Point[0].y);

	return(0);
}

// ???a?n?D??????????
long CMapServer::CB_PlayerAttack(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_PLAYER_ATTACK *	pAttack = (struct MAP_PLAYER_ATTACK *)pBuffer;
	CMapPlayer *	pPlayer;
	CMapChar *		pTarget;
	long is_deleted;
	long force, r;
	struct gs_StageData * pStg;

	pPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pPlayer == NULL)
	{
	//	// ?D???`?s?u, ?j?????_?s?u(PS: ???W?L?a???|?X?k?o????{?H)
	//	if (!is_deleted)
	//	{
	//		GetServer()->KickClient(nClientID);
	GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
	//	}
		return(-1);
	}

	if(nLen != sizeof(struct MAP_PLAYER_ATTACK))
	{
		//GetServer()->Log("ERROR: CB_PlayerAttack: Wrong buffer length!");
		pPlayer->SetBotRecord(BOT_REASON_INVALID_COMMAND2);
		pPlayer->PlayerAttackErrorCount(0);
		return(-1);
	}

	// 2006/01/09
#ifdef CHECK_ATTACK_FREQUENCE
	pPlayer->CheckLastActionTick(0, pAttack->m_nClientTick, pPlayer->m_nAttackInputDelayRecord);
	pPlayer->m_nAttackInputDelayRecord = 0;
#else
	if (pPlayer->m_nAttackInputDelay)
	{
	//	pPlayer->PlayerAttackErrorCount(BOT_REASON_GM_NOTICE_ATTACKSPD);
err_stop_move:
		if (pPlayer->IsMoving())
			pPlayer->StopMoving();
		return(-1);
	}
	pPlayer->m_nAttackInputDelay = (short)pPlayer->CalcAttackTime();
#endif
	//
	if (pPlayer->IsCasting() || pPlayer->IsBlocking())
	{
#ifdef CHECK_BOT_SO_SYSTEM
		pPlayer->CheckBOT(pAttack->m_nFlags & 0xfff);
#endif
#ifdef CHECK_ATTACK_FREQUENCE
		if (pPlayer->m_nAttackInputDelayRecord)
		{
			pPlayer->CheckLastActionTick(0, pAttack->m_nClientTick, pPlayer->m_nAttackInputDelayRecord);
			pPlayer->m_nAttackInputDelayRecord = 0;
		}
#endif
		//return(-1);
		goto err_stop_move;
	}

	force = 0;
#ifdef USE_FORCE_PK
	if (pAttack->m_nFlags & gameBOT_MARK_FLAG_FORCE)
	{
 #if (defined(USE_FORCE_PK) && defined(USE_FORCE_PK_LEVEL_LIMIT))
  #ifdef USE_FORCE_PK_NO_HISTORY_LIMIT
		if (!(pPlayer->m_nCharFlags & CHAR_HISTORY_BATTLE))	// ???v???(2011/01/26)
  #endif
 #endif
		{
//GetServer()->Log("Force attack !");
 #if (defined(USE_FORCE_PK) && defined(USE_FORCE_PK_LEVEL_LIMIT))
			if (!GetServer()->IsWar())	// ????L????
			{
				if (pPlayer->GetSaveData()->plrLevel < PLAYER_USE_FORCE_PK_LEVEL)		// 2008/11/17
					//return(-1);
					goto err_stop_move;
			}
 #endif
			//
			force = 0x80000000;
			// ??d?O?_?i?HPK
			pStg = gameStageGetPtr(pPlayer->GetMapCode());
			if (!pStg)
			{
	//			GetServer()->Log("ERROR: Force attack 1!");
				//return(-1);
				goto err_stop_move;
			}
			if (!(pStg->gstgFlag & gstgFLAG_FORCE_PK))
			{
	//			GetServer()->Log("ERROR: Force attack 2!");
			//	pPlayer->PlayerAttackErrorCount(2);
				//return(-1);
				goto err_stop_move;
			}
		}
	}
#endif

	// ?O?@?G??d??? IP
	if (!GetServer()->IsSafeLockSelfIP())
	{
//		GetServer()->Log("1 !");
		//return(-1);
		goto err_stop_move;
	}

	if (!(pAttack->m_nFlags & gameBOT_MARK_FLAG))	// ?~??
		pPlayer->SetBotRecord(BOT_REASON_ATTACK_ERR);
	if (pAttack->m_nFlags & gameBOT_MARK_FLAG_NO_INPUT)
	{
#ifndef USE_PACKAGE_DATA
	GetServer()->Log("ERROR: CB_PlayerAttack: %s No Input", pPlayer->GetName());
#endif
		//CMapServer::GetServer()->BOT_NotifyGM(pPlayer, BOT_REASON_GM_NOTICE_BOT1);
	// ???^???A?? TracePlayer ????
	//	pPlayer->PlayerActErrorLogCount(BOT_REASON_GM_NOTICE_BOT1);
	}
#ifdef CHECK_BOT_SO_SYSTEM
	pPlayer->CheckBOT(pAttack->m_nFlags & 0xfff);
#endif

	if (!pPlayer->CheckCan_Attack_Cast_Move(2))
	{
//		GetServer()->Log("2 !");
//		pPlayer->PlayerAttackErrorCount(3);
		//return(-1);
		goto err_stop_move;
	}
	if (!pPlayer->Soul_CanAttack())
	{
//		GetServer()->Log("3 !");
	//	pPlayer->PlayerAttackErrorCount(4);
		//return(-1);
		goto err_stop_move;
	}

	// GM ????Y????p???B?z
	if (pPlayer->GetSaveData()->plrBaseSPCFlag & (spcFLAG_INVISIBLE | spcFLAG_GODMODE))
	{
#ifndef USE_PACKAGE_DATA
		if (pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_GODMODE)
			GetServer()->Log("ERROR: GM in god mode !");
#endif
		//return(-1);
		goto err_stop_move;
	}

	if (pAttack->m_nTargetUID > gameMAX_ENEMY_UID)
	{
		pPlayer->SetBotRecord(BOT_REASON_ATTACK_ERR);
		pPlayer->PlayerAttackErrorCount(5);
		//return(-1);
		goto err_stop_move;
	}
/*
#ifdef CHECK_ATTACK_FREQUENCE
	pPlayer->CheckLastActionTick(0, pAttack->m_nClientTick, pPlayer->m_nAttackInputDelayRecord);
	pPlayer->m_nAttackInputDelayRecord = 0;
#else
	if (pPlayer->m_nAttackInputDelay)
		return(-1);
	pPlayer->m_nAttackInputDelay = pPlayer->CalcAttackTime();
#endif
*/
	//if (!pPlayer->IsAvailable() || pPlayer->IsNPCTalk())
	//{
	//	pPlayer->PlayerActErrorLogCount();
	//	return(-1);
	//}

	//pTarget = (CMapChar *)GetServer()->FindObjectByUID(pAttack->m_nTargetUID, CMapChar::CLASS_ID);
	pTarget = (CMapChar *)GetServer()->FindAndCheckCharExistByUID(pAttack->m_nTargetUID, CMapChar::CLASS_ID);
	if(pTarget == NULL)
	{
//		GetServer()->Log("ERROR: CB_PlayerAttack: тぃю阑ヘ夹, PlayerUID = %d, TargetUID = %d", pPlayer->GetUID(), pAttack->m_nTargetUID);
		pPlayer->PlayerAttackErrorCount(6);
		//return(-1);
		goto err_stop_move;
	}

	if (pPlayer->GetMapCode() != pTarget->GetMapCode())
	{
//		GetServer()->Log("ERROR: Map not match !");
		pPlayer->PlayerAttackErrorCount(7);
		//return(-1);
		goto err_stop_move;
	}

	if (pTarget->m_nCharFlags & CHAR_TRAP)
		//return(-1);
		goto err_stop_move;

#if (defined(USE_FORCE_PK) && defined(USE_FORCE_PK_LEVEL_LIMIT))
	if (force & 0x80000000)
	{
		if (!GetServer()->IsWar())	// ????L????
		{
			if (IsPlayerUID(pAttack->m_nTargetUID))
			{
				if (pTarget->GetSaveData()->plrLevel < PLAYER_USE_FORCE_PK_LEVEL)		// 2008/11/17
					//return(-1);
					goto err_stop_move;
			}
			else if (IsEnemyUID(pAttack->m_nTargetUID))		// 2010/03/09 ??H???j??P?w
			{
				force = 0;
			}
		}
	}
#endif

//GetServer()->Log("Player(%d) attack (%d)", pPlayer->GetUID(), pTarget->GetUID());

//	pPlayer->ClearSuperMode();
//	pPlayer->ExitNPCTalk();
	if(pPlayer->IsAttackTarget(pTarget->GetHandle(), force))
	{
//		GetServer()->Log("Player(%d) attack char(%d)", pPlayer->GetUID(), pTarget->GetUID());
		//
		if (!pTarget->IsSuperMode())
		{
			pPlayer->SetAttackTarget(pTarget->GetHandle());
#ifdef CHECK_ATTACK_FREQUENCE
			if (r = pPlayer->Attack(pPlayer, NULL, force))
			{
				pPlayer->ClearNoAttackMode();
				pPlayer->m_nAttackInputDelayRecord = (pPlayer->CalcAttackTime() * 3) >> 2;
			}
//			else
//				GetServer()->Log("ERROR: attack !");
#else
			r = pPlayer->Attack(pPlayer, NULL, force);
			pPlayer->ClearNoAttackMode();
#endif
			// 2011/08/09 ?????????\?n???U?A?_?hclient ????|????
			//if (!r)
			{
				if (pPlayer->IsMoving())
					pPlayer->StopMoving();
			}
		}
//		else
//			GetServer()->Log("Player(%d) can not attack, char(%d) is super mode", pPlayer->GetUID(), pTarget->GetUID());
	}
	else
	{
//		GetServer()->Log("Player(%d) can not attack, char(%d) is not an enemy", pPlayer->GetUID(), pTarget->GetUID());
		pPlayer->PlayerAttackErrorCount(8);
	}

	return(0);
}

// ?O?_?O?Q????
long Inner_Is_Passive_Skill(long magic_id)
{
	if ((magic_id >= 251) && (magic_id < 300))
		return(1);
	switch(magic_id)
	{
	case 501:				// 皚剪絤
	case 502:				// 璽矗狜
		return(1);
		break;
	}
	return(0);
}

long CMapServer::CB_PlayerCastMagic(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_PLAYER_CAST_MAGIC *	pCast = (struct MAP_PLAYER_CAST_MAGIC *)pBuffer;
	struct magicDATA *				pMagic;
	CMapPlayer *	pPlayer;
	CMapObject *	pObject;
	CMapChar *		pTarget;
	CMapCharMsg *	pMsg;
	long is_deleted;
	long i, no_input, dly, need_retry;
	long force;
	long retry = 0;
	struct gs_StageData * pStg;

	pPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pPlayer == NULL)
	{
	//	// ?D???`?s?u, ?j?????_?s?u(PS: ???W?L?a???|?X?k?o????{?H)
	//	if (!is_deleted)
	//	{
	//		GetServer()->KickClient(nClientID);
	GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
	//	}
		return(-1);
	}

	if(nLen != sizeof(struct MAP_PLAYER_CAST_MAGIC))
	{
		//GetServer()->Log("ERROR: CB_PlayerCastMagic: Wrong buffer length!");
		pPlayer->SetBotRecord(BOT_REASON_INVALID_COMMAND2);
		pPlayer->PlayerAttackErrorCount(20);
		return(-1);
	}

#ifdef CHECK_ATTACK_FREQUENCE
	pPlayer->CheckLastActionTick(1, pCast->m_nClientTick, pPlayer->m_nCastInputDelayRecord);
//GetServer()->Log("%s,%d,%d", pPlayer->GetName(), pCast->m_nClientTick, pPlayer->m_nCastInputDelayRecord);
#endif
	//
	// GM ????Y????p???B?z
	if (pPlayer->GetSaveData()->plrBaseSPCFlag & (spcFLAG_INVISIBLE | spcFLAG_GODMODE))
	{
#ifndef USE_PACKAGE_DATA
		if (pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_GODMODE)
			GetServer()->Log("ERROR: GM in god mode !");
#endif
		goto abort_stop_mov;
	}
	//
	if (pPlayer->m_nCastInputDelay)
	{
//		GetServer()->Log("Fast cast delay(%d)", pPlayer->m_nCastInputDelay);
//GetServer()->Log("Cast stop 2");
		//goto abort_stop_mov;
		//pPlayer->PlayerAttackErrorCount(21);
		if (!pPlayer->IsOutMap() && !pPlayer->IsDead())
			pPlayer->PlayerCastResultNotify(1);
		return(-1);
	}
	pPlayer->m_nCastInputDelayRecord = 0;

	//if (pPlayer->IsCasting() || pPlayer->m_nCastInputDelay || pPlayer->IsBlocking())
	if (pPlayer->IsCasting() || pPlayer->IsBlocking())
	{
//GetServer()->Log("Cast stop 3");
#ifdef CHECK_BOT_SO_SYSTEM
		pPlayer->CheckBOT(pCast->m_nFlags & 0xfff);
#endif
	//	if (pPlayer->m_nCastInputDelayRecord)
	//	{
#ifdef CHECK_ATTACK_FREQUENCE
	//		pPlayer->CheckLastActionTick(1, pCast->m_nClientTick, pPlayer->m_nCastInputDelayRecord);
	//		pPlayer->m_nCastInputDelayRecord = 0;
#endif
	//		// ???n?@?????e
	//		if (!pPlayer->IsOutMap() && !pPlayer->IsDead())
	//			pPlayer->PlayerCastResultNotify(1);
	//	}
		// ???n?@?????e
		if (!pPlayer->IsOutMap() && !pPlayer->IsDead())
		{
			pPlayer->PlayerCastResultNotify(1);
		}
abort_stop_mov:
		if (pPlayer->IsMoving())
			pPlayer->StopMoving();
		return(-1);
	}

	// ???????????
	//Sleep(2000);

	force = 0;
#ifdef USE_FORCE_PK
	if (pCast->m_nFlags & gameBOT_MARK_FLAG_FORCE)
	{
 #if (defined(USE_FORCE_PK) && defined(USE_FORCE_PK_LEVEL_LIMIT))
  #ifdef USE_FORCE_PK_NO_HISTORY_LIMIT
		if (!(pPlayer->m_nCharFlags & CHAR_HISTORY_BATTLE))	// ???v???(2011/01/26)
  #endif
 #endif
		{
 #if (defined(USE_FORCE_PK) && defined(USE_FORCE_PK_LEVEL_LIMIT))
			if (!GetServer()->IsWar())	// ????L????
			{
				if (pPlayer->GetSaveData()->plrLevel < PLAYER_USE_FORCE_PK_LEVEL)		// 2008/11/17
					return(-1);
			}
 #endif
			//
			force = 0x80000000;
			// ??d?O?_?i?HPK
			pStg = gameStageGetPtr(pPlayer->GetMapCode());
			if (!pStg)
				return(-1);
			if (!(pStg->gstgFlag & gstgFLAG_FORCE_PK))
			{
			//	pPlayer->PlayerAttackErrorCount(22);
				return(-1);
			}
		}
	}
#endif

	if (!(pCast->m_nFlags & gameBOT_MARK_FLAG))	// ?~??
		pPlayer->SetBotRecord(BOT_REASON_CAST_ERR);
	no_input = 0;
	if (pCast->m_nFlags & gameBOT_MARK_FLAG_NO_INPUT)
	{
		no_input++;
	}
#ifdef CHECK_BOT_SO_SYSTEM
	pPlayer->CheckBOT(pCast->m_nFlags & 0xfff);
#endif

	if (!pPlayer->CheckCan_Attack_Cast_Move(2))
	{
//GetServer()->Log("Cast stop 1");
		pPlayer->PlayerAttackErrorCount(23);
		goto abort_stop_mov;
	}
	if (!pPlayer->Soul_CanAttack())
	{
	//	pPlayer->PlayerAttackErrorCount(24);
		goto abort_stop_mov;
	}

//	if (pPlayer->IsCasting())
//	{
//		GetServer()->Log("Is casting !");
//		return(-1);
//	}

	//if (!pPlayer->IsAvailable() || pPlayer->IsNPCTalk())
	//{
	//	pPlayer->PlayerActErrorLogCount();
	//	return(-1);
	//}

	//if (!pPlayer->IsAvailable())
	//	return(-1);

#ifdef USE_COOL_DOWN_SYSTEM
	if (CoolDown_CheckSkill(pCast->m_nMagicID, pPlayer->GetUID(), 0))
	{
 #ifndef USE_PACKAGE_DATA
		GetServer()->Log("DEBUG: Use skill %d in cold down phase(%d,%s)", pCast->m_nMagicID, pPlayer->GetUID(), pPlayer->GetName());
 #endif
		goto abort_stop_mov;
	}
#endif

//	pPlayer->ClearSuperMode();
//	pPlayer->ExitNPCTalk();
	pMagic = gameMagic_GetPointer(pCast->m_nMagicID, pPlayer->GetCharData()->plrBattleSkillMagic);
	if(pMagic == NULL)
	{
		GetServer()->Log("ERROR: CB_PlayerCastMagic: No this magic(uid=%d, MagicID=%d)", pPlayer->GetUID(), pCast->m_nMagicID);
		goto abort_stop_mov;
	}

	//if (pCast->m_nMagicID >= magic_RUN)		// ?Q???
	if (Inner_Is_Passive_Skill(pCast->m_nMagicID))
	{
		if(pPlayer->IsOutMap() || pPlayer->IsDead() || pPlayer->IsBlocking()/* || IsCasting()*/)
		{
			pPlayer->PlayerCastResultNotify(0);
			return(-1);
		}
		//
		force = 0;		// 2010/03/09
		//
		switch(pCast->m_nMagicID)
		{
		case magic_RUN:
			// ?????]?B/????
			if(pPlayer->CheckRunSkill())
			{
				// LRG: ??????
				pPlayer->StopMoving();
				//
				if(pPlayer->IsRun())
				{
//					GetServer()->Log("Player(%d) change to walk mode", pPlayer->GetUID(), pCast->m_nMagicID);
					pPlayer->SetRunMode(false);
					pPlayer->GetSaveData()->plrSwitchMode &= ~gameSKILL_MODE_RUN;
				}
				else
				{
//					GetServer()->Log("Player(%d) change to run mode", pPlayer->GetUID(), pCast->m_nMagicID);
					pPlayer->SetRunMode(true);
					pPlayer->GetSaveData()->plrSwitchMode |= gameSKILL_MODE_RUN;
				}
#ifdef USE_AUTO_USE_SKILL
send_msg:
#endif
				pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_SWITCH_MODE, 0, pPlayer->GetUID(), pPlayer->GetSaveData()->plrSwitchMode);

				pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			}
			else
				GetServer()->Log("Player(%d) does not have run skill", pPlayer->GetUID());
			pPlayer->m_nCastInputDelay = FAST_CAST_DELAY_TIME;
#ifdef USE_AUTO_USE_SKILL
d_ok:
#endif
			//pPlayer->StopMoving();
			pPlayer->PlayerCastResultNotify(0);
			return(0);
			break;

		case magic_SITE:	// ??U
		//	if (!pPlayer->IsRide())
			{
//GetServer()->Log("Player(%d) switch crouch mode", pPlayer->GetUID());
			//	pPlayer->StopMoving();
				pPlayer->Crouch();
		/*		if (pPlayer->IsCrouch())
				{
					pPlayer->GetSaveData()->plrSwitchMode |= gameSKILL_MODE_SITE;
				}
				else
					pPlayer->GetSaveData()->plrSwitchMode &= ~gameSKILL_MODE_SITE;
				pMsg			= pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
			//	pMsg->m_nSize	= sizeof(struct MAP_UPDATE_PLAYER_DATA_PART2);
				pMsg->m_nSize	= pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_SWITCH_MODE;
				pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = pPlayer->GetUID();
				pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData2 = pPlayer->GetSaveData()->plrSwitchMode;
		*/
			}
			pPlayer->PlayerCastResultNotify(0);
			return(0);
			break;
#ifdef USE_AUTO_USE_SKILL
		case magic_AUTOUSE_HP:
			if(pPlayer->GetSkillLevel(magic_AUTOUSE_HP) > 0)
			{
				pPlayer->GetSaveData()->plrSwitchMode ^= gameSKILL_MODE_AUTO_HP;
				goto send_msg;
			}
			goto d_ok;
			break;

		case magic_AUTOUSE_MP:
			if(pPlayer->GetSkillLevel(magic_AUTOUSE_MP) > 0)
			{
				pPlayer->GetSaveData()->plrSwitchMode ^= gameSKILL_MODE_AUTO_MP;
				goto send_msg;
			}
			goto d_ok;
			break;
#endif
	/*	case magic_AUTO_ATTACK:
			if(pPlayer->GetSkillLevel(magic_AUTO_ATTACK) > 0)
			{
				pPlayer->GetSaveData()->plrSwitchMode ^= gameSKILL_MODE_AUTO_ATTACK;
				goto send_msg;
			}
			goto d_ok;
			break;

		case magic_AUTO_ATTACKBACK:
			if(pPlayer->GetSkillLevel(magic_AUTO_ATTACKBACK) > 0)
			{
				pPlayer->GetSaveData()->plrSwitchMode ^= gameSKILL_MODE_AUTO_OFFENSE;
				goto send_msg;
			}
			goto d_ok;
			break;
	*/
		case magic_CHANGE_WEAPON:		// ???Z??
			GetServer()->CB_ChangeWeapon( NULL, 0, nClientID, pComm );
			//pPlayer->m_nCastInputDelay = FAST_CAST_DELAY_TIME;
			pPlayer->m_nCastInputDelay = FAST_CAST_DELAY_TIME_LONG;
			//
			pPlayer->PlayerCastResultNotify(0);
			return(0);		// ????????
			break;

		default:
			//pPlayer->PlayerCastResultNotify(0);
			return(0);
			break;
		}
	}
	//
	switch (pPlayer->GetMagicTargetType(pCast->m_nMagicID))
	{
	case effTARGET_PARTY_ALL:			// 钉ね砰
		if (pPlayer->PartyGetTotal() < 1)
			goto cast_abort;
		// ?????A????O magicSelectType_NONE?A???`?B?z
		force = 0;		// 2010/03/09
		break;
	case effTARGET_PARTY_SINGLE:		// 钉ね虫砰
		if (pPlayer->PartyGetTotal() < 1)
			goto cast_abort;
		if (!pPlayer->PartyFind(pCast->m_nTargetUID))
			goto cast_abort;
		//
		force = 0;		// 2010/03/09
		goto do_cast;					// ?????Z??
		break;
	case effTARGET_MATE:				// ?t??
		if (!pPlayer->GetSaveData()->plrMarry_UID)
			goto cast_abort;
		// ?N?o???
		long l_time;

		l_time = pPlayer->GetSaveData()->plrMarrySkillColdTime - CMapServer::GetServer()->GetLoopTime();
 #ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("DEBUG: Mate cold time(%s,%d)", pPlayer->GetName(), l_time);
 #endif
		if (l_time > 0)
		{
			long size;
			struct MAP_UPDATE_PLAYER_DATA_PART2 nPartData;

			nPartData.Reset();
			nPartData.Add(UPART2_TYPE_MATE_SKILL_NOTIFY, 1, l_time);

			size = nPartData.GetSize();
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nPartData, size, 0);
			//
			goto cast_abort;
		}
		//
		force = 0;		// 2010/03/09
		goto do_cast;					// ?????Z??
		break;
	}

	switch(pMagic->magicTargetSelectType)
	{
	case magicSelectType_NONE:
		// ?H??v??????
//		GetServer()->Log("Player(%d) cast magic(%d)", pPlayer->GetUID(), pCast->m_nMagicID);
		force = 0;		// 2010/03/09
		break;

	case magicSelectType_OBJECT:
		if (no_input)
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("ERROR: CB_PlayerCast: %s No Input", pPlayer->GetName());
#endif
			//CMapServer::GetServer()->BOT_NotifyGM(pPlayer, BOT_REASON_GM_NOTICE_BOT1);
			// ???^???A?? TracePlayer ????
			//	pPlayer->PlayerActErrorLogCount(BOT_REASON_GM_NOTICE_BOT1);
		}
		// ???@???
		if (pCast->m_nTargetUID > gameMAX_ENEMY_UID)
		{
			pPlayer->SetBotRecord(BOT_REASON_ATTACK_ERR);
			pPlayer->PlayerAttackErrorCount(25);
			goto abort_stop_mov;
		}
		//
		i = 0;
		if (pMagic->magicEffect.effAntiStatus & effFun_RESURRECT)
		{
			pObject = GetServer()->FindObjectByUID(pCast->m_nTargetUID, CMapChar::CLASS_ID);
			if(pObject == NULL)
			{
				pPlayer->PlayerAttackErrorCount(26);
				//
cast_abort:		pPlayer->PlayerCastResultNotify(0);
				goto abort_stop_mov;
			}
			if(!pObject->IsKindOf(CMapChar::CLASS_ID))
			{
				pPlayer->PlayerAttackErrorCount(27);
				goto cast_abort;
			}
			pTarget = (CMapChar *)pObject;
			i++;
		}
		else
			pTarget = GetServer()->FindAndCheckCharExistByUID(pCast->m_nTargetUID, CMapChar::CLASS_ID);
		if (pTarget == NULL)
		{
//			GetServer()->Log("ERROR: CB_PlayerCastMagic: тぃ琁猭ヘ夹, PlayerUID = %d, TargetUID = %d", pPlayer->GetUID(), pCast->m_nTargetUID);
			pPlayer->PlayerAttackErrorCount(28);
			goto cast_abort;
		}
		if (pPlayer->GetMapCode() != pTarget->GetMapCode())
		{
			pPlayer->PlayerAttackErrorCount(29);
			goto cast_abort;
		}
		//
		if (pTarget->m_nCharFlags & CHAR_TRAP)
			return(0);
		//
		if (IsEnemyUID(pCast->m_nTargetUID))		// 2010/03/09 ??H???j??P?w
			force = 0;
		//if(!pPlayer->IsMagicTarget(pCast->m_nMagicID, pTarget->GetHandle()))
		if(!pPlayer->IsMagicTarget_Fast(pCast->m_nMagicID, pTarget, i, force))
		{
			GetServer()->Log("ERROR: Player(%d) can't cast magic(%d) to char(%d)", pPlayer->GetUID(), pCast->m_nMagicID, pTarget->GetUID());
			pPlayer->PlayerAttackErrorCount(30);
			goto cast_abort;
		}
		if (pTarget->pStatus_IsEnable(effFun_INVISIBLE))
			goto cast_abort;

//		GetServer()->Log("Player(%d) cast magic(%d) to char(%d)", pPlayer->GetUID(), pCast->m_nMagicID, pTarget->GetUID());
		break;

	case magicSelectType_POS:
		if (no_input)
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("ERROR: CB_PlayerCast: %s No Input", pPlayer->GetName());
#endif
			//CMapServer::GetServer()->BOT_NotifyGM(pPlayer, BOT_REASON_GM_NOTICE_BOT1);
			// ???^???A?? TracePlayer ????
			//	pPlayer->PlayerActErrorLogCount(BOT_REASON_GM_NOTICE_BOT1);
		}
		// ??d??
//		GetServer()->Log("Player(%d) cast magic(%d) to [%d,%d]", pPlayer->GetUID(), pCast->m_nMagicID, pCast->m_TargetPos.x, pCast->m_TargetPos.y);
		force = 0;		// 2010/03/09
		break;
	}
	//
do_cast:
	dly = 0;
	if(pPlayer->CastMagic(pCast, &dly, &need_retry, force))
	{
		// ????]?k?????]?w????
		i = pMagic->magicType;
		if (i & plrSKILL_GEN_MAGIC)			// ?Z?N??
		{
			pPlayer->m_nCastInputDelay = FAST_CAST_DELAY_TIME_LONG;
		}
		else if (i & plrSKILL_SUPER)		// ゲ炳м
		{
			if (pCast->m_nMagicLevel <= 2)
			{
				//pPlayer->m_nCastInputDelay = FAST_CAST_DELAY_TIME;
				pPlayer->m_nCastInputDelay = (short)gameGetSkillAfterAttackDelay(pCast->m_nMagicID, pCast->m_nMagicLevel, FAST_CAST_DELAY_TIME, pPlayer->GetCharData()->plrBattleSkillMagic);
			//	if (pPlayer->m_nCastInputDelay)	// ?]?w?????@(?p????)
			//		pPlayer->SetActionBlockTime(pPlayer->m_nCastInputDelay);
			}
			else
			{
				//pPlayer->m_nCastInputDelay = FAST_CAST_DELAY_TIME_LONG;
				pPlayer->m_nCastInputDelay = (short)gameGetSkillAfterAttackDelay(pCast->m_nMagicID, pCast->m_nMagicLevel, FAST_CAST_DELAY_TIME_LONG, pPlayer->GetCharData()->plrBattleSkillMagic);
			}
			if (pPlayer->m_nCastInputDelay)	// ?]?w?????@(?p????)
				pPlayer->SetActionBlockTime(pPlayer->m_nCastInputDelay);
//GetServer()->Log("DEBUG: Player(%s) cast magic delay(%d)", pPlayer->GetName(), pPlayer->m_nCastInputDelay);
		}
		else
			pPlayer->m_nCastInputDelay = FAST_CAST_DELAY_TIME;
#ifdef CHECK_ATTACK_FREQUENCE
		//pPlayer->m_nCastInputDelayRecord = pPlayer->m_nCastInputDelay;
		if (dly > 50)
		{
			dly = (dly-50) & 0xffff;
		}
		pPlayer->m_nCastInputDelayRecord = (unsigned short)dly;
//GetServer()->Log("Player(%d) cast magic delay(%d)", pPlayer->GetUID(), dly);
#endif
		//
		if (pMagic->magicEffect.effFunction & (effFun_ATTACK | effFun_ADD_HP))
			pPlayer->ClearNoAttackMode();
	}
	else
	{
//		GetServer()->Log("Player(%d) fail cast magic(%d)", pPlayer->GetUID(), pCast->m_nMagicID);
		if(!pPlayer->IsOutMap() && !pPlayer->IsDead())
		{
			pPlayer->PlayerCastResultNotify(need_retry);
		}
	}

	return(0);
}

void CMapServer::CB_PlayerCancelMagic(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	CMapPlayer *	pPlayer;
	long is_deleted;

	pPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pPlayer == NULL)
	{
	//	// ?D???`?s?u, ?j?????_?s?u
	//	if (!is_deleted)
	//	{
	//		GetServer()->KickClient(nClientID);
	GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
	//	}
		return;
	}

	pPlayer->CancelMagic(true, 1);

	return;
}

void CMapServer::PlayerResurrectToSavePos(CMapPlayer * pPlayer)
{
	CMapCtrl * pCtrl;
	long map_code;
	long map_x, map_y;

	if (pPlayer->IsDead())
	{
		map_code = pPlayer->GetMapCode();
		// ?Y??a???e?H??????e?]?w???D
		pCtrl = FindMap(map_code);
		if (pCtrl)
		{
			if (pPlayer->IsMoving())
				pPlayer->StopMoving();
			//
			switch(pCtrl->m_pStageData->gstgMode)
			{
			case mapMode_FreeOPK:				// ???PK
			case mapMode_OrganizePK:			// ???PK
				if (pPlayer->ChangeMapInStageMapPos())
					return;
				break;
			case mapMode_HistoryBattle:			// ???v???
//history_teleport:
				if (GetServer()->m_nHistoryManager.GetHistoryPos(pPlayer, map_code, &map_x, &map_y))
				{
					pPlayer->ChangeMap(map_code, map_x, map_y);
				}
				else
				{
					if (pPlayer->ChangeMapInStageMapPos())
						return;
				}
				break;
			//default:
			//	if (pCtrl->m_pStageData->gstgHistoryBattleType)
			//		goto history_teleport;
			//	break;
			}
		}
		//
	//	map_code = pPlayer->GetSaveData()->plrSaveMapCode;
	//	map_x = pPlayer->GetSaveData()->plrSavePosX;
	//	map_y = pPlayer->GetSaveData()->plrSavePosY;
	//	if (CMapServer::GetServer()->ChangeSaveMap(pPlayer, map_code, map_x, map_y, 0))
	//	{
	//		pPlayer->ResurrectImmed(0);
	//	}
	//	else
		{
			//
			//if (!pPlayer->ChangeMapInStageMapPos())
			{
				pPlayer->Resurrect();
			}
		}
	}
}

// ?_???^?????I
long CMapServer::CB_PlayerResurrect(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_PLAYER_RESURRECT *	pResurrect = (struct MAP_PLAYER_RESURRECT *)pBuffer;
	CMapPlayer *	pPlayer;

	if(nLen != sizeof(struct MAP_PLAYER_RESURRECT))
	{
		GetServer()->Log("ERROR: CB_PlayerResurrect: Wrong buffer length!");
		return(-1);
	}

	pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	if(pPlayer == NULL)
	{
	//	// ?D???`?s?u, ?j?????_?s?u
	//	GetServer()->KickClient(nClientID);
	GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
		return(-1);
	}

//	GetServer()->Log("Player(%d, %s) resurrect", pPlayer->GetUID(), pPlayer->GetSaveData()->plrName);

	if (pPlayer->IsDead())
	{
		pPlayer->CWar_ActiveResurrectTick();
		if (!pPlayer->CWar_IsResurrectWait())	// 狦礚確┑
		{
			GetServer()->PlayerResurrectToSavePos(pPlayer);
		}
	}
	return(0);
}

// ?P?N??a?_??
long CMapServer::CB_ResurrectConfirm( char * pBuffer, int nLen, int nClientID, proto_COMM * pComm )
{
	struct MAP_USE_ITEM* pData = (struct MAP_USE_ITEM*)pBuffer;
	CMapPlayer*	pPlayer;
	long perc;
	
	pPlayer = GetServer()->FindPlayerByClientID( nClientID );

	if( pPlayer == NULL )
	{
		return(-1);
	}

	unsigned long uid = pPlayer->GetUID();

	if( !pPlayer->IsDead() )
	{
	//	GetServer()->Log( "ERROR: CB_ResurrectConfirm: Player(name=%s,uid=%u) is not dead!", pPlayer->GetName(), uid );
		return(-1);
	}

	//???D??_????v
	if(pData->itemID)
	{
		//check item data
		if(pData->targetPlrUID != pPlayer->GetUID())
			return -1;

		struct itemDATA_SAVE* pSourceItemSave = ::CheckPlayerCarryItem(pPlayer, pData->itemID, pData->itemUID.iUID_Time, pData->itemUID.iUID_Serial, pData->sourcePosID);
		if(pSourceItemSave == NULL)
		{
			GetServer()->Log("ERROR: CB_ResurrectConfirm: 珇籔ㄓ方矪ぃ才! Player: uid=%u, name=%s", pPlayer->GetUID(), pPlayer->GetName());
			return(-1);
		}

		//??d?O?_?????w?????~
		long iPercent = gameItem_GetResurrectPerc(pData->itemID);
#ifdef USE_COOL_DOWN_SYSTEM
		if(iPercent <= 0 || CoolDown_CheckItem(pData->itemID, pPlayer->GetUID(), 0))
#else
		if(iPercent <= 0)
#endif
			return -1;
		
		pPlayer->SetResurrectQueryTime(GetServer()->GetLoopTime());
		pPlayer->SetResurrectHpPercent(iPercent);
		
		if(::UpdatePlayerCarryItem(pPlayer, pData->sourcePosID, -1, true, LOGTYPE_ITEM_USE) < 0)
			return -1;

#ifdef USE_COOL_DOWN_SYSTEM
		CoolDown_CheckItem(pData->itemID, pPlayer->GetUID(), 1);
#endif
	}

	if(GetServer()->GetLoopTime() > (long)(pPlayer->GetResurrectQueryTime() + 60)) //筄(虫:)
	{
		pPlayer->SetResurrectQueryTime( 0 );
		pPlayer->SetResurrectHpPercent( 0 );
		pPlayer->SendMsgToClientByID( 20593 ); //確ア毖筄莱
		return(-1);
	}

	pPlayer->CWar_ClearResurrectTick();

	perc = pPlayer->GetResurrectHpPercent();
	pPlayer->ResurrectImmed( perc );
	pPlayer->SetResurrectQueryTime( 0 );
	pPlayer->SetResurrectHpPercent( 0 );

	if(pData->itemID)
		GetServer()->Log("Player(%u) resurrect by item(%d) ...", uid, perc);
	else
		GetServer()->Log("Player(%u) resurrect by magic(%d) ...", uid, perc);

	// ?_???L????
	pPlayer->m_nSuperTime = 15*1000;
	if (pPlayer->GetSaveData()->plrWarSuperTimes < WAR_TELEPORT_SUPER_TIMES+1)
	{
		pPlayer->GetSaveData()->plrWarSuperTimes = WAR_TELEPORT_SUPER_TIMES+1;
		pPlayer->AutoSaveCharData();
	}
	// ?s?? Client ?_??
	CMapCharMsg * pMsg;
	pMsg			= pPlayer->ReplaceMsg( PROTO_MAP_CHAR_RESURRECT, CMapCharMsg::SEND_TO_ALL );
	pMsg->m_nSize	= sizeof(struct MAP_CHAR_RESURRECT);
	pMsg->m_Msg.m_Resurrect.m_nUID = uid;
	//
	pPlayer->UpdatePlayerHPMP(); 

	return(0);
}


//CB_PlayerRebirth
long CMapServer::CB_PlayerRebirth(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
#ifdef _DEBUG
	GetServer()->Log( "Enter CB_PlayerRebirth" );
#endif
	struct MAP_USE_ITEM* pData = (struct MAP_USE_ITEM*)pBuffer;
	CMapPlayer * pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	if(pPlayer == NULL)
	{
		GetServer()->Log( "ERROR: CB_PlayerRebirth: map player is null!" );
		return -1;
	}

	if(pPlayer->m_nCastInputDelay || pPlayer->CheckPlayerNoSave()
		|| pPlayer->IsDead() || pPlayer->IsAttacking() || pPlayer->IsCasting())
	{
		return -1;
	}
	
	//
	plrDATA_SAVE* pSave = pPlayer->GetSaveData();

	int iSourcePos = *(int*)pBuffer;
	int iLevelChange, iPointChange, iItemNeed;
	long itemConsume;

	if(gamePlayerCanRebirth(pSave->plrLevel, pSave->plrRebirth, &iLevelChange, &iPointChange, &iItemNeed) == false)
		return -1;

	itemConsume = gameGetRebirthItemConsume((int)pSave->plrRebirth);

	//???D??
	if(iItemNeed)
	{
		struct itemDATA_SAVE* pSourceItemSave = NULL;

		if(pData->targetPlrUID != pPlayer->GetUID() && pData->targetPlrUID != 0)
			return -1;

		if(pData->itemID == (unsigned short)iItemNeed)
		{
			pSourceItemSave = ::CheckPlayerCarryItem(pPlayer, pData->itemID, pData->itemUID.iUID_Time, pData->itemUID.iUID_Serial, pData->sourcePosID);
		}

		if(pSourceItemSave == NULL)
		{
			long slot;
			for(slot = 0; slot < gameMAX_CARRYITEM; ++slot)
			{
				pSourceItemSave = ::GetPlrItemPtr(pPlayer, posITEMBAG, slot);
				if(pSourceItemSave && pSourceItemSave->m_Item.itemCode == (unsigned short)iItemNeed)
				{
					pData->sourcePosID = slot;
					break;
				}
				pSourceItemSave = NULL;
			}
		}

		if(pSourceItemSave == NULL)
			return(-1);

		//??d?O?_?????w?????~
#ifdef USE_COOL_DOWN_SYSTEM
		if(iItemNeed != (int)pSourceItemSave->m_Item.itemCode || CoolDown_CheckItem(pSourceItemSave->m_Item.itemCode, pPlayer->GetUID(), 0))
#else
		if(iItemNeed != (int)pSourceItemSave->m_Item.itemCode)
#endif
			return -1;

		if(itemConsume)
		{
			if(::UpdatePlayerCarryItem(pPlayer, pData->sourcePosID, -1, true, LOGTYPE_ITEM_USE) < 0)
				return -1;

#ifdef USE_COOL_DOWN_SYSTEM
			CoolDown_CheckItem(pSourceItemSave->m_Item.itemCode, pPlayer->GetUID(), 1);
#endif
		}
	}

	//?p?????
	pSave->plrLevel += iLevelChange;
	pSave->plrAttrPoint += iPointChange;
	if (pSave->plrAttrPoint > gameMAX_ATTR_POINT)
		pSave->plrAttrPoint = gameMAX_ATTR_POINT;
	
	pPlayer->GetCharData()->plrLevelUpExp = gameServerGetLevelUpExp(pSave->plrLevel);
	++pSave->plrRebirth;
	::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());

	//?e?X????
	struct plrDATASHOWSELF Self;
	::memset(&Self, 0, sizeof(plrDATASHOWSELF));
	::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
	::msgSendData( nClientID, pComm->pcUID, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );

	//
	pPlayer->UpdateShowData();
	pPlayer->UpdatePlayerDDE_Login();
	pPlayer->SaveCharData();

	return 0;
}

/*
long CMapServer::CB_PlayerCrouch(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_PLAYER_CROUCH *	pCrouch = (struct MAP_PLAYER_CROUCH *)pBuffer;
	CMapPlayer *	pPlayer;

	if(nLen != sizeof(struct MAP_PLAYER_CROUCH))
	{
		GetServer()->Log("ERROR: CB_PlayerCrouch: Wrong buffer length!");
		return(-1);
	}

	pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		GetServer()->KickClient(nClientID);
		GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
		return(-1);
	}

	GetServer()->Log("Player(%d) Crouch", pPlayer->GetUID());

	if(pPlayer->IsDead())
		pPlayer->Crouch();

	return(0);
}
*/
// **************************** ?t??\?? ****************************

long CMapServer::CB_SendMessageToClient(char * pBuffer, int nLen, int nID, proto_COMM * pComm)
{
	struct MAP_SEND_MESSAGE_TO_CLIENT *	pSendMsg = (struct MAP_SEND_MESSAGE_TO_CLIENT *)pBuffer;
//	struct MAP_SYSTEM_MESSAGE	SystemMsg;
	CMapPlayer *				pPlayer;
	unsigned long uid;
	long is_delete;

//	GetServer()->Log("Send message to Player(%d)", pSendMsg->m_nUID);

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pSendMsg->m_nUID, CMapPlayer::CLASS_ID, &is_delete);
	//if (is_delete)	// ?O?_?n?j????o?????e?T?????
	//{
	//}
	if(pPlayer == NULL)
	{
		// ?|?]?????a?????a???_?????a
		//GetServer()->Log("ERROR: CB_SendMessageToClient: тぃ產 PlayerUID = %d", pSendMsg->m_nUID);
		//
		uid = pSendMsg->m_nUID;
		pSendMsg->m_nUID = pSendMsg->m_nTalkerUID;
		GetServer()->kimKeepData(uid, PROTO_MAP_SYSTEM_MESSAGE, (char *)pSendMsg, pSendMsg->GetSize());
		return(-1);
	}

	switch(pSendMsg->m_nChannel)
	{
	case gameChannel_4:		// ?K?W
		// ?????W??
		//
		if (gameFriend_CheckInBlackList(pPlayer->GetFriendData(), pSendMsg->m_nTalkerUID))
		{
		//	GetServer()->Log("WARNING: CB_SendMessageToClient: ???a?b??W??(%d)", pSendMsg->m_nTalkerUID);
			return(0);
		}
		//
		//
		break;
	}

//	SystemMsg.m_nUID			= pSendMsg->m_nTalkerUID;
//	SystemMsg.m_nChannel		= pSendMsg->m_nChannel;
//	SystemMsg.m_nMessageSize	= pSendMsg->m_nMessageSize;
//	::memcpyT(SystemMsg.m_TalkerName, pSendMsg->m_TalkerName, gameMAX_PLAYER_NAME_SIZE + 1);
//	::memcpyT(SystemMsg.m_Message, pSendMsg->m_Message, pSendMsg->m_nMessageSize + 1);
//	::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_SYSTEM_MESSAGE, (char *)&SystemMsg, SystemMsg.GetSize(), 0);
	pSendMsg->m_nUID			= pSendMsg->m_nTalkerUID;
	::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_SYSTEM_MESSAGE, (char *)pSendMsg, pSendMsg->GetSize(), 0);

	return(0);
}

void CMapServer::CB_SendMessageToClientGroup(char * pBuffer, int nLen, int nID, proto_COMM * pComm)
{
	struct MAP_SEND_MESSAGE_TO_CLIENT_GROUP * pData;
	long i, size;
#ifdef USE_ALLY_CHANNEL_ORG_NAME
	CMapPlayer * pPlayer;
	unsigned long nUID;
	unsigned long nTalkerUID;
#endif

	pData = (struct MAP_SEND_MESSAGE_TO_CLIENT_GROUP *)pBuffer;
	//
	size = pData->nMsgData.GetSize();
#ifdef USE_ALLY_CHANNEL_ORG_NAME
	if (pData->nMsgData.m_nChannel == gameChannel_6)
	{
		nTalkerUID = pData->nMsgData.m_nTalkerUID;
		pData->nMsgData.m_nTalkerUID = pData->nOrgUID;	// ノ逆魁祇杠瓁刮UID
		//
		for (i=0; i<pData->nPlayerCount; i++)
		{
			nUID = pData->nPlayerUID[i];
			pData->nMsgData.m_nUID = nTalkerUID;
			//
			pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(nUID, CMapPlayer::CLASS_ID);
			if (!pPlayer)
			{
				GetServer()->kimKeepData(nUID, PROTO_MAP_SYSTEM_MESSAGE, (char *)&pData->nMsgData, pData->nMsgData.GetSize());
			}
			else
			{
				::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_SYSTEM_MESSAGE, (char *)&pData->nMsgData, pData->nMsgData.GetSize(), 0);
			}
		}
	}
	else
#endif
	{
		for (i=0; i<pData->nPlayerCount; i++)
		{
			pData->nMsgData.m_nUID = pData->nPlayerUID[i];
			GetServer()->CB_SendMessageToClient((char *)&pData->nMsgData, size, nID, pComm);
		}
	}
}

void CMapServer::CB_UpdateArmyChannel(char * pBuffer, int nLen, int nID, proto_COMM * pComm)
{
	struct MAP_UPDATE_ARMY_CHANNEL_DATA * pData;
	CMapPlayer *				pPlayer;
	unsigned long uid;
	long is_delete;

	pData = (struct MAP_UPDATE_ARMY_CHANNEL_DATA *)pBuffer;
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->nPlayerUID, CMapPlayer::CLASS_ID, &is_delete);
	if(pPlayer == NULL)
	{
		uid = pData->nPlayerUID;
		GetServer()->kimKeepData(uid, PROTO_MAP_UPDATE_ARMY_CHANNEL_DATA, (char *)pData, nLen);
		return;
	}
	::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_UPDATE_ARMY_CHANNEL_DATA_TO_CLIENT, (char *)pData, nLen, 0);
}

void CMapServer::CB_UpdateArmyChannelGroup(char * pBuffer, int nLen, int nID, proto_COMM * pComm)
{
	struct MAP_UPDATE_ARMY_CHANNEL_DATA_GROUP * pData;
	long i, size;

	pData = (struct MAP_UPDATE_ARMY_CHANNEL_DATA_GROUP *)pBuffer;
	//
	size = sizeof(pData->nAllyData);
	for (i=0; i<pData->nPlayerCount; i++)
	{
		pData->nAllyData.nPlayerUID = pData->nPlayerUID[i];
		GetServer()->CB_UpdateArmyChannel((char *)&pData->nAllyData, size, nID, pComm);
	}
}

// Pass Login ?????Client
void CMapServer::CB_LoginGetArmyDataResult(char * pBuffer, int nLen, int nID, proto_COMM * pComm)
{
	struct LOGIN_GET_ARMY_DATA_RESULT_PASS_MAP * pData;
	CMapPlayer * pPlayer;

	pData = (struct LOGIN_GET_ARMY_DATA_RESULT_PASS_MAP *)pBuffer;
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->uid, CMapPlayer::CLASS_ID);
	if (pPlayer)
	{
//GetServer()->Log("Trace: Get army full data to Player(%s)", pPlayer->GetName());
		::msgSendData( pPlayer->GetClientID(), pComm->pcUID, PROTO_LOGIN_MAP_GET_ARMY_DATA_RESULT, (char *)&pData->nResult, sizeof(pData->nResult), msgSEND_FLAG_COMPRESS );
	}
}

long CMapServer::CB_SendChatMessageToClient(char * pBuffer, int nLen, int nID, proto_COMM * pComm)
{
	struct MAP_SEND_CHAT_MESSAGE_TO_CLIENT *	pSendMsg = (struct MAP_SEND_CHAT_MESSAGE_TO_CLIENT *)pBuffer;
	CMapPlayer *	pPlayer;

//	GetServer()->Log("Send chat message to Player(%d)", pSendMsg->m_nPlayerUID); //xiun 04/02/03

//	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pSendMsg->m_nUID);
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pSendMsg->m_nPlayerUID); //xiun 04/02/03
	if(pPlayer == NULL)
	{
//GetServer()->Log("ERROR: CB_SendMessageToClient: тぃ產 PlayerUID = %d", pSendMsg->m_nUID);
//	GetServer()->Log("ERROR: CB_SendChatMessageToClient: тぃ產 PlayerUID = %d", pSendMsg->m_nPlayerUID); //xiun 04/02/03
		return(-1);
	}

	::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_CHAT_MESSAGE, (char *)pSendMsg, pSendMsg->GetSize(), 0);

	return(0);
}

long CMapServer::CB_SendMessageFromClient(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_SEND_MESSAGE_FROM_CLIENT *	pSendMsg = (struct MAP_SEND_MESSAGE_FROM_CLIENT *)pBuffer;
	CMapPlayer *	pPlayer;

//	GetServer()->Log("Send message from Player(%d)", pSendMsg->m_nUID);

	//pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pSendMsg->m_nUID);
	pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	if(pPlayer == NULL)
	{
	//GetServer()->Log("ERROR: CB_SendMessageToClient: тぃ產 PlayerUID = %d", pSendMsg->m_nUID);
		return(-1);
	}

//	if (pSendMsg->m_nChannel & gameChannel_BOT)		// ?~??
//	{
//		pPlayer->SetBotRecord(BOT_REASON_HACKER_CLIENT, 1);
//		pSendMsg->m_nChannel &= ~gameChannel_BOT;
//	}

#if defined MSG_EMBED_ITEM_INFO
	if (pSendMsg->m_Message[0] == 0x00 && pSendMsg->m_Message[1] == 0x0a)
	{
		int is_wawa = 0;
		itemDATASHOWSELF* piSelf = (itemDATASHOWSELF*)&pSendMsg->m_Message[2];
		itemDATA_SAVE* piData = pPlayer->GetItemDATA_SAVE(&piSelf->m_Item.itemUID, &is_wawa);
		if (piData)
		{
			if (!is_wawa)
				gameServer_Item_MakeShowSelf(piData, piSelf);
		}
		else
			return(0);
	}
#endif

	pSendMsg->m_nUID = pPlayer->GetUID();
	pPlayer->SendChatMessage(pSendMsg);

	return(0);
}

long CMapServer::CB_SavePlayerDataResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_SAVE_PLAYERDATA_RESULT *	pRes = (struct DB_SAVE_PLAYERDATA_RESULT *)pBuffer;
	CMapPlayer *	pPlayer;

	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUIDForce(pRes->lUID);
	if(pPlayer == NULL)
	{
//		GetServer()->Log("ERROR: CB_SavePlayerDataResult: тぃ產 PlayerUID = %d", pRes->lUID);
		return(-1);
	}

	pPlayer->SaveResult(pRes->lSaveProtoco, pRes->lOK);

	return(0);
}

// **************************** GM ?R?O ****************************

void CMapServer::CB_SendBroadcastMessage(char * pBuffer, int nLen, int nID, proto_COMM * pComm)
{
	long nPrivilege;
	long val;
	long is_deleted;
	CMapPlayer * pPlayer;
	struct MAP_SEND_BROADCAST_MESSAGE * pData;
	struct MAP_SYSTEM_MESSAGE nMsg;

	pData = (struct MAP_SEND_BROADCAST_MESSAGE *)pBuffer;
	pPlayer = GetServer()->FindPlayerByClientID(nID, &is_deleted);
	if(pPlayer == NULL)
	{
	//	// ?D???`?s?u, ?j?????_?s?u
	//	if (!is_deleted)
	//	{
	//		GetServer()->KickClient(nID);
	//		GetServer()->Log("ERROR: CB_SendBroadcastMessage: 獶タ盽硈絬, 眏い耞硈絬, ClientID = %d", nID);
	//	}
		return;
	}
	// ??d?O?_?? GM ?v??
	nPrivilege = 0;
	val = pPlayer->GetCharData()->plrSPCFlag;
	if ((val & (spcFLAG_GM | spcFLAG_GM2)) == (spcFLAG_GM | spcFLAG_GM2))
	{
		nPrivilege = gamePRIVILEGE_TYPE_GM3;
	}
	else if (val & spcFLAG_GM)
	{
		nPrivilege = gamePRIVILEGE_TYPE_GM1;
	}
	else if (val & spcFLAG_GM2)
	{
		nPrivilege = gamePRIVILEGE_TYPE_GM2;
	}
	//
	if (!nPrivilege)
		return;
	// ...........................
	memset(&nMsg, 0, sizeof(nMsg));
	nMsg.m_nUID = pData->m_nMapCode;
	nMsg.m_nTalkerUID = pPlayer->GetUID();
	nMsg.m_nChannel = gameChannel_Broadcast;
	msg_strncpy(nMsg.m_TalkerName, pPlayer->GetSaveData()->plrName, sizeof(nMsg.m_TalkerName));
	msg_strncpy(nMsg.m_Message, pData->m_Message, sizeof(nMsg.m_Message));
	nMsg.m_nMessageSize = strlen(nMsg.m_Message);
	GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_SYSTEM_MESSAGE, (char *)&nMsg, nMsg.GetSize(), 0);
	GetServer()->Log("Send Broadcast Message (%d)... ", pPlayer->GetUID());
}

// Goto player ???~ Login Server ???????
void CMapServer::CB_AskMapPlayerPos(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct LOGIN_ASK_MAP_PLAYER_POS * pData;
	CMapPlayer * pPlayer;
	long x, y;
	struct LOGIN_ASK_MAP_PLAYER_POS_RESULT nRet;

	pData = (struct LOGIN_ASK_MAP_PLAYER_POS *)pBuffer;
	//
	memset(&nRet, 0, sizeof(nRet));
	nRet.src_uid = pData->src_uid;
	nRet.nConnectMapID = pData->nConnectMapID;
	nRet.dest_uid = pData->dest_uid;
	//
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->dest_uid);
	if (pPlayer)
	{
		nRet.nType = pData->nType;
		if (pData->nType != 0)		// 0 = GM ???O
		{
			// ??d?O?_?i?H?????e
			// ?O?_?O????B?O?_???i??O
			//if (!IsPartyMemberTeleportStage(pPlayer->GetMapID()))
			if (!gameCheckCanTeleportToMarkMap(pPlayer->GetMapID(),pData->src_EnterStageID,pData->src_EnterStageTime))
			{
 #ifndef USE_PACKAGE_DATA
				CMapServer::GetServer()->Log("DEBUG: (%d)Goto party member but cannot mark !", pData->src_uid);
 #endif
				nRet.nType = (char)254;	// ヘ夹瓜ぃ肚癳
				GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_ASK_MAP_PLAYER_POS_RESULT, (char *)&nRet, sizeof(nRet), 0);
				return;				// ???i??e
			}
			//
 #ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG: (%d)Goto party member ask pos ok !", pData->src_uid);
 #endif
		}
		//
		nRet.nMapCode = pPlayer->GetMapID();	// pData->nMapCode;
		GetServer()->FindPos(nRet.nMapCode, pPlayer->GetPosX(), pPlayer->GetPosY(), 6*gameIconSize, 6*gameIconSize, &x, &y);
		nRet.nPosX = x;
		nRet.nPosY = y;
		GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_ASK_MAP_PLAYER_POS_RESULT, (char *)&nRet, sizeof(nRet), 0);
		//::msgSendData(nClientID, 0, PROTO_LOGIN_ASK_MAP_PLAYER_POS_RESULT, (char *)&nRet, sizeof(nRet), 0);
	}
	else
	{
		//nRet.nType = pData->nType;
		if (pData->nType != 0)		// 0 = GM ???O
		{
			nRet.nType = (char)255;		// ヘ夹ぃ(筁瓜い┪琌祅)
			//
			GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_ASK_MAP_PLAYER_POS_RESULT, (char *)&nRet, sizeof(nRet), 0);
		}
	}
}

// Goto player ??o???G
void CMapServer::CB_MapGotoPlayerResult(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct LOGIN_MAP_GOTO_PLAYER_RESULT * pData;
	CMapPlayer * pPlayer;

	pData = (struct LOGIN_MAP_GOTO_PLAYER_RESULT *)pBuffer;
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->src_uid);
	if (pPlayer)
	{
		if (pData->nType != 0)		// 0 = GM ???O
		{
			if (pPlayer->IsDead() || !pPlayer->IsReady() || !pPlayer->CanSaveData())
			{
				GetServer()->Log("ERROR: Goto player fail(%d,%s,%d)... ", pData->src_uid, pPlayer->GetName(), pData->dest_uid);
				return;
			}
			//
			switch(pData->nType)
			{
			case 255:				// ヘ夹ぃ(筁瓜い┪琌祅)
				pPlayer->SendMsgToClientByID(24214);		// ㄏノмア毖
				return;
				break;
			case 254:				// ヘ夹瓜ぃ肚癳
				pPlayer->SendMsgToClientByID(21354);
				return;
				break;
			}
			//
 #ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG: Goto player type(%d,%s)(%d)", pPlayer->GetUID(), pPlayer->GetName(), pData->nType);
 #endif
			if (pData->nType == 1)
			{
				// ?N?o???
				if (pPlayer->GetSaveData()->plrMarrySkillColdTime >= CMapServer::GetServer()->GetLoopTime())
					return;
				//
				if (pPlayer->IsGM(0))		// GM à︹玱だ牧
				{
					pPlayer->GetSaveData()->plrMarrySkillColdTime = CMapServer::GetServer()->GetLoopTime() + 60;
				}
				else
					pPlayer->GetSaveData()->plrMarrySkillColdTime = CMapServer::GetServer()->GetLoopTime() + MARRY_SKILL_COLD_TIME;
				pPlayer->AutoSaveCharData();
				//
 #ifndef USE_PACKAGE_DATA
				CMapServer::GetServer()->Log("DEBUG: Set mate skill cold down(%d,%s)", pPlayer->GetUID(), pPlayer->GetName());
 #endif
				// ??s???a????
				long size;
				struct MAP_UPDATE_PLAYER_DATA_PART2 nPartData;

				nPartData.Reset();
				nPartData.Add(UPART2_TYPE_MATE_SKILL_NOTIFY, 0, pPlayer->GetSaveData()->plrMarrySkillColdTime);
				
				size = nPartData.GetSize();
				::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nPartData, size, 0);
			}
			// ?n??X????e
			CMapServer::GetServer()->ChangeSaveMap(pPlayer, pData->nMapCode, pData->nPosX, pData->nPosY);
 #ifndef USE_PACKAGE_DATA
		GetServer()->Log("Goto player(%d, %d)(%d, %d, %d)... ", pData->src_uid, pData->dest_uid, pData->nMapCode, pData->nPosX, pData->nPosY);
 #endif
			return;
		}
		// ??s?a???m
		pPlayer->ChangeMap(pData->nMapCode, pData->nPosX, pData->nPosY);
#ifndef USE_PACKAGE_DATA
		GetServer()->Log("GM goto player(%d, %d)(%d, %d, %d)... ", pData->src_uid, pData->dest_uid, pData->nMapCode, pData->nPosX, pData->nPosY);
#endif
	}
}

void CMapServer::CB_MapGMCallPlayer(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_GM_CALL_PLAYER * pData;
	struct MAP_GM_CALL_PLAYER_RESULT nRet;
	CMapPlayer * pPlayer;

	pData = (struct MAP_GM_CALL_PLAYER *)pBuffer;
	//
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->nUID);
	if (pPlayer)
	{
		memset(&nRet, 0, sizeof(nRet));
		nRet.nGM_UID = pData->nGM_UID;
		//
		if (pPlayer->IsInShop())
		{
			nRet.nReason = nGM_CALL_PLAYER_REASON_IN_SHOP;
		}
		else if (pPlayer->IsInTrade())
		{
			nRet.nReason = nGM_CALL_PLAYER_REASON_TRADE;
		}
		else if (pPlayer->IsInStall())
		{
			nRet.nReason = nGM_CALL_PLAYER_REASON_IN_STALL;
		}
		//
		if (!nRet.nReason)
		{
			pPlayer->ChangeMap(pData->nGM_MapCode, pData->nGM_PosX, pData->nGM_PosY);
		}
		else
			GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_GM_CALL_PLAYER_RESULT, (char *)&nRet, sizeof(nRet), 0);
	}
}

void CMapServer::CB_MapGMCallPlayerResult(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_GM_CALL_PLAYER_RESULT * pData;
	CMapPlayer * pPlayer;
	struct MAP_UPDATE_PLAYER_DATA_PART2 nMsg;

	pData = (struct MAP_GM_CALL_PLAYER_RESULT *)pBuffer;
	//
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->nGM_UID);
	if (pPlayer)
	{
		memset(&nMsg, 0, sizeof(nMsg));
		
		nMsg.Reset();
		nMsg.Add(UPART2_TYPE_GM_CALL_PLAYER_RESULT, 0, pData->nReason);

		::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsg.GetSize(), 0);
	}
}

void CMapServer::CB_MapTraceServerResult(char * pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	struct MAP_TRACE_PLAYER_SERVER_RESULT * pData;
	CMapPlayer * pPlayer;
	CMapPlayer * pPlayerMonitor;

	if (!pBuffer || nLen < (int)sizeof(struct MAP_TRACE_PLAYER_SERVER_RESULT))
		return;

	pData = (struct MAP_TRACE_PLAYER_SERVER_RESULT *)pBuffer;
	//
	switch(pData->nResult)
	{
	case TRACE_RESULT_TARGET_STOP:			// Serverノ: 氨ゎ發萝ヘ夹
		pData->nResult = TRACE_RESULT_TARGET_LOGOUT;	// Client 讽Θ琌ヘ夹祅
	case TRACE_RESULT_TARGET_LOGOUT:
	case TRACE_RESULT_TARGET_GM_STOP:
		// ???G?M??
	//	if (pData->TargetUID & 0x80000000)
	//	{
	//		pPlayer = NULL;
	//		::smsDelMessageMonitor(pData->TargetUID & 0x7fffffff);
	//	}
	//	else
		{
			pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->TargetUID, CMapPlayer::CLASS_ID);
			if (pPlayer)
			{
#ifndef USE_PACKAGE_DATA
GetServer()->Log("Trace: Stop/Logout -- Target(%d,%s)", pPlayer->GetSaveData()->plrUID, pPlayer->GetName());
#endif
				::smsDelMessageMonitor(pPlayer->GetClientID());
			}
		//	else
		//		GetServer()->kimKeepData(pData->TargetUID, pComm->pcProtoco, pBuffer, nLen);
		}
		// ?????G??_???A
		pPlayerMonitor = (CMapPlayer *)GetServer()->FindObjectByUID(pData->MonitorUID, CMapPlayer::CLASS_ID);
		if (pPlayerMonitor)
		{
#ifndef USE_PACKAGE_DATA
GetServer()->Log("Trace: Stop/Logout -- Monitor(%d,%s)", pPlayerMonitor->GetSaveData()->plrUID, pPlayerMonitor->GetName());
#endif
			// ??_???????
			::msgSendData(pPlayerMonitor->GetClientID(), 0, PROTO_MAP_TRACE_PLAYER_RESULT, pBuffer, nLen, 0);
			//
			//if (pPlayerMonitor->m_nTraceLastPosX)
			if (pPlayerMonitor->m_nCharFlags & CHAR_FLAG_GM_TRACE)
			{
				pPlayerMonitor->m_nCharFlags &= ~CHAR_FLAG_GM_TRACE;
				if (pPlayerMonitor->IsOutMap())
				{
					pPlayerMonitor->MapCtrl_UnLock(1);
					pPlayerMonitor->LeaveMapBeforeChangeMap();		// ?b?????m?e??????????
					pPlayerMonitor->SetPos(pPlayerMonitor->m_nTraceLastPosX, pPlayerMonitor->m_nTraceLastPosY);
					//CMapServer::GetServer()->ChangeObjectState(pPlayerMonitor->GetHandle(), CMapObject::STATE_ENTER_MAP);
					pPlayerMonitor->EnterMap();
				}
				pPlayerMonitor->m_nTraceLastPosX = 0;
				pPlayerMonitor->m_nTraceLastPosY = 0;
			}
			//
			GetServer()->Trace_SendPlayerFullData(pPlayerMonitor->GetUID(), pPlayerMonitor);
			GetServer()->Trace_MonitorSendSightObjects(pData->MonitorUID, pPlayerMonitor);
			// ?M???[?t????
			pPlayerMonitor->ClearAccelerateRecord();
		}
		break;
	case TRACE_RESULT_TARGET_START:			// Serverノ: 砞﹚秨﹍發萝ヘ夹
		// ???G?]?w
		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->TargetUID, CMapPlayer::CLASS_ID);
		if (pPlayer)
		{
#ifndef USE_PACKAGE_DATA
GetServer()->Log("Trace: Start(%d,%s)", pPlayer->GetSaveData()->plrUID, pPlayer->GetName());
#endif
			// ?]?w??????A?]?w??????
			::smsSetMessageMonitorCallBack_Recv(InnerCB_MonitorPlayer_Recv);
			::smsSetMessageMonitorCallBack_Send(InnerCB_MonitorPlayer_Send);
			// ???h?_?u??|???M??
			::smsAddMessageMonitor(pPlayer->GetClientID(), pData->MonitorUID);
		}
		else
		{
			GetServer()->kimKeepData(pData->TargetUID, pComm->pcProtoco, pBuffer, nLen);
#ifndef USE_PACKAGE_DATA
GetServer()->Log("Trace: Start keep target hook(%d)", pData->TargetUID);
#endif
		}
		//
		// ?????G???N???A?B???X?a????B
		pPlayerMonitor = (CMapPlayer *)GetServer()->FindObjectByUID(pData->MonitorUID, CMapPlayer::CLASS_ID);
		if (pPlayerMonitor)
		{
//GetServer()->Log("Trace: Set far pos 1");
			//if (!pPlayerMonitor->m_nTraceLastPosX)
			if (!(pPlayerMonitor->m_nCharFlags & CHAR_FLAG_GM_TRACE))
			{
				pPlayerMonitor->m_nCharFlags |= CHAR_FLAG_GM_TRACE;
				//
				// ?????G???N???A
				if (pPlayer)
				{
#ifndef USE_PACKAGE_DATA
GetServer()->Log("Trace: Start send all data (%d,%s)", pPlayer->GetSaveData()->plrUID, pPlayer->GetName());
#endif
					GetServer()->Trace_SendPlayerFullData(pData->MonitorUID, pPlayer);
					GetServer()->Trace_MonitorSendSightObjects(pData->MonitorUID, pPlayer);
				}
				//
//GetServer()->Log("Trace: Set far pos 2");
				pPlayerMonitor->m_nTraceLastPosX = pPlayerMonitor->GetPosX();
				pPlayerMonitor->m_nTraceLastPosY = pPlayerMonitor->GetPosY();
				if (!pPlayerMonitor->IsOutMap())
				{
//GetServer()->Log("Trace: Set far pos 3");
					pPlayerMonitor->MapCtrl_UnLock(1);
					//CMapServer::GetServer()->ChangeObjectState(pPlayerMonitor->GetHandle(), CMapObject::STATE_LEAVE_MAP);
					pPlayerMonitor->LeaveMapBeforeChangeMap();		// ?b?????m?e??????????
					pPlayerMonitor->LeaveMap();
					pPlayerMonitor->SetPos(300000, 300000);
//GetServer()->Log("Trace: Set far pos");
					// ?M???[?t????
					pPlayerMonitor->ClearAccelerateRecord();
				}
			}
		}
		else
		{
			// ?????G???N???A
			if (pPlayer)
			{
#ifndef USE_PACKAGE_DATA
GetServer()->Log("Trace: Start send all data (%d,%s)", pPlayer->GetSaveData()->plrUID, pPlayer->GetName());
#endif
				GetServer()->Trace_SendPlayerFullData(pData->MonitorUID, pPlayer);
				GetServer()->Trace_MonitorSendSightObjects(pData->MonitorUID, pPlayer);
			}
		}
		break;
	case TRACE_RESULT_RESTORE_DATA:
/*		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->MonitorUID, CMapPlayer::CLASS_ID);
		if (pPlayer)
		{
			type = pData->nDataType;
			pData->nMsgCompressSize = 0;	// 逆㎝ DataType ノ
#ifndef USE_PACKAGE_DATA
GetServer()->Log("Trace: Restore data (%d,%s,%d)", pPlayer->GetSaveData()->plrUID, pPlayer->GetName(), type);
#endif
			GetServer()->Trace_SendPlayerFullData2(pPlayer, pData->nDataType, pData->nMsgData, pData->nMsgSize);
		}
		return;
		break;
*/
	//
//	case TRACE_RESULT_HOOK_FAIL:			// ?L?k???
//	case TRACE_RESULT_SETTING_OK:			// ?]?w??????\
//	case TRACE_RESULT_DATA_RECV_OK:			// ???e?? Server ??????
//	case TRACE_RESULT_DATA_SEND_OK:			// Server ?e??????????
//	case TRACE_RESULT_TARGET_LOGOUT:		// ??????u
//	case TRACE_RESULT_TARGET_NOT_EXIST:		// ?????s?b
//	case TRACE_RESULT_TARGET_ALREADY_SET:	// ???w?g?Q?]?w?l??
//		break;
	default:
		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->MonitorUID, CMapPlayer::CLASS_ID);
		if (pPlayer)
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_TRACE_PLAYER_RESULT, pBuffer, nLen, 0);
		break;
	}
}

void SetCNPCItemLevel(struct itemDATA_SAVE * pItemSave, long item_id, long level)
{
	long i, j;
	struct gameCNPC_ATTR_LEVEL* pAttrLevel = NULL;
	int calStr,calInt,calMind,calCon,calDex,calDamage,calDefense;
	struct itemDATA * pItem;
#ifdef NEW_NPC_ATTR
	long k;
#endif
	pItem = gameGetItemPtr(pItemSave->m_NPC.inpcItemCode);

#ifdef NEW_NPC_ATTR
	for(k = 0 ; k < gameMAX_CNPC_AttrLV_SETTING ; k++)
	{
		pAttrLevel  = gameGetCNPC_AttrLevelPtr(k);
		if (pAttrLevel && pAttrLevel->calSolCode == pItem->itemUseMagicID)
			break;
		pAttrLevel = NULL;
	}
	if (pAttrLevel)
	{
		calStr		= pAttrLevel->calStr;
		calInt		= pAttrLevel->calInt;
		calMind		= pAttrLevel->calMind;
		calCon		= pAttrLevel->calCon;
		calDex		= pAttrLevel->calDex;
		calDamage	= pAttrLevel->calDamage;
		calDefense	= pAttrLevel->calDefense;
	}
	else
#endif
	{
		calStr		= 30;
		calInt		= 10;
		calMind		= 15;
		calCon		= 30;
		calDex		= 15;
		calDamage	= 50;
		calDefense	= 50;
	}

#ifndef USE_PACKAGE_DATA		
	CMapServer::GetServer()->Log("DEBUG: どだ皌瞯 = %d,%d,%d,%d,%d,%d,%d", calStr, calInt, calMind, calCon, calDex, calDamage, calDefense);	
#endif

	if (pItemSave->m_Item.itemCode != item_id)
		return;
	// ???B?z
	if (pItemSave->m_NPC.inpcLevel < level)
	{
		//
		while (pItemSave->m_NPC.inpcLevel < level)
		{
			if (pItemSave->m_NPC.inpcLevel >= gameServerNPC_GetMaxLevel())
				break;
			//		
			pItemSave->m_NPC.inpcLevel++;
			// ?H?????t???
			for (i=0; i<gameLU_ATTR_POINT; i++)
			{
				// ?Z?O 30/100?A??z30/100?A???O10/100?A????15/100, ??15/100
				j = gameGetRandomRange(0, 100);
				if (j <= calStr)
				{
					pItemSave->m_NPC.inpcAttrStr += 1;
				}
				else if (j <= (calStr+calCon))
				{
					pItemSave->m_NPC.inpcAttrCon += 1;
				}
				else if (j <= (calStr+calCon+calInt))
				{
					pItemSave->m_NPC.inpcAttrInt += 1;
				}
				else if (j <= (calStr+calCon+calInt+calDex))
				{
					pItemSave->m_NPC.inpcAttrDex += 1;
				}
				else
				{
					pItemSave->m_NPC.inpcAttrMind += 1;
				}
			}
			// ?????@ ?????O ?? ???m?O 1?I
			/* ??????????/???????? inpcAddAttack/inpcAddDefense */
		}
	}
}

long InnerCheckIsLocalIP(char * real_ip)
{
	char tmpstr[64];

	if (real_ip)
	{
		msg_strncpy(tmpstr, real_ip, sizeof(tmpstr));
		if (!memcmp(tmpstr, "10.1.", 5) || !memcmp(tmpstr, "192.168.", 8))
			return(1);
	}
	return(0);
}

long InnerCheckLoginIP(char * real_ip)
{
	char tmpstr[64];

	if (real_ip)
	{
		msg_strncpy(tmpstr, real_ip, sizeof(tmpstr));
#if defined(GBMode_TW)
		CMapServer::GetServer()->Log("DEBUG: InnerCheckLoginIP (%s)", tmpstr);
		if(memcmp(tmpstr, "60.251.48.106", 13) == 0 || memcmp(tmpstr, "59.124.243.115", 14) == 0)
			return(1);
		if (memcmp(tmpstr, "10.1.162.118", 12) == 0)
			return(1);
#endif
	}
	return(0);
}

long CMapServer::CB_GMCommand(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_GM_COMMAND *		pCmd = (struct MAP_GM_COMMAND *)pBuffer;
	struct plrDATA_SKILL *		pSkillData;
//	struct MAP_FIX_CHAR_POS		FixMsg;
	struct MAP_UPDATE_PLAYER_DATA_PART2 Part2Msg;
	CMapPlayer *				pPlayer;
	long						nGold;
	struct itemDATA *			pItem;
	int							cnt1;
	long						nPrivilege;
	long val;
	long is_deleted;
	unsigned long exp, valexp;
	struct plrDATASHOWSELF Self;
	long x, y, map_id, map_copy_uid, px, py;
	long* lpParam = (long*)&pCmd->m_Param;
	//
	CMapPlayer * pTarget;
	unsigned long uid;
	CMapCtrl * pMapCtrl;
	CMapNPC * pNPC;
	CMapCharMsg * pMsg;
	//
	struct MAP_CITY_DATA_PACK nCityPack;
	struct plrMISSION_SAVE * pMIS;
	struct gameMISSION_DATA * pMissionTbl;

	//pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pCmd->m_nUID);
	pPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pPlayer == NULL)
	{
	//	// ?D???`?s?u, ?j?????_?s?u
	//	if (!is_deleted)
	//	{
	//		GetServer()->KickClient(nClientID);
	GetServer()->Log("ERROR: CB_ClientLogin: Data error, ClientID=%d", nClientID);
	//	}
		return(-1);
	}

	if (pCmd->m_nCommandID == MAP_GM_COMMAND_ASK_VER)
	{
		if (pCmd->m_nUID == pPlayer->GetUID())
		{
			char tmpstr[128];

			wsprintf(tmpstr, "%s,%s(%s)", __DATE__, __TIME__, ASK_VER_COUNTRY_NAME);
			if (tmpstr[0] == 0)
			{
				tmpstr[0] = 'N';
				tmpstr[1] = 'o';
				tmpstr[2] = 0;
			}
			pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
		}
		return(-1);
	}
#ifndef USE_PACKAGE_DATA		// ?????G??????x??
 #ifdef ARMY_OFFLINE_AUTO_CHANGE
	if (pCmd->m_nCommandID == MAP_GM_COMMAND_AUTO_DISMISS_ARMY)
	{
		if (pPlayer->GetSaveData()->plrOrganizeUID)
		{
			Log("DEBUG: Cannot enter now(%s)", pPlayer->GetName());
			//
			::memset( &nCityPack, 0, sizeof(nCityPack) );
			nCityPack.nType = TYPE_PACK_AUTO_DISMISS_TEST;
			nCityPack.nData1 = pPlayer->GetSaveData()->plrOrganizeUID;
			GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
		}
		else
			Log("DEBUG: Cannot enter now(%s)", pPlayer->GetName());
		return(-1);
	}
 #endif
#endif

	// ??d?O?_?? GM ?v??
	nPrivilege = 0;
	val = pPlayer->GetCharData()->plrSPCFlag;
	if ((val & (spcFLAG_GM | spcFLAG_GM2)) == (spcFLAG_GM | spcFLAG_GM2))
	{
		nPrivilege = gamePRIVILEGE_TYPE_GM3;
	}
	else if (val & spcFLAG_GM)
	{
		nPrivilege = gamePRIVILEGE_TYPE_GM1;
	}
	else if (val & spcFLAG_GM2)
	{
		nPrivilege = gamePRIVILEGE_TYPE_GM2;
	}
	//
	if (!nPrivilege)
	{
#ifndef USE_PACKAGE_DATA
		GetServer()->Log("DEBUG: ?LGM?v??!(%s,%d)", pPlayer->GetName(), val);
#endif
		return(-1);
	}

	if (pCmd->m_nUID != pPlayer->GetUID())	// タ戈
	{
		if (pCmd->m_nCommandID != MAP_GM_COMMAND_TRACE_PLAYER)	// 發萝產ㄒ 2005/11/01
		{
			GetServer()->Log("ERROR: CB_GMCommand: UID not match(%d,%d,%s)", pCmd->m_nUID, pPlayer->GetUID(), pPlayer->GetName());
			return(-1);
		}
	}
	// ?R?O?B?z
	switch(pCmd->m_nCommandID)
	{
	case MAP_GM_COMMAND_GOTO:
		// ?????m
		GetServer()->Log("GM ㏑: 簿笆竚, UID(%d, %s), Pos[%d,%d]", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_Goto.x, pCmd->m_Param.m_Goto.y);
	//	pPlayer->SetPos(pCmd->m_Param.m_Goto.x, pCmd->m_Param.m_Goto.y);
	//	FixMsg.m_nUID	= pCmd->m_nUID;
	//	FixMsg.m_Pos.x	= pCmd->m_Param.m_Goto.x;
	//	FixMsg.m_Pos.y	= pCmd->m_Param.m_Goto.y;
	//	::msgSendData(nClientID, pComm->pcUID, PROTO_MAP_FIX_CHAR_POS, (char *)&FixMsg, sizeof(struct MAP_FIX_CHAR_POS), 0);
		//
		pPlayer->ChangeMap(pPlayer->GetMapID(), pCmd->m_Param.m_Goto.x, pCmd->m_Param.m_Goto.y);
		break;

	case MAP_GM_COMMAND_GOTO_MAP:
		// ?????L?a??
		//GetServer()->Log("GM ㏑: ち传瓜, UID(%d, %s), MapID(%d), Pos[%d,%d]", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_GotoMap.m_nMapID, pCmd->m_Param.m_GotoMap.m_Pos.x, pCmd->m_Param.m_GotoMap.m_Pos.y);
		//pPlayer->ChangeMap(pCmd->m_Param.m_GotoMap.m_nMapID, pCmd->m_Param.m_GotoMap.m_Pos.x, pCmd->m_Param.m_GotoMap.m_Pos.y);
		GetServer()->Log("GM ㏑: ち传瓜, UID(%d, %s), MapID(%d), CopyUID(%d), Pos[%d,%d]", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_GotoMap.m_nMapID, pCmd->m_Param.m_GotoMap.m_nCopyUID, pCmd->m_Param.m_GotoMap.m_Pos.x, pCmd->m_Param.m_GotoMap.m_Pos.y);
		pPlayer->ChangeMap(pCmd->m_Param.m_GotoMap.m_nMapID, pCmd->m_Param.m_GotoMap.m_Pos.x, pCmd->m_Param.m_GotoMap.m_Pos.y,0,0,pCmd->m_Param.m_GotoMap.m_nCopyUID);
		break;

	case MAP_GM_COMMAND_KICK_PLAYER:
		// ??U?u
	/*	uid = GetServer()->CharName_FindUID(pCmd->m_Param.m_KickPlayer.m_szName);
		if (uid)
		{
			pTarget = (CMapPlayer *)GetServer()->FindObjectByUID(uid);
			if (pTarget)
			{
				if (!pTarget->IsObjectDeleted(pTarget->GetHandle()))
				{
					GetServer()->Disconnect_ClearAllRecord( pTarget->GetUID(), 0 );
					// ??????s??, ???}?a??
					pTarget->SaveAllData();
					pTarget->SetExitCode(CMapPlayer::EXIT_KICK);
					GetServer()->DeleteObject(pTarget->GetHandle());
					GetServer()->KickClient(pTarget->GetClientID());
					GetServer()->Log("GM ?R?O: ??X???a, UID(%d, %s), Min(%d)", uid, pCmd->m_Param.m_KickPlayer.m_szName, pCmd->m_Param.m_KickPlayer.m_nMinutes);
				}
			}
		}
		else	*/		// ?q?? login ??U?u
		{
			if(GetServer()->IsConnectedToServer(GetServer()->m_hLoginServer))
			{
				struct LOGIN_MAP_KICK_PLAYER nReq;

				memset(&nReq, 0, sizeof(nReq));
				nReq.uid = pPlayer->GetUID();
				nReq.nMinutes = pCmd->m_Param.m_KickPlayer.m_nMinutes;
				nReq.nType = 0;
				msg_strncpy(nReq.m_szName, pCmd->m_Param.m_KickPlayer.m_szName, sizeof(nReq.m_szName));
				GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_MAP_KICK_PLAYER, (char *)&nReq, sizeof(nReq), 0);
				GetServer()->Log("GM ?R?O: (%d, %s)??X???a(%s), Min(%d)", pPlayer->GetSaveData()->plrUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_KickPlayer.m_szName, pCmd->m_Param.m_KickPlayer.m_nMinutes);
				// ?O?? Log
				struct LOG_INSERT_MSG_ACT nLogAct;

				memset(&nLogAct, 0, sizeof(nLogAct));
				nLogAct.nType = LOGTYPE_ACT_GM_KICK_LOCK_PLAYER;
				nLogAct.nMapCode = pPlayer->GetMapCode();
				nLogAct.nPosX = pPlayer->GetPosX();
				nLogAct.nPosY = pPlayer->GetPosY();
				::msg_strncpy(nLogAct.nName, pPlayer->GetName(), sizeof(nLogAct.nName));
				nLogAct.nData1 = pCmd->m_Param.m_KickPlayer.m_nMinutes;
				::msg_strncpy(nLogAct.nStr1, pCmd->m_Param.m_KickPlayer.m_szName, sizeof(nLogAct.nStr1));
				CMapServer::GetServer()->SendLogMessage_Act(&nLogAct);
			}
		}
		break;

	case MAP_GM_COMMAND_SET_LEVEL:
		// ????
		GetServer()->Log("GM ㏑: 砞﹚单, UID(%d, %s), Level(%d), Point(%d)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_SetLevel.m_nSetLevel, pCmd->m_Param.m_SetLevel.m_nAttrPoint);
		val = pCmd->m_Param.m_SetLevel.m_nSetLevel;
		if (val > CHAR_LEVEL_MAX)
			val = CHAR_LEVEL_MAX;
		if (val > gameServerGetMaxLevel())
			val = gameServerGetMaxLevel();
		if (val < 1)
			val = 1;
		pPlayer->GetSaveData()->plrLevel = (unsigned short)val;
		{
			long long attrPt = pCmd->m_Param.m_SetLevel.m_nAttrPoint;
			if (attrPt)
			{
				unsigned long long attrPts;
				if (attrPt < 0)
					attrPts = 0;
				else if ((unsigned long long)attrPt > gameMAX_ATTR_POINT)
					attrPts = gameMAX_ATTR_POINT;
				else
					attrPts = (unsigned long long)attrPt;
				pPlayer->GetSaveData()->plrAttrPoint = attrPts;
			}
		}
		//
		exp = gameServerGetLevelUpExp(val);
		pPlayer->GetCharData()->plrLevelUpExp = exp;
		exp = 0;
	//	if (pPlayer->GetSaveData()->plrExp < exp)
		{
			pPlayer->GetSaveData()->plrExp = exp;
			//
			Part2Msg.Reset();
			Part2Msg.Add(UPART2_TYPE_EXP, 0, pPlayer->GetUID(), exp);

			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&Part2Msg, Part2Msg.GetSize(), 0);
		}
		// ?p???O
re_calc:
		::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
		//
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
		break;

	case MAP_GM_COMMAND_SET_FULLSTATE:
		// ???A????
		GetServer()->Log("GM ㏑: 砞﹚篈骸, UID(%d, %s)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName);
		pPlayer->SetHP(pPlayer->GetMaxHP());
		pPlayer->SetMP(pPlayer->GetMaxMP());
		pPlayer->GetSaveData()->plrST = pPlayer->GetSaveData()->plrSTMax;
		// ?p???O
		pPlayer->UpdatePlayerDataPart1();
		break;

	case MAP_GM_COMMAND_SET_EXP:
		// ?]?w?g????
		break;

	case MAP_GM_COMMAND_SET_SKILLEXP:
		if (nPrivilege == gamePRIVILEGE_TYPE_GM2)
			return(-1);
		// ?]?w???g????
		exp = pCmd->m_Param.m_SkillExp.m_nSkillExp;
		GetServer()->Log("GM ㏑: 砞﹚м竒喷, UID(%d, %s), Skill exp(%d)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, exp);
		valexp = pPlayer->GetCharData()->plrEH_AddSkillExp + gameMAX_SKILL_EXP;
		if (exp > valexp)
			exp = valexp;
		if (pPlayer->GetSaveData()->plrSkillExp != exp)
		{
			pPlayer->GetSaveData()->plrSkillExp	= (unsigned long)exp;
			pPlayer->UpdatePlayerDataPart1();
		}
		break;

	case MAP_GM_COMMAND_SET_SKILL_LEVEL:
		// ?]?w?????
	//	if (nPrivilege != gamePRIVILEGE_TYPE_GM1)
	//		return(-1);
		GetServer()->Log("GM ㏑: 砞﹚м单, UID(%d, %s), SkillID(%d), SkillLevel(%d)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_SkillLevel.m_nID, pCmd->m_Param.m_SkillLevel.m_nLevel);
		pSkillData = pPlayer->GetSkillData();
		//
		if(pCmd->m_Param.m_SkillLevel.m_nID	== -1)
		{
			if (pCmd->m_Param.m_SkillLevel.m_nLevel > gameMAX_SKILL_LEVEL)
				pCmd->m_Param.m_SkillLevel.m_nLevel = gameMAX_SKILL_LEVEL;
		}
		if (pCmd->m_Param.m_SkillLevel.m_nLevel < 0)
			pCmd->m_Param.m_SkillLevel.m_nLevel = 0;
		//
		if(pCmd->m_Param.m_SkillLevel.m_nID	!= -1)
		{
			//if(pCmd->m_Param.m_SkillLevel.m_nID < 1 || pCmd->m_Param.m_SkillLevel.m_nID > (gameMAX_SKILL_LEARN + 1))
			if(pCmd->m_Param.m_SkillLevel.m_nID < 1 || pCmd->m_Param.m_SkillLevel.m_nID > gameMAX_SKILL_LEARN)
				break;
			//if(pSkillData->plrSkillLevelMax[pCmd->m_Param.m_SkillLevel.m_nID] == 0)
			//	pSkillData->plrSkillLevelMax[pCmd->m_Param.m_SkillLevel.m_nID] = 1;
			//if(pSkillData->plrSkillLevelMax[pCmd->m_Param.m_SkillLevel.m_nID] < pCmd->m_Param.m_SkillLevel.m_nLevel)
				pSkillData->plrSkillLevelMax[pCmd->m_Param.m_SkillLevel.m_nID] = (unsigned char)pCmd->m_Param.m_SkillLevel.m_nLevel;
			//
			pSkillData->plrSkillLevel[pCmd->m_Param.m_SkillLevel.m_nID] = pSkillData->plrSkillLevelMax[pCmd->m_Param.m_SkillLevel.m_nID];
			//
			pPlayer->UpdateSkillTable();
		}
		else
		{
			// set all
			for(cnt1 = 1; cnt1 <= gameMAX_SKILL_LEARN; cnt1++)
			{
			//	if(pSkillData->plrSkillLevelMax[cnt1] == 0)
			//		pSkillData->plrSkillLevelMax[cnt1] = 1;
				//if(pSkillData->plrSkillLevelMax[cnt1] < pCmd->m_Param.m_SkillLevel.m_nLevel)
					pSkillData->plrSkillLevelMax[cnt1] = (unsigned char)pCmd->m_Param.m_SkillLevel.m_nLevel;
				//
				pSkillData->plrSkillLevel[cnt1] = pSkillData->plrSkillLevelMax[cnt1];
			}
			//
			pPlayer->UpdateSkillTable();
		}
		//
		::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());

//GetServer()->Log("MagicPower = %d, level=%d", pPlayer->GetCharData()->plrMagicPower, gameATK_GetSkillMagicDamageLevel(pPlayer->GetCharData()));
		//
	//	if ((pCmd->m_Param.m_SkillLevel.m_nID	== -1) || (pCmd->m_Param.m_SkillLevel.m_nID	== magic_ADD_ST))
	//	{
	//		::gameChar_CalcSTMax(pPlayer->GetCharData());
			//
			::memset(&Self, 0, sizeof(plrDATASHOWSELF));
			::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
			::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
			memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
	//	}
		break;

	case MAP_GM_COMMAND_ADD_GOLD:
		//if (nPrivilege != gamePRIVILEGE_TYPE_GM1)
		if (nPrivilege != gamePRIVILEGE_TYPE_GM3)
			return(-1);
#ifdef NO_GM_MAKE_ITEM_GOLD_COMMAND
		if (!InnerCheckIsLocalIP(GetServer()->GetRealIP()) && !InnerCheckLoginIP(napiServer_GetClientIP(pPlayer->GetClientID())))
			return(-1);
#endif
		//
		// ?W?[????????
		GetServer()->Log("GM ㏑: 砞﹚à︹窥, UID(%d, %s), Gold(%d)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_nAddGold);
		//nGold = pPlayer->GetSaveData()->plrGold;
		//nGold += pCmd->m_Param.m_nAddGold;
		nGold = pCmd->m_Param.m_nAddGold;
		if(nGold < 0)
		{
		//	pCmd->m_Param.m_nAddGold -= nGold;
			nGold = 0;
		}
		if(nGold > gameMAX_GOLD)
		{
		//	pCmd->m_Param.m_nAddGold -= (nGold - gameMAX_GOLD);
			nGold = gameMAX_GOLD;
		}
		pPlayer->GetSaveData()->plrGold = nGold;
		pPlayer->UpdateClientGoldAndWeight();
		//
		struct itemDATA_SAVE logItem;

		memset(&logItem, 0, sizeof(logItem));
		logItem.m_Item.itemCode = (unsigned short)item_Money;
		logItem.m_Item.itemGoldNumber = pCmd->m_Param.m_nAddGold;
		CMapServer::GetServer()->SendLogMessage_Item(pPlayer, LOGTYPE_ITEM_FROM_GM_COMMAND, NULL, &logItem);
		break;

	case MAP_GM_COMMAND_MAKE_ITEM:
		//if (nPrivilege != gamePRIVILEGE_TYPE_GM1)
		if (nPrivilege != gamePRIVILEGE_TYPE_GM3)
			return(-1);
#ifdef NO_GM_MAKE_ITEM_GOLD_COMMAND
		if (!InnerCheckIsLocalIP(GetServer()->GetRealIP()) && !InnerCheckLoginIP(napiServer_GetClientIP(pPlayer->GetClientID())))
			return(-1);
#endif
		// ?????~
		if (pItem = gameGetItemPtr(pCmd->m_Param.m_MakeItem.m_nItemID))
		{
			GetServer()->Log("GM ㏑: ミ珇, UID(%d, %s), ItemID(%d), Count(%d)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_MakeItem.m_nItemID, pCmd->m_Param.m_MakeItem.m_nCount);
			if (pItem->itemType & itemTypeSoul)
			{
				x = CMapServer::GetServer()->Soul_GetSoulItemDuedate();
				CMapServer::GetServer()->Soul_SetSoulItemDuedate(0);
				pPlayer->MakeItem(pCmd->m_Param.m_MakeItem.m_nItemID, pCmd->m_Param.m_MakeItem.m_nCount, LOGTYPE_ITEM_FROM_GM_COMMAND, pPlayer);
				CMapServer::GetServer()->Soul_SetSoulItemDuedate(x);
			}
			else if (pItem->itemType & itemTypeTicket)
			{
 #ifndef USE_PACKAGE_DATA
				//?????
				struct itemDATA_SAVE * pEmptyItemSave = NULL;
				long emptyItemIdx = -1;

				//for(cnt1=0; cnt1<gameMAX_CARRYITEM; cnt1++)
				val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
				for(cnt1=0; cnt1<val; cnt1++)
				{
					struct itemDATA_SAVE * pItemSave = pPlayer->GetSingleItemData( cnt1 );

					if( pItemSave && pItemSave->m_Item.itemNumber == 0 )
					{
						pEmptyItemSave = pItemSave;
						emptyItemIdx = cnt1;
						break;
					}
				}
				if (pEmptyItemSave)
				{
					struct itemDATASHOWSELF newItem;

					::gameCreateItem_SoulTicket( &newItem, GetServer()->GenerateItemUID(), pPlayer->GetUID(), pPlayer->GetName(), 1118764800, 4, 1, 0);
					::gameServer_ItemSave_MakeShowSelf( &newItem, pEmptyItemSave );
					pPlayer->SaveItemData();
					pPlayer->UpdateClientSingleItem( emptyItemIdx, 0 );
					GetServer()->SendLogMessage_Item( pPlayer, LOGTYPE_ITEM_FROM_GM_COMMAND, 0, pEmptyItemSave );
				}
 #endif
			}
			else if (pItem->itemType & itemTypeCard)
			{
 #ifndef USE_PACKAGE_DATA
				//?????
				struct itemDATA_SAVE * pEmptyItemSave = NULL;
				long emptyItemIdx = -1;

				//for(cnt1=0; cnt1<gameMAX_CARRYITEM; cnt1++)
				val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
				for(cnt1=0; cnt1<val; cnt1++)
				{
					struct itemDATA_SAVE * pItemSave = pPlayer->GetSingleItemData( cnt1 );

					if( pItemSave && pItemSave->m_Item.itemNumber == 0 )
					{
						pEmptyItemSave = pItemSave;
						emptyItemIdx = cnt1;
						break;
					}
				}
				if (pEmptyItemSave)
				{
					struct itemDATASHOWSELF newItem;
					char tmpstr[128];

					wsprintf(tmpstr, "test-%d", ::time(NULL));
					gameCreateItem_Card(&newItem, GetServer()->GenerateItemUID(), pPlayer->GetUID(), pPlayer->GetName(), gameITEM_ID_CARD, tmpstr, 29, 1);
					::gameServer_ItemSave_MakeShowSelf( &newItem, pEmptyItemSave );
					pPlayer->SaveItemData();
					pPlayer->UpdateClientSingleItem( emptyItemIdx, 0 );
					GetServer()->SendLogMessage_Item( pPlayer, LOGTYPE_ITEM_FROM_GM_COMMAND, 0, pEmptyItemSave );
				}
 #endif
			}
			else
			{
 #ifndef USE_PACKAGE_DATA
				//if (pItem->itemType & (itemTypeSoldier | itemTypeSiegeWeapon))	// ?O?_?O?h?L
				if (gameIsSoldierItemByPtr(pItem, FLAG_SIT_ALL))
				{
					struct itemDATA_SAVE * pItemSave;
					long itemIdx;

					pItemSave = GetServer()->CarryItem_FindFirstItem(pPlayer, 0, &itemIdx);
					pPlayer->MakeItem(pCmd->m_Param.m_MakeItem.m_nItemID, pCmd->m_Param.m_MakeItem.m_nCount, LOGTYPE_ITEM_FROM_GM_COMMAND, pPlayer);
					// ?]?w????
					if (pItemSave)
					{
						if (pCmd->m_Param.m_MakeItem.m_nRandomFlag > 1)
						{
							SetCNPCItemLevel(pItemSave, pCmd->m_Param.m_MakeItem.m_nItemID, pCmd->m_Param.m_MakeItem.m_nRandomFlag);
							pPlayer->UpdateClientSingleItem( itemIdx );
						}
					}
				}
				else
 #endif
				{
					if (pCmd->m_Param.m_MakeItem.m_nRandomFlag)
					{
						pPlayer->MakeItem(pCmd->m_Param.m_MakeItem.m_nItemID, pCmd->m_Param.m_MakeItem.m_nCount, LOGTYPE_ITEM_FROM_GM_COMMAND, pPlayer, 1);
					}
					else
						pPlayer->MakeItem(pCmd->m_Param.m_MakeItem.m_nItemID, pCmd->m_Param.m_MakeItem.m_nCount, LOGTYPE_ITEM_FROM_GM_COMMAND, pPlayer);
				}

			}
		}
		break;

	case MAP_GM_COMMAND_MAKE_ENEMY:
		//if (nPrivilege != gamePRIVILEGE_TYPE_GM1)
		if (nPrivilege == gamePRIVILEGE_TYPE_GM2)
			return(-1);
		GetServer()->Log("GM ㏑: (%d, %s)ミ寄, code(%d), Count(%d)", pPlayer->GetSaveData()->plrUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_MakeEnemy.m_nCode, pCmd->m_Param.m_MakeEnemy.m_nCount);
		x = pPlayer->GetPosX();
		y = pPlayer->GetPosY();
		map_id = pPlayer->GetMapID();
		map_copy_uid = pPlayer->GetMapCopyUID();
		//
		if (pCmd->m_Param.m_MakeEnemy.m_nCount > 30000)
			pCmd->m_Param.m_MakeEnemy.m_nCount = 30000;
		//
		pMapCtrl = CMapServer::GetServer()->FindMap(map_id,(unsigned short)map_copy_uid);
		if (pMapCtrl)
		{
			GetServer()->m_SpeedRecord.SetElapseTickStart();
			for (cnt1=0; cnt1< pCmd->m_Param.m_MakeEnemy.m_nCount; cnt1++)
			{
				val = 3;
				px = x; py = y;
				// ?O?_???????
				while(1)
				{
					px = x + gameGetRandomRange(0, 1024) - 512;
					py = y + gameGetRandomRange(0, 1024) - 512;
					long code = pMapCtrl->GetIconData(gameIconCalc_DIV(px), gameIconCalc_DIV(py));
					//if (code == MAPCODE_ID_WALL)
					if (!code)
						break;
					if (--val <= 0)
					{
						px = x; py = y;
						break;
					}
				}
				pNPC = GetServer()->CreateEnemy(px, py, pCmd->m_Param.m_MakeEnemy.m_nCode, map_id, 1, 1, map_copy_uid);
				if (pNPC)
				{
#ifndef USE_PACKAGE_DATA
					// ????Z???_?c
					switch(pCmd->m_Param.m_MakeEnemy.m_nCode)
					{
					case SOUL_BOX_ENEMY_CODE1:
					//case SOUL_BOX_ENEMY_CODE2:
					case SOUL_BOX_ENEMY_CODE3:
					//case SOUL_BOX_ENEMY_CODE4:
						pNPC->m_ParentHandle = -1;
						pNPC->m_ParentHandleFlag = PARENT_FLAG_SOUL_BOX;		// ?Z???_?c
						if (gameGetRandomRange(0, 100) > 80)
						{
							pNPC->m_nSoulData_ItemID = 3921;
						}
						else
							pNPC->m_nSoulData_ItemID = 0;
						break;
					case BOX_ENEMY_CODE01:
						pNPC->m_ParentHandle = -1;
						pNPC->m_ParentHandleFlag = PARENT_FLAG_BOX;
#if (defined(GAMEFLIER_NORMAL) || defined(GAMEFLIER_ITEMMALL) || defined(GAMEFLIER_CLASSIC))
						pNPC->m_nSoulData_ItemID = 0;
#else
						if (gameGetRandomRange(0, 100) > 80)
						{
							pNPC->m_nSoulData_ItemID = gameITEM_ID_SETUP_FORCE_01;
						}
						else
							pNPC->m_nSoulData_ItemID = 0;
#endif
						pNPC->m_nBoxData_DisappearTime = CMapServer::GetServer()->GetLoopTime() + 10;
						break;
					}
					//pNPC->m_nCharFlags |= CHAR_FLAG_NO_MOVE_BLOCK | CHAR_FLAG_HURT_SUPER;
#endif
					pNPC->m_nCharFlags |= CHAR_FLAG_HURT_SUPER;
//GetServer()->Log("GM ㏑: ミ寄(%s,%s)", pNPC->GetName(), pNPC->GetShowData()->plrName);
				}
			}
			//
			exp = GetServer()->m_SpeedRecord.GetElapseTick();
			GetServer()->Log("GM ㏑: ミ寄OK, time = %d", exp);
		}
		break;

	case MAP_GM_COMMAND_CALL_ARMY:
//#ifndef USE_PACKAGE_DATA
		if (nPrivilege == gamePRIVILEGE_TYPE_GM2)
			return(-1);
		x = pPlayer->GetPosX();
		y = pPlayer->GetPosY();
		map_id = pPlayer->GetMapID();
		GetServer()->Log("GM ?R?O: ?I?s?x??(%d,%d)", pCmd->m_Param.m_MakeArmy.m_nArmyID, pCmd->m_Param.m_MakeArmy.m_nRoleID);
		GetServer()->GetMapManager()->ForceCreateBossArmy(map_id, x, y, pCmd->m_Param.m_MakeArmy.m_nArmyID, pCmd->m_Param.m_MakeArmy.m_nRoleID);
//#endif
		break;

	case MAP_GM_COMMAND_GOTO_PLAYER:
		uid = GetServer()->CharName_FindUID(pCmd->m_Param.m_GotoPlayer.m_szName);
		if (uid)
		{
			pTarget = (CMapPlayer *)GetServer()->FindObjectByUID(uid);
			if (pTarget)
			{
			//	if (!pTarget->IsObjectDeleted(pTarget->GetHandle()))
				{
					GetServer()->FindPos(pTarget->GetMapID(), pTarget->GetPosX(), pTarget->GetPosY(), 6*gameIconSize, 6*gameIconSize, &x, &y);
					pPlayer->ChangeMap(pTarget->GetMapID(), x, y);
					GetServer()->Log("GM ㏑: (%d, %s)肚癳產ō娩, UID(%d, %s)", pPlayer->GetSaveData()->plrUID, pPlayer->GetSaveData()->plrName, uid, pCmd->m_Param.m_KickPlayer.m_szName);
				}
			}
		}
		else			// ?q?? Login ??e???a????
		{
			if(GetServer()->IsConnectedToServer(GetServer()->m_hLoginServer))
			{
				struct LOGIN_MAP_GOTO_PLAYER nReq;

				nReq.nType = 0;
				nReq.nTargetUID = 0;
				nReq.uid = pPlayer->GetUID();
				msg_strncpy(nReq.m_szName, pCmd->m_Param.m_GotoPlayer.m_szName, sizeof(nReq.m_szName));
				GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_MAP_GOTO_PLAYER, (char *)&nReq, sizeof(nReq), 0);
				GetServer()->Log("GM ㏑: (%d, %s)肚癳產ō娩, UID(%d, %s)", pPlayer->GetSaveData()->plrUID, pPlayer->GetSaveData()->plrName, uid, pCmd->m_Param.m_GotoPlayer.m_szName);
			}
		}
		break;
	case MAP_GM_COMMAND_SET_OFFICE:			// ?]?w?x??		[id]
	//	if (nPrivilege != gamePRIVILEGE_TYPE_GM1)
	//		return(-1);
		x = pCmd->m_Param.m_SetOffice.m_nOffice;
		if (!x)
			x++;
		if ((x >= 1) && (x <= gameMAX_OFFICER))
		{
			if (::gameGetOfficerPtr(x))
			{
				GetServer()->Log("GM ?R?O: ?]?w?x??, UID(%d, %s), Office(%d)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, x);
				//
				pPlayer->UpdateSkillTable(2, pPlayer->GetSaveData()->plrOffice);
				pPlayer->GetSaveData()->plrOffice = (unsigned short)x;
				pPlayer->GetSaveData()->plrTime_OfficeGift = 0;
				pPlayer->UpdateSkillTable(1, pPlayer->GetSaveData()->plrOffice);
				pPlayer->m_nHonorVal_Max = (unsigned short)gameCalcCountryWarHonorMax(pPlayer->GetCharData());
#ifndef USE_PACKAGE_DATA		// ?????G?[??
				pPlayer->GetSaveData()->plrHonorVal = pPlayer->m_nHonorVal_Max;
#endif
				// ?t???????
				if (x == 255)
				{
					memset(pPlayer->GetSaveData()->plrCompositeTable, 0xff, sizeof(pPlayer->GetSaveData()->plrCompositeTable));
				}
				goto re_calc;
			}
		}
	/*	else if (x > 255)
		{
			if (::gameGetOfficerPtr(x))
			{
				GetServer()->Log("GM ?R?O: ?]?w?x??>255, UID(%d, %s), Office(%d)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, x);
				//
				if (pPlayer->GetSaveData()->plrSoulOffice)
					pPlayer->UpdateSkillTable(2, pPlayer->GetSaveData()->plrSoulOffice);
				pPlayer->GetSaveData()->plrSoulWID = 1;
				pPlayer->GetSaveData()->plrSoulOffice = x;
				pPlayer->UpdateSkillTable(1, pPlayer->GetSaveData()->plrSoulOffice);
				pPlayer->m_nHonorVal_Max = gameCalcCountryWarHonorMax(pPlayer->GetCharData());
				goto re_calc;
			}
		} */
		break;
	case MAP_GM_COMMAND_SET_HONOR:			// ?]?w?\??
		//if (nPrivilege != gamePRIVILEGE_TYPE_GM1)
		if (nPrivilege != gamePRIVILEGE_TYPE_GM3)
			return(-1);
		GetServer()->Log("GM ?R?O: ?]?w?\??, UID(%d, %s), Office(%d)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_SetHonor.m_nHonor);
		//x = pPlayer->GetSaveData()->plrHonor + pCmd->m_Param.m_SetHonor.m_nHonor;
		x = pCmd->m_Param.m_SetHonor.m_nHonor;
		if (x > gameMAX_HONOR)
			x = gameMAX_HONOR;
		if (x < 0)
			x = 0;
		pPlayer->GetSaveData()->plrHonor = (unsigned long)x;
		//
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		break;
	case MAP_GM_COMMAND_INVISIBLE:
		if (pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_INVISIBLE)
		{
			GetServer()->Log("GM ㏑: 砞﹚篈骸, UID(%d, %s)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName);
		}
		else
			GetServer()->Log("GM ㏑: 砞﹚篈骸, UID(%d, %s)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName);
		pPlayer->GetSaveData()->plrBaseSPCFlag ^= spcFLAG_INVISIBLE;
		pPlayer->GetCharData()->plrSPCFlag ^= spcFLAG_INVISIBLE;
		pPlayer->m_nCharFlags ^= CHAR_FLAG_GM_NO_APPEAR;
		//
		pPlayer->UpdateShowData();
		pPlayer->CarryNPC_Update();
		//
		if (pPlayer->m_nCharFlags & CHAR_FLAG_GM_NO_APPEAR)
		{
			pMsg = pPlayer->InsertMsg(PROTO_MAP_OBJECT_OUT, CMapCharMsg::SEND_TO_OTHER);
			pMsg->m_Msg.m_ObjectOUT.m_nCount = 1;
			pMsg->m_nSize	= (short)pMsg->m_Msg.m_ObjectOUT.GetSize();
			pMsg->m_Msg.m_ObjectOUT.m_nObjectUID[0] = pPlayer->GetUID();
		}
		else
		{
			pMsg = pPlayer->InsertMsg(PROTO_MAP_OBJECT_IN, CMapCharMsg::SEND_TO_OTHER);
			pMsg->m_Msg.m_ObjectIN.m_nCount = 1;
			pMsg->m_nSize	= (short)pMsg->m_Msg.m_ObjectIN.GetSize();
			memcpyT(pMsg->m_Msg.m_ObjectIN.m_DataShow, pPlayer->GetShowData(), sizeof(pMsg->m_Msg.m_ObjectIN.m_DataShow));
		}
		break;
	case MAP_GM_COMMAND_SUPERMODE:			// ?L??
		if (pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_GODMODE)
		{
			GetServer()->Log("GM ㏑: 砞﹚篈骸, UID(%d, %s)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName);
		}
		else
			GetServer()->Log("GM ?R?O: ?}?l?L??UID(%d, %s)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName);
		pPlayer->GetSaveData()->plrBaseSPCFlag ^= spcFLAG_GODMODE;
		pPlayer->GetCharData()->plrSPCFlag ^= spcFLAG_GODMODE;
		break;
	case MAP_GM_COMMAND_FOLLOW_PLAYER:
		// !!!!!
		break;
	case MAP_GM_COMMAND_CHAR_SET_POS:		// à︹: 簿笆竚	[name][x][y]{map_id}
		uid = GetServer()->CharName_FindUID(pCmd->m_Param.m_CharSetPos.m_szName);
		if (uid)
		{
			pTarget = (CMapPlayer *)GetServer()->FindObjectByUID(uid);
			if (pTarget)
			{
			//	if (!pTarget->IsObjectDeleted(pTarget->GetHandle()))
				{
					pTarget->ChangeMap(pTarget->GetMapID(), pCmd->m_Param.m_CharSetPos.m_nX * gameIconSize, pCmd->m_Param.m_CharSetPos.m_nY * gameIconSize);
					GetServer()->Log("GM ㏑: 簿笆產竚(%d, %s)", uid, pTarget->GetSaveData()->plrName);
					//
					pPlayer->SendMsgToClientByID(24271);
					break;
				}
			}
		}
		pPlayer->SendMsgToClientByID(24270);
		break;
	case MAP_GM_COMMAND_CHAR_STOP_TALK:		// à︹: 窽ē		[name][hour][min]
		uid = GetServer()->CharName_FindUID(pCmd->m_Param.m_CharStopTalk.m_szName);
		if (uid)
		{
			pTarget = (CMapPlayer *)GetServer()->FindObjectByUID(uid);
			if (pTarget)
			{
			//	if (!pTarget->IsObjectDeleted(pTarget->GetHandle()))
				{
					pTarget->GetSaveData()->plrPrivilege_DeuDate = GetServer()->GetLoopTime() + pCmd->m_Param.m_CharStopTalk.m_nHour * 60 * 60 + pCmd->m_Param.m_CharStopTalk.m_nMin * 60;
					pTarget->m_nPrivilege |= gamePRIVILEGE_TYPE_STOPTALK;
					pTarget->GetSaveData()->plrPrivilege |= gamePRIVILEGE_TYPE_STOPTALK;
					pTarget->AutoSaveCharData();
					GetServer()->Log("GM ?R?O: ???a?T??(%d, %s)(%02d:%02d)", uid, pTarget->GetSaveData()->plrName, pCmd->m_Param.m_CharStopTalk.m_nHour, pCmd->m_Param.m_CharStopTalk.m_nMin);
					//
					pPlayer->SendMsgToClientByID(24271);
					break;
				}
			}
		}
		pPlayer->SendMsgToClientByID(24270);
		break;
	case MAP_GM_COMMAND_CALL_PLAYER:		// 簿笆產GMō娩	[name]
		// ?q?? Login ??e???a????
		if(GetServer()->IsConnectedToServer(GetServer()->m_hLoginServer))
		{
			struct MAP_GM_CALL_PLAYER nReq;

			nReq.nGM_UID = pPlayer->GetUID();
			nReq.nGM_MapCode = pPlayer->GetMapCode();
			nReq.nGM_PosX = pPlayer->GetPosX();
			nReq.nGM_PosY = pPlayer->GetPosY();
			nReq.nUID = 0;
			msg_strncpy(nReq.m_szName, pCmd->m_Param.m_CallPlayer.m_szName, sizeof(nReq.m_szName));
			GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_GM_CALL_PLAYER, (char *)&nReq, sizeof(nReq), 0);
			GetServer()->Log("GM ㏑: (%d, %s)肚癳產(%s)ō娩", pPlayer->GetSaveData()->plrUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_GotoPlayer.m_szName);
		}
		break;
	case MAP_GM_COMMAND_SET_BOT_TYPE:
		::memset( &nCityPack, 0, sizeof(nCityPack) );
		nCityPack.nType = TYPE_PACK_BOT_ENEMY_STATUS;
		nCityPack.nData1 = pCmd->m_Param.m_SetBOTType.m_nType;		// Type
		nCityPack.nData2 = pCmd->m_Param.m_SetBOTType.m_nSec;		// ???(??)
		GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
		GetServer()->Log("GM ?R?O: (%d, %s)?]?wBOT???A(%d,%d)", pPlayer->GetSaveData()->plrUID, pPlayer->GetSaveData()->plrName, nCityPack.nData1, nCityPack.nData2);
		break;
	case MAP_GM_COMMAND_TRACE_PLAYER:		// 發萝產[name]
		// ?q?? Login ?l????a
		if(GetServer()->IsConnectedToServer(GetServer()->m_hLoginServer))
		{
			struct MAP_GM_TRACE_PLAYER nReq;

			nReq.nGM_UID = pPlayer->GetUID();
			//nReq.nUID = 0;
			msg_strncpy(nReq.m_szName, pCmd->m_Param.m_CallPlayer.m_szName, sizeof(nReq.m_szName));
			GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_GM_TRACE_PLAYER, (char *)&nReq, sizeof(nReq), 0);
			GetServer()->Log("GM ?R?O: Trace(%d,%s,%s)", pPlayer->GetSaveData()->plrUID, pPlayer->GetSaveData()->plrName, pCmd->m_Param.m_CallPlayer.m_szName);
		}
		break;
	case MAP_GM_COMMAND_KILL_TEN:			// ?R???Q?j??W[name]
		if (nPrivilege != gamePRIVILEGE_TYPE_GM3)
			return(-1);
		if(GetServer()->IsConnectedToServer(GetServer()->m_hLoginServer))
		{
			struct LOGIN_DB_CLEAR_PLAYER_NOTIFY nReq;

			nReq.uid = pCmd->m_Param.m_KillTen.m_nUID;
			GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_DB_CLEAR_PLAYER_NOTIFY, (char *)&nReq, sizeof(nReq), 0);
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("GM ?R?O: Kill ten(%d)", nReq.uid);
#endif
		}
		break;
	case MAP_GM_COMMAND_SET_MAX_ITEM:
#ifndef USE_PACKAGE_DATA
		val = pCmd->m_Param.m_SetMaxItem.m_nNum;
		if (!pCmd->m_Param.m_SetMaxItem.m_nType)
		{
			if (val > gameMAX_CARRYITEM)
				val = gameMAX_CARRYITEM;
			if (val < 1)
				val = 1;
			pPlayer->GetSaveData()->plrMax_CarryItem = (unsigned char)val;
		}
		else
		{
			if (val > gameMAX_STORAGEITEM)
				val = gameMAX_STORAGEITEM;
			if (val < 1)
				val = 1;
			pPlayer->GetSaveData()->plrMax_StorageItem = (unsigned char)val;
			pPlayer->GetStorageData()->psMax_StorageItem = (unsigned char)val;
			pPlayer->SaveStorageData();
		}
		pPlayer->SaveCharData();
		//
		//::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		//::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		//::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		pPlayer->UpdateClientMaxItem();
#endif
		break;
	case MAP_GM_COMMAND_GET_CHANGE_NAME_COUNTER:
		::memset( &nCityPack, 0, sizeof(nCityPack) );
		nCityPack.nType = TYPE_PACK_GM_ASK_CHANGE_NAME_COUNTER;
		nCityPack.nData1 = pPlayer->GetUID();
		GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
		break;
	case MAP_GM_COMMAND_ASK_MAPSPACE_STATUS:
		GetServer()->GetMapSpaceManager()->MapSpace_ReportEnterMapStatus(pPlayer);
		break;

	case MAP_GM_COMMAND_SET_QUEST:
		if (nPrivilege != gamePRIVILEGE_TYPE_GM3)
			return(-1);
		//
		val = pCmd->m_Param.m_SetQuest.m_nType;
		if ((val < 0) || (val > 2))
			return(-1);
		//
		pMIS = pPlayer->GetMissionData();
		if (pMIS)
		{
			memset(pMIS, 0, sizeof(*pMIS));
			//
			for (cnt1=1; cnt1 < gameMAX_MISSION; cnt1++)
			{
				pMissionTbl = gameGetMissionTablePtr(cnt1);
				if (pMissionTbl)
				{
					if (pMissionTbl->gmdID)
					{
						pMIS->pmStatus[cnt1] = (char)val;
						//
						if (pMissionTbl->gmdRepeat)
						{
							pPlayer->SetMissionRepeat(cnt1, pMissionTbl->gmdRepeat);
						}
					}
				}
			}
			//
			pPlayer->SendMissionDataToClient();
			pPlayer->SaveMissionData(0);
		}
		break;
	case MAP_GM_COMMAND_SET_CITY_ORG:
		if (nPrivilege != gamePRIVILEGE_TYPE_GM3)
			return(-1);
		//
		struct gameFORCECHANGE_DATA * pForceChange;
		struct LOGIN_MAP_FORCE_CHANGE_GM_COMMAND nReq;

		nReq.m_nMapCode = (unsigned short)pCmd->m_Param.m_SetCityOrg.m_nMapCode;
		msg_strncpy(nReq.m_sLeaderName, pCmd->m_Param.m_SetCityOrg.m_sLeaderName, sizeof(nReq.m_sLeaderName));
		// ?s?@??O?????
		pForceChange = gameGetForceChangeTablePtr(nReq.m_nMapCode);
		if (pForceChange)
		{
			x = 0;
			for (cnt1=0; cnt1<gameMAX_FORCECHANGE_NUMBER; cnt1++)
			{
				val = pForceChange->fcSet[cnt1];
				if (!val)
					break;
				nReq.m_nMapCode_ForceChange[x++] = (unsigned short)val;
			}
			GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_MAP_FORCE_CHANGE_GM_COMMAND, (char *)&nReq, sizeof(nReq), 0 );
			GetServer()->Log("GM ㏑: (%s)(%d, %s)砞﹚瓁刮(%d)", pPlayer->GetSaveData()->plrName, nReq.m_nMapCode, nReq.m_sLeaderName);
		}
		break;
	case MAP_GM_COMMAND_SET_ORG_VICE:
		if (nPrivilege != gamePRIVILEGE_TYPE_GM3)
			return(-1);
		//
		struct LOGIN_MAP_SET_ORG_VICE_GM_COMMAND nSetVice;

		nSetVice.m_nMode = pCmd->m_Param.m_SetOrgVice.m_nMode;
		nSetVice.m_nGM_PlayerUID = pPlayer->GetUID();
		msg_strncpy(nSetVice.m_sViceLeaderName, pCmd->m_Param.m_SetOrgVice.m_sViceLeaderName, sizeof(nSetVice.m_sViceLeaderName));
		GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_MAP_SET_ORG_VICE_GM_COMMAND, (char *)&nSetVice, sizeof(nSetVice), 0 );
		GetServer()->Log("GM ㏑: (%s)(%d, %s)砞﹚瓁刮捌刮(%d)", pPlayer->GetSaveData()->plrName, nSetVice.m_nMode, nSetVice.m_sViceLeaderName);
		break;
	case MAP_GM_COMMAND_CLEAR_ALL_ITEM:
		memset(pPlayer->GetItemData(), 0, sizeof(struct plrCARRYITEM_SAVE));
		// ???s?p??t??
		gameServer_CalcCharacterWeight(pPlayer->GetCharData(), pPlayer->GetItemData());
		//
		pPlayer->SaveItemData();
		pPlayer->UpdateClientItemData(0);
		pPlayer->UpdateClientGoldAndWeight();
		GetServer()->Log("错误: 玩家登陆超时(%d,%s)", pPlayer->GetUID(), pPlayer->GetName());
		break;
	case MAP_GM_COMMAND_SET_CRIME:
#ifdef TW_PVP_MODE
		val = pCmd->m_Param.m_SetCrime.m_nCrime;
		if ((val < 0) || (val > PLAYER_MAX_CRIME_VAL))
			return(-1);
		//
		pPlayer->UpdatePlayerCrime(val);
		if (val > 0)
		{
			pPlayer->pStatus_Set(effFun_RED, 1);
		}
		else
			pPlayer->pStatus_Clear(effFun_RED, 1);
		pPlayer->SetCarryNPC_REDStatus();
		pPlayer->m_nCrimeClearTick = 0;
		GetServer()->Log("GM ?R?O: (%d,%s)?]?w?g?@??(%d)", pPlayer->GetUID(), pPlayer->GetName(), val);
#endif
		break;

	case MAP_GM_COMMAND_KSPOINT:
		{
#ifdef NO_GM_MAKE_ITEM_GOLD_COMMAND
			if (!InnerCheckIsLocalIP(GetServer()->GetRealIP()) && !InnerCheckLoginIP(napiServer_GetClientIP(pPlayer->GetClientID())))
				return(-1);
#endif
			if ((unsigned long)lpParam[0] > 9999999)
				lpParam[0] = 9999999;

			if ((unsigned long)lpParam[1] > 9999999)
				lpParam[1] = 9999999;

			pPlayer->GetSaveData()->plrKS_Score = lpParam[0];
			pPlayer->GetSaveData()->plrKS_RedScore = lpParam[1];

			CMapCharMsg * pMsg = pPlayer->InsertMsg( PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF );
			
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_KS_SCORE, 0, pPlayer->GetSaveData()->plrKS_RedScore, pPlayer->GetSaveData()->plrKS_Score);

			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}
		break;

	case MAP_GM_COMMAND_SET_ARMY_POINT:
		{
			if (pPlayer->GetSaveData()->plrOrganizeUID)
			{
				if (GetServer()->m_hLoginServer && GetServer()->IsConnectedToServer(GetServer()->m_hLoginServer))
				{
					struct LOGIN_MAP_SET_ARMY_POINT_GM_COMMAND nSetArmyPoint;
					nSetArmyPoint.nPlayerUID = pPlayer->GetUID();
					nSetArmyPoint.nSetPoints = lpParam[0];
					GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_MAP_SET_ARMY_POINT_GM_COMMAND, (char *)&nSetArmyPoint, sizeof(nSetArmyPoint), 0 );
				}
			}
		}
		break;

	case MAP_GM_COMMAND_CLEAR_BATTLEFIELD_DUETIME:
		{
			if (pPlayer->GetSaveData()->plrOrganizeUID)
			{
				if (lpParam[0] == 0)
				{
					pPlayer->m_nArmyBattleFieldDueTime = 0;
				}
				else
				{
					if (GetServer()->m_hLoginServer && GetServer()->IsConnectedToServer(GetServer()->m_hLoginServer))
					{
						struct LOGIN_MAP_CLEAR_BATTLEFIELD_DUETIME_GM_COMMAND nClearBattleField;
						nClearBattleField.nPlayerUID = pPlayer->GetUID();
						GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_MAP_CLEAR_BATTLEFIELD_DUETIME_GM_COMMAND, (char *)&nClearBattleField, sizeof(nClearBattleField), 0 );
					}
				}
			}
		}
		break;
	case MAP_GM_COMMAND_CLEAR_PAYMAPTIME:
		{
			if(pPlayer->GetSaveData()->plrEnterStageTime > 0)
			{
				CMapCharMsg *	pMsg;
				pPlayer->GetSaveData()->plrEnterStageTime = 0;

				pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				if (pMsg)
				{
					pMsg->m_Msg.m_UpdateData2Msg.Reset();
					pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_ENTER_STAGE_NOTIFY, 0, pPlayer->GetSaveData()->plrEnterStageID, pPlayer->GetSaveData()->plrEnterStageTime); // data1=stage id, data2=time

					pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				}
			}
		}
		break;
	case MAP_GM_COMMAND_CLEAR_EIGHTGATES:
		//long pageNum = lpParam[0];
		//char tmpstr[64];
		//if(pageNum < 0 || pageNum > 1) break;
		if(lpParam[0] == 0)//场﹍て
		{
			memset( pPlayer->GetSaveData()->plrEightGatesData1, -1, gameMAX_EIGHTGATES_NUM*sizeof(char) );	
			memset( pPlayer->GetSaveData()->plrEightGatesData2, -1, gameMAX_EIGHTGATES_NUM*sizeof(char) );	
			memset( pPlayer->GetSaveData()->plrEightGatesData3, -1, gameMAX_EIGHTGATES_NUM*sizeof(char) );
		}
		else if(lpParam[0] == 1)
		{
			memset (pPlayer->GetSaveData()->plrEightGatesData1, -1, gameMAX_EIGHTGATES_NUM*sizeof(char) );
		}
		else if(lpParam[0] == 2)
		{
			memset (pPlayer->GetSaveData()->plrEightGatesData2, -1, gameMAX_EIGHTGATES_NUM*sizeof(char) );
		}
		else if(lpParam[0] == 3)
		{
			memset (pPlayer->GetSaveData()->plrEightGatesData3, -1, gameMAX_EIGHTGATES_NUM*sizeof(char) );
		}
		pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		if (pMsg)
		{
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_SET_EIGHTGATES_INIT, 0, lpParam[0]); // data1=stage id, data2=time

			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}
		::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
		//
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
		memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
		break;
	case MAP_GM_COMMAND_SET_EIGHTGATES_ALL20:
		{
			long i;
			memset( pPlayer->GetSaveData()->plrEightGatesData1, 20, gameMAX_EIGHTGATES_NUM*sizeof(char) );
			memset( pPlayer->GetSaveData()->plrEightGatesData2, 20, gameMAX_EIGHTGATES_NUM*sizeof(char) );
			memset( pPlayer->GetSaveData()->plrEightGatesData3, 20, gameMAX_EIGHTGATES_NUM*sizeof(char) );
			pPlayer->GetSaveData()->plrOpenGateTime = game_EIGHTGATE_TIME;
			pPlayer->GetCharData()->plrCheckGateOpen = 1;
			pPlayer->UpdateEightGateTime(pPlayer->GetSaveData()->plrOpenGateTime, pPlayer->GetCharData()->plrCheckGateOpen);

			for (i = 1; i <= 3; ++i)
			{
				pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				if (pMsg)
				{
					pMsg->m_Msg.m_UpdateData2Msg.Reset();
					pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_SET_EIGHTGATES_INIT, 0, i);
					pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				}
			}

			::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
			::memset(&Self, 0, sizeof(plrDATASHOWSELF));
			::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
			::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
			memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
		}
		break;
	case MAP_GM_COMMAND_SET_GENERAL_ALL_ACTIVE:
		{
			long i;
			/* ??General_Set.txt ??????? GENERAL_SET_DATA ??????*/
			memset(pPlayer->GetSaveData()->plrGeneral.level, 0, sizeof(pPlayer->GetSaveData()->plrGeneral.level));
			for (i = 1; i <= gameMAX_GENERAL_NUM; ++i)
				if (gameGetGenSetPtr(i))
					gameSetGen(pPlayer->GetSaveData()->plrGeneral.level, i);

			pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			if (pMsg)
			{
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_GET_GENERAL);
				pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
			}

			::gameServer_CalcCharacterAttribute(pPlayer->GetCharData());
			::memset(&Self, 0, sizeof(plrDATASHOWSELF));
			::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
			::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
			memcpyT(pPlayer->GetShowData(), &Self.plrShowData, sizeof(struct plrDATASHOW));
		}
		break;
	
	case MAP_GM_COMMAND_SET_ABILITY:
		{
			unsigned short nAddAttr = (unsigned short)lpParam[0]-1;
			long long targetVal = (long long)lpParam[1];
			long long AddNum;
			unsigned long long * pBaseAttrPts, * pAttrPtsMax;
			unsigned long long * pAttrPtsLeft;

			//?s??
			unsigned long long * pStr = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrStr;
			unsigned long long * pInt = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrInt;
			unsigned long long * pDex = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrDex;
			unsigned long long * pMind = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrMind;
			unsigned long long * pCon = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrCon;
			unsigned long long * pLeader = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrLeader;

			unsigned long long * pStrMax = &pPlayer->GetCharData()->plrSaveData.plrAttrStrMax;
			unsigned long long * pIntMax = &pPlayer->GetCharData()->plrSaveData.plrAttrIntMax;
			unsigned long long * pDexMax = &pPlayer->GetCharData()->plrSaveData.plrAttrDexMax;
			unsigned long long * pMindMax = &pPlayer->GetCharData()->plrSaveData.plrAttrMindMax;
			unsigned long long * pConMax = &pPlayer->GetCharData()->plrSaveData.plrAttrConMax;
			unsigned long long * pLeaderMax = &pPlayer->GetCharData()->plrSaveData.plrAttrLeaderMax;

			struct plrDATA * pPlayerData = pPlayer->GetCharData();

			pAttrPtsLeft = &pPlayer->GetCharData()->plrSaveData.plrAttrPoint; //妮┦逞緇だ皌翴计


			switch(nAddAttr)
			{
			case ATTRIB_STR:
				pBaseAttrPts = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrStr;
				pAttrPtsMax = &pPlayer->GetCharData()->plrSaveData.plrAttrStrMax;
				break;
			case ATTRIB_INT:
				pBaseAttrPts = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrInt;
				pAttrPtsMax = &pPlayer->GetCharData()->plrSaveData.plrAttrIntMax;
				break;
			case ATTRIB_DEX:
				pBaseAttrPts = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrDex;
				pAttrPtsMax = &pPlayer->GetCharData()->plrSaveData.plrAttrDexMax;
				break;
			case ATTRIB_MIND:
				pBaseAttrPts = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrMind;
				pAttrPtsMax = &pPlayer->GetCharData()->plrSaveData.plrAttrMindMax;
				break;
			case ATTRIB_CON:
				pBaseAttrPts = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrCon;
				pAttrPtsMax = &pPlayer->GetCharData()->plrSaveData.plrAttrConMax;
				break;
			case ATTRIB_LEADER:
				pBaseAttrPts = &pPlayer->GetCharData()->plrSaveData.plrBaseAttrLeader;
				pAttrPtsMax = &pPlayer->GetCharData()->plrSaveData.plrAttrLeaderMax;
				break;
			default:
				return -1;
			}

			if(!pBaseAttrPts || !pAttrPtsMax)
				return -1;

			AddNum = targetVal - (long long)(*pBaseAttrPts);
			if (AddNum <= 0)
				return -1;

			if ((*pBaseAttrPts + (unsigned long long)AddNum) >= *pAttrPtsMax)
				AddNum = (long long)(*pAttrPtsMax - *pBaseAttrPts);
			if (AddNum <= 0)
				return -1;

			(*pBaseAttrPts) += (unsigned long long)AddNum;

			//?s??
			if( pBaseAttrPts != pStr )
				(*pStrMax) += (unsigned long long)(2 * AddNum);
			if( pBaseAttrPts != pInt )
				(*pIntMax) += (unsigned long long)(2 * AddNum);
			if( pBaseAttrPts != pDex )
				(*pDexMax) += (unsigned long long)(2 * AddNum);
			if( pBaseAttrPts != pMind )
				(*pMindMax) += (unsigned long long)(2 * AddNum);
			if( pBaseAttrPts != pCon )
				(*pConMax) += (unsigned long long)(2 * AddNum);
			if( pBaseAttrPts != pLeader )
				(*pLeaderMax) += (unsigned long long)(2 * AddNum);

			::gameServer_CalcCharacterAttribute( pPlayer->GetCharData() ); //璸衡妮┦

			//?s??
			pPlayer->AutoSaveCharData();
		//?^??
			struct MAP_ADD_ATTRIB_RESULT sendData;
			memset(&sendData, 0, sizeof(sendData));
	
			sendData.nAttrType = nAddAttr;
			sendData.nAttrPts = *pBaseAttrPts;
			sendData.nPtsLeft = *pAttrPtsLeft;
			//max
			sendData.nStrMax = pPlayer->GetCharData()->plrSaveData.plrAttrStrMax;
			sendData.nIntMax = pPlayer->GetCharData()->plrSaveData.plrAttrIntMax;
			sendData.nDexMax = pPlayer->GetCharData()->plrSaveData.plrAttrDexMax;
			sendData.nMindMax = pPlayer->GetCharData()->plrSaveData.plrAttrMindMax;
			sendData.nConMax = pPlayer->GetCharData()->plrSaveData.plrAttrConMax;
			sendData.nLeaderMax = pPlayer->GetCharData()->plrSaveData.plrAttrLeaderMax;
	
			unsigned char pct = 0;
			//hp
			sendData.nHP = pPlayer->GetHP();
			sendData.nHPMax = pPlayer->GetMaxHP();

			if(pPlayer->GetMaxHP())
				pct = (unsigned char)(pPlayer->GetHP() * 100 / pPlayer->GetMaxHP());
	
			sendData.HP_pct = pct;
			//mp
			sendData.nMP = pPlayer->GetMP();
			sendData.nMPMax = pPlayer->GetMaxMP();

			if(pPlayer->GetMaxMP())
				pct = (unsigned char)(pPlayer->GetMP() * 100 / pPlayer->GetMaxMP());
	
			sendData.MP_pct = pct;
			//attack, defense
			sendData.nAttackMin = pPlayerData->plrAttackPowerMin;
			sendData.nAttackMax = pPlayerData->plrAttackPowerMax;
			sendData.nDefense = pPlayerData->plrDefense;
			sendData.nMagicPower = pPlayerData->plrMagicPower;
			sendData.nMagicPowerVal = pPlayerData->plrMagicPowerVal;
			sendData.nMagicDefense = pPlayerData->plrMagicDefense;
			//load
			sendData.nLoadMax = pPlayerData->plrWeightMax;

			::msgSendData( nClientID, pComm->pcUID, PROTO_MAP_ADD_ATTRIB_RESULT, (char *)&sendData, sizeof(sendData), 0 );
		}
		break;
	case MAP_GM_COMMAND_ONEMAP:			// ??H???
		if (pPlayer->GetSaveData()->plrBaseSPCFlag & spcFLAG_MAPSPACE)
		{
			GetServer()->Log("GM ㏑: 砞﹚篈骸, UID(%d, %s)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName);
		}
		else
			GetServer()->Log("GM ?R?O: ?}?l??H???UID(%d, %s)", pCmd->m_nUID, pPlayer->GetSaveData()->plrName);
		pPlayer->GetSaveData()->plrBaseSPCFlag ^= spcFLAG_MAPSPACE;
		pPlayer->GetCharData()->plrSPCFlag ^= spcFLAG_MAPSPACE;
		break;
	case MAP_GM_COMMAND_BATTLE_SKILL_RESET:			// 竚驹м戈
		if(pPlayer->GetSaveData()->plrBattleSkill)
		{
			memset(pPlayer->GetSaveData()->plrBattleSkill, 0, sizeof(pPlayer->GetSaveData()->plrBattleSkill));

			pPlayer->UpdateClientShowData(0);
			pPlayer->UpdateClientItemData();
		}
		break;
	case MAP_GM_COMMAND_ATTACK_CALCULATE:
		bool sendData;
		sendData = 1;

		::msgSendData(nClientID, pComm->pcUID, PROTO_MAP_OPEN_ATTACK_CALCULATE, (char *)&sendData, sizeof(sendData), 0);
		break;
	default:
		return(-1);
	}

	return(0);
}

// ===============================================
// C ?I?s???
// ===============================================
void srvPrintf(const char *fmt, ...)
{
	if (CMapServer::m_pServer)
	{
		CMapServer::m_pServer->Log((char *)fmt);
	}
}

// ********************* LRG ?s?W ****************************
// mode = 0 ????O x,y
//	    = 1 ?H x,y ????????d??
//		= 2 ?j?????
long CMapServer::FindPos(long map_id, long x, long y, long x_range, long y_range, long * dx, long * dy, long mode)
{
	long val, px, py;
	long code;
	CMapCtrl * pMapCtrl;
	
	val = 3;
	px = x; py = y;
	if (!x_range && !y_range)
		goto get01;
	// ?O?_???????
	pMapCtrl = CMapServer::GetServer()->FindMap(map_id);
	if (pMapCtrl)
	{
		if (mode == 2)
		{
			px = x + gameGetRandomRange(0, x_range*2) - x_range;
			py = y + gameGetRandomRange(0, y_range*2) - y_range;
			//
			px = gameIconPosCenter(px, gameIconSize);
			py = gameIconPosCenter(py, gameIconSize);
			// goto get01;	Bug: ??b???????
			goto get02;
		}
		//
		if (!mode)
		{
get02:		if ((px >= 0) && (py >= 0))
			{
				code = pMapCtrl->GetIconData(gameIconCalc_DIV(px), gameIconCalc_DIV(py));
				if (!code)
					goto get01;
			}
		}
		while(1)
		{
			px = x + gameGetRandomRange(0, x_range*2) - x_range;
			py = y + gameGetRandomRange(0, y_range*2) - y_range;
			//
			if ((px >= 0) && (py >= 0))		// Bug Fix: 璽计岿粇 2005/10/06
			{
				px = gameIconPosCenter(px, gameIconSize);
				py = gameIconPosCenter(py, gameIconSize);
				//
				code = pMapCtrl->GetIconData(gameIconCalc_DIV(px), gameIconCalc_DIV(py));
				//if (code == MAPCODE_ID_WALL)
				if (!code)
					break;
			}
			if (--val <= 0)
			{
				px = x; py = y;
				goto ret01;
			}
		}
get01:
		*dx = px;
		*dy = py;
		return(1);
	}
//	else
//		CMapServer::GetServer()->Log("ERROR: Find pos but cannot get map ctrl(%d)...", map_id);
	//
ret01:
	*dx = px;
	*dy = py;
	return(0);
}

void CMapServer::Army_RecordDel(long code, long army_id)
{
	m_MapManager.Army_RecordDel(code, army_id);
}
// ================================================================
// Function
// ================================================================
// ptCountryID = 0 ?L???d??(?)
struct plrDATA_WORLD_TOWN_DATA * CMapServer::GetTownDataByID(long town_id, long no_safe_ptr)
{
	struct plrDATA_WORLD_TOWN_DATA * pTown;

#ifdef CROSS_SERVER_CWAR
	if (GetServer()->IsCrossCWarServer())		// ??A???Server????
	{
		if ((town_id >= gameCWAR_KS_STAGE_BEGIN) && (town_id <= gameCWAR_KS_TOWN_STAGE_END))
		{
			town_id -= gameCWAR_KS_STAGE_BEGIN;
		}
	}
#endif
	//
	if (town_id >= gameMAX_WORLD_TOWN)
	{
		if (no_safe_ptr)
			return(NULL);
		pTown = &g_nSafeTownData;
		if (pTown->ptCountryID)
		{
			Log("ERROR: Town data should be zero (%d)...", town_id);
			pTown->ptCountryID = 0;
		}
	}
	else
		pTown = &m_TownData.pwTown[town_id];
	return(pTown);
}

struct plrDATA_WORLD_TOWN_DETAIL * CMapServer::GetTownDetailDataByID(long town_id)
{
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	struct plrDATA_WORLD_TOWN_DETAIL * pTownDetail = NULL;
	
	pTown = GetTownDataByID(town_id);
	if (pTown)
	{
		if (pTown->ptDetailID && (pTown->ptDetailID < gameMAX_WORLD_TOWN_DETAIL))
			pTownDetail = &m_TownData.pwTown2[pTown->ptDetailID];
	}
	//
	if (!pTownDetail)
		pTownDetail = &g_nSafeTownDetailData;

#ifdef NEW_CROSS_SERVER_CWAR
	if (CMapServer::GetServer()->IsCrossCWarServer())
	{
		pTownDetail->ptForceLevel = gameMAX_FORCE_STATUE_LEVEL + gameCITY_SACRIFICE_ADD;
		pTownDetail->ptStatueLevel = gameMAX_FORCE_STATUE_LEVEL + gameCITY_SACRIFICE_ADD;
	}
#endif

	return(pTownDetail);
}

// return: NULL ?L??O????
struct plrDATA_WORLD_FORCE * CMapServer::GetForceDataByID(long country_id)
{
	struct plrDATA_WORLD_FORCE * pForce = NULL;

	if (country_id >= ID_COUNTRY_FORCE01)
	{
		country_id -= ID_COUNTRY_FORCE01;
		if (country_id < gameMAX_PLAYER_COUNTRY_ID)
			pForce = &m_ForceData[country_id];
	}
	else		// ?T???O????
	{
		switch(country_id)
		{
		case ID_COUNTRY_WEI:
			pForce = &g_nForceData[0];
			break;
		case ID_COUNTRY_SHU:
			pForce = &g_nForceData[1];
			break;
		case ID_COUNTRY_WU:
			pForce = &g_nForceData[2];
			break;
		}
	}
	return(pForce);
}

// .......... NPC ?p?L ..........
// ???i?H???? NPC ???????m
// return: 0 ???? ??O ?L???
long CMapServer::PlayerCheckFreeNPCPos(CMapPlayer * pPlayer, long pos, long role_item_code, long summon_role_code)
{
	long r = 0;
	long i, max_num, num;
	struct plrDATA_NPC * pNPCData;
	//
	long plr_code, n;
	struct itemDATA * pItemPtr;
	struct plrDATA_ORIGIN * pOrg;

	pNPCData = pPlayer->GetNPCData();
	//
	max_num = gameGetMaxSoldierNumByJobCfg(pPlayer->GetSaveData()->plrJobID);
	// ????p
	//num = 0;
	num = 1;
	if (role_item_code)
	{
		if (pItemPtr = ::gameGetItemPtr(role_item_code))
		{
			if (pOrg = gameServerGetCharacterPtr(pItemPtr->itemUseMagicID))	// ??????w?q
			{
				num = pOrg->plrSetData.plrCharCell;
			}
		}
	}
	else		// ?l???~???h????
	{
		if (summon_role_code)
		{
			if (pOrg = gameServerGetCharacterPtr(summon_role_code))			// Τà︹﹚竡
			{
				num = pOrg->plrSetData.plrCharCell;
			}
		}
	}
	//
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		if (pNPCData->plrNPCData[i].plrNPC_UID)
		{
		//	num++;
		//	if (num >= max_num)
		//		goto abort;
		//
			n = 1;
			if (plr_code = pNPCData->plrNPCData[i].plrNPC_ItemCode)
			{
				if (pItemPtr = gameGetItemPtr(plr_code))
				{
					if (plr_code = pItemPtr->itemUseMagicID)
					{
						if (pOrg = gameServerGetCharacterPtr(plr_code))
							n = pOrg->plrSetData.plrCharCell;
					}
				}
			}
			else		// ?l???~???h????
			{
				if (plr_code = pNPCData->plrNPCData[i].plrNPC_SummonCode)
				{
					if (pOrg = gameServerGetCharacterPtr(plr_code))
						n = pOrg->plrSetData.plrCharCell;
				}
			}
			num += n;
			if (num > max_num)
				goto abort;
		//
		}
	}
	//
	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		if (!pNPCData->plrNPCData[i].plrNPC_UID)
		{
			r = i+1;
			break;
		}
	}
abort:
	return(r);
}

// ??l??p?L????
void CMapServer::InnerInitCarryNPCData(CMapNPC * pNPC, CMapPlayer * pPlayer, struct plrDATA_NPC_SAVE * pNPCSaveData)
{
	struct plrDATA * pCharData;
	long x, y;
	struct plrDATA_ORIGIN * pOrg;
#ifdef TW_PVP_MODE
	long crime_val;
#endif

	pCharData = pNPC->GetCharData();
	//
	pCharData->plrSaveData.plrMapCode = pPlayer->GetMapCode();
	pNPC->SetMapID(pCharData->plrSaveData.plrMapCode,pCharData->plrSaveData.plrMapCopyCode);
	pNPC->SetBornPos(0, 0);
	pNPC->SetPatrolPos(0, 0);
	pNPC->m_nTalkID = 0;
	pNPC->m_nCNPC_State = 0;			// ????@?w?b???a?e??????
	pNPC->m_nCNPC_UpdateFlag = 1;
	pNPC->m_nPlayerUID = pPlayer->GetUID();		// ???o?? ??O npc uid ?N???O???a?? NPC(??e?H IsNPCUID(uid) ?P?_)
	pNPC->m_nCharFlags |= CHAR_FLAG_NO_MOVE_BLOCK;
	pNPC->m_nCharFlags &= ~(CHAR_NO_TRANSFER_TO_ITEM | CHAR_IS_SUMMON | CHAR_FLAG_BOT_INVISIBLE | CHAR_FLAG_STUN | CHAR_FLAG_CNPC_ENGINEER);
	//
	if (gameIsSoldierItem(pCharData->plrNPC_ItemCode, FLAG_SIT_ENGINEER))
		pNPC->m_nCharFlags |= CHAR_FLAG_CNPC_ENGINEER;
	//
#ifdef USE_ORGANIZE_MAP_PK
	pCharData->plrMapPK_MapCode = pPlayer->GetCharData()->plrMapPK_MapCode;
	pCharData->plrMapPK_OrganizeUID = pPlayer->GetCharData()->plrMapPK_OrganizeUID;
	pCharData->plrSaveData.plrOrganizeUID = pPlayer->GetSaveData()->plrOrganizeUID;
	pCharData->plrShowData.plrOrganizeUID = pCharData->plrSaveData.plrOrganizeUID;
#endif
	//
#ifdef SMART_PLR_DATA2
	pCharData->plrSkillTable = &plrEnemySkillTable;
	pCharData->plrNPC = &plrEnemyNPC;
#endif
#ifdef DEBUG_SMART_PLR_DATA
	//pCharData->plrAIPtr = gameGetCharacterAIPtr(ai_type);
	pCharData->plrOrg = gameServerGetCharacterPtr(pCharData->plrSaveData.plrCode);
	if (pCharData->plrOrg)
		gameServer_SetCharacterAIType(pCharData, pCharData->plrOrg->plrSetData.plrAI_Type, 0);
#endif
	//
	if (pPlayer->IsRedStatus())
	{
		pNPC->pStatus_Set(effFun_RED, 0);
	}
	else
		pNPC->pStatus_Clear(effFun_RED, 0);
#ifdef TW_PVP_MODE
	crime_val = pPlayer->GetSaveData()->plrCrimeVal;
	pNPC->GetSaveData()->plrCrimeVal = crime_val;
	pNPC->GetShowData()->plrCrimeVal = (char)crime_val;
#endif
	//
	pNPC->SetHitObject(false);
	x = pPlayer->GetPosX();
	y = pPlayer->GetPosY();
	pNPC->SetPos(x, y);
	// ----- ?~????a??????-----
	// ???y
	pCharData->plrSaveData.plrCountryID = pPlayer->GetSaveData()->plrCountryID;
	pCharData->plrShowData.plrCountryID = pCharData->plrSaveData.plrCountryID;
	//pCharData->plrSetColor = (unsigned short)::gameGetCountryColor(pCharData->plrSaveData.plrCountryID);
	pCharData->plrSetColor = pPlayer->GetCharData()->plrSetColor;
	pCharData->plrShowData.plrSetColor = pPlayer->GetShowData()->plrSetColor;
	msg_strncpy(pCharData->plrSaveData.plrOrganizeName, pPlayer->GetSaveData()->plrName, sizeof(pCharData->plrSaveData.plrOrganizeName));
	msg_strncpy(pCharData->plrShowData.plrOrganizeName, pPlayer->GetSaveData()->plrName, sizeof(pCharData->plrSaveData.plrOrganizeName));
	// ?S???]?w
	pNPC->m_nCharFlags &= ~CHAR_CLEAR_ALL_SET;
	pOrg = ::gameServerGetCharacterPtr(pCharData->plrSaveData.plrCode);
	if (pOrg)
	{
		pNPC->m_nCharFlags |= pOrg->plrSetData.plrCharFlag;	// CHAR_NO_LEVEL_UP ???]?w
	}
	// ?}??
	pNPC->GetSaveData()->plrMatrix = pNPC->GetCNPC_BA_Code(pPlayer);
 #ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("DEBUG: CNPC matrix(%d)", pNPC->GetSaveData()->plrMatrix);
 #endif
	//
	pCharData->plrNPC_ChangeCode = 0;
	pCharData->plrNPC_ChangeCodeSec = 0;

	//
	if (pNPCSaveData)
	{
		pNPCSaveData->plrNPC_SummonCode = 0;
		// ???? NPC ??(??L???t?~??)
		// NPC ?????CharData ?? SaveData
		gameServer_NPCSave_MakeFullData(pCharData, pNPCSaveData);
		// ???s?p?????l??
		gameNPC_CalcLeader(pPlayer->GetCharData());
	}
}
/*
void CastMagicOnCharImmed(long magic_id, long magic_level, CMapChar * pAttacker, CMapChar * pTarget)
{
	long eff_flag, eff_flag2;
	struct magicDATA * pMagicData;
	long power, spc_power, add_damage;
	long int_val;

	pMagicData = gameMagic_GetPointer(magic_id);
	if (pMagicData)
	{
		eff_flag = pMagicData->magicEffect.effFunction;
		eff_flag2 = eff_flag & ~effFun_ADDMAGICPOWER;
		int_val = gameNPC_CalcLeader_GetIntVal(pAttacker->GetCharData());
		gameGetSkillDamage2(magic_id, magic_level, int_val, &power);
		spc_power = ::gameGetSkillSPCValue(pMagicData, magic_level);
		gameCalcEffect(&pMagicData->magicEffect, pAttacker->GetCharData(), pTarget->GetCharData(), eff_flag2, power, spc_power, &add_damage);
	}
}
*/
// ?s?@??e?X?t??T???????a: ???r???????a %s ???J???~?W??
// type ?????X?R??
void CMapServer::SendPlayerSystemMessageWithItem(CMapPlayer * pPlayer, long type, long res_id, long item_id)
{
	char tmpstr[512];

	tmpstr[0] = 0;
	if (!type)
	{
		wsprintf(tmpstr, gameGetResourceName(res_id), gameGetResourceName(gameGetItemNameIDByID(item_id)));
	}
	//
	if (tmpstr[0])
		pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
}

// ?l??
long CMapServer::CreateNPCFromSummon(long role_code, long summon_time, CMapPlayer * pPlayer, long npc_pos_id)
{
	unsigned long uid;
	long hNPC = -1;
	CMapNPC * pNPC;
	struct plrDATA * pCharData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	struct plrDATA_ORIGIN * pOrg;
	unsigned long long val;
	CMapCharMsg * pMsg;

	if ((npc_pos_id >= 0) && PlayerCheckFreeNPCPos(pPlayer, npc_pos_id, 0, role_code))
	{
		if (pOrg = gameServerGetCharacterPtr(role_code))				// Τà︹﹚竡
		{
			// ????????
			if (gameStageGetStageMode(pPlayer->GetMapCode()) == mapMode_SoulBattle)
			{
				if (!pPlayer->Soul_CanEquipHorseCNPC())
				{
					pPlayer->SendMsgToClientByID(24189);
					return(0);
				}
			}
			else if (gameStageGetStage_NoCNPC(pPlayer->GetMapCode()))
			{
				pPlayer->SendMsgToClientByID(24189);
				return(0);
			}
			//
		//	uid = ::gameNPC_GetUID(pPlayer->GetUID(), npc_pos_id);
			uid = ::gameNPC_GetUID(pPlayer->GetUID(), pPlayer->GetNPCData());	// ???t UID
			if (uid)
			{
				// ????O?_????
				pCharData = pPlayer->GetCharData();
				val = pCharData->plrLeaderUsed + pOrg->plrSetData.plrLeaderUsed;
				if (pCharData->plrSaveData.plrJobID == jobID_WIZARD)	// ΤよΤ
				{
					val = val - pCharData->plrAttrLeader;
					if (val > 0)
					{
						val = val * 2;									// ???O??n 2??
						if ((unsigned long)val > pCharData->plrAttrInt)
						{
							pPlayer->SendMsgToClientByID(24128);
							return(0);
						}
					}
				}
				else
				{
					if ((unsigned long)val > pCharData->plrAttrLeader)
					{
						pPlayer->SendMsgToClientByID(24128);
//GetServer()->Log("CreateNPCFromItem: Player(%d) leader point not enough", pPlayer->GetUID());
						return(0);
					}
				}
				//
				pNPC = (CMapNPC *)FindObjectByUIDForce(uid, CMapNPC::CLASS_ID);
				if (!pNPC)		// ?p?G?????s?b
				{
					hNPC = CreateNPC(role_code, uid, CMapNPC::TYPE_MERCENARY);
//GetServer()->Log("CreateNPCFromItem: NPC handle(%d), uid(%d)", hNPC, uid);
				}
				else
				{
					// ?p?G????s?b?A?i???p?L NPC ?????z?A???????p?L NPC ?|??P
					// ????p?G???O?b delete ?h????
					if (pNPC->GetStateProc() == CMapObject::STATE_DELETE)
					{
						hNPC = pNPC->GetHandle();
						if(!pNPC->IsOutMap())
							pNPC->LeaveMap();
					//	pNPC->SetStateProc(CMapObject::STATE_ENTER_MAP);
					pNPC->SetStateProc(CMapObject::STATE_NORMAL);
						//pNPC->EnterMap();
						//ChangeObjectState(hNPC, CMapObject::STATE_CREATE);
						//update_show_flg++;
						goto mrh001;
					}
					else
						GetServer()->Log("CreateNPCSummon: NPC (%d) already exist", uid);
				}
			}
			else
				GetServer()->Log("CreateNPCSummon: Can not get NPC uid (player id = %d)", pPlayer->GetUID());
		}
		//
		if (hNPC != -1)
		{
			pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByHandle(hNPC);
			if(pNPC != NULL)
			{
mrh001:			pCharData = pNPC->GetCharData();
				pNPCSaveData = &pPlayer->GetNPCData()->plrNPCData[npc_pos_id];
				//gameMakeItem_ItemToNPC(pItem, pCharData, pPlayer->GetUID(), npc_pos_id, uid, dead_force);
				//gameServer_NPC_MakeFullData2(role_code, uid, pCharData, pPlayer->GetSaveData()->plrMatrix);
				gameServer_NPC_MakeFullData2(role_code, uid, pCharData, pNPC->GetCNPC_BA_Code(pPlayer));
//GetServer()->Log("NPC HP: %d", pCharData->plrSaveData.plrHP);
				pNPCSaveData->plrNPC_UID = uid;
				// ??l??p?L
				//InnerInitCarryNPCData(pNPC, pPlayer, pNPCSaveData);
				InnerInitCarryNPCData(pNPC, pPlayer);
				// ??h?l??[??
				//pCharData->plrSummon_Wizard = 0;
				if (pPlayer->GetSaveData()->plrJobID == jobID_WIZARD)
				{
					//pCharData->plrSummon_Wizard |= 1;
					//gameServer_CalcCharacterAttribute(pCharData);
					val = pNPC->GetSaveData()->plrBaseDefense;
					val = val * pPlayer->GetSaveData()->plrLevel / 100;		// 50??+0.5??, 100??+1??
					if (val > 999999999999LL)
						val = 999999999999LL;
					pNPCSaveData->plrNPC_AddDefense = val;
					pCharData->plrNPC_AddDefense = val;
					pNPC->GetSaveData()->plrBaseDefense += val;
					// ?w?]??O?G????B????B?K??
					//gameSetHelpMagic(effFun_ADDATTACKPOWER | effFun_ADDDEFENSEPOWER | effFun_ADDHIT, pNPC->GetCharData(), 20, SUMMON_TIME_SEC+10);
					//gameSetHelpMagic(effFun_ADDHITMISS, pNPC->GetCharData(), 30, SUMMON_TIME_SEC+10);
					//
					pNPC->GetSaveData()->plrStatus |= effFun_WIZARD_SUMMON;
				}
				//
				if (!summon_time)
					summon_time = SUMMON_TIME_SEC;
				//
				pNPC->m_nCharFlags |= CHAR_NO_TRANSFER_TO_ITEM;
				//pNPC->m_nCharFlags |= CHAR_NO_LEVEL_UP;
				pNPCSaveData->plrNPC_ItemCode = 0;
				pNPCSaveData->plrNPC_Loyalty = 100;
				pNPCSaveData->plrNPC_SummonCode = (unsigned short)role_code;
				pNPCSaveData->plrNPC_SummonSec = (unsigned short)summon_time;
				//pNPCSaveData->plrNPC_SummonSec = 80;	// ???? 80??
				//
				//
				pCharData->plrNPC_ItemCode = pNPCSaveData->plrNPC_ItemCode;
				pCharData->plrNPC_Loyalty = pNPCSaveData->plrNPC_Loyalty;
				pCharData->plrNPC_SummonCode = pNPCSaveData->plrNPC_SummonCode;
				pCharData->plrNPC_SummonSec = pNPCSaveData->plrNPC_SummonSec;
				// ?P???a?????P
//Log("Summon: %d, %d", pCharData->plrSaveData.plrBaseAttrStr, pCharData->plrSaveData.plrBaseAttrInt);
				pCharData->plrSaveData.plrLevel = pPlayer->GetLevel();
				gameServer_Recalc_Level(pCharData);
				gameServer_NPCSave_MakeFullData(pCharData, pNPCSaveData);
//Log("Summon: %d, %d", pCharData->plrSaveData.plrBaseAttrStr, pCharData->plrSaveData.plrBaseAttrInt);
//Log("Summon: %d, %d", pCharData->plrDefense, pCharData->plrAttackPowerMax);
				//
				// ?q?????A
				pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_CNPC_SUMMON_SEC, 0, pCharData->plrNPC_SummonSec, pNPC->GetUID());
				
				pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				// ?q?? Client NPC ??????
				struct plrDATA_NPCSHOW npcShow;
				struct MAP_GET_SOLDIER_RESULT sendData;

				::gameServer_MakeNPC(pPlayer->GetNPCData(), &npcShow, pPlayer);
				sendData.solIdx = npc_pos_id;
				::memcpyT(&sendData.solShowSelf, &npcShow.plrNPCData[ npc_pos_id ], sizeof(sendData.solShowSelf));
				::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_SOLDIER_RESULT, (char *)&sendData, sizeof(sendData), 0 );
				// ???s?p?????l??
				gameNPC_CalcLeader(pPlayer->GetCharData());
				// ?q?? Client ??s???
				pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				
				pMsg->m_Msg.m_UpdateData2Msg.Reset();
				pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_LEADER_USED, 0, pPlayer->GetCharData()->plrLeaderUsed);

				pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				//
				//pPlayer->AutoSaveNPCData();
				//
				pPlayer->ClearNoAttackMode();
//if (pNPC->GetSaveData()->plrStatus & effFun_WIZARD_SUMMON)
//	CMapServer::GetServer()->Log("DEBUG: Has summon flag(0x%08X) !!!", pNPCSaveData->plrNPC_Status);
				return(1);
			}
			else
			{
				GetServer()->Log("ERROR: CreateNPCSummon: NPC object not found (%d) ", hNPC);
				DeleteObject(hNPC);
			}
		}
	}
	else
	{
		pPlayer->SendMsgToClientByID(24127);
//GetServer()->Log("CreateNPCFromItem: No NPC room for Player(%d) ", pPlayer->GetUID());
	}
	return(0);
}

// ?? ???~ ?? NPC(?W?h?R?????~)
long CMapServer::CreateNPCFromItem(struct itemDATA_SAVE * pItem, CMapPlayer * pPlayer, long npc_pos_id)
{
	unsigned long uid;
	struct itemDATA * pItemPtr;
	long hNPC = -1;
	CMapNPC * pNPC;
	struct plrDATA * pCharData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;
	struct plrDATA_ORIGIN * pOrg;
	//long update_show_flg = 0;
	long dead_force = 1;		// ?p?L???`???\??X??
	long role_code = 0;
	long lvl_diff;
	long SuperSoldier = 0;

	if (pItem->m_NPC.inpcFlags & itemSHOW_FLAG_ALL_CNPC)
	{
		if (!pPlayer->IsReady())
			return(0);
		if (gameStageGetStageMode(pPlayer->GetMapCode()) == mapMode_SoulBattle)
		{
			if (!pPlayer->Soul_CanEquipHorseCNPC())
			{
				pPlayer->SendMsgToClientByID(24189);
				return(0);
			}
		}
		else if (gameStageGetStage_NoCNPC(pPlayer->GetMapCode()))
		{
			pPlayer->SendMsgToClientByID(24189);
			return(0);
		}
		//
		if (!dead_force)
		{
			if (pItem->m_NPC.inpcHP == 0)
			{
				pPlayer->SendMsgToClientByID(24129);
				return(0);
			}
		}
		// ??~?????????
		if (pPlayer->GetSaveData()->plrJobID == jobID_ENGINEER)
		{
			if (!gameIsSoldierItem(pItem->m_NPC.inpcItemCode, FLAG_SIT_ENGINEER | FLAG_SIT_SIEGE))
			{
				pPlayer->SendMsgToClientByID(24453);	//??~????A?L?k???????~?I
				return(0);
			}
			// ???????A????O?_????
			if (pPlayer->GetSaveData()->plrEquipEngineer.m_Item.itemCode)
			{
				pPlayer->SendMsgToClientByID(5000068);	//叫秆埃砰篈
				return(0);
			}
		}
		else
		{
			if (gameIsSoldierItem(pItem->m_NPC.inpcItemCode, FLAG_SIT_ENGINEER))
			{
				pPlayer->SendMsgToClientByID(24453);	//??~????A?L?k???????~?I
				return(0);
			}
		}
		// ???C???????
		lvl_diff = pItem->m_NPC.inpcLevel - pPlayer->GetSaveData()->plrLevel;
		if (lvl_diff > gameCARRY_CNPC_LEVEL_LOW)
		{
			pPlayer->SendMsgToClientByID(20589); //单ぃì礚猭盿烩
			return(0);
		}
		// ??N?u???@??
#ifdef SUPER_SOLDIER
		int i;
		struct gameCNPC_SKILL* pNpcSkill = NULL;
		pItemPtr = ::gameGetItemPtr(pItem->m_NPC.inpcItemCode);
		if(pItemPtr)
		{
			for(i = 0 ; i < gameMAX_CNPC_Skill_SETTING; i++)
			{
				pNpcSkill = gameGetCNPC_SkillPtr(i);
				if (pNpcSkill && pNpcSkill->csSolCode == pItemPtr->itemUseMagicID)
					break;
				pNpcSkill = NULL;
			}
		}
		SuperSoldier = pPlayer->CheckSuperSoldier();
		if( SuperSoldier && pNpcSkill )
		{
			pPlayer->SendMsgToClientByID(5000317); //??N?u???@???I
			return(0);
		}
#endif
		//
		if ((npc_pos_id >= 0) && PlayerCheckFreeNPCPos(pPlayer, npc_pos_id, pItem->m_NPC.inpcItemCode))
		{
		//	uid = ::gameNPC_GetUID(pPlayer->GetUID(), npc_pos_id);
			uid = ::gameNPC_GetUID(pPlayer->GetUID(), pPlayer->GetNPCData());
			if (uid)
			{
				if (pItemPtr = ::gameGetItemPtr(pItem->m_NPC.inpcItemCode))
				{
					if (pOrg = gameServerGetCharacterPtr(pItemPtr->itemUseMagicID))	// ??????w?q
					{
						role_code = pItemPtr->itemUseMagicID;
						// ????O?_????
						pCharData = pPlayer->GetCharData();
						if ((pCharData->plrLeaderUsed + pOrg->plrSetData.plrLeaderUsed) > pCharData->plrAttrLeader)
						{
							pPlayer->SendMsgToClientByID(24128);
//GetServer()->Log("CreateNPCFromItem: Player(%d) leader point not enough", pPlayer->GetUID());
							return(0);
						}
						//
						pNPC = (CMapNPC *)FindObjectByUIDForce(uid, CMapNPC::CLASS_ID);
						if (!pNPC)		// ?p?G?????s?b
						{
							hNPC = CreateNPC(pItemPtr->itemUseMagicID, uid, CMapNPC::TYPE_MERCENARY);
//GetServer()->Log("CreateNPCFromItem: NPC handle(%d), uid(%d)", hNPC, uid);
						}
						else
						{
							// ?p?G????s?b?A?i???p?L NPC ?????z?A???????p?L NPC ?|??P
							// ????p?G???O?b delete ?h????
							if (pNPC->GetStateProc() == CMapObject::STATE_DELETE)
							{
								hNPC = pNPC->GetHandle();
								if(!pNPC->IsOutMap())
									pNPC->LeaveMap();
							//	pNPC->SetStateProc(CMapObject::STATE_ENTER_MAP);
							pNPC->SetStateProc(CMapObject::STATE_NORMAL);
								//pNPC->EnterMap();
								//ChangeObjectState(hNPC, CMapObject::STATE_CREATE);
								//update_show_flg++;
								goto mrh001;
							}
							else
								GetServer()->Log("CreateNPCFromItem: NPC (%d) already exist", uid);
						}
					}
				}	
			}
			else
				GetServer()->Log("CreateNPCFromItem: Can not get NPC uid (player id = %d)", pPlayer->GetUID());
			if (hNPC != -1)
			{
				pNPC = (CMapNPC *)CMapServer::GetServer()->FindObjectByHandle(hNPC);
				if(pNPC != NULL)
				{
	mrh001:			pCharData = pNPC->GetCharData();
					pNPCSaveData = &pPlayer->GetNPCData()->plrNPCData[npc_pos_id];
					gameMakeItem_ItemToNPC(pPlayer->GetSaveData(), pItem, pCharData, pPlayer->GetUID(), npc_pos_id, uid, dead_force, pNPC->GetCNPC_BA_Code(pPlayer));
//GetServer()->Log("NPC HP: %d", pCharData->plrSaveData.plrHP);
				//	pNPCSaveData->plrNPC_UID = uid;
					// ??l??p?L
					InnerInitCarryNPCData(pNPC, pPlayer, pNPCSaveData);
					// UID ????
#ifdef SMART_PLR_DATA2
					pNPC->GetCharData()->plrNPC_ItemUID = pItem->m_NPC.inpcUID;
					pNPC->GetCharData()->plrNPC_Flags = 1;
#else
					pNPC->GetCharData()->plrNPC.plrNPCData[0].plrNPC_ItemUID = pItem->m_NPC.inpcUID;
					pNPC->GetCharData()->plrNPC.plrNPCData[0].plrNPC_Flags = 1;
#endif
					{
						struct CNPC_ITEM_SKILL_SAVE deploySkills[gameMAX_SPECIAL_ATTACK];
						if (gameCNPC_ItemSkillCopyForDeploy(pPlayer->GetUID(), &pItem->m_NPC.inpcUID, deploySkills))
							gameCNPC_ApplyItemSkillsToBattle(pCharData, deploySkills);
					}
					if (pItem->m_NPC.inpcHP == 0)
						pNPC->SetHP(0);
					//
				/*	pCharData->plrSaveData.plrMapCode = pPlayer->GetMapCode();
					pNPC->SetMapID(pCharData->plrSaveData.plrMapCode);
					pNPC->SetBornPos(0, 0);
					pNPC->SetPatrolPos(0, 0);
					pNPC->m_nTalkID = 0;
					pNPC->m_nCNPC_State = 0;			// ????@?w?b???a?e??????
					pNPC->m_nCNPC_UpdateFlag = 1;
					pNPC->m_nPlayerUID = pPlayer->GetUID();		// ???o?? ??O npc uid ?N???O???a?? NPC(??e?H IsNPCUID(uid) ?P?_)
					pNPC->m_nCharFlags |= CHAR_FLAG_NO_MOVE_BLOCK;
					pNPC->m_nCharFlags &= ~CHAR_NO_TRANSFER_TO_ITEM;
					if (role_code)						// ????????~???l???~
					{
						if (IsSpecialNPC(0, role_code))
						{
							pNPC->m_nCharFlags |= CHAR_NO_TRANSFER_TO_ITEM;
							pNPC->plrNPC_ItemCode = 0;
						}
					}
					pNPC->SetHitObject(false);
					// ??m
					x = pPlayer->GetPosX();
					y = pPlayer->GetPosY();
					pNPC->SetPos(x, y);
					// ----- ?~????a??????-----
					// ???y
					pCharData->plrSaveData.plrCountryID = pPlayer->GetSaveData()->plrCountryID;
					pCharData->plrShowData.plrCountryID = pCharData->plrSaveData.plrCountryID;
					pCharData->plrSetColor = (unsigned short)::gameGetCountryColor(pCharData->plrSaveData.plrCountryID);
					pCharData->plrShowData.plrSetColor = pCharData->plrSetColor;
					msg_strncpy(pCharData->plrSaveData.plrOrganizeName, pPlayer->GetSaveData()->plrName, sizeof(pCharData->plrSaveData.plrOrganizeName));
					msg_strncpy(pCharData->plrShowData.plrOrganizeName, pPlayer->GetSaveData()->plrName, sizeof(pCharData->plrSaveData.plrOrganizeName));
					//
					// ???? NPC ??(??L???t?~??)
					// NPC ?????CharData ?? SaveData
					gameServer_NPCSave_MakeFullData(pCharData, pNPCSaveData);
					// ???s?p?????l??
					gameNPC_CalcLeader(pPlayer->GetCharData());
				*/
					if (role_code)						// ????????~???l???~
					{
						if (IsSpecialNPC(0, role_code))
						{
							pNPC->m_nCharFlags |= CHAR_NO_TRANSFER_TO_ITEM;
							//pNPC->m_nCharFlags |= CHAR_NO_LEVEL_UP;
							pNPCSaveData->plrNPC_ItemCode = 0;
							pNPCSaveData->plrNPC_SummonCode = (unsigned short)role_code;
#ifdef USE_SUMMON_ITEM_TIME_LIMIT
							pNPCSaveData->plrNPC_SummonSec = SUMMON_TIME_SEC;
#endif
							//
							pCharData->plrNPC_ItemCode = pNPCSaveData->plrNPC_ItemCode;
							pCharData->plrNPC_Loyalty = pNPCSaveData->plrNPC_Loyalty;
							pCharData->plrNPC_SummonCode = pNPCSaveData->plrNPC_SummonCode;
#ifdef USE_SUMMON_ITEM_TIME_LIMIT
							pCharData->plrNPC_SummonSec = pNPCSaveData->plrNPC_SummonSec;
#endif
							// ?P???a?????P
							pCharData->plrSaveData.plrLevel = pPlayer->GetLevel();
							gameServer_Recalc_Level(pCharData);
							gameServer_NPCSave_MakeFullData(pCharData, pNPCSaveData);
							//
							// ?q?????A
							CMapCharMsg * pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
							
							pMsg->m_Msg.m_UpdateData2Msg.Reset();
							pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_CNPC_SUMMON_SEC, 0, pCharData->plrNPC_SummonSec, pNPC->GetUID());

							pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
						}
					}
					// ????p?L?????????
					pNPC->m_nAttackInputDelay = 1000 * 3;	// 3ずぃю阑
					pNPC->m_nCastInputDelay = 1000 * 3;
					// ?W?h?q?? Client NPC ??????
					//
					//if (update_show_flg)
					//	pNPC->UpdateShowData();
//GetServer()->Log("NPC HP: %d", pCharData->plrSaveData.plrHP);
					//
//GetServer()->Log("NPC Status: %d", pNPC->GetStateProc());
				//	pNPC->SetStateProc(CMapObject::STATE_NORMAL);
				//	GetServer()->ChangeObjectState(pNPC->GetHandle(), CMapObject::STATE_ENTER_MAP);
					//
					pPlayer->AutoSaveNPCData();
					//
					{
						struct MAP_GET_SOLDIER_RESULT sendData;
						::memset(&sendData, 0, sizeof(sendData));
						::gameServer_NPC_MakeShowSelf(pCharData, &sendData.solShowSelf);
						sendData.solIdx = npc_pos_id;
						::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_GET_SOLDIER_RESULT, (char *)&sendData, sizeof(sendData), 0);
					}
					//
					pPlayer->ClearNoAttackMode();
					return(1);
				}
				else
				{
					GetServer()->Log("ERROR: CreateNPCFromItem: NPC object not found (%d) ", hNPC);
					DeleteObject(hNPC);
				}
			}
		}
		else
		{
			pPlayer->SendMsgToClientByID(24127);
//GetServer()->Log("CreateNPCFromItem: No NPC room for Player(%d) ", pPlayer->GetUID());
		}
	}
	return(0);
}

// ?? NPC ?? ???~
long CMapServer::InsertItemFromNPC(CMapPlayer * pPlayer, unsigned long npcUID, struct itemDATA_SAVE * pItem)
{
	long i;
	CMapNPC * pNPC;
	struct plrDATA_NPC * pNPCData;
	struct plrDATA_NPC_SAVE * pNPCSaveData;

	if (!pPlayer->IsReady())
		return(0);
	if (npcUID)
	{
		pNPCData = pPlayer->GetNPCData();
		for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
		{
			pNPCSaveData = &pNPCData->plrNPCData[i];
			if (pNPCSaveData->plrNPC_UID == npcUID)
			{
				pNPC = (CMapNPC *)FindObjectByUID(npcUID, CMapNPC::CLASS_ID);
				if (pNPC)
				{
					pNPC->SendMsgToPlayer();		// р程癟肚
					//
					memset(pItem, 0, sizeof(struct itemDATA_SAVE));
					// ?L?k???_?A?j???R??
					if (pNPC->m_nCharFlags & CHAR_NO_TRANSFER_TO_ITEM)
					{
						goto del_npc;
					}
					// ????~????m, ?s?W???~
					if (gameMakeItem_NPCToItem(pNPC->GetCharData(), pItem, GenerateItemUID()))
					{
					//	if (InsertItemToPlayer( pItem, 1, pPlayer->GetCharData() ))
						{
#ifdef SMART_PLR_DATA2
							if (pNPC->GetCharData()->plrNPC_Flags & 1)
								pItem->m_NPC.inpcUID = pNPC->GetCharData()->plrNPC_ItemUID;
#else
							if (pNPC->GetCharData()->plrNPC.plrNPCData[0].plrNPC_Flags & 1)
								pItem->m_NPC.inpcUID = pNPC->GetCharData()->plrNPC.plrNPCData[0].plrNPC_ItemUID;
#endif
							//
		del_npc:			memset(&pNPCData->plrNPCData[i], 0, sizeof(pNPCData->plrNPCData[i]));
							// ???s?p?????l??
							gameNPC_CalcLeader(pPlayer->GetCharData());
							// ?q?? Client NPC ?R??????
							// !!!
							// ???\??R?? NPC
							DeleteObject(pNPC->GetHandle());
							//
							pPlayer->AutoSaveNPCData();
							if (!gameCNPC_ItemSkillDbUidEmpty(&pItem->m_NPC.inpcUID))
								pPlayer->SyncCnpcItemSkillEntry(&pItem->m_NPC.inpcUID);
							return(1);
						}
					}
				}
				else
					GetServer()->Log("ERROR: NPC to item but not found object(%08x) ", npcUID);
				break;
			}
		}
	}
	return(0);
}

// ?O?_?O?S??NPC:?l???~?B???L????
// type = 0 ?l???~
long CMapServer::IsSpecialNPC(long type, long role_code)
{
	struct plrDATA_ORIGIN * pOrg;

	pOrg = gameServerGetCharacterPtr(role_code);
	if (pOrg)
	{
		switch(type)
		{
		case 0:
			if (pOrg->plrSetData.plrClass == classSUMMON_CREATURE)
				return(1);
			break;
		case 1:
			if (pOrg->plrSetData.plrClass == classSIEGE_WEAPON)
				return(1);
			break;
		}
	}
	return(0);
}

// ?p?L???
long CMapServer::ChangeCarryNPCCode(CMapPlayer * pPlayer, CMapNPC * pNPC, long role_code, long time_sec)
{
	unsigned long npc_uid;
	struct plrDATA_NPC_SAVE * pNPCSave;
	CMapCharMsg * pMsg;

	npc_uid = pNPC->GetUID();
	pNPCSave = pPlayer->GetCarryNPCDataPtr(npc_uid);
	if (pNPCSave)
	{
 #ifndef USE_PACKAGE_DATA
		Log("DEBUG: CNPC change code(%s,%d,%d)(%s)", pNPC->GetName(), role_code, time_sec, pPlayer->GetName());
 #endif
		// ????]?w:  ???B?l?? ??????F???`??O???^?M?????
		if (pNPC->m_nCharFlags & CHAR_IS_SUMMON)
			goto abort;
		if (pNPC->m_nCharFlags & CHAR_NO_LEVEL_UP)	// ???L?? ??O ?l???~
			goto abort;
		// ???\??\
		if (pNPCSave->plrNPC_ChangeCodeSec && pNPCSave->plrNPC_ChangeCode)
		{
			pPlayer->SendMsgToClientByID(24152);
			goto abort;
		}
		// ?]?w?p?L???s?????
		if (time_sec > 65535)
			time_sec = 65535;
		pNPCSave->plrNPC_ChangeCode = (unsigned short)role_code;				// NPC ?x?s??
		pNPCSave->plrNPC_ChangeCodeSec = (unsigned short)time_sec;
		pNPC->GetCharData()->plrNPC_ChangeCode = (unsigned short)role_code;		// à︹ <-> NPC 纗 锣传既ノ穦紇臫 MakeShow  plrWID
		pNPC->GetCharData()->plrNPC_ChangeCodeSec = (unsigned short)time_sec;
		//
		pPlayer->AutoSaveNPCData();
		// ??G?q??
		pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_ALL);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_CNPC_CHANGE_CODE, (short)time_sec, pNPC->GetUID(), role_code);

		pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		//
		pPlayer->SendMsgToPlayer();						// ????T????X
		//
		pNPC->UpdateShowData(1);								// 穝 Client 陪ボ
		return(1);
	}
abort:
	return(0);
}

// ...........
// ??????x?s??m
void CMapServer::PlayerSavePos(CMapPlayer * pPlayer, long disp_x, long disp_y)
{
	struct plrDATASHOWSELF Self;

	if (pPlayer)
	{
		pPlayer->GetSaveData()->plrSavePosX = pPlayer->GetPosX() + disp_x;
		pPlayer->GetSaveData()->plrSavePosY = pPlayer->GetPosY() + disp_y;
		pPlayer->GetSaveData()->plrSaveMapCode = pPlayer->GetMapID();
		//pPlayer->SaveAllData();
		pPlayer->AutoSaveCharData();
		//
		::memset(&Self, 0, sizeof(plrDATASHOWSELF));
		::gameServer_MakeShowSelf(pPlayer->GetCharData(), &Self);
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_GET_PLAYER_SHOW_DATA_RESULT, (char *)&Self, sizeof( struct plrDATASHOWSELF ), 0 );
	}
}

// ???????_???A
void CMapServer::PlayerRestoreAll(CMapPlayer * pPlayer)
{
	if (pPlayer)
	{
		gameEff_ClearObjectSingleStatus(0, pPlayer->GetCharData(), 0, -2);

		pPlayer->SetHP(pPlayer->GetMaxHP());
		pPlayer->SetMP(pPlayer->GetMaxMP());
		pPlayer->GetSaveData()->plrStatus &= ~(effFUN_STATUS_ALL);
		pPlayer->GetShowData()->plrStatus &= ~(effFUN_STATUS_ALL);
		pPlayer->UpdatePlayerDataPart1();
		pPlayer->UpdateCharStatus(1);
		// ??_?p?L???A
		pPlayer->FullRestoreCarryNPC();
	}
}

// ==================================================
// ?g Log ?????Log Server
// ==================================================
void CMapServer::SendLogMessage_System(CMapPlayer * pPlayer, long type)
{
	struct LOG_INSERT_MSG_SYSTEM nData;

	if (IsWar())
		return;
	if(IsConnectedToServer(m_hLogServer))
	{
		memset(&nData, 0, sizeof(nData));
		nData.nType = (unsigned short)type;
		if (pPlayer)
		{
			//nData.nAccountID = pPlayer->GetAccountID();
			nData.nUID = pPlayer->GetUID();
			nData.nMapCode = pPlayer->GetMapCode();
			msg_strncpy(nData.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nData.szAccount));
			msg_strncpy(nData.szCharName, pPlayer->GetSaveData()->plrName, sizeof(nData.szCharName));
			//
			if (type == LOGTYPE_SYS_BOT)
			{
				nData.nPosX = 0;
				nData.nPosY = 0;
				nData.nData1[0] = pPlayer->m_nBotReason[0];
				nData.nData1[1] = pPlayer->m_nBotReason[1];
				nData.nData1[2] = pPlayer->m_nBotReason[2];
			}
			else if (type == LOGTYPE_SYS_BOT_COUNT)
			{
				nData.nPosX = 0;
				nData.nPosY = 0;
				nData.nData1[0] = pPlayer->m_nBotReason[0];
				nData.nData1[1] = pPlayer->m_nBotReason[1];
				nData.nData1[2] = pPlayer->m_nBotReason[2];
			}
			else
			{
				nData.nPosX = pPlayer->GetPosX();
				nData.nPosY = pPlayer->GetPosY();
			}
			msg_strncpy(nData.szIP, napiServer_GetClientIP(pPlayer->GetClientID()), sizeof(nData.szIP));
		}
		SendData(m_hLogServer, 0, PROTO_LOG_INSERT_MSG_SYSTEM, (char *)&nData, sizeof(nData), 0);
	}
}

void CMapServer::SendLogMessage_Talk(CMapPlayer * pPlayer, long type, char * char_chat_name, char * msg)
{
	struct LOG_INSERT_MSG_TALK nData;

	if (IsWar())
		return;
	if(IsConnectedToServer(m_hLogServer))
	{
		memset(&nData, 0, sizeof(nData));
		nData.nType = (unsigned short)type;
		if (pPlayer)
		{
			//nData.nAccountID = pPlayer->GetAccountID();
			nData.nUID = pPlayer->GetUID();
			nData.nMapCode = pPlayer->GetMapID();
			//msg_strncpy(nData.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nData.szAccount));
			msg_strncpy(nData.szCharName, pPlayer->GetSaveData()->plrName, sizeof(nData.szCharName));
			//
			msg_strncpy(nData.szTargetCharName, char_chat_name, sizeof(nData.szTargetCharName));
			msg_strncpy(nData.szBuffer, msg, sizeof(nData.szBuffer));
			//
			nData.nPosX = pPlayer->GetPosX();
			nData.nPosY = pPlayer->GetPosY();
			msg_strncpy(nData.szIP, napiServer_GetClientIP(pPlayer->GetClientID()), sizeof(nData.szIP));
		}
		SendData(m_hLogServer, 0, PROTO_LOG_INSERT_MSG_TALK, (char *)&nData, sizeof(nData), 0);
	}
}

// pTarget ?O????a??O??H
void CMapServer::SendLogMessage_Item(CMapPlayer * pPlayer, long type, CMapChar * pTarget, struct itemDATA_SAVE * pItem, struct itemDATA_SAVE * pItem_new)
{
	struct itemDATA * pItemPtr = NULL;
	struct LOG_INSERT_MSG_ITEM nData;
	//long a_str, a_int, a_mind;
	//long a_con, a_dex, a_leader;

	if(IsConnectedToServer(m_hLogServer))
	{
		if (pItem->m_Item.itemCode == (item_Money & 0xffff))
		{
		//	if (pItem->m_Item.itemGoldNumber >= 1000)
			{
				if (!pItem->m_Item.itemGoldNumber)
					return;
				memset(&nData, 0, sizeof(nData));
				nData.nType = (unsigned short)type;
				nData.nItem.itemCode = pItem->m_Item.itemCode;
				nData.nItem.itemNumber = pItem->m_Item.itemGoldNumber;
				nData.nItem.itemLevel = (unsigned short)m_ServerInfo.m_LogItemLevel;
				strcpy(nData.szItemName, SMAP_LOG_MONEY);		// 窥
				goto do_log;
			}
		}
		else if (pItem->m_Item.itemCode == (item_SkillExp & 0xffff))
		{
			memset(&nData, 0, sizeof(nData));
			nData.nType = (unsigned short)type;
			nData.nItem.itemCode = pItem->m_Item.itemCode;
			nData.nItem.itemNumber = pItem->m_Item.itemGoldNumber;
			nData.nItem.itemLevel = (unsigned short)m_ServerInfo.m_LogItemLevel;
			strcpy(nData.szItemName, SMAP_LOG_SKILLEXP);		// ???g??
			goto do_log;
		}
		else if (pItem->m_Item.itemCode == (item_Honor & 0xffff))
		{
			memset(&nData, 0, sizeof(nData));
			nData.nType = (unsigned short)type;
			nData.nItem.itemCode = pItem->m_Item.itemCode;
			nData.nItem.itemNumber = pItem->m_Item.itemGoldNumber;
			nData.nItem.itemLevel = (unsigned short)m_ServerInfo.m_LogItemLevel;
			strcpy(nData.szItemName, SMAP_LOG_HONOR);			// ?\??
			goto do_log;
		}
		//
		if (pItemPtr = ::gameGetItemPtr(pItem->m_Item.itemCode))
		{
			if (type == LOGTYPE_ITEM_GET_FROM_WEBPOINT)		// ???n type ?j??O??
				goto force_log;
			if (type == LOGTYPE_ITEM_SOL_REVIVE_ESCAPE)
				goto force_log;
			if ((type == LOGTYPE_ITEM_NPC_CHANGE_LOST) || (type == LOGTYPE_ITEM_NPC_CHANGE_GET))
				goto force_log;
			if (pItemPtr->itemCostLevel >= m_ServerInfo.m_LogItemLevel)
			{
force_log:		memset(&nData, 0, sizeof(nData));
				nData.nType = (unsigned short)type;
				nData.nItem.itemNumber = pItem->m_Item.itemNumber;
				nData.nItem.itemLevel = pItemPtr->itemCostLevel;
				nData.nItem.itemCode = pItem->m_Item.itemCode;
				//
				memcpyT(&nData.nItem.itemUID, &pItem->m_Item.itemUID, sizeof(nData.nItem.itemUID));
				memcpyT(&nData.nItem.itemMod, &pItem->m_Item.itemMod, sizeof(nData.nItem.itemMod));
				//
				// ???~?W??
			//	if (pItem->m_Item.itemCode & 0x80000000)
			//	{
			//		if (pItem->m_Item.itemCode == (item_Money & 0xffff))
			//		{
			//			if (!pItem->m_Item.itemGoldNumber)
			//				return;
			//			strcpy(nData.szItemName, "窥");
			//		}
			//		else
			//			nData.szItemName[0] = 0;
			//	}
			//	else
				{
					//msg_strncpy(nData.szItemName, gameGetResourceName(gameGetItemNameIDByID(pItem->m_Item.itemCode)), sizeof(nData.szItemName));
					char tmpstr[128];

					gameMakeSaveItemName(pItem, tmpstr);
					msg_strncpy(nData.szItemName, tmpstr, sizeof(nData.szItemName));
				}
do_log:			if (pPlayer)
				{
					if (pItem_new)
					{
						if ((type == LOGTYPE_ITEM_MERGE_DISAPPEAR) || (type == LOGTYPE_ITEM_SEPARATE))
						{
							memcpyT(&nData.itemUID_New, &pItem_new->m_Item.itemUID, sizeof(nData.itemUID_New));
						}
					}
					//
					//nData.nAccountID = pPlayer->GetAccountID();
					nData.nUID = pPlayer->GetUID();
					nData.nMapCode = pPlayer->GetMapID();
					//msg_strncpy(nData.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nData.szAccount));
					msg_strncpy(nData.szCharName, pPlayer->GetSaveData()->plrName, sizeof(nData.szCharName));
					//
					
					//
					nData.nPosX = pPlayer->GetPosX();
					nData.nPosY = pPlayer->GetPosY();
					//msg_strncpy(nData.szIP, napiServer_GetClientIP(pPlayer->GetClientID()), sizeof(nData.szIP));
					//
					if (type == LOGTYPE_ITEM_SOL_REVIVE_ESCAPE)
					{
						nData.nPosX = pItem->m_NPC.inpcLevel;
						nData.nPosY = pItem->m_NPC.inpcLoyalty;
						//
						//nData.itemUID_New.iUID_Time = pItem->m_NPC.inpcAttrBit2;
						//nData.itemUID_New.iUID_Serial = pItem->m_NPC.inpcAttrBit1;
					}
					if ((type == LOGTYPE_ITEM_NPC_CHANGE_LOST) || (type == LOGTYPE_ITEM_NPC_CHANGE_GET))
					{
						//gameSoldierItem_GetAttrVal(pItem->m_NPC.inpcAttrBit1, &a_str, &a_int, &a_mind);
						//gameSoldierItem_GetAttrVal(pItem->m_NPC.inpcAttrBit2, &a_con, &a_dex, &a_leader);
						//
						nData.nPosX = pItem->m_NPC.inpcLevel;
						nData.nPosY = pItem->m_NPC.inpcAddDefense;
						nData.nMapCode = pItem->m_NPC.inpcAddAttack;
						//nData.itemUID_New.iUID_Time = pItem->m_NPC.inpcAttrBit2;
						//nData.itemUID_New.iUID_Serial = pItem->m_NPC.inpcAttrBit1;
					}
					if ((type == LOGTYPE_ITEM_ARMY_STORAGE_GET_ITEM) || (type == LOGTYPE_ITEM_ARMY_STORAGE_PUT_ITEM))	// 瓁刮畐珇
					{
						nData.itemUID_New.iUID_Serial = 0;
						nData.itemUID_New.iUID_Time = pPlayer->GetSaveData()->plrOrganizeUID;
						msg_strncpy(nData.szTargetCharName, pPlayer->GetSaveData()->plrOrganizeName, sizeof(nData.szTargetCharName));
					}
				}

				if (pTarget)
					{
						if (pTarget->IsKindOf(CMapPlayer::CLASS_ID))
						{
							//nData.nTargetAccountID = ((CMapPlayer *)pTarget)->GetAccountID();
							nData.nTargetUID = pTarget->GetUID();
							//msg_strncpy(nData.szTargetAccount, pTarget->GetSaveData()->plrAccount, sizeof(nData.szTargetAccount));
							msg_strncpy(nData.szTargetCharName, pTarget->GetSaveData()->plrName, sizeof(nData.szTargetCharName));
							//msg_strncpy(nData.szTargetIP, napiServer_GetClientIP(((CMapPlayer *)pTarget)->GetClientID()), sizeof(nData.szIP));
						}
						else
						{
							//nData.nTargetAccountID = 0;
							nData.nTargetUID = pTarget->GetUID();
							//if (IsEnemyUID(nData.nTargetUID))
							//{
							//	strcpy(nData.szTargetAccount, "_Enemy_");
							//}
							//else
							//	strcpy(nData.szTargetAccount, "_NPC_");
							msg_strncpy(nData.szTargetCharName, pTarget->GetSaveData()->plrName, sizeof(nData.szTargetCharName));
							//strcpy(nData.szTargetIP, "0.0.0.0");
						}
					}

				SendData(m_hLogServer, 0, PROTO_LOG_INSERT_MSG_ITEM, (char *)&nData, sizeof(nData), 0);
//GetServer()->Log("Send Item log to log server(%d) ...", type);
			}
		}
	}
}

void CMapServer::SendLogMessage_ItemMerge(CMapPlayer * pPlayer, struct itemDATA_SAVE * pItem_disappear, struct itemDATA_SAVE * pItem_new)
{
#ifndef NO_USE_ITEM_UID2
	SendLogMessage_Item(pPlayer, LOGTYPE_ITEM_MERGE_DISAPPEAR, pPlayer, pItem_disappear, pItem_new);
#endif
}

void CMapServer::SendLogMessage_ItemSeparate(CMapPlayer * pPlayer, struct itemDATA_SAVE * pItem_old, struct itemDATA_SAVE * pItem_new)
{
#ifndef NO_USE_ITEM_UID2
	SendLogMessage_Item(pPlayer, LOGTYPE_ITEM_SEPARATE, pPlayer, pItem_old, pItem_new);
#endif
}

void CMapServer::SendLogMessage_Act(struct LOG_INSERT_MSG_ACT * pReq)
{
	if(IsConnectedToServer(m_hLogServer))
	{
		SendData(m_hLogServer, 0, PROTO_LOG_INSERT_MSG_ACT, (char *)pReq, sizeof(struct LOG_INSERT_MSG_ACT), 0);
	}
}
// ================================================================
// CallBack Function
// ================================================================
void CMapServer::CB_LoginAskMapServerID(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct LOGIN_ASK_MAPSERVER_ID * pData;
	struct LOGIN_ASK_MAPSERVER_ID_RESULT nData;

	GetServer()->m_nIsLoginAskID = 1;
	//
	pData = (struct LOGIN_ASK_MAPSERVER_ID *)pBuffer;
	nData.nCheckCode = pData->nCheckCode;
	nData.nMapServerID = GetServer()->m_nMapServerID;
	nData.nMapServerSubID = GetServer()->m_nMapServerSubID;
//	::msgSendData(nClientID, pComm->pcUID, PROTO_LOGIN_ASK_MAPSERVER_ID_RESULT, (char *)&nData, sizeof(nData), 0);
	GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_ASK_MAPSERVER_ID_RESULT, (char *)&nData, sizeof(nData), 0);
	GetServer()->Log("将标识信息发送到登录服务器(%d,%d).", nData.nMapServerID, nData.nMapServerSubID);
	//
	// 捌セ蝴臔诀
 #ifndef USE_PACKAGE_DATA
	GetServer()->Log("连接到登录服务器：发送地图空间状态 ...");
 #endif
	//
	// 穝ㄤMap Server捌セ瓜篈
	GetServer()->GetMapSpaceManager()->MapSpace_SendLoginReadyStatus();
	// 穝捌セ瓜篈
	GetServer()->GetMapSpaceManager()->MapSpace_AskLoginReadyStatus();

#if (defined(ItemMode))
	GetServer()->IMode_SendQuotaInfoToLogin();
#endif
}

// 祅 / 阁狝竟狝竟
void CMapServer::CB_MapLogoutToLogin(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
//#ifdef USE_RELOGIN
#if (defined(USE_RELOGIN) || defined(CROSS_SERVER_SYSTEM))
	CMapPlayer *	pPlayer;
 #ifdef USE_RELOGIN
	unsigned long uid;
 #endif

	// ??????w???s?b????b?R???{???????`?_?u
	pPlayer = GetServer()->FindPlayerByClientID(nClientID);
	if(pPlayer != NULL)
	{
		if(!GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
		{
			if (pPlayer->CheckPlayerNoSave())
				return;
			if (!pPlayer->IsReady())
				return;
			if (pPlayer->IsLoadingData())
				return;
			//
 #ifdef CROSS_SERVER_SYSTEM
			if (GetServer()->IsCrossServer(1))
			{
				//pPlayer->SendMsgToClientByID(5000000 | 0x80000000);		// ?i?J????A?????L?k???s?n?J!(???????????
				// ?^??n???A?t?~??e
				// !!!
				// ?B?z??^????A??
				struct LOGIN_NPCTALK_PACK_DATA_KS nTalkPackDataKS;

				memset(&nTalkPackDataKS, 0, sizeof(nTalkPackDataKS));
				nTalkPackDataKS.nType = NPCTALK_PACK_TYPE_KS_RETURN;
				nTalkPackDataKS.nData1 = 0;
				nTalkPackDataKS.nData2 = 0;
				nTalkPackDataKS.nAccountID = pPlayer->GetSaveData()->plrAccountID;
				nTalkPackDataKS.nData3 = 0;				// server id( login server ?? )
				nTalkPackDataKS.player_uid = pPlayer->GetUID();
				nTalkPackDataKS.nUserData3 = pPlayer->m_nPrivilege;		// privilege
				msg_strncpy(nTalkPackDataKS.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nTalkPackDataKS.szAccount));
				//
				CMapServer::GetServer()->SendData(GetServer()->GetLoginServer(), 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)&nTalkPackDataKS, nTalkPackDataKS.GetSmartSize(), 0);
				//
  #ifndef USE_PACKAGE_DATA
				GetServer()->Log("玩家返回原服务(%d,%s)", pPlayer->GetSaveData()->plrAccountID, pPlayer->GetSaveData()->plrAccount);
  #endif
				return;
			}
 #endif
			//
 #ifdef USE_RELOGIN
			uid = pPlayer->GetUID();
			GetServer()->SendData(GetServer()->m_hLoginServer, pComm->pcUID, PROTO_MAP_LOGOUT_2LOGIN_FROM_MAP, (char *)&uid, sizeof(uid), 0);
			//
			GetServer()->Log("玩家(%s,%d)注销并返回登录服务器.", pPlayer->GetName(), uid);
 #endif
		}
	}
#endif
}

// ?? login server ???
void CMapServer::CB_MapLogout2LoginFromMapResult(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
#ifdef USE_RELOGIN
	CMapPlayer *	pPlayer;
	struct MAP_LOGOUT_2LOGIN_FROM_MAP_RESULT * pReq;

	pReq = (MAP_LOGOUT_2LOGIN_FROM_MAP_RESULT *)pBuffer;
	pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(pReq->nUID);
	if(pPlayer != NULL)
	{
		if(!GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
		{
#ifndef USE_PACKAGE_DATA
			GetServer()->Log("获取重新定位结果(%s,%d).", pPlayer->GetName(), pReq->nUID);
#endif
			//
			if (pPlayer->CheckPlayerNoSave())
				return;
			if (!pPlayer->IsReady())
				return;
			//
			pPlayer->m_ReloginCommUID = pComm->pcUID;
			pPlayer->m_ReLoginData.nUID = pReq->nUID;
			pPlayer->m_ReLoginData.nAccountID = pReq->nAccountID;
			pPlayer->m_ReLoginData.nCheckCode = pReq->nCheckCode;
			pPlayer->m_ReLoginData.m_nPort = pReq->m_nPort;
			strcpy((char *)&pPlayer->m_ReLoginData.m_IP, (char *)&pReq->m_IP);
			// ??????s??
			GetServer()->			Disconnect_ClearAllRecord( pPlayer->GetUID(), 0 );
			MapServer_SendLogoutToLogin(GetServer(), pPlayer, 0);

			pPlayer->SaveAllData(0, 1);
			pPlayer->SetReady(false);
			pPlayer->SetExitCode(CMapPlayer::EXIT_TO_LOGIN_SERVER);
			GetServer()->DeleteObject(pPlayer->GetHandle());
		}
	}
#endif
}

// ?? Login Server ?q????e?? BOSS ???W
void CMapServer::CB_MapBossBroadcastAllMapMessage(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_BROADCAST_MESSAGE * pData;

	pData = (struct MAP_BROADCAST_MESSAGE *)pBuffer;
	if (pData && pData->nMessageSize > 0)
	{
		long map_id = 0, army_id = 0, role_code = 0, alive = 0;
		// Boss status relay message. Parse and broadcast as status notify (no chat spam).
		if (sscanf(pData->nMessage, "BOSSSTAT %ld %ld %ld %ld", &map_id, &army_id, &role_code, &alive) == 4)
		{
			CMapManager * pMgr = CMapServer::GetServer()->GetMapManager();
			if (pMgr && map_id > 0 && army_id > 0)
			{
				long nid = (army_id << 16) + (role_code & 0xffff);
				if (alive)
				{
					pMgr->m_nWorldArmyGenRecord[nid].ardMapCode = map_id;
					pMgr->m_nWorldArmyGenRecord[nid].ardArmyID = army_id;
				}
				else
				{
					pMgr->m_nWorldArmyGenRecord.erase(nid);
				}
				pMgr->BroadcastBossStatus(map_id, army_id, role_code, alive, 0);
				return;
			}
		}
	}
	CMapServer::GetServer()->GetMapManager()->BroadcastSPMessageToAllMap(pData);
}

void CMapServer::CB_MapBossBroadcastMapMessage(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_BROADCAST_MAP_MESSAGE * pData;

	pData = (struct MAP_BROADCAST_MAP_MESSAGE *)pBuffer;
	CMapServer::GetServer()->GetMapManager()->BroadcastSPMessageToMap(&pData->nMsg, pData->nMapCode);
}

void CMapServer::CB_MapBossBroadcastPlayerMessage(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_BROADCAST_PLAYER_MESSAGE * pData;

	pData = (struct MAP_BROADCAST_PLAYER_MESSAGE *)pBuffer;
	CMapServer::GetServer()->GetMapManager()->BroadcastSPMessageToPlayer(&pData->nMsg, pData->nPlayerUID);
}

void CMapServer::CB_MapUpdateForceOrganizeTotal(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_UPDATE_FORCE_ORGANIZE_TOTAL * pData;
	struct plrDATA_WORLD_FORCE * pForce;
	long country_id;
	long r = 0;

	pData = (struct MAP_UPDATE_FORCE_ORGANIZE_TOTAL *)pBuffer;
	country_id = pData->nCountryID;
	switch(country_id)
	{
	case ID_COUNTRY_WEI:
		r++;
		GetServer()->pwfOrgTotal_WEI = pData->nTotal;
		break;
	case ID_COUNTRY_SHU:
		r++;
		GetServer()->pwfOrgTotal_SHU = pData->nTotal;
		break;
	case ID_COUNTRY_WU:
		r++;
		GetServer()->pwfOrgTotal_WU = pData->nTotal;
		break;
	default:
		if (country_id >= ID_COUNTRY_FORCE01)
		{
			country_id -= ID_COUNTRY_FORCE01;
			if (country_id < gameMAX_PLAYER_COUNTRY_ID)
			{
				r++;
				pForce = GetServer()->GetForceDataArray();
				pForce[country_id].pwfOrgTotal = pData->nTotal;
			}
		}
		break;
	}
	// ???T?????? Client
	if (r)
		GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_FORCE_ORGANIZE_TOTAL_RESULT, pBuffer, nLen);
}

void CMapServer::CB_MapUpdateForceData(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_UPDATE_FORCE_DATA * pData;
	struct plrDATA_WORLD_FORCE * pForce;
	long country_id;

	pData = (struct MAP_UPDATE_FORCE_DATA *)pBuffer;
	country_id = pData->nCountryID;
	if (country_id >= ID_COUNTRY_FORCE01)
	{
		country_id -= ID_COUNTRY_FORCE01;
		if (country_id < gameMAX_PLAYER_COUNTRY_ID)
		{
			pForce = GetServer()->GetForceDataArray();
			memcpyT(&pForce[country_id], &pData->nSingleForce, sizeof(pData->nSingleForce));
			//
#if (!defined(USE_PACKAGE_DATA) || defined(CROSS_SERVER_SYSTEM_LOG))
			GetServer()->Log("更新地图势力资料(%s,%d,%d,%d)", pData->nSingleForce.pwfName, pData->nSingleForce.pwfCapital, pData->nSingleForce.pwfKingUID, pData->nSingleForce.pwfCreatorOrgUID);
#endif
			//
			GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_FORCE_DATA_RESULT, pBuffer, nLen);
		}
	}
}
// .......... NPC ?p?L .............
// NPC ?p?L
void CMapServer::CB_CNPCWalkTo(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	CVector2D					PathPoint[MAP_PLAYER_WALK_TO_MAX_POINTS];
	struct MAP_FIX_CHAR_POS		FixMsg;
	struct MAP_PLAYER_WALK_TO *	pWalkTo = (struct MAP_PLAYER_WALK_TO *)pBuffer;
	CMapPlayer *				pPlayer;
	long						dx, dy;
	long lx, ly;
//	int							cnt1, nx, ny;
	long is_deleted;
	CMapNPC * pNPC;
	CMapCtrl *					pMapCtrl;

	pPlayer = GetServer()->FindPlayerByClientID(nID, &is_deleted);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		if (!is_deleted)
		{
		//	GetServer()->KickClient(nID);
		//		GetServer()->Log("ERROR: CB_SendBroadcastMessage: 獶タ盽硈絬, 眏い耞硈絬, ClientID = %d", nID);
		}
		return;
	}

	// ??d npc_uid
//	struct plrDATA_NPC_SAVE * pNPCData = pPlayer->GetCarryNPCDataPtr(pWalkTo->m_nUID);
//	if (!pNPCData)
//	{
//		GetServer()->KickClient(nID);
//		GetServer()->Log("ERROR: CB_CNPCWalkTo: uid(%d)岿粇 眏い耞硈絬, ClientID = %d", pWalkTo->m_nUID, nID);
//		return;
//	}
	//
	pNPC = (CMapNPC *)GetServer()->FindAndCheckCharExistByUID(pWalkTo->m_nUID, CMapNPC::CLASS_ID);
	if (pNPC)
	{
		if (pNPC->GetPlayerUID() != pPlayer->GetUID())
		{
		//	GetServer()->KickClient(nID);
		//		GetServer()->Log("ERROR: CB_CNPCWalkTo: uid(%d)岿粇 眏い耞硈絬, ClientID = %d", pWalkTo->m_nUID, nID);
			return;
		}
		// ?p?L?? Client ????
		pNPC->m_nCNPC_State = 0;
		//
		if (pNPC->m_nMoveInputDelay)
			return;
		pNPC->m_nMoveInputDelay = PLAYER_NPC_MOVE_DELAY_TIME;
		//
		if(pNPC->IsOutMap())
		{
			// ?????b?a????L?k????
		GetServer()->Log("Walk: Point data error, PlayerUID = %d", pPlayer->GetUID());
			return;
		}
		if (pNPC->IsCasting())
			return;
		//
		if(pWalkTo->m_nPointCount < 2 || pWalkTo->m_nPointCount > MAP_PLAYER_WALK_TO_MAX_POINTS)
		{
			pNPC->m_nMoveInputDelay = PLAYER_NPC_MOVE_DELAY_TIME_LONG;
			// Point ??q???b?d??, ?L?k???????
		GetServer()->Log("Walk: Point data error, PlayerUID = %d", pPlayer->GetUID());
			return;
		}
		//
		if (pNPC->GetSaveData()->plrStatus2 & effFun2_NO_MOVE)
			return;

		lx = pNPC->GetPosX();
		ly = pNPC->GetPosY();
		dx = abs((long)(pWalkTo->m_Point[0].x - lx));
		dy = abs((long)(pWalkTo->m_Point[0].y - ly));
		if(dx > gameFIX_POS_DIST || dy > gameFIX_POS_DIST)
		{
			// ??m?~?t?L?j, ????????m
//			GetServer()->Log("竚粇畉筁, タà︹竚, Player NPC = %d", pNPC->GetUID());

			FixMsg.m_nUID	= pNPC->GetUID();
			FixMsg.m_Pos.x	= lx;
			FixMsg.m_Pos.y	= ly;
			::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_FIX_CHAR_POS, (char *)&FixMsg, sizeof(struct MAP_FIX_CHAR_POS), 0);
			//return;		// ?~??
		}

		// ???????, ??d?Z??(??d??????u???)
/*		for(cnt1 = 0; cnt1 < pWalkTo->m_nPointCount; cnt1++)
		{
			//PathPoint[cnt1].x = pWalkTo->m_Point[cnt1].x;
			//PathPoint[cnt1].y = pWalkTo->m_Point[cnt1].y;
			//
			nx = pWalkTo->m_Point[cnt1].x;
			ny = pWalkTo->m_Point[cnt1].y;
			dx = abs(nx - lx);
			dy = abs(ny - ly);
			if ( dx > gameERROR_POS_DIST || dy > gameERROR_POS_DIST)
			{
				pNPC->m_nMoveInputDelay = PLAYER_NPC_MOVE_DELAY_TIME_LONG;
				return;
			}
			PathPoint[cnt1].x = nx;
			PathPoint[cnt1].y = ny;
			lx = nx;
			ly = ny;
		}
		*/
		// Bug fix: ???? 2011/08/01
		pMapCtrl = pNPC->GetMapCtrl();
		if (!pMapCtrl)
			return;
		if (pMapCtrl->GetIconData(gameIconCalc_DIV(pNPC->GetPosX()), gameIconCalc_DIV(pNPC->GetPosY())) == MAPCODE_ID_WALL)
			return;
		//
		dx = sizeof(CVector2D) * pWalkTo->m_nPointCount;
		memcpy(PathPoint, pWalkTo->m_Point, dx);
		//
	//	// ???????
	//	for(cnt1 = 0; cnt1 < pWalkTo->m_nPointCount; cnt1++)
	//	{
	//		PathPoint[cnt1].x = pWalkTo->m_Point[cnt1].x;
	//		PathPoint[cnt1].y = pWalkTo->m_Point[cnt1].y;
	//	}

		//pNPC->SetHitObject(false);
		if (!pNPC->WalkTo(pWalkTo->m_nPointCount, PathPoint))
		{
			// ??e???????T??
			struct MAP_MOVE_CHAR_TO nRet;

			memset(&nRet, 0, sizeof(nRet));	// speed = 0 ??????~
			nRet.m_nUID	= pNPC->GetUID();
			::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_MOVE_CHAR_TO, (char *)&nRet, sizeof(nRet), 0);
//GetServer()->Log("WARNING: NPC(%d) move to pos error !", pNPC->GetUID());
		}
	}
}

void CMapServer::CB_CNPCAttack(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_PLAYER_ATTACK *	pAttack = (struct MAP_PLAYER_ATTACK *)pBuffer;
	CMapPlayer *	pPlayer;
	CMapChar *		pTarget;
	long is_deleted;
	CMapNPC * pNPC;
	struct plrDATA * plrData;
#ifdef USE_ADDATTACKSPEED_TO_CNPC
	unsigned long attack_delay;
#endif

	pPlayer = GetServer()->FindPlayerByClientID(nID, &is_deleted);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		if (!is_deleted)
		{
		//	GetServer()->KickClient(nID);
		//		GetServer()->Log("ERROR: CB_SendBroadcastMessage: 獶タ盽硈絬, 眏い耞硈絬, ClientID = %d", nID);
		}
		return;
	}

	if(nLen != sizeof(struct MAP_PLAYER_ATTACK))
	{
		//GetServer()->Log("ERROR: CB_CNPCAttack: Wrong buffer length!");
		pPlayer->SetBotRecord(BOT_REASON_INVALID_COMMAND2);
		pPlayer->PlayerAttackErrorCount(100);
		return;
	}

	//pPlayer->CheckLastActionTick(pAttack->m_nClientTick, 0);

	pNPC = (CMapNPC *)GetServer()->FindAndCheckCharExistByUID(pAttack->m_nUID, CMapNPC::CLASS_ID);
	if (pNPC)
	{
		// ?O??v???p?L
		if (pNPC->GetPlayerUID() != pPlayer->GetUID())
		{
		//	GetServer()->KickClient(nID);
		//	GetServer()->Log("ERROR: CB_CNPCAttack: uid(%d)岿粇 眏い耞硈絬, ClientID = %d", pAttack->m_nUID, nID);
			pPlayer->PlayerAttackErrorCount(BOT_REASON_GM_NOTICE_BOT1);
			return;
		}
		// ?p?L?? Client ????
		pNPC->m_nCNPC_State = 0;
		if (pNPC->m_nAttackInputDelay)
		{
			//pPlayer->PlayerAttackErrorCount(BOT_REASON_GM_NOTICE_ATTACKSPD);
			return;
		}
		plrData = pNPC->GetCharData();
#ifdef USE_ADDATTACKSPEED_TO_CNPC
		attack_delay = plrData->plrAttackDelay;
		if (attack_delay < PLAYER_NPC_ATTACK_DELAY_TIME)
			attack_delay = PLAYER_NPC_ATTACK_DELAY_TIME;
		if (pNPC->GetSaveData()->plrStatus & effFun_ADDATTACKSPEED)
			attack_delay = attack_delay * gameADD_ATTACK_SPEED_TO_CNPC / 100;
		pNPC->m_nAttackInputDelay = (unsigned short)attack_delay;
#else
		if (plrData->plrAttackDelay > PLAYER_NPC_ATTACK_DELAY_TIME)
		{
			pNPC->m_nAttackInputDelay = plrData->plrAttackDelay;
		}
		else
			pNPC->m_nAttackInputDelay = PLAYER_NPC_ATTACK_DELAY_TIME;
#endif
		//
		if (pAttack->m_nUID > gameMAX_ENEMY_UID)
		{
			pPlayer->SetBotRecord(BOT_REASON_ATTACK_ERR);
			pPlayer->PlayerAttackErrorCount(BOT_REASON_GM_NOTICE_UNKNOWN_PROTOCO);
			return;
		}
		//
		//pTarget = (CMapChar *)GetServer()->FindObjectByUID(pAttack->m_nTargetUID, CMapChar::CLASS_ID);
		pTarget = (CMapChar *)GetServer()->FindAndCheckCharExistByUID(pAttack->m_nTargetUID, CMapChar::CLASS_ID);
		if(pTarget == NULL)
		{
		//	GetServer()->Log("ERROR: CB_CNPCAttack: тぃю阑ヘ夹, PlayerUID = %d, TargetUID = %d", pNPC->GetUID(), pAttack->m_nTargetUID);
			pPlayer->PlayerAttackErrorCount(BOT_REASON_GM_NOTICE_NO_TARGET);
			return;
		}

		if (pTarget->m_nCharFlags & CHAR_TRAP)
			return;

		if(pNPC->IsAttackTarget(pTarget->GetHandle()))
		{
	//		GetServer()->Log("Player NPC(%d) attack char(%d)", pNPC->GetUID(), pTarget->GetUID());
			//
			if (!pTarget->IsSuperMode())
			{
				pNPC->SetAttackTarget(pTarget->GetHandle());
				pNPC->Attack((CMapPlayer *)-1, pPlayer);
				//pPlayer->ClearNoAttackMode();		// ?p?L??????????
			}
//			else
//				GetServer()->Log("Player NPC(%d) can not attack, char(%d) is super mode", pNPC->GetUID(), pTarget->GetUID());
		}
		else
		{
//			GetServer()->Log("Player NPC(%d) can not attack, char(%d) is not an enemy", pNPC->GetUID(), pTarget->GetUID());
			pPlayer->PlayerAttackErrorCount(BOT_REASON_GM_NOTICE_TARGET_ERR);
		}
	}
	else
		pPlayer->PlayerAttackErrorCount(BOT_REASON_GM_NOTICE_CNPC_SKL);
}

void CMapServer::CB_CNPCCastMagic(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_PLAYER_CAST_MAGIC *	pCast = (struct MAP_PLAYER_CAST_MAGIC *)pBuffer;
	struct magicDATA *				pMagic;
	CMapPlayer *	pPlayer;
//	CMapObject *	pObject;
	CMapChar *		pTarget;
	long is_deleted;
	CMapNPC * pNPC;
	long i, magic_id;
	struct plrDATA * plrData;
#ifdef NPC_AI_ATTACK_RANGE
	long dx, dy;
#endif

	pPlayer = GetServer()->FindPlayerByClientID(nID, &is_deleted);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		if (!is_deleted)
		{
		//	GetServer()->KickClient(nID);
		//		GetServer()->Log("ERROR: CB_SendBroadcastMessage: 獶タ盽硈絬, 眏い耞硈絬, ClientID = %d", nID);
		}
		return;
	}

	if(nLen != sizeof(struct MAP_PLAYER_CAST_MAGIC))
	{
		//GetServer()->Log("ERROR: CB_CNPCCastMagic: Wrong buffer length!");
		pPlayer->SetBotRecord(BOT_REASON_INVALID_COMMAND2);
		pPlayer->PlayerAttackErrorCount(120);
		return;
	}

	//pPlayer->CheckLastActionTick(pCast->m_nClientTick, 0);

	pNPC = (CMapNPC *)GetServer()->FindAndCheckCharExistByUID(pCast->m_nPlayerUID, CMapNPC::CLASS_ID);
	if (pNPC)
	{
		// ?O??v???p?L
		if (pNPC->GetPlayerUID() != pPlayer->GetUID())
		{
		//	GetServer()->KickClient(nID);
//GetServer()->Log("ERROR: CB_CNPCCastMagic: uid(%d)岿粇 眏い耞硈絬, ClientID = %d", pCast->m_nPlayerUID, nID);
			pPlayer->PlayerAttackErrorCount(122);
			return;
		}
		// ?p?L?? Client ????
		pNPC->m_nCNPC_State = 0;
		//
		if (pNPC->m_nCastInputDelay)
		{
			//pPlayer->PlayerActErrorLogCount(BOT_REASON_GM_NOTICE_CNPC_SKL);
			// ???s?p????v
r_abort:	pNPC->m_nMercenaryCastSelf = 0;
			pNPC->m_nMercenaryCast = 0;
			return;
		}
		//
		plrData = pNPC->GetCharData();
		if (plrData->plrCastAttackDelay > PLAYER_NPC_CAST_DELAY_TIME)
		{
			pNPC->m_nCastInputDelay = plrData->plrCastAttackDelay;
		}
		else
			pNPC->m_nCastInputDelay = PLAYER_NPC_CAST_DELAY_TIME;
		//
		if (pCast->m_nPlayerUID > gameMAX_ENEMY_UID)
		{
			pPlayer->SetBotRecord(BOT_REASON_ATTACK_ERR);
			pPlayer->PlayerAttackErrorCount(123);
			return;
		}
		//
		if (pNPC->IsCasting())
			goto r_abort;
		// ??d?p?L???? magic id, level ?O?_?X?k
		magic_id = pCast->m_nMagicID;
#ifdef DEBUG_SMART_PLR_DATA
		if (!plrData->plrAIPtr || !plrData->plrOrg)
			goto r_abort;
		if (magic_id == plrData->plrOrg->plrSetData.plrAIHelpSelfMagicID)
#else
		if (magic_id == pNPC->GetCharData()->plrAIHelpSelfMagicID)
#endif
		{
			if (!pNPC->m_nMercenaryCastSelf)
			{
				pPlayer->PlayerAttackErrorCount(124);
				return;
			}
			pNPC->m_nMercenaryCastSelf = 0;
			//
#ifdef DEBUG_SMART_PLR_DATA
			pCast->m_nMagicLevel = plrData->plrOrg->plrSetData.plrAIHelpSelfLevel;
#else
			pCast->m_nMagicLevel = pNPC->GetCharData()->plrAIHelpSelfLevel;
#endif
			goto do_ok;
		}
#ifdef DEBUG_SMART_PLR_DATA
		else if (magic_id == plrData->plrAIPtr->aiMagicID_Status)
#else
		else if (magic_id == pNPC->GetCharData()->plrAI_MagicID_Status)
#endif
		{
			if (!pNPC->m_nMercenaryCast)
			{
				pPlayer->PlayerAttackErrorCount(125);
				return;
			}
			pNPC->m_nMercenaryCast = 0;
			//
			if (pCast->m_nTargetUID != pNPC->GetUID())
				return;
			goto do_ok;
		}
#ifdef DEBUG_SMART_PLR_DATA
		else if (magic_id == plrData->plrAIPtr->aiMagicID_HP)
#else
		else if (magic_id == pNPC->GetCharData()->plrAI_MagicID_HP)
#endif
		{
			if (!pNPC->m_nMercenaryCast)
			{
				pPlayer->PlayerAttackErrorCount(126);
				return;
			}
			pNPC->m_nMercenaryCast = 0;
			//
			if (pCast->m_nTargetUID != pNPC->GetUID())
				return;
			goto do_ok;
		}
		else
		{
			struct plrSPC_ATTACK * pSpec = pNPC->GetCharData()->plrSpecialAttack;

			if (!pNPC->m_nMercenaryCast)
			{
				pPlayer->PlayerAttackErrorCount(127);
				return;
			}
			pNPC->m_nMercenaryCast = 0;
			//
			for (i=0; i<gameMAX_SPECIAL_ATTACK; i++)
			{
				if (pSpec[i].spCode == magic_id)
				{
				//	if (pSpec[i].spLevel == pCast->m_nMagicLevel)
				pCast->m_nMagicLevel = pSpec[i].spLevel;
					{
						goto do_ok;
					}
				}
			}
			{
				struct CNPC_ITEM_SKILL_SAVE tmpSkills[gameMAX_SPECIAL_ATTACK];
				struct itemUIDDATA itemUID;

				memset(&itemUID, 0, sizeof(itemUID));
#ifdef SMART_PLR_DATA2
				if (plrData->plrNPC_Flags & 1)
					itemUID = plrData->plrNPC_ItemUID;
#else
				if (plrData->plrNPC.plrNPCData[0].plrNPC_Flags & 1)
					itemUID = plrData->plrNPC.plrNPCData[0].plrNPC_ItemUID;
#endif
				gameCNPC_SessionCopyTo(&itemUID, tmpSkills);
				for (i = 0; i < gameMAX_SPECIAL_ATTACK; i++)
				{
					if (tmpSkills[i].skillID == (unsigned short)magic_id)
					{
						pCast->m_nMagicLevel = tmpSkills[i].skillLv ? tmpSkills[i].skillLv : 1;
						gameCNPC_ApplyItemSkillsToBattle(plrData, tmpSkills);
						goto do_ok;
					}
				}
			}
			pPlayer->PlayerAttackErrorCount(128);
		}
		GetServer()->Log("ERROR: CB_CNPCCastMagic: uid(%d) use skill error(id=%d, level=%d), ClientID = %d", pCast->m_nPlayerUID, pCast->m_nMagicID, pCast->m_nMagicLevel, nID);
		return;
		//
do_ok:
#ifdef NPC_AI_ATTACK_RANGE
		dx = abs(pNPC->GetPosX() - pPlayer->GetPosX());
		dy = abs(pNPC->GetPosY() - pPlayer->GetPosY());
		if ((dx >= NPC_AI_MAX_ATTACK_RANGE) || (dy >= NPC_AI_MAX_ATTACK_RANGE))
		{
			return;
		}
#endif
		//
		pMagic = gameMagic_GetPointer(pCast->m_nMagicID, NULL);
		if(pMagic == NULL)
		{
			GetServer()->Log("ERROR: CB_CNPCCastMagic: No this magic(uid=%d, MagicID=%d)", pPlayer->GetUID(), pCast->m_nMagicID);
			return;
		}

		//if (pCast->m_nMagicID >= magic_RUN)		// ?Q???
		if (Inner_Is_Passive_Skill(pCast->m_nMagicID))
		{
			return;
		}

		switch(pMagic->magicTargetSelectType)
		{
		case magicSelectType_NONE:
			// ?H??v??????
	//		GetServer()->Log("Player NPC(%d) cast magic(%d)", pNPC->GetUID(), pCast->m_nMagicID);
			break;

		case magicSelectType_OBJECT:
			// ???@???
			if (pMagic->magicEffect.effAntiStatus & effFun_RESURRECT)
			{
				return;		// ?p?L?????_??
			//	pObject = GetServer()->FindObjectByUID(pCast->m_nTargetUID);
			//	if(pObject == NULL)
			//		return;
			//	if(!pObject->IsKindOf(CMapChar::CLASS_ID))
			//		return;
			//	pTarget = (CMapChar *)pObject;
			}
			else
				pTarget = GetServer()->FindAndCheckCharExistByUID(pCast->m_nTargetUID, CMapChar::CLASS_ID);
			if(pTarget == NULL)
			{
//				GetServer()->Log("ERROR: CB_CNPCCastMagic: тぃ琁猭ヘ夹, PlayerUID = %d, TargetUID = %d", pNPC->GetUID(), pCast->m_nTargetUID);
				pPlayer->PlayerAttackErrorCount(129);
				return;
			}
			//
			if (pTarget->m_nCharFlags & CHAR_TRAP)
				return;
			//
			if(!pNPC->IsMagicTarget(pCast->m_nMagicID, pTarget->GetHandle()))
			{
				GetServer()->Log("ERROR: Player NPC(%d) can't cast magic(%d) to char(%d)", pNPC->GetUID(), pCast->m_nMagicID, pTarget->GetUID());
				pPlayer->PlayerAttackErrorCount(130);
				return;
			}

	//		GetServer()->Log("Player NPC(%d) cast magic(%d) to char(%d)", pNPC->GetUID(), pCast->m_nMagicID, pTarget->GetUID());
			break;

		case magicSelectType_POS:
			// ??d??
	//		GetServer()->Log("Player NPC(%d) cast magic(%d) to [%d,%d]", pNPC->GetUID(), pCast->m_nMagicID, pCast->m_TargetPos.x, pCast->m_TargetPos.y);
			break;
		}

//GetServer()->Log("NPC(%d)cast magic %d", pNPC->GetUID(), pCast->m_nMagicID);
		if(pNPC->CastMagic(pCast))
		{
		//	pNPC->m_nCastInputDelay = PLAYER_NPC_CAST_DELAY_TIME;
		//	if (pMagic->magicEffect.effFunction & effFun_ATTACK)	// ?p?L??????????
		//		pPlayer->ClearNoAttackMode();						// ?p?L??????????
		}
//		else
//			GetServer()->Log("Player NPC(%d) fail cast magic(%d)", pNPC->GetUID(), pCast->m_nMagicID);

		return;
	}
	else
		pPlayer->PlayerAttackErrorCount(121);
}

void CMapServer::CB_CNPCInOut(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_CNPC_IN_OUT * pData = (struct MAP_CNPC_IN_OUT *)pBuffer;
	CMapPlayer *	pPlayer;
	long is_deleted;
	CMapNPC * pNPC;

	pPlayer = GetServer()->FindPlayerByClientID(nID, &is_deleted);
	if(pPlayer == NULL)
	{
		// ?D???`?s?u, ?j?????_?s?u
		if (!is_deleted)
		{
		//	GetServer()->KickClient(nID);
		//		GetServer()->Log("ERROR: CB_SendBroadcastMessage: 獶タ盽硈絬, 眏い耞硈絬, ClientID = %d", nID);
		}
		return;
	}

	pNPC = (CMapNPC *)GetServer()->FindAndCheckCharExistByUID(pData->nUID, CMapNPC::CLASS_ID);
	if (pNPC)
	{
		if (pNPC->GetPlayerUID() != pPlayer->GetUID())
		{
		//	GetServer()->KickClient(nID);
		//	GetServer()->Log("ERROR: CB_CNPCInOut: uid(%d)岿粇 眏い耞硈絬, ClientID = %d", pData->nUID, nID);
			return;
		}
		pNPC->m_nCNPC_State = (char)pData->nMode;		// ??e Client ?u??? Out ???A
//GetServer()->Log("Set NPC move state: uid(%d), mode(%d)", pData->nUID, pData->nMode);
	}
}
// .......... NPC ????..........
// Client ?}?l NPC ????
void CMapServer::CB_TalkToNPC(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_TALK_TO_NPC * pData;
	struct MAP_TALK_TO_NPC_RESULT nRet;
	CMapPlayer * pPlayer;
	CMapNPC * pChar;
	long id, org_id;
	unsigned long cnt;
	struct talkNPCMESSAGE * pNPCMsg;

//CMapServer::GetServer()->Log("Talk to NPC ...");
	pData = (struct MAP_TALK_TO_NPC *)pBuffer;
	pPlayer = GetServer()->FindPlayerByClientID(nID);
	if(pPlayer != NULL)
	{
		if (pPlayer->IsMoving())
			pPlayer->StopMoving();
		//
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)
			return;
		if(!GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
		{
			if (!pPlayer->IsReady())
				return;
			if (!pPlayer->IsAvailable())
				return;
			if (pPlayer->m_nNPCTalkMsgID)	// 2006/06/05 ???J
				return;
			if (CMapServer::GetServer()->IsMapWar(pPlayer->GetMapCode()))
			{
				// ??d???\???????
				pChar = (CMapNPC *)GetServer()->FindAndCheckCharExistByUID(pData->npcUID, CMapNPC::CLASS_ID);
				if (pChar)
				{
					if (pChar->GetShowData()->plrSetColor == pPlayer->GetShowData()->plrSetColor)	// ?P??O
					{
						if (pChar->IsKindOf(CMapNPC::CLASS_ID))
						{
							if (pNPCMsg = gameGetNPCTalkPtr(pChar->m_nTalkID, 0))
							{
								if (pNPCMsg->npcmWarTalk)
									goto do_talk;
							}
						}
					}
				}
				return;
			}
			//
			pChar = (CMapNPC *)GetServer()->FindAndCheckCharExistByUID(pData->npcUID, CMapNPC::CLASS_ID);
			if (pChar)
			{
				if (pChar->IsKindOf(CMapNPC::CLASS_ID))
				{
do_talk:			// ???????
					if (pPlayer->IsMoving())
						pPlayer->StopMoving();
					// ??d?Z??
					if (pPlayer->CheckInDistance(pChar, gameIconSize * 10))
					{
						cnt = 0;		// DEBUG ??
						org_id = pChar->m_nTalkID;
						//
#ifdef USE_SPCMODE_CN_ADULT_LIMIT		// 2008/04/15
						if (pPlayer->m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
						{
								char tmpstr[1024];

						//	switch (pChar->GetSaveData()->plrCode)
						//	{
						//	case 2420:		//	?j?E?
						//	case 2433:		//	?j?q?A
						//	case 2445:		//	?j?q?A(?K?O)
								//pPlayer->SendMsgToClientByID(24499);
								strcpy(tmpstr, gameGetResourceName(24499));
								pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
								pPlayer->ExitNPCTalk();
								pPlayer->SendNPCTalkResult(pComm, 0, 0, 0);
								return;
						//		break;
						//	}
						}
#endif
						//
						nRet.npcUID = pData->npcUID;
						nRet.npcTalkID = pChar->m_nTalkID;
						if (nRet.npcTalkID)
						{
							// ?S?? ID?B?z(?G?i??)
							switch(nRet.npcTalkID)
							{
							case NPCTALK_SYSID_BILLBOARD:
							case NPCTALK_SYSID_CITYFORCE:
							case NPCTALK_SYSID_STATUE:
							case NPCTALK_SYSID_STATUE2:
							case NPCTALK_SYSID_STATUE3:
							case NPCTALK_SYSID_STATUE4:
							case NPCTALK_SYSID_STATUE5:
							case NPCTALK_SYSID_STATUE6:
								::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_TALK_TO_NPC_RESULT, (char *)&nRet, sizeof(nRet), 0);
								return;
								break;
							}
							// ????e Script
							pPlayer->ExitNPCTalk();
							//
	retry:					//memset(pPlayer->m_nNPCSelectID, 0, sizeof(pPlayer->m_nNPCSelectID));
							// ...............................
							if (cnt >= 90)
							{
								if (cnt < 100)
								{	// map, current id, talk id
									CMapServer::GetServer()->Log("SERIOUS: NPC talk to (%d,%d,%d)", pPlayer->GetMapCode(), nRet.npcTalkID, org_id);
									cnt++;
									//
									if (cnt >= 100)
										dbg_FlushOutputFile();
								}
							}
							else
								cnt++;
							// ...............................
							id = pPlayer->gameServer_NPCTalkProcess1(nRet.npcTalkID);
//CMapServer::GetServer()->Log("NPC talk process 1(%d, %d)", nRet.npcTalkID, id);
							if (id)
							{
								if (id == -1)		// Wait state
								{
									pPlayer->m_Wait_PCOMM_UID = pComm->pcUID;
									pPlayer->SetNPCTalk(pData->npcUID, pChar, nRet.npcTalkID+1);
									// ??e?^??
									nRet.npcTalkID = 0;	// ヘ玡礚戈
									::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_TALK_TO_NPC_RESULT, (char *)&nRet, sizeof(nRet), 0);
									return;
								}
								else if (id == -2)	// Abort
								{
									pPlayer->ExitNPCTalk();
									pPlayer->SendNPCTalkResult(pComm, 0, 0, 0);
									return;
								}
								//
								if (id != nRet.npcTalkID)
								{
									nRet.npcTalkID = id;
									goto retry;
								}
								else	// ?p?G??e?L?T????L???h?~??U?@??
								{
									nRet.npcTalkID = id;
									if (!pPlayer->m_nNPCSelectID[0])
									{
										pNPCMsg = gameGetNPCTalkPtr(id, 0);
										if (pNPCMsg)
										{
											if (!pNPCMsg->npcmMsgID)
											{
												if (LOWORDPTR(pNPCMsg->npcmClose) != 0xffff)
												{
													if (pNPCMsg->npcmNextID && (pNPCMsg->npcmNextID != id))
													{
														id = pNPCMsg->npcmNextID;
													}
													else
														id = id + 1;
													nRet.npcTalkID = id;
//CMapServer::GetServer()->Log("Notice:?~??(%d)", id);
													goto retry;
												}
												else
													id = 0;		// ˙挡
											}
										}
									}
								}
								pPlayer->m_nNPCTalkSerial = 0;
								//
								//nRet.npcTalkID = id;
								::msgSendData(pPlayer->GetClientID(), pComm->pcUID, PROTO_MAP_TALK_TO_NPC_RESULT, (char *)&nRet, sizeof(nRet), 0);
								//
								pPlayer->SetNPCTalk(pData->npcUID, pChar, id);
							}
							else
								CMapServer::GetServer()->Log("WARNING: NPC talk process 1 return 0(code = %d)", nRet.npcTalkID);
						}
					//	else
					//		CMapServer::GetServer()->Log("WARNING: Talk to NPC but no talk id(%08x, code = %d)", pData->npcUID, pChar->GetSaveData()->plrCode);
					}
					else
						CMapServer::GetServer()->Log("ERROR: Talk to NPC but out of range(%08x), Acc=%s, Name=%s", pData->npcUID, pPlayer->GetSaveData()->plrAccount, pPlayer->GetSaveData()->plrName);
				}
				else
					CMapServer::GetServer()->Log("ERROR: Talk to NPC but not a NPC object(%08x)", pData->npcUID);
			}
			else
				CMapServer::GetServer()->Log("ERROR: Talk to NPC but not found(%08x)", pData->npcUID);
		}
	}
}

// Client ???? NPC ???AServer ???^??
void CMapServer::CB_TalkToNPCEnd(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_TALK_TO_NPC_END * pData;
	CMapPlayer * pPlayer;

	pData = (struct MAP_TALK_TO_NPC_END *)pBuffer;
	pPlayer = GetServer()->FindPlayerByClientID(nID);
	if(pPlayer != NULL)
	{
		if(!GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
		{
			pPlayer->ExitNPCTalk();
		}
	}
}

// ?x?s??m(?????)
void CMapServer::CB_NPCSavePos(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
/*	struct MAP_NPC_SAVE_POS * pData;
	CMapPlayer * pPlayer;

	pData = (struct MAP_NPC_SAVE_POS *)pBuffer;
	pPlayer = GetServer()->FindPlayerByClientID(nID);
	if(pPlayer != NULL)
	{
		if(!GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
		{
			if (!pPlayer->IsNPCTalk())
				return;
			pPlayer->GetSaveData()->plrSavePosX = pPlayer->GetPosX();
			pPlayer->GetSaveData()->plrSavePosY = pPlayer->GetPosY();
			pPlayer->GetSaveData()->plrSaveMapCode = pPlayer->GetMapID();
			//pPlayer->SaveAllData();
			pPlayer->SaveCharData(0);
			// ?@???^????G
			pPlayer->SendNPCTalkResult(pComm, 1, 0, 0);
		}
	}
	*/
}

// ?U?@?B
void CMapServer::CB_NPCNextStep(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_TALK_TO_NPC_NEXT_STEP * pData;
	CMapPlayer * pPlayer;
	long i, id, next_id;
	struct talkNPCMESSAGE * pNPCMsg;
	//
	long org_id;
	unsigned long cnt;
	//
	CMapNPC * pChar;

	pData = (struct MAP_TALK_TO_NPC_NEXT_STEP *)pBuffer;
	pPlayer = GetServer()->FindPlayerByClientID(nID);
	//
	if(pPlayer != NULL)
	{
		{
			int btnIndex = -1;
			int i2;
			for (i2 = 0; i2 < 4; i2++)
			{
				if ((long)pData->nSelectID == (long)pPlayer->m_nNPCSelectID[i2])
				{
					btnIndex = i2 + 1; // 1-based
					break;
				}
			}
			pPlayer->m_nNPCTalkLastBtnIndex = btnIndex > 0 ? btnIndex : 0;
		}
		if (pPlayer->IsMoving())
			pPlayer->StopMoving();
		if ((unsigned short)pData->nSerial != pPlayer->m_nNPCTalkSerial)
			return;
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)
			return;
		if (!pPlayer->m_nNPCTalkMsgID)
		{
			pPlayer->SendNPCTalkResult(pComm, 0, 0, 0);
			return;
		}
		if (!pPlayer->IsReady())
			return;
		if (!pPlayer->IsAvailable())
			goto err_talk;
		//
#ifdef USE_SPCMODE_CN_ADULT_LIMIT		// 2008/04/15
		if (pPlayer->m_nWebIP_Flags & (LOGIN_CHAR_FLAGS_SPCMODE_CN_HF | LOGIN_CHAR_FLAGS_SPCMODE_CN_NO))
		{
			char tmpstr[1024];
		//	pChar = (CMapNPC *)GetServer()->FindAndCheckCharExistByUID(pPlayer->m_nTalkNPC_UID, CMapNPC::CLASS_ID);
		//	if (pChar)
		//	{
		//		if (pChar->IsKindOf(CMapNPC::CLASS_ID))
		//		{
		//			switch (pChar->GetSaveData()->plrCode)
		//			{
		//			case 2420:		//	?j?E?
		//			case 2433:		//	?j?q?A
		//			case 2445:		//	?j?q?A(?K?O)
						//pPlayer->SendMsgToClientByID(24499);
						strcpy(tmpstr, gameGetResourceName(24499));
						pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
						pPlayer->ExitNPCTalk();
						pPlayer->SendNPCTalkResult(pComm, 0, 0, 0);
						return;
		//				break;
		//			}
		//		}
		//	}
		}
#endif
		//
		pPlayer->m_nNPCTalkSerial = apiGetTickCounter() & 0xffff;
//GetServer()->Log("Player(%d) talk to npc next step, msg id(%d) ", pPlayer->GetUID(), pData->nSelectID);
		if(!GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
		{
			if (CMapServer::GetServer()->IsMapWar(pPlayer->GetMapCode()))
			{
				// ??d???\???????
				pChar = (CMapNPC *)GetServer()->FindAndCheckCharExistByUID(pPlayer->m_nTalkNPC_UID, CMapNPC::CLASS_ID);
				if (pChar)
				{
					if (pChar->GetShowData()->plrSetColor == pPlayer->GetShowData()->plrSetColor)	// ?P??O
					{
						if (pNPCMsg = gameGetNPCTalkPtr(pChar->m_nTalkID, 0))
						{
							if (pNPCMsg->npcmWarTalk)
								goto do_talk;
						}
					}
				}
				goto err_talk;
			}
do_talk:
			//
		//	if (!pPlayer->IsNPCTalk())
		//		return;
			//
			cnt = 0;		// DEBUG ??
			org_id = pPlayer->m_nNPCTalkMsgID;
			// ?o???? ID
			next_id = 0;
			switch(pData->nSelectID)
			{
			case -1:			// ????
				pPlayer->ExitNPCTalk();
		//		pPlayer->SendNPCTalkResult(pComm, 0, 0, 0);
				return;
		//next_id = pPlayer->m_nNPCTalkMsgID;
				break;
			case -2:			// ?U?@?B
				next_id = pPlayer->m_nNPCTalkMsgID;
				if (pPlayer->m_nNPCSelectID[0])
				{				// ?p?G?n??????A???? ?U?@?B ????????D
					CMapServer::GetServer()->Log("WARNING: NPC talk process 2 select id should not be Next-Step(-2) (code = %d)", next_id);
						goto err_talk;
				}
				break;
			default:			// ??(?s?? talk id)
				// ??d??ID?O?_?X?k
				next_id = -1;
				for (i=0; i<talkNPCOPTION_MAX; i++)
				{
					id = pPlayer->m_nNPCSelectID[i];
					if (!id)
						break;
					if (id == pData->nSelectID)
					{
						next_id = pPlayer->m_nNPCTalkMsgID;
						// ==============================================
						// ?o????????????????? Script, ?q?`???|?]?w
						// ==============================================
						goto new_id;
						break;
					}
				}
				//
				if (next_id == -1)
				{
					CMapServer::GetServer()->Log("ERROR: Player(%d, %s) talk to NPC select id error(%d, tbl id = %d, %d, %d, %d)", pPlayer->GetUID(), pPlayer->GetSaveData()->plrName, pData->nSelectID, pPlayer->m_nNPCSelectID[0], pPlayer->m_nNPCSelectID[1], pPlayer->m_nNPCSelectID[2], pPlayer->m_nNPCSelectID[3]);
					//
					pPlayer->SaveAllData();
					pPlayer->SetReady(false);
					pPlayer->SetExitCode(CMapPlayer::EXIT_LOGOUT);
					GetServer()->DeleteObject(pPlayer->GetHandle());
				//	GetServer()->KickClient(pPlayer->GetClientID());	// 2004/10/26 ?H?K???n?X
					return;
				}
				break;
			}
		//	memset(pPlayer->m_nNPCSelectID, 0, sizeof(pPlayer->m_nNPCSelectID));
			//
			if (next_id)
			{
				// ??????Script
				id = pPlayer->gameServer_NPCTalkProcess2(next_id, pData->nSelectID);
//CMapServer::GetServer()->Log("NPC talk process 2(%d, %d)", next_id, id);
				if (id)
				{
					if (id == next_id)
					{
						CMapServer::GetServer()->Log("WARNING: NPC talk process 2 return same next id(%d)", id);
						goto err_talk;
					}
					if (id < 0)
					{
						CMapServer::GetServer()->Log("WARNING: NPC talk process 2 return not support id(%d)", id);
						goto err_talk;
					}
					// ????e Script
	new_id:			
					// ...............................
					if (cnt >= 90)
					{
						if (cnt < 100)
						{	// map, current id, talk id
							CMapServer::GetServer()->Log("SERIOUS: NPC talk next (%d,%d,%d)", pPlayer->GetMapCode(), id, org_id);
							cnt++;
							//
							if (cnt >= 100)
								dbg_FlushOutputFile();
						}
					}
					else
						cnt++;
					// ...............................
					next_id = id;
					//
		//	memset(pPlayer->m_nNPCSelectID, 0, sizeof(pPlayer->m_nNPCSelectID));
					//
					id = pPlayer->gameServer_NPCTalkProcess1(next_id);
//CMapServer::GetServer()->Log("NPC talk process 1(%d, %d)", next_id, id);
					if (id)
					{
						if (id == -1)	// Wait state
						{
							pPlayer->m_Wait_PCOMM_UID = pComm->pcUID;
							pPlayer->SetNPCTalk(0, NULL, next_id+1);
							return;
						}
						else if (id == -2)	// Abort
						{
							goto err_talk;
						}
						//
						if (id != next_id)
						{
							goto new_id;
						}
						else	// ?p?G??e?L?T????L???h?~??U?@??
						{
							if (!pPlayer->m_nNPCSelectID[0])
							{
								pNPCMsg = gameGetNPCTalkPtr(id, 0);
								if (pNPCMsg)
								{
									if (!pNPCMsg->npcmMsgID)
									{
										if (LOWORDPTR(pNPCMsg->npcmClose) != 0xffff)
										{
											if (pNPCMsg->npcmNextID && (pNPCMsg->npcmNextID != id))
											{
												id = pNPCMsg->npcmNextID;
											}
											else
												id = id + 1;
//CMapServer::GetServer()->Log("Notice:?~??(%d)", id);
											goto new_id;
										}
										else
											next_id = 0;	// ?U?@?B????
									}
								}
							}
						}
						pPlayer->SendNPCTalkResult(pComm, 1, id, pPlayer->m_nNPCTalkSerial);
						//
						pPlayer->SetNPCTalk(0, NULL, next_id);
					}
					else
						CMapServer::GetServer()->Log("WARNING: NPC talk process 1(next step) return 0(code = %d)", next_id);
				}
				else
				{
					if ((pData->nSelectID != -1) && (pData->nSelectID != -2))
					{
						CMapServer::GetServer()->Log("WARNING: NPC talk process 2 return 0(code = %d)", next_id);
						goto err_talk;
					}
				}
			}
			else
				goto err_talk;
		}
		else
		{
			CMapServer::GetServer()->Log("WARNING: Talk to NPC but object been deleted(player uid = %d)", pPlayer->GetUID());
			//
err_talk:	pPlayer->ExitNPCTalk();
			pPlayer->SendNPCTalkResult(pComm, 0, 0, 0);
		}
	}
}
// ....... ??? ........
void CMapServer::CB_NPCHotelSavePos(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
//	struct MAP_Hotel_ResetBornPoint * pData;
	CMapPlayer * pPlayer;

	pPlayer = GetServer()->FindPlayerByClientID(nID);
	if(pPlayer != NULL)
	{
		if(!GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
		{
			if (pPlayer->CheckShopType(shopType_Hotel))
			{
				GetServer()->PlayerSavePos(pPlayer, 0, gameIconSize*2);	// ?U??B
			}
		}
	}
}

void CMapServer::CB_NPCHotelRest(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
//	struct MAP_Hotel_TakeRest * pData;
	CMapPlayer * pPlayer;

	pPlayer = GetServer()->FindPlayerByClientID(nID);
	if(pPlayer != NULL)
	{
		if(!GetServer()->IsObjectDeleted(pPlayer->GetHandle()))
		{
			if (pPlayer->CheckShopType(shopType_Hotel))
			{
				long nNeedGold = ::gameGetHotelRestGold( pPlayer->GetLevel() );
				if( pPlayer->GetGold() < (unsigned long long)nNeedGold )
					pPlayer->SendMsgToClientByID(20038);
				else
				{
					pPlayer->GetSaveData()->plrGold -= nNeedGold;
					pPlayer->UpdateClientGoldAndWeight();
					GetServer()->PlayerRestoreAll(pPlayer);
					pPlayer->SendMsgToClientByID(20258);
					//???log
					::SendGoldLog( nNeedGold, pPlayer, LOGTYPE_ITEM_HOTEL_REST_SPEND_GOLD, 0 );
				}
			}
		}
	}
}

void CMapServer::CB_NPCVTAskRecordResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct VT_ASK_RECORD_RESULT * pData;
	CMapPlayer * pPlayer;
	long msg_id, i, sid;
	char tmpstr[512];
	struct proto_COMM nComm;

	pData = (struct VT_ASK_RECORD_RESULT *)pBuffer;
	pPlayer = (CMapPlayer *)GetServer()->FindAndCheckCharExistByUID(pData->nPlayerUID);
	if (pPlayer)
	{
		long bagWait = (pPlayer->m_WaitVT_State & PLAYER_BAG_WAIT_VT) ? 1 : 0;
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)
		{
			if (!pPlayer->IsReady())
			{
				pPlayer->m_WaitVT_Timeout = 0;
				pPlayer->m_WaitVT_TalkID_NoData = 0;
				pPlayer->m_WaitVT_TalkID_Error = 0;
				pPlayer->m_WaitVT_TalkID_Ok = 0;
				pPlayer->m_WaitVT_State = 0;
				return;
			}
			if (!pPlayer->IsAvailable())
			{
				pPlayer->m_WaitVT_Timeout = 0;
				pPlayer->m_WaitVT_TalkID_NoData = 0;
				pPlayer->m_WaitVT_TalkID_Error = 0;
				pPlayer->m_WaitVT_TalkID_Ok = 0;
				pPlayer->m_WaitVT_State = 0;
				return;
			}
			if (!bagWait && (!pPlayer->IsNPCTalk() || !(pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_DATA)))
			{
				// ?????G?A?????a???b??????A?A???????G
				CMapServer::GetServer()->Log("WARNING: %s Get VT record but not talking", pPlayer->GetSaveData()->plrName);
				pPlayer->m_WaitVT_Timeout = 0;
				pPlayer->m_WaitVT_TalkID_NoData = 0;
				pPlayer->m_WaitVT_TalkID_Error = 0;
				pPlayer->m_WaitVT_TalkID_Ok = 0;
				pPlayer->m_WaitVT_State = 0;
				return;
			}
			//
			switch(pData->nResult)
			{
			case ASK_VT_RESULT_ERROR:		// 狝竟Τ拜肈
			default:
				msg_id = pPlayer->m_WaitVT_TalkID_Error;
				break;
			case ASK_VT_RESULT_OK:
				msg_id = pPlayer->m_WaitVT_TalkID_Ok;
				//
				// ?????_?A??s???_Server
				//
				if (pData->nItemTotal)
				{
					if (!pPlayer->CarryItem_CheckFreeSpace(pData->nItemTotal))
					{
						wsprintf(tmpstr, gameGetResourceName(24273), pData->nItemTotal);
						pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
						msg_id = 0;
						goto exit_npc;
					}
					//
					sid = GetServer()->GetVTServer();
					if ( GetServer()->IsConnectedToServer(sid) )
					{
						// ??s???w
						struct VT_UPDATE_RECORD nUpdateVT;

						memset(&nUpdateVT, 0, sizeof(nUpdateVT));
						nUpdateVT.nPlayerUID = pPlayer->GetUID();
						msg_strncpy(nUpdateVT.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nUpdateVT.szAccount));
						msg_strncpy(nUpdateVT.szCardNo, pData->szCardNo, sizeof(nUpdateVT.szCardNo));
						msg_strncpy(nUpdateVT.szCharName, pPlayer->GetSaveData()->plrName, sizeof(nUpdateVT.szCharName));
						msg_strncpy(nUpdateVT.szServerName, GetServer()->m_ServerInfo.iniServerName, sizeof(nUpdateVT.szServerName));
						CMapServer::GetServer()->SendData(sid, 0, PROTO_VT_UPDATE_RECORD, (char *)&nUpdateVT, sizeof(nUpdateVT), 0);
						// ???F??
						char tmpstr2[1024];

						PlayerDrop_Init();
						for (i=0; i<pData->nItemTotal; i++)
						{
							if (pData->nTypeID == 10)		// 杆称摸 +5 ~ +10
							{
								gameSetCreateItemSuperRandom(3);
								pPlayer->CarryItem_MakeItemOnFreeSpace(pData->nItemTbl[i].nCode, pData->nItemTbl[i].nNumber, LOGTYPE_ITEM_FROM_VIRTUAL, 1, pPlayer, 1, -1, 1);
							}
							else
							{
								// ?@?w?O???????
								if (g_Server_Setting & SERVER_FLAG_VT_ITEM_SUPER_MODE)
								{
									gameSetCreateItemSuperRandom(3);
								}
								pPlayer->CarryItem_MakeItemOnFreeSpace(pData->nItemTbl[i].nCode, pData->nItemTbl[i].nNumber, LOGTYPE_ITEM_FROM_VIRTUAL, 1, pPlayer, 1);
							}
						}
						PlayerDrop_SendResult(pPlayer, 0, 1, tmpstr);	// ??X???G??r??
						wsprintf(tmpstr2, gameGetResourceName(24177), tmpstr);
						pPlayer->SendMessage(gameChannel_System, NULL, tmpstr2);
						// ??s???A
						gameServer_CalcCharacterWeight(pPlayer->GetCharData(), pPlayer->GetItemData());
						pPlayer->UpdateClientGoldAndWeight();
						pPlayer->SaveCharData();
						//pPlayer->SaveItemData();
					}
					else
						msg_id = pPlayer->m_WaitVT_TalkID_Error;
				}
				else if (pData->nTypeID)
				{
					// !!! ?|?????w?q, 0 = ?L
					msg_id = pPlayer->m_WaitVT_TalkID_NoData;
				}
				else
					msg_id = pPlayer->m_WaitVT_TalkID_NoData;
				break;
			case ASK_VT_RESULT_NO_DATA:
				msg_id = pPlayer->m_WaitVT_TalkID_NoData;
				break;
			}
			// ?M??????
exit_npc:	pPlayer->m_WaitVT_Timeout = 0;
			pPlayer->m_WaitVT_TalkID_NoData = 0;
			pPlayer->m_WaitVT_TalkID_Error = 0;
			pPlayer->m_WaitVT_TalkID_Ok = 0;
			pPlayer->m_WaitVT_State = 0;
			//
			if (bagWait)
			{
				if (!(pData->nItemTotal && pData->nResult == ASK_VT_RESULT_OK) && msg_id)
					pPlayer->SendMessage(gameChannel_System, NULL, gameGetResourceName(msg_id));
				return;
			}
			//
			if (!msg_id)
			{
				pPlayer->ExitNPCTalk();
				//
				memset(&nComm, 0, sizeof(nComm));
				nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
				pPlayer->SendNPCTalkResult(&nComm, 0, 0, 0);
				return;
			}
			//
			// ???? NPC ?????U?@?B
			//
//CMapServer::GetServer()->Log("Test: %s Get VT record(%d, %d)", pPlayer->GetSaveData()->plrName, pPlayer->IsNPCTalk(), msg_id);
			struct MAP_TALK_TO_NPC_NEXT_STEP nEmul;

			//pPlayer->SetNPCTalk(0, NULL, msg_id);
			//pPlayer->m_nNPCSelectID[0] = 0;		// ?M????
			pPlayer->m_nNPCSelectID[0] = msg_id;
			//
			memset(&nComm, 0, sizeof(nComm));
			nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
			//nEmul.nSelectID = -2;
			nEmul.nSelectID = msg_id;
			nEmul.nSerial = pPlayer->m_nNPCTalkSerial;
			GetServer()->CB_NPCNextStep((char *)&nEmul, sizeof(nEmul), pPlayer->GetClientID(), &nComm);
		}
	}
}

void CMapServer::CB_NPCPointToVTCardResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_WEB_POINT_TO_VTCARD_RESULT * pData;
	CMapPlayer * pPlayer;
	struct proto_COMM nComm;
	long msg_id = 0;

	pData = (struct LOGIN_WEB_POINT_TO_VTCARD_RESULT *)pBuffer;
	//
	pPlayer = (CMapPlayer *)GetServer()->FindAndCheckCharExistByUID(pData->nPlayerUID);
	if (pPlayer)
	{
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)
		{
			if (!pPlayer->IsReady())
				return;
			if (!pPlayer->IsAvailable())
				return;
			if (!pPlayer->IsNPCTalk() || !(pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_DATA))
			{
				// ?????G?A?????a???b??????A?A???????G
				CMapServer::GetServer()->Log("WARNING: %s Get Point To VT Card record but not talking", pPlayer->GetSaveData()->plrName);
				return;
			}
			//
//CMapServer::GetServer()->Log("Get VT Card result(%s,%d,%s)", pPlayer->GetSaveData()->plrName, pData->nResult, pData->szCardNo);
			switch(pData->nResult)
			{
			case LWP_SERVER_BUSY:
				msg_id = pPlayer->m_WaitVT_TalkID_Error;
				break;
			case LWP_POINT_NOT_ENOUGH:
				msg_id = pPlayer->m_WaitVT_TalkID_NoData;
				break;
			case LWP_OK:
				msg_id = pPlayer->m_WaitVT_TalkID_Ok;
				// 2009/03/10 ??????X?k??
				pPlayer->CheckDupeItem();
				break;
			}
			//
			pPlayer->m_WaitVT_Timeout = 0;
			pPlayer->m_WaitVT_TalkID_NoData = 0;
			pPlayer->m_WaitVT_TalkID_Error = 0;
			pPlayer->m_WaitVT_TalkID_Ok = 0;
			pPlayer->m_WaitVT_State = 0;
			//
			if (!msg_id)
			{
				pPlayer->ExitNPCTalk();
				//
				memset(&nComm, 0, sizeof(nComm));
				nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
				pPlayer->SendNPCTalkResult(&nComm, 0, 0, 0);
				return;
			}
			//
			// ???? NPC ?????U?@?B
			//
//CMapServer::GetServer()->Log("Test: %s Get VT record(%d, %d)", pPlayer->GetSaveData()->plrName, pPlayer->IsNPCTalk(), msg_id);
			struct MAP_TALK_TO_NPC_NEXT_STEP nEmul;

			//pPlayer->SetNPCTalk(0, NULL, msg_id);
			//pPlayer->m_nNPCSelectID[0] = 0;		// ?M????
			pPlayer->m_nNPCSelectID[0] = msg_id;
			//
			memset(&nComm, 0, sizeof(nComm));
			nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
			//nEmul.nSelectID = -2;
			nEmul.nSelectID = msg_id;
			nEmul.nSerial = pPlayer->m_nNPCTalkSerial;
			GetServer()->CB_NPCNextStep((char *)&nEmul, sizeof(nEmul), pPlayer->GetClientID(), &nComm);
		}
	}
}

void CMapServer::CB_NPCVTCardAskRecordResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct VT_CARD_ASK_RECORD_RESULT * pData;
	CMapPlayer * pPlayer;
	long msg_id, sid, cnt1;
	char tmpstr[512];
	struct proto_COMM nComm;
	long update_sql, use_card = 0;
	struct itemDATA_SAVE * pItemSave;
	struct CARD_TO_ITEM_SETDATA * pGift;
	//
	struct LOG_INSERT_MSG_ACT nLogAct;
	//
	struct itemDATA_SAVE * pEmptyItemSave;
	long emptyItemIdx;
	//
#ifdef RECORD_COST_DATA
	struct MAP_CITY_DATA_PACK nCityPack;
#endif
	//
	long id, num, val;
	char tmpstr2[1024];

	pData = (struct VT_CARD_ASK_RECORD_RESULT *)pBuffer;
	pPlayer = (CMapPlayer *)GetServer()->FindAndCheckCharExistByUID(pData->nPlayerUID);
	if (pPlayer)
	{
		if (pData->nMode == 2)	// ???2?G?{??
		{
//CMapServer::GetServer()->Log("Get VT Card check result(%s,%d,%s)", pPlayer->GetSaveData()->plrName, pData->nResult, pData->szCardNo);
			if (pData->nResult == ASK_CARD_VT_RESULT_OK)
			{
				CMapServer::GetServer()->Card_SetItemStatus(pPlayer, gameITEM_ID_CARD, pData->szCardNo, gameVCARD_STATUS_OK);
			}
			else
			{
			//	if (pData->nCardFuncID == 0)		// ?N???L???d?????A?_?h?N?O ??????O????????
			//	{
			//	}
				CMapServer::GetServer()->Card_SetItemStatus(pPlayer, gameITEM_ID_CARD, pData->szCardNo, gameVCARD_STATUS_FAIL);
			}
		}
		else
		{
			if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)
			{
				if (!pPlayer->IsReady())
					return;
				if (!pPlayer->IsAvailable())
					return;
				if (!pPlayer->IsNPCTalk() || !(pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_DATA))
				{
					// ?????G?A?????a???b??????A?A???????G
					CMapServer::GetServer()->Log("WARNING: %s Get VT Card record but not talking", pPlayer->GetSaveData()->plrName);
					return;
				}
				//
	//CMapServer::GetServer()->Log("Get VT Card result(%s,%d,%s)", pPlayer->GetSaveData()->plrName, pData->nResult, pData->szCardNo);
				switch(pData->nResult)
				{
				case ASK_CARD_VT_RESULT_ERROR:		// 狝竟Τ拜肈
				default:
					msg_id = pPlayer->m_WaitVT_TalkID_Error;
					break;
				case ASK_CARD_VT_RESULT_OK:
					msg_id = pPlayer->m_WaitVT_TalkID_Ok;
					//
					// ?? Card ???_?A??s???_Server
					//
#if (defined(GBMode) || defined(GBMode_TW))
					if ((pData->nCardFuncID != 1) && (pData->nCardFuncID != 2))		// ?w???G??e??1, 2
					{
						GetServer()->Log("WARNING: Card func_id = %d", pData->nCardFuncID);
						msg_id = pPlayer->m_WaitVT_TalkID_NoData;
						break;
					}
#else
 #ifdef ItemMode
	#ifdef USE_BIG_VTCARD_ITEM
					if ((pData->nCardFuncID < 2) || (pData->nCardFuncID > 5))
	#else
					if (pData->nCardFuncID != 2)		// 蝗布ヘ玡2
	#endif
					{
						GetServer()->Log("WARNING: Card func_id = %d", pData->nCardFuncID);
						msg_id = pPlayer->m_WaitVT_TalkID_NoData;
						break;
					}
 #else
					if (pData->nCardFuncID != 1)		// ???G??e??1
					{
						GetServer()->Log("WARNING: Card func_id = %d", pData->nCardFuncID);
						msg_id = pPlayer->m_WaitVT_TalkID_NoData;
						break;
					}
 #endif
#endif
					//
					if (pData->szCardNo[0])
					{
						sid = GetServer()->GetVTServer();
						if ( GetServer()->IsConnectedToServer(sid) )
						{
							update_sql = 0;
							switch(pData->nMode)
							{
							case 0:		// ??d???F??
							case 10:
								if (!pPlayer->CarryItem_CheckFreeSpace(1))
								{
									wsprintf(tmpstr, gameGetResourceName(24273), 1);
									pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
									msg_id = 0;
									goto exit_npc;
								}
								// ?????
								pEmptyItemSave = NULL;
								emptyItemIdx = -1;

								//for(cnt1=0; cnt1<gameMAX_CARRYITEM; cnt1++)
								val = ::gameServer_GetCarryItemMax(pPlayer->GetSaveData());
								for(cnt1=0; cnt1<val; cnt1++)
								{
									pItemSave = pPlayer->GetSingleItemData( cnt1 );

									if( pItemSave && pItemSave->m_Item.itemNumber == 0 )
									{
										pEmptyItemSave = pItemSave;
										emptyItemIdx = cnt1;
										break;
									}
								}
								if (pEmptyItemSave)
								{
									struct itemDATASHOWSELF newItem;

									gameCreateItem_Card(&newItem, GetServer()->GenerateItemUID(), pPlayer->GetUID(), pPlayer->GetName(), gameITEM_ID_CARD, pData->szCardNo, pData->nCardDay, pData->nCardFuncID);
									::gameServer_ItemSave_MakeShowSelf( &newItem, pEmptyItemSave );
									pPlayer->SaveItemData();
									pPlayer->UpdateClientSingleItem( emptyItemIdx, 0 );
									GetServer()->SendLogMessage_Item( pPlayer, LOGTYPE_ITEM_FROM_VIRTUAL, 0, pEmptyItemSave );
									//
									PlayerDrop_Init();
									PlayerDrop_SetItem(gameITEM_ID_CARD, 1);
									PlayerDrop_SendResult(pPlayer, 0, 1, tmpstr);	// ??X???G??r??
									wsprintf(tmpstr2, gameGetResourceName(24177), tmpstr);
									pPlayer->SendMessage(gameChannel_System, NULL, tmpstr2);
									// ??s???A
									gameServer_CalcCharacterWeight(pPlayer->GetCharData(), pPlayer->GetItemData());
									pPlayer->UpdateClientGoldAndWeight();
									//pPlayer->SaveCharData();
									//pPlayer->SaveItemData();
									//
									update_sql++;
								}
								break;
							case 1:		// ????F??
							case 4:
								emptyItemIdx = -1;

								pItemSave = CMapServer::GetServer()->Card_FindItem(pPlayer, gameITEM_ID_CARD, pData->szCardNo, &emptyItemIdx);
								if (pItemSave)
								{
									// ?s?@ Log ????
									memset(&nLogAct, 0, sizeof(nLogAct));
									nLogAct.nType = LOGTYPE_ACT_PLAYER_USE_CARD;
									nLogAct.nMapCode = pPlayer->GetMapCode();
									nLogAct.nPosX = pPlayer->GetPosX();
									nLogAct.nPosY = pPlayer->GetPosY();
									::msg_strncpy(nLogAct.nName, pPlayer->GetName(), sizeof(nLogAct.nName));
									nLogAct.nData1 = pItemSave->m_CardItem.itemCode;
									nLogAct.nData2 = pItemSave->m_CardItem.itemCardFuncID;
									nLogAct.nData3 = pItemSave->m_CardItem.itemCardDay;
									nLogAct.nData4 = pItemSave->m_CardItem.itemUID.iUID_Serial;
									nLogAct.nData5 = pItemSave->m_CardItem.itemUID.iUID_Time;
									::msg_strncpy(nLogAct.nStr1, pItemSave->m_CardItem.itemCardNo, sizeof(nLogAct.nStr1));
									// ???
									memset(pItemSave, 0, sizeof(struct itemDATA_SAVE));
									//
									pPlayer->UpdateClientSingleItem( emptyItemIdx, 0 );
									// ??s???A
									gameServer_CalcCharacterWeight(pPlayer->GetCharData(), pPlayer->GetItemData());
									pPlayer->UpdateClientGoldAndWeight();
									//pPlayer->SaveCharData();
									pPlayer->SaveItemData();
									//
									use_card++;
									update_sql++;
								}
								break;
							case 3:		// ?????]
								if (!pPlayer->CarryItem_CheckFreeSpace(1))
								{
									wsprintf(tmpstr, gameGetResourceName(24273), 1);
									pPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
									msg_id = 0;
									goto exit_npc;
								}
								//
								emptyItemIdx = -1;
								pItemSave = CMapServer::GetServer()->Card_FindItem(pPlayer, gameITEM_ID_CARD, pData->szCardNo, &emptyItemIdx);
								if (pItemSave)
								{
									pGift = gameGetCardToItemPtr(pData->nData);
									if (pGift)
									{
										id = pGift->ciItemCode;
										num = pGift->ciItemNumber;
										//
										if (!gameGetItemPtr(id))
											goto no_gift;
										PlayerDrop_Init();
										if (pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_VTCARD_GIFT, 1, pPlayer, 1))
										{
											PlayerDrop_SendResult(pPlayer, 0, 1, tmpstr);	// ??X???G??r??
											wsprintf(tmpstr2, gameGetResourceName(24287), gameGetResourceName(gameGetItemNameIDByID(gameITEM_ID_CARD)), tmpstr);
											pPlayer->SendMessage(gameChannel_System, NULL, tmpstr2);
											//
											// ?s?@ Log ????
											memset(&nLogAct, 0, sizeof(nLogAct));
											nLogAct.nType = LOGTYPE_ACT_PLAYER_USE_CARD;
											nLogAct.nMapCode = pPlayer->GetMapCode();
											nLogAct.nPosX = pPlayer->GetPosX();
											nLogAct.nPosY = pPlayer->GetPosY();
											::msg_strncpy(nLogAct.nName, pPlayer->GetName(), sizeof(nLogAct.nName));
											nLogAct.nData1 = pItemSave->m_CardItem.itemCode;
											nLogAct.nData2 = id;
											nLogAct.nData3 = pItemSave->m_CardItem.itemCardDay;
											nLogAct.nData4 = pItemSave->m_CardItem.itemUID.iUID_Serial;
											nLogAct.nData5 = pItemSave->m_CardItem.itemUID.iUID_Time;
											::msg_strncpy(nLogAct.nStr1, pItemSave->m_CardItem.itemCardNo, sizeof(nLogAct.nStr1));
											::msg_strncpy(nLogAct.nStr2, gameGetResourceName(gameGetItemNameIDByID(id)), sizeof(nLogAct.nStr2));
											// ???
											memset(pItemSave, 0, sizeof(struct itemDATA_SAVE));
											//
											pPlayer->UpdateClientSingleItem( emptyItemIdx, 0 );
											//
											// ??s???A
											gameServer_CalcCharacterWeight(pPlayer->GetCharData(), pPlayer->GetItemData());
											pPlayer->UpdateClientGoldAndWeight();
											//pPlayer->SaveCharData();
											pPlayer->SaveItemData();
											//
											use_card++;
											update_sql++;
										}
									}
									else
									{
		no_gift:						msg_id = pPlayer->m_WaitVT_TalkID_Error;
									}
									//
	/*								switch(pData->nData)	// Gift Type
									{
									case 1:					// ?s?K??U
										// .....................
										id = gameITEM_ID_VTCARD_LOTTO;
										num = 10;
			give_gift:					if (!gameGetItemPtr(id))
											goto no_gift;
										PlayerDrop_Init();
										if (pPlayer->CarryItem_MakeItemOnFreeSpace(id, num, LOGTYPE_ITEM_VTCARD_GIFT, 1, pPlayer, 1))
										{
											PlayerDrop_SendResult(pPlayer, 0, 1, tmpstr);	// ??X???G??r??
											wsprintf(tmpstr2, gameGetResourceName(24287), gameGetResourceName(gameGetItemNameIDByID(gameITEM_ID_CARD)), tmpstr);
											pPlayer->SendMessage(gameChannel_System, NULL, tmpstr2);
											//
											// ?s?@ Log ????
											memset(&nLogAct, 0, sizeof(nLogAct));
											nLogAct.nType = LOGTYPE_ACT_PLAYER_USE_CARD;
											nLogAct.nMapCode = pPlayer->GetMapCode();
											nLogAct.nPosX = pPlayer->GetPosX();
											nLogAct.nPosY = pPlayer->GetPosY();
											::msg_strncpy(nLogAct.nName, pPlayer->GetName(), sizeof(nLogAct.nName));
											nLogAct.nData1 = pItemSave->m_CardItem.itemCode;
											nLogAct.nData2 = id;
											nLogAct.nData3 = pItemSave->m_CardItem.itemCardDay;
											nLogAct.nData4 = pItemSave->m_CardItem.itemUID.iUID_Serial;
											nLogAct.nData5 = pItemSave->m_CardItem.itemUID.iUID_Time;
											::msg_strncpy(nLogAct.nStr1, pItemSave->m_CardItem.itemCardNo, sizeof(nLogAct.nStr1));
											::msg_strncpy(nLogAct.nStr2, gameGetResourceName(gameGetItemNameIDByID(id)), sizeof(nLogAct.nStr2));
											// ???
											memset(pItemSave, 0, sizeof(struct itemDATA_SAVE));
											//
											pPlayer->UpdateClientSingleItem( emptyItemIdx, 0 );
											//
											// ??s???A
											gameServer_CalcCharacterWeight(pPlayer->GetCharData(), pPlayer->GetItemData());
											pPlayer->UpdateClientGoldAndWeight();
											//pPlayer->SaveCharData();
											pPlayer->SaveItemData();
											//
											use_card++;
											update_sql++;
										}
										break;
#ifdef GBMode
									case 2:					// ??~????U
										//id = gameITEM_ID_VTCARD_LOTTO_CJOB1;
										//id = gameITEM_ID_VTCARD_LOTTO_CJOB21;
										id = gameITEM_ID_VTCARD_LOTTO_CJOB31;
										num = 5;
										goto give_gift;
										break;
									case 3:					// ??~????U
										//id = gameITEM_ID_VTCARD_LOTTO_CJOB2;
										//id = gameITEM_ID_VTCARD_LOTTO_CJOB22;
										id = gameITEM_ID_VTCARD_LOTTO_CJOB32;
										num = 5;
										goto give_gift;
										break;
									case 4:					// ??~????U
										//id = gameITEM_ID_VTCARD_LOTTO_CJOB3;
										//id = gameITEM_ID_VTCARD_LOTTO_CJOB23;
										id = gameITEM_ID_VTCARD_LOTTO_CJOB33;
										num = 5;
										goto give_gift;
										break;
									case 5:					// ??~????U
										//id = gameITEM_ID_VTCARD_LOTTO_CJOB4;
										//id = gameITEM_ID_VTCARD_LOTTO_CJOB24;
										id = gameITEM_ID_VTCARD_LOTTO_CJOB34;
										num = 5;
										goto give_gift;
										break;
#else
									case 2:					// ??~????U
										id = gameITEM_ID_VTCARD_LOTTO_JOB1;
										num = 5;
										goto give_gift;
										break;
									case 3:					// ??~????U
										id = gameITEM_ID_VTCARD_LOTTO_JOB2;
										num = 5;
										goto give_gift;
										break;
									case 4:					// ??~????U
										id = gameITEM_ID_VTCARD_LOTTO_JOB3;
										num = 5;
										goto give_gift;
										break;
									case 5:					// ??~????U
										id = gameITEM_ID_VTCARD_LOTTO_JOB4;
										num = 5;
										goto give_gift;
										break;
#endif
#if (defined(GBMode) || !defined(USE_PACKAGE_DATA))
									case 6:					// ??x??U
										id = gameITEM_ID_VTCARD_LOTTO_WN;
										num = 1;
										goto give_gift;
										break;
									case 7:					// ?Z?x??U
										id = gameITEM_ID_VTCARD_LOTTO_WU;
										num = 1;
										goto give_gift;
										break;
									case 40:				// ?g?~?y?O?P
										id = gameITEM_ID_VTCARD_LOTTO_YEAR;
										num = 2;
										goto give_gift;
										break;
#endif
									case 8:					// ?K????
										id = gameITEM_ID_USE_UNDEAD;
										num = 20;
										goto give_gift;
										break;
									case 9:					// ??\?Y
										id = gameITEM_ID_RESET_ATTR;
#ifdef GBMode
										num = 8;
#else
										num = 20;
#endif
										goto give_gift;
										break;
									case 10:				// ??????~??U
										id = gameITEM_ID_VTCARD_LOTTO_JOB1_4;
										num = 5;
										goto give_gift;
										break;
									case 11:				// ??????~??U
										id = gameITEM_ID_VTCARD_LOTTO_JOB2_4;
										num = 5;
										goto give_gift;
										break;
									case 12:				// ??????~??U
										id = gameITEM_ID_VTCARD_LOTTO_JOB3_4;
										num = 5;
										goto give_gift;
										break;
									case 13:				// ??????~??U
										id = gameITEM_ID_VTCARD_LOTTO_JOB4_4;
										num = 5;
										goto give_gift;
										break;
									case 14:					// ?K?????P
										id = gameITEM_ID_USE_UNDEAD_ITEM;
										num = 80;
										goto give_gift;
										break;
									case 15:					// ????W?L??U
										id = gameITEM_ID_VTCARD_LOTTO_JOBA_1;
										num = 5;
										goto give_gift;
									case 16:					// ??U?L????U
										id = gameITEM_ID_VTCARD_LOTTO_JOBA_2;
										num = 5;
										goto give_gift;
									case 17:					// ???Q?r?N??U
										id = gameITEM_ID_VTCARD_LOTTO_0001;
										num = 5;
										goto give_gift;
									case 18:					// ??@?????U
										id = gameITEM_ID_VTCARD_LOTTO_0002;
										num = 5;
										goto give_gift;
									case 19:					// ?_??x?v??U
										id = gameITEM_ID_VTCARD_LOTTO_0003;
										num = 5;
										goto give_gift;
									case 20:					// ??k??h??U
										id = gameITEM_ID_VTCARD_LOTTO_0004;
										num = 5;
										goto give_gift;
									case 21:					//12*??_?t???U
										id = gameITEM_ID_VTCARD_LOTTO_V5_001;
										num = 12;
										goto give_gift;
									case 22:					//5*???r?N??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_002;
										num = 5;
										goto give_gift;
									case 23:					//5*??????U
										id = gameITEM_ID_VTCARD_LOTTO_V5_003;
										num = 5;
										goto give_gift;
									case 24:					//5*???x?v??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_004;
										num = 5;
										goto give_gift;
									case 25:					//5*????h??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_005;
										num = 5;
										goto give_gift;
									case 26:					//5*????r?N??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_011;
										num = 5;
										goto give_gift;
									case 27:					//5*????????U
										id = gameITEM_ID_VTCARD_LOTTO_V5_012;
										num = 5;
										goto give_gift;
									case 28:					//5*???s?x?v??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_013;
										num = 5;
										goto give_gift;
									case 29:					//5*?_????h??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_014;
										num = 5;
										goto give_gift;
									case 30:					//20*?g???[????
										id = gameITEM_ID_VTCARD_ADD_EXP;
										num = 20;
										goto give_gift;
										break;
									case 31:					//5*?Q??r?N??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_021;
#ifdef GBMode
										num = 3;
#else
										num = 5;
#endif
										goto give_gift;
									case 32:					//5*镀花城褐砋
										id = gameITEM_ID_VTCARD_LOTTO_V5_022;
#ifdef GBMode
										num = 3;
#else
										num = 5;
#endif
										goto give_gift;
									case 33:					//5*?????x?v??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_023;
#ifdef GBMode
										num = 3;
#else
										num = 5;
#endif
										goto give_gift;
									case 34:					//5*?_???h??U
										id = gameITEM_ID_VTCARD_LOTTO_V5_024;
#ifdef GBMode
										num = 3;
#else
										num = 5;
#endif
										goto give_gift;
									case 35:					//5*????|????U
										id = gameITEM_ID_VTCARD_LOTTO_WUHU4;
										num = 5;
										goto give_gift;
									case 36:					// 5*??N???|??U
										id = gameITEM_ID_VTCARD_LOTTO_V6_01;
										num = 5;
										goto give_gift;
									case 37:					// 5*????????U
										id = gameITEM_ID_VTCARD_LOTTO_V6_02;
										num = 5;
										goto give_gift;
									case 38:					// 4*???Q???U
										id = gameITEM_ID_VTCARD_LOTTO_V7_01;
										num = 4;
										goto give_gift;
									case 39:					// 4*???Q???U
										id = gameITEM_ID_VTCARD_LOTTO_V7_02;
										num = 4;
										goto give_gift;
									case 41:					// 4*?C??x???N?]
										id = gameITEM_ID_VTCARD_LOTTO_71JULY;
										num = 4;
										goto give_gift;
									case 42:					// 4*る瞨花κ
										id = gameITEM_ID_VTCARD_LOTTO_72JULY;
										num = 4;
										goto give_gift;
									case 43:					// 4*?K??x???N?]
										id = gameITEM_ID_VTCARD_LOTTO_81AUG;
										num = 4;
										goto give_gift;
									case 44:					// 4*る瞨花κ
										id = gameITEM_ID_VTCARD_LOTTO_82AUG;
										num = 4;
										goto give_gift;
									case 45:					// 4*?E??x???N?]
										id = gameITEM_ID_VTCARD_LOTTO_91SEP;
										num = 4;
										goto give_gift;
									case 46:					// 4*る瞨花κ
										id = gameITEM_ID_VTCARD_LOTTO_92SEP;
										num = 4;
										goto give_gift;
									case 47:					// 5*?Z?x?p?Z??
										id = gameITEM_ID_VTCARD_LOTTO_WU01;
										num = 5;
										goto give_gift;
									case 48:					// 5*??x?p?Z??
										id = gameITEM_ID_VTCARD_LOTTO_WN01;
										num = 5;
										goto give_gift;
									case 49:					// 4*弘絤锣ネ
										id = gameITEM_ID_VTCARD_LETTO_GEN;
										num = 4;
										goto give_gift;
									case 50:					// 4*???Z?r????U
										id = gameITEM_ID_VTCARD_LETTO_001;
										num = 4;
										goto give_gift;
									case 51:					// 4*猌瞨花褐砋
										id = gameITEM_ID_VTCARD_LETTO_002;
										num = 4;
										goto give_gift;
									case 52:					// 30*??U
										id = gameITEM_ID_VTCARD_LETTO_CNN;
										num = 30;
										goto give_gift;
									case 53:					// 30*??U
										id = gameITEM_ID_VTCARD_LETTO_2008_1;
										num = 5;
										goto give_gift;
									case 54:					// 30*??U
										id = gameITEM_ID_VTCARD_LETTO_2008_2;
										num = 5;
										goto give_gift;
									case 55:					// 40*??U
										id = gameITEM_ID_VTCARD_LETTO_SDL;
										num = 40;
										goto give_gift;
									case 56:					// ??W???}?
										id = gameITEM_ID_VTCARD_LETTO_080306_1;
										num = 5;
										goto give_gift;
									case 57:					// 花冻搂
										id = gameITEM_ID_VTCARD_LETTO_080306_2;
										num = 5;
										goto give_gift;
									case 58:					// ?M??d???
										id = gameITEM_ID_VTCARD_LETTO_080306_3;
										num = 5;
										goto give_gift;
									case 59:					// 跑ほ馋代搂
										id = gameITEM_ID_VTCARD_LETTO_080306_4;
										num = 5;
										goto give_gift;
									case 60:					// 疍ネ褐砋
										id = gameITEM_ID_VTCARD_LETTO_080318_1;
										num = 9;
										goto give_gift;
									case 61:					// ??E???L?]
										id = gameITEM_ID_VTCARD_LETTO_080318_2;
										num = 5;
										goto give_gift;
									case 62:					// 绰Π
										id = gameITEM_ID_VTCARD_LETTO_080318_3;
										num = 5;
										goto give_gift;
									case 63:					// 冻萝﹀
										id = gameITEM_ID_VTCARD_LETTO_080318_4;
										num = 5;
										goto give_gift;
									case 64:					// 褐
										id = gameITEM_ID_VTCARD_LETTO_080328_1;
										num = 24;
										goto give_gift;
									case 65:					// 瞓てぇ才
										id = gameITEM_ID_VTCARD_LETTO_080430_1;
										num = 13;
										goto give_gift;
									case 66:					// 瞓てぇ
										id = gameITEM_ID_VTCARD_LETTO_080430_2;
										num = 13;
										goto give_gift;
									case 67:					// 籠璣动褐砋
										id = gameITEM_ID_VTCARD_LETTO_080514_1;
										num = 5;
										goto give_gift;
									case 68:					// ???@???h??U
										id = gameITEM_ID_VTCARD_LETTO_080514_2;
										num = 5;
										goto give_gift;
									case 69:					// 芬て褐砋
										id = gameITEM_ID_VTCARD_LETTO_080521;
										num = 6;
										goto give_gift;
									case 70:					// ??G???
										id = gameITEM_ID_VTCARD_LETTO_080606;
										num = 40;
										goto give_gift;
									case 71:					// 確辈禸
										id = gameITEM_ID_VTCARD_LETTO_080626_1;
										num = 180;
										goto give_gift;
									case 72:					// 粆┚才
										id = gameITEM_ID_VTCARD_LETTO_080626_2;
										num = 45;
										goto give_gift;
									case 73:					// エ褐砋
										id = gameITEM_ID_VTCARD_LETTO_080731_1;
										num = 9;
										goto give_gift;
									case 74:					// ┮┸名褐砋
										id = gameITEM_ID_VTCARD_LETTO_080731_2;
										num = 9;
										goto give_gift;
									case 75:					// 臦褐砋 眎る布传ㄢ(GBMode)
										id = gameITEM_ID_VTCARD_LETTO_080827;
										num = 2;
										goto give_gift;
									case 76:					// ?d?x???](GBMode)
										id = gameITEM_ID_VTCARD_LETTO_080904;
										num = 2;
										goto give_gift;
										break;
									case 77:					// 瞨盢Н褐砋
										id = gameITEM_ID_VTCARD_LETTO_080905_1;
										num = 9;
										goto give_gift;
										break;
									case 78:					// ??x???]??U
										id = gameITEM_ID_VTCARD_LETTO_080905_2;
										num = 9;
										goto give_gift;
										break;
									case 79:					// じ酬褐砋*7
										id = gameITEM_ID_VTCARD_LETTO_080924;
										num = 7;
										goto give_gift;
										break;
									case 80:					// ??a???~??U
										id = gameITEM_ID_VTCARD_LETTO_081015_1;
										num = 18;
										goto give_gift;
										break;
									case 81:					// 疭匡弘芬褐砋
										id = gameITEM_ID_VTCARD_LETTO_081015_2;
										num = 9;
										goto give_gift;
										break;
									case 82:					// ?U?t?`???_?c*8 82
										id = gameITEM_ID_VTCARD_LETTO_081029_1;
										num = 8;
										goto give_gift;
										break;
									case 83:					// ?U?t?`???_?c*4 83
										id = gameITEM_ID_VTCARD_LETTO_081029_2;
										num = 4;
										goto give_gift;
										break;
									case 84:					// 瓣褐砋*5 84
										id = gameITEM_ID_VTCARD_LETTO_081113;
										num = 5;
										goto give_gift;
										break;
									case 85:					// 褐 20 じ 眎传 14  ID 85
										id = gameITEM_ID_VTCARD_LETTO_081127_1;
										num = 14;
										goto give_gift;
										break;
									case 86:					// ň脄才 25 じ 眎传 10  ID 86
										id = gameITEM_ID_VTCARD_LETTO_081127_2;
										num = 10;
										goto give_gift;
										break;
									case 87:					// ??_?? 100 ?? ?@?i?? 4 ?? ID 87
										id = gameITEM_ID_VTCARD_LETTO_081127_3;
										num = 4;
										goto give_gift;
										break;
									case 88:					// ?y?P?K*9
										id = gameITEM_ID_VTCARD_LETTO_081211_1;
										num = 9;
										goto give_gift;
										break;
									case 89:					// κ芬葵*36
										id = gameITEM_ID_VTCARD_LETTO_081211_2;
										num = 36;
										goto give_gift;
										break;
									case 90:					// ???B??q??U*3
										id = gameITEM_ID_VTCARD_LETTO_090114;
										num = 3;
										goto give_gift;
										break;
									case 91:					// Нぱ褐砋*3
										id = gameITEM_ID_VTCARD_LETTO_090311;
										num = 3;
										goto give_gift;
										break;
									case 92:					// 伐猌Ρ褐砋
										id = gameITEM_ID_VTCARD_LETTO_090319_1;
										num = 9;
										goto give_gift;
										break;
									case 93:					// うじ稧璼褐砋
										id = gameITEM_ID_VTCARD_LETTO_090319_2;
										num = 9;
										goto give_gift;
										break;
									case 94:					// ?m???K??U
										id = gameITEM_ID_VTCARD_LETTO_090401;
										num = 3;
										goto give_gift;
										break;
									case 95:					// ???p?^??U
										id = gameITEM_ID_VTCARD_LETTO_090409;
										num = 9;
										goto give_gift;
										break;
									case 96:					// ?n???_?Q?O
										id = gameITEM_ID_VTCARD_LETTO_090423;
										num = 36;
										goto give_gift;
										break;
									default:
			no_gift:					msg_id = pPlayer->m_WaitVT_TalkID_Error;
										break;
									}
	*/
								}
								break;
							}
							//
							if (update_sql)
							{
								// ??s???w
								struct VT_CARD_UPDATE_RECORD nUpdateVT;

								memset(&nUpdateVT, 0, sizeof(nUpdateVT));
								nUpdateVT.nPlayerUID = pPlayer->GetUID();
								nUpdateVT.nMode = pData->nMode;
								nUpdateVT.nItemType = (unsigned char)pData->nData;		// Gift Type
								msg_strncpy(nUpdateVT.szAccount, pPlayer->GetSaveData()->plrAccount, sizeof(nUpdateVT.szAccount));
								msg_strncpy(nUpdateVT.szCardNo, pData->szCardNo, sizeof(nUpdateVT.szCardNo));
								msg_strncpy(nUpdateVT.szCharName, pPlayer->GetSaveData()->plrName, sizeof(nUpdateVT.szCharName));
								msg_strncpy(nUpdateVT.szServerName, GetServer()->m_ServerInfo.iniServerName, sizeof(nUpdateVT.szServerName));
								CMapServer::GetServer()->SendData(sid, 0, PROTO_VT_CARD_UPDATE_RECORD, (char *)&nUpdateVT, sizeof(nUpdateVT), 0);
								// ??????? Log
								if (use_card)
								{
									CMapServer::GetServer()->SendLogMessage_Act(&nLogAct);
									//
#ifdef RECORD_COST_DATA
									memset(&nCityPack, 0, sizeof(nCityPack));
									nCityPack.nType = TYPE_PACK_RECORD_VTCARD_COST;
									nCityPack.nData1 = pPlayer->GetUID();
									nCityPack.nData2 = gameGetVTCardPointByFuncID(pData->nCardFuncID);
									GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
#endif
									//
									//Card_FindItemAndMarkFail(pPlayer, gameITEM_ID_CARD, pData->szCardNo);
									// 2009/03/10 ??????X?k??
									pPlayer->CheckDupeItem();
								}
							}
						}
						else
							msg_id = pPlayer->m_WaitVT_TalkID_Error;
					}
					else
						msg_id = pPlayer->m_WaitVT_TalkID_NoData;
					break;
				case ASK_CARD_VT_RESULT_NO_DATA:
					msg_id = pPlayer->m_WaitVT_TalkID_NoData;
					break;
				}
				// ?M??????
	exit_npc:	pPlayer->m_WaitVT_Timeout = 0;
				pPlayer->m_WaitVT_TalkID_NoData = 0;
				pPlayer->m_WaitVT_TalkID_Error = 0;
				pPlayer->m_WaitVT_TalkID_Ok = 0;
				pPlayer->m_WaitVT_State = 0;
				//
				if (!msg_id)
				{
					pPlayer->ExitNPCTalk();
					//
					memset(&nComm, 0, sizeof(nComm));
					nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
					pPlayer->SendNPCTalkResult(&nComm, 0, 0, 0);
					return;
				}
				//
				// ???? NPC ?????U?@?B
				//
	//CMapServer::GetServer()->Log("Test: %s Get VT record(%d, %d)", pPlayer->GetSaveData()->plrName, pPlayer->IsNPCTalk(), msg_id);
				struct MAP_TALK_TO_NPC_NEXT_STEP nEmul;

				//pPlayer->SetNPCTalk(0, NULL, msg_id);
				//pPlayer->m_nNPCSelectID[0] = 0;		// ?M????
				pPlayer->m_nNPCSelectID[0] = msg_id;
				//
				memset(&nComm, 0, sizeof(nComm));
				nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
				//nEmul.nSelectID = -2;
				nEmul.nSelectID = msg_id;
				nEmul.nSerial = pPlayer->m_nNPCTalkSerial;
				GetServer()->CB_NPCNextStep((char *)&nEmul, sizeof(nEmul), pPlayer->GetClientID(), &nComm);
			}
		}
	}
}

#ifdef CROSS_SERVER_SYSTEM

void CMapServer::KS_ConnectToCrossServer(CMapPlayer * pPlayer, unsigned long new_player_uid, long map_id, long posx, long posy, char * map_server_ip, long map_server_port, long serial)
{
	struct MAP_CHANGE_MAP_SERVER * pChangeMsg;

	pPlayer->MapCtrl_UnLock(1);
	if (pPlayer->IsCrouch())
		pPlayer->Crouch();
	pPlayer->m_nTeleport2MapCode = 0;		// ?M????e?]?w 2010/04/01
	// ?o?q?L?k?????KS?A???DNPC TALK?i?J???L?h
	//	ServerMsg.m_nMapCode	= nMapID;
	//	ServerMsg.m_nUID		= GetUID();
	//	ServerMsg.nAccountID	= GetAccountID();
	//	ServerMsg.m_nLimitPlayer = limit_player_count;
	//	ServerMsg.nMapFlags = 0;
	//	if (IsBotPlayer())
	//		ServerMsg.nMapFlags |= LOGIN_CHAR_FLAGS_IS_BOT;
	//	ServerMsg.nMapFlags |= m_nWebIP_Flags;
	//
	// ?T?w?a??i?H????, ?x?s???a????]?w???u?T??
	pChangeMsg = pPlayer->GetChangeMapServerData();
	pChangeMsg->m_nMapCode = map_id;
	pChangeMsg->m_Pos.x = posx;
	pChangeMsg->m_Pos.y = posy;
	pChangeMsg->m_nCheckCode = serial;
	pChangeMsg->m_nPort = map_server_port;
	msg_strncpy(pChangeMsg->m_IP, map_server_ip, sizeof(pChangeMsg->m_IP));
	//
	pPlayer->m_KS_NewPlayerUID = new_player_uid;
	// ???`???_??
	if (pPlayer->GetHP() <= 0)
		pPlayer->ResurrectImmed(0);
	pPlayer->m_nTeleportMapCode = 0;			// ?M????e?]?w	2010/01/12
	pPlayer->m_nTeleport2MapCode = 0;
	//	
	GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 1 );
	// By LRG
	pPlayer->LeaveMapBeforeChangeMap();		// ?b?????m?e??????????
	//pPlayer->SetMapID();					// ???O?u???n???L?h
	//pPlayer->SetPos(x, y);				// ???O?u???n???L?h
	pPlayer->SaveAllData_Lite();
	pPlayer->SetReady(false);
	pPlayer->SetExitCode(CMapPlayer::EXIT_CHANGE_KS_SERVER);
	GetServer()->DeleteObject(pPlayer->GetHandle());
}

void CMapServer::KS_ReturnToOriginServer(CMapPlayer * pPlayer, unsigned long new_player_uid, long map_id, long posx, long posy, char * map_server_ip, long map_server_port, long serial)
{
	struct MAP_CHANGE_MAP_SERVER * pChangeMsg;

	pPlayer->MapCtrl_UnLock(1);
	if (pPlayer->IsCrouch())
		pPlayer->Crouch();
	pPlayer->m_nTeleport2MapCode = 0;		// ?M????e?]?w 2010/04/01
	// ?o?q?L?k?????KS?A???DNPC TALK?i?J???L?h
	//	ServerMsg.m_nMapCode	= nMapID;
	//	ServerMsg.m_nUID		= GetUID();
	//	ServerMsg.nAccountID	= GetAccountID();
	//	ServerMsg.m_nLimitPlayer = limit_player_count;
	//	ServerMsg.nMapFlags = 0;
	//	if (IsBotPlayer())
	//		ServerMsg.nMapFlags |= LOGIN_CHAR_FLAGS_IS_BOT;
	//	ServerMsg.nMapFlags |= m_nWebIP_Flags;
	//
	// ?T?w?a??i?H????, ?x?s???a????]?w???u?T??
	pChangeMsg = pPlayer->GetChangeMapServerData();
	pChangeMsg->m_nMapCode = map_id;
	pChangeMsg->m_Pos.x = posx;
	pChangeMsg->m_Pos.y = posy;
	pChangeMsg->m_nCheckCode = serial;
	pChangeMsg->m_nPort = map_server_port;
	msg_strncpy(pChangeMsg->m_IP, map_server_ip, sizeof(pChangeMsg->m_IP));
	//
	pPlayer->m_KS_NewPlayerUID = new_player_uid;
	// ???`???_??
	if (pPlayer->GetHP() <= 0)
		pPlayer->ResurrectImmed(0);
	pPlayer->m_nTeleportMapCode = 0;			// ?M????e?]?w	2010/01/12
	pPlayer->m_nTeleport2MapCode = 0;
	//	
	GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 1 );
	// By LRG
	pPlayer->LeaveMapBeforeChangeMap();		// ?b?????m?e??????????
	//pPlayer->SetMapID();					// ???O?u???n???L?h
	//pPlayer->SetPos(x, y);				// ???O?u???n???L?h
	pPlayer->SaveAllData_Lite();
	pPlayer->SetReady(false);
	pPlayer->SetExitCode(CMapPlayer::EXIT_KS_SERVER_RETURN);
	GetServer()->DeleteObject(pPlayer->GetHandle());
}

 #ifdef CROSS_SERVER_CWAR
// ??s??@???a?n??
void CMapServer::KS_SendPlayerRedScoreToLogin(unsigned long player_uid, unsigned long rscore, unsigned long score)
{
	struct LOGIN_UPDATE_KS_SCORE nInfo;

	nInfo.nTotal = 65535;
	nInfo.nInfo[0].nPlayerUID = player_uid;
	nInfo.nInfo[0].nRedScore = rscore;
	nInfo.nInfo[0].nScoreWar = 0;
	nInfo.nInfo[0].nScoreHistory = score;
	SendData( m_hLoginServer, 0, PROTO_LOGIN_UPDATE_KS_SCORE, (char *)&nInfo, nInfo.GetSize(), 0 );
	//
  #ifndef USE_PACKAGE_DATA
	CMapServer::GetServer()->Log("DEBUG: 魁眔縩だ(%d,%d)", nInfo.nInfo[0].nPlayerUID, nInfo.nInfo[0].nRedScore);
  #endif
}

void CMapServer::CB_LoginUpdateClientArmyData(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_UPDATE_CLIENT_ARMY_DATA * pData;
	CMapPlayer * pPlayer;

	pData = (struct LOGIN_UPDATE_CLIENT_ARMY_DATA *)pBuffer;
 #ifndef USE_PACKAGE_DATA
	GetServer()->Log("DEBUG: 穝阁狝瓁刮戈(%d)", pData->nPlayerUID);
 #endif
	//
	pPlayer = (CMapPlayer *)GetServer()->FindAndCheckCharExistByUID(pData->nPlayerUID);
	if (pPlayer)
	{
		::msgSendData(pPlayer->GetClientID(), 0, PROTO_LOGIN_UPDATE_CLIENT_ARMY_DATA, (char *)pBuffer, nLen, 0);
	}
}

void CMapServer::CB_MapUpdateCountryKSCWarListRealTime(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct plrDATA_WORLD_COUNTRY_WAR_LIST_REAL_TIME_SEND * pData;
	pData = (struct plrDATA_WORLD_COUNTRY_WAR_LIST_REAL_TIME_SEND *)pBuffer;

	memcpyT(&GetServer()->m_rtRankList, pBuffer, sizeof(struct plrDATA_WORLD_COUNTRY_WAR_LIST_REAL_TIME_SEND)*10);
}

void CMapServer::CB_MapUpdateCWarKListShowResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct plrDATA_WORLD_COUNTRY_WAR_LIST_SHOW * pData;
	pData = (struct plrDATA_WORLD_COUNTRY_WAR_LIST_SHOW *)pBuffer;

	memcpyT(&GetServer()->nCWarKListShow, pBuffer, sizeof(struct plrDATA_WORLD_COUNTRY_WAR_LIST_SHOW));

	// ?q????????a
	CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_KWAR_LIST_SHOW, (char *)&GetServer()->nCWarKListShow, sizeof(struct plrDATA_WORLD_COUNTRY_WAR_LIST_SHOW), 1);
}

 #endif

#endif

void CMapServer::CB_NPCTalkPackDataResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_NPCTALK_PACK_DATA * pData;
	CMapPlayer * pPlayer;
	long msg_id;
	struct proto_COMM nComm;
#ifdef CROSS_SERVER_SYSTEM
	CMapCharMsg *	pMsg;
	struct LOGIN_NPCTALK_PACK_DATA_KS * pDataKS;
	CLoginCheck *	pLoginCheck;
	unsigned long player_uid;
	long is_delete;
#endif

	pData = (struct LOGIN_NPCTALK_PACK_DATA *)pBuffer;
//CMapServer::GetServer()->Log("Μ NPC Pack data(%d,%d)", pData->player_uid, pData->nResult);
	//
#ifdef CROSS_SERVER_SYSTEM
	pDataKS = (struct LOGIN_NPCTALK_PACK_DATA_KS *)pData;
	//
	if (GetServer()->IsCrossServer())		// 阁狝ServerΜ
	{
		struct gameHISTORYBATTLE_SET_DATA * pHBSet;
		//
		long status, next_time, total;

 #if (!defined(USE_PACKAGE_DATA) || defined(CROSS_SERVER_SYSTEM_LOG))
		GetServer()->Log("DEBUG: Cross server: Μ癟(%d,%d)", pData->nType, pDataKS->nResult);
 #endif
		//
		switch(pData->nType)
		{
		case NPCTALK_PACK_TYPE_KS_REGISTER:		// d1=map, d2=team, d3=server id
			pDataKS->nType = NPCTALK_PACK_TYPE_KS_REGISTER_RESPONSE;
			//
 #ifndef USE_PACKAGE_DATA
			GetServer()->Log("DEBUG: Cross server: Μ爹璶―(org_plr_uid=%d,plr_uid=%d)(map=%d,%d)", pData->player_uid, pDataKS->nPlrSaveData.plrUID, pDataKS->nData1, pDataKS->nData2);
 #endif
			if (!GetServer()->IsConnectedToServer(GetServer()->m_hDBServer))
			{
				GetServer()->Log("DEBUG: Cross server: ?PDB Server?_?u!");
				goto err01;
			}
			//
			msg_id = CMapServer::GetServer()->m_nHistoryManager.KS_PlayerRegister(&pDataKS->nPlrSaveData, pDataKS->nData1, pDataKS->nData2);
			switch(msg_id)
			{
			case -1:	// ???????~
			default:
	err01:
				pDataKS->nResult = 0;
				break;
			case 0:		// ?w?g???W?L
				pDataKS->nResult = 2;
				// ?^??W?@?????W???@??(?u?n????a?? + ????ID?A?i?? gameGetHistoryTeamName ??o)
				pDataKS->nData2 = CMapServer::GetServer()->m_nHistoryManager.KS_GetPlayerRegisterTeamID(pDataKS->nPlrSaveData.plrUID, pDataKS->nData1);
				break;
			case 2:		// ?W?B?w??or??????W
				pDataKS->nResult = 3;
				break;
			case 3:		// ?j?????
				pDataKS->nResult = 4;
				break;
			case 1:		// ok
				if (!GetServer()->IsConnectedToServer(GetServer()->m_hDBServer))
				{
					GetServer()->Log("DEBUG: Cross server: ?PDB Server?_?u 2!");
					//
					CMapServer::GetServer()->m_nHistoryManager.KS_PlayerUnRegister(pDataKS->nPlrSaveData.plrUID, pDataKS->nPlrSaveData.plrLevel, pDataKS->nData1);
					goto err01;
				}
				// ?????^?????T??
				pDataKS->nData2 = CMapServer::GetServer()->m_nHistoryManager.KS_GetPlayerRegisterTeamID(pDataKS->nPlrSaveData.plrUID, pDataKS->nData1);
				// ?M?w???a?a??M??m(?w?s??????)
				pHBSet = gameGetHistoryBattleSetPtrByMap(pDataKS->nData1);
				if (!pHBSet)
				{
					GetServer()->Log("ERROR: Cross server: ⊿Τ驹初砞﹚(%d)!", pDataKS->nData1);
					goto err01;
				}
				//
				// Test02
			//	CMapServer::GetServer()->Log("KS: 代刚ミà︹ぃΘ!");
			//	CMapServer::GetServer()->m_nHistoryManager.KS_PlayerUnRegister(pDataKS->nPlrSaveData.plrUID, pDataKS->nPlrSaveData.plrLevel, pDataKS->nData1);
			//	goto err01;
				//
				pDataKS->nPlrSaveData.plrPosX = KS_REGISTER_WAIT_POSX;
				pDataKS->nPlrSaveData.plrPosY = KS_REGISTER_WAIT_POSY;
				pDataKS->nPlrSaveData.plrMapCode = pHBSet->hsdEnterMap;
				pDataKS->nPlrSaveData.plrSavePosX = pDataKS->nPlrSaveData.plrPosX;
				pDataKS->nPlrSaveData.plrSavePosY = pDataKS->nPlrSaveData.plrPosY;
				pDataKS->nPlrSaveData.plrSaveMapCode = pHBSet->hsdEnterMap;
				// ?M?w?}??
				pDataKS->nPlrSaveData.plrCountryID = ID_COUNTRY_NONE;
				// ???U???\????n??s??????DB Server
				pDataKS->nType = NPCTALK_PACK_TYPE_KS_REGISTER;
				GetServer()->SendData(GetServer()->m_hDBServer, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)pBuffer, nLen, 0);
				// pDataKS->nResult = 1;
				return;
				break;
			}
			break;
		case NPCTALK_PACK_TYPE_KS_REGISTER_DB_SAVE:		// DB ?s??^??
			pDataKS->nType = NPCTALK_PACK_TYPE_KS_REGISTER_RESPONSE;
			if (pDataKS->nResult != 1)
			{
				GetServer()->Log("ERROR: Cross server: Save data to DB Server fail (%d,%d)!", pDataKS->player_uid, pDataKS->nPlrSaveData.plrUID);
				//
				pDataKS->nResult = 0;
				CMapServer::GetServer()->m_nHistoryManager.KS_PlayerUnRegister(pDataKS->nPlrSaveData.plrUID, pDataKS->nPlrSaveData.plrLevel, pDataKS->nData1);
			}
 #ifndef USE_PACKAGE_DATA
			else
				GetServer()->Log("DEBUG: Cross server: ΜDB莱Θ(%d)(map=%d,%d)", pDataKS->player_uid, pDataKS->nData1, pDataKS->nData2);
 #endif
			GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)pBuffer, nLen, 0);
			return;
			break;
		// ......................
		case NPCTALK_PACK_TYPE_KS_CHECK_REGISTER:
			pDataKS->nType = NPCTALK_PACK_TYPE_KS_CHECK_REGISTER_RESULT;
			if (CMapServer::GetServer()->m_nHistoryManager.KS_GetPlayerRegisterHistoryInfo(pDataKS->nData4, pDataKS->nData1, &status, &next_time, &total))
			{
				pDataKS->nData2 = status;
				pDataKS->nData3 = next_time;
				pDataKS->nData4 = total;
				pDataKS->nResult = 1;
				//
 #ifndef USE_PACKAGE_DATA
				GetServer()->Log("DEBUG: Battle info:(%d,%d,%d)", status, next_time, total);
 #endif
			}
			else
				pDataKS->nResult = 2;
			break;
		// .....................
		case NPCTALK_PACK_TYPE_KS_ENTER:
			pDataKS->nType = NPCTALK_PACK_TYPE_KS_ENTER_RESULT;
			pDataKS->nResult = 0;
			if (CMapServer::GetServer()->m_nHistoryManager.KS_IsEnterHistoryBattleMap(pDataKS->nData4, pDataKS->nData1))	// 耞丁初
			{	// ????O?_???W
				//if (CMapServer::GetServer()->m_nHistoryManager.KS_GetPlayerRegisterHistoryInfo(pDataKS->nData4, pDataKS->nData1, &status, &next_time, &total))
				if (CMapServer::GetServer()->m_nHistoryManager.KS_GetPlayerRegisterTeamID(pDataKS->nData4, pDataKS->nData1))
				{
					player_uid = pDataKS->nData4;
					//
				//	pDataKS->nData2 = status;
				//	pDataKS->nData3 = next_time;
				//	pDataKS->nData4 = total;
					// ?M?w???a?a??M??m(?s????a??)
					pHBSet = gameGetHistoryBattleSetPtrByMap(pDataKS->nData1);
					if (!pHBSet)
					{
						GetServer()->Log("ERROR: Cross server: No battle setting(%d)!", pDataKS->nData1);
						goto err01;
					}
					pDataKS->nData1 = pHBSet->hsdEnterMap;
					//pDataKS->nData2 = port;	// ?o?? data2 ?w?g?Q Login ?]?w?? port
					pDataKS->nData3 = KS_REGISTER_WAIT_POSX;
					pDataKS->nData4 = KS_REGISTER_WAIT_POSY;
					// ?????n?J????
					if (!CMapServer::GetServer()->GetMapManager()->FindMap(pDataKS->nData1))
					{
						GetServer()->Log("ERROR: Cross server: Enter map not exist(%d)!", pDataKS->nData1);
						goto err01;
					}
					pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(player_uid, CMapPlayer::CLASS_ID, &is_delete);
					if ((pPlayer != NULL) || is_delete)
					{
						GetServer()->Log("ERROR: Cross server: Player already exist(%d)!", player_uid);
						goto err01;
					}
					// ?????n?J???a
					pDataKS->nLoginSerial = gameGetRandom();
					//
					if (GetServer()->m_LoginCheckMap.find(player_uid) != GetServer()->m_LoginCheckMap.end())
						GetServer()->m_LoginCheckMap.erase(player_uid);
					pLoginCheck = &(GetServer()->m_LoginCheckMap[player_uid]);
					pLoginCheck->m_nCheckCode	= pDataKS->nLoginSerial;
					pLoginCheck->m_nAccountID	= pDataKS->nAccountID;
					pLoginCheck->m_nCancelTime	= ::GetTickCount() + CLoginCheck::CANCEL_TIME + (10*1000);
					//pLoginCheck->m_nPrivilege	= 0;
					pLoginCheck->m_nPrivilege	= pDataKS->nUserData3;
					pLoginCheck->m_nMapFlags = 0;
					pLoginCheck->m_nIsFirstFromLogin = CHECK_LOGIN_FLAG_FIRST_LOGIN;
					pLoginCheck->m_nMapCode = pDataKS->nData1;
					// ???~??
					pLoginCheck->m_nBotCheckType = 0;
					pLoginCheck->m_nBotCheckTimes = 0;
					// ???I?g
				#ifdef USE_SPCMODE_CN_ADULT_LIMIT
					pLoginCheck->m_nOnlineTime = 0;
					pLoginCheck->m_nOfflineTime = 0;
					pLoginCheck->m_nLastLogoutTime = 0;
				#endif
					//
					pDataKS->nResult = 1;
				}
				else
					pDataKS->nResult = 2;
			}
 #ifndef USE_PACKAGE_DATA
			else
				GetServer()->Log("DEBUG: Cross server: ヘ玡ぃ初(pid=%d,battle=%d)", pDataKS->nData4, pDataKS->nData1);
 #endif
			break;
		default:
			//return;
			goto proc_next;
			break;
		}
		//
		//::msgSendData(nID, 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)pBuffer, nLen, 0);	// ?????o??A??H?O Login
		GetServer()->SendData( GetServer()->GetLoginServer(), 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)pBuffer, nLen, 0 );
		//
		return;
	}
proc_next:
	//
 #ifdef CROSS_SERVER_CWAR
	if (GetServer()->IsCrossCWarServer())		// ??A???Server????
	{
  #if (!defined(USE_PACKAGE_DATA) || defined(CROSS_SERVER_SYSTEM_LOG))
		GetServer()->Log("DEBUG: Cross server: Μ癟(%d,%d)", pData->nType, pDataKS->nResult);
  #endif
		//
		switch(pData->nType)
		{
		case NPCTALK_PACK_TYPE_CWAR_KS_ENTER:
			pDataKS->nType = NPCTALK_PACK_TYPE_CWAR_KS_ENTER_RESULT;
			pDataKS->nResult = 0;
			//
			player_uid = pDataKS->nUserData1;		// pDataKS->nData4;
			//
		//	pDataKS->nData2 = status;
		//	pDataKS->nData3 = next_time;
		//	pDataKS->nData4 = total;
			//
			//pDataKS->nData1 = map;
			//pDataKS->nData2 = port;	// ?o?? data2 ?w?g?Q Login ?]?w?? port
			//pDataKS->nData3 = posx;
			//pDataKS->nData4 = posy;
			// ?????n?J????
			if (!CMapServer::GetServer()->GetMapManager()->FindMap(pDataKS->nData1))
			{
				GetServer()->Log("ERROR: CWar cross server: Enter map not exist(%d)!", pDataKS->nData1);
				break;
			}
			pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(player_uid, CMapPlayer::CLASS_ID, &is_delete);
			if ((pPlayer != NULL) || is_delete)
			{
				GetServer()->Log("ERROR: CWar cross server: Player already exist(%d)!", player_uid);
				break;
			}
			// ?????n?J???a
			pDataKS->nLoginSerial = gameGetRandom();
			//
			if (GetServer()->m_LoginCheckMap.find(player_uid) != GetServer()->m_LoginCheckMap.end())
				GetServer()->m_LoginCheckMap.erase(player_uid);
			pLoginCheck = &(GetServer()->m_LoginCheckMap[player_uid]);
			pLoginCheck->m_nCheckCode	= pDataKS->nLoginSerial;
			pLoginCheck->m_nAccountID	= pDataKS->nAccountID;
			pLoginCheck->m_nCancelTime	= ::GetTickCount() + CLoginCheck::CANCEL_TIME + (10*1000);
			//pLoginCheck->m_nPrivilege	= 0;
			pLoginCheck->m_nPrivilege	= pDataKS->nUserData3;
			pLoginCheck->m_nMapFlags = 0;
			pLoginCheck->m_nIsFirstFromLogin = CHECK_LOGIN_FLAG_FIRST_LOGIN;
			pLoginCheck->m_nMapCode = pDataKS->nData1;
			// ???~??
			pLoginCheck->m_nBotCheckType = 0;
			pLoginCheck->m_nBotCheckTimes = 0;
			// ???I?g
		#ifdef USE_SPCMODE_CN_ADULT_LIMIT
			pLoginCheck->m_nOnlineTime = 0;
			pLoginCheck->m_nOfflineTime = 0;
			pLoginCheck->m_nLastLogoutTime = 0;
		#endif
			//
			pDataKS->nResult = 1;
			break;

		default:
			//return;
			goto proc_old;
			break;
		}
		//
		GetServer()->SendData( GetServer()->GetLoginServer(), 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)pBuffer, nLen, 0 );
		return;
	}
proc_old:
 #endif
	//
#endif
	// ?q KS ?^?k
#ifdef CROSS_SERVER_SYSTEM
	switch(pData->nType)
	{
	case NPCTALK_PACK_TYPE_KS_RETURN:		// 狝竟Map Server: 箇非称硄
		pDataKS->nType = NPCTALK_PACK_TYPE_KS_RETURN_RESULT;
		pDataKS->nResult = 0;
		//
		player_uid = pDataKS->nUserData1;
		//
 #if (!defined(USE_PACKAGE_DATA) || defined(CROSS_SERVER_SYSTEM_LOG))
		GetServer()->Log("DEBUG: ???a?n?qKS?^?k:(%d,%s:%d)", player_uid, pDataKS->szMapIP, pDataKS->nData2);
 #endif
		pDataKS->nData1 = pDataKS->nData1;		// map id
		//pDataKS->nData2 = port;	// ?o?? data2 ?w?g?Q Login ?]?w?? port
		//pDataKS->nData3 = ;		// ?o?? data3 ?w?g?Q Login ?]?w
		//pDataKS->nData4 = ;		// ?o?? data4 ?w?g?Q Login ?]?w
		// ?????n?J????
		if (!CMapServer::GetServer()->GetMapManager()->FindMap(pDataKS->nData1))
		{
			GetServer()->Log("ERROR: Cross Server Return: Enter map not exist(%d)!", pDataKS->nData1);
err02:
			GetServer()->SendData( GetServer()->GetLoginServer(), 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)pBuffer, nLen, 0 );
			return;
		}
		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(player_uid, CMapPlayer::CLASS_ID, &is_delete);
		if ((pPlayer != NULL) || is_delete)
		{
			GetServer()->Log("ERROR: Cross Server Return: Player already exist(%d)!", player_uid);
			goto err02;
		}
		// ?????n?J???a
		pDataKS->nLoginSerial = gameGetRandom();
		//
		if (GetServer()->m_LoginCheckMap.find(player_uid) != GetServer()->m_LoginCheckMap.end())
			GetServer()->m_LoginCheckMap.erase(player_uid);
		pLoginCheck = &(GetServer()->m_LoginCheckMap[player_uid]);
		pLoginCheck->m_nCheckCode	= pDataKS->nLoginSerial;
		pLoginCheck->m_nAccountID	= pDataKS->nAccountID;
		pLoginCheck->m_nCancelTime	= ::GetTickCount() + CLoginCheck::CANCEL_TIME + (10*1000);
		//pLoginCheck->m_nPrivilege	= 0;
		pLoginCheck->m_nPrivilege	= pDataKS->nUserData3;
		pLoginCheck->m_nMapFlags = 0;				// ???
		pLoginCheck->m_nIsFirstFromLogin = CHECK_LOGIN_FLAG_FIRST_LOGIN;
		pLoginCheck->m_nMapCode = pDataKS->nData1;
		// ???~??
		pLoginCheck->m_nBotCheckType = 0;			// ???
		pLoginCheck->m_nBotCheckTimes = 0;			// ???
		// ???I?g
 #ifdef USE_SPCMODE_CN_ADULT_LIMIT
		pLoginCheck->m_nOnlineTime = 0;				// ???
		pLoginCheck->m_nOfflineTime = 0;			// ???
		pLoginCheck->m_nLastLogoutTime = 0;			// ???
 #endif
		//
		pDataKS->nResult = 1;
		GetServer()->SendData( GetServer()->GetLoginServer(), 0, PROTO_LOGIN_NPCTALK_PACK_DATA, (char *)pBuffer, nLen, 0 );
		return;
		break;
	case NPCTALK_PACK_TYPE_KS_RETURN_RESULT:	// 阁狝竟Map Server: 非称ЧΘ
		player_uid = pDataKS->nUserData1;
		//
 #if (!defined(USE_PACKAGE_DATA) || defined(CROSS_SERVER_SYSTEM_LOG))
		GetServer()->Log("DEBUG: 硄產耴狝竟(%d,%d) !!", pData->player_uid, player_uid);
 #endif
		//GetServer()->Log("DEBUG: Return(map_id=%d,ip=%s:%d)(pos=%d,%d)(serial=%08x)", pDataKS->nData1, pDataKS->szMapIP, pDataKS->nData2, pDataKS->nData3, pDataKS->nData4, pDataKS->nLoginSerial);
		//GetServer()->Log("DEBUG: Return(player_uid=%d,privilege=%d)", pDataKS->nUserData1, pDataKS->nUserData3);
		//
		pPlayer = (CMapPlayer *)GetServer()->FindAndCheckCharExistByUID(pData->player_uid);
		if (pPlayer)
		{
			GetServer()->KS_ReturnToOriginServer(pPlayer, player_uid, pDataKS->nData1, pDataKS->nData3, pDataKS->nData4, pDataKS->szMapIP, pDataKS->nData2, pDataKS->nLoginSerial);
		}
		else
			CMapServer::GetServer()->Log("ERROR: Cross server return but player not exist!(%d)", player_uid);
		return;
		break;

	//?i?J??A???a??
	case NPCTALK_PACK_TYPE_CWAR_KS_ENTERCWARMAP:
		struct CWAR_KS_WORLD_INIT_DATA_MAP *CWarMapData;
		long PosX,PosY;
		player_uid = pDataKS->player_uid;
		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(player_uid, CMapPlayer::CLASS_ID, &is_delete);
		if ( !pPlayer ) return; 
		switch(pDataKS->nResult)		// 0=error???????~, 1=ok, 2=???????
		{
			case 0:
			default:		//?X?{???????~
				msg_id = pPlayer->m_WaitKS_TalkID_InnerError;
				break;
			case 1:	
				if ((pPlayer != NULL) || is_delete)
				{
					msg_id = pPlayer->m_WaitVT_TalkID_Ok;
					CWarMapData = CWar_KS_GetWorldSetting_Map(pPlayer->GetSaveData()->plrEnterStageID);//眔墩戈
					if(!CWarMapData) break;
					if(!CWarMapData->nPlayerMap)//璝礚戈ぃ肚癳 
					{
						msg_id = pPlayer->m_WaitKS_TalkID_InnerError;
						break;
					}

					pPlayer->GetSaveData()->plrCountryID = pPlayer->GetSaveData()->plrEnterStageID + ID_COUNTRY_WEI;//砞狝竟墩
					pPlayer->GetSaveData()->plrEnterStageID = 0;

					//???o??O??????e??m
					CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SetPlayerDueTime(pPlayer->GetMapCode(), pPlayer->GetUID());
					PosX = gameGetRandomRange(CWarMapData->nPlayerPos1_X1, CWarMapData->nPlayerPos1_X2+1);
					PosY = gameGetRandomRange(CWarMapData->nPlayerPos1_Y1, CWarMapData->nPlayerPos1_Y2+1);
					//?]?w???????I
					pPlayer->GetSaveData()->plrSaveMapCode = (unsigned short)CWarMapData->nPlayerMap;
					pPlayer->GetSaveData()->plrSavePosX = PosX;
					pPlayer->GetSaveData()->plrSavePosY = PosY;
					pPlayer->GetSaveData()->plrCWar_Honor = (unsigned short)pDataKS->nUserData2; //眖逼︽篯弄莉眔颈
					pPlayer->GetSaveData()->plrCWar_Duedate = CMapServer::GetServer()->GetLoopTime() + 24*60*60;//砞Τ丁磷颈砆睲埃
					// ?q?? Client?\??
					if (CMapServer::GetServer()->IsCrossServer(1))		// ?O????A??Server
					{
						pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
						
						pMsg->m_Msg.m_UpdateData2Msg.Reset();
						pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_CWAR_HONOR, 0, pPlayer->GetSaveData()->plrCWar_Honor, 0);

						pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					}
					pPlayer->m_WaitKS_TalkID_InnerError = 0;
					pPlayer->m_WaitKS_TalkID_AlreadyReg = 0;
					pPlayer->m_WaitKS_TalkID_Full = 0;
					pPlayer->m_WaitKS_TalkID_NoBalance = 0;
					pPlayer->m_WaitVT_TalkID_Ok = 0;
					//
					pPlayer->m_WaitVT_Timeout = 0;
					pPlayer->m_WaitVT_State = 0;

					struct MAP_TALK_TO_NPC_NEXT_STEP nEmul;

					pPlayer->m_nNPCSelectID[0] = msg_id;
					//
					memset(&nComm, 0, sizeof(nComm));
					nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
					nEmul.nSelectID = msg_id;
					nEmul.nSerial = pPlayer->m_nNPCTalkSerial;
					GetServer()->CB_NPCNextStep((char *)&nEmul, sizeof(nEmul), pPlayer->GetClientID(), &nComm);


					pPlayer->ChangeMap(CWarMapData->nPlayerMap, PosX, PosY);
					return;

				}
				pDataKS->nResult = 1;
				break;
			case 2:	//???????
				msg_id = pPlayer->m_WaitKS_TalkID_NoBalance;
				break;
			
		}
		goto clear;
		break;

	
		//?i?J??A?Z??a??
	case NPCTALK_PACK_TYPE_SWAR_KS_ENTERSWARMAP:
		player_uid = pDataKS->player_uid;
		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(player_uid, CMapPlayer::CLASS_ID, &is_delete);
		if ( !pPlayer ) return; 
		switch(pDataKS->nResult)		// 0=error???????~, 1=ok, 2=???????
		{
			case 0:
			default:		//?X?{???????~
				msg_id = pPlayer->m_WaitKS_TalkID_InnerError;
				break;
			case 1:	
				if ((pPlayer != NULL) || is_delete)
				{
#ifdef CROSS_SOUL_WAR_FUNC_FIX
					if (pDataKS->nUserData2 && pDataKS->nData4)//珇IDの计秖Τ砞﹚Ι珇
						if(!pPlayer->DelItemAndUpdateClient(pDataKS->nUserData2, pDataKS->nData4, LOGTYPE_ITEM_FROM_NPC_LOSE, NULL))
						{
							msg_id = pPlayer->m_WaitVT_TalkID_Error;
							break;
						}
#endif
					msg_id = pPlayer->m_WaitVT_TalkID_Ok;
					//?]?w?????I
					pPlayer->GetSaveData()->plrSaveMapCode = (unsigned short)pDataKS->nData1;
					pPlayer->GetSaveData()->plrSavePosX = pDataKS->nData2;
					pPlayer->GetSaveData()->plrSavePosY = pDataKS->nData3;
					pPlayer->m_WaitKS_TalkID_InnerError = 0;
					pPlayer->m_WaitKS_TalkID_AlreadyReg = 0;
					pPlayer->m_WaitKS_TalkID_Full = 0;
					pPlayer->m_WaitKS_TalkID_NoBalance = 0;
					pPlayer->m_WaitVT_TalkID_Ok = 0;
					//
					pPlayer->m_WaitVT_Timeout = 0;
					pPlayer->m_WaitVT_State = 0;

					struct MAP_TALK_TO_NPC_NEXT_STEP nEmul;

					pPlayer->m_nNPCSelectID[0] = msg_id;
					//
					memset(&nComm, 0, sizeof(nComm));
					nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
					nEmul.nSelectID = msg_id;
					nEmul.nSerial = pPlayer->m_nNPCTalkSerial;
					GetServer()->CB_NPCNextStep((char *)&nEmul, sizeof(nEmul), pPlayer->GetClientID(), &nComm);
					pPlayer->ChangeMap(pDataKS->nData1, pDataKS->nData2, pDataKS->nData3);
					return;

				}
				pDataKS->nResult = 1;
				break;
			case 2:	//???????
				msg_id = pPlayer->m_WaitKS_TalkID_NoBalance;
				break;
			case 3: //?w??[?L
				msg_id = pPlayer->m_WaitKS_TalkID_Full;
				break;
			
		}
		goto clear;
		break;
	}
#endif

	// ????
	pPlayer = (CMapPlayer *)GetServer()->FindAndCheckCharExistByUID(pData->player_uid);
	if (pPlayer)
	{
		if (pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_STATE)
		{
			if (!pPlayer->IsReady())
				return;
			if (!pPlayer->IsAvailable())
				return;
			if (!pPlayer->IsNPCTalk() || !(pPlayer->m_WaitVT_State & PLAYER_NPCTALK_WAIT_DATA))
			{
				// ?????G?A?????a???b??????A?A???????G
				CMapServer::GetServer()->Log("WARNING: %s Get talk pack data but not talking", pPlayer->GetSaveData()->plrName);
				return;
			}
			//
			msg_id = 0;
#ifdef CROSS_SERVER_SYSTEM
			//
 #if (!defined(USE_PACKAGE_DATA) || defined(CROSS_SERVER_SYSTEM_LOG))
			GetServer()->Log("DEBUG: Cross server: ΜNPC Talk莱(%s)(type=%d,result=%d)", pPlayer->GetName(), pData->nType, pData->nResult);
 #endif
			//
			switch(pData->nType)
			{
			case NPCTALK_PACK_TYPE_KS_REGISTER_RESPONSE:
 #ifdef CROSS_SERVER_CWAR
			case NPCTALK_PACK_TYPE_CWAR_KS_REGISTER_CONFIRM_RESULT:
 #endif
				switch(pData->nResult)		// 0=error???????~, 1=ok, 2=?w?g???W, 3=?W?B?w??or??????W, 4=?j???????????????
				{
				case 0:
				default:
					msg_id = pPlayer->m_WaitKS_TalkID_InnerError;
					break;
				case 1:
					msg_id = pPlayer->m_WaitVT_TalkID_Ok;
					break;
				case 2:
					msg_id = pPlayer->m_WaitKS_TalkID_AlreadyReg;
					break;
				case 3:
					msg_id = pPlayer->m_WaitKS_TalkID_Full;
					break;
				case 4:
					msg_id = pPlayer->m_WaitKS_TalkID_NoBalance;
					break;
				}
				// ?M??????
	clear:
				pPlayer->m_WaitKS_TalkID_InnerError = 0;
				pPlayer->m_WaitKS_TalkID_AlreadyReg = 0;
				pPlayer->m_WaitKS_TalkID_Full = 0;
				pPlayer->m_WaitKS_TalkID_NoBalance = 0;
				pPlayer->m_WaitVT_TalkID_Ok = 0;
				//
				pPlayer->m_WaitVT_Timeout = 0;
				pPlayer->m_WaitVT_State = 0;
				break;
			case NPCTALK_PACK_TYPE_KS_CHECK_REGISTER_RESULT:
 #ifdef CROSS_SERVER_CWAR
			case NPCTALK_PACK_TYPE_CWAR_KS_CHECK_REGISTER_RESULT:
 #endif
				switch(pData->nResult)
				{
				case 0:
				default:
					msg_id = pPlayer->m_WaitKS_TalkID_InnerError;
					break;
				case 1:
					msg_id = pPlayer->m_WaitVT_TalkID_Ok;
					//
					pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
					
					pMsg->m_Msg.m_UpdateData2Msg.Reset();
					pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_HISTORY_INFO, (short)pData->nData2, pData->nData3, pData->nData4);

					pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					break;
				case 2:
					msg_id = pPlayer->m_WaitKS_TalkID_NoReg;
					break;
				}
				goto clear;
				break;
			case NPCTALK_PACK_TYPE_KS_ENTER_RESULT:
 #ifdef CROSS_SERVER_CWAR
			case NPCTALK_PACK_TYPE_CWAR_KS_ENTER_RESULT:
 #endif
				switch(pData->nResult)
				{
				case 0:
				default:
					msg_id = pPlayer->m_WaitKS_TalkID_InnerError;
					break;
				case 1:
					msg_id = pPlayer->m_WaitVT_TalkID_Ok;
					//
				//	pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
				//	pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
				//	pMsg->m_nSize	= pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				//	pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_HISTORY_INFO;
				//	//
				//	pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nVal = pData->nData2;
				//	pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = pData->nData3;
				//	pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData2 = pData->nData4;
					// ???n?J??PK???A??
					pDataKS = (struct LOGIN_NPCTALK_PACK_DATA_KS *)pData;
					if (nLen < pDataKS->GetSmartSize())
					{
						CMapServer::GetServer()->Log("ERROR: KS data size incorrect !");
						msg_id = pPlayer->m_WaitKS_TalkID_InnerError;
					}
					else
					{
						// ??? LOGIN_ENTER_MAP_RESULT ?@?k
						// nData2 = port
						// nLoginSerial = ?y????
 #if (!defined(USE_PACKAGE_DATA) || defined(CROSS_SERVER_SYSTEM_LOG))
						CMapServer::GetServer()->Log("DEBUG: Goto KS server:(%d,%d)(map=%d,%d,%d)(%s:%d,serial=%d) !", pPlayer->GetUID(), pDataKS->nUserData1, pDataKS->nData1, pDataKS->nData3, pDataKS->nData4, pDataKS->szMapIP, pDataKS->nData2, pDataKS->nLoginSerial);
 #endif

						CMapServer::GetServer()->KS_ConnectToCrossServer(pPlayer, pDataKS->nUserData1, pDataKS->nData1, pDataKS->nData3, pDataKS->nData4, pDataKS->szMapIP, pDataKS->nData2, pDataKS->nLoginSerial);
					}
					break;
				case 2:
					msg_id = pPlayer->m_WaitKS_TalkID_NoReg;
					break;
				case 3:
					msg_id = pPlayer->m_WaitKS_TalkID_Full;
					break;
				}
				goto clear;
				break;
 #ifdef CROSS_SERVER_CWAR
			case NPCTALK_PACK_TYPE_CWAR_KS_REGISTER_RESULT:
				switch(pData->nResult)		// 0=error???????~, 1=ok?A?n?~??T?{???W, 2=?x????|?????W?A??O????H??w???A???O?_?n?~????W, 3=?w?g???W, 4=?W?B?w??
				{
				case 0:
				default:
					msg_id = pPlayer->m_WaitKS_TalkID_InnerError;
					break;
				case 1:
					msg_id = pPlayer->m_WaitVT_TalkID_Ok;
					break;
				case 2:
					msg_id = pPlayer->m_WaitKS_TalkID_ArmyNoRegister;
					break;
				case 3:
					msg_id = pPlayer->m_WaitKS_TalkID_AlreadyReg;
					break;
				case 4:
					msg_id = pPlayer->m_WaitKS_TalkID_Full;
					break;
				}
				goto clear;
				break;


 #endif
			default:
				switch(pData->nResult)
				{
				case 0:
					msg_id = pPlayer->m_WaitVT_TalkID_NoData;
					break;
				case 1:
					msg_id = pPlayer->m_WaitVT_TalkID_Ok;
					break;
				case 2:
					msg_id = pPlayer->m_WaitVT_TalkID_Error;
					break;
				}
				// ?M??????
				pPlayer->m_WaitVT_Timeout = 0;
				pPlayer->m_WaitVT_TalkID_NoData = 0;
				pPlayer->m_WaitVT_TalkID_Error = 0;
				pPlayer->m_WaitVT_TalkID_Ok = 0;
				pPlayer->m_WaitVT_State = 0;
				break;
			}
#else
			switch(pData->nResult)
			{
			case 0:
				msg_id = pPlayer->m_WaitVT_TalkID_NoData;
				break;
			case 1:
				msg_id = pPlayer->m_WaitVT_TalkID_Ok;
				break;
			case 2:
				msg_id = pPlayer->m_WaitVT_TalkID_Error;
				break;
			}
			// ?M??????
			pPlayer->m_WaitVT_Timeout = 0;
			pPlayer->m_WaitVT_TalkID_NoData = 0;
			pPlayer->m_WaitVT_TalkID_Error = 0;
			pPlayer->m_WaitVT_TalkID_Ok = 0;
			pPlayer->m_WaitVT_State = 0;
#endif
			//
			if (!msg_id)
			{
				pPlayer->ExitNPCTalk();
				//
				memset(&nComm, 0, sizeof(nComm));
				nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
				pPlayer->SendNPCTalkResult(&nComm, 0, 0, 0);
				return;
			}
			//
			// ???? NPC ?????U?@?B
			//
//CMapServer::GetServer()->Log("Test: %s Get npc talk pack data(%d, %d)", pPlayer->GetSaveData()->plrName, pPlayer->IsNPCTalk(), msg_id);
			struct MAP_TALK_TO_NPC_NEXT_STEP nEmul;

			pPlayer->m_nNPCSelectID[0] = msg_id;
			//
			memset(&nComm, 0, sizeof(nComm));
			nComm.pcUID = pPlayer->m_Wait_PCOMM_UID;
			nEmul.nSelectID = msg_id;
			nEmul.nSerial = pPlayer->m_nNPCTalkSerial;
			GetServer()->CB_NPCNextStep((char *)&nEmul, sizeof(nEmul), pPlayer->GetClientID(), &nComm);
		}
	}
}

// ?? Client ??n?D?d??u?W?H??
void CMapServer::CB_AskClientNumber(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_ASK_CLIENT_NUMBER * pData;
	CMapPlayer * pPlayer;

	pData = (struct LOGIN_ASK_CLIENT_NUMBER *)pBuffer;
	if (nLen != sizeof(struct LOGIN_ASK_CLIENT_NUMBER))
		return;
	pPlayer = GetServer()->FindPlayerByClientID(nID);
	if (pPlayer != NULL)
	{
		// ?u?? GM ????
		if (pPlayer->GetCharData()->plrSPCFlag & (spcFLAG_GM | spcFLAG_GM2))
		//if (pPlayer->GetCharData()->plrSPCFlag & spcFLAG_GM)
		{
			pData->nUID = pPlayer->GetUID();
			GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_ASK_CLIENT_NUMBER, (char *)pData, nLen, 0);
		}
	}
}

// ?? Login Server ???d??u?W?H????G
void CMapServer::CB_AskClientNumberServerResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_ASK_CLIENT_NUMBER_RESULT * pData;
	CMapPlayer * pPlayer;

	pData = (struct MAP_ASK_CLIENT_NUMBER_RESULT *)pBuffer;
	//pPlayer = GetServer()->FindPlayerByClientID(pData->nUID);
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->nUID);
	if (pPlayer != NULL)
	{
		if (pPlayer->IsKindOf(CMapPlayer::CLASS_ID))
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_ASK_CLIENT_NUMBER_RESULT, (char *)pData, nLen, 0);
	}
}

struct MAP_SET_HISTORY_BATTLE * CMapServer::GetHistorySetTimeData(long id)
{
	if ((id > 0) && (id <= gameMAX_HISTORYBATTLE_SETTING))
		return(&m_nHistorySetData[id-1]);
	return(NULL);
}
// ================================================================
// ?O?d???a???n?T??2
// ================================================================
void CMapServer::pfcAdd(unsigned long uid, struct KEEP_CHANGE_FORCE * pData)
{
	m_nPlayerForceChange[uid] = *pData;
	m_nKeepChangeForceTime = GetLoopTime() + 60*2;		// 2 だ牧
}

void CMapServer::pfcUpdateData(unsigned long uid)
{
	CMapPlayer * pPlayer;
	struct KEEP_CHANGE_FORCE * pData;

	if (m_nPlayerForceChange.find(uid) != m_nPlayerForceChange.end())
	{
		pPlayer = (CMapPlayer *)FindObjectByUID(uid, CMapPlayer::CLASS_ID);
		if (pPlayer)
		{
			pData = &m_nPlayerForceChange[uid];
			if (pPlayer->GetSaveData()->plrCountryID == pData->m_nOldCountryID)
			{
				if (!pData->m_nType)
				{
					if (!pPlayer->GetSaveData()->plrOrganizeUID)
						goto do_it;
				}
				else
				{
	do_it:		//	pPlayer->GetSaveData()->plrCountryID = pData->m_nCountryID;
					// ?]?t?s??
					pPlayer->ChangeCountryAndUpdateClient((unsigned char)pData->m_nCountryID, 0, 2);
				}
			}
		}
		//
		pfcClearData(uid);
	}
}

// 0 = ?M??????
void CMapServer::pfcClearData(unsigned long uid)
{
	if (uid)
	{
		if (m_nPlayerForceChange.find(uid) != m_nPlayerForceChange.end())
			m_nPlayerForceChange.erase(uid);
	}
	else
		m_nPlayerForceChange.clear();
}

void CMapServer::pfcLoopProcess()
{
	if (m_nKeepChangeForceTime)
	{
		if (GetLoopTime() >= m_nKeepChangeForceTime)
		{
			m_nKeepChangeForceTime = 0;
			pfcClearData(0);
		}
	}
}

void CMapServer::PollCombatPowerRankTitleFromLogin(void)
{
	long now;
	CMapPlayer *pl;
	std::map<int, CMapPlayer *>::iterator it;
	struct MAP_GET_COMBAT_POWER_RANK_REQUEST req;

	now = GetLoopTime();
	/* ?????????????????????????????????????? */
	if (m_nLastCombatPowerRankTitlePoll != 0 && (now - m_nLastCombatPowerRankTitlePoll) < 5)
		return;
	if (!IsConnectedToServer(m_hLoginServer))
		return;
	pl = NULL;
	for (it = GetClientPlayerMap().begin(); it != GetClientPlayerMap().end(); ++it)
	{
		if (it->second && it->second->GetCharData() &&
			IsPlayerUID(it->second->GetCharData()->plrSaveData.plrUID))
		{
			pl = it->second;
			break;
		}
	}
	if (!pl)
		return;
	m_nLastCombatPowerRankTitlePoll = now;
	pl->m_nLastCPRankKindReq = 0;
	memset(&req, 0, sizeof(req));
	req.playerUID = pl->GetUID();
	req.rankKind = 0;
	m_cpRankSilentPollUIDs.insert(pl->GetUID());
	SendData(m_hLoginServer, pl->GetUID(), PROTO_LOGIN_GET_COMBAT_POWER_RANK, (char *)&req, sizeof(req), 0);
}
// ================================================================
// ?O?d???a???n?T??
// ================================================================
struct KEEP_IMPORTANT_MSG_DATA * CMapServer::kimGetData(unsigned long uid)
{
	struct KEEP_IMPORTANT_MSG_DATA * pData = NULL;
	//
	std::map<unsigned long, struct KEEP_IMPORTANT_MSG_DATA>::iterator iter;
	std::map<unsigned long, struct KEEP_IMPORTANT_MSG_DATA>::iterator iter_end;

	iter = m_nKeepImportantData.find(uid);
	iter_end = m_nKeepImportantData.end();
	if (iter != iter_end)
	{
		pData = &iter->second;
	}
	return(pData);
}

void CMapServer::kimKeepData(unsigned long uid, long protoco, char * buffer, long size)
{
	struct KEEP_IMPORTANT_MSG_DATA * pData;
	struct KEEP_IMPORTANT_MSG_DATA nData;
	//
	struct KEEP_IMPORTANT_MSG_DATA2 * pMsg;

	// ???b????n?J????O??
	if (m_LoginCheckMap.find(uid) == m_LoginCheckMap.end())
		return;
	//
	pMsg = (struct KEEP_IMPORTANT_MSG_DATA2 *)apiAllocateMemory(sizeof(struct KEEP_IMPORTANT_MSG_DATA2) + size);
	if (!pMsg)
		return;
	//
	pData = kimGetData(uid);
	if (!pData)
	{
		memset(&nData, 0, sizeof(nData));
		nData.nUID = uid;
		//
		nData.nLink = pMsg;				// ??@??
		nData.nLink_Last = pMsg;		// 簿笆程魁
		m_nKeepImportantData[uid] = nData;
	}
	else
	{
		pData->nLink_Last->nLink = pMsg;
		pData->nLink_Last = pMsg;
	}
	pMsg->nProtoco = (unsigned short)protoco;
	pMsg->nSize = (unsigned short)size;
	pMsg->nBuffer = ((char *)pMsg) + sizeof(struct KEEP_IMPORTANT_MSG_DATA2);
	memcpyT(pMsg->nBuffer, buffer, size);
	pMsg->nLink = NULL;
}

// ?e?T?????w??n?J??
// ?q?`?O???W??
// map_code = 0 ?????A?_?h???w??X???a??
void CMapServer::kimKeepDataToCheckLogin(long map_code, long protoco, char * buffer, long size)
{
	unsigned long uid;
	CLoginCheck * pLoginCheck;
	std::map<unsigned long, CLoginCheck>::iterator	li;

	for(li = m_LoginCheckMap.begin(); li != m_LoginCheckMap.end(); li++)
	{
		uid = li->first;
		if (uid)
		{
			if (map_code)
			{
				pLoginCheck = &li->second;
				if (pLoginCheck->m_nMapCode == map_code)
					kimKeepData(uid, protoco, buffer, size);
			}
			else
				kimKeepData(uid, protoco, buffer, size);
		}
	}
}

// uid = 0 ?M??????
void CMapServer::kimClear(unsigned long uid)
{
	struct KEEP_IMPORTANT_MSG_DATA * pData;
	struct KEEP_IMPORTANT_MSG_DATA2 * pMsg;
	char * p;
	//
	std::map<unsigned long, struct KEEP_IMPORTANT_MSG_DATA>::iterator iter;
	std::map<unsigned long, struct KEEP_IMPORTANT_MSG_DATA>::iterator iter_end;

	if (uid)
	{
		pData = kimGetData(uid);
		if (pData)
		{
			pMsg = pData->nLink;
			while(pMsg)
			{
				p = (char *)pMsg;
				pMsg = pMsg->nLink;
				apiFreeMemory(p);
			}
			m_nKeepImportantData.erase(uid);
		}
	}
	else
	{
		iter = m_nKeepImportantData.begin();
		iter_end = m_nKeepImportantData.end();
		if (iter != iter_end)
		{
			for (; iter != iter_end; iter++)
			{
				pData = &iter->second;
				if (pData)
				{
					pMsg = pData->nLink;
					while(pMsg)
					{
						p = (char *)pMsg;
						pMsg = pMsg->nLink;
						apiFreeMemory(p);
					}
				}
			}
			m_nKeepImportantData.clear();
		}
	}
}

// ?e????|???M??
void CMapServer::kimSendData(CMapPlayer * pPlayer, unsigned long uid, long nClientID)
{
	struct KEEP_IMPORTANT_MSG_DATA * pData;
	struct KEEP_IMPORTANT_MSG_DATA2 * pMsg;
	//
	struct ArmyMemberDataNotify * p;
	struct tm * ptm;
	struct tm ntm;
	long i;
	//
	struct MAP_TRACE_PLAYER_SERVER_RESULT * pTraceData;

	if (uid && pPlayer)
	{
		pData = kimGetData(uid);
		if (pData)
		{
			pMsg = pData->nLink;
			while(pMsg)
			{
				// ?@?? Message ??n?S?O?B?z??
				switch(pMsg->nProtoco)
				{
				case PROTO_MAP_ARMY_MEMBER_JOIN_NOTIFY:
					p = (struct ArmyMemberDataNotify *)pMsg->nBuffer;
					if (p->member.uid == p->playerUID)
					{
						// ?j?????
						ptm = ::apiGetTimeStruct(0);
						memcpyT(&ntm, ptm, sizeof(ntm));
						ntm.tm_hour = 0;
						ntm.tm_min = 0;
						ntm.tm_sec = 0;
						i = (long)mktime(&ntm);
						i = i + (60 * 60 * 24);
						pPlayer->GetSaveData()->plrTime_OfficeGift = i;
						pPlayer->AutoSaveCharData();
					}
					break;
				case PROTO_MAP_TRACE_PLAYER_SERVER_RESULT:
					pTraceData = (struct MAP_TRACE_PLAYER_SERVER_RESULT *)pMsg->nBuffer;
					// ?]?w??????A?]?w??????
					::smsSetMessageMonitorCallBack_Recv(InnerCB_MonitorPlayer_Recv);
					::smsSetMessageMonitorCallBack_Send(InnerCB_MonitorPlayer_Send);
					// ???h?_?u??|???M??
					::smsAddMessageMonitor(nClientID, pTraceData->MonitorUID);
					goto do_next;
					break;
				}
				//
				::msgSendData( nClientID, 0, pMsg->nProtoco, (char *)pMsg->nBuffer, pMsg->nSize, 0 );
do_next:
				pMsg = pMsg->nLink;
			}
			kimClear(uid);
		}
	}
}

void CMapServer::kimSetFlagToCheckLogin(long map_code, long flag)
{
	unsigned long uid;
	CLoginCheck * pLoginCheck;
	std::map<unsigned long, CLoginCheck>::iterator	li;

	for(li = m_LoginCheckMap.begin(); li != m_LoginCheckMap.end(); li++)
	{
		uid = li->first;
		if (uid)
		{
			pLoginCheck = &li->second;
			if (map_code)
			{
				if (pLoginCheck->m_nMapCode == map_code)
					pLoginCheck->m_nIsFirstFromLogin |= flag;
			}
			else
				pLoginCheck->m_nIsFirstFromLogin |= flag;
		}
	}
}

/*
void CMapServer::kimLoop()
{
	static unsigned long time_kim_tick = 0;
	//
	long i;
	struct KEEP_IMPORTANT_MSG_DATA * pData;
	std::map<unsigned long, struct KEEP_IMPORTANT_MSG_DATA>::iterator iter;
	std::map<unsigned long, struct KEEP_IMPORTANT_MSG_DATA>::iterator iter_end;
	std::vector<unsigned long> RemoveList;

	time_kim_tick += m_nLoopTickCount;
	if (time_kim_tick >= 20000)
	{
		time_kim_tick = 0;
		//
		RemoveList.clear();
		iter = m_nKeepImportantData.begin();
		iter_end = m_nKeepImportantData.end();
		while(iter != iter_end)
		{
			pData = &iter->second;
			if (m_nLoopTime >= pData->nKeep_Time)
				RemoveList.push_back(iter->first);
			iter++;
		}
		for (i=0; i<RemoveList.size(); i++)
			kimClear(RemoveList[i]);
	}
}
*/
// ================================================================
// ????
// ================================================================
// ????NPC: ??n?P?_?O?_?O???a??
long CMapServer::IsNPCWar(long map_code)
{
	if (IsWar())
	{
		//if (IsMapWar(map_code, 1))
		if (IsMapWar(map_code))			// ???? ???]?t?T??
			return(1);
	}
	return(0);
}
// ?]?w???w??X??(?????e1???????e)
void CMapServer::SetWarPrepStatus(long status)
{
	m_IsWarPrep  = status;
}

// ?]?w??????
long CMapServer::SetWarStatus(long status, long broadcast)
{
	CMapPlayer * pPlayer;
	CMapCharMsg * pMsg;
	//
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;

	if (m_IsWar != status)
	{
		m_IsWar = status;

#ifdef NEW_CROSS_SERVER_CWAR
		if(CMapServer::GetServer()->IsCrossCWarServer() && status > 1) return 0;
#endif

		// ?q????????a??s???
	//	if (broadcast)
		{
			mMapPlayer = &CMapServer::GetServer()->GetClientPlayerMap();
			iter_end = mMapPlayer->end();
			for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
			{
				pPlayer	= iter->second;
				if (pPlayer)
				{
				//	if (pPlayer->IsKindOf(CMapPlayer::CLASS_ID))
					{
						long sendWarMode = IsWar();
						
						/* For nation war, only send war mode to participating countries */
						if (IsWarType() == WAR_TYPE_NATION && m_nNationWarAttacker && m_nNationWarDefender)
						{
							unsigned char playerCountry = pPlayer->GetSaveData()->plrCountryID;
							if (playerCountry != m_nNationWarAttacker && playerCountry != m_nNationWarDefender)
							{
								/* Third country - keep normal mode */
								sendWarMode = 0;
							}
						}
						
						// ???]????
						pPlayer->m_bIsMapWar = 0;
						// 2011/10/25 ??K???}?l?????????A?????a?Q??????Q???????D?A??????????
						pPlayer->ClearMagic();
						//
						// if (pPlayer->IsOutMap())	// ?????????A???A?@???_?u?H
						// KickPlayer(CMapPlayer * pPlayer)
						//
						if (broadcast)
						{
							if (!IsObjectDeleted(pPlayer->GetHandle()))
							{
								pMsg = pPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
								
								pMsg->m_Msg.m_UpdateData2Msg.Reset();
								pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_WAR_MODE, 0, sendWarMode, IsWarType());
								int test = IsWar();

								pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
								//
								Disconnect_ClearAllRecord( pPlayer->GetUID(), 3 );
							}
						}
					}
				}
			}
		}
		return(1);
	}
	return(0);
}

// ???a??O?_?????a??(NPC ?H??\??B?z?W?P?_)
long CMapServer::IsMapWar(long map_id, long include_3_city)
{
	struct gs_StageData * pStg;

	if (IsWar())
	{
		if (include_3_city)
		{
			if ((map_id == gameCAPITAL_WEI) || (map_id == gameCAPITAL_SHU) || (map_id == gameCAPITAL_WU))
				return(1);
		}
		//
		if (pStg = gameStageGetPtr(map_id))
		{
			if (pStg->gstgMode == mapMode_CountryWar)
				return(1);
		}
	}
	return(0);
}

// ?S???a?I????e
long CMapServer::IsSpecialMapWar(long map_code, long * country_id)
{
	long r = 0;

	if (IsWar())
	{
		switch(map_code)
		{
		case 1:							// ?Q
		case 130:
		case 279:
		case 278:		// ?Q???Z??
		case 300:
			*country_id = ID_COUNTRY_WEI;
			r++;
			break;
		case 11:						// ??
		case 131:
		case 280:
		case 303:		// 妇瓣ゑ猌初
		case 301:
			*country_id = ID_COUNTRY_SHU;
			r++;
			break;
		case 21:						// ?d
		case 132:
		case 281:
		case 304:		// ?d???Z??
		case 302:
			*country_id = ID_COUNTRY_WU;
			r++;
			break;
		case 314:
		case 315:
		case 316:
		case 317:
		case 305:	// ???n?Z?x??x
		case 306:	// ???n??x??x
		case 307:	// ?s???Z?x??x
		case 308:	// 穝偿ゅ﹛据
		case 309:	// ???L?Z?x??x
		case 310:	// ???L??x??x
		case 333:	// ???w?Z?x??x
		case 334:	// ???w??x??x
		case 335:	// 粮锭猌﹛据
		case 336:	// 粮锭ゅ﹛据
		case 337:	// ?~???Z?x??x
		case 338:	// 簙いゅ﹛据
		case 311:	// ???n??Z??
		case 312:	// 穝偿ゑ猌初
		case 313:	// ???L??Z??
		case 339:	// ???w??Z??
		case 340:	// 粮锭ゑ猌初
		case 341:	// 簙いゑ猌初
		case 351:	// í纠驹初
		case 357:	// ?r?s??
		case 358:	// ?H?B?}
		case 369:	// キゑ猌初
		case 370:	// ???{??Z??
		case 371:	// 锭ゑ猌初
		case 350:	// 绰锭瑌
		case 164:	// ?r?s?}
		//
		case 548:
		case 550:
		case 552:
		case 554:
			*country_id = ID_COUNTRY_NULL;
			r++;
			break;
		}
	}
	return(r);
}

// teleport_other = 1???P??O???e??, ????????B?z
// return: 1 map????v??z???A?B?z????(?_?h?n???Login?B?z)
long CMapServer::CountryWar_TeleportOtherCountryPlayer(long map_code, long teleport_other)
{
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	long to_map_code, to_map_x, to_map_y;
	CMapCtrl * pCtrl;
	CObjectManager * pObjMgr;
	std::map<long, CMapObject *>::iterator iter;
	CMapPlayer * pPlayer;

	pTown = GetTownDataByID(map_code);
	if (pTown)
	{
		if (IsMapWar(map_code))
		{
			pCtrl = FindMap(map_code);
			if (pCtrl)
			{
				// ????????
				if (pCtrl->m_pGate)
				{
					if (pCtrl->m_pGate->m_NoReborn)
						pCtrl->m_pGate->Reborn();
				}
				//
				if (teleport_other)
				{
					pObjMgr = pCtrl->GetAllPlayerList();
					// ........ ???a??????a .........
					for (iter = pObjMgr->GetBegin(); iter != pObjMgr->GetEnd(); iter++)
					{
						pPlayer	= (CMapPlayer *)iter->second;
						if (pPlayer)
						{
							if (pTown->ptCountryID && (pPlayer->GetSaveData()->plrCountryID != pTown->ptCountryID))
							{
								to_map_code = pPlayer->GetSaveData()->plrSaveMapCode;
								to_map_x = pPlayer->GetSaveData()->plrSavePosX;
								to_map_y = pPlayer->GetSaveData()->plrSavePosY;
								ChangeSaveMap(pPlayer, to_map_code, to_map_x, to_map_y, 1);
							}
						}
					}
					// ????n?J???????a?n??O?A????a???e
					kimSetFlagToCheckLogin(map_code, CHECK_LOGIN_FLAG_COUNTRY_WAR_TELEPORT);
				}
				//
				return(1);
			}
		}
		else
			return(1);
	}
	return(0);
}

void CMapServer::SetNPCCountryUpdate(long clear)
{
	if (clear)
		m_IsNPCCountryUpdateFlag &= ~1;
	else
		m_IsNPCCountryUpdateFlag |= 1;
}

long CMapServer::IsNPCCountryUpdate()
{
	if (m_IsNPCCountryUpdate & 1)
		return(1);
	return(0);
}

void CMapServer::SetNPCRestorePos(long clear)
{
	if (clear)
		m_IsNPCCountryUpdateFlag &= ~2;
	else
		m_IsNPCCountryUpdateFlag |= 2;
}

long CMapServer::IsNPCRestorePos()
{
	if (m_IsNPCCountryUpdate & 2)
		return(1);
	return(0);
}

void CMapServer::SetNPCSetColor(long clear)
{
	if (clear)
		m_IsNPCCountryUpdateFlag &= ~4;
	else
		m_IsNPCCountryUpdateFlag |= 4;
}

long CMapServer::IsNPCSetColor()
{
	if (m_IsNPCCountryUpdate & 4)
		return(1);
	return(0);
}

// ??n???? CMapCtrl->Update ????B?z
void CMapServer::SetNPCMapSpaceUpdate(long clear)
{
	if (clear)
		m_IsNPCCountryUpdateFlag &= ~8;
	else
		m_IsNPCCountryUpdateFlag |= 8;
}

long CMapServer::IsNPCMapSpaceUpdate()
{
	if (m_IsNPCCountryUpdate & 8)
		return(1);
	return(0);
}

// ???W?q??
void CMapServer::CB_CountryWar_CountdownNotify(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_COUNTRY_WAR_COUNTDOWN_NOTIFY * pData;
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;
	CMapPlayer * pPlayer;
	//
	long nMsgSize;
/*	char msg[2048];
	struct MAP_SYSTEM_MESSAGE nMsg;

	pData = (struct MAP_COUNTRY_WAR_COUNTDOWN_NOTIFY *)pBuffer;
	if (!pData->nWarType)	// ???}?l???
	{
		wsprintf(msg, gameGetResourceName(msg_sp_START_COUNT), pData->nMin);
	}
	else					// ??????????
	{
		wsprintf(msg, gameGetResourceName(msg_sp_END_COUNT), pData->nMin);
	}
	//
	nMsg.m_nTalkerUID = 0;
	nMsg.m_nChannel = gameChannel_System;
	nMsg.m_TalkerName[0] = 0;
	msg_strncpy(nMsg.m_Message, msg, sizeof(nData.m_Message));
	nData.m_nMessageSize = strlen(nMsg.m_Message);
	nMsgSize = nMsg.GetSize();
*/
	struct MAP_UPDATE_PLAYER_DATA_PART2 nMsg;

	pData = (struct MAP_COUNTRY_WAR_COUNTDOWN_NOTIFY *)pBuffer;
	//
	//
	nMsg.Reset();
	nMsg.Add(UPART2_TYPE_WAR_COUNTDOWN, 0, pData->nWarType, pData->nMin);

	nMsgSize = nMsg.GetSize();
	//
	mMapPlayer = &CMapServer::GetServer()->GetClientPlayerMap();
	iter_end = mMapPlayer->end();
	for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
	{
		pPlayer	= iter->second;
		if (pPlayer)
		{
//			nMsg.m_nUID = pPlayer->GetUID();
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsgSize, 0);

#if defined NOTIFY_CWAR_READY
			// ??A?????q??
			if (pData->nWarType == 0xFE)
				continue;
#endif
			//
		}
	}
	//?]?w???w??X??(?????e1???????e)
	if(pData->nWarType == 0)
	{
		if(pData->nMin == 1)
		{
			CMapServer::GetServer()->SetWarPrepStatus(1);
			return;
		}
	}
	CMapServer::GetServer()->SetWarPrepStatus(0);

}
// ??s??????
void CMapServer::CB_CountryWar_InfoUpdate(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_UPDATE_COUNTRY_WAR_INFO * pData;
	struct MAP_UPDATE_PLAYER_DATA_PART2 nMsg;
	//
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;
	CMapPlayer * pPlayer;
	//
	long nMsgSize;
	long i, r;

	pData = (struct MAP_UPDATE_COUNTRY_WAR_INFO *)pBuffer;

#ifdef NEW_CROSS_SERVER_CWAR
	if(CMapServer::GetServer()->IsCrossCWarServer() && pData->nIsBegin > 1)
	{
		r = CMapServer::GetServer()->SetWarStatus(pData->nIsBegin, 0);
		return;
	}
#endif
	//
	CMapServer::GetServer()->m_nWarWeekDay = pData->nWeekDay;
	CMapServer::GetServer()->m_nWarType = pData->nType;
	CMapServer::GetServer()->m_nWarHour = pData->nHour;
	CMapServer::GetServer()->m_nWarMin = pData->nMin;
	 memcpyT(&CMapServer::GetServer()->m_nWarKingScore, &pData->nWarKingScore, sizeof(WAR_KING_SCORE));
	//
	if (CMapServer::GetServer()->IsWar())
		i = 1;
	else
		i = 0;
	if (i != pData->nIsBegin)
		i = 1;			// ???A???
	else
		i = 0;
//	// ???????????
//#ifndef USE_PACKAGE_DATA
	r = CMapServer::GetServer()->SetWarStatus(pData->nIsBegin, 0);
	CMapServer::GetServer()->SetWarPrepStatus(0);
//#endif
	// ????????^?_NPC??m
	if (i)
	{
		if (!pData->nIsBegin)
			CMapServer::GetServer()->SetNPCRestorePos();
		// ?????J????_???A?B??q
		CMapServer::GetServer()->GetMapManager()->Statue_Gate_Restore(0);
	}
	//
	nMsg.Reset();
	nMsg.Add(UPART2_TYPE_WAR_TIME, pData->nWeekDay, pData->nHour, pData->nMin);

	//
	if (i || pData->nIsBegin)
	{
		nMsg.Add(UPART2_TYPE_WAR_MODE, 0, pData->nIsBegin, pData->nType);
		nMsg.Add(UPART2_TYPE_WAR_KING_SCORE, 0, 1, pData->nWarKingScore.nValue[1]);
		nMsg.Add(UPART2_TYPE_WAR_KING_SCORE, 0, 2, pData->nWarKingScore.nValue[2]);
		nMsg.Add(UPART2_TYPE_WAR_KING_SCORE, 0, 3, pData->nWarKingScore.nValue[3]);
	}
	//
	nMsgSize = nMsg.GetSize();
	//
	mMapPlayer = &CMapServer::GetServer()->GetClientPlayerMap();
	iter_end = mMapPlayer->end();
	for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
	{
		pPlayer	= iter->second;
		if (pPlayer)
		{
			// ???\??
			if (i)
			{
#ifdef USE_NEW_CWAR_LIST_TIME_GIFT
				pPlayer->GetSaveData()->plrCWar_Gift_Tick = 0;
#endif
#if (defined(WAR_ADD_ARMY_POINT) || defined(CWAR_ADD_RED_SCORE))
				pPlayer->GetSaveData()->plrCWar_ArmyPoint_Tick = 0;
#endif
				pPlayer->AutoSaveCharData();
				if (pData->nIsBegin)	//// 秨﹍籔挡常骸
				{
					if (pPlayer->m_nHonorVal_Max == 0xffff)
						pPlayer->m_nHonorVal_Max = (unsigned short)gameCalcCountryWarHonorMax(pPlayer->GetCharData());
					pPlayer->GetSaveData()->plrHonorVal = (pPlayer->m_nHonorVal_Max >> 1);
					pPlayer->AutoSaveCharData();
				}
			}
			//
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsgSize, 0);
			//
			if (r)
				CMapServer::GetServer()->Disconnect_ClearAllRecord( pPlayer->GetUID(), 3 );
		}
	}
}

// ??O???(?? Login Server ?q??)(??/????/?x????)
void CMapServer::CB_CountryWar_UpdateForceTownData(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_UPDATE_MAP_FORCE_AND_TOWN_DATA * pData;
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	long i, map_code;
	//
	CMapCtrl * pCtrl;

	pData = (struct MAP_UPDATE_MAP_FORCE_AND_TOWN_DATA *)pBuffer;
	if (pData->nMapCode)
	{
		pTown = GetServer()->GetTownDataByID(pData->nMapCode);
		if (pTown)
		{
			pTown->ptOrgUID = pData->nOrgUID;
			pTown->ptConquestTime = pData->nConquestTime;		// 烩丁

			//if (pData->nCountryID)	// pData->nCountryID = 0 ?x????
			{
				pTown->ptCountryID = pData->nCountryID;
//GetServer()->Log("カ(%d)墩%d, 瓁刮%d", pData->nMapCode, pTown->ptCountryID, pTown->ptOrgUID);
				//
				pCtrl = CMapServer::GetServer()->FindMap(pData->nMapCode);
				if (pCtrl)
					pCtrl->Statue_Gate_UpdateCountry();
			}
		}
	}
	if (pData->nCountryID)
	{
		for (i=0; i<gameMAX_FORCECHANGE_NUMBER; i++)
		{
			map_code = pData->nForceChange[i];
			if (!map_code)
				break;
			pTown = GetServer()->GetTownDataByID(map_code);
			if (pTown)
			{
				pTown->ptCountryID = pData->nCountryID;
//GetServer()->Log("カ(%d)墩%d", map_code, pTown->ptCountryID);
				//
				pCtrl = CMapServer::GetServer()->FindMap(map_code);
				if (pCtrl)
					pCtrl->Statue_Gate_UpdateCountry();
			}
		}
	}
	//
	GetServer()->CalcTownTotalData();
	// ?????????a
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;
	CMapPlayer * pPlayer;
	long nMsgSize;

	nMsgSize = nLen;
	mMapPlayer = &CMapServer::GetServer()->GetClientPlayerMap();
	iter_end = mMapPlayer->end();
	for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
	{
		pPlayer	= iter->second;
		if (pPlayer)
			::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_CLIENT_MAP_FORCE_AND_TOWN_DATA, (char *)pBuffer, nMsgSize, 0);
	}
	//
	CMapServer::GetServer()->kimKeepDataToCheckLogin(0, PROTO_MAP_CLIENT_MAP_FORCE_AND_TOWN_DATA, (char *)pBuffer, nMsgSize);
	//
	//
//CMapServer::GetServer()->Log("Set NPC update country color mode !");
	GetServer()->SetNPCCountryUpdate();
}

// ??O???(?? Login Server ?q??)(?j?a???O???/??/????/?x????)
// ?s?@?????^??? Login(?L??)
void CMapServer::CB_LoginUpdateTownForceData(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_UPDATE_TOWN_FORCE_DATA * pData;
	struct LOGIN_MAP_FORCE_CHANGE2 nData;
	long i, k, val;
	struct gameFORCECHANGE_DATA * pForceChange;

	pData = (struct LOGIN_UPDATE_TOWN_FORCE_DATA *)pBuffer;
	//
	memset(&nData, 0, sizeof(nData));
	nData.m_nMapCode = pData->nMapCode;
	nData.m_nCountryID = pData->nCountryID;
	nData.m_nOrgUID = pData->nOrgUID;
//CMapServer::GetServer()->Log("Army change force: %08x, %d", pData->nOrgUID, pData->nCountryID);
	// ?s?@??O?????
	pForceChange = gameGetForceChangeTablePtr(nData.m_nMapCode);
	if (pForceChange)
	{
		k = 0;
		for (i=0; i<gameMAX_FORCECHANGE_NUMBER; i++)
		{
			val = pForceChange->fcSet[i];
			if (!val)
				break;
			nData.m_nMapCode_ForceChange[k++] = (unsigned short)val;
		}
	}
	// ?e?X?T??
	int hLoginServer = CMapServer::GetServer()->GetLoginServer();
	CMapServer::GetServer()->SendData(hLoginServer, 0, PROTO_LOGIN_MAP_FORCE_CHANGE2, (char *)&nData, sizeof(nData), 0);
}

void CMapServer::CB_CountryWar_TeleportOtherCountry(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_MAP_TELEPORT_OTHER_COUNTRY * pData;

	pData = (struct LOGIN_MAP_TELEPORT_OTHER_COUNTRY *)pBuffer;
	CMapServer::GetServer()->CountryWar_TeleportOtherCountryPlayer(pData->nMapCode, pData->nTeleportOther);
}

void CMapServer::CB_CountryWar_StatueBrokenMessage(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_STATUE_BROKEN_MESSAGE * pData;
	struct MAP_BROADCAST_MESSAGE spMsg;

	pData = (struct MAP_STATUE_BROKEN_MESSAGE *)pBuffer;
	if (pData->nOrganizeName[0])
	{
		memset(&spMsg, 0, sizeof(spMsg));
		spMsg.nWavType = gameSPMSG_WAV_STATUE_BROKEN;
		CMapServer::GetServer()->GetMapManager()->MakeSPMessage_GetCity(0, pData->nOrganizeName, pData->nMapCode, (char *)&spMsg.nMessage);
		spMsg.nMessageSize = strlen(spMsg.nMessage);
		CMapServer::GetServer()->GetMapManager()->BroadcastSPMessageToAllMap(&spMsg);
		//
		if (pData->nPlayerName[0])
		{
		//	spMsg.nWavType = gameSPMSG_WAV_PLAYERTALK;
		//	CMapServer::GetServer()->GetMapManager()->MakeSPMessage_GetCity(1, pData->nPlayerName, pData->nMapCode, (char *)&spMsg.nMessage);
			spMsg.nWavType = gameSPMSG_WAV_PLAYERTALK;
			CMapServer::GetServer()->GetMapManager()->MakeSPMessage_GetCity(2, pData->nOrganizeName, pData->nMapCode, (char *)&spMsg.nMessage);
			spMsg.nMessageSize = strlen(spMsg.nMessage);
			CMapServer::GetServer()->GetMapManager()->BroadcastSPMessageToAllMap(&spMsg);
		}
	}
}

void CMapServer::CB_MapUpdateCWarKListResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_UPDATE_COUNTRY_WAR_LIST_RESULT * pData;

	pData = (struct MAP_UPDATE_COUNTRY_WAR_LIST_RESULT *)pBuffer;

#if defined USE_WARLIST_ORDER_SEND
	warlist_order_head* whptr = (warlist_order_head*)pBuffer;

	if (whptr->count_size > sizeof(GetServer()->nCWarKList) ||
		whptr->write_pos + whptr->current_size > sizeof(GetServer()->nCWarKList))
	{
		memset(&GetServer()->nCWarKList, 0, sizeof(GetServer()->nCWarKList));
		GetServer()->Log("ERROR: USE_WARLIST_ORDER_SEND Get CKWAR list data size invalid(%d)", whptr->count_size);
		return;
	}

	if (whptr->current == 1)
	{
		GetServer()->nCWarKListSize = 0;
	}

	if (whptr->current == whptr->count)
	{
		GetServer()->nCWarKListSize = whptr->count_size;
	}

	char* write_pos = ((char*)&GetServer()->nCWarKList) + whptr->write_pos;
	char* read_pos = pBuffer + sizeof(warlist_order_head);

	memcpyT(write_pos, read_pos, whptr->current_size);
#else
	if (nLen <= sizeof(GetServer()->nCWarKList))
	{
		GetServer()->nCWarKListSize = pData->nDataSize;
		memcpyT(&GetServer()->nCWarKList, &pData->nData, pData->nDataSize);
	}
	else
	{
		memset(&GetServer()->nCWarKList, 0, sizeof(GetServer()->nCWarKList));
	}
#endif
}



int __cdecl innerCB_Sort_CWarList(const void *p1, const void *p2 )
{
	long r;
	struct plrDATA_WORLD_COUNTRY_WAR_LIST_DETAIL * pd1;
	struct plrDATA_WORLD_COUNTRY_WAR_LIST_DETAIL * pd2;

	if (p1 && p2)
	{
		pd1 = (struct plrDATA_WORLD_COUNTRY_WAR_LIST_DETAIL *)p1;
		pd2 = (struct plrDATA_WORLD_COUNTRY_WAR_LIST_DETAIL *)p2;
		//
		// ?H??~????
		if (pd1->pwcwJobID != pd2->pwcwJobID)
		{
			// ... ?p??j ...
			r = pd1->pwcwJobID - pd2->pwcwJobID;
			return(r);
		}

		// ?A??n??
		if (pd1->pwcwHonor != pd2->pwcwHonor)
		{
			// ... ?j??p ...
			r = pd2->pwcwHonor - pd1->pwcwHonor;
			return(r);
		}

		// ... ?j??p ...
		r = pd2->pwcwKillCount - pd1->pwcwKillCount;
		return(r);
	}
	return(0);
}


void CMapServer::CB_MapUpdateCountryWarListResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_UPDATE_COUNTRY_WAR_LIST_RESULT * pData;

	pData = (struct MAP_UPDATE_COUNTRY_WAR_LIST_RESULT *)pBuffer;
	int i;
	int count = 6;
	struct plrDATA_WORLD_COUNTRY_WAR_LIST TempCWarList;
	warlist_order_head* whptr = (warlist_order_head*)pBuffer;

#if defined USE_WARLIST_ORDER_SEND

	if (whptr->count_size > sizeof(GetServer()->nCWarList) ||
		whptr->write_pos + whptr->current_size > sizeof(GetServer()->nCWarList))
	{
		memset(&GetServer()->nCWarList, 0, sizeof(GetServer()->nCWarList));
		GetServer()->Log("ERROR: USE_WARLIST_ORDER_SEND Get CWAR list data size invalid(%d)", whptr->count_size);
		return;
	}

	if (whptr->current == 1)
	{
		GetServer()->nCWarListSize = 0;
	}

	if (whptr->current == whptr->count)
	{
		GetServer()->nCWarListSize = whptr->count_size;
	}

	char* write_pos = ((char*)&GetServer()->nCWarList) + whptr->write_pos;
	char* read_pos = pBuffer + sizeof(warlist_order_head);

	memcpyT(write_pos, read_pos, whptr->current_size);

	if (whptr->current == whptr->count)//?s?U??~??@????
	{
		memset(&GetServer()->m_KingStatue, 0, sizeof(GetServer()->m_KingStatue));
		UncompressMemoryToMemoryZIP((void *)&TempCWarList, (void *)&GetServer()->nCWarList, whptr->count_size, sizeof(TempCWarList));
		qsort(TempCWarList.nData, gameMAX_CWAR_LIST, sizeof(struct plrDATA_WORLD_COUNTRY_WAR_LIST_DETAIL), innerCB_Sort_CWarList);
		for (i=0; i<gameMAX_CWAR_LIST; i++)
		{
			if(TempCWarList.nData[i].pwcwJobID > 0 && TempCWarList.nData[i].pwcwJobID <= 6)
			{				
				if(GetServer()->m_KingStatue[TempCWarList.nData[i].pwcwJobID-1].pwcwLevel==0)
				{
					memcpyT(&GetServer()->m_KingStatue[TempCWarList.nData[i].pwcwJobID-1],&TempCWarList.nData[i],sizeof(struct plrDATA_WORLD_COUNTRY_WAR_LIST_DETAIL));
					GetServer()->m_KingStatueFlag[TempCWarList.nData[i].pwcwJobID-1]++;
					if(GetServer()->m_KingStatueNPC[TempCWarList.nData[i].pwcwJobID-1])
						if(TempCWarList.nData[i].pwcwSex == 2)
							GetServer()->m_KingStatueNPC[TempCWarList.nData[i].pwcwJobID-1]->GetShowData()->plrWID = 1028+(TempCWarList.nData[i].pwcwJobID-1)*2+1;
						else
							GetServer()->m_KingStatueNPC[TempCWarList.nData[i].pwcwJobID-1]->GetShowData()->plrWID = 1028+(TempCWarList.nData[i].pwcwJobID-1)*2;
					count -= 1;
				}
				if(count==0)
					break;
			}
		}
	}
#else
	if (nLen <= sizeof(GetServer()->nCWarList))
	{
		GetServer()->nCWarListSize = pData->nDataSize;
		memcpyT(&GetServer()->nCWarList, &pData->nData, pData->nDataSize);
	}
	else
	{
		memset(&GetServer()->nCWarList, 0, sizeof(GetServer()->nCWarList));
		GetServer()->Log("ERROR: Get CWAR list data size invalid(%d)", nLen);
	}
	memset(&GetServer()->m_KingStatue, 0, sizeof(GetServer()->m_KingStatue));
	UncompressMemoryToMemoryZIP((void *)&TempCWarList, (void *)&GetServer()->nCWarList, whptr->count_size, sizeof(TempCWarList));
	for (i=0; i<gameMAX_CWAR_LIST; i++)
	{
		if(TempCWarList.nData[i].pwcwJobID > 0 && TempCWarList.nData[i].pwcwJobID <= 6)
		{
			if(GetServer()->m_KingStatue[TempCWarList.nData[i].pwcwJobID-1].pwcwLevel==0)
			{
				memcpyT(&GetServer()->m_KingStatue[TempCWarList.nData[i].pwcwJobID-1],&TempCWarList.nData[i],sizeof(struct plrDATA_WORLD_COUNTRY_WAR_LIST_DETAIL));
				GetServer()->m_KingStatueFlag[TempCWarList.nData[i].pwcwJobID-1]++;
				if(GetServer()->m_KingStatueNPC[TempCWarList.nData[i].pwcwJobID-1])
					if(TempCWarList.nData[i].pwcwSex == 2)
						GetServer()->m_KingStatueNPC[TempCWarList.nData[i].pwcwJobID-1]->GetShowData()->plrWID = 1028+(TempCWarList.nData[i].pwcwJobID-1)*2+1;
					else
						GetServer()->m_KingStatueNPC[TempCWarList.nData[i].pwcwJobID-1]->GetShowData()->plrWID = 1028+(TempCWarList.nData[i].pwcwJobID-1)*2;
				count -= 1;
			}
		}
		if(count==0)
			break;
	}
#endif
}

void CMapServer::CB_MapUpdateAutoSO(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_MAP_UPDATE_AUTO_SO_RESULT * pData;
	CMapPlayer * pPlayer;

	pData = (struct LOGIN_MAP_UPDATE_AUTO_SO_RESULT *)pBuffer;
	//
	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(pData->nPlayerUID, CMapPlayer::CLASS_ID);
	if (pPlayer)
	{	// ???? Login Server ??e???G
		::msgSendData( pPlayer->GetClientID(), pComm->pcUID, PROTO_LOGIN_UPDATE_AUTO_SO, (char *)&pData->nSoData, pData->nSoData.GetSize(), 0 );
	}
}

void CMapServer::CB_MapUpdateSPCModeTime(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
#ifdef USE_SPCMODE_CN_ADULT_LIMIT
	struct MAP_UPDATE_SPCMODE_TIME * pData;
	CMapPlayer * pPlayer;
	CLoginCheck * pLoginCheck;

	pData = (struct MAP_UPDATE_SPCMODE_TIME *)pBuffer;
	//
#ifndef USE_PACKAGE_DATA
	GetServer()->Log("DEBUG: Update spcmode time(%d)(%d,%d)", pData->m_nUID, pData->nOnlineTime, pData->nOfflineTime);
#endif
	//
	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(pData->m_nUID, CMapPlayer::CLASS_ID);
	if (pPlayer)
	{
		pPlayer->m_nOnlineTime = pData->nOnlineTime;
		pPlayer->m_nOfflineTime = pData->nOfflineTime;
		pPlayer->m_nLastLogoutTime = pData->nLastLogoutTime;
		//
		pPlayer->m_nPlayerFlags |= PLAYER_FLAGS_UPDATE_SPCMODE;
		//pPlayer->GetSaveData()->plrSPCMode_CN_OnLineTime = pPlayer->m_nOnlineTime;
		//pPlayer->GetSaveData()->plrSPCMode_CN_OffLineTime = pPlayer->m_nOfflineTime;
		//pPlayer->GetSaveData()->plrSPCMode_CN_LastLogoutTime = pPlayer->m_nLastLogoutTime;
	}
	else
	{
		if (GetServer()->m_LoginCheckMap.find(pData->m_nUID) != GetServer()->m_LoginCheckMap.end())
		{
			pLoginCheck = &GetServer()->m_LoginCheckMap[pData->m_nUID];
			pLoginCheck->m_nOnlineTime = pData->nOnlineTime;
			pLoginCheck->m_nOfflineTime = pData->nOfflineTime;
			pLoginCheck->m_nLastLogoutTime = pData->nLastLogoutTime;
		}
	}
#endif
}
// ========================================================================
void CMapServer::CB_MapSetRepayment(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_SET_REPAYMENT_FROM_LOGIN * pData;

	pData = (struct MAP_SET_REPAYMENT_FROM_LOGIN *)pBuffer;
	//
	memcpyT(CMapServer::GetServer()->GetRepayData(), &pData->nRepayment, sizeof(struct MAP_SET_REPAYMENT));
	CMapServer::GetServer()->Log("获取偿还数据(%d,%d).", pData->nRepayment.nType, pData->nAcitvity.nType);
	//
	memcpyT(CMapServer::GetServer()->GetActivityData(), &pData->nAcitvity, sizeof(struct MAP_SET_ACTIVITY));
}

void CMapServer::CB_MapSetHistoryTimeData(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_SET_HISTORY_BATTLE * pData;
#ifdef USE_HISTORY_COMPRES_SETTING
	long size, max_size;
#endif

	pData = (struct MAP_SET_HISTORY_BATTLE *)pBuffer;
	//
	if (CMapServer::GetServer()->m_nMapServerSubID != WORLD_MAPSERVER_SUBID)
	{
		CMapServer::GetServer()->Log("获取历史数据不过没有战争服务器.");
	}
	else
	{
#ifdef USE_HISTORY_COMPRES_SETTING
		max_size = sizeof(CMapServer::GetServer()->m_nHistorySetData);
		size = UncompressMemoryToMemoryZIP(&CMapServer::GetServer()->m_nHistorySetData, pData, nLen, max_size);
		if (size != max_size)
		{
			CMapServer::GetServer()->Log("错误：解码历史数据失败(%s,%s).", size, max_size);
		}
		else
		{
			CMapServer::GetServer()->Log("获取历史数据.");
			// 穝砞﹚丁
			CMapServer::GetServer()->m_nHistoryManager.HistoryResetTimeData();
		}
#else
		memcpyT(CMapServer::GetServer()->m_nHistorySetData, pData, sizeof(CMapServer::GetServer()->m_nHistorySetData));
		CMapServer::GetServer()->Log("获取历史数据.");
		// 穝砞﹚丁
		CMapServer::GetServer()->m_nHistoryManager.HistoryResetTimeData();
#endif
	}
}

// 砞﹚猌活丁
void CMapServer::CB_MapSetSoulTime(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_SET_SOUL_TIME * pData;

	pData = (struct MAP_SET_SOUL_TIME *)pBuffer;
	//
	memcpyT(CMapServer::GetServer()->GetSoulSetData(), &pData->nSoulSet, sizeof(struct MAP_SET_SOUL_DATA));
	CMapServer::GetServer()->Log("获取武魂数据(%d,%d)(%d,%d).", pData->nCanEnter, pData->nIsStart, pData->nSoulTicketDuedate, pData->nSoulItemDuedate);
	//
	//CMapServer::GetServer()->InnerSoulCalcData();
	CMapServer::GetServer()->Soul_SetSoulTicketDuedate(pData->nSoulTicketDuedate);
	CMapServer::GetServer()->Soul_SetSoulItemDuedate(pData->nSoulItemDuedate);
	//
	//CMapServer::GetServer()->m_IsSoulBattle = pData->nIsStart;
	CMapServer::GetServer()->m_CanEnterMode = pData->nCanEnter;
	CMapServer::GetServer()->SetSoulBattleStatus(pData->nIsStart, 0);
}

// Login ?]?w?Z?????????
void CMapServer::CB_MapLoginUpdateSTicketCount(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_SOUL_RECORD_DATA * pData;
	long i;
	struct MAP_SOUL_RECORD_INNER_DATA * pRec;

	pData = (struct MAP_SOUL_RECORD_DATA *)pBuffer;
	for (i=0; i<pData->nTotal; i++)
	{
		pRec = CMapServer::GetServer()->GetSoulRecordDetail(pData->nData[i].nMapCode);
		if (pRec)
		{
			memcpyT(pRec->nCount, pData->nData[i].nCount, sizeof(pRec->nCount));
		}
	}
}

// ??l??:
// ????map_code ??b???D?n???? code
long CMapServer::FindForceChangeCity(long map_code)
{
	struct gameFORCECHANGE_DATA * pForceChange;
	long i, r, val;

	for (r=1; r<=gameMAX_FORCECHANGE; r++)
	{
	//	if (r == map_code)								// 2010/04/02 BUG?
	//		return(r);
		pForceChange = gameGetForceChangeTablePtr(r);
		if (pForceChange)
		{
			for (i=0; i<gameMAX_FORCECHANGE_NUMBER; i++)
			{
				val = pForceChange->fcSet[i];
				if (!val)
					break;
				if (val == map_code)
				{
					return(r);
				}
			}
		}
	}
	return(0);
}

// ?????y???~???
long InnerGetCWarRewardNum(long billing)
{
	long val;

	val = 0;
	if (billing > 0)
	{
#ifdef USE_JAPAN_CWAR_REWARD
		if (billing <= 10)
		{
			val = 12;
		}
		else if (billing <= 20)
		{
			val = 10;
		}
		else if (billing <= 30)
		{
			val = 8;
		}
		else
			val = 6;
#elif defined(VietnamMode)
		if (billing == 1)
		{
			val = 40;
		}
		else if (billing == 2)
		{
			val = 20;
		}
		else if (billing == 3)
		{
			val = 10;
		}
		else if (billing <= 10)
		{
			val = 5;
		}
		else if (billing <= 20)
		{
			val = 2;
		}
#else
		if (billing <= 25)
		{
			val = 12;
		}
		else if (billing <= 50)
		{
			val = 10;
		}
		else if (billing <= 75)
		{
			val = 8;
		}
		else
			val = 6;
#endif
	}
	return(val);
}

// ?g?D????y???~???
int InnerGetCWarRewardKingNum(int billing, int score)
{
	float bonus = 1.0f;

	if(billing > 0)
	{
		if(billing <= 1)
			bonus = 3.0f;
		else if(billing <= 10)
			bonus = 2.7f;
		else if(billing <= 20)
			bonus = 2.4f;
		else if(billing <= 30)
			bonus = 2.1f;
		else if(billing <= 40)
			bonus = 1.8f;
		else if(billing <= 60)
			bonus = 1.6f;
		else if(billing <= 80)
			bonus = 1.4f;
		else if(billing <= 100)
			bonus = 1.2f;
	}
	
	return (int)(score * bonus);
}

// ????????s
void CMapServer::CB_MapCityDataPack(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_CITY_DATA_PACK * pData;
	struct MAP_UPDATE_PLAYER_DATA_PART2 nMsg;
	CMapCharMsg *	pMsg;
	//
	struct plrDATA_WORLD_TOWN_DETAIL * pTownDetail;
	//
	CMapPlayer * pMapPlayer = 0;
	unsigned long long gold = 0;
	long hLoginServer = 0;
	long city_id = 0;
	MAP_ABANDON_CITY_NOTIFY abandonCityData;
	MAP_CITY_TAX_RATE_NOTIFY taxRateData;
	//
	CMapCtrl * pCtrl;
	//
	struct gameFORCECHANGE_DATA * pForceChange;
	long val, val2;
	//
	long i;
	char tmpstr[512];

	pData = (struct MAP_CITY_DATA_PACK *)pBuffer;
	switch(pData->nType)
	{
	case TYPE_PACK_GATE_DOWN_LEVEL:		// (nData1 = Map Code, nData2 = level)
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData1);
		if (pTownDetail)
		{
			pTownDetail->ptForceLevel = (unsigned char)pData->nData2;
			// ?q????????a
			nMsg.Reset();
			nMsg.Add(UPART2_TYPE_CITYDATA_GATE_LEVEL, 0, pData->nData1, pData->nData2); // map code // level

			CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsg.GetSize(), 1);
		}
		// !!! ?O?_?n????e?????A(?|???????n)
		break;
	case TYPE_PACK_STATUE_DOWN_LEVEL:	// 繨钩
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData1);
		if (pTownDetail)
		{
			pTownDetail->ptStatueLevel = (unsigned char)pData->nData2;
			// ?q????????a
			nMsg.Reset();
			nMsg.Add(UPART2_TYPE_CITYDATA_STATUE_LEVEL, 0, pData->nData1, pData->nData2); // map code // level

			CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsg.GetSize(), 1);
		}
		// !!! ?O?_?n????e?????A(?|???????n)
		break;
	case TYPE_PACK_GET_PREBEND:
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->nData1);
		if (pMapPlayer)
		{
			if (pData->nData2 == 1)
			{
				pMapPlayer->GetPrebendResult();
			}
			else if (pData->nData2 == 2)
			{
				pMapPlayer->SendMsgToClientByID( 24272 );
			}
		}
		break;
		//???D?R?O
	case TYPE_PACK_DEPOSIT_GOLD: // ?s??

		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );

		if( pMapPlayer == NULL )
		{
			GetServer()->Log( "Player not found on map" );
			return;
		}

		gold = pMapPlayer->GetGold();
		if( gold >= (unsigned long long)pData->nData2 )
			gold -= (unsigned long long)pData->nData2;
		else
		{
			GetServer()->Log( "存入金额(=%u)错误！玩家(%s,uid=%u)持有金额(=%u)", pData->nData2, pMapPlayer->GetName(), pMapPlayer->GetUID(), pMapPlayer->GetGold() );
			return;
		}

		if( pData->nData3 == 0 ) //login??@???^??
		{
			// ?G???T?{?w?q??login?[??
			pData->nData3 = 1; // mode : 0(???X?n?D)?B1(?T?{)
			hLoginServer = GetServer()->m_hLoginServer;
			if( GetServer()->IsConnectedToServer(hLoginServer) && pMapPlayer->CanSaveData() )
			{
				pMapPlayer->GetSaveData()->plrGold = gold;
				pMapPlayer->SaveCharData();
				pMapPlayer->UpdateClientGoldAndWeight();

				GetServer()->SendData( hLoginServer, pComm->pcUID, PROTO_LOGIN_CITY_DATA_PACK, pBuffer, nLen, 0 );

				::SendGoldLog(pData->nData2, pMapPlayer, LOGTYPE_ITEM_CITY_DEPOSIT_GOLD, NULL);
			}
		}
		break;

	case TYPE_PACK_WITHDRAW_GOLD: // 矗窥

		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );

		if( pMapPlayer == NULL )
		{
			GetServer()->Log( "ERROR: CB_MapCityDataPack withdraw: map player is null" );
			return;
		}

		gold = pMapPlayer->GetGold() + (unsigned long long)pData->nData2;
		if( gold > (unsigned long long)gameMAX_GOLD )
		{
			GetServer()->Log( "取出金额(=%u)错误！玩家(%s,uid=%u)持有金额(=%u)", pData->nData2, pMapPlayer->GetName(), pMapPlayer->GetUID(), pMapPlayer->GetGold(), gameMAX_GOLD );
			return;
		}

		if( pData->nData3 == 0 ) //login??@???^??
		{
			// ?G???T?{?w?q??login????
			pData->nData3 = 1; // mode : 0(???X?n?D)?B1(?T?{)
			hLoginServer = GetServer()->m_hLoginServer;
			if( GetServer()->IsConnectedToServer(hLoginServer) && pMapPlayer->CanSaveData() )
			{
				pMapPlayer->GetSaveData()->plrGold = gold;
				pMapPlayer->SaveCharData();
				pMapPlayer->UpdateClientGoldAndWeight();

				GetServer()->SendData( hLoginServer, pComm->pcUID, PROTO_LOGIN_CITY_DATA_PACK, pBuffer, nLen, 0 );

				::SendGoldLog(pData->nData2, pMapPlayer, LOGTYPE_ITEM_CITY_WITHDRAW_GOLD, NULL);
			}
		}

		break;

	case TYPE_PACK_SET_TAX_RATE: // ?]?w?|?v

		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData3);

	//	if( pTownDetail == NULL )
	//	{
	//		GetServer()->Log( "ERROR: CB_MapCityDataPack set tax rate: city detail data is null" );
	//		return;
	//	}
		if (pTownDetail)
			pTownDetail->ptTax = ( unsigned char )pData->nData2;

		//?q???P?@map??????a
		city_id = pData->nData3;
		::memset( &taxRateData, 0, sizeof(taxRateData) );
		taxRateData.city_id = city_id;
		taxRateData.taxRate = pData->nData2;
	//	CMapServer::GetServer()->SendMessageToAllPlayers_Map( city_id, PROTO_MAP_CITY_TAX_RATE_NOTIFY, (char *)&taxRateData, sizeof(taxRateData) );
		CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_CITY_TAX_RATE_NOTIFY, (char *)&taxRateData, sizeof(taxRateData), 1);

		break;

	case TYPE_PACK_ABANDON_CITY: // ??
		//???????????s??login?w?B?z?A???B?u?w??P?b?c???????a?B?z
		//?q???P?@map??????a
		city_id = pData->nData2;
		::memset( &abandonCityData, 0, sizeof(abandonCityData) );
		abandonCityData.cityLordUid = pData->nData1;
		abandonCityData.city_id = city_id;
	//	CMapServer::GetServer()->SendMessageToAllPlayers_Map( city_id, PROTO_MAP_ABANDON_CITY_NOTIFY, (char *)&abandonCityData, sizeof(abandonCityData) );
		CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_ABANDON_CITY_NOTIFY, (char *)&abandonCityData, sizeof(abandonCityData), 1);

		break;

	case TYPE_PACK_GATE_LEVEL_UP:		//ど
		//xiun : copy ?? TYPE_PACK_GATE_DOWN_LEVEL
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData2);
		if (pTownDetail)
		{
			pTownDetail->ptForceLevel = (unsigned char)pData->nData3;
			// ?q????????a
			nMsg.Reset();
			nMsg.Add(UPART2_TYPE_CITYDATA_GATE_LEVEL, 0, pData->nData2, pData->nData3); // map code // level

			CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsg.GetSize(), 1);
		}
		// ?n????e?????A
		pCtrl = CMapServer::GetServer()->GetMapManager()->FindMap(pData->nData2);
		if (pCtrl)
			pCtrl->Statue_Gate_Restore(0x4);
		// ?????b??O?]?w????
		pForceChange = gameGetForceChangeTablePtr(pData->nData2);
		if (pForceChange)
		{
			for (i=0; i<gameMAX_FORCECHANGE_NUMBER; i++)
			{
				val = pForceChange->fcSet[i];
				if (!val)
					break;
				if (val != pData->nData2)
				{
					pCtrl = CMapServer::GetServer()->GetMapManager()->FindMap(val);
					if (pCtrl)
						pCtrl->Statue_Gate_Restore(0x4);
				}
			}
		}
		break;

	case TYPE_PACK_STATUE_LEVEL_UP:		//繨钩ど
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData2);
		if (pTownDetail)
		{
			pTownDetail->ptStatueLevel = (unsigned char)pData->nData3;
			// ?q????????a
			nMsg.Reset();
			nMsg.Add(UPART2_TYPE_CITYDATA_STATUE_LEVEL, 0, pData->nData2, pData->nData3); // map code // level

			CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsg.GetSize(), 1);
		}
		// ?n????e?????A
		pCtrl = CMapServer::GetServer()->GetMapManager()->FindMap(pData->nData2);
		if (pCtrl)
			pCtrl->Statue_Gate_Restore(0x2);
		break;
/*
	case TYPE_PACK_HIRE_GUARD:		//侗ノ猌盢
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData2);
		if (pTownDetail)
		{
			if( pData->nData3 >= 0 || pData->nData3 < gameMAX_CITYGUARD_ROLE_SETTING )
			{
				pTownDetail->ptGen_GuardPos[ pData->nData3 ] = pData->nData4;
				//?q?????D
				pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );

				if( pMapPlayer )
				{
					CMapCharMsg *	pMsg;

					pMsg			= pMapPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
					pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
					pMsg->m_nSize = pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_CITYDATA_GUARD_POS;
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nVal = pData->nData2;
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = pData->nData3;
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData2 = pData->nData4;
				}
			}
		}
		break;

	case TYPE_PACK_FIRE_GUARD:		//秆侗猌盢
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData2);
		if (pTownDetail)
		{
			if( pData->nData3 >= 0 || pData->nData3 < gameMAX_CITYGUARD_ROLE_SETTING )
			{
				pTownDetail->ptGen_GuardPos[ pData->nData3 ] = 0;
				//?q?????D
				pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );

				if( pMapPlayer )
				{
					pMsg			= pMapPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
					pMsg->m_Msg.m_UpdateData2Msg.m_nCount = 1;
					pMsg->m_nSize = pMsg->m_Msg.m_UpdateData2Msg.GetSize();
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nType = UPART2_TYPE_CITYDATA_GUARD_POS;
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nVal = pData->nData2;
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData1 = pData->nData3;
					pMsg->m_Msg.m_UpdateData2Msg.m_nData[0].m_nData2 = 0;
				}
			}
		}
		break; */

	case TYPE_PACK_UPDATE_GUARD_POS:		//??s?Z?N?n?u??m, pData->nData2 = city id
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData2);
		if (pTownDetail)
		{
			long role_idx = pData->nData3;
			if( role_idx >= 0 && role_idx < gameMAX_CITYGUARD_ROLE_SETTING )
			{
				long newGuardPos = pData->nData4;
				pTownDetail->ptGen_GuardPos[ role_idx ] = (unsigned char)newGuardPos;	// 0 = ?L, 1, 2, ...
				// ......................
				// ??z?Z?N???? ?s?W/????
				struct gameCITYGUARD_SET_DATA * pCGSetData;

				pCGSetData = gameGetCityGuardSetPtr(pData->nData2);
				if (pCGSetData)
				{
					if (newGuardPos == 0)		// ?M??
					{
#ifdef CITY_FAITH
						if(pTownDetail->ptFaith[gameCITY_SACRIFICE_GUARD] == gameCITY_SACRIFICE_MAX)//?H???F???????
							GetServer()->m_nCityGuard.CityGuardProc_DeleteGen(pCGSetData->cgsRole2[role_idx], pData->nData2);
						else
#endif
							GetServer()->m_nCityGuard.CityGuardProc_DeleteGen(pCGSetData->cgsRole[role_idx], pData->nData2);
					}
					else
#ifdef CITY_FAITH
						if(pTownDetail->ptFaith[gameCITY_SACRIFICE_GUARD] == gameCITY_SACRIFICE_MAX)//?H???F???????
							GetServer()->m_nCityGuard.CityGuardProc_InsertGen(pCGSetData->cgsRole2[role_idx], pData->nData2, newGuardPos);
						else
#endif
							GetServer()->m_nCityGuard.CityGuardProc_InsertGen(pCGSetData->cgsRole[role_idx], pData->nData2, newGuardPos);
				}
				else
					GetServer()->Log("ERROR: Update city guard but no data(%d,%d,%d)", pData->nData2, pData->nData3, pData->nData4);
				// ......................
				//?q?????D
				pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );

				if( pMapPlayer )
				{
					pMsg			= pMapPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
					
					pMsg->m_Msg.m_UpdateData2Msg.Reset();
					pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_CITYDATA_GUARD_POS, (short)pData->nData2, role_idx, newGuardPos);

					pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
				}
			}
		}
		break;

	case TYPE_PACK_UPDATE_GOLD: // 穝ず窥计

		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData2);
		
		if( pTownDetail == NULL )
		{
			GetServer()->Log( "ERROR: CB_MapCityDataPack update gold: city detail data is null" );
			return;
		}

		pTownDetail->ptCityGold = pData->nData3;
		pTownDetail->ptCityGoldCount = pData->nData4;
		//?q??client?A?p?G????n????
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );

		if( pMapPlayer )
		{
			pMsg			= pMapPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
			
			pMsg->m_Msg.m_UpdateData2Msg.Reset();
			pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_CITYDATA_UPDATE_GOLD, (short)pData->nData2, pData->nData4, pData->nData3);

			pMsg->m_nSize = (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
		}

		break;
	case TYPE_PACK_UPDATE_GOLD_RESET:
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData1);
		if( pTownDetail == NULL )
			return;
		pTownDetail->ptCityGoldCount = 0;
		break;
	case TYPE_PACK_BOT_ENEMY_STATUS:
		if (pData->nData1)
		{
//GetServer()->Log("BOT: Start");
			CMapServer::GetServer()->IsBotEnemyCheck = true;
		}
		else
		{
//GetServer()->Log("BOT: Stop");
			CMapServer::GetServer()->IsBotEnemyCheck = false;
		}
		break;
	case TYPE_PACK_UPDATE_GOLD_ARMY_NOTIFY:
//GetServer()->Log("CITY: %d, %d, %d", pData->nData1, pData->nData2, pData->nData3);
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
		if (pMapPlayer)
		{
			i = ::gameStageGetStageNameID(pData->nData2);
			if (i)
			{
				if (!pData->nData4)		// ????
				{
					wsprintf(tmpstr, gameGetResourceName(24178), gameGetResourceName(i), pData->nData3);
				}
				else					// ?s??
					wsprintf(tmpstr, gameGetResourceName(24324), gameGetResourceName(i), pData->nData3);
				pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
			}
		}
		break;
	case TYPE_PACK_CHANGE_SINGLE_PLAYER_FORCE:	// Login 硄穝┮Τ產墩
		GetServer()->ChangeAllPlayersForce(pData->nData1, pData->nData2, pData->nData3);
		break;

	case TYPE_PACK_ARMY_SETUP_FORCE: //ミ墩
#ifndef NO_ARMY_SETUP_FORCE
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );

		if( pMapPlayer == NULL )
		{
			GetServer()->Log( "ERROR: CB_MapCityDataPack setup force: map player is null" );
			return;
		}

		if( pData->nData3 == 0 ) //login?????\???
		{
			GetServer()->Log( "ERROR: CB_MapCityDataPack setup force: 產(%s,%u) login ぃす砛ミ", pMapPlayer->GetName(), pMapPlayer->GetUID() );
			return;
		}

		if( pMapPlayer->GetGold() < gameARMY_SETUP_FORCE_GOLD )
		{
			GetServer()->Log( "ERROR: CB_MapCityDataPack setup force: 產(%s,%u) login ぃす砛ミ", pMapPlayer->GetName(), pMapPlayer->GetUID() );
			return;
		}

		if( !pMapPlayer->IsItemEnough( gameITEM_ID_SETUP_FORCE_01, 1 ) || 
			!pMapPlayer->IsItemEnough( gameITEM_ID_SETUP_FORCE_02, 1 ) || 
			!pMapPlayer->IsItemEnough( gameITEM_ID_SETUP_FORCE_03, 1 ) )
		{
			GetServer()->Log( "ERROR: CB_MapCityDataPack setup force: 產(%s,%u) login ぃす砛ミ", pMapPlayer->GetName(), pMapPlayer->GetUID() );
			return;
		}

		hLoginServer = GetServer()->m_hLoginServer;
		if( GetServer()->IsConnectedToServer(hLoginServer) && pMapPlayer->CanSaveData() )
		{
			pMapPlayer->GetSaveData()->plrGold -= gameARMY_SETUP_FORCE_GOLD;
			pMapPlayer->SaveCharData();
			pMapPlayer->UpdateClientGoldAndWeight();

			pMapPlayer->DelItemAndUpdateClient( gameITEM_ID_SETUP_FORCE_01, 1, LOGTYPE_ITEM_SETUP_FORCE_ITEM );
			pMapPlayer->DelItemAndUpdateClient( gameITEM_ID_SETUP_FORCE_02, 1, LOGTYPE_ITEM_SETUP_FORCE_ITEM );
			pMapPlayer->DelItemAndUpdateClient( gameITEM_ID_SETUP_FORCE_03, 1, LOGTYPE_ITEM_SETUP_FORCE_ITEM );

			struct LOGIN_ARMY_SETUP_FORCE sendmsg;
			::memset( &sendmsg, 0, sizeof(sendmsg) );
			sendmsg.nPlayerUID = pData->nData1;
			sendmsg.nOrgUID = pMapPlayer->GetArmyUID();
			sendmsg.nMapCode = pMapPlayer->GetMapCode();
			sendmsg.nCountryID = (unsigned char)pData->nData2;
			::memcpyT( sendmsg.szCountryName, &pData->nData4, 2 );
			// ????O???W
			// ?x??b@7%s@1????s??O?A??@5%s@1?C
			wsprintf( sendmsg.szSPMessage, gameGetResourceName(21512), gameGetResourceName(gameStageGetStageNameID(pMapPlayer->GetMapCode())), sendmsg.szCountryName);
			GetServer()->SendData( hLoginServer, pComm->pcUID, PROTO_LOGIN_ARMY_SETUP_FORCE, (char*)&sendmsg, sizeof(sendmsg), 0 );

			::SendGoldLog( gameARMY_SETUP_FORCE_GOLD, pMapPlayer, LOGTYPE_ITEM_SETUP_FORCE_GOLD, NULL );
			// ????O???W
			CMapServer::GetServer()->GetMapManager()->BroadcastSPMessage(-2, gameGetResourceName(24318), gameSPMSG_WAV_PLAYERTALK, pMapPlayer, 1);
		}
#endif
		break;
	case TYPE_PACK_WEB_POINT_RESULT:
#ifdef ItemMode
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
		if( pMapPlayer == NULL )
		{
			GetServer()->Log( "ERROR: CB_MapCityDataPack Update web points: map player is null" );
			return;
		}
		//
		switch (pData->nData2)	// 1 = 琩高ok, 0 = ぃㄏノ(翴计ぃì), 2 = 狝竟岿粇, 3 = Ι翴ok
		{
		case 1:
		case 3:
			break;
		case 0:
			pMapPlayer->SendMsgToClientByID( 20862 );
			return;
			break;
		case 2:
			CMapServer::GetServer()->Log("ERROR: Get web point result(%d)", pData->nData2);
			return;
			break;
		case 20:			// ??e???????n?D
			pMapPlayer->SendMsgToClientByID( 20863 );
			return;
			break;
		default:
			return;
			break;
		}
		//
		pMsg = pMapPlayer->InsertMsg(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, CMapCharMsg::SEND_TO_SELF);
		
		pMsg->m_Msg.m_UpdateData2Msg.Reset();
		pMsg->m_Msg.m_UpdateData2Msg.Add(UPART2_TYPE_UPDATE_WEB_POINTS, 0, pData->nData3);

		pMsg->m_nSize	= (short)pMsg->m_Msg.m_UpdateData2Msg.GetSize();
#endif
		break;
	case TYPE_PACK_ADD_CHAR_SLOT:	// Login ?T?{ ok
	case TYPE_PACK_ADD_MALL_POINT:	// Login ?T?{ ok
#ifdef ItemMode
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
		if (pMapPlayer)
		{
			//if (pMapPlayer->FindItemAndDelete(LOWORDPTR(pData->nData3)))	// ?w????
			{
				pData->nData2 = 1;		// mode
				CMapServer::GetServer()->SendData( CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, pBuffer, nLen, 0 );
			}
		}
#endif
		break;
	case TYPE_PACK_DELETE_PLAYER_FACE_CACHE:
		if (pData->nData1)
		{
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("CityPack: Delete player face cache(%d)", pData->nData1);
#endif
			CMapServer::GetServer()->DeletePlayerFaceCache(pData->nData1);
		}
		break;
	case TYPE_PACK_NOTIFY_VTITEM_SUPER_MODE:		// ??s?]?w
		g_Server_Setting = pData->nData1;
		//
		if (CMapServer::GetServer()->IsCrossServer())		// 琌阁狝竟Server
			CMapServer::GetServer()->Log("Server mode: Cross Server");
		if (CMapServer::GetServer()->IsCrossCWarServer())	// ?O????A?????Server
			CMapServer::GetServer()->Log("Server mode: Cross CWar Server");
		break;
	case TYPE_PACK_RESET_SPEC_CN_MODE_TIME:
		CMapServer::GetServer()->Log("CityPack: Get reset spec cn mode time(%d)", pData->nData1);
		CMapServer::GetServer()->m_nResetSpecCNModeTime = pData->nData1;
		CMapServer::GetServer()->ResetAllPlayer_SpecCNModeTime();
		break;
	case TYPE_PACK_LOGIN_NOTIFY:
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
		if (pMapPlayer)
		{
			switch(pData->nData2)
			{
			case 0:				// ?????l pData->nData3 ???????
				if (pData->nData3)
				{
					if (pData->nData3 == -1)	// ボ丁⊿逼钉
					{		// ???????????A??e?L?????a????}??A?t??|?????p??????????I
						strcpy(tmpstr, gameGetResourceName(24387));
					}
					else	// ?????l%d???????????I
					{
						wsprintf(tmpstr, gameGetResourceName(24386), pData->nData3);
					}
					pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
				}
				break;
			case 1:				// ?????????????
				// ????Q?t???????I
				strcpy(tmpstr, gameGetResourceName(24388));
				pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
				break;
			case 2:				// ????????G(?????)
				// PROTO_LOGIN_OPEN_CHATROOM_RESULT
				break;
			case 3:				// ?q????????, ?A???}???????????O%d?I
				wsprintf(tmpstr, gameGetResourceName(24389), pData->nData3);
				pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
				break;
			}
		}
		break;
	case TYPE_PACK_MAP_SPACE_READY:			// d1= enter map code, d2=map code, d3=status, d4=mode
		if (!pData->nData4)		// ?]?w?i?J???A
		{
			CMapServer::GetServer()->GetMapSpaceManager()->NotifyReadyStatus(pData->nData2, pData->nData3);
		}
		else					// ?]?w?}?l
		{
			if (nLen == sizeof(MAP_CITY_DATA_PACK_EX_SPACE))
			{
				MAP_CITY_DATA_PACK_EX_SPACE* pex = (MAP_CITY_DATA_PACK_EX_SPACE*)pData;
				CMapServer::GetServer()->GetMapSpaceManager()->MapSpace_SetRunModeStart(pData->nData2, pex->second, gameMAX_PARTY_PLAYER);
			}
			else
			{
 #ifndef USE_PACKAGE_DATA
				CMapServer::GetServer()->Log("秈捌セΤぃ才戈挡篶痹瓃");
 #endif
			}
		}
		break;
	case TYPE_PACK_CLEAR_MAIL:				// d1=player uid, d2=account id
 #ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("CityPack Debug: Delete player notify(%d,%d)", pData->nData1, pData->nData2);
 #endif
		MailDelete(pData->nData1);
		break;
	case TYPE_PACK_GET_CWAR_REWARD:			// d1=player uid(to Login: d2=type,0?d??,1?T?{ok)(to MAP:d2=honor billing, d3=war king score)
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
		if (pMapPlayer)
		{
			struct MAP_CITY_DATA_PACK nCityPack;
// #ifdef ItemMode
//			struct LOGIN_ADD_PLAYER_WEB_POINTS nAddWebPoints;
//			//struct LOG_INSERT_MSG_ACT nLogAct;
// #endif

			pMapPlayer->m_nPlayerFlags &= ~PLAYER_FLAGS_GET_CWAR_REWARD;
			//
			if (pMapPlayer->CanSaveData())
			{
				if (pData->nData2 == -1)		// ?M???^??
					break;

				val = 0;					// ???T?w??n???~???
				if (pData->nData2)
					++val;
				if(pData->nData3)
					++val;

	#ifndef USE_PACKAGE_DATA
				GetServer()->Log("DEBUG: Get CWAR reward(%d,%d)", pData->nData2, pData->nData3);
	#endif
				if (!val)
					break;
				if (!pMapPlayer->CarryItem_CheckFreeSpace(val))
				{
					wsprintf(tmpstr, gameGetResourceName(24273), val);
					pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
					break;
				}
				//
				val = 0;
				if (pData->nData2)			// ?\????W
					val = InnerGetCWarRewardNum(pData->nData2);
				
				val2 = 0;
				if(pData->nData3)
					val2 = InnerGetCWarRewardKingNum(pData->nData2, pData->nData3);

				if (val)
				{
				// ?S??????b
#ifdef USE_NEW_CWAR_LIST_TIME_GIFT
					if (!pData->nData4)
					{
						val = val >> 1;
						if (!val)
							val++;
					}
#endif
				}

				// ?????~
				PlayerDrop_Init();
				if(val)
				{
					if (pMapPlayer->CarryItem_MakeItemOnFreeSpace(gameITEM_ID_CWAR_REWARD, val, LOGTYPE_ITEM_FROM_NPC_GET, 1, pMapPlayer, 1))
						val = 1;
					else
						break;
				}

				if(val2)
				{
					if (pMapPlayer->CarryItem_MakeItemOnFreeSpace(gameITEM_ID_CWAR_KING_REWARD, val2, LOGTYPE_ITEM_FROM_NPC_GET, 1, pMapPlayer, 1))
						val = 2;
				}

				if(!val)
					break;

				// WarProject direct rewards (parallel with original item rewards)
				{
					long directExp = (long)pMapPlayer->m_nCWarRewardExp;
					long directDragon = (long)pMapPlayer->m_nCWarRewardDragonSilver;
					long directToken = (long)pMapPlayer->m_nCWarRewardToken;
					long directHonor = (long)pMapPlayer->m_nCWarRewardHonor;

					if (directExp > 0)
						pMapPlayer->AddExp((unsigned long)directExp, pMapPlayer);
					if (directDragon > 0)
						pMapPlayer->AddKSScore((unsigned long)directDragon, 0);
					if (directHonor > 0)
						pMapPlayer->AddHonor((unsigned long)directHonor);

#ifdef ItemMode
					if (directToken > 0 && GetServer()->IsConnectedToServer(GetServer()->m_hLoginServer))
					{
						struct LOGIN_ADD_PLAYER_WEB_POINTS nAddWebPoints;
						::memset(&nAddWebPoints, 0, sizeof(nAddWebPoints));
						nAddWebPoints.nPlayerUID = pMapPlayer->GetUID();
						nAddWebPoints.nGold = directToken;
						msg_strncpy(nAddWebPoints.szAccount, pMapPlayer->GetSaveData()->plrAccount, sizeof(nAddWebPoints.szAccount));
						GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_ADD_PLAYER_WEB_POINTS, (char *)&nAddWebPoints, sizeof(nAddWebPoints), 0);
					}
#endif
				}

				pMapPlayer->m_nCWarRewardExp = 0;
				pMapPlayer->m_nCWarRewardDragonSilver = 0;
				pMapPlayer->m_nCWarRewardToken = 0;
				pMapPlayer->m_nCWarRewardHonor = 0;

				pMapPlayer->SendMsgToClientByID(21300);
				PlayerDrop_SendMissionResult(pMapPlayer);
// #endif
				//
				::memset( &nCityPack, 0, sizeof(nCityPack) );
				nCityPack.nType = TYPE_PACK_GET_CWAR_REWARD;
				nCityPack.nData1 = pMapPlayer->GetUID();
				nCityPack.nData2 = val;		// ?T?{ok
				GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
			}
		}
		break;
	case TYPE_PACK_ACCOUNT_DUEDATE_NOTIFY:
#ifdef USE_DUEDATE_NOTIFY
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
		if (pMapPlayer)
		{
			nMsg.Reset();
			nMsg.Add(UPART2_TYPE_DUEDATE_NOTIFY, 0, pData->nData2);

			::msgSendData( pMapPlayer->GetClientID(), 0, PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char*)&nMsg, nMsg.GetSize(), 0 );
		}
#endif
		break;
	case TYPE_PACK_PLAYER_UPDATE_MATE:		// ㄑ瞒盉ノd1=player uid, d2=mate uid 穝皌案uid, d3=喷靡皌案
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
		if (pMapPlayer)
		{
			if (pData->nData3)
			{
				if (pMapPlayer->GetSaveData()->plrMarry_UID != pData->nData3)
				{
					CMapServer::GetServer()->Log("ERROR: Update mate info(%s,%d,%d)", pMapPlayer->GetName(), pMapPlayer->GetSaveData()->plrMarry_UID, pData->nData3);
					return;
				}
			}
			//
			if (pData->nData2 != 0xffffffff)
				CMapServer::GetServer()->Log("ERROR: Update mate info data incorrect(%s,%d)", pMapPlayer->GetName(), pData->nData2);
			pMapPlayer->ProcDivorce();
		}
		break;
	case TYPE_PACK_SELF_BOOM:
		GetServer()->StartSelfBoom(pData->nData1);
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("Start bm time(%d)", pData->nData1);
#endif
		break;
	case TYPE_PACK_KS_REDSCORE_NOTIFY:	// d1=uid, d2=rscore
#ifdef CROSS_SERVER_SYSTEM
		if (CMapServer::GetServer()->IsCrossServer(1))		// ?O????A??Server
		{
			CMapServer::GetServer()->Log("ERROR: KS rscore notify mistake(%d,%d)", pData->nData1, pData->nData2);
		}
		else
		{
			pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
			if (pMapPlayer)
			{
				if (pMapPlayer->CheckPlayerNoSave())
					break;
				if (!pMapPlayer->IsReady())
					break;
				if (pMapPlayer->IsLoadingData())
					break;
				if (CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hLoginServer))
				{
					struct MAP_CITY_DATA_PACK nCityPack;

					::memset( &nCityPack, 0, sizeof(nCityPack) );
					nCityPack.nType = TYPE_PACK_KS_REDSCORE_NOTIFY_RESULT;
					nCityPack.nData1 = pData->nData1;
					nCityPack.nData2 = pData->nData2;
					GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
 #ifndef USE_PACKAGE_DATA
					CMapServer::GetServer()->Log("CityPack Debug: Delete player notify(%d,%d)", pData->nData1, pData->nData2);
 #endif
					//
					pMapPlayer->AddKSScore(0, pData->nData2);

					//
					wsprintf(tmpstr, gameGetResourceName(5000003), pData->nData2);		// 眖阁狝竟驹初い羆莉眔 %d 翴笴栏縩だ
					pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
					
					//
					::SendGoldLog( (unsigned long)pData->nData2, pMapPlayer, LOGTYPE_ITEM_KS_GET_REDSCORE, 0 ); //???log
				}
			}
		}
#endif
		break;
case TYPE_PACK_KS_SCORE_WAR_NOTIFY:	// d1=uid, d2=score
#ifdef CROSS_SERVER_SYSTEM
		if (CMapServer::GetServer()->IsCrossServer(1))		// ?O????A??Server
		{
			CMapServer::GetServer()->Log("ERROR: KS score notify mistake(%d,%d)", pData->nData1, pData->nData2);
		}
		else
		{
			pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
			if (pMapPlayer)
			{
				if (pMapPlayer->CheckPlayerNoSave())
					break;
				if (!pMapPlayer->IsReady())
					break;
				if (pMapPlayer->IsLoadingData())
					break;
				if (CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hLoginServer))
				{
					struct MAP_CITY_DATA_PACK nCityPack;

					::memset( &nCityPack, 0, sizeof(nCityPack) );
					nCityPack.nType = TYPE_PACK_KS_SCORE_WAR_NOTIFY_RESULT;
					nCityPack.nData1 = pData->nData1;
					nCityPack.nData2 = pData->nData2;
					GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
 #ifndef USE_PACKAGE_DATA
					CMapServer::GetServer()->Log("CityPack Debug: Delete player notify(%d,%d)", pData->nData1, pData->nData2);
 #endif
					//
					pMapPlayer->AddKSScore(pData->nData2, 0);
					
					//
					wsprintf(tmpstr, gameGetResourceName(5000246), pData->nData2);		// 眖阁狝竟瓣驹い羆莉眔 %d 翴纒蝗
					pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
									
					//
					::SendGoldLog( (unsigned long)pData->nData2, pMapPlayer, LOGTYPE_ITEM_KS_GET_SCORE, 0 ); //???log
				}
			}
		}
#endif
		break;
	case TYPE_PACK_KS_SCORE_HISTORY_NOTIFY:	// d1=uid, d2=score
#ifdef CROSS_SERVER_SYSTEM
		if (CMapServer::GetServer()->IsCrossServer(1))		// ?O????A??Server
		{
			CMapServer::GetServer()->Log("ERROR: KS score notify mistake(%d,%d)", pData->nData1, pData->nData2);
		}
		else
		{
			pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nData1 );
			if (pMapPlayer)
			{
				if (pMapPlayer->CheckPlayerNoSave())
					break;
				if (!pMapPlayer->IsReady())
					break;
				if (pMapPlayer->IsLoadingData())
					break;
				if (CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hLoginServer))
				{
					struct MAP_CITY_DATA_PACK nCityPack;

					::memset( &nCityPack, 0, sizeof(nCityPack) );
					nCityPack.nType = TYPE_PACK_KS_SCORE_HISTORY_NOTIFY_RESULT;
					nCityPack.nData1 = pData->nData1;
					nCityPack.nData2 = pData->nData2;
					GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
 #ifndef USE_PACKAGE_DATA
					CMapServer::GetServer()->Log("CityPack Debug: Delete player notify(%d,%d)", pData->nData1, pData->nData2);
 #endif
					//
					pMapPlayer->AddKSScore(pData->nData2, 0);

					//
					wsprintf(tmpstr, gameGetResourceName(5000245), pData->nData2);		// 眖阁狝竟驹初い羆莉眔 %d 翴纒蝗
					pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
					
					//
					::SendGoldLog( (unsigned long)pData->nData2, pMapPlayer, LOGTYPE_ITEM_KS_GET_SCORE, 0 ); //???log
				}
			}
		}
#endif
		break;
	case TYPE_PACK_ADD_FAITH:		// 步(d1=uid, d2=翴计, d3=city id, d4=獺ヵmode)
#ifdef CITY_FAITH
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData3);
		if (pTownDetail)
		{
			pTownDetail->ptFaith[pData->nData4] = (unsigned char)pData->nData2;

			// ?q????????a
			nMsg.Reset();
			nMsg.Add(UPART2_TYPE_CITYDATA_FAITH, (short)pData->nData2, pData->nData3, pData->nData4); //?I?? //city id // ?H??mode
			
			CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsg.GetSize(), 1);

			if(pData->nData4 == gameCITY_SACRIFICE_GUARD && pData->nData2 == gameCITY_SACRIFICE_MAX )
				GetServer()->m_nCityGuard.CityGuardProc_UpdateGen(pData->nData3);
		}
#endif
		break;
	case TYPE_PACK_RESET_FAITH:		// 獺ヵ竚(nData1 = Map Code)
#ifdef CITY_FAITH
		pTownDetail = CMapServer::GetServer()->GetTownDetailDataByID(pData->nData1);
		if (pTownDetail)
		{
			pTownDetail->ptFaith[gameCITY_SACRIFICE_STATUE] = 0;
			pTownDetail->ptFaith[gameCITY_SACRIFICE_GATE] = 0;
			pTownDetail->ptFaith[gameCITY_SACRIFICE_GUARD] = 0;
			GetServer()->m_nCityGuard.CityGuardProc_UpdateGen2(pData->nData1);
			// ?q????????a
			nMsg.Reset();
			nMsg.Add(UPART2_TYPE_CITYDATA_FAITH_RESET, 0, pData->nData1); // city id

			CMapServer::GetServer()->SendMessageToAllPlayers(PROTO_MAP_UPDATE_PLAYER_DATA_PART2, (char *)&nMsg, nMsg.GetSize(), 1);
		}
#endif
		break;
	}
}

#ifdef TW_PVP_MODE
// TW PVP ??W???G
void CMapServer::CB_AskTWPVPListResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_ASK_TW_PVP_LIST_RESULT * pData;
	CMapPlayer * pMapPlayer;

	pData = (struct MAP_ASK_TW_PVP_LIST_RESULT *)pBuffer;
	pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nPlayerUID );
	if (pMapPlayer)
	{
		::msgSendData( pMapPlayer->GetClientID(), 0, PROTO_MAP_ASK_TW_PVP_LIST_RESULT, pBuffer, nLen, 0 );
	}
}
#endif

void CMapServer::CB_ClientPackData(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_CLIENT_PACK_DATA * pData;
	CMapPlayer * pPlayer;
	CMapPlayer * pDest;
	unsigned long uid;

#if (defined(SETUP_FORCE_CONTROL) || defined(TW_PVP_MODE))
	struct MAP_CITY_DATA_PACK nCityPack;
#endif
	struct plrDATA_WORLD_TOWN_DATA * pTown;
	struct gs_StageData * pStage;
	unsigned long long gold;
	char tmpstr[256];

	pPlayer = GetServer()->FindPlayerByClientID(nID);
	if (!pPlayer)
		return;
	//
	pData = (struct MAP_CLIENT_PACK_DATA *)pBuffer;
	switch(pData->nType)
	{
	case CP_TYPE_ASK_MERRY_RESPONSE:
		if (!(pPlayer->m_nPlayerFlags & PLAYER_FLAGS_WAIT_MERRY))
		{
 #ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG ERROR: Get ask merry response status incorrect(%s)", pPlayer->GetName());
 #endif
			return;
		}
		// pDest = ?D?B??
		pDest = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(pPlayer->m_nMerryPlayerUID);
		if (!pDest)
		{
 #ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG ERROR: Merry target disappear(%s)", pPlayer->GetName());
 #endif
			return;
		}
		if (!(pDest->m_nPlayerFlags & PLAYER_FLAGS_WAIT_MERRY))
		{
 #ifndef USE_PACKAGE_DATA
			CMapServer::GetServer()->Log("DEBUG ERROR: Merry target status incorrect(%s)", pPlayer->GetName());
 #endif
			return;
		}
		if (pDest->PartyGetTotal() != 2)
		{
fail_merry:
			pDest->ProcWaitMerryResponseFail();
			return;
		}
		uid = pDest->PartyGetMemberByPos(0);
		if (uid == pDest->GetUID())
			uid = pDest->PartyGetMemberByPos(1);
		if (!uid || uid != pPlayer->GetUID())
			goto fail_merry;
		if ((pPlayer->m_nMerryPlayerUID != pDest->GetUID()) || (pDest->m_nMerryPlayerUID != pPlayer->GetUID()))
			goto fail_merry;
		if (!pPlayer->IsReady() || !pDest->IsReady())
			goto fail_merry;
		if (!pPlayer->CanSaveData() || !pDest->CanSaveData())
			goto fail_merry;
		if (pPlayer->GetSaveData()->plrMarry_UID || pDest->GetSaveData()->plrMarry_UID)
			goto fail_merry;
		if (pData->nData1 != 1)
			goto fail_merry;
		//
		pPlayer->ProcMerrySuccess(pDest);
		break;
	case CP_TYPE_ASK_TW_PVP_LIST:		// TW PVP ??W
#ifdef TW_PVP_MODE
		::memset( &nCityPack, 0, sizeof(nCityPack) );
		nCityPack.nType = TYPE_PACK_ASK_TW_PVP_LIST;
		nCityPack.nData1 = pPlayer->GetUID();
		GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
#endif
		break;
	case CP_TYPE_FORCE_CHANGE_RESPONSE:		// ??O??e
		if (pPlayer->m_nPlayerFlags & PLAYER_FLAGS_WAIT_FORCE_CHG)	// 墩肚癳单莱篈
		{
			pPlayer->m_nPlayerFlags &= ~PLAYER_FLAGS_WAIT_FORCE_CHG;
			//
			if (!pPlayer->IsReady())
				return;
			//if (GetServer()->IsWar())		// ?????????
			//	return;
			pTown = CMapServer::GetServer()->GetTownDataByID(pData->nData1, 1);
			if (!pTown)
				return;
			if (pTown->ptCountryID != pPlayer->GetSaveData()->plrCountryID)
				return;
			//
			pStage = gameStageGetPtr(pData->nData1);
			if (!pStage)
				return;
			if (!pStage->gstTeleportPosX || !pStage->gstTeleportPosY)
				return;
			// ????
			gold = game_FORCE_TELEPORT_GOLD_NEED;
			if (pPlayer->GetSaveData()->plrGold >= gold)
			{
				pPlayer->GetSaveData()->plrGold -= gold;
				pPlayer->UpdateClientGoldAndWeight();
				pPlayer->AutoSaveCharData();
				//
				::SendGoldLog( gold, pPlayer, LOGTYPE_ITEM_FORCE_TELEPORT_GOLD, 0 ); //???log
				// ?A?I?X?F%d??C
				wsprintf(tmpstr, gameGetResourceName(24130), gold);
				pPlayer->SendMessage( gameChannel_System, NULL, tmpstr );
				//
				pPlayer->ChangeMap(pData->nData1, pStage->gstTeleportPosX, pStage->gstTeleportPosY);
			}
			else
				pPlayer->SendMsgToClientByID(20038);	// ?A???????????I
		}
		break;
	case CP_TYPE_PACK_HELPACTSEND:
		if (!IsValidActSendMap(pData->nData1))
			break;
		if (CMapServer::GetServer()->IsMapWar(pData->nData1, 1))
			break;
		if (pPlayer->GetSaveData()->plrGold < 100)
			break;
		pPlayer->GetSaveData()->plrGold -= 100;
		pPlayer->UpdateClientGoldAndWeight();
		pPlayer->AutoSaveCharData();

		pPlayer->ChangeMap(pData->nData1, pData->nData2 * gameIconSize, pData->nData3 * gameIconSize);
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("Start CP_TYPE_PACK_HELPACTSEND(%d,%d,%d)", pData->nData1, pData->nData2, pData->nData3);
#endif
		break;
#ifdef SETUP_FORCE_CONTROL
	case CP_TYPE_PACK_GET_JN_PLAYER_LIST:		// ミ墩眔瓣ビ叫虫
		::memset( &nCityPack, 0, sizeof(nCityPack) );
		nCityPack.nType = TYPE_PACK_GET_JN_PLAYER_LIST;			// d1=player uid, d2=mate uid 穝皌案uid, d3=喷靡皌案
		nCityPack.nData1 = pPlayer->GetUID();
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0);
		break;
	case CP_TYPE_PACK_ALLOW_JN_PLAYER:			// ミ墩∕﹚琌種瓣砛
		::memset( &nCityPack, 0, sizeof(nCityPack) );
		nCityPack.nType = TYPE_PACK_ALLOW_JN_PLAYER;			// d1=player uid, d2=allow
		nCityPack.nData1 = pPlayer->GetUID();
		nCityPack.nData2 = pData->nData1;
		nCityPack.nData3 = pData->nData2;
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0);
		break;
#endif
	}
}

// ?? Login ?o?X?T???q?? Player?A?r?????n?q GameResource ???o
void CMapServer::CB_LoginToMapMsgInfo(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	CMapPlayer * pMapPlayer;
	struct MAP_LOGIN_TO_MAP_MSG_INFO * pData;
	char tmpstr[512];
	//
	struct MAP_CITY_DATA_PACK nCityPack;

	pData = (struct MAP_LOGIN_TO_MAP_MSG_INFO *)pBuffer;
	//
	pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->uid );
	if ( pMapPlayer )
	{
		tmpstr[0] = 0;
		switch(pData->nInfo.nType)
		{
		case L_TO_M_TYPE_MATE_ONLINE:			// ?t???W?u
			// ????t??????
			if (pMapPlayer->GetSaveData()->plrMarry_UID != pData->nInfo.nData1)
			{
				// ??????t??????
				::memset( &nCityPack, 0, sizeof(nCityPack) );
				nCityPack.nType = TYPE_PACK_PLAYER_UPDATE_MATE;			// d1=player uid, d2=mate uid 穝皌案uid, d3=喷靡皌案
				nCityPack.nData1 = pData->nInfo.nData1;
				nCityPack.nData2 = 0xffffffff;	// ボア皌案
				nCityPack.nData3 = 0;			// ぃ斗喷靡
				CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0);
 #ifndef USE_PACKAGE_DATA
				CMapServer::GetServer()->Log("Fix mate data: %s(%d,%d)", pMapPlayer->GetName(), pMapPlayer->GetSaveData()->plrMarry_UID, pData->nInfo.nData1);
 #endif
			}
			else
				wsprintf(tmpstr, gameGetResourceName(pData->nInfo.nMsgID), pData->nInfo.szStr1);
			break;
		case L_TO_M_TYPE_MATE_OFFLINE:			// 皌案絬(ゼ龟暗)
			wsprintf(tmpstr, gameGetResourceName(pData->nInfo.nMsgID), pData->nInfo.szStr1);
			break;
		case L_TO_M_TYPE_CROSS_CWAR_RSCORE:
			wsprintf(tmpstr, gameGetResourceName(pData->nInfo.nMsgID), pData->nInfo.nData2);	// 5000024	莉眔阁狝竟墩縩だ%d翴
			break;
		}
		//
		if (tmpstr[0])
			pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
	}
}

void CMapServer::CB_MapSendMsgToClientByID(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_SEND_MSG_BY_ID_TO_CLIENT * p;

	p = (struct LOGIN_SEND_MSG_BY_ID_TO_CLIENT *)pBuffer;

	if( !p )
		return;

	CMapPlayer * pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( p->playerUID );

	if( pMapPlayer )
	{
		pMapPlayer->SendMsgToClientByID( p->msgData.msg_id, p->msgData.msg_type );
	}
}

void CMapServer::CB_MapSendMsgToClientByIDAndTime(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_SEND_MSG_BY_ID_AND_TIME_TO_CLIENT * p;

	p = (struct LOGIN_SEND_MSG_BY_ID_AND_TIME_TO_CLIENT *)pBuffer;

	if( !p )
		return;

	CMapPlayer * pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( p->playerUID );

	if( pMapPlayer )
	{
		pMapPlayer->SendMsgAndTimeToClientByID( p->msgData.msg_id, p->msgData.msg_time, p->msgData.msg_type );
	}
}

void CMapServer::CB_MapUseRenameItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_USE_RENAME_ITEM * pData;
	CMapPlayer * pMapPlayer;
	struct DB_PLAYER_RENAME nRename;

	pData = (struct MAP_USE_RENAME_ITEM *)pBuffer;
	pMapPlayer = CMapServer::GetServer()->FindPlayerByClientID(nID);
	if (pMapPlayer)
	{
		if (!CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hDBServer))
		{
no_now:		// ??e???????n?D?I
			pMapPlayer->SendMsgToClientByID(20863);
			return;
		}
		// ?O?_?????e?????A
		if ((pMapPlayer->m_nPlayerFlags & PLAYER_FLAGS_RENAME) || !pMapPlayer->IsReady())
			goto no_now;
		//
		if(pData->itemID == gameITEM_ID_RENAME || pData->itemID == gameITEM_ID_RENAME2)
		if (!pMapPlayer->FindItem(pData->itemID))
		{
			pMapPlayer->SendMsgToClientByID(24427);			// ?S?????m?W?????~
			return;
		}
		// ???????A?A????DB?^??
		pMapPlayer->m_nPlayerFlags |= PLAYER_FLAGS_RENAME;
		memset(&nRename, 0, sizeof(nRename));
		nRename.nPlayerUID = pMapPlayer->GetUID();
		nRename.nItemID = pData->itemID;
		msg_strncpy(nRename.szName, pData->szName, sizeof(nRename.szName));
		nRename.nMode = 0;
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hDBServer, 0, PROTO_DB_PLAYER_RENAME, (char *)&nRename, sizeof(nRename), 0);
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("Rename: %s => %s", pMapPlayer->GetName(), pData->szName);
#endif
	}
}

void CMapServer::CB_MapUseRenameItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_PLAYER_RENAME_RESULT * pData;
	CMapPlayer * pMapPlayer;
	struct DB_PLAYER_RENAME nRename;

	pData = (struct DB_PLAYER_RENAME_RESULT *)pBuffer;
	pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nPlayerUID );
	if (pMapPlayer)
	{
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("Rename: %s(%d)", pData->szName, pData->nResult);
#endif
		switch(pData->nResult)
		{
		case gameRENAME_RESULT_OK:
			if (!pMapPlayer->IsReady())
			{
no_now:			pMapPlayer->SendMsgToClientByID(20863);
			}
			else if (!pMapPlayer->FindItem(pData->nItemID))
			{
				pMapPlayer->SendMsgToClientByID(24427);			// ?S?????m?W?????~
			}
			else
			{
				if (pMapPlayer->FindItemAndDelete(pData->nItemID))
				{
					// ??sDB?m?W????
					memset(&nRename, 0, sizeof(nRename));
					nRename.nPlayerUID = pMapPlayer->GetUID();
					nRename.nItemID = pData->nItemID;
					msg_strncpy(nRename.szName, pData->szName, sizeof(nRename.szName));
					nRename.nMode = 1;
					nRename.nAccountID = pMapPlayer->GetAccountID();
					msg_strncpy(nRename.szOldName, pMapPlayer->GetName(), sizeof(nRename.szOldName));
					CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hDBServer, 0, PROTO_DB_PLAYER_RENAME, (char *)&nRename, sizeof(nRename), 0);
					// ????log
					struct LOG_INSERT_MSG_ACT nLogAct;

					memset(&nLogAct, 0, sizeof(nLogAct));
					nLogAct.nType = LOGTYPE_ACT_RENAME;
					nLogAct.nMapCode = pMapPlayer->GetMapCode();
					nLogAct.nPosX = pMapPlayer->GetPosX();
					nLogAct.nPosY = pMapPlayer->GetPosY();
					nLogAct.nData1 = pData->nItemID;
					::msg_strncpy(nLogAct.nName, pMapPlayer->GetName(), sizeof(nLogAct.nName));
					::msg_strncpy(nLogAct.nStr1, pMapPlayer->GetName(), sizeof(nLogAct.nStr1));
					::msg_strncpy(nLogAct.nStr2, pData->szName, sizeof(nLogAct.nStr1));
					CMapServer::GetServer()->SendLogMessage_Act(&nLogAct);
					// ??s???a????
					msg_strncpy(pMapPlayer->GetSaveData()->plrName, pData->szName, sizeof(pMapPlayer->GetSaveData()->plrName));
					pMapPlayer->SaveCharData();
					//
					pMapPlayer->UpdateShowData();
					pMapPlayer->UpdateClientShowData(0);	// 璶 UpdateShowData ぇ
					pMapPlayer->SendMsgToClientByID(24426);		// ??W???\
					// ?M??GM?u?? DB?q??LoginServer ???m?W
					struct DB_CHANGE_NAME_NOTIFY nChangeName;

					nChangeName.nResult = 1;
					nChangeName.acc_id = 0;
					nChangeName.nPlayerUID = pData->nPlayerUID;
					nChangeName.nOrganizeUID = pMapPlayer->GetSaveData()->plrOrganizeUID;
					msg_strncpy(nChangeName.szName, pData->szName, sizeof(nChangeName.szName));
					CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_DB_CHANGE_NAME_NOTIFY, (char *)&nChangeName, sizeof(nChangeName), 0);
				}
				else
					goto no_now;
			}
			break;
		case gameRENAME_RESULT_ALREADY_EXIST:				// ?w?s?b
			pMapPlayer->SendMsgToClientByID(20071);			// à︹嘿竒Τㄏノ
			break;
		case gameRENAME_RESULT_SYSTEM_BUSY:
			goto no_now;
			break;
		case gameRENAME_RESULT_NAME_ERROR:					// ?m?W???~
		default:
			char tmpstr[256];								// ?m?W??????2??14??r???A?B???i?t??%s?r??????(?t????)?C

			wsprintf(tmpstr, gameGetResourceName(20017), mapInvalidNameChar);
			pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
			break;
		}
		//
		pMapPlayer->m_nPlayerFlags &= ~PLAYER_FLAGS_RENAME;
	}
}

void CMapServer::CB_MapUseGetPlayerPosItem(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct USE_GET_PLAYER_POS_ITEM * pData;
	CMapPlayer * pMapPlayer;
	struct LOGIN_GET_PLAYER_POS nGetPosData;

	pData = (struct USE_GET_PLAYER_POS_ITEM *)pBuffer;
	pMapPlayer = CMapServer::GetServer()->FindPlayerByClientID(nID);
	if (pMapPlayer)
	{
		if (!CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hLoginServer))
		{
no_now:		// ??e???????n?D?I
			pMapPlayer->SendMsgToClientByID(20863);
			return;
		}
		// ?O?_?????e?????A
		if ((pMapPlayer->m_nPlayerFlags & PLAYER_FLAGS_GET_PLAYER_POS) || !pMapPlayer->IsReady())
			goto no_now;
		//
		if (!pMapPlayer->FindItem(gameITEM_ID_GET_PLAYER_POS))
		{
			pMapPlayer->SendMsgToClientByID(24439);			// ō⊿Τㄏノ珇
			return;
		}
		// ???????A?A????DB?^??
		pMapPlayer->m_nPlayerFlags |= PLAYER_FLAGS_GET_PLAYER_POS;
		memset(&nGetPosData, 0, sizeof(nGetPosData));
		nGetPosData.nPlayerUID = pMapPlayer->GetUID();
		nGetPosData.nItemID = gameITEM_ID_GET_PLAYER_POS;
		msg_strncpy(nGetPosData.szName, pMapPlayer->GetName(), sizeof(nGetPosData.szName));
		msg_strncpy(nGetPosData.szTargetName, pData->szName, sizeof(nGetPosData.szTargetName));
		CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_GET_PLAYER_POS, (char *)&nGetPosData, sizeof(nGetPosData), 0);
#ifndef USE_PACKAGE_DATA
		CMapServer::GetServer()->Log("Get Pos: %s => %s", pMapPlayer->GetName(), pData->szName);
#endif
	}
}

void CMapServer::CB_MapUseGetPlayerPosItemResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_GET_PLAYER_POS_RESULT * pData;
	CMapPlayer * pMapPlayer;
	struct USE_GET_PLAYER_POS_ITEM_RESULT nRet;

	pData = (struct LOGIN_GET_PLAYER_POS_RESULT *)pBuffer;
	pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nPlayerUID );
	if (pMapPlayer)
	{
		if (pData->nOK == 1)
		{
			if (pMapPlayer->FindItemAndDelete(pData->nItemID))
			{
				nRet.nMapCode = pData->nMapCode;
				nRet.nPosX = pData->nPosX;
				nRet.nPosY = pData->nPosY;
				nRet.nSubNumber = pData->nSubNumber;
				msg_strncpy(nRet.szName, pData->szTargetName, sizeof(nRet.szName));
				::msgSendData( pMapPlayer->GetClientID(), 0, PROTO_MAP_USE_GET_PLAYER_POS_ITEM_RESULT, (char*)&nRet, sizeof(nRet), 0 );
				// ????log
			/*	struct LOG_INSERT_MSG_ACT nLogAct;

				memset(&nLogAct, 0, sizeof(nLogAct));
				nLogAct.nType = LOGTYPE_ACT_GET_PLAYER_POS;
				nLogAct.nMapCode = pMapPlayer->GetMapCode();
				nLogAct.nPosX = pMapPlayer->GetPosX();
				nLogAct.nPosY = pMapPlayer->GetPosY();
				::msg_strncpy(nLogAct.nName, pMapPlayer->GetName(), sizeof(nLogAct.nName));
				::msg_strncpy(nLogAct.nStr1, pMapPlayer->GetName(), sizeof(nLogAct.nStr1));
				::msg_strncpy(nLogAct.nStr2, pData->szName, sizeof(nLogAct.nStr1));
				CMapServer::GetServer()->SendLogMessage_Act(&nLogAct);
			*/
			}
		}
		//else		// ???s?b?@??W?T???? Login Server ?o?X
		//{
		//}
		//
		pMapPlayer->m_nPlayerFlags &= ~PLAYER_FLAGS_GET_PLAYER_POS;
	}
	// ?q?????
	if (pData->nOK == 1)
	{
		pMapPlayer = (CMapPlayer *)GetServer()->FindObjectByUID( pData->nPlayerTargetUID );
		if (pMapPlayer)
		{
			char tmpstr[256];

			// 24441 ???a %s ??A????????m?\??I
			wsprintf(tmpstr, gameGetResourceName(24441), pData->szName);
			pMapPlayer->SendMessage(gameChannel_System, NULL, tmpstr);
		}
	}
}

void CMapServer::CB_MapUpdateOnlineStatus(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
#ifdef USE_ARMY_FRIEND_ONLINE_FLAG
	struct MAP_UPDATE_ONLINE_STATUS * pData;
	CMapPlayer * pMapPlayer;
	struct LOGIN_UPDATE_ONLINE_STATUS nReq;

	pData = (struct MAP_UPDATE_ONLINE_STATUS *)pBuffer;
	pMapPlayer = CMapServer::GetServer()->FindPlayerByClientID(nID);
	if (pMapPlayer)
	{
		if (CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hLoginServer))
		{
			nReq.nPlayerUID = pMapPlayer->GetUID();
			nReq.nStatus = pData->nStatus;
			CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_LOGIN_UPDATE_ONLINE_STATUS, (char *)&nReq, sizeof(nReq), 0);
		}
	}
#endif
}

void CMapServer::CB_GetWarReward(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_GET_CWAR_REWARD_EX
	{
		unsigned long nRewardExp;
		unsigned long nRewardDragonSilver;
		unsigned long nRewardToken;
		unsigned long nRewardHonor;
	};
	CMapPlayer * pMapPlayer;
	struct MAP_CITY_DATA_PACK nCityPack;

	pMapPlayer = CMapServer::GetServer()->FindPlayerByClientID(nID);
	if (pMapPlayer)
	{
		if (CMapServer::GetServer()->IsConnectedToServer(CMapServer::GetServer()->m_hLoginServer))
		{
			if (!(pMapPlayer->m_nPlayerFlags & PLAYER_FLAGS_GET_CWAR_REWARD))
			{
				const MAP_GET_CWAR_REWARD_EX * pReward = NULL;
				if (nLen >= (int)sizeof(MAP_GET_CWAR_REWARD_EX))
					pReward = (const MAP_GET_CWAR_REWARD_EX *)pBuffer;

				pMapPlayer->m_nCWarRewardExp = pReward ? min(pReward->nRewardExp, 11000000UL) : 0;
				pMapPlayer->m_nCWarRewardDragonSilver = pReward ? min(pReward->nRewardDragonSilver, 1100UL) : 0;
				pMapPlayer->m_nCWarRewardToken = pReward ? min(pReward->nRewardToken, 1100UL) : 0;
				pMapPlayer->m_nCWarRewardHonor = pReward ? min(pReward->nRewardHonor, 11000UL) : 0;

				pMapPlayer->m_nPlayerFlags |= PLAYER_FLAGS_GET_CWAR_REWARD;
				//
				::memset( &nCityPack, 0, sizeof(nCityPack) );
				nCityPack.nType = TYPE_PACK_GET_CWAR_REWARD;
				nCityPack.nData1 = pMapPlayer->GetUID();
				nCityPack.nData2 = 0;		// ?d??
				GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
			}
		}
	}
}

// ???z???
void CMapServer::SetSelfBoomTime(long time)
{
	struct MAP_CITY_DATA_PACK nCityPack;

	if (!time)
		time = 60*60;		// ?w?] 1?p??
	//
	::memset( &nCityPack, 0, sizeof(nCityPack) );
	nCityPack.nType = TYPE_PACK_SELF_BOOM;
	nCityPack.nData1 = time;
	GetServer()->SendData( GetServer()->m_hLoginServer, 0, PROTO_LOGIN_CITY_DATA_PACK, (char *)&nCityPack, sizeof(nCityPack), 0 );
	//
#ifndef USE_PACKAGE_DATA
	Log("Set bm time(%d)", time);
#endif
}

void CMapServer::StartSelfBoom(long time)
{
	long e_time;

	e_time = 0;
	if (time > 0)
	{
		e_time = gameGetRandomRange(time>>1, time) + 1;
	}
	m_nSelfBoomTime = GetLoopTime() + e_time;
}

void CMapServer::ProcSelfBoom()
{
	if (m_nSelfBoomTime)
	{
		if (GetLoopTime() >= m_nSelfBoomTime)
		{
			SetProtectOn();
			m_nSelfBoomTime = 0;
		}
	}
}

// ................... ??o?_?????W ......................
void CMapServer::CB_BroadcastGetDropItemMessage(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct LOGIN_BROADCAST_GET_DROP_ITEM_MESSAGE * pData;

	pData = (struct LOGIN_BROADCAST_GET_DROP_ITEM_MESSAGE *)pBuffer;
	CMapServer::GetServer()->BroadcastGetDropItemMessageToAllMap(pData->nMsg);
}

// ???W???i, ??????e?? Client
void CMapServer::BroadcastGetDropItemMessageToAllMap(char * msg)
{
	std::map<int, CMapPlayer *> *mMapPlayer;
	std::map<int, CMapPlayer *>::iterator	iter;
	std::map<int, CMapPlayer *>::iterator	iter_end;
	CMapPlayer * pPlayer;
	struct LOGIN_BROADCAST_GET_DROP_ITEM_MESSAGE nMsg;

	if (msg && msg[0])
	{
		msg_strncpy(nMsg.nMsg, msg, sizeof(nMsg.nMsg));
		nMsg.nSize = strlen(nMsg.nMsg);
		//
		mMapPlayer = &CMapServer::GetServer()->GetClientPlayerMap();
		iter_end = mMapPlayer->end();
		for(iter = mMapPlayer->begin(); iter != iter_end; iter++)
		{
			pPlayer	= iter->second;
			if (pPlayer)
			{
				if (!(pPlayer->m_nCharFlags & CHAR_FLAG_GM_TRACE))
					::msgSendData(pPlayer->GetClientID(), 0, PROTO_MAP_BROADCAST_GET_DROP_ITEM_MESSAGE, (char *)&nMsg, nMsg.GetSize(), 0);
			}
		}
		//
		//CMapServer::GetServer()->kimKeepDataToCheckLogin(0, PROTO_MAP_BROADCAST_MESSAGE, (char *)pMsg, nMsgSize);
	}
}

// ?|??? Login Server ?s??
void CMapServer::BroadcastGetDropItemMessage(char * msg)
{
	struct LOGIN_BROADCAST_GET_DROP_ITEM_MESSAGE nMsg;

	BroadcastGetDropItemMessageToAllMap(msg);
	// ??Login Server
	msg_strncpy(nMsg.nMsg, msg, sizeof(nMsg.nMsg));
	nMsg.nSize = strlen(nMsg.nMsg);
	SendData(GetLoginServer(), 0, PROTO_LOGIN_BROADCAST_GET_DROP_ITEM_MESSAGE, (char *)&nMsg, nMsg.GetSize(), 0);
}

void CMapServer::SetMerryDelay(unsigned long player_uid)
{
	m_mapMerryDelay[player_uid] = GetLoopTime() + (60*30);
}

long CMapServer::GetMerryDelay(unsigned long player_uid)
{
	long i, cur_time;

	if (m_mapMerryDelay.find(player_uid) != m_mapMerryDelay.end())
	{
		i = m_mapMerryDelay[player_uid];
		cur_time = GetLoopTime();
		if (i > cur_time)
			return(i - cur_time);
		m_mapMerryDelay.erase(player_uid);
	}
	return(0);
}

void CMapServer::ClearMerryDelay(unsigned long player_uid)
{
	if (m_mapMerryDelay.find(player_uid) != m_mapMerryDelay.end())
		m_mapMerryDelay.erase(player_uid);
}
// .................................. Cold Down .......................................
#ifdef USE_COOL_DOWN_SYSTEM
// ?e?x?s?????DB?A?b MapServer ????????e?^?????
// DB ?w?????s??
void CMapServer::SaveCoolDownData(CMapPlayer * pPlayer)
{
	long sid, total, size;

	if (pPlayer->IsLoadingData())			// ???|?????????
		return;
	//
	sid = GetDBServer();
	if ( IsConnectedToServer(sid) )
	{
		struct DB_UPDATE_COOLDOWN nData;
		struct COOLDOWN_SAVE_DATA * pCDSaveData;

		pCDSaveData = &nData.nData;
		total = CoolDown_MakeSaveData(pPlayer->GetUID(), GetLastTickCount(), pCDSaveData);
		if (total)
		{
			size = pCDSaveData->GetSize();
			nData.nMode = 0;							// 0=穝戈1=璶―眔戈
			//
			size += sizeof(long);						// nMode ?j?p
			SendData(sid, 0, PROTO_DB_UPDATE_COOLDOWN, (char *)&nData, size, 0);
		}
 #ifndef USE_PACKAGE_DATA
		GetServer()->Log("DEBUG: Save cold down info(%d,%d)", pPlayer->GetUID(), total);
 #endif
	}
}

void CMapServer::GetCoolDownData(unsigned long player_uid, long is_first)
{
	long sid, size;

	sid = GetDBServer();
	if ( IsConnectedToServer(sid) )
	{
		struct DB_UPDATE_COOLDOWN nData;
		struct COOLDOWN_SAVE_DATA * pCDSaveData;

		pCDSaveData = &nData.nData;
		pCDSaveData->cdsTotal = 0;
		pCDSaveData->cdsPlayerUID = player_uid;
		//
		size = pCDSaveData->GetSize();
		if (is_first)
		{
			nData.nMode = 2;
		}
		else
			nData.nMode = 1;							// 0=穝戈1=璶―眔戈
		//
 #ifndef USE_PACKAGE_DATA
		GetServer()->Log("DEBUG: Load cold down info(%d,%d)", player_uid, nData.nMode);
 #endif
		//
		size += sizeof(long);							// nMode ?j?p
		SendData(sid, 0, PROTO_DB_UPDATE_COOLDOWN, (char *)&nData, size, 0);
	}
}

// ???a??@???n?J??n??sClient????Loadres ????i??)
void CMapServer::UpdateClientCoolDownData(CMapPlayer * pPlayer)
{
	long total, size;
	struct COOLDOWN_SAVE_DATA nCDSaveData;

	total = CoolDown_MakeSaveData(pPlayer->GetUID(), GetLastTickCount(), &nCDSaveData);
	if (total)
	{
		size = nCDSaveData.GetSize();
		::msgSendData(pPlayer->GetClientID(), 0, PROTO_UPDATE_COOLDOWN_RESULT, (char *)&nCDSaveData, size, 0);
	}
	//
 #ifndef USE_PACKAGE_DATA
	GetServer()->Log("DEBUG: Update client cold down info(%d,%s)(%d)", pPlayer->GetUID(), pPlayer->GetName(), total);
 #endif
}

void CMapServer::CB_DBUpdateCoolDownResult(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct DB_UPDATE_COOLDOWN_RESULT * pData;
	struct COOLDOWN_SAVE_DATA * pSaveData;
	CMapPlayer * pPlayer;
	long i;
	unsigned long player_uid;

	pData = (struct DB_UPDATE_COOLDOWN_RESULT *)pBuffer;
	pSaveData = &pData->nData;
	//
	player_uid = pSaveData->cdsPlayerUID;
	//
	if (pSaveData->cdsTotal > COOLDOWN_SAVE_MAX_NUM)
	{
		GetServer()->Log("ERROR: Get cold down info exceed!(%d,%d)", player_uid, pSaveData->cdsTotal);
		return;
	}
	// ?O?_?b?C????
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(player_uid, CMapPlayer::CLASS_ID);
	if (!pPlayer)
	{
 #ifndef USE_PACKAGE_DATA
		GetServer()->Log("ERROR: Get cold down info but player not exist!(%d)", player_uid);
 #endif
		return;
	}
	// ?O?_?b?????(????n)
	if (!pPlayer->IsLoadingData())
	{
 #ifndef USE_PACKAGE_DATA
		GetServer()->Log("ERROR: Get cold down info but player not in loading!(%d,%s)", pPlayer->GetUID(), pPlayer->GetName());
 #endif
		return;
	}
	// ??s??v??????
 #ifndef USE_PACKAGE_DATA
	GetServer()->Log("DEBUG: Get cold down info(%d, %s)(%d,%d,%d)", pPlayer->GetUID(), pPlayer->GetName(), pSaveData->cdsTotal, pSaveData->cdsInfo[0].csiID, pSaveData->cdsInfo[0].csiLeftTick);
 #endif
	CoolDown_Clear(player_uid);
	for (i=0; i<pSaveData->cdsTotal; i++)
	{
		CoolDown_AddItem2(player_uid, pSaveData->cdsInfo[i].csiID, pSaveData->cdsInfo[i].csiLeftTick, pSaveData->cdsInfo[i].csiTotalTick, GetServer()->GetLastTickCount());
	}
}

#endif

// --------------------- ?J??\?i ------------------------
#ifdef SETUP_FORCE_CONTROL

void CMapServer::CB_LoginPackGetJNPlayerList(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct PACK_GET_JN_PLAYER_LIST * pData;
	CMapPlayer * pPlayer;

	pData = (struct PACK_GET_JN_PLAYER_LIST *)pBuffer;
	pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pData->nPlayerUID, CMapPlayer::CLASS_ID);
	if (pPlayer)
	{
		::msgSendData( pPlayer->GetClientID(), 0, PROTO_MAP_PACK_GET_JN_PLAYER_LIST, pBuffer, nLen, 0 );
	}
}

void CMapServer::CB_ExpelForcePlayer(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	struct MAP_EXPEL_FORCE_PLAYER * pExpelPlayer;
	CMapPlayer * pPlayer;

	pExpelPlayer = (struct MAP_EXPEL_FORCE_PLAYER *)pBuffer;
	pPlayer = GetServer()->FindPlayerByClientID(nID);
	if (pPlayer)
	{
		pExpelPlayer->nManagerUID = pPlayer->GetUID();
		GetServer()->SendData(GetServer()->m_hLoginServer, 0, PROTO_LOGIN_EXPEL_FORCE_PLAYER, (char *)pExpelPlayer, sizeof(*pExpelPlayer), 0);
	}
}

#endif

long CMapServer::IsCrossServer(long check_all)
{
#ifdef CROSS_SERVER_SYSTEM
	if (g_Server_Setting & SERVER_FLAG_CROSS_SERVER)		// 琌阁狝竟Server
		return(1);
	if (check_all)
		return(IsCrossCWarServer());
#endif
	return(0);
}

long CMapServer::IsCrossCWarServer()
{
#ifdef CROSS_SERVER_SYSTEM
 #ifdef CROSS_SERVER_CWAR
	if (g_Server_Setting & SERVER_FLAG_CROSS_CWAR_SERVER)	// 琌阁狝竟瓣驹Server
		return(1);
 #endif
#endif
	return(0);
}

long CMapServer::IsHistorySaveDate()
{
	if (g_Server_Setting & SERVER_FLAG_HISTORY_SAVE_DATE)
		return(1);
	return(0);
}

// .......... ?w?w?u?@ ............
// Server ?????s?u?B?z
// m_hLoginServer
// m_hDBServer

// ???pass????
long CMapServer::CB_Card(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	long is_deleted;
	CMapPlayer * pMapPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pMapPlayer == NULL)
		return 0;

	return 0;
}

void CMapServer::CB_OnDataStatus(char *pBuffer, int nLen, int nID, proto_COMM *pComm)
{
	if (nLen != sizeof(struct DataStatusNotify))
	{
		GetServer()->Log("ERROR:%s:%u", __FILE__, __LINE__);
		return;
	}

	DataStatusNotify* ptr = (DataStatusNotify*)pBuffer;
	//GetServer()->Log("CB_OnDataStatus %u,%u,%u", ptr->message, ptr->wparam, ptr->lparam);

	CMapPlayer* pPlayer	= (CMapPlayer *)GetServer()->FindObjectByUID(ptr->wparam);
	if(pPlayer == NULL)
	{
		GetServer()->Log("ERROR:%s:%u", __FILE__, __LINE__);
		return;
	}

	switch(ptr->message)
	{
	case DSMSG_GET:
		memcpyT(&pPlayer->m_charStatus, ptr, sizeof(DataStatusNotify));
		break;
	}
}

void CMapServer::CB_MapAutoSort(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	long is_deleted;
	CMapPlayer * pMapPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pMapPlayer == NULL)
		return;
	pMapPlayer->AutoSortCarry();
	pMapPlayer->UpdateClientItemData(0);
}

void CMapServer::CB_MapAutoSortStorage(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	long is_deleted;
	CMapPlayer * pMapPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pMapPlayer == NULL)
		return;

	pMapPlayer->AutoSortStorage();
	pMapPlayer->UpdateClientStorageData2(0);
}

void CMapServer::CB_MapGetMaterialLib(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	long is_deleted;
	CMapPlayer * pMapPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pMapPlayer == NULL)
		return;

	// ?????DBServer
	struct DB_GETPLAYERDATA_MATERIALLIB req;
	req.nAccountID = pMapPlayer->GetAccountID();
	req.nUID = pMapPlayer->GetUID();
	
	GetServer()->SendData(GetServer()->GetDBServer(), pComm->pcUID, PROTO_DB_GETPLAYERDATA_MATERIALLIB, (char*)&req, sizeof(req), 0);
}

void CMapServer::CB_MapSaveMaterialLib(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	long is_deleted;
	CMapPlayer * pMapPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pMapPlayer == NULL)
		return;

	// ???????- ??????
	bool hasDeleteList = false;
	bool hasReduceList = false;
	bool hasAddList = false;
	struct MAP_SAVE_MATERIALLIB_WITH_DELETELIST* pDataWithDeleteList = NULL;
	struct MAP_SAVE_MATERIALLIB_WITH_REDUCELIST* pDataWithReduceList = NULL;
	struct MAP_SAVE_MATERIALLIB_WITH_ADDLIST* pDataWithAddList = NULL;
	struct plrMATERIALLIB_SAVE* pMaterialLibData = NULL;
	
	GetServer()->Log("CB_MapSaveMaterialLib: Received data length = %d", nLen);
	GetServer()->Log("CB_MapSaveMaterialLib: sizeof(MAP_SAVE_MATERIALLIB_WITH_DELETELIST) = %d", sizeof(struct MAP_SAVE_MATERIALLIB_WITH_DELETELIST));
	GetServer()->Log("CB_MapSaveMaterialLib: sizeof(MAP_SAVE_MATERIALLIB_WITH_REDUCELIST) = %d", sizeof(struct MAP_SAVE_MATERIALLIB_WITH_REDUCELIST));
	GetServer()->Log("CB_MapSaveMaterialLib: sizeof(MAP_SAVE_MATERIALLIB_WITH_ADDLIST) = %d", sizeof(struct MAP_SAVE_MATERIALLIB_WITH_ADDLIST));
	GetServer()->Log("CB_MapSaveMaterialLib: sizeof(plrMATERIALLIB_SAVE) = %d", sizeof(struct plrMATERIALLIB_SAVE));
	
	if(nLen == sizeof(struct MAP_SAVE_MATERIALLIB_WITH_DELETELIST))
	{
		// ????????????????
		hasDeleteList = true;
		pDataWithDeleteList = (struct MAP_SAVE_MATERIALLIB_WITH_DELETELIST*)pBuffer;
		pMaterialLibData = &pDataWithDeleteList->materialLib;
		GetServer()->Log("CB_MapSaveMaterialLib: Using format with delete list, deleteCount = %d", pDataWithDeleteList->deleteCount);
	}
	else if(nLen == sizeof(struct MAP_SAVE_MATERIALLIB_WITH_REDUCELIST))
	{
		// ????????????????
		hasReduceList = true;
		pDataWithReduceList = (struct MAP_SAVE_MATERIALLIB_WITH_REDUCELIST*)pBuffer;
		pMaterialLibData = &pDataWithReduceList->materialLib;
		GetServer()->Log("CB_MapSaveMaterialLib: Using format with reduce list, reduceCount = %d", pDataWithReduceList->reduceCount);
	}
	else if(nLen == sizeof(struct MAP_SAVE_MATERIALLIB_WITH_ADDLIST))
	{
		// ????????????????
		hasAddList = true;
		pDataWithAddList = (struct MAP_SAVE_MATERIALLIB_WITH_ADDLIST*)pBuffer;
		pMaterialLibData = &pDataWithAddList->materialLib;
		GetServer()->Log("CB_MapSaveMaterialLib: Using format with add list, addCount = %d", pDataWithAddList->addCount);
	}
	else if(nLen == sizeof(struct plrMATERIALLIB_SAVE))
	{
		// ????????????
		pMaterialLibData = (struct plrMATERIALLIB_SAVE*)pBuffer;
		GetServer()->Log("CB_MapSaveMaterialLib: Using old format without lists");
	}
	else
	{
		// ????????????
		GetServer()->Log("CB_MapSaveMaterialLib: Wrong data length %d", nLen);
		return;
	}

	// ????????????????
	if(hasDeleteList && pDataWithDeleteList->deleteCount > 0)
	{
		GetServer()->Log("CB_MapSaveMaterialLib: Processing delete list with %d items", pDataWithDeleteList->deleteCount);
		
		struct plrDATA* pPlayer = pMapPlayer->GetCharData();
		if(pPlayer)
		{
			struct plrCARRYITEM_SAVE* pCarryItemTbl = pMapPlayer->GetItemData();
			if(pCarryItemTbl)
			{
				// ??????????
				for(long i = 0; i < pDataWithDeleteList->deleteCount && i < 400; i++)
				{
					unsigned short slotIndex = pDataWithDeleteList->deleteSlots[i];
					if(slotIndex < gameMAX_CARRYITEM)
					{
						GetServer()->Log("CB_MapSaveMaterialLib: Clearing bag slot %d", slotIndex);
						// ??????
						memset(&pCarryItemTbl->plrCarryItem[slotIndex], 0, sizeof(struct itemDATA_SAVE));
					}
				}
				
				// ??????
				pMapPlayer->AutoSaveItemData();
				GetServer()->Log("CB_MapSaveMaterialLib: Saved bag data");
				
				// ???????????????
				struct plrCARRYITEM sendData;
				memset(&sendData, 0, sizeof(sendData));
				::gameServer_MakeCarryItem(pCarryItemTbl, &sendData);
				::msgSendData(nClientID, pComm->pcUID, PROTO_MAP_GET_PLAYER_CARRY_ITEM_RESULT, (char*)&sendData, sizeof(sendData), 0);
				GetServer()->Log("CB_MapSaveMaterialLib: Sent updated bag to client");
			}
		}
	}

	// ??????????????????
	if(hasReduceList && pDataWithReduceList->reduceCount > 0)
	{
		GetServer()->Log("CB_MapSaveMaterialLib: Processing reduce list with %d items", pDataWithReduceList->reduceCount);

		struct plrDATA* pPlayer = pMapPlayer->GetCharData();
		if(pPlayer)
		{
			struct plrCARRYITEM_SAVE* pCarryItemTbl = pMapPlayer->GetItemData();
			if(pCarryItemTbl)
			{
				for(long i = 0; i < pDataWithReduceList->reduceCount && i < 400; i++)
				{
					unsigned short slotIndex = pDataWithReduceList->reduces[i].slotIndex;
					unsigned long reduceAmount = pDataWithReduceList->reduces[i].reduceAmount;
					struct itemDATA_SAVE* pItem;

					if(slotIndex >= gameMAX_CARRYITEM || reduceAmount == 0)
						continue;

					pItem = &pCarryItemTbl->plrCarryItem[slotIndex];
					if(pItem->m_Item.itemCode == 0)
						continue;

					if((unsigned long)pItem->m_Item.itemNumber <= reduceAmount)
					{
						GetServer()->Log("CB_MapSaveMaterialLib: Clearing bag slot %d", slotIndex);
						memset(pItem, 0, sizeof(struct itemDATA_SAVE));
					}
					else
					{
						pItem->m_Item.itemNumber = (unsigned short)(pItem->m_Item.itemNumber - (unsigned short)reduceAmount);
						GetServer()->Log("CB_MapSaveMaterialLib: Reduced bag slot %d by %d, new count = %d", slotIndex, reduceAmount, pItem->m_Item.itemNumber);
					}
				}

				pMapPlayer->AutoSaveItemData();
				GetServer()->Log("CB_MapSaveMaterialLib: Saved bag data after reduce");

				struct plrCARRYITEM sendData;
				memset(&sendData, 0, sizeof(sendData));
				::gameServer_MakeCarryItem(pCarryItemTbl, &sendData);
				::msgSendData(nClientID, pComm->pcUID, PROTO_MAP_GET_PLAYER_CARRY_ITEM_RESULT, (char*)&sendData, sizeof(sendData), 0);
				GetServer()->Log("CB_MapSaveMaterialLib: Sent updated bag to client");
			}
		}
	}
	
	// ??????????????????
	if(hasAddList && pDataWithAddList->addCount > 0)
	{
		GetServer()->Log("CB_MapSaveMaterialLib: Processing add list with %d items", pDataWithAddList->addCount);
		
		struct plrDATA* pPlayer = pMapPlayer->GetCharData();
		if(pPlayer)
		{
			struct plrCARRYITEM_SAVE* pCarryItemTbl = pMapPlayer->GetItemData();
			if(pCarryItemTbl)
			{
				long carryItemMax = ::gameServer_GetCarryItemMax(&pPlayer->plrSaveData);
				
				// ????????
				for(long i = 0; i < pDataWithAddList->addCount && i < 10; i++)
				{
					unsigned short itemID = pDataWithAddList->addItems[i].itemID;
					unsigned long itemCount = pDataWithAddList->addItems[i].itemCount;
					
					GetServer()->Log("CB_MapSaveMaterialLib: Adding item %d x%d to bag", itemID, itemCount);
					
					const unsigned long MAX_STACK = 60000; // ???????
					unsigned long remainingCount = itemCount;
					
					// ??????????????
					for(long j = 0; j < carryItemMax && remainingCount > 0; j++)
					{
						struct itemDATA_SAVE* pItem = &pCarryItemTbl->plrCarryItem[j];
						unsigned short bagItemID = pItem->m_Item.itemCode;
						
						if(bagItemID == itemID)
						{
							unsigned long currentCount = pItem->m_Item.itemNumber;
							if(currentCount < MAX_STACK)
							{
								unsigned long canAdd = MAX_STACK - currentCount;
								unsigned long toAdd = (remainingCount > canAdd) ? canAdd : remainingCount;
								pItem->m_Item.itemNumber += toAdd;
								remainingCount -= toAdd;
								GetServer()->Log("CB_MapSaveMaterialLib: Stacked %d to slot %d, new count = %d", toAdd, j, pItem->m_Item.itemNumber);
							}
						}
					}
					
					// ????????????
					while(remainingCount > 0)
					{
						// ????
						long emptySlot = -1;
						for(long j = 0; j < carryItemMax; j++)
						{
							struct itemDATA_SAVE* pItem = &pCarryItemTbl->plrCarryItem[j];
							if(pItem->m_Item.itemCode == 0)
							{
								emptySlot = j;
								break;
							}
						}
						
						if(emptySlot < 0)
						{
							GetServer()->Log("CB_MapSaveMaterialLib: No empty slot, remaining count = %d", remainingCount);
							break; // ????
						}
						
						// ??????
						struct itemDATA_SAVE* pItem = &pCarryItemTbl->plrCarryItem[emptySlot];
						memset(pItem, 0, sizeof(struct itemDATA_SAVE));
						pItem->m_Item.itemCode = itemID;
						
						unsigned long toAdd = (remainingCount > MAX_STACK) ? MAX_STACK : remainingCount;
						pItem->m_Item.itemNumber = toAdd;
						pItem->m_Item.itemUID.iUID_Time = GetServer()->GenerateItemUID();
						pItem->m_Item.itemUID.iUID_Serial = 0;
						
						remainingCount -= toAdd;
						GetServer()->Log("CB_MapSaveMaterialLib: Created new item in slot %d, count = %d", emptySlot, toAdd);
					}
					
					if(remainingCount > 0)
					{
						GetServer()->Log("CB_MapSaveMaterialLib: WARNING: Could not add all items, remaining = %d", remainingCount);
					}
				}
				
				// ??????
				pMapPlayer->AutoSaveItemData();
				GetServer()->Log("CB_MapSaveMaterialLib: Saved bag data after adding items");
				
				// ???????????????
				struct plrCARRYITEM sendData;
				memset(&sendData, 0, sizeof(sendData));
				::gameServer_MakeCarryItem(pCarryItemTbl, &sendData);
				::msgSendData(nClientID, pComm->pcUID, PROTO_MAP_GET_PLAYER_CARRY_ITEM_RESULT, (char*)&sendData, sizeof(sendData), 0);
				GetServer()->Log("CB_MapSaveMaterialLib: Sent updated bag to client");
			}
		}
	}

	// ??????????DBServer
	struct DB_SAVE_PLAYERDATA_MATERIALLIB req;
	req.nAccountID = pMapPlayer->GetAccountID();
	req.nUID = pMapPlayer->GetUID();
	::memcpyT(&req.nData, pMaterialLibData, sizeof(req.nData));
	
	GetServer()->SendData(GetServer()->GetDBServer(), pComm->pcUID, PROTO_DB_SAVE_PLAYERDATA_MATERIALLIB, (char*)&req, sizeof(req), 0);
	GetServer()->Log("CB_MapSaveMaterialLib: Forwarded material lib data to DBServer");
}

void CMapServer::CB_GMToolGetFile( char *pBuffer, int nLen, int nID, proto_COMM *pComm )
{
	struct GMTOOL_GET_FILE* pData = (struct GMTOOL_GET_FILE*)pBuffer;
	struct GMTOOL_GET_FILE_RESULT ret;
	
	ret.size = gameLoadFile(pData->filename, pData->offset, pData->size, ret.buffer);
	ret.nID = pData->nID;
	ret.pcUID = pData->pcUID;
	CMapServer::GetServer()->SendData(CMapServer::GetServer()->m_hLoginServer, 0, PROTO_GMTOOL_GET_FILE_RESULT, (char*)&ret, ret.Length(), 0 );
}

void CMapServer::CB_SoulCardMerge(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	long is_deleted;
	CMapPlayer * pMapPlayer = GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if(pMapPlayer == NULL)
		return;

	pMapPlayer->SoulCardMerge(pBuffer);
}

// ?? Login Server ?q?????|?????y
void CMapServer::CB_MapWarTownTaxAdd(char * pBuffer, int nLen, int nClientID, proto_COMM * pComm)
{
	struct MAP_WAR_TOWNTAX_ADD * pData;

	pData = (struct MAP_WAR_TOWNTAX_ADD *)pBuffer;
	CMapServer::GetServer()->GetMapManager()->BroadcastMapWarTownTaxAdd(pData);
}

//
//?p??L???p?L??????W?[?q
void CMapServer::getPlayerSolSoulToAddSoldierAttr(struct plrDATA * pData,struct plrCALCDATA * pCData)
{
	unsigned long uid = pData->plrSaveData.plrUID;
	unsigned long playerUID = gameNPC_GetPlayerUID(uid);
	if(!playerUID)	//????D?Huid?N?????O?p?L?A?hreturn
		return;

	CMapPlayer * pPlayer;
	int i,j,k;

	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(playerUID);
	if(pPlayer == NULL)
		return;
	
	struct plrDATA_NPC * pNPCData;
	struct SoldierSoul* ss;
	struct itemDATA * iData;
	pNPCData = pPlayer->GetNPCData();

	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		if (pNPCData->plrNPCData[i].plrNPC_UID == uid)
			break;
	}
	if( i < gameMAX_CARRY_SOLDIER_LIMIT )
	{
		for(j=0 ; j<gameMAX_SOLDIER_SOUL; j++)
		{
			ss = &pPlayer->GetSaveData()->plrSoldierSoul[i][j];
			if(ss->nItemCode)
			{
				iData = gameGetItemPtr(ss->nItemCode);
				//?Z?B???B??B??
				if (iData->itemAddAttrStr)
					pCData->pc_Add_Str += iData->itemAddAttrStr;
				if (iData->itemAddAttrInt)
					pCData->pc_Add_Int += iData->itemAddAttrInt;
				if (iData->itemAddAttrCon)
					pCData->pc_Add_Con += iData->itemAddAttrCon;
				if (iData->itemAddAttrDex)
					pCData->pc_Add_Dex += iData->itemAddAttrDex;
				//???
				for (k=1; k<=gameMAX_SKILL_RESIST; k++)
					pCData->pc_Add_MagicAttack[k] += iData->itemMagicAttack[k];
				//???
				for (k=1; k<=gameMAX_SKILL_RESIST; k++)
					pCData->pc_Add_MagicResist[k] += iData->itemMagicResist[k];

				if(ss->nItemGemFlag)//???^?_??
				{
					switch(ss->nItemGemType)
					{
						//?Z?B???B??B??
						case iMOD_TYPE_ADDATTR_STR:
							pCData->pc_Add_Str += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_ADDATTR_INT:
							pCData->pc_Add_Int += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_ADDATTR_CON:
							pCData->pc_Add_Con += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_ADDATTR_DEX:
							pCData->pc_Add_Dex += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						//???
						case iMOD_TYPE_MAGICATTACK_SLASH:
							pCData->pc_Add_MagicAttack[1] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_STING:
							pCData->pc_Add_MagicAttack[2] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_BREAK:
							pCData->pc_Add_MagicAttack[3] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_ARROW:
							pCData->pc_Add_MagicAttack[4] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_FIRE:
							pCData->pc_Add_MagicAttack[5] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_WATER:
							pCData->pc_Add_MagicAttack[6] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_GOD:
							pCData->pc_Add_MagicAttack[7] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_EVIL:
							pCData->pc_Add_MagicAttack[8] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						//???
						case iMOD_TYPE_MAGICDEFENSE_SLASH:
							pCData->pc_Add_MagicResist[1] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_STING:
							pCData->pc_Add_MagicResist[2] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_BREAK:
							pCData->pc_Add_MagicResist[3] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_ARROW:
							pCData->pc_Add_MagicResist[4] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_FIRE:
							pCData->pc_Add_MagicResist[5] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_WATER:
							pCData->pc_Add_MagicResist[6] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_GOD:
							pCData->pc_Add_MagicResist[7] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_EVIL:
							pCData->pc_Add_MagicResist[8] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
					}
				}
			}
		}

		gameServer_ApplySoldierEquipAttr(pData, pCData, pPlayer->GetSaveData(), i);
	}	
}


//?p??????a???v?T?p?L??q?W??(?t?L??)
void CMapServer::getPlayerSkillToAddSoldierAttr(struct plrDATA * pData)
{
	unsigned long uid = pData->plrSaveData.plrUID;
	unsigned long playerUID = gameNPC_GetPlayerUID(uid);
	if(!playerUID)	//????D?Huid?N?????O?p?L?A?hreturn
		return;
	
	struct plrDATA_SKILL * pSkill;
	CMapPlayer * pPlayer;
	unsigned long long add_HP, add_ATK, add_DEF;
	long skillLV;
	struct magicDATA * pMagic;
	int i,j;
	//int k;

	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(playerUID);
	if(pPlayer == NULL)
		return;
#ifdef SMART_PLR_DATA2
	pSkill = pPlayer->GetCharData()->plrSkillTable;
#else
	pSkill = &pPlayer->GetCharData()->plrSkillTable;
#endif
	if (skillLV = pSkill->plrSkillLevelMax[magic_SOLDIER_ADD_HP])
	{
		if (pMagic = gameMagic_GetPointer(magic_SOLDIER_ADD_HP, pPlayer->GetCharData()->plrBattleSkillMagic))
		{
			add_HP = (unsigned long long)gameGetSkillSPCValue(pMagic, skillLV);
			pData->plrHPMax += add_HP;	//糤﹀秖仓
		}
	}
	if (skillLV = pSkill->plrSkillLevelMax[magic_SOLDIER_ADD_HP2])
	{
		if (pMagic = gameMagic_GetPointer(magic_SOLDIER_ADD_HP2, pPlayer->GetCharData()->plrBattleSkillMagic))
		{
			add_HP = (unsigned long long)gameGetSkillSPCValue(pMagic, skillLV);
			pData->plrHPMax += add_HP;
		}
	}
	if (skillLV = pSkill->plrSkillLevelMax[magic_SOLDIER_ADD_ATK])
	{
		if (pMagic = gameMagic_GetPointer(magic_SOLDIER_ADD_ATK, pPlayer->GetCharData()->plrBattleSkillMagic))
		{
			add_ATK = (unsigned long long)gameGetSkillSPCValue(pMagic, skillLV);
			pData->plrAttackPowerMax += add_ATK;
			pData->plrAttackPowerMin += add_ATK;
		}
	}
	if (skillLV = pSkill->plrSkillLevelMax[magic_SOLDIER_ADD_ATK2])
	{
		if (pMagic = gameMagic_GetPointer(magic_SOLDIER_ADD_ATK2, pPlayer->GetCharData()->plrBattleSkillMagic))
		{
			add_ATK = (unsigned long long)gameGetSkillSPCValue(pMagic, skillLV);
			pData->plrAttackPowerMax += add_ATK;
			pData->plrAttackPowerMin += add_ATK;
		}
	}
	if (skillLV = pSkill->plrSkillLevelMax[magic_SOLDIER_ADD_DEF])
	{
		if (pMagic = gameMagic_GetPointer(magic_SOLDIER_ADD_DEF, pPlayer->GetCharData()->plrBattleSkillMagic))
		{
			add_DEF = (unsigned long long)gameGetSkillSPCValue(pMagic, skillLV);
			pData->plrDefense += add_DEF;
		}
	}
	if (skillLV = pSkill->plrSkillLevelMax[magic_SOLDIER_ADD_DEF2])
	{
		if (pMagic = gameMagic_GetPointer(magic_SOLDIER_ADD_DEF2, pPlayer->GetCharData()->plrBattleSkillMagic))
		{
			add_DEF = (unsigned long long)gameGetSkillSPCValue(pMagic, skillLV);
			pData->plrDefense += add_DEF;
		}
	}

// ?L????
	struct plrDATA_NPC * pNPCData;
	struct SoldierSoul* ss;
	struct itemDATA * iData;
	pNPCData = pPlayer->GetNPCData();

	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		if (pNPCData->plrNPCData[i].plrNPC_UID == uid)
			break;
	}
	if( i < gameMAX_CARRY_SOLDIER_LIMIT )
	{
		for(j=0 ; j<gameMAX_SOLDIER_SOUL; j++)
		{
			ss = &pPlayer->GetSaveData()->plrSoldierSoul[i][j];
			if(ss->nItemCode)
			{
				iData = gameGetItemPtr(ss->nItemCode);
				//??B???B??
				if (iData->itemAddAttackPowerMin || iData->itemAddAttackPowerMax)
				{
					pData->plrAttackPowerMin += iData->itemAddAttackPowerMin;
					pData->plrAttackPowerMax += iData->itemAddAttackPowerMax;					
				}				
				if (iData->itemAddDefense)
					pData->plrDefense += iData->itemAddDefense;
				if (iData->itemAddHP)
					pData->plrHPMax += iData->itemAddHP;
				//?Z?B???B??B??
				/*if (iData->itemAddAttrStr)
					pData->plrAttrStr += iData->itemAddAttrStr;
				if (iData->itemAddAttrInt)
					pData->plrAttrInt += iData->itemAddAttrInt;
				if (iData->itemAddAttrCon)
					pData->plrAttrCon += iData->itemAddAttrCon;
				if (iData->itemAddAttrDex)
					pData->plrAttrDex += iData->itemAddAttrDex;
				//???
				for (k=1; k<=gameMAX_SKILL_RESIST; k++)
					pData->plrMagicAttack[k] += iData->itemMagicAttack[k];
				//???
				for (k=1; k<=gameMAX_SKILL_RESIST; k++)
					pData->plrMagicResist[k] += iData->itemMagicResist[k];*/

				if(ss->nItemGemFlag)//???^?_??
				{
					switch(ss->nItemGemType)
					{
						//??B???B??
						case iMOD_TYPE_ADD_ATTACK:
							pData->plrAttackPowerMax += (long)gameSoldierSoul_GetGemVal(ss);
							pData->plrAttackPowerMin += (long)gameSoldierSoul_GetGemVal(ss);
							break;						
						case iMOD_TYPE_ADD_DEFENSE:
							pData->plrDefense += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_ADDHP:
							pData->plrHPMax += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						//?Z?B???B??B??
						/*case iMOD_TYPE_ADDATTR_STR:
							pData->plrAttrStr += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_ADDATTR_INT:
							pData->plrAttrInt += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_ADDATTR_CON:
							pData->plrAttrCon += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_ADDATTR_DEX:
							pData->plrAttrDex += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						//???
						case iMOD_TYPE_MAGICATTACK_SLASH:
							pData->plrMagicAttack[1] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_STING:
							pData->plrMagicAttack[2] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_BREAK:
							pData->plrMagicAttack[3] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_ARROW:
							pData->plrMagicAttack[4] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_FIRE:
							pData->plrMagicAttack[5] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_WATER:
							pData->plrMagicAttack[6] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_GOD:
							pData->plrMagicAttack[7] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICATTACK_EVIL:
							pData->plrMagicAttack[8] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						//???
						case iMOD_TYPE_MAGICDEFENSE_SLASH:
							pData->plrMagicResist[1] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_STING:
							pData->plrMagicResist[2] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_BREAK:
							pData->plrMagicResist[3] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_ARROW:
							pData->plrMagicResist[4] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_FIRE:
							pData->plrMagicResist[5] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_WATER:
							pData->plrMagicResist[6] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_GOD:
							pData->plrMagicResist[7] += (long)gameSoldierSoul_GetGemVal(ss);
							break;
						case iMOD_TYPE_MAGICDEFENSE_EVIL:
							pData->plrMagicResist[8] += (long)gameSoldierSoul_GetGemVal(ss);
							break;*/
					}
				}
			}
		}
	}

#ifdef STAGE_EFFECT
	//?a??S??
	/*struct MAPEFFECT_DATA *pMapData;
	struct gs_StageData * pStg = gameStageGetPtr(pPlayer->GetSaveData()->plrMapCode);
	if(pStg)
	{
		pMapData = gameGetMapEffecPtr(pStg->gstgStageEffect);
		if(pMapData)
		{
			for(i = 0; i < MAX_STAGE_EFFECT_MOD; i++)
			{
				switch(pMapData->effect_data[i*2])
				{
					case iMOD_TYPE_ADDHP:
						pData->plrHPMax += pMapData->effect_data[i*2+1];
						if(pData->plrHPMax <= 0) pData->plrHPMax = 1;
						break;
					default:
						break;
				}
			}
		}
			 
	}*/
#endif
}

unsigned long long CMapServer::getPlayerSkillToAddSoldierAttrSF(unsigned long uid)
{
	if(!IsPlayerUID(uid))	//??d?O?_??playerUID
		return 0;

	struct plrDATA_SKILL * pSkill;
	CMapPlayer * pPlayer;
	unsigned long long hpBonus = 0;
	long skillLV;
	struct magicDATA * pMagic;

	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);
	if(pPlayer == NULL)
		return 0;
	
#ifdef SMART_PLR_DATA2
	pSkill = pPlayer->GetCharData()->plrSkillTable;
#else
	pSkill = &pPlayer->GetCharData()->plrSkillTable;
#endif

	if (skillLV = pSkill->plrSkillLevelMax[magic_SOLDIER_ADD_HP])
	{
		if (pMagic = gameMagic_GetPointer(magic_SOLDIER_ADD_HP, pPlayer->GetCharData()->plrBattleSkillMagic))
		{
			hpBonus += (unsigned long long)gameGetSkillSPCValue(pMagic, skillLV);
		}
	}
	if (skillLV = pSkill->plrSkillLevelMax[magic_SOLDIER_ADD_HP2])
	{
		if (pMagic = gameMagic_GetPointer(magic_SOLDIER_ADD_HP2, pPlayer->GetCharData()->plrBattleSkillMagic))
		{
			hpBonus += (unsigned long long)gameGetSkillSPCValue(pMagic, skillLV);
		}
	}
// ?L????
	struct plrDATA_NPC * pNPCData;
	struct SoldierSoul* ss;
	struct itemDATA * iData;
	int i,k;
	pNPCData = pPlayer->GetNPCData();

	for (i=0; i<gameMAX_CARRY_SOLDIER_LIMIT; i++)
	{
		if (pNPCData->plrNPCData[i].plrNPC_UID == uid)
			break;
	}
	if( i < gameMAX_CARRY_SOLDIER_LIMIT )
	{
		for(k=0 ; k<gameMAX_SOLDIER_SOUL; k++)
		{
			ss = &pPlayer->GetSaveData()->plrSoldierSoul[i][k];
			if(ss->nItemCode)
			{
				iData = gameGetItemPtr(ss->nItemCode);
				//??
				if (iData->itemAddHP)
					hpBonus += iData->itemAddHP;
				if(ss->nItemGemFlag)//???^?_??
				{
					if(ss->nItemGemType == iMOD_TYPE_ADDHP)
						hpBonus += (long)gameSoldierSoul_GetGemVal(ss);
				}
			}
		}
	}	


	return hpBonus;
}

void CMapServer::GetDungeonCoolDown(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	unsigned long uid;
	uid = *(unsigned long*)pBuffer;

	CMapPlayer * pPlayer;
	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(uid);

	//?qLOGIN SERVER??MAP???W?h??????-- chenyin
	CMapServer * server = CMapServer::GetServer();
	struct LOGIN_MAP_GET_DUNGEON_COOLDOWN * data = (struct LOGIN_MAP_GET_DUNGEON_COOLDOWN *)::apiAllocateMemory(sizeof(struct LOGIN_MAP_GET_DUNGEON_COOLDOWN));
		
	memset(data,0,sizeof(struct LOGIN_MAP_GET_DUNGEON_COOLDOWN));
	 
	data->uID = uid;		
	data->subID = server->m_nMapServerSubID;
	server->SendData(server->m_hLoginServer, 0, PROTO_LOGIN_MAP_GET_DUNGEON_COOLDOWN, (char *)data, sizeof(struct LOGIN_MAP_GET_DUNGEON_COOLDOWN), 0);

}

void CMapServer::CB_MapDungeonSweep(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	long is_deleted;
	CMapPlayer * pPlayer;
	struct MAP_DUNGEON_SWEEP * pData;
	long leftSec;
	char msg[128];

	if (nLen != sizeof(struct MAP_DUNGEON_SWEEP))
		return;

	// ????????????clientID ???? uid ??
	pPlayer = (CMapPlayer *)GetServer()->FindPlayerByClientID(nClientID, &is_deleted);
	if (!pPlayer)
	{
		pPlayer = (CMapPlayer *)GetServer()->FindObjectByUID(pComm ? pComm->pcUID : 0, CMapPlayer::CLASS_ID, &is_deleted);
	}
	if (pPlayer == NULL)
	{
		return;
	}

	pData = (struct MAP_DUNGEON_SWEEP *)pBuffer;
	if (pData->enterMapCode <= 0)
	{
		return;
	}

	if (!GetServer()->GetMapSpaceManager()->HasPlayerDungeonClearRecord(pPlayer->GetUID(), pData->enterMapCode))
	{
		pPlayer->SendMessage(gameChannel_System, NULL, (char *)"Dungeon not cleared yet.");
		return;
	}

	leftSec = GetServer()->GetMapSpaceManager()->MapSpace_CheckPlayerDueTime(pData->enterMapCode, pPlayer->GetUID());
	leftSec = leftSec - GetServer()->GetLoopTime();
	if (leftSec > 0)
	{
		pPlayer->SendMessage(gameChannel_System, NULL, (char *)"Dungeon is cooling down.");
		return;
	}

	if (!GetServer()->GetMapSpaceManager()->DungeonSweep(pPlayer, pData->enterMapCode))
	{
		pPlayer->SendMessage(gameChannel_System, NULL, (char *)"Dungeon sweep failed.");
		return;
	}
}
//Sync cooldown from login -- chenyin
void CMapServer::SyncDungeonCoolDown(char *pBuffer, int nLen, int nClientID, proto_COMM *pComm)
{
	struct LOGIN_MAP_GET_DUNGEON_COOLDOWN_RESULT * pData;
	pData = (struct LOGIN_MAP_GET_DUNGEON_COOLDOWN_RESULT *) pBuffer;	

	CMapPlayer * pPlayer;
	pPlayer = (CMapPlayer *)CMapServer::GetServer()->FindObjectByUID(pData->uID);

	CMapServer::GetServer()->GetMapSpaceManager()->MergeSyncPlayerMapCoolDownData(pData);

		
	//CMapServer::GetServer()->GetMapSpaceManager()->GetDungeonCoolDown(pPlayer, uid, nLen, nClientID, pComm);
}

//============?p??C?i?a????H?? --chenyin
unsigned short CMapServer::GetMapPlayersCount(unsigned short mapCode){
	//Minus mapid player count
	return playerNumber[mapCode];
}
unsigned short CMapServer::AddMapPlayer(unsigned short mapCode){
	playerNumber[mapCode]++;
	if(playerNumberMax[mapCode]< playerNumber[mapCode]){
		playerNumberMax[mapCode] = playerNumber[mapCode];
#ifdef _DEBUG
		Log("?a???Max???acount update??s. Map[%hu] : %hu ?H",mapCode,playerNumberMax[mapCode]);
#endif
	}
	return playerNumber[mapCode];
}
unsigned short CMapServer::MinusMapPlayer(unsigned short mapCode){
	if(GetMapPlayersCount(mapCode)>0)
		playerNumber[ mapCode]--;
	else
		playerNumber[mapCode] = 0;
	return playerNumber[mapCode];
}
void CMapServer::SavePlayerMaxNumber(){	
	std::map<unsigned short, unsigned short> ::iterator	iter;
	std::map<unsigned short, unsigned short> ::iterator	iter_end;

	FILE * fp = NULL;	
	char fileName[40];
	char record[100];
	struct tm *ptm = ::apiGetTimeStruct(0);
	::wsprintf( fileName, "PlayerNumber\\%04d%02d%02d_%02d%02d%02d.txt", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec );
	
	//????VECTOR
	iter = playerNumberMax.begin();
	iter_end = playerNumberMax.end();
	vector<pair<unsigned short, unsigned short>> temp(iter,iter_end);
	sort(temp.begin(),temp.end(),CmpByValue());
	//?}??
	if (CreateDirectory(string("PlayerNumber").c_str(), NULL) ||
		ERROR_ALREADY_EXISTS == GetLastError())
	{
		fp = fopen(fileName, "a+b");
		if (!fp)	
			fp = fopen(fileName, "w+b");
		if (fp)
		{		
			//?q???n??temp????X???
			for(unsigned int i = 0; i< temp.size(); i++){
				if(temp[i].second==0)
					break;
				int size = ::wsprintf( record, "%hu\t%hu\r\n", temp[i].first,temp[i].second);			
				fwrite(record,size,1, fp);				
			}
			fflush(fp);
			fclose(fp);
		}
	}
}
void CMapServer::MapSpace_InitCopyObjects(CMapCtrl* pCtrl)
{
	GetMapSpaceManager()->MapSpace_InitCopyObjects(pCtrl);
}
//============?p??C?i?a????H??(End)
