#pragma comment(lib, "ws2_32.lib") //�������� ���̺귯���� ��ũ. ���� ���̺귯�� ����
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <string>
#include <sstream>
#include <iostream>
#include <thread>
#include <vector>
#include <mysql/jdbc.h>
#include <algorithm>
#include <cstdio> // connect error Ȯ��
#include <ctime>
#define MAX_SIZE 1024 //������ ������ �� �ӽ÷� �����ϴ� ��
#define MAX_CLIENT 3 //client ���� ����
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
const string db_server = "tcp://127.0.0.1:3306"; // �����ͺ��̽� �ּ�
const string db_username = "root"; // �����ͺ��̽� �����
const string db_password = "1234"; // �����ͺ��̽� ���� ��й�ȣ

struct SOCKET_INFO { // ����� ���� ������ ���� Ʋ ����
    SOCKET sck; //Ŭ���̾�Ʈ ������ ���� ���� ����.
    string user_id;
    string user_pw;
    string user_nick;
    string user_email;
};

struct membership {     // ȸ������ send ������ ���� ����ü
    char user_id[128];
    char user_pw[128];
    char user_nick[128];
    char user_email[128];
};
std::vector<SOCKET_INFO> sck_list; // ����� Ŭ���̾�Ʈ ���ϵ��� ������ �迭 ����.
SOCKET_INFO server_sock; // ���� ���Ͽ� ���� ������ ������ ���� ����.
int client_count = 0; // ���� ������ �ִ� Ŭ���̾�Ʈ�� count �� ���� ����.

enum {
    login_possible_0,
    join_membership_1,
    modify_user_info_2,
    my_info_3,
    user_delete_4,
    current_user_5,
    delete_current_user_6,
    duplicate_current_user_7
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

int main() {
    WSADATA wsa;
    int code = WSAStartup(MAKEWORD(2, 2), &wsa);
    // Winsock�� �ʱ�ȭ�ϴ� �Լ�. MAKEWORD(2, 2)�� Winsock�� 2.2 ������ ����ϰڴٴ� �ǹ�.
    // ���࿡ �����ϸ� 0��, �����ϸ� �� �̿��� ���� ��ȯ.
    // 0�� ��ȯ�ߴٴ� ���� Winsock�� ����� �غ� �Ǿ��ٴ� �ǹ�.
    try {
        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect(db_server, db_username, db_password);
    }
    catch (sql::SQLException& e) {
        std::cout << "Could not connect to server. Error message: " << e.what() << endl;
        exit(1);
    }
    // �����ͺ��̽� ����
    con->setSchema("chat");
    // db �ѱ� ������ ���� ����
    stmt = con->createStatement();
    stmt->execute("set names euckr");
    if (stmt) { delete stmt; stmt = nullptr; }

    if (!code) {
        server_init();
        std::thread th1[MAX_CLIENT];
        for (int i = 0; i < MAX_CLIENT; i++) {
            // �ο� �� ��ŭ thread �����ؼ� ������ Ŭ���̾�Ʈ�� ���ÿ� ������ �� �ֵ��� ��.
            th1[i] = std::thread(add_client); //new �Ⱦ��� overload ��ü ����?
        }
        
        for (int i = 0; i < MAX_CLIENT; i++) {
            th1[i].join();
            //join : �ش��ϴ� thread ���� ������ �����ϸ� �����ϴ� �Լ�.
            //join �Լ��� ������ main �Լ��� ���� ����Ǿ thread�� �Ҹ��ϰ� ��.
            //thread �۾��� ���� ������ main �Լ��� ������ �ʵ��� ����.
        }
        closesocket(server_sock.sck);
    }
    else {
        cout << "���α׷� ����. (Error code : " << code << ")";
    }
    WSACleanup();
    return 0;
}

void server_init() { //���� �ʱ�ȭ
    server_sock.sck = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    // Internet�� Stream ������� ���� ����
   // SOCKET_INFO�� ���� ��ü�� socket �Լ� ��ȯ��(��ũ���� ����)
   // ���ͳ� �ּ�ü��, ��������, TCP �������� �� ��.
    SOCKADDR_IN server_addr = {}; // ���� �ּ� ���� ����
    // ���ͳ� ���� �ּ�ü�� server_addr
    server_addr.sin_family = AF_INET; // ������ Internet Ÿ��
    server_addr.sin_port = htons(5555); // ���� ��Ʈ ���� -> ��Ʈ�� ���� �ϱ� ����. �ٲ㵵 ��
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // �����̱� ������ local �����Ѵ�.
    //Any�� ���� ȣ��Ʈ�� 127.0.0.1�� ��Ƶ� �ǰ� localhost�� ��Ƶ� �ǰ� ���� �� ����ϰ� �� �� �ִ�. �װ��� INADDR_ANY�̴�.
    //ip �ּҸ� ������ �� server_addr.sin_addr.s_addr -- ������ ���?
    //������ �帧. socket -> bind
    bind(server_sock.sck, (sockaddr*)&server_addr, sizeof(server_addr)); // ������ ���� ������ ���Ͽ� ���ε��Ѵ�.
    listen(server_sock.sck, SOMAXCONN); // ������ ��� ���·� ��ٸ���.*
    cout << "Server On" << endl;
}// ���� �ʱ�ȭ

void add_client() {
    SOCKADDR_IN addr = {};
    int addrsize = sizeof(addr);
    char buf[MAX_SIZE] = { };
    SOCKET_INFO new_client = {};

    while (true) {
        ZeroMemory(&addr, addrsize); // addr�� �޸� ������ 0���� �ʱ�ȭ
        new_client.sck = accept(server_sock.sck, (sockaddr*)&addr, &addrsize);
        if (new_client.sck == -1) {
            closesocket(new_client.sck);
            continue;
        }
        else break;
    }
    sck_list.push_back(new_client); // client ������ ��� sck_list �迭�� ���ο� client �߰�

    std::thread th(recv_msg, client_count);
    client_count++; // client �� ����.
    th.join();
}

void recv_msg(int idx) {
    char buf[MAX_SIZE] = { };
    string msg = "";

    while (1) {
        int cnt = 0;
        ZeroMemory(&buf, MAX_SIZE);
        if (recv(sck_list[idx].sck, buf, MAX_SIZE, 0) > 0) { // ������ �߻����� ������ recv�� ���ŵ� ����Ʈ ���� ��ȯ. 0���� ũ�ٴ� ���� �޽����� �Դٴ� ��.
            get_order(buf, idx);

        }
        else { //�׷��� ���� ��� ���忡 ���� ��ȣ�� �����Ͽ� ���� �޽��� ����
            
            del_client(idx); // Ŭ���̾�Ʈ ����
            return;
        }
    }
}

void get_order(char* buf, int idx) {       //Ŭ���̾�Ʈ�� ���ɾ�� ���ϸ���Ʈ�� idx�� �Ű������� ������.
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

    }
    
}

