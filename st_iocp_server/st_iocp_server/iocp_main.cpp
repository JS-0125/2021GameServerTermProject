#include <iostream>
#include <unordered_set>
#include <thread>
#include <vector>
#include <mutex>
#include <array>
#include <queue>
#include<atomic>
using namespace std;
#include <WS2tcpip.h>
#include <MSWSock.h>


extern "C" {
#include"lua.h"
#include "lauxlib.h"
#include"lualib.h"
}

#pragma comment( lib, "lua54.lib" )

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

#include "2021_����_protocol.h"

constexpr int MAX_BUFFER = 1024;
constexpr int SECTOR_RADIUS = 20;


enum OP_TYPE  { OP_RECV, OP_SEND, OP_ACCEPT, OP_RANDOM_MOVE, OP_ATTACK, OP_PLAYER_APROACH, OP_RUNAWAY};
enum PL_STATE { PLST_FREE, PLST_CONNECTED, PLST_INGAME };
//enum DIRECTION { D_N, D_S, D_W, D_E, D_NO };

struct EX_OVER
{
	WSAOVERLAPPED	m_over;
	WSABUF			m_wsabuf[1];
	unsigned char	m_packetbuf[MAX_BUFFER];
	OP_TYPE			m_op;
	SOCKET			m_csocket;					// OP_ACCEPT������ ���
};

struct TIMER_EVENT {
	int object;
	OP_TYPE e_type;
	chrono::system_clock::time_point start_time;
	int target_id;

	constexpr bool operator < (const TIMER_EVENT& L) const
	{
		return (start_time > L.start_time);
	}
};


struct S_OBJECT
{
	mutex  m_slock;
	atomic<PL_STATE> m_state;
	int		id;
	EX_OVER m_recv_over;
	char m_name[100];
	short	x, y;
	short sectorX, sectorY;
};

struct PLAYER : S_OBJECT {
	SOCKET m_socket;
	int m_prev_size;
	int move_time;
	unordered_set<int> m_view_list;
	mutex m_vl;
};

struct NPC : S_OBJECT {
	atomic< bool> is_active;

	lua_State* L;
	mutex m_sl;
};

struct Sector {
	mutex m_playerLock;
	unordered_set<int> players;
};

struct pair_hash
{
	template <class T1, class T2>
	std::size_t operator () (std::pair<T1, T2> const& pair) const
	{
		std::size_t h1 = std::hash<T1>()(pair.first);
		std::size_t h2 = std::hash<T2>()(pair.second);

		return h1 ^ h2;
	}
};

priority_queue <TIMER_EVENT> timer_queue;
mutex timer_l;

constexpr int SERVER_ID = 0;
Sector sector[WORLD_HEIGHT / SECTOR_RADIUS][WORLD_WIDTH / SECTOR_RADIUS];
array <S_OBJECT*, MAX_USER + 1> objects;

HANDLE h_iocp;


unordered_set<pair<int, int>, pair_hash> sector_update(int p_id)
{
	int sectorY = objects[p_id]->y / SECTOR_RADIUS, sectorX = objects[p_id]->x / SECTOR_RADIUS;

	// sector update
	if (sector[sectorY][sectorX].players.count(p_id) == 0) {
		sector[sectorY][sectorX].m_playerLock.lock();
		sector[sectorY][sectorX].players.insert(p_id);
		sector[sectorY][sectorX].m_playerLock.unlock();

		sector[objects[p_id]->sectorY][objects[p_id]->sectorX].m_playerLock.lock();
		sector[objects[p_id]->sectorY][objects[p_id]->sectorX].players.erase(p_id);
		sector[objects[p_id]->sectorY][objects[p_id]->sectorX].m_playerLock.unlock();

		objects[p_id]->sectorX = sectorX;
		objects[p_id]->sectorY = sectorY;
	}

	unordered_set<pair<int, int>, pair_hash> serctorIndex;
	int x = objects[p_id]->x, y = objects[p_id]->y;

	if (x <= VIEW_RADIUS)
		x = VIEW_RADIUS;
	if (x >= WORLD_WIDTH - VIEW_RADIUS - 1)
		x = WORLD_WIDTH - VIEW_RADIUS - 1;
	if (y <= VIEW_RADIUS)
		y = VIEW_RADIUS;
	if (y >= WORLD_HEIGHT - VIEW_RADIUS - 1)
		y = WORLD_HEIGHT - VIEW_RADIUS - 1;

	serctorIndex.insert(pair<int, int>((y - VIEW_RADIUS) / SECTOR_RADIUS, (x - VIEW_RADIUS) / SECTOR_RADIUS));
	serctorIndex.insert(pair<int, int>((y - VIEW_RADIUS) / SECTOR_RADIUS, (x + VIEW_RADIUS) / SECTOR_RADIUS));
	serctorIndex.insert(pair<int, int>((y + VIEW_RADIUS) / SECTOR_RADIUS, (x - VIEW_RADIUS) / SECTOR_RADIUS));
	serctorIndex.insert(pair<int, int>((y + VIEW_RADIUS) / SECTOR_RADIUS, (x + VIEW_RADIUS) / SECTOR_RADIUS));

	return serctorIndex;
}

