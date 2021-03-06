#include "stdafx.h"
#include "PokemonTool.h"
#include "PokemonCodec.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
LPCTSTR szGameVersions[] =
{
	L"？",			// 0
	L"蓝宝石",		// 1
	L"红宝石",		// 2
	L"绿宝石",		// 3
	L"火红",		// 4
	L"叶绿",		// 5
	L"？",			// 6
	L"？",			// 7
	L"？",			// 8
	L"？",			// 9
	L"？",			// A
	L"？",			// B
	L"？",			// C
	L"？",			// D
	L"？",			// E
	L"？"			// F
};
DWORD dwGameVersionsCount = sizeof(szGameVersions) / sizeof(szGameVersions[0]);

///////////////////////////////////////////////////////////////////////////////////////////////////
BYTE GetIsPokemonShiny(DWORD dwChar, DWORD dwID)
{
	if((DWORD)(LOWORD(dwChar) ^ HIWORD(dwChar) ^ LOWORD(dwID) ^ HIWORD(dwID)) <= 0x07)
		return 1;
	else
		return 0;
}

BYTE GetPokemonPersonality(DWORD dwChar)
{
	return (BYTE)(dwChar % 25);
}

BYTE GetSex(DWORD dwChar, BYTE bFemaleRatio)
{
	if(bFemaleRatio == 0xFF)		// 性別不明
	{
		return 2;
	}
	else if(bFemaleRatio == 0xFE)	// 雌のみ
	{
		return 0;
	}
	else if(bFemaleRatio == 0x00)	// 雄のみ
	{
		return 1;
	}
	else
	{
		// the least significant byte indicates sex
		if((BYTE)(dwChar) <= bFemaleRatio)
			return 0;
		else
			return 1;
	}	
}

///////////////////////////////////////////////////////////////////////////////////////////////////
CPokemonCodec::CPokemonCodec(VOID)
{
	m_pPokemon = NULL;
	m_pBreedInfo = NULL;
	m_pSkillInfo = NULL;
	m_pAcquiredInfo = NULL;
	m_pInnateInfo = NULL;
	m_bEncoded = TRUE;
	m_dwLang = lang_jp;
}

CPokemonCodec::~CPokemonCodec(VOID)
{
}

VOID CPokemonCodec::SetLang(DWORD dwLang)
{
	if(dwLang == lang_en)
		m_dwLang = lang_en;
	else
		m_dwLang = lang_jp;
}

VOID CPokemonCodec::DetermineWhereIsWhich(BYTE bOrder[4])
{
	/*
	WhereIsWhich:
	1230 means type 1, 2, 3, 0 are at the 1st, 2nd, 3rd, 4th place,
	so bOrder[i] means at i, the type is bOrder[i].
	type0 -> breed
	type1 -> skills
	type2 -> ability-points-gained-by-battles
	type3 -> innate abilities

	Consider that 0 < 1 < 2 < 3, the initiate set is {0, 1, 2, 3},
	the total number of placements of the set is 4! = 24.
	How should we map every placement to a number in {0,1,2,...,23} ?
	The game generates this squence:

	First, choose the 1st number:
	0
	1
	2
	3
	(Notice that the 1st numbr is from the smallest to the largest.)

	Second, recursively, choose the 2nd number:
	0 -> 01   02   03
	1 -> 10   12   13
	2 -> 20   21   23
	3 -> 30   31   32
	(Notice that the 2nd numbr is from the smallest to the largest.)

	Third, choose the 3rd number:
	01 -> 012  013    02 -> 021  023    03 -> 031  032
	10 -> 101  102    12 -> 120  123    13 -> 130  132
	20 -> 201  203    21 -> 210  213    23 -> 230  231
	30 -> 301  302    31 -> 310  312    32 -> 320  321
	(Notice that these numbr is from the smallest to the largest.)

	Fourth, we've got the sequence (no need to choose the 4th number):
	0123 0132 0213 0231 0312 0321
	1023 1032 1203 1230 1302 1320
	2013 2031 2103 2130 2301 2310
	3012 3021 3102 3120 3201 3210

	Fifth, we map integers to every number:
	 0 <-> 0123,  1 <-> 0132, ...
	 6 <-> 1023,  7 <-> 1032, ...
	12 <-> 2013, 13 <-> 2031, ...
	18 <-> 3012, 19 <-> 3021, ...

	Finally, we get the algorithm:
	The whole set is a level 0 group,
	there are 4 level I group (the 1st number), each has 6 elements,
	(i.e. {1023 1032 1203 1230 1302 1320} is a level I group)
	each level I group has 3 level II group (the 2nd number), each has 2 elements,
	(i.e. {1023 1032}, {1203 1230}, {1302 1320} are level II groups)
	each level II group has 2 level III group (the 3rd number), each has 1 elements.
	(i.e. {1023}, {1032} are level III groups)
	*/

	bOrder[0] = 0;
	bOrder[1] = 1;
	bOrder[2] = 2;
	bOrder[3] = 3;
	BYTE	bPosition = (BYTE)(m_pPokemon->Header.dwChar % 24);	// the position in the (bLevel)th group
	BYTE	bSubGroupIdAddTypeId, bCount, bTypeId;

	bCount = 6;		// how many elements in level (bLevel+1) group
	bTypeId = 0;	// now is choosing the (bLevel+1)th number (choosing in the (bLevel+1)th group)
	while(bPosition > 0)
	{
		for(bSubGroupIdAddTypeId = (bPosition / bCount) + bTypeId;
			bSubGroupIdAddTypeId > bTypeId;
			--bSubGroupIdAddTypeId)
		{
			BYTE	b = bOrder[bSubGroupIdAddTypeId];
			bOrder[bSubGroupIdAddTypeId] = bOrder[bSubGroupIdAddTypeId - 1];
			bOrder[bSubGroupIdAddTypeId - 1] = b;
		}

		bPosition %= bCount;
		bCount /= (3 - bTypeId);
		++bTypeId;
	}
}

