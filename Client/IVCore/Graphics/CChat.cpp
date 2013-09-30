//================= IV:Network - https://github.com/GTA-Network/IV-Network =================
//
// File: CChat.cpp
// Project: Client.Core
// Author: FRi<FRi.developing@gmail.com>
// License: See LICENSE in root directory
//
//==============================================================================

#include "CChat.h"
#include <CCore.h>
#include <Scripting/CClientCommands.h>

extern CCore * g_pCore;

CChat::CChat(float fX, float fY) : m_fX(fX),
	m_fY(fY)
{
	Reset();
}

CChat::~CChat()
{
	Clear();
	ClearInput();
}

void CChat::Setup(D3DPRESENT_PARAMETERS * pPresentParameters)
{
	// Get the chat x, y position from the settings
	float fX = 50.0f;
	float fY = 50.0f;

	// Work out the maximum coordinates
	float fMaxX = (1920.0f - CHAT_WIDTH); // Or should we use already 2500 ?
	float fMaxY = (1080.0f - (m_iRenderLines * (CChat::GetFontHeight() + 2.0f) + 10.0f + 30.0f));

	// Clamp the coordinates
	fX = Math::Clamp(0.0f, fX, fMaxX);
	fY = Math::Clamp(0.0f, fY, fMaxY);

	// Store the chat position
	m_fX = fX;
	m_fY = fY;
}

void CChat::Reset()
{
	// Reset the variables
	SetVisible(true);
	m_bPaused = false;
	m_bInputBlocked = false;
	m_bMap = false;
	m_bOldState = false;
	m_uiNumLines = CHAT_MAX_LINES;
	m_uiMostRecentLine = CHAT_MAX_LINES - 1;
	m_TextColor = CHAT_TEXT_COLOR;
	m_InputTextColor = CColor(255, 255, 255, 255);
	m_iRenderLines = 10;

	// Scroll
	m_uiScrollOffset = 0;
	m_fSmoothScroll = 0;
	m_fSmoothLastTimeSeconds = 0;
	m_fSmoothAllowAfter = 0;
	m_fSmoothScrollResetTime = 0;

	// History
	m_iCurrentHistory = -1;
	m_iTotalHistory = 0;

	// Input
	SetInputVisible(false);
	SetInputPrefix("> Say: ");
}

void CChat::Render()
{
	if(!m_bVisible)
		return;

	// Work out Y offset the position
	float fLineDifference = (CChat::GetFontHeight() + 2.0f);
	float fOffsetY = (m_iRenderLines * fLineDifference + 10.0f);

	// Apply line smooth scroll
	unsigned int uiLine = m_uiMostRecentLine + m_uiScrollOffset;
	unsigned int uiLinesDrawn = 0;

	// Loop over all chatlines
	while (uiLinesDrawn < m_iRenderLines)
	{
		// Draw the current line
		m_Lines[uiLine].Draw(m_fX, fOffsetY, 255, true);

		// Adjust the offset
		fOffsetY -= fLineDifference;

		// Increment the lines drawn
		uiLine++;
		uiLinesDrawn++;

		// Is this line the end?
		if(uiLine >= CHAT_MAX_LINES)
			break;
	}

	if(m_bInputVisible)
	{
		float y = (m_fY + (m_iRenderLines * fLineDifference) + 30.0f);
		m_InputLine.Draw(m_fX, y, 255, true);
	}

}

