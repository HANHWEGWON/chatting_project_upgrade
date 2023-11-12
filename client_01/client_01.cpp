#pragma comment(lib, "ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS //Ȥ�� localtime_s�� ���

#include <WinSock2.h> //Winsock ������� include. WSADATA �������
#include <WS2tcpip.h>
#include <string>
#include <sstream>
#include <iostream>
#include <thread>
#include <mysql/jdbc.h>
#include <ctime>
#include <windows.h>
#include <cstdio> 
#include <vector>

#define MAX_SIZE 1024

WSADATA wsa;
using std::cout;
using std::cin;
using std::endl;
using std::string;

sql::mysql::MySQL_Driver* driver;
sql::Connection* con;
sql::Statement* stmt;
sql::PreparedStatement* pstmt;
sql::ResultSet* result;

const string server = "tcp://127.0.0.1:3306"; //�����ͺ��̽� �ּ�
const string username = "root"; //�����ͺ��̽� �����
const string db_password = "1234"; //�����ͺ��̽� ���� ��й�ȣ

int all_color_num = 16;
int delete_return = 0;
SOCKET client_sock;
string id, password, real_nickname, user;
SOCKADDR_IN client_addr = {};
SOCKADDR_IN new_client = {};
std::vector <string> bad_word = { "�ٺ�", "��û��" };

struct membership {     // ȸ������ send ������ ���� ����ü
    char user_id[128];
    char user_pw[128];
    char user_nick[128];
    char user_email[128];
};


time_t timer;
struct tm* t;

int chat_recv();                              //�����κ��� �����͸� �޴� ���
void modify_user_info();                        //ȸ���������� ���
bool user_delete();                             //ȸ��Ż�� ���
void menu_list(SOCKADDR_IN&, string*, string*);//�޴�����Ʈ�� �����ִ� ���
void first_screen();                          //����ȭ��
void init_socket();                               //���� �ʱ�ȭ�� �ٽ� �����ϴ� ���
void delete_current_user();
void output_chat(char buf[MAX_SIZE]);         //���� ���� ��� ������ ê -> �ߺ� ��� ����
void go_chatting(SOCKADDR_IN&, int);          //���Ͽ����ؼ� ä���ϴ� ���
void my_info(string, string);                 //������Ȯ�� ���
void go_chat();                               //ä�ù� ������ ä���ϴ� ���
void run_game();                              //���ӹ� ������ �����ϴ� ���
bool password_check(string);


namespace my_to_str { //�����Ϸ��� ǥ�� ������ �� �¾Ƽ� to_string ��� �� �����߻�
    template < typename T > std::string to_string(const T& n)
    {
        std::ostringstream stm;
        stm << n;
        return stm.str();
    }
}
enum ConsolColor {
    CC_BLACK,
    CC_DARKBLUE,
    CC_DARKGREEN,
    CC_DARKCYAN,
    CC_DARKRED,
    CC_DARKMAGENTA,
    CC_DARKYELLOW,
    CC_LIGHTGRAY,
    CC_DARKGRAY,
    CC_BLUE,
    CC_GREEN,
    CC_CYAN,
    CC_RED,
    CC_MAGENTA,
    CC_YELLOW,
    CC_WHITE,
};
const char* enum_str[] = {
    "BLACK",
    "DARKBLUE",
    "DARKGREEN",
    "DARKCYAN",
    "DARKRED",
    "DARKMAGENTA",
    "DARKYELLOW",
    "LIGHTGRAY",
    "DARKGRAY",
    "BLUE",
    "GREEN",
    "CYAN",
    "RED",
    "MAGENTA",
    "YELLOW",
    "WHITE",
};
typedef struct _Color {
    int num = 1;
    int color[10] = { CC_BLACK };
    string user_nickname[10] = { "." };
}Color;

Color c;

enum Modify {
    MODIFY_ID,
    MODIFY_PASSWORD,
    MODIFY_NICKNAME,
    MODIFY_EMAIL,
    MODIFY_BACK,
    MEMBER_INFORMATION,
};
enum Delete {
    _DELETE,
    YES_DELETE,
    NO_DELETE,
};
enum Menu {
    _MENU,
    MENU_MODIFY,
    MENU_CHAT,
    MENU_DELETE,
    MENU_MAIN,
    MENU_GAME,
};
enum Main {
    _MAIN,
    MAIN_LOGIN,
    MAIN_MEMBERSHIP,
};

