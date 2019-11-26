#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <string>
#include <thread>
#include <ctime>
#include <bits/stdc++.h>

#define DEFAULT_PORT 80
#define DEFAULT_HTTP "HTTP/1.1"
#define NOTFOUND_message "HTTP/1.1 404 NotFound \r\n"
#define OK_message "HTTP/1.1 200 OK \r\n"

#define EOR "\r\n"


using namespace std;

std::vector<std::string> splitByWhitespace(const std::string& message);
void process_GET_request(const string& path, const string& home_path);
void process_POST_request(const string& path, const string& home_path);

struct sockaddr_in serv_addr;
char buffer[1024] = {0};
int client_fd;


int main() {

    ifstream fin;

    std::string home_path = getenv("HOME");
    home_path.append("/client");

    std::vector<std::string> commands;
    std::vector<std::string> command;

    //store the commands
    std::string line;
    fin.open(home_path + "/commands.txt");
    while(fin)
    {
        getline(fin, line);
        if(line.length() > 0) commands.push_back(line);
    }
    fin.close();


    // server default info
    serv_addr.sin_family = AF_INET; // host byte order
    serv_addr.sin_port = htons(DEFAULT_PORT); // short, network byte order
    serv_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(&(serv_addr.sin_zero), '\0', 8); // zero the rest of the struct

    // create client socket
    if ((client_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        cerr << "Can't create a socket! Quitting" << endl;
        return -2;
    }


    //process the commands
    for(const string& com : commands){
        command = splitByWhitespace(com);
        if(command.size() == 4)
        {
            serv_addr.sin_port = htons(stoi(command[3]));
        }
        // create client socket
        if ((client_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
            cerr << "Can't create a socket! Quitting" << endl;
            return -2;
        }

        if(connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1)
        {
            cerr << "couldn't connect to the server !!!" << endl;
            return -3;
        }else{cout << "CONNECTED." << endl;}

        if(command[0] == "GET")
            process_GET_request(command[1], home_path);
        else if(command[0] == "POST")
            process_POST_request(command[1], home_path);

        close(client_fd);
    }

    return 0;
}

std::vector<std::string> splitByWhitespace(const std::string& message){
    std::istringstream iss(message);
    std::vector<std::string> result((std::istream_iterator<std::string>(iss)),
                                    std::istream_iterator<std::string>());
    return result;
}

void process_GET_request(const string& path, const string& home_path){
    ofstream fout;
    string msg = "GET " + path + " " + DEFAULT_HTTP + "\r\n\r\n";
    string data;
    string response;
    string chunk;


    send( client_fd, msg.c_str() , msg.length() , 0 );

    memset(buffer,0, 1024);
    int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_received == -1) cerr << "Error in recv(). Quitting" << endl;
    if (bytes_received == 0) cerr << "Client disconnected !!! " << endl;
    response.append(buffer,bytes_received);
    cout <<"srever response: " << response <<endl;

    if(strcmp(response.c_str(), NOTFOUND_message) == 0) return;


    while ((bytes_received = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
        chunk.append(buffer, bytes_received);
        if(strcmp(chunk.c_str(),EOR) == 0) break;
        data.append(chunk);
        chunk.clear();
    }

    fout.open(home_path + "/received");
    for(char c : data)
    {
        fout << c;
    }
    cout << "file downloaded !!! at: " << home_path << endl;
}
void process_POST_request(const string& path, const string& home_path){
    ifstream fin;
    string msg = "POST " + path + " " + DEFAULT_HTTP + "\r\n\r\n";
    string data;
    string response;

    fin.open(home_path + path);

    send(client_fd, msg.c_str(), msg.length(), 0);

    //receive acknowledgment
    memset(buffer,0, 1024);
    int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_received == -1) cerr << "Error in recv(). Quitting" << endl;
    if (bytes_received == 0) cerr << "Client disconnected !!! " << endl;
    response.append(buffer,bytes_received);
    cout <<"srever response: " << response <<endl;

    if(fin && (strcmp(response.c_str(), OK_message) == 0)){
        char c;
        while(fin.get(c)){
            data.push_back(c);
        }
        fin.close();
        // send data
        send(client_fd, data.c_str(), data.length(), 0);

        //end_of_response
        usleep(500000);
        send(client_fd, EOR, strlen(EOR) , 0);


    }else{
        cerr << "file not found !" << endl;
        return;
    }
}