void add_event(int obj, int target_id, OP_TYPE ev_t, int delay_ms)
{
	using namespace chrono;
	TIMER_EVENT ev;
	ev.e_type = ev_t;
	ev.object = obj;
	ev.start_time = system_clock::now() + milliseconds(delay_ms);
	ev.target_id = target_id;
	timer_l.lock();
	timer_queue.push(ev);
	timer_l.unlock();
}

void wake_up_npc(int npc_id)
{
	if ((*static_cast<NPC*>(objects[npc_id])).is_active == false) {
		bool old_state = false;
		if (atomic_compare_exchange_strong(&(*static_cast<NPC*>(objects[npc_id])).is_active, &old_state, true))
			add_event(npc_id,-1, OP_RANDOM_MOVE, 1000);
	}
}

void put_sleep_npc(int npc_id)
{
	if ((*static_cast<NPC*>(objects[npc_id])).is_active == true) {
		bool old_state = true;
		atomic_compare_exchange_strong(&(*static_cast<NPC*>(objects[npc_id])).is_active, &old_state, false);
			//add_event(npc_id, -1, OP_RANDOM_MOVE, 1000);
	}
}

bool is_npc(int id)
{
	return id >= NPC_ID_START;
}

void disconnect(int p_id);

void display_error(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	cout << msg;
	wcout << lpMsgBuf << endl;
	LocalFree(lpMsgBuf);
}

bool can_see(int id_a, int id_b)
{
	return VIEW_RADIUS >= abs(objects[id_a]->x - objects[id_b]->x) && VIEW_RADIUS >= abs(objects[id_a]->y - objects[id_b]->y);

}

void send_packet(int p_id, void *p)
{
	int p_size = reinterpret_cast<unsigned char*>(p)[0];
	int p_type = reinterpret_cast<unsigned char*>(p)[1];
	//cout << "To client [" << p_id << "] : ";
	//cout << "Packet [" << p_type << "]\n";
	EX_OVER* s_over = new EX_OVER;
	s_over->m_op = OP_SEND;
	memset(&s_over->m_over, 0, sizeof(s_over->m_over));
	memcpy(s_over->m_packetbuf, p, p_size);
	s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR *>(s_over->m_packetbuf);
	s_over->m_wsabuf[0].len = p_size;
	int ret = WSASend((*static_cast<PLAYER*>(objects[p_id])).m_socket, s_over->m_wsabuf, 1,
		NULL, 0, &s_over->m_over, 0);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			display_error("WSASend : ", WSAGetLastError());
			disconnect(p_id);
		}
	}
}

void do_recv(int key)
{
	objects[key]->m_recv_over.m_wsabuf[0].buf =
		reinterpret_cast<char*>(objects[key]->m_recv_over.m_packetbuf)
		+ (*static_cast<PLAYER*>(objects[key])).m_prev_size;
	objects[key]->m_recv_over.m_wsabuf[0].len = MAX_BUFFER - (*static_cast<PLAYER*>(objects[key])).m_prev_size;
	memset(&objects[key]->m_recv_over.m_over, 0, sizeof(objects[key]->m_recv_over.m_over));
	DWORD r_flag = 0;
	int ret = WSARecv((*static_cast<PLAYER*>(objects[key])).m_socket, objects[key]->m_recv_over.m_wsabuf, 1,
		NULL, &r_flag, &objects[key]->m_recv_over.m_over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("WSARecv : ", WSAGetLastError());
	}
}

