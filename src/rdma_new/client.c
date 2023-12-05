#include "utils.h"
/* These are basic RDMA resources */
/* These are RDMA connection related resources */
static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_client_id = NULL;
static struct ibv_pd *pd = NULL;
static struct ibv_comp_channel *io_completion_channel = NULL;
static struct ibv_cq *client_cq = NULL;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp *client_qp;
/* These are memory buffers related resources */
static struct ibv_mr *client_metadata_mr = NULL,
        *client_src_mr = NULL,
        *client_dst_mr = NULL,
        *server_metadata_mr = NULL;
static struct exchange_buffer client_metadata_attr, server_metadata_attr;
static struct ibv_send_wr client_send_wr, *bad_client_send_wr = NULL;
static struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr = NULL;
static struct ibv_sge client_send_sge, server_recv_sge;
/* Source and Destination buffers, where RDMA operations source and sink */
static char *src = NULL, *dst = NULL;

// client resources struct
struct per_client_resources {
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_comp_channel *completion_channel;
    struct ibv_qp *qp;
    struct rdma_cm_id *client_id;
    unsigned long *table_start_addr;
};

// connection struct
struct per_connection_struct {
    char* memory_region;
    struct ibv_mr *memory_region_mr;

    struct msg *send_msg;
    struct ibv_mr *send_buffer;

    struct msg *receive_msg;
    struct ibv_mr *receive_buffer;

};

static struct per_client_resources *client_res = NULL;

/* This is our testing function */
static int check_src_dst()
{
    return memcmp((void*) src, (void*) dst, strlen(src));
}

static void client_prepare_connection(struct sockaddr_in *s_addr) {
    client_res = (struct per_client_resources*) malloc(sizeof(struct per_client_resources));

    // Client side - create event channel
    HANDLE(cm_event_channel = rdma_create_event_channel());
    info("RDMA CM event channel created: %p \n", cm_event_channel);

    // Create client ID
    HANDLE_NZ(rdma_create_id(cm_event_channel, &cm_client_id,
                             NULL,
                             RDMA_PS_TCP));

    client_res->client_id = cm_client_id;
    // Resolve IP address to RDMA address and bind to client_id
    HANDLE_NZ(rdma_resolve_addr(client_res->client_id, NULL, (struct sockaddr *) s_addr, 2000));
    info("waiting for cm event: RDMA_CM_EVENT_ADDR_RESOLVED\n");
}

static int setup_client_resources(struct sockaddr_in *s_addr) {
    printf("Trying to connect to server at : %s port: %d \n",
           inet_ntoa(s_addr->sin_addr),
           ntohs(s_addr->sin_port));

    // Create a protection domain
    HANDLE(client_res->pd = ibv_alloc_pd(client_res->client_id->verbs));
    info("Protection Domain (PD) allocated: %p \n", client_res->pd );

    // Create a completion channel
    HANDLE(client_res->completion_channel = ibv_create_comp_channel(cm_client_id->verbs));
    info("Completion channel created: %p \n", client_res->completion_channel);

    // Create a completion queue pair
    HANDLE(client_res->cq = ibv_create_cq(client_res->client_id->verbs /* which device*/,
                              CQ_CAPACITY /* maximum capacity*/,
                              NULL /* user context, not used here */,
                              client_res->completion_channel /* which IO completion channel */,
                              0 /* signaling vector, not used here*/));
    info("CQ created: %p with %d elements \n", client_res->cq, client_res->cq->cqe);

    // Receive notifications from complete queue pair
    HANDLE_NZ(ibv_req_notify_cq(client_res->cq, 0));

    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
    qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
    qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
    qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
    qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */

    /* We use same completion queue, but one can use different queues */
    qp_init_attr.recv_cq = client_res->cq;
    qp_init_attr.send_cq = client_res->cq;

    HANDLE_NZ(rdma_create_qp(client_res->client_id,
                         client_res->pd,
                         &qp_init_attr));

    client_res->qp = cm_client_id->qp;
    info("QP created: %p \n", client_res->qp);
    return 0;
}

