#pragma comment(lib, "ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS //혹은 localtime_s를 사용

#include <WinSock2.h> //Winsock 헤더파일 include. WSADATA 들어있음
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

const string server = "tcp://127.0.0.1:3306"; //데이터베이스 주소
const string username = "root"; //데이터베이스 사용자
const string db_password = "1234"; //데이터베이스 접속 비밀번호

int all_color_num = 16;
int delete_return = 0;
SOCKET client_sock;
string id, password, real_nickname, user;
SOCKADDR_IN client_addr = {};
SOCKADDR_IN new_client = {};
std::vector <string> bad_word = { "바보", "멍청이" };

struct membership {     // 회원가입 send 보내기 위한 구조체
    char user_id[128];
    char user_pw[128];
    char user_nick[128];
    char user_email[128];
};


time_t timer;
struct tm* t;

int chat_recv();                              //서버로부터 데이터를 받는 기능
void modify_user_info();                        //회원정보수정 기능
bool user_delete();                             //회원탈퇴 기능
void menu_list(SOCKADDR_IN&, string*, string*);//메뉴리스트를 보여주는 기능
void first_screen();                          //시작화면
void init_socket();                               //소켓 초기화후 다시 생성하는 기능
void delete_current_user();
void output_chat(char buf[MAX_SIZE]);         //색상 변경 대상 유저의 챗 -> 중복 출력 방지
void go_chatting(SOCKADDR_IN&, int);          //소켓연결해서 채팅하는 기능
void my_info(string, string);                 //내정보확인 기능
void go_chat();                               //채팅방 입장후 채팅하는 기능
void run_game();                              //게임방 입장후 게임하는 기능
bool password_check(string);


namespace my_to_str { //컴파일러의 표준 버전이 안 맞아서 to_string 사용 시 문제발생
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

void setFontColor(int color) { //텍스트 색상 변경
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (info.wAttributes & 0xf0) | (color & 0xf));
}
void setColor(int color, int bgcolor) { //텍스트 & 배경 색상 변경
    color &= 0xf;
    bgcolor &= 0xf;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        (bgcolor << 4) | color);
}