VOID CPokemonCodec::DetermineWhichIsWhere(BYTE bOrder[4])
{
	/*
	WhichIsWhere:
	1230 means at the 1st, 2nd, 3rd, 4th place are type 1, 2, 3, 0,
	so bOrder[i] means type i is at bOrder[i].
	*/

	BYTE	bTemp[4];
	DetermineWhereIsWhich(bTemp);
	BYTE	bWhere;
	for(bWhere = 0; bWhere < 4; ++bWhere)
	{
		bOrder[bTemp[bWhere]] = bWhere;
	}
}

VOID CPokemonCodec::CalculatePokemonStructInfoPtr(VOID)
{
	if(m_pPokemon == NULL)
	{
		m_pBreedInfo = NULL;
		m_pSkillInfo = NULL;
		m_pAcquiredInfo = NULL;
		m_pInnateInfo = NULL;
	}
	else
	{
		BYTE	bOrder[4];
		DetermineWhichIsWhere(bOrder);

		m_pBreedInfo = &(m_pPokemon->rgInfo[bOrder[0]].BreedInfo);
		m_pSkillInfo = &(m_pPokemon->rgInfo[bOrder[1]].SkillInfo);
		m_pAcquiredInfo = &(m_pPokemon->rgInfo[bOrder[2]].AcquiredInfo);
		m_pInnateInfo = &(m_pPokemon->rgInfo[bOrder[3]].InnateInfo);
	}
}

BOOL CPokemonCodec::Attach(PokemonStruct * pPokemon, BOOL bEncoded)
{
	if(pPokemon == NULL)
		return FALSE;

	m_pPokemon = pPokemon;
	m_bEncoded = bEncoded;

	if((m_pPokemon->Header.bBadEgg & 0x05) != 0)
		m_pPokemon->Header.bBadEgg &= (BYTE)(~0x05);

	CalculatePokemonStructInfoPtr();

	return TRUE;
}

PokemonStruct * CPokemonCodec::Detach(VOID)
{
	PokemonStruct *	pPokemon = m_pPokemon;

	m_pPokemon = NULL;
	CalculatePokemonStructInfoPtr();

	return pPokemon;
}

VOID CPokemonCodec::Decode(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded == FALSE)
		return;

	LPDWORD	pdwPokemon = reinterpret_cast<LPDWORD>(m_pPokemon->rgInfo);
	DWORD	dwXorMask, n;

	dwXorMask = m_pPokemon->Header.dwChar ^ m_pPokemon->Header.dwID;
	m_pPokemon->Header.wChecksum = 0;

	for(n = 0; n < sizeof(m_pPokemon->rgInfo) / sizeof(DWORD); ++n)
	{
		pdwPokemon[n] ^= dwXorMask;
		m_pPokemon->Header.wChecksum += LOWORD(pdwPokemon[n]) + HIWORD(pdwPokemon[n]);
	}

	m_bEncoded = FALSE;
}

VOID CPokemonCodec::Encode(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	LPDWORD	pdwPokemon = reinterpret_cast<LPDWORD>(m_pPokemon->rgInfo);
	DWORD	dwXorMask, n;

	// make sure this pokemon is daycare-center-enbled, and not a bag egg
	if(m_pBreedInfo->wBreed != 0)
	{
		m_pPokemon->Header.bBadEgg |= 0x02;
		m_pPokemon->Header.bBadEgg &= (BYTE)(~0x05);
	}

	dwXorMask = m_pPokemon->Header.dwChar ^ m_pPokemon->Header.dwID;
	m_pPokemon->Header.wChecksum = 0;

	for(n = 0; n < sizeof(m_pPokemon->rgInfo) / sizeof(DWORD); ++n)
	{
		m_pPokemon->Header.wChecksum += LOWORD(pdwPokemon[n]) + HIWORD(pdwPokemon[n]);
		pdwPokemon[n] ^= dwXorMask;
	}

	m_bEncoded = TRUE;
}

VOID CPokemonCodec::GetNickName(BYTE bCode[POKEMON_NICK_NAME_SIZE])
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	switch(m_pPokemon->Header.bNickNameLanguage)
	{
	case 0x00:		// an empty one
		FillMemory(bCode, POKEMON_NICK_NAME_SIZE, 0xFF);
		break;

	case 0x01:		// jp version
		MoveMemory(bCode, m_pPokemon->Header.bNickName, 5);
		bCode[5] = 0xFF;
		FillMemory(bCode + 6, POKEMON_NICK_NAME_SIZE - 6, 0x00);
		break;

	default:		// en version
		MoveMemory(bCode, m_pPokemon->Header.bNickName, POKEMON_NICK_NAME_SIZE);
		break;
	}
}