static int post_recv_server_memory_map()
{
    HANDLE(server_metadata_mr = rdma_buffer_register(client_res->pd,
                                &server_metadata_attr,
                                sizeof(server_metadata_attr),
                                (IBV_ACCESS_LOCAL_WRITE)));
    server_recv_sge.addr = (uint64_t) server_metadata_mr->addr;
    server_recv_sge.length = (uint32_t) server_metadata_mr->length;
    server_recv_sge.lkey = (uint32_t) server_metadata_mr->lkey;

    bzero(&server_recv_wr, sizeof(server_recv_wr));
    server_recv_wr.sg_list = &server_recv_sge;
    server_recv_wr.num_sge = 1;

    HANDLE_NZ(ibv_post_recv(client_res->qp /* which QP */,
                        &server_recv_wr /* receive work request*/,
                        &bad_server_recv_wr /* error WRs */));
    info("Receive buffer pre-posting is successful \n");
    return 0;
}

static void connect_to_server()
{
    struct rdma_conn_param conn_param;
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;

    bzero(&conn_param, sizeof(conn_param));
    conn_param.initiator_depth = 3;
    conn_param.responder_resources = 3;
    conn_param.retry_count = 3; // if fail, then how many times to retry
    HANDLE_NZ(rdma_connect(client_res->client_id, &conn_param));
}

static int post_send_to_server(struct per_connection_struct* conn)
{
    struct ibv_wc wc[2];
    int ret = -1;

    conn->send_msg = malloc(sizeof(struct msg));
    // send msg with offset and offset num as 0
    conn->send_msg->type = OFFSET;
    conn->send_msg->data.offset = 0;

    /* conn->send_buffer = ( conn->send_msg )
     * client_metadata_attr.address = conn->send_buffer
     * client_metadata_mr = (client_metadata_attr.address, client_metadata_attr.length, client_metadata_attr.local_stag)
     * sge = client_metadata_mr
     * send_wr = sge
     * */

    // Create a client memory buffer
    HANDLE(conn->send_buffer = rdma_buffer_register(client_res->pd,
                                         conn->send_msg,
                                         sizeof(struct msg),
                                         (IBV_ACCESS_LOCAL_WRITE|
                                          IBV_ACCESS_REMOTE_READ|
                                          IBV_ACCESS_REMOTE_WRITE)));

    client_metadata_attr.address = (uint64_t) conn->send_msg;
    client_metadata_attr.length = sizeof(struct msg);
    client_metadata_attr.stag.local_stag = conn->send_buffer->lkey;

    /* now we register the metadata memory */
    HANDLE(client_metadata_mr = rdma_buffer_register(client_res->pd,
                                              &client_metadata_attr,
                                              sizeof(client_metadata_attr),
                                              IBV_ACCESS_LOCAL_WRITE));

    // Create SGE for Work Request
    client_send_sge.addr = (uint64_t) client_metadata_mr->addr;
    client_send_sge.length = (uint32_t) client_metadata_mr->length;
    client_send_sge.lkey = client_metadata_mr->lkey;

    // Link the SGE to the Work Request
    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_SEND;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;

    // Now Post Send Work Request to the server
    HANDLE_NZ(ibv_post_send(client_res->qp,
                        &client_send_wr,
                        &bad_client_send_wr));

    /* at this point we are expecting 2 work completion. One for our
     * send and one for recv that we will get from the server for
     * its buffer information */
    ret = process_work_completion_events(client_res->completion_channel,
                                         wc, 2);
    if(ret != 2) {
        error("We failed to get 2 work completions , ret = %d \n",
                   ret);
        return ret;
    }
    info("Server sent us its buffer location and credentials, showing \n");
    show_exchange_buffer(&server_metadata_attr);
    return 0;
}

/* This function does :
 * 1) Prepare memory buffers for RDMA operations
 * 1) RDMA write from src -> remote buffer
 * 2) RDMA read from remote bufer -> dst
 */