void CChat::Output(const char * szTextOld, bool bColorCoded)
{
	CString strText = szTextOld;
	if (strlen(szTextOld) > 128)
		strText = strText.Substring(0, 127);
	 
	CChatLine * pLine = NULL;
	const char * szRemainingText = strText.Get();
	CColor color = m_TextColor;

	do
	{
		if (m_uiMostRecentLine == 0)
		{
			m_Lines [CHAT_MAX_LINES - 1].SetActive (false);

			// Making space for the new line
			for (int i = CHAT_MAX_LINES - 2; i >= 0; i--)
				m_Lines [i + 1] = m_Lines [i];

			m_uiMostRecentLine = 0;
		}
		else
		{
			m_uiMostRecentLine--;
		}
		
		pLine = &m_Lines[m_uiMostRecentLine];
		
		if(pLine)
		{
			// Add the message to the chatlog
			CLogFile::Close();
			CLogFile::Open(CLIENT_CHATLOG_FILE, true);
			CLogFile::Printf("%s", pLine->RemoveColorCodes(szRemainingText).Get());
			CLogFile::Close();
			CLogFile::Open(CLIENT_LOG_FILE, true); // Reopen client logfile

			szRemainingText = pLine->Format(szRemainingText, CHAT_WIDTH, color, bColorCoded);
			
			pLine->SetActive(true);

		}	
	}
	while(szRemainingText);
}

void CChat::Outputf(bool bColorCoded, const char * szFormat, ...)
{
	char szString[512];

	va_list ap;
	va_start(ap, szFormat);
	vsprintf_s(szString, szFormat, ap);
	va_end(ap);

	Output(szString, bColorCoded);
}

void CChat::Clear()
{
	for(int i = 0; i < CHAT_MAX_LINES; i++)
	{
		m_Lines[i].SetActive(false);
	}

	m_uiMostRecentLine = CHAT_MAX_LINES - 1;
	m_fSmoothScroll = 0;
}

void CChat::ClearInput()
{
	m_strInputText.Clear();
	m_InputLine.Clear();
}

float CChat::GetFontHeight()
{
	return g_pCore->GetGraphics()->GetFontHeight(1.0f);
}

float CChat::GetCharacterWidth(int iChar)
{
	return g_pCore->GetGraphics()->GetCharacterWidth((char)iChar, 1.0f);
}

float CChat::GetPosition(bool bPositionType)
{
	if (bPositionType)
		return m_fY;
	else
		return m_fX;
}

void CChat::SetInputVisible(bool bVisible)
{
	m_bInputVisible = bVisible;

	// Unlock the player controls
	if(g_pCore->GetGame()->GetLocalPlayer() && !bVisible)
		g_pCore->GetGame()->GetLocalPlayer()->SetPlayerControlAdvanced(true,true);
}

bool CChat::HandleUserInput(unsigned int uMsg, DWORD dwChar)
{
	if (!m_bInputBlocked)
	{
		// Enable the input after 2 seconds
		unsigned uiTime = timeGetTime();

		if ((uiTime - 2000) > g_pCore->GetGameLoadInitializeTime())
		{
			m_bInputBlocked = true;
		}
		else
		{
			return false;
		}
	}

	if (!IsVisible() || g_pCore->GetClientState() != GAME_STATE_INGAME)
		return false;

	// Was it a key release?
	if(uMsg == WM_KEYUP)
	{
		if(dwChar == VK_ESCAPE)
		{
			
		}
		else if(dwChar == VK_RETURN)
		{
			if(m_bInputVisible)
			{
				// Process input
				ProcessInput();

				// Clear the input buffer
				m_strInputText.Clear();

				// Clear the input line
				m_InputLine.Clear();

				// Disable input
				SetInputVisible(false);
				return true;
			}
		}
		else if (dwChar == VK_UP)
		{
			// Move the history up
			HistoryUp();

			return true;
		}
		else if(dwChar == VK_DOWN)
		{
			// Move the history down
			HistoryDown();

			return true;
		}
	}
	else if(uMsg == WM_KEYDOWN)
	{
		if(dwChar == VK_BACK)
		{
			// Is the input enabled?
			if(m_bInputVisible)
			{
				// Is there any input to delete?
				if(m_strInputText.GetLength() > 0)
				{
					// Resize the input
					m_strInputText.Resize((m_strInputText.GetLength() - 1));

					// Set the input
					SetInput(m_strInputText);

					return true;
				}
			}
		}
		else if(dwChar == VK_PRIOR)
		{
			// Scroll the page up
			ScrollUp();

			return true;
		}
		else if(dwChar == VK_NEXT)
		{
			// Scroll the page down
			ScrollDown();

			return true;
		}
	}
	else if(uMsg == WM_CHAR)
	{
		if(dwChar == 't' || dwChar == 'T' || dwChar == '`') // 0x54
		{
			// Is the input disabeld, we're not paused and not in the map ?
			if(!m_bInputVisible && !m_bPaused && !m_bMap)
			{
				// Enable the input
				SetInputVisible(true);

				// Lock the player controls
				g_pCore->GetGame()->GetLocalPlayer()->SetPlayerControlAdvanced(false,false);
				return true;
			}
		}

		// Is the input enbaled?
		if(m_bInputVisible)
		{
			// Is the char a valid input?
			if(dwChar >= ' ')
			{
				// We haven't exceeded the max input
				if(m_strInputText.GetLength() < CHAT_MAX_CHAT_LENGTH)
				{
					// Add the character to the input text
					m_strInputText += (char)dwChar;

					// Set the input
					SetInput(m_strInputText);
				}

				return true;
			}
		}
	}

	return false;
}

