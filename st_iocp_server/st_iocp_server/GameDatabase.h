#pragma once
#include"default.h"
#include <windows.h>  
#include <string>
#include <atlstr.h>
#include"2021_ерга_protocol.h"

#define UNICODE  
#include <sqlext.h>  
using namespace std;

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
                    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                        retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
                        cout << "ODBC connect ok" << endl;
                    }
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

    bool IdCheck(char* id) {
        SQLRETURN retcode;

        //SQLINTEGER nCharId;
        SQLWCHAR nCharId[MAX_ID_LEN];
        SQLLEN  cbCharID = 0;

        CString str;
        str.Format(L"EXEC select_id %s", (CString)id);

        if (SQL_SUCCESS == SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt)) {
            retcode = SQLExecDirect(hstmt, (SQLWCHAR*)(const wchar_t*)str, SQL_NTS);
            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                //cout << "select id ok" << endl;

                // Bind columns 1
                retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, &nCharId, MAX_ID_LEN, &cbCharID);

                // Fetch and print each row of data. On an error, display a message and exit.  
                for (int i = 0; ; i++) {
                    retcode = SQLFetch(hstmt);
                    if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
                        HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
                        //break;
                    }
                    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
                    {
                        if (strcmp((char*)nCharId, id))
                            return true;
                        return false;
                    }
                    else {
                        break;  // end of data
                    }
                }
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					SQLCancel(hstmt);
					SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
				}
            }
        }
        
        return false;
    }

    bool addID(char* id) {
        SQLRETURN retcode;

        //SQLINTEGER nCharId;
        SQLWCHAR nCharId[MAX_ID_LEN];
        SQLLEN  cbCharID = 0;

        CString str;
        str.Format(L"EXEC insert_id %s", (CString)id);

        if (SQL_SUCCESS == SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt)) {
            retcode = SQLExecDirect(hstmt, (SQLWCHAR*)(const wchar_t*)str, SQL_NTS);
            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                //cout << "insert id ok" << endl;

                // Bind columns 1
                retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, &nCharId, MAX_ID_LEN, &cbCharID);

                // Fetch and print each row of data. On an error, display a message and exit.  
                for (int i = 0; ; i++) {
                    retcode = SQLFetch(hstmt);
                    if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
                        HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
                        //break;
                    }
                    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
                    {
                        if (strcmp((char*)nCharId, id))
                            return true;
                        return false;
                    }
                    else {
                        break;  // end of data
                    }
                }

                if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                    SQLCancel(hstmt);
                    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
                }
            }
        }

        return false;
    }
};