static int client_remote_memory_ops()
{
    struct ibv_wc wc;
    int ret = -1;
    client_dst_mr = rdma_buffer_register(pd,
                                         dst,
                                         strlen(src),
                                         (IBV_ACCESS_LOCAL_WRITE |
                                          IBV_ACCESS_REMOTE_WRITE |
                                          IBV_ACCESS_REMOTE_READ));
    if (!client_dst_mr) {
        error("We failed to create the destination buffer, -ENOMEM\n");
        return -ENOMEM;
    }
    /* Step 1: is to copy the local buffer into the remote buffer. We will
     * reuse the previous variables. */
    /* now we fill up SGE */
    client_send_sge.addr = (uint64_t) client_src_mr->addr;
    client_send_sge.length = (uint32_t) client_src_mr->length;
    client_send_sge.lkey = client_src_mr->lkey;
    /* now we link to the send work request */
    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_RDMA_WRITE;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;
    /* we have to tell server side info for RDMA */
    client_send_wr.wr.rdma.rkey = server_metadata_attr.stag.remote_stag;
    client_send_wr.wr.rdma.remote_addr = server_metadata_attr.address;
    /* Now we post it */
    ret = ibv_post_send(client_qp,
                        &client_send_wr,
                        &bad_client_send_wr);
    if (ret) {
        error("Failed to write client src buffer, errno: %d \n",
                   -errno);
        return -errno;
    }
    /* at this point we are expecting 1 work completion for the write */
    ret = process_work_completion_events(io_completion_channel,
                                         &wc, 1);
    if(ret != 1) {
        error("We failed to get 1 work completions , ret = %d \n",
                   ret);
        return ret;
    }
    info("Client side WRITE is complete \n");

    // CLIENT READ
    /* Now we prepare a READ using same variables but for destination */
    client_send_sge.addr = (uint64_t) client_dst_mr->addr;
    client_send_sge.length = (uint32_t) client_dst_mr->length;
    client_send_sge.lkey = client_dst_mr->lkey;
    /* now we link to the send work request */
    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_RDMA_READ;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;


    /* we have to tell server side info for RDMA */
    client_send_wr.wr.rdma.rkey = server_metadata_attr.stag.remote_stag;
    client_send_wr.wr.rdma.remote_addr = server_metadata_attr.address;
    /* Now we post it */
    ret = ibv_post_send(client_qp,
                        &client_send_wr,
                        &bad_client_send_wr);
    if (ret) {
        error("Failed to read client dst buffer from the master, errno: %d \n",
                   -errno);
        return -errno;
    }
    /* at this point we are expecting 1 work completion for the write */
    ret = process_work_completion_events(io_completion_channel,
                                         &wc, 1);
    if(ret != 1) {
        error("We failed to get 1 work completions , ret = %d \n",
                   ret);
        return ret;
    }

    info("The RDMA read data is ");
    printf("buffer attr, addr: %s , len: %u\n",
           (char*) client_dst_mr->addr,
           (unsigned int) client_dst_mr->length);
    info("Client side READ is complete \n");
    return 0;
}

/* This function disconnects the RDMA connection from the server and cleans up
 * all the resources.
 */
static int client_disconnect_and_clean()
{
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;
    /* active disconnect from the client side */
    ret = rdma_disconnect(cm_client_id);
    if (ret) {
        error("Failed to disconnect, errno: %d \n", -errno);
        //continuing anyways
    }
    ret = on_event(cm_event_channel,
                                RDMA_CM_EVENT_DISCONNECTED,
                                &cm_event);
    if (ret) {
        error("Failed to get RDMA_CM_EVENT_DISCONNECTED event, ret = %d\n",
                   ret);
        //continuing anyways
    }
    ret = rdma_ack_cm_event(cm_event);
    if (ret) {
        error("Failed to acknowledge cm event, errno: %d\n",
                   -errno);
        //continuing anyways
    }
    /* Destroy QP */
    rdma_destroy_qp(cm_client_id);
    /* Destroy client cm id */
    ret = rdma_destroy_id(cm_client_id);
    if (ret) {
        error("Failed to destroy client id cleanly, %d \n", -errno);
        // we continue anyways;
    }
    /* Destroy CQ */
    ret = ibv_destroy_cq(client_cq);
    if (ret) {
        error("Failed to destroy completion queue cleanly, %d \n", -errno);
        // we continue anyways;
    }
    /* Destroy completion channel */
    ret = ibv_destroy_comp_channel(io_completion_channel);
    if (ret) {
        error("Failed to destroy completion channel cleanly, %d \n", -errno);
        // we continue anyways;
    }
    /* Destroy memory buffers */
    rdma_buffer_deregister(server_metadata_mr);
    rdma_buffer_deregister(client_metadata_mr);
    rdma_buffer_deregister(client_src_mr);
    rdma_buffer_deregister(client_dst_mr);
    /* We free the buffers */
    free(src);
    free(dst);
    /* Destroy protection domain */
    ret = ibv_dealloc_pd(pd);
    if (ret) {
        error("Failed to destroy client protection domain cleanly, %d \n", -errno);
        // we continue anyways;
    }
    rdma_destroy_event_channel(cm_event_channel);
    printf("Client resource clean up is complete \n");
    return 0;
}
void usage() {
    printf("Usage:\n");
    printf("rdma_client: [-a <server_addr>] [-p <server_port>] -s string (required)\n");
    printf("(default IP is 127.0.0.1 and port is %d)\n", DEFAULT_RDMA_PORT);
    exit(1);
}