void CChat::ProcessInput()
{
	// Was anything entered?
	if(m_strInputText.GetLength() > 0)
	{
		// Is the input a command?
		bool bIsCommand = (m_strInputText.GetChar(0) == CHAT_COMMAND_CHAR);

		if(bIsCommand)
		{
			// Get the end of the command
			size_t sCommandEnd = m_strInputText.Find(" ");

			// If we don't have a valid end use the end of the string
			if (sCommandEnd == std::string::npos)
			{
				sCommandEnd = m_strInputText.GetLength();
			}

			// Get the command name
			std::string strCommand = m_strInputText.Substring(1, (sCommandEnd - 1));

			// Get the command parameters
			std::string strParams;

			// Do we have any parameters?
			if(sCommandEnd < m_strInputText.GetLength())
			{
				strParams = m_strInputText.Substring((sCommandEnd + 1), m_strInputText.GetLength());
			}

			AddToHistory();
			
			if(strCommand == "chat-renderlines")
			{
				if(strParams.size() <= 0)
					return g_pCore->GetChat()->Output("USE: /chat-renderlines [amount]", false);

				// Get the amount of lines to render
				int iRenderLines = atoi(strParams.c_str());

				if(iRenderLines <= 0 
					|| iRenderLines > CHAT_RENDER_LINES)
					return g_pCore->GetChat()->Output("USE: /chat-renderlines [amount]", false);

				// Set the render lines amount
				CVAR_SET_INTEGER("chat-renderlines", iRenderLines);
				m_iRenderLines = iRenderLines;

				// Save the settings
				CSettings::Save();

				Outputf(false, " -> Chat render lines set to %d.", iRenderLines);
				return;
			}

			// If our command is defined as client command & exists, return and stop the function call
			if(CClientCommands::HandleUserInput(strCommand, strParams))
				return;
		}

		/*if(!bIsCommand)
		{
			// Temporary(to print messages, until we've added the network manager
			CString strInput = m_strInputText.Get();
			Outputf(false, "%s: %s", g_pCore->GetNick().Get(), m_strInputText.Get());			AddToHistory();
		}*/

		// Is the network module instance valid?
		if(g_pCore->GetNetworkManager())
		{
			// Are we connected?
			if(g_pCore->GetNetworkManager()->IsConnected())
			{
				RakNet::BitStream bitStream;
				RakNet::RakString strInput;

				// Check if we have a valid string(otherwise if we don't check it -> crash)
				std::string Checkstring;
				Checkstring.append(m_strInputText.Get());
				for(int i = 0; i < m_strInputText.GetLength(); i++)
				{
					if(Checkstring[i] >= 'a' && Checkstring[i] <= 'z')
						continue;
					if(Checkstring[i] >= 'A' && Checkstring[i] <= 'Z')
						continue;
					if(Checkstring[i] >= '0' && Checkstring[i] <= '9')
						continue;
					if(Checkstring[i] == '[' || Checkstring[i] == ']' || Checkstring[i] == '(' || Checkstring[i] == ')')
						continue;
					if(Checkstring[i] == '_' || Checkstring[i] == ' ')
						continue;
					if(Checkstring[i] == '.' || Checkstring[i] == ':' || Checkstring[i] == ',' || Checkstring[i] == ';');
						continue;

					return Output("Unkown message input, please use only words from a-z", false);
				}

				// Is this a command?
				if(bIsCommand)
				{
					// Write 1
					bitStream.Write1();

					// Set the input minus the command character
					strInput = (m_strInputText.Get() + 1);
				}
				else
				{
					// Write 0
					bitStream.Write0();

					// Set the input
					strInput = m_strInputText.Get();

					// 
					Outputf(true, "#%s%s#FFFFFF: %s", CString::DecimalToString(g_pCore->GetGame()->GetLocalPlayer()->GetColor()).Get(), g_pCore->GetNick().Get(), m_strInputText.Get());
				}

				// Write the input
				bitStream.Write(strInput.C_String());

				// Send it to the server
				g_pCore->GetNetworkManager()->Call(GET_RPC_CODEX(RPC_PLAYER_CHAT), &bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, true);

				// Add this message to the history
				AddToHistory();
			}
		}
	}
}

