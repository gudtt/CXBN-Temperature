//	Listener.cpp - Sample application for CSerial
//
//	Copyright (C) 1999-2003 Ramon de Klein (Ramon.de.Klein@ict.nl)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#define STRICT
#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "Serial.h"

#define sync1       0
#define sync2       1
#define temperature 2


enum { EOF_Char = 27 };

int ShowError (LONG lError, LPCTSTR lptszMessage)
{
	// Generate a message text
	TCHAR tszMessage[256];
	wsprintf(tszMessage,_T("%s\n(error code %d)"), lptszMessage, lError);

	// Display message-box and return with an error-code
	::MessageBox(0,tszMessage,_T("Listener"), MB_ICONSTOP|MB_OK);
	return 1;
}

int __cdecl _tmain (int /*argc*/, char** /*argv*/)
{
    CSerial serial;
	LONG    lLastError = ERROR_SUCCESS;

	printf("Temperature Reading Program\n");

    // Attempt to open the serial port (COM1)
    lLastError = serial.Open(_T("COM4"),0,0,false);
	if (lLastError != ERROR_SUCCESS)
		return ::ShowError(serial.GetLastError(), _T("Unable to open COM-port"));

    // Setup the serial port (9600,8N1, which is the default setting)
    lLastError = serial.Setup(CSerial::EBaud115200,CSerial::EData8,CSerial::EParNone,CSerial::EStop1);
	if (lLastError != ERROR_SUCCESS)
		return ::ShowError(serial.GetLastError(), _T("Unable to set COM-port setting"));

    // Register only for the receive event
    lLastError = serial.SetMask(CSerial::EEventBreak |
								CSerial::EEventCTS   |
								CSerial::EEventDSR   |
								CSerial::EEventError |
								CSerial::EEventRing  |
								CSerial::EEventRLSD  |
								CSerial::EEventRecv);
	if (lLastError != ERROR_SUCCESS)
		return ::ShowError(serial.GetLastError(), _T("Unable to set COM-port event mask"));

	// Use 'non-blocking' reads, because we don't know how many bytes
	// will be received. This is normally the most convenient mode
	// (and also the default mode for reading data).
    lLastError = serial.SetupReadTimeouts(CSerial::EReadTimeoutNonblocking);
	if (lLastError != ERROR_SUCCESS)
		return ::ShowError(serial.GetLastError(), _T("Unable to set COM-port read timeout."));

    // Keep reading data, until an EOF (CTRL-Z) has been received
	bool fContinue = true;
	do
	{
		char check;

		printf("Please type 'R' to recieve temperature data: \n");
		check = getchar();

		if(check == 'R')
		{
			lLastError = serial.Write("S");
		}
		if(check &= ~'R')
		{
			printf("Invalid Command.\n");
		}

		// Wait for an event
		lLastError = serial.WaitEvent();
		if (lLastError != ERROR_SUCCESS)
			return ::ShowError(serial.GetLastError(), _T("Unable to wait for a COM-port event."));

		// Save event
		const CSerial::EEvent eEvent = serial.GetEventType();

		// Handle break event
		if (eEvent & CSerial::EEventBreak)
		{
			printf("\n### BREAK received ###\n");
		}

		// Handle CTS event
		if (eEvent & CSerial::EEventCTS)
		{
			printf("\n### Clear to send %s ###\n", serial.GetCTS()?"on":"off");
		}

		// Handle DSR event
		if (eEvent & CSerial::EEventDSR)
		{
			printf("\n### Data set ready %s ###\n", serial.GetDSR()?"on":"off");
		}

		// Handle error event
		if (eEvent & CSerial::EEventError)
		{
			printf("\n### ERROR: ");
			switch (serial.GetError())
			{
			case CSerial::EErrorBreak:		printf("Break condition");			break;
			case CSerial::EErrorFrame:		printf("Framing error");			break;
			case CSerial::EErrorIOE:		printf("IO device error");			break;
			case CSerial::EErrorMode:		printf("Unsupported mode");			break;
			case CSerial::EErrorOverrun:	printf("Buffer overrun");			break;
			case CSerial::EErrorRxOver:		printf("Input buffer overflow");	break;
			case CSerial::EErrorParity:		printf("Input parity error");		break;
			case CSerial::EErrorTxFull:		printf("Output buffer full");		break;
			default:						printf("Unknown");					break;
			}
			printf(" ###\n");
		}

		// Handle ring event
		if (eEvent & CSerial::EEventRing)
		{
			printf("\n### RING ###\n");
		}

		// Handle RLSD/CD event
		if (eEvent & CSerial::EEventRLSD)
		{
			printf("\n### RLSD/CD %s ###\n", serial.GetRLSD()?"on":"off");
		}

		// Handle data receive event
		if (eEvent & CSerial::EEventRecv)
		{
			// Read data, until there is nothing left
			DWORD dwBytesRead = 0;
			char szBuffer[101];
			do
			{
				// Read data from the COM-port
				lLastError = serial.Read(szBuffer,sizeof(szBuffer)-1,&dwBytesRead);
				if (lLastError != ERROR_SUCCESS)
					return ::ShowError(serial.GetLastError(), _T("Unable to read from COM-port."));

				if (dwBytesRead > 0)
				{
					unsigned char uart_state=sync1;
					unsigned char i;
					int k = 0,j = 0,l = 0, m=0;
					float tempdata[8];
					int value=0;
					unsigned char ck_a = 0x00;
					unsigned char ck_b = 0x00;

					for (i = 0; i < dwBytesRead; i++)
					{
						switch (uart_state)
						{
						case sync1:
							if(szBuffer[i] == 'T')
							{
								uart_state = sync2;
							}
							break;
						case sync2:
						if(szBuffer[i] == 'e')
							{
								uart_state = temperature;
								ck_a = 0;
								ck_b = 0;
							}
							break; 
						case temperature:
							unsigned char test_cka;
							unsigned char test_ckb;
							ck_a = ck_a + szBuffer[i];
							ck_b = ck_a + ck_b;							
							value = (value>>8) & ~0xff000000;
							value = (szBuffer[i]<<24) | value;
							k = k+1;
							if(k>=4)
							{
								k=0;
								memcpy(&tempdata[j++],&value, sizeof (value));
								value = 0;
							}
								if( j >=8 )
								{
									test_cka = szBuffer[++i];
									test_ckb = szBuffer[++i];
									if( (test_cka == ck_a) & (test_ckb == ck_b) )
									{
										for(l=0; l<8; l++)
										{
										printf("temperature %i: ", l);
										printf("%f\n", tempdata[l]);
										}
									}
									printf("%x : %x,", ck_a, test_cka);
									printf("%x : %x\n", ck_b, test_ckb);
									j = 0;
									uart_state = sync1;
								}
							break;
						default:
							uart_state = sync1;
							break;	
						}
					}
				}
			}
		    while (dwBytesRead == 0);
		}
	}
	while (fContinue);

    // Close the port again
    serial.Close();
    return 0;
}