static int wait_for_event(struct sockaddr_in *s_addr) {

    struct rdma_cm_event *dummy_event = NULL;
    struct per_connection_struct* connection = NULL;

    while(rdma_get_cm_event(cm_event_channel, &dummy_event) == 0) {
        struct rdma_cm_event cm_event;
        memcpy(&cm_event, dummy_event, sizeof(*dummy_event));
        info("%s event received \n", rdma_event_str(cm_event.event));
        switch(cm_event.event) {
            case RDMA_CM_EVENT_ADDR_RESOLVED:
                HANDLE_NZ(rdma_ack_cm_event(dummy_event));
                connection = (struct per_connection_struct*) malloc (sizeof(struct per_connection_struct*));
                rdma_resolve_route(client_res->client_id, 2000);
                break;
            case RDMA_CM_EVENT_ROUTE_RESOLVED:
                HANDLE_NZ(rdma_ack_cm_event(dummy_event));
                setup_client_resources(s_addr);
                post_recv_server_memory_map();
                connect_to_server();
                break;
            case RDMA_CM_EVENT_ESTABLISHED:
                HANDLE_NZ(rdma_ack_cm_event(dummy_event));
                post_send_to_server(connection);
                break;
            default:
                error("Event not found %s", (char *) cm_event.event);
                break;
        }
    }
}

int main(int argc, char **argv) {
    struct sockaddr_in server_sockaddr;
    int ret, option;

    bzero(&server_sockaddr, sizeof server_sockaddr);
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    src = dst = NULL;

    while ((option = getopt(argc, argv, "s:a:p:")) != -1) {
        switch (option) {
            case 's':
                printf("Passed string is : %s , with count %u \n",
                       optarg,
                       (unsigned int) strlen(optarg));
                src = calloc(strlen(optarg) , 1);
                if (!src) {
                    error("Failed to allocate memory : -ENOMEM\n");
                    return -ENOMEM;
                }
                /* Copy the passes arguments */
                strncpy(src, optarg, strlen(optarg));
                dst = calloc(strlen(optarg), 1);
                if (!dst) {
                    error("Failed to allocate destination memory, -ENOMEM\n");
                    free(src);
                    return -ENOMEM;
                }
                break;
            case 'a':
                /* remember, this overwrites the port info */
                ret = get_addr(optarg, (struct sockaddr*) &server_sockaddr);
                if (ret) {
                    error("Invalid IP \n");
                    return ret;
                }
                break;
            case 'p':
                /* passed port to listen on */
                server_sockaddr.sin_port = htons(strtol(optarg, NULL, 0));
                break;
        }
    }
    if (!server_sockaddr.sin_port) {
        /* no port provided, use the default port */
        server_sockaddr.sin_port = htons(DEFAULT_RDMA_PORT);
    }
    if (src == NULL) {
        printf("Please provide a string to copy \n");
        usage();
    }
    client_prepare_connection(&server_sockaddr);
    wait_for_event(&server_sockaddr);

//    ret = client_pre_post_recv_buffer();
//    if (ret) {
//        error("Failed to setup client connection , ret = %d \n", ret);
//        return ret;
//    }
//    ret = client_connect_to_server();
//    if (ret) {
//        error("Failed to setup client connection , ret = %d \n", ret);
//        return ret;
//    }
//    ret = client_xchange_metadata_with_server();
//    if (ret) {
//        error("Failed to setup client connection , ret = %d \n", ret);
//        return ret;
//    }
//    ret = client_remote_memory_ops();
//    if (ret) {
//        error("Failed to finish remote memory ops, ret = %d \n", ret);
//        return ret;
//    }
//    if (check_src_dst()) {
//        error("src and dst buffers do not match \n");
//    } else {
//        printf("...\nSUCCESS, source and destination buffers match \n");
//    }
//
//    ret = client_disconnect_and_clean();
//    if (ret) {
//        error("Failed to cleanly disconnect and clean up resources \n");
//    }
    return ret;

}