VOID CPokemonCodec::SetNickName(BYTE bCode[POKEMON_NICK_NAME_SIZE])
{
	if(m_pPokemon == NULL)
		return;

	if(m_pPokemon->Header.bNickNameLanguage == 0x00)
	{
		if(m_dwLang == lang_jp)
			m_pPokemon->Header.bNickNameLanguage = 0x01;
		else
			m_pPokemon->Header.bNickNameLanguage = 0x02;
	}

	switch(m_pPokemon->Header.bNickNameLanguage)
	{
	case 0x01:		// jp version
		MoveMemory(m_pPokemon->Header.bNickName, bCode, 5);
		m_pPokemon->Header.bNickName[5] = 0xFF;
		FillMemory(m_pPokemon->Header.bNickName + 6, POKEMON_NICK_NAME_SIZE - 6, 0x00);
		break;

	default:		// en version
		MoveMemory(m_pPokemon->Header.bNickName, bCode, POKEMON_NICK_NAME_SIZE);
		break;
	}
}

VOID CPokemonCodec::GetNickName(CString &szName)
{
	if(m_pPokemon == NULL)
		return;

	switch(m_pPokemon->Header.bNickNameLanguage)
	{
	case 0x00:		// an empty one
		szName = _T("");
		break;

	case 0x01:		// jp version
		CodeToString(szName, m_pPokemon->Header.bNickName, 5, 0xFF, lang_jp);
		break;

	default:		// en version
		CodeToString(szName, m_pPokemon->Header.bNickName, POKEMON_NICK_NAME_SIZE, 0xFF, lang_en);
		break;
	}
}

VOID CPokemonCodec::SetNickName(LPCTSTR szName)
{
	if(m_pPokemon == NULL)
		return;

	if(m_pPokemon->Header.bNickNameLanguage == 0x00)
	{
		if(m_dwLang == lang_jp)
			m_pPokemon->Header.bNickNameLanguage = 0x01;
		else
			m_pPokemon->Header.bNickNameLanguage = 0x02;
	}

	switch(m_pPokemon->Header.bNickNameLanguage)
	{
	case 0x01:		// jp version
		StringToCode(szName, m_pPokemon->Header.bNickName, 5, 0xFF, 0xFF, lang_jp);
		m_pPokemon->Header.bNickName[5] = 0xFF;
		FillMemory(m_pPokemon->Header.bNickName + 6, POKEMON_NICK_NAME_SIZE - 6, 0x00);
		break;

	default:		// en version
		StringToCode(szName, m_pPokemon->Header.bNickName, POKEMON_NICK_NAME_SIZE, 0xFF, 0xFF, lang_en);
		break;
	}
}

VOID CPokemonCodec::GetCatcherName(BYTE bCode[POKEMON_TRAINER_NAME_SIZE])
{
	if(m_pPokemon == NULL)
		return;

	if(m_pPokemon->Header.bNickNameLanguage == 0x00)
	{
		FillMemory(bCode, POKEMON_TRAINER_NAME_SIZE, 0xFF);
	}
	else
	{
		switch(m_dwLang)
		{
		case lang_jp:	// jp version
			MoveMemory(bCode, m_pPokemon->Header.bCatcherName, 5);
			bCode[5] = 0xFF;
			FillMemory(bCode + 6, POKEMON_TRAINER_NAME_SIZE - 6, 0x00);
			break;

		default:		// en version
			MoveMemory(bCode, m_pPokemon->Header.bCatcherName, POKEMON_TRAINER_NAME_SIZE);
			break;
		}
	}
}

VOID CPokemonCodec::SetCatcherName(BYTE bCode[POKEMON_TRAINER_NAME_SIZE])
{
	if(m_pPokemon == NULL)
		return;

	if(m_pPokemon->Header.bNickNameLanguage == 0x00)
	{
		if(m_dwLang == lang_jp)
			m_pPokemon->Header.bNickNameLanguage = 0x01;
		else
			m_pPokemon->Header.bNickNameLanguage = 0x02;
	}

	switch(m_pPokemon->Header.bNickNameLanguage)
	{
	case lang_jp:	// jp version
		MoveMemory(m_pPokemon->Header.bCatcherName, bCode, POKEMON_TRAINER_NAME_SIZE);
		break;

	default:		// en version
		MoveMemory(m_pPokemon->Header.bCatcherName, bCode, POKEMON_TRAINER_NAME_SIZE);
		break;
	}
}

VOID CPokemonCodec::GetCatcherName(CString &szCatcherName)
{
	if(m_pPokemon == NULL)
		return;

	if(m_pPokemon->Header.bNickNameLanguage == 0x00)
	{
		szCatcherName = _T("");
	}
	else
	{
		switch(m_dwLang)
		{
		case lang_jp:		// jp version
			CodeToString(szCatcherName, m_pPokemon->Header.bCatcherName, POKEMON_TRAINER_NAME_SIZE, 0xFF, lang_jp);
			break;

		default:		// en version
			CodeToString(szCatcherName, m_pPokemon->Header.bCatcherName, POKEMON_TRAINER_NAME_SIZE, 0xFF, lang_en);
			break;
		}
	}
}