int get_new_player_id(SOCKET p_socket)
{
	for (int i = SERVER_ID + 1; i <= MAX_USER; ++i) {
		lock_guard<mutex> lg{ objects[i]->m_slock };
		if (PLST_FREE == objects[i]->m_state) {
			objects[i]->m_state = PLST_CONNECTED;
			(*static_cast<PLAYER*>(objects[i])).m_socket = p_socket;
			objects[i]->m_name[0] = 0;
			return i;
		}
	}
	return -1;
}

void send_login_ok_packet(int p_id)
{
	sc_packet_login_ok p;
	p.HP = 10;
	p.EXP = 0;
	p.id = p_id;
	p.LEVEL = 2;
	p.type = SC_LOGIN_OK;
	p.x = objects[p_id]->x = rand() % WORLD_WIDTH;
	p.y = objects[p_id]->y = rand() % WORLD_HEIGHT;
	p.size = sizeof(p);
	send_packet(p_id, &p);
}

void send_move_packet(int c_id, int p_id)
{
	sc_packet_position p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = SC_POSITION;
	p.x = objects[p_id]->x;
	p.y = objects[p_id]->y;
	p.move_time = (*static_cast<PLAYER*>(objects[p_id])).move_time;
	send_packet(c_id, &p);
}

void send_add_object(int c_id, int p_id)
{
	sc_packet_add_object p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = SC_ADD_OBJECT;
	p.x = objects[p_id]->x;
	p.y = objects[p_id]->y;
	p.obj_class = 0;
	strcpy_s(p.name, objects[p_id]->m_name);
	send_packet(c_id, &p);
}

void send_remove_object(int c_id, int p_id)
{
	sc_packet_remove_object p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	send_packet(c_id, &p);
}

void send_chat(int c_id, int p_id, const char* mess)
{
	sc_packet_chat p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = SC_CHAT;
	strcpy_s(p.message, mess);
	send_packet(c_id, &p);
}

void do_move(int p_id, char dir)
{
	auto& x = objects[p_id]->x;
	auto& y = objects[p_id]->y;
	switch (dir) {
	case 0: if (y> 0) y--; break;
	case 1: if (y < (WORLD_HEIGHT - 1)) y++; break;
	case 2: if (x > 0) x--; break;
	case 3: if (x < (WORLD_WIDTH - 1)) x++; break;
	}

	auto serctorIndex = sector_update(p_id);

	PLAYER* player = static_cast<PLAYER*>(objects[p_id]);

	unordered_set <int> old_vl;
	player->m_vl.lock();
	old_vl = player->m_view_list;
	player->m_vl.unlock();

	unordered_set <int> new_vl;


	for (auto& index : serctorIndex) {
		for (auto& pl_id : sector[index.first][index.second].players) {
			if (pl_id == p_id)
				continue;
			if (objects[pl_id]->m_state == PLST_INGAME && can_see(p_id, pl_id)) {
				new_vl.insert(pl_id);
				if (is_npc(pl_id)) {
					EX_OVER* ex_over = new EX_OVER;
					ex_over->m_op = OP_PLAYER_APROACH;
					*reinterpret_cast<int*> (ex_over->m_packetbuf) = p_id;
					PostQueuedCompletionStatus(h_iocp, 1, pl_id, &ex_over->m_over);
				}
			}
		}
	}

	/*for (auto& pl : objects) {
		if (pl.id == p_id) continue;
		if ((pl.m_state == PLST_INGAME) && can_see(p_id, pl.id)) {
			new_vl.insert(pl.id);
			if (is_npc(pl.id)) {
				EX_OVER* ex_over = new EX_OVER;
				ex_over->m_op = OP_PLAYER_APROACH;
				*reinterpret_cast<int*> (ex_over->m_packetbuf) = p_id;
				PostQueuedCompletionStatus(h_iocp, 1, pl.id, &ex_over->m_over);
			}
		}
	}*/

	send_move_packet(p_id, p_id);
	for (auto pl : new_vl) {
		if (0 == old_vl.count(pl)) {		// 1. ���� �þ߿� ������ ��ü
			player->m_vl.lock();
			player->m_view_list.insert(pl);
			player->m_vl.unlock();
			send_add_object(p_id, pl);

			if (false == is_npc(pl)) {
				PLAYER* otherPl = static_cast<PLAYER*>(objects[pl]);

				otherPl->m_vl.lock();
				if (0 == otherPl->m_view_list.count(p_id)) {
					otherPl->m_view_list.insert(p_id);
					otherPl->m_vl.unlock();
					send_add_object(pl, p_id);
				}
				else {
					otherPl->m_vl.unlock();
					send_move_packet(pl, p_id);
				}
			}
			else wake_up_npc(pl);
		}
		else {		// 2. ���� �þ߿��� �ְ� �� �þ߿��� �ִ� ���
			if (false == is_npc(pl)) {
				PLAYER* otherPl = static_cast<PLAYER*>(objects[pl]);

				otherPl->m_vl.lock();
				if (0 == otherPl->m_view_list.count(p_id)) {
					otherPl->m_view_list.insert(p_id);
					otherPl->m_vl.unlock();
					send_add_object(pl, p_id);
				}
				else {
					otherPl->m_vl.unlock();
					send_move_packet(pl, p_id);
				}
			}
		}
	}

	for (auto pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			// 3. �þ߿��� ����� ���
			player->m_vl.lock();
			player->m_view_list.erase(pl);
			player->m_vl.unlock();
			send_remove_object(p_id, pl);

			if (false == is_npc(pl)) {
				PLAYER* otherPl = static_cast<PLAYER*>(objects[pl]);

				otherPl->m_vl.lock();
				if (0 != otherPl->m_view_list.count(p_id)) {
					otherPl->m_view_list.erase(p_id);
					otherPl->m_vl.unlock();
					send_remove_object(pl, p_id);
				}
				else {
					otherPl->m_vl.unlock();
				}
			}
			else {

			}
		}
	}
}