void CChat::SetInput(CString strText)
{
	// Clear the current line
	m_InputLine.Clear();

	CColor color = m_InputTextColor;
	const char * szRemainingText = m_InputLine.Format(strText.Get(), (CHAT_WIDTH - m_InputLine.m_Prefix.GetWidth()), color, false);

	CChatLine * pLine = NULL;

	while(szRemainingText && m_InputLine.m_ExtraLines.size() < 2)
	{
		m_InputLine.m_ExtraLines.resize(m_InputLine.m_ExtraLines.size() + 1);
		CChatLine& line = (m_InputLine.m_ExtraLines.back());
		szRemainingText = line.Format(szRemainingText, CHAT_WIDTH, color, false);
	}

	if(strText != m_strInputText)
		m_strInputText = strText;

	if(szRemainingText)
		m_strInputText.Resize((szRemainingText - strText.Get()));
}

void CChat::ScrollUp()
{
	// Are there enough lines to scroll up?
	if (m_uiMostRecentLine + m_iRenderLines + m_uiScrollOffset < CHAT_MAX_LINES)
		m_uiScrollOffset = m_uiScrollOffset + m_iRenderLines;
}

void CChat::ScrollDown()
{
	// Are there any lines to scroll down?
	if (m_uiScrollOffset > 0)
		m_uiScrollOffset = m_uiScrollOffset - m_iRenderLines;
	
	// Is our index lower than the current line?
	if (m_uiScrollOffset < m_uiMostRecentLine)
		m_uiScrollOffset = 0;
}
void CChat::AddToHistory()
{
	// Align the history
	AlignHistory();

	// Add the current mesasge to the history
	m_strHistory[0] = GetInput();

	// Reset the current history
	m_iCurrentHistory = -1;

	// Increase the total history
	if(m_iTotalHistory < CHAT_MAX_HISTORY)
		m_iTotalHistory++;
}

void CChat::AlignHistory()
{
	// Loop through all the history items
	for(unsigned int i = (CHAT_MAX_HISTORY - 1); i; i--)
	{
		// Align the current chat history item
		m_strHistory[i] = m_strHistory[i - 1];
	}
}

void CChat::HistoryUp()
{
	// Is there any history to move to?
	if(m_iCurrentHistory < CHAT_MAX_HISTORY && ((m_iTotalHistory - 1) > m_iCurrentHistory))
	{
		if(m_iCurrentHistory == -1)
			m_strInputHistory = GetInput();

		// Increase the current history
		m_iCurrentHistory++;

		// Is there no history here?
		if(m_strHistory[m_iCurrentHistory].GetLength() == 0)
		{
			// Decrease the current history
			m_iCurrentHistory--;
		}

		// Set the input
		SetInput(m_strHistory[m_iCurrentHistory]);
	}
}

