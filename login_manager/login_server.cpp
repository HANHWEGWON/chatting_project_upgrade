#pragma comment(lib, "ws2_32.lib") //명시적인 라이브러리의 링크. 윈속 라이브러리 참조
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <string>
#include <sstream>
#include <iostream>
#include <thread>
#include <vector>
#include <mysql/jdbc.h>
#include <algorithm>
#include <cstdio> // connect error 확인
#include <ctime>
#define MAX_SIZE 1024 //데이터 전송할 때 임시로 저장하는 곳
#define MAX_CLIENT 3 //client 명수 제한
using std::cout;
using std::cin;
using std::endl;
using std::string;
time_t timer;
struct tm* t;
sql::mysql::MySQL_Driver* driver;
sql::Connection* con;
sql::Statement* stmt;
sql::PreparedStatement* pstmt;
sql::ResultSet* result;
const string db_server = "tcp://127.0.0.1:3306"; // 데이터베이스 주소
const string db_username = "root"; // 데이터베이스 사용자
const string db_password = "1234"; // 데이터베이스 접속 비밀번호

struct SOCKET_INFO { // 연결된 소켓 정보에 대한 틀 생성
    SOCKET sck; //클라이언트 각각의 소켓 저장 위함.
    string user_id;
    string user_pw;
    string user_nick;
    string user_email;
};

struct membership {     // 회원가입 send 보내기 위한 구조체
    char user_id[128];
    char user_pw[128];
    char user_nick[128];
    char user_email[128];
};
std::vector<SOCKET_INFO> sck_list; // 연결된 클라이언트 소켓들을 저장할 배열 선언.
SOCKET_INFO server_sock; // 서버 소켓에 대한 정보를 저장할 변수 선언.
int client_count = 0; // 현재 접속해 있는 클라이언트를 count 할 변수 선언.

enum {
    login_possible_0,
    join_membership_1,
    modify_user_info_2,
    user_delete_3,
    delete_current_user_4,
};

enum Modify_user_info {
    MODIFY_ID,
    MODIFY_PASSWORD,
    MODIFY_NICKNAME,
    MODIFY_EMAIL,
    MODIFY_BACK,
    MEMBER_INFORMATION,
};

void server_init();
void add_client();
void recv_msg(int idx);
void get_order(char* buf, int idx);
void login_possible(string possible_id, int idx);
void current_user(string current_id);
bool duplicate_current_user(string duplicate_current_id);
void del_client(int idx);
void join_membership(char* buf, int idx);
void modify_user_info(string, int);
string my_info(string, string);
bool password_check(string a);
bool email_check(string email);
void delete_current_user(string);
void user_delete(string, int);

int main() {
    WSADATA wsa;
    int code = WSAStartup(MAKEWORD(2, 2), &wsa);
    // Winsock를 초기화하는 함수. MAKEWORD(2, 2)는 Winsock의 2.2 버전을 사용하겠다는 의미.
    // 실행에 성공하면 0을, 실패하면 그 이외의 값을 반환.
    // 0을 반환했다는 것은 Winsock을 사용할 준비가 되었다는 의미.
    try {
        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect(db_server, db_username, db_password);
    }
    catch (sql::SQLException& e) {
        std::cout << "Could not connect to server. Error message: " << e.what() << endl;
        exit(1);
    }
    // 데이터베이스 선택
    con->setSchema("chat");
    // db 한글 저장을 위한 셋팅
    stmt = con->createStatement();
    stmt->execute("set names euckr");
    if (stmt) { delete stmt; stmt = nullptr; }

    if (!code) {
        server_init();
        std::thread th1[MAX_CLIENT];
        for (int i = 0; i < MAX_CLIENT; i++) {
            // 인원 수 만큼 thread 생성해서 각각의 클라이언트가 동시에 소통할 수 있도록 함.
            th1[i] = std::thread(add_client); //new 안쓰고 overload 객체 생성?
        }

        for (int i = 0; i < MAX_CLIENT; i++) {
            th1[i].join();
            //join : 해당하는 thread 들이 실행을 종료하면 리턴하는 함수.
            //join 함수가 없으면 main 함수가 먼저 종료되어서 thread가 소멸하게 됨.
            //thread 작업이 끝날 때까지 main 함수가 끝나지 않도록 해줌.
        }


        closesocket(server_sock.sck);
    }
    else {
        cout << "프로그램 종료. (Error code : " << code << ")";
    }
    WSACleanup();
    return 0;
}

