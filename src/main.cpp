/*
 * Copyright (C) 2018 Microchip Technology Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <egt/ui>
#include <iostream>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <planes/plane.h>
#include "egt/detail/screen/kmsoverlay.h"
#include "egt/detail/screen/kmsscreen.h"
#include "djpeg.h"

#include <string.h>
#include <dirent.h>

#include <unistd.h>
#include <limits.h>

#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "color.h"

//#define DEBUG_TIME

#ifdef DEBUG_TIME
#include <sys/time.h>
#include <unistd.h>
#endif

#define MAX_WIDTH	800
#define MAX_HEIGHT	360

#define WELCOME_IMAGE	"welcome.jpg"

//#define PORT 55234
//#define TRANS_ACK	0x55AA00
//#define TRANS_RETRY 0x55AA01

#define TRANS_ACK_SUCCESS	0x55AA00
#define TRANS_ACK_FAILURE	0x55AA01

#define CMD_NEXT_KEEPGOING	1
#define CMD_NEXT_FINISHED	2

#define SA struct sockaddr

struct ImageFrame {
    char* buffer;
    size_t bufferSize;
};

int show_ip(const char *adapter)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to "eth0" */
	strncpy(ifr.ifr_name, adapter, IFNAMSIZ-1);

	if( ioctl(fd, SIOCGIFADDR, &ifr) < 0 )
	{
		std::cout << adapter << " is not available" << std::endl;
	}
	else
	{
		std::cout << "Server IP (this board) : " << inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr) << std::endl;
	}

	close(fd);
	
	return 0;
}

bool readJPEG(const char* filename, ImageFrame& frame) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return false;
    }

    std::cout << "Reading: " << filename << std::endl;
    
    frame.bufferSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    frame.buffer = static_cast<char*>(malloc(frame.bufferSize));
    if (!frame.buffer) {
        std::cerr << "Error allocating memory for buffer." << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(frame.buffer), frame.bufferSize);
    file.close();

    return true;
}

bool showJPEG(const char *filename, ImageFrame frame, char* disp_buffer)
{
    if( !readJPEG(filename, frame)) {
        std::cerr << "Failed to read JPEG file: " << filename << std::endl;
        return false;
    }
    
    int width = MAX_WIDTH;
    int height = MAX_HEIGHT;
    
    djpeg_yuv((char *)frame.buffer, frame.bufferSize, disp_buffer, &width, &height);
    
    return true;
}

int recvPackage(int connfd, void *buf, int recv_size)
{
	int ack = TRANS_ACK_SUCCESS;
	int byteLeft;
	int retval;
	unsigned char *ptr = (unsigned char*)buf;
	
	byteLeft = recv_size;
	int index = 0;
	while (index < recv_size)
	{
		retval = recv(connfd, ptr+index, byteLeft, 0);
		
		if( retval == -1 )
		{
			printf("receive packet error\r\n");
			ack = TRANS_ACK_FAILURE;
			break;
		}
		
		index += retval;
		byteLeft -= retval;
	}
	
	// send ack
	byteLeft = sizeof(unsigned int);
	retval = send(connfd, &ack, byteLeft, 0);
	if( retval != byteLeft )
	{
		printf("send ack error\r\n");
		ack = TRANS_ACK_FAILURE;
	}
	
	return (ack == TRANS_ACK_SUCCESS) ? recv_size : -1;
}

// Function designed for chat between client and server.
unsigned char *receiveProcess(int connfd, int *fSize, int *nextCommand)
{
    unsigned char *file_ptr = NULL;
    int file_size;
    int next_command;
    unsigned int ack = TRANS_ACK_SUCCESS;
    int count=0;

	int section_size;
	int retval;
	int transferSize;
	int index = 0;

	printf(RED "Start receving file\r\n" NONE);
	
	// 1. receive file size
	retval = recvPackage(connfd, (void*)&file_size, sizeof(file_size));
	if( retval < 0 )
	{
		printf("receive file size error\r\n");
		goto exit;
	}
	
	if( file_size < 0 )
	{
		// transfer finished
		printf("Client finished transfer\r\n");
		goto exit;
	}
	
	printf("File size is %d\r\n", file_size);
	*fSize = file_size;

	// 2. receive section size
	retval = recvPackage(connfd, (void*)&section_size, sizeof(section_size));
	if( retval < 0 )
	{
		printf("receive section size error\r\n");
		goto exit;
	}
	printf("section size is %d\r\n", section_size);
	
	// 3. receive data loop
	file_ptr = (unsigned char*)malloc(file_size);
	
	do
	{
		int len = ((index+section_size) > file_size) ? (file_size-index) : section_size;
		
		retval = recvPackage(connfd, (void*)(file_ptr+index), len);
		//printf("Received %d bytes\r\n", retval);
		if( retval != len )
		{
			printf("Receive data error\r\n");
			free(file_ptr);
			file_ptr = NULL;
			goto exit;
		}

		index += section_size;
	}while(index < file_size);	
	
	//printf("checksum = %08X\r\n", checksum(file_ptr, file_size));
	
	// receive next_comamnd
	transferSize = sizeof(next_command);
	retval = recvPackage(connfd, (void*)&next_command, transferSize);
	if( retval != transferSize )
	{
		printf("receive next command error\r\n");
		free(file_ptr);
		file_ptr = NULL;
		goto exit;
	}
	
	*nextCommand = next_command;

exit:
	return file_ptr;
}