VOID CPokemonCodec::SetCatcherName(LPCTSTR szCatcherName)
{
	if(m_pPokemon == NULL)
		return;

	if(m_pPokemon->Header.bNickNameLanguage == 0x00)
	{
		if(m_dwLang == lang_jp)
			m_pPokemon->Header.bNickNameLanguage = 0x01;
		else
			m_pPokemon->Header.bNickNameLanguage = 0x02;
	}

	switch(m_dwLang)
	{
	case lang_jp:		// jp version
		StringToCode(szCatcherName, m_pPokemon->Header.bCatcherName, POKEMON_TRAINER_NAME_SIZE, 0xFF, 0xFF, lang_jp);
		//FillMemory(m_pPokemon->Header.bCatcherName + 5, POKEMON_TRAINER_NAME_SIZE - 5, 0x00);
		break;

	default:		// en version
		StringToCode(szCatcherName, m_pPokemon->Header.bCatcherName, POKEMON_TRAINER_NAME_SIZE, 0xFF, 0xFF, lang_en);
		break;
	}
}

DWORD CPokemonCodec::GetChar(VOID)
{
	if(m_pPokemon == NULL)
		return (0);

	return m_pPokemon->Header.dwChar;
}

VOID CPokemonCodec::SetChar(DWORD dwChar)
{
	if(m_pPokemon == NULL)
		return;

	BYTE	bWhere;
	BYTE	bOrder[4];
	PokemonStructInfo	rgInfo[4];

	DetermineWhereIsWhich(bOrder);
	for(bWhere = 0; bWhere < 4; ++bWhere)
	{
		CopyMemory(&rgInfo[bOrder[bWhere]], &m_pPokemon->rgInfo[bWhere], sizeof(PokemonStructInfo));
	}

	m_pPokemon->Header.dwChar = dwChar;

	DetermineWhereIsWhich(bOrder);
	for(bWhere = 0; bWhere < 4; ++bWhere)
	{
		CopyMemory(&m_pPokemon->rgInfo[bWhere], &rgInfo[bOrder[bWhere]], sizeof(PokemonStructInfo));
	}

	CalculatePokemonStructInfoPtr();
}

DWORD CPokemonCodec::GenShinyChar(VOID)
{
	if(m_pPokemon == NULL)
		return (0);

	DWORD	dwShinyChar;

	// preserve low word
	dwShinyChar =
		LOWORD(m_pPokemon->Header.dwChar) ^
		LOWORD(m_pPokemon->Header.dwID)   ^
		HIWORD(m_pPokemon->Header.dwID)   ^
		(GenShortRandom() & 0x07);

	dwShinyChar = (dwShinyChar << 16) + (m_pPokemon->Header.dwChar & 0xFFFF);

	return dwShinyChar;
}

DWORD CPokemonCodec::GenNoShinyChar(VOID)
{
	if(m_pPokemon == NULL)
		return (0);

	WORD	wNoShinyRand;
	DWORD	dwNoShinyChar;

	// preserve low word
	wNoShinyRand = GenShortRandom() & 0xFFF8;
	if(wNoShinyRand == 0)
		wNoShinyRand = 8;
	dwNoShinyChar =
		LOWORD(m_pPokemon->Header.dwChar) ^
		LOWORD(m_pPokemon->Header.dwID)   ^
		HIWORD(m_pPokemon->Header.dwID);

	dwNoShinyChar = ((dwNoShinyChar ^ wNoShinyRand) << 16) + (m_pPokemon->Header.dwChar & 0xFFFF);

	return dwNoShinyChar;
}

DWORD CPokemonCodec::GetID(VOID)
{
	if(m_pPokemon == NULL)
		return (0);

	return m_pPokemon->Header.dwID;
}

VOID CPokemonCodec::SetID(DWORD dwID)
{
	if(m_pPokemon == NULL)
		return;

	m_pPokemon->Header.dwID = dwID;
}

DWORD CPokemonCodec::GenShinyID(VOID)
{
	if(m_pPokemon == NULL)
		return (0);

	DWORD	dwShinyId;

	// preserve low word
	dwShinyId =
		LOWORD(m_pPokemon->Header.dwID)   ^
		LOWORD(m_pPokemon->Header.dwChar) ^
		HIWORD(m_pPokemon->Header.dwChar) ^
		(GenShortRandom() & 0x07);

	dwShinyId = (dwShinyId << 16) + (m_pPokemon->Header.dwID & 0xFFFF);

	return dwShinyId;
}

DWORD CPokemonCodec::GenNoShinyID(VOID)
{
	if(m_pPokemon == NULL)
		return (0);

	WORD	wNoShinyRand;
	DWORD	dwNoShinyId;

	// preserve low word
	wNoShinyRand = GenShortRandom() & 0xFFF8;
	if(wNoShinyRand == 0)
		wNoShinyRand = 8;
	dwNoShinyId =
		LOWORD(m_pPokemon->Header.dwID)   ^
		LOWORD(m_pPokemon->Header.dwChar) ^
		HIWORD(m_pPokemon->Header.dwChar);

	dwNoShinyId = ((dwNoShinyId ^ wNoShinyRand) << 16) + (m_pPokemon->Header.dwID & 0xFFFF);

	return dwNoShinyId;
}

