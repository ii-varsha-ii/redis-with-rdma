<h1 align=center> CSCI 5571 - Advanced Operating Systems -  Final Project</h1>   
<h2 align=center> Exploring Secure Inter-Container Communication on Different Hosts </h2>

Cloudlab profile: https://github.com/ii-varsha-ii/CloudLab-RDMARoCE-profile.git

    /* We have a valid connection identifier, lets start to allocate
     * resources. We need:
     * 1. Protection Domains (PD)
     * 2. Memory Buffers
     * 3. Completion Queues (CQ)
     * 4. Queue Pair (QP)
     * Protection Domain (PD) is similar to a "process abstraction"
     * in the operating system. All resources are tied to a particular PD.
     * And accessing recourses across PD will result in a protection fault.
     */

RDMA Updates:
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