// Function designed for chat between client and server.
int server_proc(int connfd, char* disp_buffer)
{
	unsigned char *file_ptr;
	int file_size;
	int width = MAX_WIDTH;
	int height = MAX_HEIGHT;
	int next_command;
    
    // infinite loop for chat
    for (;;) {   	
    	std::cout << "Receiving data..." << std::endl;
    	
		file_ptr = receiveProcess(connfd, &file_size, &next_command);
		if( file_ptr != NULL )
		{
			djpeg_yuv((char*)file_ptr, file_size, disp_buffer, &width, &height);          
    	    free(file_ptr);

			std::cout << "Next command = " << next_command << std::endl;
			if( next_command == CMD_NEXT_FINISHED )
				break;
		}
		else
			break;
    }
    
    return 1;
}

class EGT_API OverlayWindow : public egt::Window
{
public:
    OverlayWindow(const egt::Rect& rect, 
              egt::PixelFormat format_hint = egt::PixelFormat::yuv420,
              egt::WindowHint hint = egt::WindowHint::overlay)
        : egt::Window(rect, format_hint, hint)
    {
        allocate_screen();
        m_overlay = reinterpret_cast<egt::detail::KMSOverlay*>(screen());
        assert(m_overlay);
        plane_set_pos(m_overlay->s(), 0, 0);
        plane_apply(m_overlay->s());
    }

    egt::detail::KMSOverlay* GetOverlay()
    {
        return m_overlay;
    }

private:
    egt::detail::KMSOverlay* m_overlay;
};

int main(int argc, char** argv)
{

	if( argc != 3 )
	{
		printf("USAGE: nave DEVICE_NODE PORT_NUMBER\r\n");
		return -1;
	}
	
    egt::Application app(argc, argv);
    egt::add_search_path("/usr/share/libegt");
    egt::TopWindow window;

    ///-- Read the JPEG file to local buffer
    std::vector<std::string> filenames;
    
    OverlayWindow heoWin(egt::Rect(0, 0, MAX_WIDTH, MAX_HEIGHT));
    window.add(heoWin);
    
    char* disp_ptr = (char*)heoWin.GetOverlay()->raw();
    ImageFrame frame;
    std::string filename;
    int idx = 0;
                
    if( !showJPEG(WELCOME_IMAGE, frame, disp_ptr) ) {
    	std::cout << "Read first JPEG file error" << std::endl;
    	return -1;
    }
    
    heoWin.GetOverlay()->schedule_flip();

	//auto ServerProc ([&heoWin](){
	auto ServerProc ([&disp_ptr, argv](){
		std::cout << "Start receive data from client" << std::endl;	

		show_ip(argv[1]);
		
		int sockfd, connfd;
		unsigned int len;
		struct sockaddr_in servaddr, cli;
	   
		// socket create and verification
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd == -1) {
		    std::cout << "socket creation failed..." << std::endl;
		    exit(0);
		}
		else {
		    std::cout << "Socket successfully created.." << std::endl;
		}
		
		bzero(&servaddr, sizeof(servaddr));
	   
		// assign IP, PORT
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(atoi(argv[2]));
	   
		// Binding newly created socket to given IP and verification
		if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
		    std::cout << "socket bind failed..." << std::endl;
		    exit(0);
		}
		else {
			std::cout << "Socket successfully binded.." << std::endl;
		}
	   
   		// Now server is ready to listen and verification
		if ((listen(sockfd, 5)) != 0) {
			std::cout << "Listen failed..." << std::endl;
			exit(0);
		}
		else {
			std::cout << "Server listening.." << std::endl;
		}
		
		len = sizeof(cli);

		while( (connfd = accept(sockfd, (SA*)&cli, &len)) >= 0 )
		{
			std::cout << "server accept the client..." << std::endl;
			
			// Function for chatting between client and server
			int ret = server_proc(connfd, disp_ptr);
			
			close(connfd);
		}
		
		close(sockfd);
	});
	ServerProc();

    window.show();

    auto ret = app.run();

    return ret;
}