//소켓 재설정 함수
void init_socket() { //socket을 닫은 후, 새로운 socket 생성
    closesocket(client_sock);
    WSACleanup();

    int code = WSAStartup(MAKEWORD(2, 2), &wsa);

    if (!code) {
        client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        client_addr.sin_family = AF_INET;
        //client_addr.sin_port = htons(7777); 포트번호 지정하지 않는다.
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
            if (msg.find("1~9 사이의 숫자 3개를 입력 하세요.") != string::npos) {
                std::getline(cin, text);
                send(client_sock, text.c_str(), text.size(), 0);
                continue;
            }
            else if (msg.find("******** 축하합니다") != string::npos) {

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
            std::stringstream ss(msg); //문자열을 스트림화
            ss >> user;

            if (msg.substr(0, 6) == "/color") {
                std::stringstream s(msg);
                string nick, color;
                while (s >> nick >> color) {
                    if (nick != "/color") {
                        break;
                    }
                }
                c.color[c.num] = stoi(color); //원하는 색상과 특정 유저의 닉네임을 구조체에 저장
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
        const char* buffer = text.c_str(); //string형을 char* 타입으로 변환

        if (text == "") { continue; } //enter -> db 저장 x
        if (text == "/back") { //뒤로가기 기능
            send(client_sock, buffer, strlen(buffer), 0);
            break;
        }

        if (text == "/help") { //사용자 도움 명령어
            setFontColor(CC_YELLOW);
            cout << "/dm <닉네임> <메시지>  : 특정 유저에게만 메시지를 전송하고, 다른 유저에게는 표시되지 않습니다.\n";
            cout << "/dm_msg                : 유저가 받은 DM만을 표시합니다.\n";
            cout << "/color <색상> <닉네임> : 특정 유저의 메시지 색상을 변경시킵니다.(/color 명령어 : 색상 정보)\n";
            cout << "/back                  : 메뉴화면으로 돌아가는 명령어입니다.\n";
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
            if (text.find(bad_word[i]) != string::npos) { //욕설 시 퇴장. socket을 닫고, 다시 socket 생성
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
void go_chatting(SOCKADDR_IN& client_addr, int port_num) { //채팅방 입장 함수
    client_addr.sin_port = htons(port_num);

    while (1) {
        if (!connect(client_sock, (SOCKADDR*)&client_addr, sizeof(client_addr))) {
            if (port_num == 7777) cout << "Server Connect" << endl; //sin_port = 7777 -> 채팅 서버 입장
            if (port_num == 6666) cout << "gameServer Connect" << endl; //sin_port = 6666 -> 게임 서버 입장
            send(client_sock, real_nickname.c_str(), real_nickname.length(), 0);
            break;
        }
        cout << "Connecting..." << endl;
    }
    if (port_num == 6666) {     //게임방 입장
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

void output_chat(char buf[MAX_SIZE]) { //색상 변경 대상 유저의 챗 -> 중복 출력 방지

    for (int i = 0; i < c.num; i++) {
        if (user == c.user_nickname[i]) {
            setFontColor(c.color[i]);

            cout << buf << endl;

            setColor(CC_LIGHTGRAY, CC_BLACK); //기존 색상.
            return;
        }
    }
    if (user != real_nickname) {
        setColor(CC_LIGHTGRAY, CC_BLACK);
        cout << buf << endl;
        return;
    }//내가 보낸 게 아닐 경우에만 출력하도록
}
void menu_list(SOCKADDR_IN& client_addr, string* menu_id, string* menu_password) {      //현재 작업 중!
    int num = 0;

    system("cls");
    cout << "☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆\n";
    cout << "★                                                  ★\n";
    cout << "☆                                                  ☆\n";
    cout << "★                                                  ★\n";
    cout << "☆                  1. 회원정보                     ☆\n";
    cout << "★                                                  ★\n";
    cout << "☆                                                  ☆\n";
    cout << "☆                  2. 채팅방 입장                  ☆\n";
    cout << "★                                                  ★\n";
    cout << "☆                                                  ☆\n";
    cout << "★                  3. 회원탈퇴                     ★\n";
    cout << "☆                                                  ☆\n";
    cout << "★                                                  ★\n";
    cout << "☆                  4. 로그아웃                     ☆\n";
    cout << "★                                                  ★\n";
    cout << "☆                                                  ☆\n";
    cout << "★                  5. 게임방 입장                  ★\n";
    cout << "☆                                                  ☆\n";
    cout << "★                                                  ★\n";
    cout << "☆                                                  ☆\n";
    cout << "★                                                  ★\n";
    cout << "☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆\n";

    cin >> num;

    string temp;
    getline(cin, temp);
    system("cls");
    switch (num) {
    case MENU_MODIFY:
        modify_user_info();
        menu_list(client_addr, menu_id, menu_password); //회원정보 수정이 완료된 후 다시 메뉴로 이동
        break;
    case MENU_CHAT:
        setFontColor(CC_YELLOW);
        cout << "\n\n\n\n\n\n\n\t\t/help 명령어는 사용자에게 명령어 목록을 제공합니다. ♥";
        setColor(CC_LIGHTGRAY, CC_BLACK); //기존 색상
        Sleep(3000);
        system("cls");
        init_socket();
        go_chatting(client_addr, 7777); //채팅 서버 입장
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
        delete_current_user(); //로그아웃 시 현재 접속중인 유저 테이블에서 삭제
        first_screen();
        break;
    case MENU_GAME:
        system("cls");
        init_socket();
        go_chatting(client_addr, 6666); //게임 서버 입장
        break;

    }
}

void delete_current_user() { //로그아웃 시 해당 유저를 테이블에서 삭제
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
            cout << "1.비밀번호  2.닉네임  3.이메일  4.뒤로가기  5.회원정보확인" << endl;
            cout << "원하시는 항목을 선택해주세요. >> ";
            cin >> what_modify;
            if (what_modify < 1 || what_modify > 5) {
                cout << "옳바른 명령어가 아닙니다. 다시입력해주세요.\n";
                continue;
            }
            break;

        }

        if (what_modify == MODIFY_BACK) return;
        char buf[MAX_SIZE] = {};

        ZeroMemory(&buf, MAX_SIZE);
        switch (what_modify) {
        case MODIFY_PASSWORD:
            cout << "새로운 비밀번호를 입력해주세요. (!, ?, # 중 1개 이상 포함) >> ";
            cin >> new_info;
            break;
        case MODIFY_NICKNAME:
            cout << "새로운 닉네임을 입력해주세요. >> ";
            cin >> new_info;
            break;
        case MODIFY_EMAIL:
            cout << "새로운 이메일을 입력해주세요. >> ";
            cin >> new_info;
            break;
        }
        // 클라이언트에서 해야할일 send 형식 "2 <1, 2, 3, 4, 5> <보낼 데이터> 
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
                cout << "패스워드 형식에 맞게 입력해주세요.\n";
                continue;
            }
            cout << buf << '\n';
            Sleep(3000);
            system("cls");
            continue;
        }
        else if (what_modify == MODIFY_EMAIL) {
            if (strcmp(buf, "false") == 0) {
                cout << "이메일 형식에 맞게 입력해주세요.\n";
                continue;
            }
            cout << buf << '\n';
            Sleep(3000);
            system("cls");
            continue;
        }
        else if (what_modify == MODIFY_NICKNAME) {
            cout << "성공적으로 닉네임을 변경하였습니다.";
            Sleep(3000);
            system("cls");
            continue;
        }
    }



}
bool user_delete() {

    int order;
    string password="", msg = "";

    cout << "회원탈퇴를 할 경우 모든 대화 내용이 사라집니다." << endl
        << "계속 진행하시겠습니까? (1. 예  2. 아니오) >> ";
    cin >> order;

    char buf[MAX_SIZE] = {};
    
    ZeroMemory(&buf, MAX_SIZE);
        switch (order) {
        case YES_DELETE:
            cout << "안전한 회원탈퇴를 위해, 비밀번호를 입력해주세요. >> ";
            cin >> password;
        case NO_DELETE:
            break;
        }
        msg = "3 " + password;

        send(client_sock, msg.c_str(), msg.size(), 0);
        recv(client_sock, buf, MAX_SIZE, 0);
        cout << buf << '\n';
        if (strcmp(buf, "올바르지 않은 비밀번호를 입력하셨습니다.\n") == 0) {
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
        cout << "사용할 아이디를 입력해주세요. >> ";
        cin >> membership_id;
        cout << "사용할 비밀번호를 입력해주세요. (!, ?, # 중 1개 이상 포함) >> ";
        cin >> membership_password;
        cout << "닉네임을 입력해주세요 >> ";
        cin >> nickname;
        cout << "이메일을 입력해주세요. >> ";
        cin >> email;

        membership_id = "1 " + membership_id;

        strcpy(member.user_id, membership_id.c_str());
        strcpy(member.user_pw, membership_password.c_str());
        strcpy(member.user_nick, nickname.c_str());
        strcpy(member.user_email, email.c_str());
        //구조체 데이터 입력

        string serializedData(reinterpret_cast<const char*>(&member), sizeof(member));  //데이터 직렬화


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
        cout << "☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆\n";
        cout << "★                                                  ★\n";
        cout << "☆                                                  ☆\n";
        cout << "★                                                  ★\n";
        cout << "☆                                                  ☆\n";
        cout << "★                                                  ★\n";
        cout << "☆                                                  ☆\n";
        cout << "★                                                  ★\n";
        cout << "☆                                                  ☆\n";
        cout << "☆     ★★★★★      ★      ★       ★  ★      ☆\n";
        cout << "★         ★        ★ ★     ★       ★★        ★\n";
        cout << "☆         ★       ★★★★   ★       ★ ★       ☆\n";
        cout << "★         ★     ★       ★  ★★★★ ★   ★     ★\n";
        cout << "☆                                                  ☆\n";
        cout << "★                                                  ★\n";
        cout << "☆              1. 로그인   2. 회원가입             ☆\n";
        cout << "★                                                  ★\n";
        cout << "☆                                                  ☆\n";
        cout << "★                                                  ★\n";
        cout << "☆                                                  ☆\n";
        cout << "★                                                  ★\n";
        cout << "☆                                                  ☆\n";
        cout << "★                                                  ★\n";
        cout << "☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆★☆\n";

        cin >> client_choose;
        string msg = "";
        char buf[MAX_SIZE] = {};
        ZeroMemory(&buf, MAX_SIZE);

        if (client_choose == MAIN_LOGIN) { //로그인
            cout << "ID :";
            cin >> id;
            cout << "PW :";
            cin >> password;
            msg = "0 " + id + " " + password;

            send(client_sock, msg.c_str(), msg.size(), 0);
            recv(client_sock, buf, MAX_SIZE, 0);
            cout << buf << "\n";
            int index = string(buf).find("로그인 되었습니다.");
            if (index == string::npos) continue;
            real_nickname = string(buf).substr(0, index - 1);

            
            menu_list(client_addr, &id, &password); //로그인 성공 시 메뉴로
            continue; //회원탈퇴 후 main으로 오도록
        }
        if (client_choose == MAIN_MEMBERSHIP) { //회원가입
            join_membership();
            continue;
        }
    }
}

int main() {
    bool login = true;

    int code = WSAStartup(MAKEWORD(2, 2), &wsa);
    //Winsock를 초기화하는 함수. MAKEWORD(2, 2)는 Winsock의 2.2 버전을 사용하겠다는 의미
    //실행에 성공하면 0을, 실패하면 그 이외의 값을 반환
    //0을 반환했다는 것은 Winsock을 사용할 준비가 되었다는 의미

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
    stmt->execute("set names euckr");//한국어를 받기위한 설정문

    if (stmt) { delete stmt; stmt = nullptr; }
    stmt = con->createStatement();
    delete stmt;

    if (!code) {
        client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  //연결할 서버 정보 설정 부분

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