void server_init() { //서버 초기화
    server_sock.sck = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    // Internet의 Stream 방식으로 소켓 생성
   // SOCKET_INFO의 소켓 객체에 socket 함수 반환값(디스크립터 저장)
   // 인터넷 주소체계, 연결지향, TCP 프로토콜 쓸 것.
    SOCKADDR_IN server_addr = {}; // 소켓 주소 설정 변수
    // 인터넷 소켓 주소체계 server_addr
    server_addr.sin_family = AF_INET; // 소켓은 Internet 타입
    server_addr.sin_port = htons(5555); // 서버 포트 설정 -> 포트는 내가 하기 나름. 바꿔도 됨
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 서버이기 때문에 local 설정한다.
    //Any인 경우는 호스트를 127.0.0.1로 잡아도 되고 localhost로 잡아도 되고 양쪽 다 허용하게 할 수 있다. 그것이 INADDR_ANY이다.
    //ip 주소를 저장할 땐 server_addr.sin_addr.s_addr -- 정해진 모양?
    //소켓의 흐름. socket -> bind
    bind(server_sock.sck, (sockaddr*)&server_addr, sizeof(server_addr)); // 설정된 소켓 정보를 소켓에 바인딩한다.
    listen(server_sock.sck, SOMAXCONN); // 소켓을 대기 상태로 기다린다.*
    cout << "Server On" << endl;
}// 서버 초기화

void add_client() {
    SOCKADDR_IN addr = {};
    int addrsize = sizeof(addr);
    char buf[MAX_SIZE] = { };
    SOCKET_INFO new_client = {};

    while (true) {
        ZeroMemory(&addr, addrsize); // addr의 메모리 영역을 0으로 초기화
        new_client.sck = accept(server_sock.sck, (sockaddr*)&addr, &addrsize);
        if (new_client.sck == -1) {
            closesocket(new_client.sck);
            continue;
        }
        else break;
    }
    sck_list.push_back(new_client); // client 정보를 답는 sck_list 배열에 새로운 client 추가

    std::thread th(recv_msg, client_count);
    client_count++; // client 수 증가.
    th.join();
}

void recv_msg(int idx) {
    char buf[MAX_SIZE] = { };
    string msg = "";

    while (1) {
        int cnt = 0;
        ZeroMemory(&buf, MAX_SIZE);
        if (recv(sck_list[idx].sck, buf, MAX_SIZE, 0) > 0) { // 오류가 발생하지 않으면 recv는 수신된 바이트 수를 반환. 0보다 크다는 것은 메시지가 왔다는 것.
            if (strcmp(buf, "!!socketChangeEvent!!") == 0) {
                del_client(idx);
                return;
            }
            get_order(buf, idx);

        }
        else { //그렇지 않을 경우 퇴장에 대한 신호로 생각하여 퇴장 메시지 전송
            delete_current_user(sck_list[idx].user_id);
            del_client(idx); // 클라이언트 삭제
            return;
        }
    }
}
void delete_current_user(string delete_id) { //로그아웃 시 해당 유저를 테이블에서 삭제
    pstmt = con->prepareStatement("DELETE FROM connecting_user where user_id = ?");
    pstmt->setString(1, delete_id);
    pstmt->execute();
}

void get_order(char* buf, int idx) {       //클라이언트의 명령어와 소켓리스트의 idx를 매개변수로 가진다.
    int order = buf[0] - '0';

    string msg = (string)buf;
    string sub = "";
    switch (order) {
    case login_possible_0:
        login_possible(msg.substr(2), idx);
        break;
    case join_membership_1:
        join_membership(buf, idx);
        break;
    case modify_user_info_2:
        modify_user_info(msg.substr(2), idx);
        break;
    case user_delete_3:
        user_delete(msg.substr(2), idx);
        break;
    case delete_current_user_4:
        delete_current_user(sck_list[idx].user_id);
        break;

    }

}