void setFontColor(int color) { //�ؽ�Ʈ ���� ����
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (info.wAttributes & 0xf0) | (color & 0xf));
}
void setColor(int color, int bgcolor) { //�ؽ�Ʈ & ��� ���� ����
    color &= 0xf;
    bgcolor &= 0xf;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        (bgcolor << 4) | color);
}

//���� �缳�� �Լ�
void init_socket() { //socket�� ���� ��, ���ο� socket ����
    closesocket(client_sock);
    WSACleanup();

    int code = WSAStartup(MAKEWORD(2, 2), &wsa);

    if (!code) {
        client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        client_addr.sin_family = AF_INET;
        //client_addr.sin_port = htons(7777); ��Ʈ��ȣ �������� �ʴ´�.
        InetPton(AF_INET, TEXT("127.0.0.1"), &client_addr.sin_addr);
    }
}

void run_game() {
    char buf[MAX_SIZE] = { };
    string msg = "", text = "";
    while (1) {
        ZeroMemory(&buf, MAX_SIZE);
        if (recv(client_sock, buf, MAX_SIZE, 0) > 0) {
            msg = buf;
            cout << msg << endl;
            if (msg.find("1~9 ������ ���� 3���� �Է� �ϼ���.") != string::npos) {
                std::getline(cin, text);
                send(client_sock, text.c_str(), text.size(), 0);
                continue;
            }
            else if (msg.find("******** �����մϴ�") != string::npos) {

                text = "end_game";
                send(client_sock, text.c_str(), text.size(), 0);
                Sleep(2000);
                system("cls");
                closesocket(client_sock);
                break;
            }
        }
    }
}

int chat_recv() {
    char buf[MAX_SIZE] = { };
    string msg;

    while (1) {
        ZeroMemory(&buf, MAX_SIZE);
        if (recv(client_sock, buf, MAX_SIZE, 0) > 0) {

            msg = buf;
            std::stringstream ss(msg); //���ڿ��� ��Ʈ��ȭ
            ss >> user;

            if (msg.substr(0, 6) == "/color") {
                std::stringstream s(msg);
                string nick, color;
                while (s >> nick >> color) {
                    if (nick != "/color") {
                        break;
                    }
                }
                c.color[c.num] = stoi(color); //���ϴ� ����� Ư�� ������ �г����� ����ü�� ����
                c.user_nickname[c.num] = nick;
                c.num++;
                continue;
            }

            std::thread th_color(output_chat, buf);

            th_color.join();
        }
        else {
            cout << "Server Off" << endl;
            return -1;
        }
    }
}

