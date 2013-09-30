//================= IV:Network - https://github.com/GTA-Network/IV-Network =================
//
// File: CIVWeather.cpp
// Project: Client.Core
// Author: FRi<FRi.developing@gmail.com>
// License: See LICENSE in root directory
//
//==============================================================================

#include "CIVWeather.h"
#include <Game/COffsets.h>

void CIVWeather::SetWeather( eWeather weather )
{
	if( weather >= WEATHER_EXTRA_SUNNY && weather <= WEATHER_SUNNY_WINDY_2 )
	{
		_asm	push weather;
		_asm	call COffsets::FUNC_SetWeather;
		_asm	add esp, 4;
	}
}

eWeather CIVWeather::GetWeather( void )
{
	return *(eWeather *)COffsets::VAR_CurrentWeather;
}

void CIVWeather::SetTime( int uHour, int uMinute )
{
	_asm	push -1;
	_asm	push 0;
	_asm	push uMinute;
	_asm	push uHour;
	_asm	call COffsets::FUNC_SetTimeOfDay;
	_asm	add esp, 10h;

	*(DWORD *)(*(DWORD *)(COffsets::VAR_TimeOfDay) + 0x260) = 2;
}

void CIVWeather::SetDayOfWeek( int uDay )
{
	*(DWORD *)COffsets::VAR_CClock__DayOfWeek = uDay;
}

void CIVWeather::LockTime( int uHour, int uMinute )
{
	*(DWORD *)COffsets::VAR_CClock__LockedHour = uHour;
	*(DWORD *)COffsets::VAR_CClock__LockedMinute = uMinute;
}

void CIVWeather::GetTime( int * uHour, int * uMinute )
{
	*uHour = *(DWORD *)COffsets::VAR_CClock__LockedHour;

	if( *uHour == -1 )
		*uHour = *(DWORD *)COffsets::VAR_CClock__Hour;

	*uMinute = *(DWORD *)COffsets::VAR_CClock__LockedMinute;

	if( *uMinute == -1 )
		*uMinute = *(DWORD *)COffsets::VAR_CClock__Minute;
}