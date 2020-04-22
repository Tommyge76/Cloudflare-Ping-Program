# Cloudflare-Ping-Program

* This is a simple ping program that is meant to be run from the command line. The program was developed and tested on Ubuntu.
* You must be the root user to run this program because a RAW socket is created. Type "sudo su" to access root permissions.
* Compile the program by doing "gcc ping.c -o ping". 
* Run the program by doing ./ping <hostname/ip-address>. Currently, the program only works for IPv4 addresses.
