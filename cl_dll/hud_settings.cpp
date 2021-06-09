#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include "discord_integration.h"

DECLARE_MESSAGE(m_Settings, Settings);

int CHudSettings::Init()
{
	HOOK_MESSAGE(Settings);

	m_iFlags = 0;

	gamemode[ARRAYSIZE(gamemode) - 1] = '\0';
	ag_version[ARRAYSIZE(ag_version) - 1] = '\0';
	wallgauss[ARRAYSIZE(wallgauss) - 1] = '\0';
	headshot[ARRAYSIZE(headshot) - 1] = '\0';
	blast_radius[ARRAYSIZE(blast_radius) - 1] = '\0';

	hud_settings = CVAR_CREATE("hud_settings", "1", FCVAR_ARCHIVE);

	gHUD.AddHudElem(this);
	return 0;
}

int CHudSettings::VidInit()
{
	m_iFlags &= ~HUD_ACTIVE;

	return 1;
}

int CHudSettings::Draw(float time)
{
	if (gHUD.m_flTime >= draw_until) {
		m_iFlags &= ~HUD_ACTIVE;
		return 0;
	}

	if (hud_settings->value == 0.0f)
		return 0;

	int r, g, b;
	UnpackRGB(r, g, b, gHUD.m_iDefaultHUDColor);

	const auto x = ScreenWidth - ScreenWidth / 5;
	int y = 0;

	char str[32];

	sprintf(str, "AG version %s", ag_version);
	gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight), str, r, g, b);

	gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight), gamemode, r, g, b);

	sprintf(str, "Time limit: %hhd", time_limit);
	gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight / 2 * 3), str, r, g, b);

	sprintf(str, "Frag limit: %hhd", frag_limit);
	gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight), str, r, g, b);

	sprintf(str, "Friendly fire: %s", friendly_fire ? "On" : "Off");
	gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight), str, r, g, b);

	sprintf(str, "Weaponstay: %s", weapon_stay ? "On" : "Off");
	gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight), str, r, g, b);

	if (wallgauss[0]) {
		sprintf(str, "Wallgauss: %sx (1)", wallgauss);
		gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight), str, r, g, b);
	}

	if (headshot[0]) {
		sprintf(str, "Headshot: %sx (3)", headshot);
		gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight), str, r, g, b);
	}

	if (blast_radius[0]) {
		sprintf(str, "Blast radius: %sx (1)", blast_radius);
		gEngfuncs.pfnDrawString(x, (y += gHUD.m_scrinfo.iCharHeight), str, r, g, b);
	}

	if (match_is_on) {
		gHUD.DrawHudStringCentered(
			ScreenWidth / 2,
			gHUD.m_scrinfo.iCharHeight * 2,
			"A match is on!",
			r,
			g,
			b
		);
	}

	return 0;
}

const char* CHudSettings::GetGamemode() const {
	return gamemode;
}

int CHudSettings::MsgFunc_Settings(const char* name, int size, void* buf)
{
	BEGIN_READ(buf, size);

	match_is_on = (READ_BYTE() != 0);

	strncpy(gamemode, READ_STRING(), ARRAYSIZE(gamemode) - 1);

	time_limit = READ_BYTE();
	frag_limit = READ_BYTE();
	friendly_fire = (READ_BYTE() != 0);
	weapon_stay = (READ_BYTE() != 0);

	strncpy(ag_version, READ_STRING(), ARRAYSIZE(ag_version) - 1);
	strncpy(wallgauss, READ_STRING(), ARRAYSIZE(wallgauss) - 1);
	strncpy(headshot, READ_STRING(), ARRAYSIZE(headshot) - 1);
	strncpy(blast_radius, READ_STRING(), ARRAYSIZE(blast_radius) - 1);

	if (wallgauss[0] == '1' && wallgauss[1] == '\0')
		wallgauss[0] = '\0';

	if (headshot[0] == '3' && headshot[1] == '\0')
		headshot[0] = '\0';

	if (blast_radius[0] == '1' && blast_radius[1] == '\0')
		blast_radius[0] = '\0';

	draw_until = gHUD.m_flTime + 10.0f;

	m_iFlags |= HUD_ACTIVE;

	// gEngfuncs.Con_Printf("Settings:\n\ts1 = `%s`\n\ttime_limit = %hhd\n\tfrag_limit=%hhd\n\tfrag_limit_present = %s\n\tweapon_stay = %s\n\ts2 = `%s`\n\ts3 = `%s`\n\ts4 = `%s`\n\ts5 = `%s`\n", s1, time_limit, frag_limit, frag_limit_present ? "true" : "false", weapon_stay ? "true" : "false", s2, s3, s4, s5);

	discord_integration::set_gamemode(gamemode);
	discord_integration::set_match_is_on(match_is_on);



#define _printf gEngfuncs.Con_Printf
	_printf("%s\n", gamemode);

	static char last_gamemode[ARRAYSIZE(gamemode) + 1]{};

	if (!strstr(last_gamemode, gamemode)) // Execute a userconfig if the gamemode has changed
	{
		_printf("weszlo\n");
		FILE* fp = NULL;

		char filename[ARRAYSIZE(gamemode) + 16];
		filename[ARRAYSIZE(filename) - 1] = '\0';

		char cmd[ARRAYSIZE(filename) + sizeof(char) * 6];
		cmd[ARRAYSIZE(cmd) - 1] = '\0';

		snprintf(filename, ARRAYSIZE(filename) - 1, "%s_userconfig.cfg", gamemode);

		fp = fopen(filename, "r");

		if (fp) // Check if the gamemode-specific userconfig exists, if so - execute it
		{
			fclose(fp);

			snprintf(cmd, ARRAYSIZE(cmd) - 1, "exec %s", filename);
			_printf("%s\n", cmd);
			gEngfuncs.pfnClientCmd(cmd);
		}
		else
		{
			if (![&cmd, &fp, this]() mutable -> bool { // Returns true if the current gamemode is DM-like and dm_userconfig.cfg has been executed
				fp = fopen("dm_userconfig.cfg", "r");

				if (fp == NULL) // If this file doesn't exist theres no point in checking if the current gamemode is DM-like
					return false;

				fclose(fp);
					
				const std::vector<std::string> dm_gamemodes = {
					"HLCCL", "AG TDM", "AG Arcade", "LLHL", "Agt"
				};

				for (const std::string& gm : dm_gamemodes)
				{
					if (strstr(gm.c_str(), gamemode))
					{
						strncpy(cmd, "exec dm_userconfig.cfg", ARRAYSIZE(cmd) - 1);
						gEngfuncs.pfnClientCmd(cmd);
						return true;
					}
				}
				return false; }())
			{
				gEngfuncs.pfnClientCmd("exec userconfig.cfg"); // dm_userconfig.cfg missing or unknown gamemode
				_printf("ex usrc.cfg\n");
			}
		}

		strncpy(last_gamemode, gamemode, ARRAYSIZE(last_gamemode) - 1);
	}

	return 1;
}