BYTE CPokemonCodec::GetIsShiny(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	WORD wShiny =
		LOWORD(m_pPokemon->Header.dwChar) ^ HIWORD(m_pPokemon->Header.dwChar) ^
		LOWORD(m_pPokemon->Header.dwID)   ^ HIWORD(m_pPokemon->Header.dwID);

	if(wShiny <= 0x07)
		return 1;
	else
		return 0;
}

BYTE CPokemonCodec::GetPersonality(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return (BYTE)(m_pPokemon->Header.dwChar % 25);
}

VOID CPokemonCodec::SetPersonality(BYTE bType)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	BYTE	bShiny = GetIsShiny();
	DWORD	dwDiff;

	if(bType >= 25)
		bType %= 25;

	dwDiff = bType + 25 - GetPersonality();
	if(dwDiff >= 25)
		dwDiff %= 25;

	if(dwDiff == 0)
		return;

	// note: 0x00100000 % 25 = 1
	// it is an ideal number for preserving the low word
	dwDiff <<= 20;

	DWORD dwChar = m_pPokemon->Header.dwChar;
	if(0xFFFFFFFF - dwChar < dwDiff)	// must not overflow !!!
		dwChar -= 0x01900000 - dwDiff;
	else
		dwChar += dwDiff;

	SetChar(dwChar);
	if(bShiny)
	{
		SetID(GenShinyID());
	}
	else
	{
		while(GetIsShiny())
		{
			SetID(m_pPokemon->Header.dwID + 0x00010000);
		}
	}
}

BYTE CPokemonCodec::GetSexByte(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return (BYTE)(m_pPokemon->Header.dwChar & 0xFF);
}

VOID CPokemonCodec::SetSexByte(BYTE bSexByte)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	BYTE	bShiny = GetIsShiny();
	BYTE	bPersonality = GetPersonality();

	// the code is a bit slow, but it is easy to write
	SetChar((m_pPokemon->Header.dwChar & 0xFFFFFF00) + bSexByte);	// the least significant byte indicates sex
	SetPersonality(bPersonality);

	if(bShiny)
	{
		SetID(GenShinyID());
	}
	else
	{
		while(GetIsShiny())
		{
			SetID(m_pPokemon->Header.dwID + 0x00010000);
		}
	}
}

BYTE CPokemonCodec::GetSex(BYTE bFemaleRatio)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return pm_neither;

	if(bFemaleRatio == 0xFF)		// 性別不明
	{
		return pm_neither;
	}
	else if(bFemaleRatio == 0xFE)	// 雌のみ
	{
		return pm_female;
	}
	else if(bFemaleRatio == 0x00)	// 雄のみ
	{
		return pm_male;
	}
	else
	{
		// the least significant byte indicates sex
		if(GetSexByte() <= bFemaleRatio)
			return pm_female;
		else
			return pm_male;
	}
}

VOID CPokemonCodec::SetSex(BYTE bType, BYTE bFemaleRatio)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	if(bFemaleRatio == 0xFF  ||	// 性別不明
		bFemaleRatio == 0xFE ||	// 雌のみ
		bFemaleRatio == 0x00)	// 雄のみ
	{
		return;
	}

	BYTE	bSexByte;
	switch(bType)
	{
	case pm_female:
		bSexByte = GenShortRandom() % (bFemaleRatio + 1);
		break;
	case pm_male:
		bSexByte = 0xFF - GenShortRandom() % (0xFF - bFemaleRatio);
		break;
	}

	SetSexByte(bSexByte);
}

WORD CPokemonCodec::GetBreed(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return m_pBreedInfo->wBreed;
}

VOID CPokemonCodec::SetBreed(WORD wBreed)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pBreedInfo->wBreed = wBreed;
}

BYTE CPokemonCodec::GetCatchPlace(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return m_pInnateInfo->bCatchPlace;
}

VOID CPokemonCodec::SetCatchPlace(BYTE bCatchPlace)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bCatchPlace = bCatchPlace;
}

VOID CPokemonCodec::GetIndvAbilities(BYTE lpbIndvAbl[6])
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	lpbIndvAbl[0] = m_pInnateInfo->bHPIndv;
	lpbIndvAbl[1] = m_pInnateInfo->bAtkIndv;
	lpbIndvAbl[2] = m_pInnateInfo->bDefIndv;
	lpbIndvAbl[3] = m_pInnateInfo->bDexIndv;
	lpbIndvAbl[4] = m_pInnateInfo->bSAtkIndv;
	lpbIndvAbl[5] = m_pInnateInfo->bSDefIndv;
}

BYTE CPokemonCodec::GetIndvAbility(BYTE bIndex)
{
	if(m_pPokemon == NULL || m_bEncoded || bIndex >= 6)
		return 0;

	return (BYTE)(GetBitField(m_pInnateInfo, 0x20UL + bIndex * 5, 5));
}