void go_chat() {
    std::thread th_recv(chat_recv);

    while (1) {
        string text;
        std::getline(cin, text);
        const char* buffer = text.c_str(); //string���� char* Ÿ������ ��ȯ

        if (text == "") { continue; } //enter -> db ���� x
        if (text == "/back") { //�ڷΰ��� ���
            send(client_sock, buffer, strlen(buffer), 0);
            break;
        }

        if (text == "/help") { //����� ���� ��ɾ�
            setFontColor(CC_YELLOW);
            cout << "/dm <�г���> <�޽���>  : Ư�� �������Ը� �޽����� �����ϰ�, �ٸ� �������Դ� ǥ�õ��� �ʽ��ϴ�.\n";
            cout << "/dm_msg                : ������ ���� DM���� ǥ���մϴ�.\n";
            cout << "/color <����> <�г���> : Ư�� ������ �޽��� ������ �����ŵ�ϴ�.(/color ��ɾ� : ���� ����)\n";
            cout << "/back                  : �޴�ȭ������ ���ư��� ��ɾ��Դϴ�.\n";
            setColor(CC_LIGHTGRAY, CC_BLACK);
            continue;
        }
        if (text == "/color") {
            for (int color = 0; color < all_color_num; color++) {
                setFontColor(CC_YELLOW);
                cout << color << " : " << enum_str[color] << endl;
            }setColor(CC_LIGHTGRAY, CC_BLACK);
            continue;
        }

        for (int i = 0; i < 2; i++) {
            if (text.find(bad_word[i]) != string::npos) { //�弳 �� ����. socket�� �ݰ�, �ٽ� socket ����
                send(client_sock, buffer, strlen(buffer), 0);
                init_socket();
                menu_list(client_addr, &id, &password);
                break;
            }
        }
        send(client_sock, buffer, strlen(buffer), 0);
    }
    th_recv.join();
    closesocket(client_sock);
}
void go_chatting(SOCKADDR_IN& client_addr, int port_num) { //ä�ù� ���� �Լ�
    client_addr.sin_port = htons(port_num);

    while (1) {
        if (!connect(client_sock, (SOCKADDR*)&client_addr, sizeof(client_addr))) {
            if (port_num == 7777) cout << "Server Connect" << endl; //sin_port = 7777 -> ä�� ���� ����
            if (port_num == 6666) cout << "gameServer Connect" << endl; //sin_port = 6666 -> ���� ���� ����
            send(client_sock, real_nickname.c_str(), real_nickname.length(), 0);
            break;
        }
        cout << "Connecting..." << endl;
    }
    if (port_num == 6666) {     //���ӹ� ����
        run_game();
    }
    else {
        go_chat();
    }

    init_socket();
    client_addr.sin_port = htons(5555);

    while (1) {
        if (!connect(client_sock, (SOCKADDR*)&client_addr, sizeof(client_addr))) {
            cout << "login_server_on\n";
            break;
        }
        cout << "Connecting..." << endl;
    }

    string msg = "0 " + id + " " + password;
    send(client_sock, msg.c_str(), msg.size(), 0);

    menu_list(client_addr, &id, &password);

}

void output_chat(char buf[MAX_SIZE]) { //���� ���� ��� ������ ê -> �ߺ� ��� ����

    for (int i = 0; i < c.num; i++) {
        if (user == c.user_nickname[i]) {
            setFontColor(c.color[i]);

            cout << buf << endl;

            setColor(CC_LIGHTGRAY, CC_BLACK); //���� ����.
            return;
        }
    }
    if (user != real_nickname) {
        setColor(CC_LIGHTGRAY, CC_BLACK);
        cout << buf << endl;
        return;
    }//���� ���� �� �ƴ� ��쿡�� ����ϵ���
}
void menu_list(SOCKADDR_IN& client_addr, string* menu_id, string* menu_password) {      //���� �۾� ��!
    int num = 0;

    system("cls");
    cout << "�١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ�\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                  1. ȸ������                     ��\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                  2. ä�ù� ����                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                  3. ȸ��Ż��                     ��\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                  4. �α׾ƿ�                     ��\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                  5. ���ӹ� ����                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "��                                                  ��\n";
    cout << "�١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ�\n";

    cin >> num;

    string temp;
    getline(cin, temp);
    system("cls");
    switch (num) {
    case MENU_MODIFY:
        modify_user_info();
        menu_list(client_addr, menu_id, menu_password); //ȸ������ ������ �Ϸ�� �� �ٽ� �޴��� �̵�
        break;
    case MENU_CHAT:
        setFontColor(CC_YELLOW);
        cout << "\n\n\n\n\n\n\n\t\t/help ��ɾ�� ����ڿ��� ��ɾ� ����� �����մϴ�. ��";
        setColor(CC_LIGHTGRAY, CC_BLACK); //���� ����
        Sleep(3000);
        system("cls");
        init_socket();
        go_chatting(client_addr, 7777); //ä�� ���� ����
        break;
    case MENU_DELETE:
        if (user_delete() == false) {
            Sleep(2000);
            menu_list(client_addr, menu_id, menu_password);
        }else {
            first_screen();
            break;
        }
    case MENU_MAIN:
        delete_current_user(); //�α׾ƿ� �� ���� �������� ���� ���̺��� ����
        first_screen();
        break;
    case MENU_GAME:
        system("cls");
        init_socket();
        go_chatting(client_addr, 6666); //���� ���� ����
        break;

    }
}

