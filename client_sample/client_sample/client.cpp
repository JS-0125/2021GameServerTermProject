#define SFML_STATIC 1
#include"default.h"
#include"TextBox.h"

using namespace std;

#ifdef _DEBUG
#pragma comment (lib, "lib/sfml-graphics-s-d.lib")
#pragma comment (lib, "lib/sfml-window-s-d.lib")
#pragma comment (lib, "lib/sfml-system-s-d.lib")
#pragma comment (lib, "lib/sfml-network-s-d.lib")
#else
#pragma comment (lib, "lib/sfml-graphics-s.lib")
#pragma comment (lib, "lib/sfml-window-s.lib")
#pragma comment (lib, "lib/sfml-system-s.lib")
#pragma comment (lib, "lib/sfml-network-s.lib")
#endif
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "ws2_32.lib")

#include "..\..\st_iocp_server\st_iocp_server\2021_????_protocol.h"

sf::TcpSocket socket;

constexpr auto SCREEN_WIDTH = 16;
constexpr auto SCREEN_HEIGHT = 16;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = TILE_WIDTH * SCREEN_WIDTH + 10;   // size of window
constexpr auto WINDOW_HEIGHT = TILE_WIDTH * SCREEN_WIDTH + 10;

constexpr int MAX_BUFFER = 1024;
constexpr auto BUF_SIZE = MAX_BUFFER;

int g_left_x;
int g_top_y;
int g_myid;

vector<vector<bool>> can_move;

sf::RenderWindow* g_window;
sf::Font g_font;
enum OBJ_CLASS { PLAYER_CLASS, ARGO_CLASS, PEACE_CLASS };


class OBJECT {
private:
	bool m_showing;
	sf::Text m_name;
	sf::Text m_chat;
	sf::Text m_hp;
	sf::Text m_level;
	chrono::system_clock::time_point m_mess_end_time;
public:
	sf::Sprite m_sprite;
	int m_x, m_y;
	int hp, exp, level;

	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		set_name("NONAME");
		m_mess_end_time = chrono::system_clock::now();
	}
	OBJECT() {
		m_showing = false;
	}
	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int x, int y) {
		m_x = x;
		m_y = y;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * 65.0f + 8;
		float ry = (m_y - g_top_y) * 65.0f + 8;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		m_hp.setPosition(rx, ry - 30);
		g_window->draw(m_hp);
		m_level.setPosition(rx - 20, ry - 30);
		g_window->draw(m_level);

		if (m_mess_end_time < chrono::system_clock::now()) {
			m_name.setPosition(rx, ry - 20);
			g_window->draw(m_name);
		}
		else {
			m_chat.setPosition(rx, ry - 20);
			g_window->draw(m_chat);
		}
	}
	void set_name(const char str[]) {
		m_name.setFont(g_font);
		m_name.setString(str);
		m_name.setFillColor(sf::Color(255, 255, 0));
		m_name.setStyle(sf::Text::Bold);
	}
	void set_chat(const char str[]) {
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 255, 255));
		m_chat.setStyle(sf::Text::Bold);
		m_mess_end_time = chrono::system_clock::now() + chrono::seconds(3);
	}
	void set_hp() {
		m_hp.setFont(g_font);
		m_hp.setString((char*)(&to_string(hp)));
		m_hp.setFillColor(sf::Color(255, 0, 0));
		m_hp.setStyle(sf::Text::Bold);
	}
	void set_level() {
		m_level.setFont(g_font);
		m_level.setString((char*)(&to_string(level)));
		m_level.setFillColor(sf::Color(255, 255, 255));
		m_level.setStyle(sf::Text::Bold);
	}
};

OBJECT avatar;
OBJECT players[MAX_USER];

OBJECT white_tile;
OBJECT black_tile;

sf::Texture* board;
sf::Texture* pieces;
Textbox *chatText;
sf::Text my_hp;
sf::Text my_exp;
sf::Text my_level;

vector<sf::Text> chatContainer;

void set_str();

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		while (true);
	}
	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_WIDTH };
	avatar = OBJECT{ *pieces, 128, 0, 64, 64 };
	set_str();
	//avatar.move(4, 4);
	for (auto& pl : players) {
		pl = OBJECT{ *pieces, 64, 0, 64, 64 };
	}

	// chat text
	chatText = new Textbox(30, sf::Color::Blue, false);
	chatText->setPosition( g_left_x + 300, 0 );
	chatText->setLimit(true, MAX_STR_LEN-1);
	chatText->setFont(g_font);
}