VOID CPokemonCodec::SetIndvAbilities(BYTE lpbIndvAbl[6])
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bHPIndv		= lpbIndvAbl[0];
	m_pInnateInfo->bAtkIndv		= lpbIndvAbl[1];
	m_pInnateInfo->bDefIndv		= lpbIndvAbl[2];
	m_pInnateInfo->bDexIndv		= lpbIndvAbl[3];
	m_pInnateInfo->bSAtkIndv	= lpbIndvAbl[4];
	m_pInnateInfo->bSDefIndv	= lpbIndvAbl[5];
}

VOID CPokemonCodec::SetIndvAbility(BYTE bIndex, BYTE bPoint)
{
	if(m_pPokemon == NULL || m_bEncoded || bIndex >= 6)
		return;

	if(bPoint >= 0x1F)
		bPoint = 0x1F;
	SetBitField(m_pInnateInfo, 0x20UL + bIndex * 5, 5, bPoint);
}

VOID CPokemonCodec::SetMaxIndvAbilities(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bHPIndv		= 0x1F;
	m_pInnateInfo->bAtkIndv		= 0x1F;
	m_pInnateInfo->bDefIndv		= 0x1F;
	m_pInnateInfo->bDexIndv		= 0x1F;
	m_pInnateInfo->bSAtkIndv	= 0x1F;
	m_pInnateInfo->bSDefIndv	= 0x1F;
}

VOID CPokemonCodec::SetRandIndvAbilities(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bHPIndv		= GenShortRandom() % 0x20;
	m_pInnateInfo->bAtkIndv		= GenShortRandom() % 0x20;
	m_pInnateInfo->bDefIndv		= GenShortRandom() % 0x20;
	m_pInnateInfo->bDexIndv		= GenShortRandom() % 0x20;
	m_pInnateInfo->bSAtkIndv	= GenShortRandom() % 0x20;
	m_pInnateInfo->bSDefIndv	= GenShortRandom() % 0x20;
}

WORD CPokemonCodec::GetSkill(BYTE bIndex)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	bIndex &= 0x03;
	return m_pSkillInfo->rgwSkillId[bIndex];
}

BYTE CPokemonCodec::GetSkillPoints(BYTE bIndex)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	bIndex &= 0x03;
	return m_pSkillInfo->rgbPoints[bIndex];
}

VOID CPokemonCodec::SetSkill(BYTE bIndex, WORD wSkill)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	bIndex &= 0x03;

	m_pSkillInfo->rgwSkillId[bIndex] = wSkill;
}

VOID CPokemonCodec::SetSkillPoints(BYTE bIndex, BYTE bPoint)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	bIndex &= 0x03;

	m_pSkillInfo->rgbPoints[bIndex] = bPoint;
}

VOID CPokemonCodec::SetSkill(BYTE bIndex, WORD wSkill, BYTE bPoint)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	bIndex &= 0x03;

	m_pSkillInfo->rgwSkillId[bIndex] = wSkill;
	m_pSkillInfo->rgbPoints[bIndex] = bPoint;
}

BYTE CPokemonCodec::GetSkillPointBoost(BYTE bIndex)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	bIndex &= 0x03;

	return (BYTE)(GetBitField((LPBYTE)(m_pBreedInfo) + 0x08, bIndex << 1, 0x02));
}

VOID CPokemonCodec::SetSkillPointBoost(BYTE bIndex, BYTE bBoost)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	bIndex &= 0x03;
	bBoost &= 0x03;

	SetBitField((LPBYTE)(m_pBreedInfo) + 0x08, bIndex << 1, 0x02, bBoost);
}

VOID CPokemonCodec::SetMaxSkillPointBoosts(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	//m_pBreedInfo->bPointUp0 = 0x03;
	//m_pBreedInfo->bPointUp1 = 0x03;
	//m_pBreedInfo->bPointUp2 = 0x03;
	//m_pBreedInfo->bPointUp3 = 0x03;
	*((LPBYTE)(m_pBreedInfo) + 0x08) = 0xFF;
}

DWORD CPokemonCodec::GetExp(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return (0);

	return m_pBreedInfo->dwExp;
}

VOID CPokemonCodec::SetExp(DWORD dwExp)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pBreedInfo->dwExp = dwExp;
}

BYTE CPokemonCodec::GetIntimate(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return (0);

	return m_pBreedInfo->bIntimate;
}

VOID CPokemonCodec::SetIntimate(BYTE bIntimate)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pBreedInfo->bIntimate = bIntimate;
}

BYTE CPokemonCodec::GetSpecialty(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return (0);

	return m_pInnateInfo->bSpecialty;
}

VOID CPokemonCodec::SetSpecialty(BYTE bIndex)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bSpecialty = bIndex;;
}

VOID CPokemonCodec::GetBattleBonuses(BYTE bBB[6])
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	for(DWORD dwIndex = 0; dwIndex < 6; ++dwIndex)
		bBB[dwIndex] = m_pAcquiredInfo->rgbBattleBonuses[dwIndex];
}

BYTE CPokemonCodec::GetBattleBonus(BYTE bIndex)
{
	if(m_pPokemon == NULL || m_bEncoded || bIndex >= 6)
		return 0;

	return m_pAcquiredInfo->rgbBattleBonuses[bIndex];
}

VOID CPokemonCodec::SetBattleBonuses(BYTE bBB[6])
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	for(DWORD dwIndex = 0; dwIndex < 6; ++dwIndex)
		m_pAcquiredInfo->rgbBattleBonuses[dwIndex] = bBB[dwIndex];
}