bool password_check(string a) {
    char special_char[3] = { '!', '?', '#' }; //!, ?, # �� �ϳ��� �ݵ�� �����Ͽ� ��й�ȣ ����
    for (char c : special_char) {
        if (a.find(c) != string::npos) {
            return true;
        }
    }
    return false;
}
bool email_check(string email) { //�̸��� ��Ģ �˻�
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
    memcpy(&member, buf, sizeof(member));       //������ ������ȭ
    use_id = (string)member.user_id;
    use_password = (string)member.user_pw;
    use_nick = (string)member.user_nick;
    use_email = (string)member.user_email;
    use_id = use_id.substr(2);

    string msg = "";
   
    
        int flag = 0;
        pstmt = con->prepareStatement("SELECT * FROM user where user_id=? ;"); //���� id�� �ߺ� üũ�� ���� select��
        pstmt->setString(1, use_id);
        result = pstmt->executeQuery();

        while (result->next()) {
            db_id = result->getString(1).c_str();
            if (db_id == use_id) {
                flag = 1; break;
            }
        }
        if (flag == 1) {
            msg = "�̹� ������� ���̵� �Դϴ�. \n";
        }
         
        if (!password_check(use_password)) {
            msg += "�̹� ������� �н������Դϴ�. \n";
        }
               
        if (!email_check(use_email)) {
            msg += "�̸��� ���Ŀ� �°� �Է����ּ���. \n";
        }

    if (msg.size() != 0) {
        send(sck_list[idx].sck, msg.c_str(), msg.size(), 0);
        return;
    }
    sck_list[idx].user_id = use_id;
    sck_list[idx].user_pw = use_password;
    sck_list[idx].user_nick = use_nick;
    sck_list[idx].user_email = use_email;

    //���̵� �ߺ� üũ�� ������ ��й�ȣ �̸��� user���̺��� ����
    pstmt = con->prepareStatement("insert into user(user_id, password, nickname, email) values(?,?,?,?)");
    pstmt->setString(1, sck_list[idx].user_id);
    pstmt->setString(2, sck_list[idx].user_pw);
    pstmt->setString(3, sck_list[idx].user_nick);
    pstmt->setString(4, sck_list[idx].user_email);
    pstmt->execute();

    //���ÿ� win_lose ���̺��� ȸ�������� client�� id ����(���� ��ŷ)
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


void login_possible(string possible_id, int idx) { //�α���
    string check_id;

    pstmt = con->prepareStatement("SELECT * FROM user where user_id=?");
    pstmt->setString(1, possible_id);
    pstmt->execute();
    result = pstmt->executeQuery(); //�����ͺ��̽����� id�� password�� ������
    
    if (result->next()) {
        check_id = result->getString(1).c_str();
    }
    string msg = "";
    
    if (check_id == possible_id) {
        if (duplicate_current_user(possible_id)) {
            msg = "�α��� �Ǿ����ϴ�.";
            current_user(possible_id);
        }
        else {
            msg = "���� ���� ���� �����Դϴ�.";
        }
    }
    else {
        msg =  "�α��ο� �����߽��ϴ�.";
    }
    send(sck_list[idx].sck, msg.c_str(), msg.size(), 0);

}

void current_user(string current_id) { //���� ���� ���� ���� ���̺�
    pstmt = con->prepareStatement("INSERT INTO connecting_user(user_id) values(?)");
    pstmt->setString(1, current_id);
    pstmt->execute();
}

bool duplicate_current_user(string duplicate_current_id) { //�ش� ������ �̹� ���� ���̸� �α��� �Ұ�
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