void set_str() {
	my_hp.setFont(g_font);
	//my_hp.setString((char*)(&to_string(avatar.hp)));
	my_hp.setFillColor(sf::Color(255, 0, 0));
	my_hp.setStyle(sf::Text::Bold);
	my_hp.setPosition(g_left_x+20, 60);

	my_exp.setFont(g_font);
	//my_exp.setString((char*)(&to_string(avatar.exp)));
	my_exp.setFillColor(sf::Color(255, 0, 0));
	my_exp.setStyle(sf::Text::Bold);
	my_exp.setPosition(g_left_x+20, 80);

	my_level.setFont(g_font);
	//my_level.setString((char*)(&to_string(avatar.level)));
	my_level.setFillColor(sf::Color(255, 0, 0));
	my_level.setStyle(sf::Text::Bold);
	my_level.setPosition(g_left_x + 20, 100);
}

void client_finish()
{
	delete board;
	delete pieces;
	delete chatText;
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case SC_LOGIN_OK:
	{
		sc_packet_login_ok* packet = reinterpret_cast<sc_packet_login_ok*>(ptr);
		g_myid = packet->id;
		avatar.m_x = packet->x;
		avatar.m_y = packet->y;
		avatar.exp = packet->EXP;
		avatar.hp = packet->HP;
		avatar.level = packet->LEVEL;

		avatar.set_name((char*)"ME!");

		g_left_x = packet->x - SCREEN_WIDTH / 2;
		g_top_y = packet->y - SCREEN_HEIGHT / 2;
		avatar.move(packet->x, packet->y);
		avatar.show();
	}
	break;
	case SC_ADD_OBJECT:
	{
		sc_packet_add_object* my_packet = reinterpret_cast<sc_packet_add_object*>(ptr);
		int id = my_packet->id;

		//players[id].set_name(my_packet->name);

		if (id < MAX_USER) {
			switch (my_packet->obj_class)
			{
			case PLAYER_CLASS:
				players[id].m_sprite.setTextureRect(sf::IntRect(198, 0, 64, 64));
				break;
			case ARGO_CLASS:
				players[id].m_sprite.setTextureRect(sf::IntRect(64, 0, 64, 64));
				break;
			case PEACE_CLASS:
				players[id].m_sprite.setTextureRect(sf::IntRect(0, 0, 64, 64));
				break;
			}
			players[id].set_name(my_packet->name);
			players[id].hp = my_packet->HP;
			players[id].level = my_packet->LEVEL;
			players[id].set_hp();
			players[id].set_level();
			players[id].move(my_packet->x, my_packet->y);
			players[id].show();
		}
		else {
			//npc[id - NPC_START].x = my_packet->x;
			//npc[id - NPC_START].y = my_packet->y;
			//npc[id - NPC_START].attr |= BOB_ATTR_VISIBLE;
		}
		break;
	}
	case SC_POSITION:
	{
		sc_packet_position* my_packet = reinterpret_cast<sc_packet_position*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
		}
		else if (other_id < MAX_USER) {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		else {
			//npc[other_id - NPC_START].x = my_packet->x;
			//npc[other_id - NPC_START].y = my_packet->y;
		}
		break;
	}

	case SC_REMOVE_OBJECT:
	{
		sc_packet_remove_object* my_packet = reinterpret_cast<sc_packet_remove_object*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else if (other_id < MAX_USER) {
			players[other_id].hide();
		}
		else {
			//		npc[other_id - NPC_START].attr &= ~BOB_ATTR_VISIBLE;
		}
		break;
	}
	case SC_CHAT:
	{
		sc_packet_chat* my_packet = reinterpret_cast<sc_packet_chat*>(ptr);
		int other_id = my_packet->id;

		sf::Text tmpTex;
		if (other_id < 0) 
			tmpTex.setFillColor(sf::Color::Blue);
		else if (other_id < MAX_USER) 
			tmpTex.setFillColor(sf::Color::Red);
			
		tmpTex.setFont(g_font);
		tmpTex.setString(my_packet->message);
		tmpTex.setStyle(sf::Text::Bold);
		if (chatContainer.size() > 10)
			chatContainer.erase(chatContainer.begin());
		chatContainer.emplace_back(tmpTex);
		break;
	}
	case SC_STAT_CHANGE:
	{
		sc_packet_stat_change* my_packet = reinterpret_cast<sc_packet_stat_change*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hp = my_packet->HP;
			avatar.exp = my_packet->EXP;
			avatar.level = my_packet->LEVEL;
			avatar.set_hp();
			avatar.set_level();
		}
		else {
			players[other_id].hp = my_packet->HP;
			players[other_id].exp = my_packet->EXP;
			players[other_id].level = my_packet->LEVEL;
			players[other_id].set_hp();
			players[other_id].set_level();
		}
		/*else {
			npc[other_id - NPC_START].attr &= ~BOB_ATTR_VISIBLE;
		}*/
		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = ptr[0];
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv ????!";
		while (true);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j)
		{
			int tile_x = i +  g_left_x;
			int tile_y = j +  g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if (can_move[tile_y][tile_x]) {
				white_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				white_tile.a_draw();
			}
			else
			{
				black_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				black_tile.a_draw();
			}
		}
	avatar.draw();
	for (auto& pl : players) pl.draw();
	sf::Text text;
	text.setFont(g_font);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.m_x, avatar.m_y);
	text.setString(buf);
	g_window->draw(text);

	my_hp.setString((char*)(&to_string(avatar.hp)));
	my_exp.setString((char*)(&to_string(avatar.exp)));
	my_level.setString((char*)(&to_string(avatar.level)));

	g_window->draw(my_hp);
	g_window->draw(my_exp);
	g_window->draw(my_level);
	chatText->drawTo(*g_window);

	for (int i = 0; i < chatContainer.size(); ++i) {
		chatContainer[i].setPosition(0, g_top_y + i*50);
		g_window->draw(chatContainer[i]);
	}
}