VOID CPokemonCodec::SetBattleBonus(BYTE bIndex, BYTE bPoint)
{
	if(m_pPokemon == NULL || m_bEncoded || bIndex >= 6)
		return;

	m_pAcquiredInfo->rgbBattleBonuses[bIndex] = bPoint;
}

VOID CPokemonCodec::SetMaxBattleBonuses(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	for(DWORD dwIndex = 0; dwIndex < 6; ++dwIndex)
		m_pAcquiredInfo->rgbBattleBonuses[dwIndex] = 0xFF;
}

VOID CPokemonCodec::GetApealPoints(BYTE bAP[6])
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	for(DWORD dwIndex = 0; dwIndex < 6; ++dwIndex)
		bAP[dwIndex] = m_pAcquiredInfo->rgbApealPoints[dwIndex];
}

BYTE CPokemonCodec::GetApealPoint(BYTE bIndex)
{
	if(m_pPokemon == NULL || m_bEncoded || bIndex >= 6)
		return 0;

	return m_pAcquiredInfo->rgbApealPoints[bIndex];
}

VOID CPokemonCodec::SetApealPoints(BYTE bAP[6])
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	for(DWORD dwIndex = 0; dwIndex < 6; ++dwIndex)
		m_pAcquiredInfo->rgbApealPoints[dwIndex] = bAP[dwIndex];
}

VOID CPokemonCodec::SetApealPoint(BYTE bIndex, BYTE bPoint)
{
	if(m_pPokemon == NULL || m_bEncoded || bIndex >= 6)
		return;

	m_pAcquiredInfo->rgbApealPoints[bIndex] = bPoint;
}

VOID CPokemonCodec::SetMaxApealPoints(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	for(DWORD dwIndex = 0; dwIndex < 5; ++dwIndex)
		m_pAcquiredInfo->rgbApealPoints[dwIndex] = 0xFF;
	m_pAcquiredInfo->rgbApealPoints[5] = 0x00;
}

BYTE CPokemonCodec::GetPokeBall(VOID)
{
	if(m_pPokemon == NULL)
		return 0;

	return m_pInnateInfo->bPokeBall;
}

VOID CPokemonCodec::SetPokeBall(BYTE bPokeBall)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	// if bPokeBall is 0x00, the ball is a Monster Ball
	//if(bPokeBall < 1)
	//	bPokeBall = 1;
	if(bPokeBall > 12)
		bPokeBall = 12;
	m_pInnateInfo->bPokeBall = bPokeBall;
}

BYTE CPokemonCodec::GetCatchLevel(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return m_pInnateInfo->bCatchLevel;
}

VOID CPokemonCodec::SetCatchLevel(BYTE bCatchLevel)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	if(bCatchLevel > 100)
		bCatchLevel = 100;
	m_pInnateInfo->bCatchLevel = bCatchLevel;
}

BYTE CPokemonCodec::GetIsEgg(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return m_pInnateInfo->bIsEgg;
}

VOID CPokemonCodec::SetIsEgg(BYTE bIsEgg)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bIsEgg = bIsEgg;
}

WORD CPokemonCodec::GetItem(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return m_pBreedInfo->wItem;
}

VOID CPokemonCodec::SetItem(WORD wItem)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pBreedInfo->wItem = wItem;
}

BYTE CPokemonCodec::GetPokerus(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return m_pInnateInfo->bPokeVirus;
}

VOID CPokemonCodec::SetPokerus(BYTE bPokerus)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	if(bPokerus > 0x0F)
		bPokerus = 0x0F;
	m_pInnateInfo->bPokeVirus = bPokerus;
}

BYTE CPokemonCodec::GetBlackPoint(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return 0;

	return m_pInnateInfo->bBlackPoint;
}

VOID CPokemonCodec::SetBlackPoint(BYTE bBlackPoint)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	if(bBlackPoint > 0x0F)
		bBlackPoint = 0x0F;
	m_pInnateInfo->bBlackPoint = bBlackPoint;
}

BYTE * CPokemonCodec::GetRibbonPointer(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return NULL;

	return ((BYTE *)(m_pInnateInfo) + 8);
}

VOID CPokemonCodec::SetMaxRibbon(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bRibbon0 = 0x4;
	m_pInnateInfo->bRibbon1 = 0x4;
	m_pInnateInfo->bRibbon2 = 0x4;
	m_pInnateInfo->bRibbon3 = 0x4;
	m_pInnateInfo->bRibbon4 = 0x4;
	m_pInnateInfo->bRibbon5 = 0x01F;
}

VOID CPokemonCodec::SetMaxRibbon32(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bRibbon0 = 0x4;
	m_pInnateInfo->bRibbon1 = 0x4;
	m_pInnateInfo->bRibbon2 = 0x4;
	m_pInnateInfo->bRibbon3 = 0x4;
	m_pInnateInfo->bRibbon4 = 0x4;
	m_pInnateInfo->bRibbon5 = 0xFFF;
}