void process_packet(int p_id, unsigned char* p_buf)
{
	switch (p_buf[1]) {
	case CS_LOGIN: {
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(p_buf);
		lock_guard <mutex> gl2{ objects[p_id]->m_slock };
		//strcpy_s(objects[p_id]->m_name, packet->name);

		send_login_ok_packet(p_id);
		objects[p_id]->m_state = PLST_INGAME;

		// sector
		int sectorX = objects[p_id]->x / SECTOR_RADIUS;
		int sectorY = objects[p_id]->y / SECTOR_RADIUS;
		objects[p_id]->sectorX = sectorX;
		objects[p_id]->sectorY = sectorY;

		sector[sectorY][sectorX].m_playerLock.lock();
		sector[sectorY][sectorX].players.insert(p_id);
		sector[sectorY][sectorX].m_playerLock.unlock();

		auto serctorIndex = sector_update(p_id);

		for (auto& index : serctorIndex) {
			//sector[index.first][index.second].m_playerLock.lock();
			for (auto& pl_id : sector[index.first][index.second].players) {
				if (p_id != pl_id) {
					lock_guard <mutex> gl{ objects[pl_id]->m_slock };
					if (PLST_INGAME == objects[pl_id]->m_state) {
						if (can_see(p_id, pl_id))
						{
							// �ٸ� �÷��̾ �þ� �ȿ� ���� ��
							if (is_npc(pl_id) == false) {
								// player
								PLAYER* player = static_cast<PLAYER*>(objects[p_id]);

								player->m_vl.lock();
								player->m_view_list.insert(pl_id);
								player->m_vl.unlock();
								send_add_object(p_id, pl_id);

								// �ٸ� �÷��̾�
								PLAYER* otherPl = static_cast<PLAYER*>(objects[pl_id]);

								otherPl->m_vl.lock();
								otherPl->m_view_list.insert(p_id);
								otherPl->m_vl.unlock();
								send_add_object(pl_id, p_id);
							}
							// npc�� ��
							else {
								// �þ� �ȿ� �ִ� npc �����
								wake_up_npc(pl_id);
								// player���� npc ����
								send_add_object(p_id, pl_id);
							}
						}
					}
				}
			}
		}

		/*for (auto& pl : objects) {
			if (p_id != pl.id) {
				lock_guard <mutex> gl{ pl.m_slock };
				if (PLST_INGAME == pl.m_state) {
					if (can_see(p_id, pl.id)) {
						objects[p_id].m_vl.lock();
						objects[p_id].m_view_list.insert(pl.id);
						objects[p_id].m_vl.unlock();
						send_add_object(p_id, pl.id);
						if (false == is_npc(pl.id)) {
							objects[pl.id].m_vl.lock();
							objects[pl.id].m_view_list.insert(p_id);
							objects[pl.id].m_vl.unlock();
							send_add_object(pl.id, p_id);
						}
						else {
							wake_up_npc(pl.id);
						}
					}
				}
			}
		}*/
	}
		break;
	case CS_MOVE: {
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(p_buf);
		(*static_cast<PLAYER*>(objects[p_id])).move_time = packet->move_time;
		do_move(p_id, packet->direction);
	}
		break;
	default:
		cout << "Unknown Packet Type from Client[" << p_id;
		cout << "] Packet Type [" << p_buf[1] << "]";
		while (true);
	}
}