void delete_current_user() { //�α׾ƿ� �� �ش� ������ ���̺��� ����
    string msg = "4 ";
    char buf[MAX_SIZE] = {};
    send(client_sock, msg.c_str(), 1, 0);
    return;
}

void modify_user_info() {
    string new_info;
    int what_modify = 0;

    while (true) {

        while (true) {
            cout << "1.��й�ȣ  2.�г���  3.�̸���  4.�ڷΰ���  5.ȸ������Ȯ��" << endl;
            cout << "���Ͻô� �׸��� �������ּ���. >> ";
            cin >> what_modify;
            if (what_modify < 1 || what_modify > 5) {
                cout << "�ǹٸ� ��ɾ �ƴմϴ�. �ٽ��Է����ּ���.\n";
                continue;
            }
            break;

        }

        if (what_modify == MODIFY_BACK) return;
        char buf[MAX_SIZE] = {};

        ZeroMemory(&buf, MAX_SIZE);
        switch (what_modify) {
        case MODIFY_PASSWORD:
            cout << "���ο� ��й�ȣ�� �Է����ּ���. (!, ?, # �� 1�� �̻� ����) >> ";
            cin >> new_info;
            break;
        case MODIFY_NICKNAME:
            cout << "���ο� �г����� �Է����ּ���. >> ";
            cin >> new_info;
            break;
        case MODIFY_EMAIL:
            cout << "���ο� �̸����� �Է����ּ���. >> ";
            cin >> new_info;
            break;
        }
        // Ŭ���̾�Ʈ���� �ؾ����� send ���� "2 <1, 2, 3, 4, 5> <���� ������> 
        string msg = "2 " + std::to_string(what_modify) + " " + new_info;
        send(client_sock, msg.c_str(), msg.size(), 0);

        recv(client_sock, buf, MAX_SIZE, 0);

        system("cls");

        if (what_modify == MEMBER_INFORMATION) {
            cout << buf << '\n';
            continue;
        }
        else if (what_modify == MODIFY_PASSWORD) {
            if (strcmp(buf, "false") == 0) {
                cout << "�н����� ���Ŀ� �°� �Է����ּ���.\n";
                continue;
            }
            cout << buf << '\n';
            Sleep(3000);
            system("cls");
            continue;
        }
        else if (what_modify == MODIFY_EMAIL) {
            if (strcmp(buf, "false") == 0) {
                cout << "�̸��� ���Ŀ� �°� �Է����ּ���.\n";
                continue;
            }
            cout << buf << '\n';
            Sleep(3000);
            system("cls");
            continue;
        }
        else if (what_modify == MODIFY_NICKNAME) {
            cout << "���������� �г����� �����Ͽ����ϴ�.";
            Sleep(3000);
            system("cls");
            continue;
        }
    }



}
bool user_delete() {

    int order;
    string password="", msg = "";

    cout << "ȸ��Ż�� �� ��� ��� ��ȭ ������ ������ϴ�." << endl
        << "��� �����Ͻðڽ��ϱ�? (1. ��  2. �ƴϿ�) >> ";
    cin >> order;

    char buf[MAX_SIZE] = {};
    
    ZeroMemory(&buf, MAX_SIZE);
        switch (order) {
        case YES_DELETE:
            cout << "������ ȸ��Ż�� ����, ��й�ȣ�� �Է����ּ���. >> ";
            cin >> password;
        case NO_DELETE:
            break;
        }
        msg = "3 " + password;

        send(client_sock, msg.c_str(), msg.size(), 0);
        recv(client_sock, buf, MAX_SIZE, 0);
        cout << buf << '\n';
        if (strcmp(buf, "�ùٸ��� ���� ��й�ȣ�� �Է��ϼ̽��ϴ�.\n") == 0) {
            return false;
        }
        else return true;
    

}
void join_membership() {
    struct membership member;
    string membership_id, membership_password, nickname, email;
    string correct_id, correct_password;
    system("cls");
    char buf[MAX_SIZE] = {};

    while (true) {
        ZeroMemory(&buf, MAX_SIZE);
        cout << "����� ���̵� �Է����ּ���. >> ";
        cin >> membership_id;
        cout << "����� ��й�ȣ�� �Է����ּ���. (!, ?, # �� 1�� �̻� ����) >> ";
        cin >> membership_password;
        cout << "�г����� �Է����ּ��� >> ";
        cin >> nickname;
        cout << "�̸����� �Է����ּ���. >> ";
        cin >> email;

        membership_id = "1 " + membership_id;

        strcpy(member.user_id, membership_id.c_str());
        strcpy(member.user_pw, membership_password.c_str());
        strcpy(member.user_nick, nickname.c_str());
        strcpy(member.user_email, email.c_str());
        //����ü ������ �Է�

        string serializedData(reinterpret_cast<const char*>(&member), sizeof(member));  //������ ����ȭ


        send(client_sock, serializedData.c_str(), serializedData.size(), 0);
        recv(client_sock, buf, MAX_SIZE, 0);
        if (strcmp(buf, "true") == 0) {
            id = membership_id;
            password = membership_password;
            real_nickname = nickname;

            break;
        }
        cout << buf;
    }


}
void first_screen() {

    int client_choose = 0;
    while (1) {
        cout << "�١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ�\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��     �ڡڡڡڡ�      ��      ��       ��  ��      ��\n";
        cout << "��         ��        �� ��     ��       �ڡ�        ��\n";
        cout << "��         ��       �ڡڡڡ�   ��       �� ��       ��\n";
        cout << "��         ��     ��       ��  �ڡڡڡ� ��   ��     ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��              1. �α���   2. ȸ������             ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "��                                                  ��\n";
        cout << "�١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ١ڡ�\n";

        cin >> client_choose;
        string msg = "";
        char buf[MAX_SIZE] = {};
        ZeroMemory(&buf, MAX_SIZE);

        if (client_choose == MAIN_LOGIN) { //�α���
            cout << "ID :";
            cin >> id;
            cout << "PW :";
            cin >> password;
            msg = "0 " + id + " " + password;

            send(client_sock, msg.c_str(), msg.size(), 0);
            recv(client_sock, buf, MAX_SIZE, 0);
            cout << buf << "\n";
            int index = string(buf).find("�α��� �Ǿ����ϴ�.");
            if (index == string::npos) continue;
            real_nickname = string(buf).substr(0, index - 1);

            
            menu_list(client_addr, &id, &password); //�α��� ���� �� �޴���
            continue; //ȸ��Ż�� �� main���� ������
        }
        if (client_choose == MAIN_MEMBERSHIP) { //ȸ������
            join_membership();
            continue;
        }
    }
}