void modify_user_info(string msg, int idx) {
    string new_info = msg.substr(2);
    int what_modify = msg[0] - '0';
    string user_id = sck_list[idx].user_id;
    string user_pw = sck_list[idx].user_pw;

    msg = "성공적으로 정보가 수정되었습니다.";
    switch (what_modify) {
    case MODIFY_PASSWORD:
        if (password_check(new_info)) {
            pstmt = con->prepareStatement("UPDATE user set password = ? where user_id = ?");
            pstmt->setString(1, new_info);
            pstmt->setString(2, user_id);
            pstmt->execute();
            sck_list[idx].user_pw = new_info;
        }
        else {
            msg = "false";
        }
        break;
    case MODIFY_NICKNAME:
        pstmt = con->prepareStatement("UPDATE user set nickname = ? where user_id = ?");
        pstmt->setString(1, new_info);
        pstmt->setString(2, user_id);
        pstmt->execute();
        sck_list[idx].user_nick = new_info;
        break;
    case MODIFY_EMAIL:
        if (email_check(new_info)) {
            pstmt = con->prepareStatement("UPDATE user set email = ? where user_id = ?");
            pstmt->setString(1, new_info);
            pstmt->setString(2, user_id);
            pstmt->execute();
        }
        else {
            msg = "false";
        }
        break;
    case MEMBER_INFORMATION:
        msg = my_info(user_id, user_pw);
        break;
    }

    send(sck_list[idx].sck, msg.c_str(), msg.size(), 0);

}

string my_info(string id, string pwd) {
    pstmt = con->prepareStatement("SELECT * FROM user where user_id = ? and password = ?");
    pstmt->setString(1, id);
    pstmt->setString(2, pwd);
    result = pstmt->executeQuery();
    string msg = "";

    if (result->next()) {
        msg = "내정보-------------------------\nID:" + result->getString(1) + "\nPW:" + result->getString(2) + "\n닉네임:" + result->getString(3) + "\nemail:" + result->getString(4) + "\n------------------------------";
    }
    return msg;
}


bool password_check(string a) {
    char special_char[3] = { '!', '?', '#' }; //!, ?, # 중 하나를 반드시 포함하여 비밀번호 구성
    for (char c : special_char) {
        if (a.find(c) != string::npos) {
            return true;
        }
    }
    return false;
}
bool email_check(string email) { //이메일 규칙 검사
    if (email.find("@") == string::npos) {
        return false;
    }
    else if (email.find(".com") == string::npos) {
        return false;
    }
    else if (email.find(".com") < email.find("@")) {
        return false;
    }
    else if (email.find("@") == 0) {
        return false;
    }
    else
        return true;
}