VOID CPokemonCodec::ClearRibbons(VOID)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	m_pInnateInfo->bRibbon0 = 0x0;
	m_pInnateInfo->bRibbon1 = 0x0;
	m_pInnateInfo->bRibbon2 = 0x0;
	m_pInnateInfo->bRibbon3 = 0x0;
	m_pInnateInfo->bRibbon4 = 0x0;
	m_pInnateInfo->bRibbon5 = 0x000;
}

BYTE CPokemonCodec::GetMarking(BYTE bIndex)
{
	if(m_pPokemon == NULL)
		return 0;

	bIndex &= 0x03;
	return GetBit(&(m_pPokemon->Header.bMarkings), bIndex);
}

VOID CPokemonCodec::SetMarking(BYTE bIndex, BYTE bFlag)
{
	if(m_pPokemon == NULL)
		return;

	bIndex &= 0x03;
	if(bFlag)
		SetBit(&(m_pPokemon->Header.bMarkings), bIndex);
	else
		ClrBit(&(m_pPokemon->Header.bMarkings), bIndex);
}

BYTE CPokemonCodec::GetGameVersion(VOID)
{
	if(m_pPokemon == NULL)
		return 0;

	return (BYTE)(m_pInnateInfo->bGameVersion);
}

VOID CPokemonCodec::SetGameVersion(BYTE bGameVersion)
{
	if(m_pPokemon == NULL)
		return;

	m_pInnateInfo->bGameVersion = min(bGameVersion, 0x0F);
}

BYTE CPokemonCodec::GetObedience()
{
	if(m_pPokemon == NULL)
		return 0;

	return (BYTE)(m_pInnateInfo->bObedience);
}

VOID CPokemonCodec::SetObedience(BYTE bObedience)
{
	if(m_pPokemon == NULL)
		return;

	m_pInnateInfo->bObedience = min(bObedience, 1);
}

VOID CPokemonCodec::SetObedience()
{
	if(m_pPokemon == NULL)
		return;

	switch(m_pBreedInfo->wBreed)
	{
	case 144:	// フリーザー
	case 145:	// サンダー
	case 146:	// ファイヤー
	case 150:	// ミュウツー
	case 151:	// ミュウ
	case 243:	// ライコウ
	case 244:	// エンテイ
	case 245:	// スイクン
	case 249:	// ルギア
	case 250:	// ホウオウ
	case 251:	// セレビイ
	case 401:	// レジロック
	case 402:	// レジアイス
	case 403:	// レジスチル
	case 404:	// カイオーガ
	case 405:	// グラードン
	case 406:	// レックウザ
	case 407:	// ラティアス
	case 408:	// ラティオス
	case 409:	// ジラーチ
	case 410:	// ディオキシス
		m_pInnateInfo->bObedience = 1;
		break;

	default:
		m_pInnateInfo->bObedience = 0;
		break;
	}
}

VOID CPokemonCodec::CreatePokemon(WORD wBreed, DWORD dwLang, BYTE bGameVersion)
{
	if(m_pPokemon == NULL || m_bEncoded)
		return;

	ZeroMemory(m_pPokemon->rgInfo, sizeof(m_pPokemon->rgInfo));

	// char
	SetChar(GenLongRandom());
	// id
	SetID(GenLongRandom());
	// nick name
	if(dwLang != lang_jp && dwLang != lang_en)
		dwLang = m_dwLang;
	if(dwLang == lang_en)
	{
		m_pPokemon->Header.bNickNameLanguage = 0x02;
		FillMemory(m_pPokemon->Header.bNickName, POKEMON_NICK_NAME_SIZE, 0xFF);
		FillMemory(m_pPokemon->Header.bCatcherName, POKEMON_TRAINER_NAME_SIZE, 0xFF);
	}
	else
	{
		m_pPokemon->Header.bNickNameLanguage = 0x01;
		FillMemory(m_pPokemon->Header.bNickName, 6, 0xFF);
		FillMemory(m_pPokemon->Header.bNickName + 6, POKEMON_NICK_NAME_SIZE - 6, 0x00);
		FillMemory(m_pPokemon->Header.bCatcherName, 5, 0xFF);
		FillMemory(m_pPokemon->Header.bCatcherName + 5, POKEMON_TRAINER_NAME_SIZE - 5, 0x00);
	}

	// bad egg
	m_pPokemon->Header.bBadEgg = 0x02;
	// unk0
	*(LPWORD)(m_pPokemon->Header.unk0) = 0;

	// wBreed
	m_pBreedInfo->wBreed = wBreed;

	// bCatchPlace
	if(bGameVersion == pm_ruby || bGameVersion == pm_sapphire)
	{
		m_pInnateInfo->bCatchPlace = 0x55;
	}
	else
	{
		m_pInnateInfo->bCatchPlace = 0xFF;	// うんめいてきな
	}
	// bCatchLevel
	m_pInnateInfo->bCatchLevel = 0x00;	// 0x00 means the pokemon comes from an egg, the game considers 0x00 to be 0x05
	// Game Version
	m_pInnateInfo->bGameVersion = min(bGameVersion, 0x0F);
	// bPokeball
	m_pInnateInfo->bPokeBall = 0xB;		// ゴージャスボール
	// bObedience
	SetObedience();
}

VOID CPokemonCodec::CreatePokemon(WORD wBreed, BYTE bGameVersion)
{
	CreatePokemon(wBreed, m_dwLang, bGameVersion);
}
