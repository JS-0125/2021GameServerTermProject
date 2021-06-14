#pragma once
#include"default.h"
#include <windows.h>  
#include < string.h >
#include <atlstr.h>

#include"2021_ерга_protocol.h"

#define UNICODE  
#include <sqlext.h>  
using namespace std;

extern vector<vector<bool>> can_move;

class GameDatabase
{
public:
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;


	GameDatabase() {
		SQLRETURN retcode;

		setlocale(LC_ALL, "korean");
		wcout.imbue(std::locale("korean"));

		// Allocate environment handle  
		retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

		// Set the ODBC version environment attribute  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

			// Allocate connection handle  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

				// Set login timeout to 5 seconds  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

					// Connect to data source  
					retcode = SQLConnect(hdbc, (SQLWCHAR*)L"GameServerDatabase", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

					// Allocate statement handle  
					/*if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
						retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
						cout << "ODBC connect ok" << endl;
					}*/
				}
			}
		}
	}

	~GameDatabase() {
		//// Process data  
		//if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		//    SQLCancel(hstmt);
		//    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		//}

		SQLCancel(hstmt);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

		SQLDisconnect(hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
	}

	void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
	{
		SQLSMALLINT iRec = 0;
		SQLINTEGER iError;
		WCHAR wszMessage[1000];
		WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
		if (RetCode == SQL_INVALID_HANDLE) {
			fwprintf(stderr, L"Invalid handle!\n");
			return;
		}
		while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
			(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
			// Hide data truncated..
			if (wcsncmp(wszState, L"01004", 5)) {
				fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
			}
		}
	}

	bool IdCheck(const char* id, sc_packet_login_ok& p) {
		SQLRETURN retcode;
		bool returnValue = false;

		SQLWCHAR nCharId[MAX_ID_LEN];
		SQLINTEGER nCharExp, nCharLevel, nCharX, nCharY;
		SQLLEN  cbCharID = 0, cbCharExp = 0, cbCharLevel = 0, cbCharX = 0, cbCharY = 0;

		CString str;
		str.Format(L"EXEC select_id %s", (CString)id);
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
		retcode = SQLExecDirect(hstmt, (SQLWCHAR*)(const wchar_t*)str, SQL_NTS);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			cout << "select id ok" << endl;

			// Bind columns
			retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, &nCharId, MAX_ID_LEN, &cbCharID);
			retcode = SQLBindCol(hstmt, 2, SQL_C_ULONG, &nCharX, 100, &cbCharX);
			retcode = SQLBindCol(hstmt, 3, SQL_C_ULONG, &nCharY, 100, &cbCharY);
			retcode = SQLBindCol(hstmt, 4, SQL_C_ULONG, &nCharExp, 100, &cbCharExp);
			retcode = SQLBindCol(hstmt, 5, SQL_C_ULONG, &nCharLevel, 100, &cbCharLevel);
			// Fetch and print each row of data. On an error, display a message and exit.  
			for (int i = 0; ; i++) {
				retcode = SQLFetch(hstmt);
				if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
					HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
					//break;
				}
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
					char tmp[20];
					sprintf_s(tmp, (const char*)nCharId -1);

					if (strcmp(id, tmp)) {
						if (nCharLevel == 0) {
							while (1) {
								int tmpX = rand() % WORLD_WIDTH, tmpY = rand() % WORLD_HEIGHT;
								if (can_move[tmpY][tmpX]) {
									p.x = tmpX;
									p.y = tmpY;
									break;
								}
							}
							p.LEVEL = 1;
							p.EXP = 0;
						}
						else {
							p.x = nCharX;
							p.y = nCharY;
							p.LEVEL = nCharLevel;
							p.EXP = nCharExp;
						}
						returnValue = true;
					}
					else
						returnValue = false;

				}
				else {
					break;  // end of data
				}
			}
		}
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

		return returnValue;
	}

	bool addID(const char* id, sc_packet_login_ok& p) {
		SQLRETURN retcode;
		bool returnValue = false;
		//SQLINTEGER nCharId;
		SQLWCHAR nCharId[MAX_ID_LEN];
		SQLINTEGER nCharExp, nCharLevel, nCharX, nCharY;
		SQLLEN  cbCharID = 0, cbCharExp = 0, cbCharLevel = 0, cbCharX = 0, cbCharY = 0;

		CString str;
		str.Format(L"EXEC insert_id %s, %d, %d, %d, %d", (CString)id, p.x, p.y, p.EXP, p.LEVEL);
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
		retcode = SQLExecDirect(hstmt, (SQLWCHAR*)(const wchar_t*)str, SQL_NTS);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			cout << "insert id ok" << endl;

			// Bind columns
			retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, &nCharId, MAX_ID_LEN, &cbCharID);
			retcode = SQLBindCol(hstmt, 2, SQL_C_ULONG, &nCharX, 100, &cbCharX);
			retcode = SQLBindCol(hstmt, 3, SQL_C_ULONG, &nCharY, 100, &cbCharY);
			retcode = SQLBindCol(hstmt, 4, SQL_C_ULONG, &nCharExp, 100, &cbCharExp);
			retcode = SQLBindCol(hstmt, 5, SQL_C_ULONG, &nCharLevel, 100, &cbCharLevel);

			// Fetch and print each row of data. On an error, display a message and exit.  
			for (int i = 0; ; i++) {
				retcode = SQLFetch(hstmt);
				if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
					HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
					//break;
				}
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
					char tmp[20];
					sprintf_s(tmp, (const char*)nCharId - 1);

					if (strcmp(id, tmp)) {
						if (nCharLevel == 0) {
							while (1) {
								int tmpX = rand() % WORLD_WIDTH, tmpY = rand() % WORLD_HEIGHT;
								if (can_move[tmpY][tmpX]) {
									p.x = tmpX;
									p.y = tmpY;
									break;
								}
							}
							p.LEVEL = 1;
							p.EXP = 0;
						}
						else {
							p.x = nCharX;
							p.y = nCharY;
							p.LEVEL = nCharLevel;
							p.EXP = nCharExp;
						}
						returnValue = true;
					}
					else
						returnValue = false;
				}
				else {
					break;  // end of data
				}
			}
			
		}
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

		return returnValue;
	}

	bool updateChar(const char* id, sc_packet_login_ok& p) {
		SQLRETURN retcode;
		bool returnValue = false;

		//SQLINTEGER nCharId;
		SQLWCHAR nCharId[MAX_ID_LEN];
		SQLINTEGER nCharExp, nCharLevel, nCharX, nCharY;
		SQLLEN  cbCharID = 0, cbCharExp = 0, cbCharLevel = 0, cbCharX = 0, cbCharY = 0;

		CString str;
		// CharPositionX = @Param2, CharPositionY = @Param3, CharExp = @Param4, CharLevel = @Param5
		str.Format(L"EXEC update_char %s, %d, %d, %d, %d", (CString)id, p.x, p.y, p.EXP, p.LEVEL);

		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
		retcode = SQLExecDirect(hstmt, (SQLWCHAR*)(const wchar_t*)str, SQL_NTS);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			returnValue = true;
		}
		else if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
			HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
			//break;
		}
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

		return returnValue;
	}
};