int main() {
    bool login = true;

    int code = WSAStartup(MAKEWORD(2, 2), &wsa);
    //Winsock�� �ʱ�ȭ�ϴ� �Լ�. MAKEWORD(2, 2)�� Winsock�� 2.2 ������ ����ϰڴٴ� �ǹ�
    //���࿡ �����ϸ� 0��, �����ϸ� �� �̿��� ���� ��ȯ
    //0�� ��ȯ�ߴٴ� ���� Winsock�� ����� �غ� �Ǿ��ٴ� �ǹ�

    try {
        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect(server, username, db_password);
    }
    catch (sql::SQLException& e) {
        cout << "Could not connect to server. Error message: " << e.what() << endl;
        exit(1);
    }

    con->setSchema("chat");
    stmt = con->createStatement();
    stmt->execute("set names euckr");//�ѱ�� �ޱ����� ������

    if (stmt) { delete stmt; stmt = nullptr; }
    stmt = con->createStatement();
    delete stmt;

    if (!code) {
        client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  //������ ���� ���� ���� �κ�

        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(5555);
        InetPton(AF_INET, TEXT("127.0.0.1"), &client_addr.sin_addr);

        while (1) {
            if (!connect(client_sock, (SOCKADDR*)&client_addr, sizeof(client_addr))) {
                cout << "login_server_on\n";
                break;
            }
            cout << "Connecting..." << endl;
        }

        first_screen();
    }
    WSACleanup();
    return 0;
}