void join_membership(char* buf, int idx) {

    string use_id, use_password, use_nick, use_email;
    string db_id, correct_password;
    membership member;
    memcpy(&member, buf, sizeof(member));       //데이터 역정렬화
    use_id = (string)member.user_id;
    use_password = (string)member.user_pw;
    use_nick = (string)member.user_nick;
    use_email = (string)member.user_email;
    use_id = use_id.substr(2);

    string msg = "";


    int flag = 0;
    pstmt = con->prepareStatement("SELECT * FROM user where user_id=? ;"); //유저 id의 중복 체크를 위한 select문
    pstmt->setString(1, use_id);
    result = pstmt->executeQuery();

    while (result->next()) {
        db_id = result->getString(1).c_str();
        if (db_id == use_id) {
            flag = 1; break;
        }
    }
    if (flag == 1) {
        msg = "이미 사용중인 아이디 입니다. \n";
    }

    if (!password_check(use_password)) {
        msg += "이미 사용중인 패스워드입니다. \n";
    }

    if (!email_check(use_email)) {
        msg += "이메일 형식에 맞게 입력해주세요. \n";
    }

    if (msg.size() != 0) {
        send(sck_list[idx].sck, msg.c_str(), msg.size(), 0);
        return;
    }
    sck_list[idx].user_id = use_id;
    sck_list[idx].user_pw = use_password;
    sck_list[idx].user_nick = use_nick;
    sck_list[idx].user_email = use_email;

    //아이디 중복 체크가 끝나면 비밀번호 이름을 user테이블에 저장
    pstmt = con->prepareStatement("insert into user(user_id, password, nickname, email) values(?,?,?,?)");
    pstmt->setString(1, sck_list[idx].user_id);
    pstmt->setString(2, sck_list[idx].user_pw);
    pstmt->setString(3, sck_list[idx].user_nick);
    pstmt->setString(4, sck_list[idx].user_email);
    pstmt->execute();

    //동시에 win_lose 테이블에 회원가입한 client의 id 저장(게임 랭킹)
    pstmt = con->prepareStatement("insert into win_lose(win, lose, user_id) values(?, ?, ?)");
    pstmt->setInt(1, 0);
    pstmt->setInt(2, 0);
    pstmt->setString(3, sck_list[idx].user_id);
    pstmt->execute();

    msg = "true";
    send(sck_list[idx].sck, msg.c_str(), msg.size(), 0);

    delete pstmt;
    delete result;
}


void login_possible(string message, int idx) { //로그인
    string check_id, nickname;
    string possible_id, password, nxt = "";
    std::stringstream stream(message);
    int cnt = 0;
    while (stream >> nxt) {
        if (cnt == 0) possible_id = nxt;
        if (cnt == 1) password = nxt;
        cnt++;
    }

    pstmt = con->prepareStatement("SELECT * FROM user where user_id=?");
    pstmt->setString(1, possible_id);
    pstmt->execute();
    result = pstmt->executeQuery(); //데이터베이스에서 id와 password를 가져옴

    if (result->next()) {
        check_id = result->getString(1).c_str();
        nickname = result->getString(3).c_str();
    }
    string msg = "";

    if (check_id == possible_id) {
        if (duplicate_current_user(possible_id)) {
            msg = nickname + " 로그인 되었습니다.";
            sck_list[idx].user_id = possible_id;
            sck_list[idx].user_pw = password;
            current_user(possible_id);
        }
        else {
            msg = "현재 접속 중인 계정입니다.";
        }
    }
    else {
        msg = "로그인에 실패했습니다.";
    }
    send(sck_list[idx].sck, msg.c_str(), msg.size(), 0);

}

void user_delete(string msg, int idx) {

    string check_password = msg;
    string password = sck_list[idx].user_pw;
    string id = sck_list[idx].user_id;

    if (check_password == password) {
        pstmt = con->prepareStatement("DELETE FROM user where password = ?");
        pstmt->setString(1, password);
        pstmt->execute();
        msg = "성공적으로 회원탈퇴가 되었습니다.\n";
        delete_current_user(id);
    }
    else {
        msg = "올바르지 않은 비밀번호를 입력하셨습니다.\n";
    }

    send(sck_list[idx].sck, msg.c_str(), msg.size(), 0);

}

void current_user(string current_id) { //현재 접속 중인 유저 테이블
    pstmt = con->prepareStatement("INSERT INTO connecting_user(user_id) values(?)");
    pstmt->setString(1, current_id);
    pstmt->execute();
}

bool duplicate_current_user(string duplicate_current_id) { //해당 유저가 이미 접속 중이면 로그인 불가
    string current_id;

    pstmt = con->prepareStatement("SELECT * FROM connecting_user");
    pstmt->execute();
    result = pstmt->executeQuery();

    while (result->next()) {
        current_id = result->getString(1).c_str();
    }
    if (current_id == duplicate_current_id) {
        return false;
    }
    else return true;
}

void del_client(int idx) {
    closesocket(sck_list[idx].sck);
    sck_list.erase(sck_list.begin() + idx);
    client_count--;
}