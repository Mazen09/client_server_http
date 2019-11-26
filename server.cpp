#include <utility>
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
#include <sys/stat.h>


#define BACKLOG 20 // how many pending connections queue will holdsetsockopt
#define MAX_THREAD 10
#define EOR "\r\n"

using namespace std;

int portNumber;

int num_of_threads = MAX_THREAD;
string ok = "HTTP/1.1 200 OK \r\n";
string notFound = "HTTP/1.1 404 NotFound \r\n";


void create_directory(string file_path);
std::vector<std::string> splitByWhitespace(const std::string& message);
void handleGETrequest(int new_fd, const std::string& file_path);
void handlePOSTrequest(const std::string& file_path, const std::string& data);
void handleRequests(struct sockaddr_in their_addr, int new_fd);

int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
struct sockaddr_in my_addr; // my address information
struct sockaddr_in their_addr; // connector’s address information

socklen_t sin_size;

int main()
{
    cout << "Enter port number: " << endl;
    cin >> portNumber;
    my_addr.sin_family = AF_INET; // host byte order
    my_addr.sin_port = htons(portNumber); // short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct



    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        cerr << "Can't create a socket! Quitting" << endl;
        return -6;
    }

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        cerr << "Can't bind the socket! Quitting" << endl;
        return -2;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        cerr << "Can't listen on the socket! Quitting" << endl;
        return -3;
    }

    thread th[MAX_THREAD];

    cout << "server is ready." << endl;
    while (true)
    {
        sin_size = sizeof(struct sockaddr_in);
        if(num_of_threads > 0){

            if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr,&sin_size)) == -1) {
                cerr << "can't accept request" << endl;
                return -4;
            }

            num_of_threads --;
            th[num_of_threads] = thread(handleRequests,their_addr, new_fd);
        }else{
            for (auto & i : th){
                i.join();
            }
        }

    }
}

std::vector<std::string> splitByWhitespace(const std::string& message){
    std::istringstream iss(message);
    std::vector<std::string> result((std::istream_iterator<std::string>(iss)),
                                    std::istream_iterator<std::string>());
    return result;
}

void handleGETrequest(int new_fd, const std::string& file_path) {
    ifstream fin;
    char c;
    string end_of_response = "\r\n";
    vector<char> data;
    string path = getenv("HOME");
    path.append("/server");

    if(file_path == "/") // main html page
    {
        path.append("/index.html");
    }else{
        path.append(file_path);
    }

    fin.open(path);
    if(fin){ // file exists
        //send response
        send(new_fd, ok.c_str(), ok.length() , 0);
        while(fin.get(c)){
            data.push_back(c);
        }
        fin.close();

    }else{
        //send response
        send(new_fd, notFound.c_str(), notFound.length() , 0);
        //end_of_response
        send(new_fd, end_of_response.c_str(), end_of_response.length() , 0);
        return;
    }

    //send data
    send(new_fd, data.data(), data.size() , 0);
    //end_of_response
    usleep(500000);
    send(new_fd, end_of_response.c_str(), end_of_response.length() , 0);
}


void handlePOSTrequest(const std::string& file_path, const std::string& data) {
    ofstream fout;

    string path = getenv("HOME");

    //create the directory if not exist
    create_directory(file_path);
    //open the file
    fout.open(path + "/server" + file_path);
    for(char c : data)
    {
        fout << c;
    }
    fout.close();
}

void handleRequests(struct sockaddr_in their_addr, int new_fd){
    string data;
    string chunk;
    char host[NI_MAXHOST];      // Client's remote name
    char service[NI_MAXSERV];   // Service (i.e. port) the client is connect on
    vector<string> splittedReq;

    memset(host, 0, NI_MAXHOST);
    memset(service, 0, NI_MAXSERV);

    if (getnameinfo((sockaddr*)&their_addr, sizeof(their_addr), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
    {
        cout << host << " connected on port " << service << endl;
    }
    else
    {
        inet_ntop(AF_INET, &their_addr.sin_addr, host, NI_MAXHOST);
        cout << host << " connected on port " << ntohs(their_addr.sin_port) << endl;
    }

    char buf[4096];


    while (true)
    {

        memset(buf, 0, 4096);

        // Wait for client to send data
        int bytesReceived = recv(new_fd, buf, 4096, 0);
        if (bytesReceived == -1){
            cerr << "Error in recv(). Quitting" << endl;
            break;
        }

        if (bytesReceived == 0){
            cerr << "Client disconnected !!! " << endl;
            break;
        }

        splittedReq = splitByWhitespace(string(buf, 0, bytesReceived));
        if(splittedReq[0] == "GET") handleGETrequest(new_fd, splittedReq[1]);
        if(splittedReq[0] == "POST"){
            memset(buf, 0, 4096);
            send(new_fd, ok.c_str(), ok.length(), 0);

            while ((bytesReceived = recv(new_fd, buf, sizeof(buf), 0)) > 0) {
                chunk.append(buf, bytesReceived);
                if(strcmp(chunk.c_str(),EOR) == 0) break;
                data.append(chunk);
                chunk.clear();
            }

            handlePOSTrequest(splittedReq[1], data);

        }
    }

    num_of_threads ++;
}

void create_directory(string file_path){
    struct stat buffer{};
    string directory;
    string make = "mkdir ";
    //remove file name from path and create the directory if not exists
    int index = 0;
    for (int i = 0; i < file_path.length(); ++i) {
        if(file_path[i] == '/') index = i;
    }
    if(index > 0){
        directory = file_path.substr(0,index);
        //check if directory exists
        if(stat(directory.c_str(), &buffer) != 0)
        {
            make.append(directory);
            system(make.c_str());
        }
    }
}