void CChat::HistoryDown()
{
	// Can we move down?
	if(m_iCurrentHistory > -1)
	{
		// Decrease the current history
		m_iCurrentHistory--;

		//
		if(m_iCurrentHistory == -1)
			SetInput(m_strInputHistory);
		else
			SetInput(m_strHistory[m_iCurrentHistory]);
	}
}

CChatLine::CChatLine()
{
	m_bActive = false;
}

bool CChatLine::IsColorCode(const char * szColorCode)
{
	if(*szColorCode != '#')
		return false;

	bool bValid = true;

	for(int i = 0; i < 6; i++)
	{
		char c = szColorCode[ 1 + i ];
		if (!isdigit ((unsigned char)c) && (c < 'A' || c > 'F') && (c < 'a' || c > 'f'))
        {
            bValid = false;
            break;
        }
	}

	return bValid;
}

CString CChatLine::RemoveColorCodes(const char * szText)
{
	bool bLastChar = false;

	CString strStrippedText = CString("");
	CString strText = CString(szText);

	for (int idx = 0; idx < strlen(szText); idx++)
	{
		if (IsColorCode(strText.Substring(idx, 7).Get()))
		{
			idx += 6; // Skip the code
		}
		else
			strStrippedText.Append(strText.Substring(idx, 1).Get());
	}
	return strStrippedText;
}

const char * CChatLine::Format(const char * szText, float fWidth, CColor& color, bool bColorCoded)
{
	float fCurrentWidth = 0.0f;
	m_Sections.clear();

	const char * szSectionStart = szText;
	const char * szSectionEnd = szText;
	const char * szLastWrapPoint = szText;
	 
	bool bLastSection = false;

	while(!bLastSection)
	{ 
		CChatLineSection section;

		section.SetColor(color);

		//if((m_Sections.size() +1) > 1 && bColorCoded)
		//	szSectionEnd += 7;
		 
		szSectionStart = szSectionEnd;
		szLastWrapPoint = szSectionStart;
		 
		while(true)
		{
			float fCharWidth = CChat::GetCharacterWidth(*szSectionEnd);
			if(*szSectionEnd == '\0' || *szSectionEnd == '\n' || (fCurrentWidth + fCharWidth) > fWidth)
			{
				bLastSection = true;
				break;
			} 
			if(bColorCoded && IsColorCode(szSectionEnd))
			{
				unsigned long ulColor = 0;
				sscanf_s(szSectionEnd + 1, "%06x", &ulColor);
				color = CColor(((ulColor >> 16) & 0xFF), ((ulColor >> 8) & 0xFF), (ulColor & 0xFF), 255);
				
				break;
			} 
			if(isspace((unsigned char) *szSectionEnd) || ispunct((unsigned char) *szSectionEnd))
			{
				szLastWrapPoint = szSectionEnd;
			}
			 
			fCurrentWidth += fCharWidth;
			szSectionEnd++;
		}
		 
		section.m_strText.assign(szSectionStart, szSectionEnd - szSectionStart);

		if(bColorCoded && IsColorCode(szSectionEnd))
			szSectionEnd += 7;

		if(section.m_strText != "")
			m_Sections.push_back(section);

		if(m_Sections.size() > CHAT_MAX_LINES)
		{
			m_Sections.pop_front();
		}
	}

	if(*szSectionEnd == '\0')
	{
		return NULL;
	}
	else if(*szSectionEnd == '\n')
	{
		return szSectionEnd + 1;
	}
	else
	{ 
		// Word wrap
		if(szLastWrapPoint == szSectionStart)
		{
			if(szLastWrapPoint == szText)
				return szSectionEnd;
			else
				m_Sections.pop_back();
		}
		else
		{ 
			((m_Sections.back())).m_strText.resize(szLastWrapPoint - szSectionStart);
		}
		return szLastWrapPoint;
	}
}