void disconnect(int p_id)
{
	PLAYER* player = static_cast<PLAYER*>(objects[p_id]);

	{
		lock_guard <mutex> gl{ player->m_slock };
		if (PLST_FREE == player->m_state) return;
		closesocket(player->m_socket);
		player->m_state = PLST_FREE;
		player->m_vl.lock();
		player->m_view_list.clear();
		player->m_vl.unlock();
	}

	auto serctorIndex = sector_update(p_id);
	sector[player->sectorY][player->sectorX].m_playerLock.lock();
	sector[player->sectorY][player->sectorX].players.erase(p_id);
	sector[player->sectorY][player->sectorX].m_playerLock.unlock();

	for (auto& index : serctorIndex) {
		for (auto& pl_id : sector[index.first][index.second].players) {
			lock_guard<mutex> gl2{ player->m_slock };
			if (PLST_INGAME == player->m_state && player->m_view_list.count(p_id) != 0)
				send_remove_object(pl_id, p_id);
		}
	}
}

void do_npc_random_move(NPC& npc)
{
	unordered_set<int> old_vl;
	auto serctorIndex = sector_update(npc.id);

	for (auto& index : serctorIndex) {
		for (auto& obj_id : sector[index.first][index.second].players) {
			if (PLST_INGAME != objects[obj_id]->m_state) continue;
			if (is_npc(objects[obj_id]->id) == true) continue;
			if (can_see(npc.id, objects[obj_id]->id))
				old_vl.insert(objects[obj_id]->id);
		}
	}

	int x = npc.x;
	int y = npc.y;

	switch (rand() % 4) {
	case 0: if (x < (WORLD_WIDTH - 1)) x++; break;
	case 1: if (x > 0) x--; break;
	case 2: if (y < (WORLD_HEIGHT - 1)) y++; break;
	case 3:if (y > 0) y--; break;
	}
	npc.x = x;
	npc.y = y;

	serctorIndex = sector_update(npc.id);

	unordered_set<int> new_vl;
	for (auto& index : serctorIndex) {
		for (auto& obj_id : sector[index.first][index.second].players) {
			if (PLST_INGAME != objects[obj_id]->m_state) continue;
			if (is_npc(objects[obj_id]->id) == true) continue;
			if (can_see(npc.id, objects[obj_id]->id))
				new_vl.insert(objects[obj_id]->id);
		}
	}

	for (auto& p_id : new_vl) {
		if (0 == old_vl.count(p_id)) {
			// �÷��̾� �þ߿� ����
			PLAYER* player = static_cast<PLAYER*>(objects[p_id]);

			player->m_vl.lock();
			player->m_view_list.insert(npc.id);
			player->m_vl.unlock();
			send_add_object(p_id, npc.id);

		}
		else {
			// �÷��̾ ��� ���� ����
			send_move_packet(p_id, npc.id);
		}
	}


	for (auto& p_id : old_vl) {
		if (new_vl.count(p_id) == 0) {
			PLAYER* player = static_cast<PLAYER*>(objects[p_id]);

			player->m_vl.lock();
			if (player->m_view_list.count(p_id) != 0) {
				player->m_view_list.erase(npc.id);
				player->m_vl.unlock();
				send_remove_object(p_id, npc.id);
			}
			else
				player->m_vl.unlock();
		}
	}
}

