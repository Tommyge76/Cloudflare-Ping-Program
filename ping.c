/**
 * CLI Ping Program written in C
 * Developed and tested on Ubuntu
 * Compile by "cd"ing into the directory with ping.c then doing "gcc ping.c -o ping" 
 * The program must be run as a super user, do "sudo su" to access this privilege
 * Run this program by doing ./ping <hostname/ip-address>, example: ./ping google.com
 * Press Control + C to exit the program
 * 
*/

/** References
 * http://cs241.cs.illinois.edu/assignments/networking_mp
 * https://stackoverflow.com/questions/9688899/sending-icmp-packets-in-a-c-program
 * https://stackoverflow.com/questions/46828866/icmp-request-with-data
 * https://stackoverflow.com/questions/17705786/getting-negative-values-using-clock-gettime
*/


#include <stdio.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>


#define PACKET_SIZE 64
//initalize variables for ICMP packets and raw socket
char packet_to_send[PACKET_SIZE];
char packet_to_receive[PACKET_SIZE];
int sockfd;
int packets_sent = 0;
int packets_received = 0;
pid_t pid;

//initialize structs to get host address
struct sockaddr_in from;
struct addrinfo hints, *infoptr;

//initalize structs to calcualte RTT
struct timespec begin;
struct timespec end;

//sum of RTT to calculate average RTT
double RTT_sum = 0;
int overflows = 0;

//initialize functions to be implemented
void ping_summary(int sig);
void ping_final_summary(int sig);
unsigned short check_sum(unsigned short *addr, int len);
int initialize_packet(int pack_no);
void send_packet(void);
void receive_packet(void);
int read_packet(char *buf);

//print out summary of packet loss and RTT
void ping_summary(int sig) {
    double average_RTT = 0;
    if (packets_received - overflows == 0) {
        average_RTT = 0;
    } else {
        average_RTT = RTT_sum / (packets_received - overflows);
    }
    printf("\n-------------------------PING SUMMARY------------------------\n");
    printf("%d packets sent, %d received , %d%% lost, average RTT = %f\n", packets_sent, packets_received, 100 * (packets_sent - packets_received) / packets_sent, average_RTT);
    printf("-------------------------------------------------------------\n\n");

}

//print out final report of packet loss and average RTT
void ping_final_summary(int sig) {
    double average_RTT = 0;
    if (packets_received - overflows == 0) {
        average_RTT = 0;
    } else {
        average_RTT = RTT_sum / (packets_received - overflows);
    }
    printf("\n-------------------------FINAL PING SUMMARY------------------------\n");
    printf("%d packets sent, %d received , %d%% lost, average RTT = %f\n", packets_sent, packets_received, 100 * (packets_sent - packets_received) / packets_sent, average_RTT);
    printf("-------------------------------------------------------------------\n\n");

}

//exit program if control + C is pressed
void ctrl_c_pressed() {
    receive_packet(); 
    ping_final_summary(SIGALRM);
    close(sockfd);
    free(infoptr);
    printf("%s", "\nExiting Ping Program\n");
    exit(42);
}

//checksum function for ICMP header
unsigned short check_sum(unsigned short *addr, int len) {
    int count = len;
    int sum = 0;
    unsigned short *temp_addr = addr;
    unsigned short answer = 0;
    while (count > 1) {
        sum +=  *temp_addr++;
        count -= 2;
    }
    if (count == 1) {
        *(unsigned char*)(&answer) = *(unsigned char*) temp_addr;
        sum += answer;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

//function to initialize header and data in echo request
int initialize_packet(int pack_no) {
    int packetsize;
    struct icmp *icmp;
    icmp = (struct icmp*) packet_to_send;

    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_seq = pack_no;
    icmp->icmp_id = pid;
    packetsize = PACKET_SIZE;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    icmp->icmp_cksum = check_sum((unsigned short*)icmp, packetsize); 
    return packetsize;
}

//send packets in a while loop
void send_packet() {
    int packetsize;
    pid = getpid();
    while (1) {
        packets_sent++;
        packetsize = initialize_packet(packets_sent); 
        if (sendto(sockfd, packet_to_send, sizeof(packet_to_send), 0, (struct sockaddr*) infoptr->ai_addr, infoptr->ai_addrlen) < 0) {
            perror("sendto()");
            continue;
        }
        receive_packet(); 
        ping_summary(SIGALRM);
        sleep(3); 
    }
}

//receive echo reply
void receive_packet() {
    int n;
    int packet_len;
    extern int errno;
    packet_len = sizeof(from);
    while (packets_received < packets_sent) {
        n = recvfrom(sockfd, packet_to_receive, sizeof(packet_to_receive), 0, (struct sockaddr*) &from, &packet_len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recvfrom()");
            continue;
        } 
        clock_gettime(CLOCK_MONOTONIC, &end); 
        if (read_packet(packet_to_receive) == -1) {
            continue;
        }
        packets_received++;
    }
}

//get contents of echo reply and calculate RTT
int read_packet(char *buf) {
    int i = 0;
    struct timespec temp;
    int iphdrlen;
    struct ip *ip;
    struct icmp *icmp;
    double rtt;
    ip = (struct ip*)buf;
    iphdrlen = ip->ip_hl << 2; 
    icmp = (struct icmp*)(buf + iphdrlen);

    if ((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == pid)) {
        
        if ((end.tv_sec - begin.tv_sec) < 0) {
            temp.tv_sec = end.tv_sec - begin.tv_sec - 1;
            temp.tv_nsec = 1000000000 + end.tv_nsec - begin.tv_nsec;

        } else {
            temp.tv_sec = end.tv_sec-begin.tv_sec;
            temp.tv_nsec = end.tv_nsec-begin.tv_nsec;
        }
        rtt = (double)(temp.tv_sec) + ((double)(temp.tv_nsec) / 1000000);
        if (rtt < 0) {
            rtt = 0;
            overflows++;
        }
        RTT_sum += rtt;
        printf("Reply from %s: packet_count=%u RTT=%.3f ms", inet_ntoa(from.sin_addr), icmp->icmp_seq, rtt);
    }
    else {
        return -1;
    }
    return 0;
}

//entry point for ping program
int main(int argc, char *argv[]) {

    //on control + C, end the program
    signal(SIGINT, ctrl_c_pressed);
   
    struct protoent *protocol;
    int size = 1024 * 1024;

    //check for correct arguments
    if (argc != 2) {
        printf("format: %s <hostname/IP-address>\n", argv[0]);
        exit(1);
    } 

    //ICMP
    protocol = getprotobyname("icmp");
    if (protocol == NULL) {
        perror("getprotobyname");
        exit(1);
    }
    
    //make raw socket
    sockfd = socket(AF_INET, SOCK_RAW, protocol->p_proto);
    if (sockfd < 0) {
        perror("socket error");
        exit(1);
    }
    
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    
    //Lookup address
    hints.ai_family = AF_INET; // AF_INET means IPv4 only addresses

    int result = getaddrinfo(argv[1], NULL, &hints, &infoptr);
    if (result) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        exit(1);
    }

    //send packets in loop
    send_packet();

    return 0;
}