void CChatLine::Draw(float fX, float fY, unsigned char ucAlpha, bool bShadow)
{
	float fOffsetX = fX;
	if(m_Sections.size() == 1)
	{
		auto section = *m_Sections.begin();
		section.Draw(fOffsetX, fY, ucAlpha, bShadow);
		fOffsetX += section.GetWidth();
	}
	else
	for(auto pChatLine : m_Sections)
	{
		pChatLine.Draw(fOffsetX, fY, ucAlpha, bShadow);
		fOffsetX += pChatLine.GetWidth();
	}
//	}
}

float CChatLine::GetWidth()
{
	if(m_Sections.size() < 1)
		return 0.0f;

	float fWidth = 0.0f;
    for(auto pChatLine:m_Sections)
    {
        fWidth += pChatLine.GetWidth ();
    }

    return fWidth;
}

void CChatInputLine::Draw(float fX, float fY, unsigned char ucAlpha, bool bShadow)
{
	CColor colPrefix;
	m_Prefix.GetColor(colPrefix);

	if(colPrefix.A > 0)
		m_Prefix.Draw(fX, fY, colPrefix.A, bShadow);

	CChat * g_pChat = g_pCore->GetChat();
	if(g_pChat->GetInputTextColor().A > 0 && m_Sections.size() > 0)
	{
		m_Sections.begin()->Draw(fX + m_Prefix.GetWidth(), fY, g_pChat->GetInputTextColor().A, bShadow);

		float fLineDifference = CChat::GetFontHeight();

		//for(auto pExtraLine:m_ExtraLines)
		//{
		//	fY += fLineDifference;
		//	pExtraLine.Draw(fX, fY, g_pChat->GetInputTextColor().A, bShadow);
		//}

		// Cleanup lines 


		//if(m_Sections.size() > CHAT_MAX_LINES) {
		//	int iCount = 0;
		//	for(auto pLine : m_Sections) {
		//		iCount++;

		//		if(iCount > CHAT_MAX_LINES)
		//			m_Sections.remove(pLine);
		//	}
		//}
	}
}

void CChatInputLine::Clear()
{
	m_Sections.clear();
	m_ExtraLines.clear();
}

CChatLineSection::CChatLineSection()
{
	m_fCachedWidth = -1.0f;
	m_uiCachedLength = 0;
	m_fCachedOnScaleX = 0.0f;
}

CChatLineSection::CChatLineSection(const CChatLineSection& other)
{
	*this = other;
}

CChatLineSection& CChatLineSection::operator = (const CChatLineSection& other)
{
	m_strText = other.m_strText;
	m_Color = other.m_Color;
	m_fCachedWidth = other.m_fCachedWidth;
	m_uiCachedLength = other.m_uiCachedLength;
	m_fCachedOnScaleX = other.m_fCachedOnScaleX;
	return *this;
}

void CChatLineSection::Draw(float fX, float fY, unsigned char ucAlpha, bool bShadow)
{
	if(!m_strText.empty() && ucAlpha > 0)
	{
		g_pCore->GetGraphics()->DrawText(fX, fY, D3DCOLOR_ARGB(ucAlpha, m_Color.R, m_Color.G, m_Color.B), 1.0f, 5, DT_NOCLIP, bShadow, m_strText.c_str());
	}
}

float CChatLineSection::GetWidth()
{
	if (m_fCachedWidth < 0.0f || m_strText.size () != m_uiCachedLength || g_pCore->GetChat()->GetPosition() != m_fCachedOnScaleX)
    {
        m_fCachedWidth = 0.0f;
        for (unsigned int i = 0; i < m_strText.size (); i++)
        {
            m_fCachedWidth += CChat::GetCharacterWidth(m_strText[ i ]);            
        }
        m_uiCachedLength = m_strText.size ();
		m_fCachedOnScaleX = g_pCore->GetChat()->GetPosition();
    }
    return m_fCachedWidth;
}