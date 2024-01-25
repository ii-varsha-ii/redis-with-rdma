<h1 align=center> CSCI 5571 - Advanced Operating Systems -  Final Project</h1>   
<h2 align=center> Exploring Secure Inter-Container Communication on Different Hosts </h2>

Cloudlab profile: https://github.com/ii-varsha-ii/CloudLab-RDMARoCE-profile.git    
Project report: [Advanced_OS_project.pdf](https://github.com/ii-varsha-ii/redis-with-rdma/files/14058312/Advanced_OS_project.pdf)

## To sync two Redis masters using RDMA:
### How to run RDMA server and client?
First, `make` to generate the binaries.  

Node 1:
`server [ -a <IP> -p <port> [optional] ]`   

Node 2:
`client -a <Server IP> -p <Server Port>`

### Experiment to test whether the Redis instances on Node 1 and Node 2 are in Sync:     

On the background or on seperate terminals, start the server and client processes in two different nodes: Node 1 and Node 2.

Node 1 and Node 2: 
1. Install and start redis-server. `sudo systemctl restart redis`
2. Start redis-cli

From Node 1 to Node 2:
| Node 1        | Node 2        |
| ------------- | ------------- |
| >> set 0 \<sometext\> <br> >> OK | >> get 0 <br>  >> \<sometext\> |

From Node 2 to Node 1:
| Node 2        | Node 1        |
| ------------- | ------------- |
| >> set 1 \<someothertext\> <br>  >> OK | >> get 1 <br> >> \<someothertext\>   |

## RDMA Basic Workflow:
**Server**
   1. Provide listening IP and port
   2. Listen in the socket and wait for RDMA_CM_EVENT_CONNECT_REQUEST. ( When client pings the server )
   3. Once it gets connected, create a buffer for the client metadata and post a receive request.
   4. Accept connect from client, and wait for RDMA_CM_EVENT_ESTABLISHED event.
   5. Once client sends its metadata, check for work completion event and print the client metadata.
   6. Create a server buffer for the client with the size given by the client to WRITE and READ with server being passive. Then post a send request.
   7. check for work completion and exit.

**Client**
1. Provide server addr and port and the size of the data
2. Resolve the addr, and wait for RDMA_CM_EVENT_ADDR_RESOLVED
3. Create a buffer for the server metadata, and post receive request.
4. Send a Connect request to server, and wait for RDMA_CM_EVENT_ESTABLISHED event.
5. Create a buffer for the client metadata, with client data length and address info, and post send request.
6. Post a send request with a buffer with the src data to WRITE, and wait for work completion
7. Post a send request with a buffer with a dst addr to READ, and wait for work completion.

# References:
1. Beautiful and clean RDMA example provided by https://github.com/animeshtrivedi/rdma-example.
2. https://www.doc.ic.ac.uk/~jgiceva/teaching/ssc18-rdma.pdf
3. https://insujang.github.io/2020-02-09/introduction-to-programming-infiniband/