void worker(HANDLE h_iocp, SOCKET l_socket)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR ikey;
		WSAOVERLAPPED* over;

		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes,
			&ikey, &over, INFINITE);

		int key = static_cast<int>(ikey);
		if (FALSE == ret) {
			if (SERVER_ID == key) {
				display_error("GQCS : ", WSAGetLastError());
				exit(-1);
			}
			else {
				display_error("GQCS : ", WSAGetLastError());
				disconnect(key);
			}
		}
		if ((key != SERVER_ID) && (0 == num_bytes)) {
			disconnect(key);
			continue;
		}
		EX_OVER* ex_over = reinterpret_cast<EX_OVER*>(over);

		switch (ex_over->m_op) {
		case OP_RECV: {
			PLAYER* player = static_cast<PLAYER*>(objects[key]);

			unsigned char* packet_ptr = ex_over->m_packetbuf;
			int num_data = num_bytes + player->m_prev_size;
			int packet_size = packet_ptr[0];

			while (num_data >= packet_size) {
				process_packet(key, packet_ptr);
				num_data -= packet_size;
				packet_ptr += packet_size;
				if (0 >= num_data) break;
				packet_size = packet_ptr[0];
			}
			player->m_prev_size = num_data;
			if (0 != num_data)
				memcpy(ex_over->m_packetbuf, packet_ptr, num_data);
			do_recv(key);
		}
					break;
		case OP_SEND:
			delete ex_over;
			break;
		case OP_ACCEPT: {
			int c_id = get_new_player_id(ex_over->m_csocket);
			PLAYER* player = static_cast<PLAYER*>(objects[c_id]);

			if (-1 != c_id) {
				player->m_recv_over.m_op = OP_RECV;
				player->m_prev_size = 0;
				CreateIoCompletionPort(
					reinterpret_cast<HANDLE>(player->m_socket), h_iocp, c_id, 0);
				do_recv(c_id);
			}
			else {
				closesocket(player->m_socket);
			}

			memset(&ex_over->m_over, 0, sizeof(ex_over->m_over));
			SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			ex_over->m_csocket = c_socket;
			AcceptEx(l_socket, c_socket,
				ex_over->m_packetbuf, 0, 32, 32, NULL, &ex_over->m_over);
		}
			break;
		case OP_RANDOM_MOVE:
			do_npc_random_move(*static_cast<NPC*>(objects[key]));
			add_event(key,-1, OP_RANDOM_MOVE, 1000);
			delete ex_over;
			break;
		case OP_RUNAWAY: {
			//do_npc_random_move(*static_cast<NPC*>(objects[key]));
			int player_id = *reinterpret_cast<int*>(ex_over->m_packetbuf);
			if(*reinterpret_cast<int*>(ex_over->m_packetbuf) != -1)
				send_chat(player_id, key, "BYE");
			delete ex_over;
		}
			break;
		case OP_ATTACK:
			delete ex_over;
			break;
		case OP_PLAYER_APROACH:
			auto& npc = *static_cast<NPC*>(objects[key]);
			npc.m_sl.lock();
			int move_player = *reinterpret_cast<int*>( ex_over->m_packetbuf);
			lua_State* L = npc.L;
			lua_getglobal(L, "event_player_move");
			lua_pushnumber(L, move_player);
			lua_pcall(L, 1, 1, 0);
			npc.m_sl.unlock();

			delete ex_over;
			break;
		}
	}
}

/*
void do_ai()
{
	using namespace chrono;

	for (;;) {
		auto start_t = chrono::system_clock::now();
		for (auto& npc : objects) {
			if (true == is_npc(npc.id)) {
				do_npc_random_move(npc);
			}
		}
		auto end_t = chrono::system_clock::now();
		auto ai_time = end_t - start_t;
		cout << "AI Exec Time : "
			<< duration_cast<milliseconds>(ai_time).count()
			<< "ms.\n";
		if (end_t < start_t + 1s)
			this_thread::sleep_for(start_t + 1s - end_t);
	}
}
*/

void do_timer()
{
	using namespace chrono;

	for (;;) {
		timer_l.lock();
		if ((false == timer_queue.empty()) 
			&& (timer_queue.top().start_time <= system_clock::now())) {
			TIMER_EVENT ev = timer_queue.top();
			timer_queue.pop();
			timer_l.unlock();
			if (ev.e_type == OP_RANDOM_MOVE) {
				EX_OVER* ex_over = new EX_OVER;
				ex_over->m_op = OP_RANDOM_MOVE;
				PostQueuedCompletionStatus(h_iocp, 1, ev.object, &ex_over->m_over);
			}
			else if (ev.e_type == OP_RUNAWAY) {
				EX_OVER* ex_over = new EX_OVER;
				ex_over->m_op = OP_RUNAWAY;
				*reinterpret_cast<int*>(ex_over->m_packetbuf) = ev.target_id;
				PostQueuedCompletionStatus(h_iocp, 1, ev.object, &ex_over->m_over);
			}
			
		}
		else {
			timer_l.unlock();
			this_thread::sleep_for(10ms);
		}
	}
}