void send_move_packet(char dr)
{
	cs_packet_move packet;
	packet.size = sizeof(packet);
	packet.type = CS_MOVE;
	packet.direction = dr;
	size_t sent = 0;
	socket.send(&packet, sizeof(packet), sent);
}


void send_attack_packet(char dr)
{
	cs_packet_attack packet;
	packet.size = sizeof(packet);
	packet.type = CS_ATTACK;
	size_t sent = 0;
	socket.send(&packet, sizeof(packet), sent);
}


void send_login_packet(string &name)
{
	cs_packet_login packet;
	packet.size = sizeof(packet);
	packet.type = CS_LOGIN;
	strcpy_s(packet.player_id, name.c_str());
	size_t sent = 0;
	socket.send(&packet, sizeof(packet), sent);
}

void send_chat_packet(string mess)
{
	cs_packet_chat packet;
	packet.size = sizeof(packet);
	packet.type = CS_CHAT;
	strcpy_s(packet.message, mess.c_str());
	size_t sent = 0;
	socket.send(&packet, sizeof(packet), sent);
}

int main()
{
	setlocale(LC_ALL, "korean");
	wcout.imbue(locale("korean"));
	sf::Socket::Status status = socket.connect("127.0.0.1", SERVER_PORT);

	socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"?????? ?????? ?? ?????ϴ?.\n";
		while (true);
	}

	cout << "id?? ?Է??ϼ???: ";
	string id;
	cin >> id;
	send_login_packet(id);
	avatar.set_name(id.c_str());

	client_initialize();

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	ifstream is("terrainData.txt");
	can_move.resize(WORLD_WIDTH);

	for (int i = 0; i < WORLD_WIDTH; ++i) {
		can_move[i].reserve(WORLD_WIDTH);
		for (int n = 0; n < WORLD_WIDTH; ++n) {
			can_move[i].push_back(is.get());
		}
	}
	cout << "teraain data setting ok" << endl;

	while (window.isOpen())
	{
		sf::Event event;

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift)) {
				chatText->setSelected(true);
		}
		else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Enter)) {
			if (chatText->isSelctedTexBox()) {
				if (0 != chatText->isInText()) {
					char tmpMess[MAX_STR_LEN];
					strcpy_s(tmpMess, (chatText->getText()).c_str());

					send_chat_packet(tmpMess);

					sf::Text tmpTex;
					tmpTex.setFont(g_font);
					tmpTex.setString(tmpMess);
					tmpTex.setFillColor(sf::Color(255, 0, 0));
					tmpTex.setStyle(sf::Text::Bold);
					if (chatContainer.size() > 10)
						chatContainer.erase(chatContainer.begin());
					chatContainer.emplace_back(tmpTex);

					chatText->textReset();
				}
			}
			chatText->setSelected(false);
		}

		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();

			if (event.type == sf::Event::TextEntered && chatText->isSelctedTexBox()) {
				chatText->typedOn(event);
				break;
			}

			if (event.type == sf::Event::KeyPressed) {
				char p_type = -1;
				switch (event.key.code) {
				case sf::Keyboard::Left:
					p_type = 2;
					break;
				case sf::Keyboard::Right:
					p_type = 3;
					break;
				case sf::Keyboard::Up:
					p_type = 0;
					break;
				case sf::Keyboard::Down:
					p_type = 1;
					break;
				case sf::Keyboard::A: {
					p_type = 4;
					send_attack_packet(p_type);
					break;
				}
				case sf::Keyboard::Escape:
					window.close();
					break;
				}
				if (-1 != p_type && p_type<4) send_move_packet(p_type);
			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}