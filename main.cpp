#include <stdio.h> // for perror
#include <string.h> // for memset
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <netdb.h>
using namespace std;

map<int,int> mp; 
vector<thread> vc;
mutex mu;

void proxy(int rcvfd, int sendfd)
{
	mu.lock();
	mp[rcvfd]=1;
	mu.unlock();
	while(true) {
		string hostName="";
		const static int BUFSIZE = 1024;
		char buf[BUFSIZE];
		ssize_t received = recv(rcvfd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			mu.lock();
			mp[rcvfd]=0;
			mu.unlock();
		}

		buf[received] = '\0';
		printf("%s\n", buf);

		int check=0;
		const char* method[6] = {"GET", "POST", "HEAD", "PUT", "DELETE", "OPTIONS"};
		for(int i=0; i<6; i++) {
			if(memcmp(buf, method[i], strlen(method[i]))!=0) continue;
 			if(buf[strlen(method[i])] == 0x20) check=1;
		}
		if(check==0) break;

        	const char find[10]="Host: ";
		char *tmp = ((char *)(strstr(buf, find))+6);
		 for(; *tmp!=0xd; tmp++){
            		hostName += *tmp;
        	}
		
		char hsn[100];
		strcpy(hsn,hostName.c_str());
        	printf("host name : %s\n", hsn);

		struct sockaddr_in addr;
	        addr.sin_family = AF_INET;
        	addr.sin_port = htons(80);
        	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

        	struct hostent * host_entry = gethostbyname(hsn); 
        	char * ip = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
   
        	int res = connect(sendfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
        	if(res < 0){
			perror("connect failed");
			continue;
		}

        	ssize_t sent = send(sendfd, buf, strlen(buf), 0);
		if(sent==0){
	            perror("sent failed");
	            continue;
	        }
	}
	mu.lock();
	mp[rcvfd]=0;
	mu.unlock();
}

int main(int argc, char *argv[]) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}
	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2);
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

	while (true) {
		struct sockaddr_in addr;
		socklen_t clientlen = sizeof(sockaddr);
		int childfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
		if (childfd < 0) {
			perror("ERROR on accept");
			break;
		}
		printf("connected\n");
		vc.push_back(thread(proxy,childfd,sockfd));
		vc.push_back(thread(proxy,sockfd,childfd));
		close(childfd);
	}
	for(int i=0; i<vc.size(); i++) vc[i].detach();

	close(sockfd);

}