int API_get_x(lua_State* L) {
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L,2);
	int x = objects[obj_id]->x;
	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State* L) {
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);
	int y = objects[obj_id]->y;
	lua_pushnumber(L, y);
	return 1;
}

int API_run_away(lua_State* L) {
	int obj_id = lua_tonumber(L, -2);
	int player_id = lua_tonumber(L, -1);
	lua_pop(L, 3);

	add_event(obj_id, -1, OP_RUNAWAY, 1000);
	add_event(obj_id, -1, OP_RUNAWAY, 2000);
	add_event(obj_id, player_id, OP_RUNAWAY, 3000);
	return 1;
}

int API_send_mess(lua_State* L) {
	int p_id = lua_tonumber(L, -3);
	int o_id = lua_tonumber(L, -2);
	const char* mess = lua_tostring(L, -1);

	lua_pop(L, 4);
	send_chat(o_id, p_id, mess);
	return 0;
}

int main()
{
	for (int i = 0; i < MAX_USER + 1; ++i) {
		if (is_npc(i) == true) {
			auto& pl = objects[i] = new NPC;

			pl->id = i;

			sprintf_s(pl->m_name, "NPC %d", i);
			pl->m_state = PLST_INGAME;
			pl->x = rand() % WORLD_WIDTH;
			pl->y = rand() % WORLD_HEIGHT;

			int sectorX = pl->x / SECTOR_RADIUS;
			int sectorY = pl->y / SECTOR_RADIUS;
			pl->sectorX = sectorX;
			pl->sectorY = sectorY;

			sector[sectorY][sectorX].m_playerLock.lock();
			sector[sectorY][sectorX].players.insert(pl->id);
			sector[sectorY][sectorX].m_playerLock.unlock();

			// ����ӽ� �ڷᱸ��
			lua_State* L = (*static_cast<NPC*>(pl)).L = luaL_newstate();
			// �⺻ ���̺귯�� �ε�
			luaL_openlibs(L);
			// ���� �ӽ��� stack�� �Է�
			int error = luaL_loadfile(L, "npc.lua") ||
				lua_pcall(L, 0, 0, 0);

			if (error != 0) {
				cout << "error" << endl;
			}
			lua_getglobal(L, "set_uid");
			lua_pushnumber(L, i);
			lua_pcall(L, 1, 1, 0);
			lua_pop(L, 1);// eliminate set_uid from stack after call

			lua_register(L, "API_get_x", API_get_x);
			lua_register(L, "API_get_y", API_get_y);
			lua_register(L, "API_SendMessage", API_send_mess);
			lua_register(L, "API_run_away", API_run_away);
		}
		else {
			auto& pl = objects[i] = new PLAYER;
			pl->id = i;
			pl->m_state = PLST_FREE;
		}
	}

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	wcout.imbue(locale("korean"));
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket), h_iocp, SERVER_ID, 0);
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	::bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
	listen(listenSocket, SOMAXCONN);

	EX_OVER accept_over;
	accept_over.m_op = OP_ACCEPT;
	memset(&accept_over.m_over, 0, sizeof(accept_over.m_over));
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	accept_over.m_csocket = c_socket;
	BOOL ret = AcceptEx(listenSocket, c_socket,
		accept_over.m_packetbuf, 0, 32, 32, NULL, &accept_over.m_over);
	if (FALSE == ret) {
		int err_num = WSAGetLastError();
		if (err_num != WSA_IO_PENDING)
			display_error("AcceptEx Error", err_num);
	}

	vector <thread> worker_threads;
	for (int i = 0; i < 4; ++i)
		worker_threads.emplace_back(worker, h_iocp, listenSocket);

	//thread ai_thread{ do_ai };
	//ai_thread.join();

	thread timer_thread{ do_timer };
	timer_thread.join();

	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}